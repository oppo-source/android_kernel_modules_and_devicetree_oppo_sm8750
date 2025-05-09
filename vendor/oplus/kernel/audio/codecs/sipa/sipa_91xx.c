/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG
// #define HAVE_BACKTRACE_SUPPORT
#define LOG_FLAG	"sipa_91xx"


#include <linux/regmap.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <linux/interrupt.h>
#include <sound/pcm_params.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/kernel.h>

#include "sipa_common.h"
#include "sipa_regmap.h"
#include "sipa_91xx.h"
#include "sipa_parameter.h"

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2023/04/18, Add for smartpa err feedback. */
#include <soc/oplus/system/oplus_mm_kevent_fb.h>
#endif

static struct snd_soc_dapm_widget sia91xx_dapm_widgets_common[] = {
	/* Stream widgets */
	SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF OUT", "AIF Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("OUTL"),
	SND_SOC_DAPM_INPUT("AEC Loopback"),
};

static const struct snd_soc_dapm_route sia91xx_dapm_routes_common[] = {
	{ "OUTL", NULL, "AIF IN" },
	{ "AIF OUT", NULL, "AEC Loopback" },
};

const struct sia91xx_irq_desc irq_range[] = {
	{0x1, "PORI"},
	{0x4, "NOCLK"},
	{0x8, "OTPI"},
	{0x20, "UVPI"},
	{0x40, "OCPI"},
	{0x80, "TDMERR"},
};

#ifdef OPLUS_FEATURE_SPEAKER_MUTE
extern int speaker_mute_control;
#endif /* OPLUS_FEATURE_SPEAKER_MUTE */

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2023/04/18, Add for smartpa err feedback. */
#define OPLUS_AUDIO_EVENTID_SMARTPA_ERR    10041
#define ERROR_INFO_MAX_LEN                 20
#define SIA91XX_STATUS_REG                 0x08
#define SIA91XX_STATUS_NORMAL_VALUE        0x0
/* 0x106D = 0001000001101101b, mask bit0/2/3/5/6 */
#define SIA91XX_STATUS_CHECK_MASK          0x6D
/* 2024/06/28, Add for smartpa vbatlow err check. */
#define VBAT_LOW_REG_BIT_MASK              0x20

#define BYPASS_PA_ERR_FB_10041             0x01
#define BYPASS_SPK_ERR_FB_10042            0x02
#define TEST_PA_ERR_FB_10041               0x04
#define TEST_SPK_ERR_FB_10042              0x08

struct check_status_err {
	int bit;
	uint32_t err_val;
	char info[ERROR_INFO_MAX_LEN];
};
static const struct check_status_err check_err[] = {
	{0,  1, "PowerOnRst"},
	{2,  1, "NoClock"},
	{3,  1, "OverTemperature"},
	{5,  1, "VbatLow"},
	{6,  1, "CurrentHigh"},
};

const unsigned char fb_regs_sia9175[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

int sia91xx_check_status_reg(sipa_dev_t *si_pa)
{
	unsigned int reg_val = 0;
	char fd_buf[MM_KEVENT_MAX_PAYLOAD_SIZE] = {0};
	char info[MM_KEVENT_MAX_PAYLOAD_SIZE] = {0};
	int offset = 0;
	int i = 0;
	int ret = 0;

	if ((si_pa->last_fb !=0) && ktime_before(ktime_get(), ktime_add_ms(si_pa->last_fb, MM_FB_KEY_RATELIMIT_1MIN))) {
		return 0;
	}

	ret = regmap_read(si_pa->regmap, SIA91XX_STATUS_REG, &reg_val);

	if (ret == 0) {
		pr_info("[ info][%s] %s: read reg[0x%x]=0x%x\n", __func__, si_pa->name, SIA91XX_STATUS_REG, reg_val);
		if (si_pa->control_fb & TEST_PA_ERR_FB_10041) {
			reg_val = 0xFF;
			pr_info("[ info][%s] just for test 10041, change reg_val=0x%x", __func__, reg_val);
		}
		/* 2024/06/28, Add for smartpa vbatlow err check. */
		if (reg_val & VBAT_LOW_REG_BIT_MASK) {
			si_pa->vbatlow_cnt++;
			pr_info("[ info][%s] vbatlow_cnt=%u", __func__, si_pa->vbatlow_cnt);
		}

		if ((SIA91XX_STATUS_NORMAL_VALUE & SIA91XX_STATUS_CHECK_MASK) != (reg_val & SIA91XX_STATUS_CHECK_MASK)) {
			offset = strlen(info);
			scnprintf(info + offset, sizeof(info) - offset - 1, \
					"sia91xx SPK%u:reg[0x%x]=0x%x,", \
					si_pa->channel_num + 1, SIA91XX_STATUS_REG, reg_val);
			for (i = 0; i < ARRAY_SIZE(check_err); i++) {
				if (check_err[i].err_val == (1 & (reg_val >> check_err[i].bit))) {
					offset = strlen(info);
					scnprintf(info + offset, sizeof(info) - offset - 1, "%s,", check_err[i].info);
				}
			}

			offset = strlen(info);
			scnprintf(info + offset, sizeof(info) - offset - 1, "regs:(");

			for (i = 0; i < sizeof(fb_regs_sia9175); i++) {
				ret = regmap_read(si_pa->regmap, fb_regs_sia9175[i], &reg_val);
				if (ret == 0) {
					offset = strlen(info);
					scnprintf(info + offset, sizeof(info) - offset - 1, "%x,", reg_val);
				} else {
					break;
				}
			}

			offset = strlen(info);
			scnprintf(info + offset, sizeof(info) - offset - 1, ")");
		}
	} else {
		offset = strlen(info);
		scnprintf(info + offset, sizeof(info) - offset - 1, \
				"sia91xx SPK%u:failed to read regs 0x%x, ret=%d,", \
				si_pa->channel_num + 1, SIA91XX_STATUS_REG, ret);

		pr_info("[ info][%s] reg read err, ret = %d", __func__, ret);
	}

	/* feedback the check error */
	offset = strlen(info);
	if ((offset > 0) && (offset < MM_KEVENT_MAX_PAYLOAD_SIZE)) {
		if (si_pa->control_fb & TEST_PA_ERR_FB_10041) {
			scnprintf(fd_buf, sizeof(fd_buf) - 1, "payload@@just for test 10041, ignore");
		} else {
			scnprintf(fd_buf, sizeof(fd_buf) - 1, "payload@@%s", info);
		}
		mm_fb_audio_kevent_named(OPLUS_AUDIO_EVENTID_SMARTPA_ERR,
				MM_FB_KEY_RATELIMIT_5MIN, fd_buf);
		si_pa->last_fb = ktime_get();
		pr_info("[ info][%s] fd_buf=%s", __func__, fd_buf);
	}

	return 1;
}
#endif /*OPLUS_FEATURE_MM_FEEDBACK*/


static irqreturn_t sia91xx_irq(
	int irq,
	void *data)
{
	sipa_dev_t *si_pa = (sipa_dev_t *)data;
	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: si_pa is NULL !!! \r\n", LOG_FLAG, __func__);
		return IRQ_HANDLED;
	}
	queue_delayed_work(si_pa->sipa_wq, &si_pa->interrupt_work, 0);

	return IRQ_HANDLED;
}


void sia91xx_register_interrupt(sipa_dev_t *si_pa)
{
	int irq_flags;
	int ret;

	if (gpio_is_valid(si_pa->irq_pin)) {
		irq_flags = IRQF_TRIGGER_RISING;

		if (si_pa->channel_num == SIPA_CHANNEL_0) {
			ret = request_irq(gpio_to_irq(si_pa->irq_pin), sia91xx_irq,
				irq_flags, "sia91xx_L", si_pa);
		} else {
			ret = request_irq(gpio_to_irq(si_pa->irq_pin), sia91xx_irq,
				irq_flags, "sia91xx_R", si_pa);
		}
	} else {
		pr_err("[  err][%s] %s: irq pin error !!! \r\n", LOG_FLAG, __func__);
	}
}

#if 0
void sia91xx_monitor(struct work_struct *work)
{
	uint32_t IntVal;
	sipa_dev_t *si_pa = container_of(work, sipa_dev_t, interrupt_work.work);

	sipa_read_reg(si_pa->regmap, SIA91XX_REG_INT_STAT, &IntVal);
	if (IntVal & 0x3ff) {
		pr_err("[  err]%s: interrupt event occurred !!! \r\n", __func__);
	}

	/* reschedule */
	queue_delayed_work(si_pa->sipa_wq, &si_pa->monitor_work, 5*HZ);

}
#endif

void sia91xx_interrupt(struct work_struct *work)
{
	sipa_dev_t *si_pa = container_of(work, sipa_dev_t, interrupt_work.work);
	uint32_t IntReg, IntEn, IntVal, regVal;
	unsigned int i;

	sipa_read_reg(si_pa->regmap, SIA91XX_REG_INT_REG, &IntReg);
	sipa_read_reg(si_pa->regmap, SIA91XX_REG_INT_EN, &IntEn);

	if (0 == (IntEn & IntReg)) {
		pr_err("[  err][%s] %s: interrupt source error !!! \r\n",LOG_FLAG, __func__);
		return;
	}
	IntVal = IntReg & 0xffff;

	for (i = 0; i < ARRAY_SIZE(irq_range); i++) {
		if (IntVal == irq_range[i].index)
			break;
	}

#if 0
	switch (IntReg & 0xff) {
	case IRQ_NOCLK:
		pr_debug("interrupt event desc = %s\n", irq_range[i].desc);
		break;
	case IRQ_OTPI:
		pr_debug("interrupt event desc = %s\n", irq_range[i].desc);
		break;
	case IRQ_UVPI:
		pr_debug("interrupt event desc = %s\n",  irq_range[i].desc);
		break;
	case IRQ_OCPI:
		pr_debug("interrupt event desc = %s\n", irq_range[i].desc);
		break;
	case IRQ_TDM:
		pr_debug("interrupt event desc = %s\n", irq_range[i].desc);
		break;
	default:
		break;
	}
#endif

	sipa_read_reg(si_pa->regmap, SIA91XX_REG_INT_STAT, &regVal);
	usleep_range(1000, 2000);
	sipa_write_reg(si_pa->regmap, SIA91XX_REG_INT_WC_REG, IntVal);
}

int sia91xx_ext_reset(sipa_dev_t *si_pa)
{
	if (si_pa && gpio_is_valid(si_pa->rst_pin)) {
		gpio_set_value_cansleep(si_pa->rst_pin, 1);
		usleep_range(5000, 6000);
		gpio_set_value_cansleep(si_pa->rst_pin, 0);
		usleep_range(5000, 6000);
	}

	return 0;
}

void *sia91xx_devm_kstrdup(struct device *dev, char *buf)
{
	char *str = devm_kzalloc(dev, strlen(buf) + 1, GFP_KERNEL);
	if (!str)
		return str;

	memcpy(str, buf, strlen(buf));
	return str;
}


int sia91xx_append_i2c_address(
	struct device *dev,
	struct i2c_client *i2c,
	struct snd_soc_dapm_widget *widgets,
	int num_widgets,
	struct snd_soc_dai_driver *dai_drv,
	int num_dai)
{
	char buf[50];
	int i;
	int i2cbus = i2c->adapter->nr;
	int addr = i2c->addr;

	if (dai_drv && num_dai > 0) {
		for (i = 0; i < num_dai; i++) {
			snprintf(buf, 50, "%s-%x-%x", dai_drv[i].name, i2cbus, addr);
			dai_drv[i].name = sia91xx_devm_kstrdup(dev, buf);

			snprintf(buf, 50, "%s-%x-%x",
						dai_drv[i].playback.stream_name,
						i2cbus, addr);
			dai_drv[i].playback.stream_name = sia91xx_devm_kstrdup(dev, buf);

			snprintf(buf, 50, "%s-%x-%x",
						dai_drv[i].capture.stream_name,
						i2cbus,
						addr);
			dai_drv[i].capture.stream_name = sia91xx_devm_kstrdup(dev, buf);
		}
	}

	if (widgets && num_widgets > 0) {
		for (i = 0; i < num_widgets; i++) {
			if (!widgets[i].sname)
				continue;

			if ((widgets[i].id == snd_soc_dapm_aif_in)
				|| (widgets[i].id == snd_soc_dapm_aif_out)) {
				snprintf(buf, 50, "%s-%x-%x", widgets[i].sname, i2cbus, addr);
				widgets[i].sname = sia91xx_devm_kstrdup(dev, buf);
			}
		}
	}
	return 0;
}

int sia91xx_detect_chip(
	sipa_dev_t *si_pa)
{
	int ret;

	/* Power up! */
	sia91xx_ext_reset(si_pa);

	ret = sipa_regmap_check_chip_id(si_pa->regmap,
		si_pa->channel_num, si_pa->chip_type);
	if (ret < 0) {
		pr_err("[  err][%s] %s: Failed to read Revision register: %d \r\n",
			LOG_FLAG, __func__, ret);
		return -EIO;
	}

	return 0;
}


int sia91xx_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	//struct snd_soc_codec *codec = dai->codec;
	//sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);
	unsigned int rate, width, channels;

	if (SNDRV_PCM_STREAM_CAPTURE == substream->stream) {
		return 0;
	}

	rate = params_rate(params);
	pr_info("[ info][%s] %s: dai:%s %s i2s rate: %u !!! \r\n",
		LOG_FLAG, __func__, dai->name, substream->name, rate);

	channels = params_channels(params);
	pr_info("[ info][%s] %s: i2s channel: %u !!! \r\n",
		LOG_FLAG, __func__, channels);
	if (channels < CHANNEL_NUM_MIN || channels > CHANNEL_NUM_MAX) {
		pr_err("[  err][%s] %s: i2s channel not match !!! \r\n",
			LOG_FLAG, __func__);
		return -SIPA_ERROR_I2S_UNMATCH;
	}

	width = snd_pcm_format_physical_width(params_format(params));
	pr_info("[ info][%s] %s: i2s width: %u !!! \r\n",
		LOG_FLAG, __func__, width);

	pr_debug("[debug][%s] %s: stream = %d, requested rate = %d, sample size = %d,"
						"physical size = %d, channel num = %d\n",
					LOG_FLAG, __func__, substream->stream, rate,
					snd_pcm_format_width(params_format(params)), width, channels);

	return 0;
}

int sia91xx_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 17, 19))
	struct snd_soc_component *component = dai->component;
	sipa_dev_t *si_pa = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = dai->codec;
	sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);
#endif

	if (SNDRV_PCM_STREAM_CAPTURE == substream->stream) {
		return 0;
	}

	pr_debug("[debug][%s] %s: dai:%s, substream:%s, startup, stream = %d \r\n",
			LOG_FLAG, __func__, dai->name, substream->name, substream->stream);

	if (SIA91XX_DISABLE_LEVEL == gpio_get_value(si_pa->rst_pin)) {
		gpio_set_value(si_pa->rst_pin, 0);
		usleep_range(5000, 6000);
	}

	return 0;
}

int sia91xx_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	pr_debug("[debug][%s] %s: fmt = 0x%x\n", LOG_FLAG, __func__, fmt);

	/* Supported mode: regular I2S, slave, or PDM */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) != SND_SOC_DAIFMT_CBS_CFS) {
			pr_err("[  err][%s] %s: Invalid Codec master mode\n",LOG_FLAG, __func__);
			return -EINVAL;
		}
		break;
	default:
		pr_err("[  err][%s] %s: Unsupported DAI format %d \n",
				LOG_FLAG, __func__, fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	return 0;
}

int sia91xx_smartpa_soft_mute(sipa_dev_t *si_pa)
{
	if (sipa_regmap_set_chip_off(si_pa))
		return 0;

	return -1;
}

int sia91xx_smartpa_start(sipa_dev_t *si_pa, int stream)
{
	uint8_t start_count = 0;

	while (start_count < SIA91XX_MAX_PA_START_TRY_COUNT) {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK
			&& sipa_regmap_set_chip_on(si_pa)) {
			break;
		}
		start_count++;
	}

	if (start_count >= SIA91XX_MAX_PA_START_TRY_COUNT &&
		stream == SNDRV_PCM_STREAM_PLAYBACK) {
			pr_err("[  err][%s] %s: Failed starting device \n",LOG_FLAG, __func__);
			return -SIPA_ERROR_DEV_START;
	} else {
		pr_debug("[debug][%s] %s: device start success! \n", LOG_FLAG, __func__);
#if 0
		queue_delayed_work(si_pa->sipa_wq,
			&si_pa->monitor_work,
			1*HZ);
#endif
	}

	return 0;
}

int sia91xx_mute(
	struct snd_soc_dai *dai,
	int mute,
	int stream)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 17, 19))
	struct snd_soc_component *component = dai->component;
	sipa_dev_t *si_pa = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = dai->codec;
	sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);
#endif

	pr_debug("[debug][%s] %s: dai:%s,%s mute = %d stream = %d\n",
		LOG_FLAG, __func__, dai->name, dai->driver->name, mute, stream);

	if (si_pa->mute == SIPA_DEVICE_MUTE_ON) {
		pr_info("[debug][%s] %s: pa is mute on, direct return!\n",LOG_FLAG, __func__);
		return 0;
	}

	if (mute) {
#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2023/04/18, Add for smartpa err feedback. */
		if (si_pa->need_chk_err && !(si_pa->control_fb & BYPASS_PA_ERR_FB_10041)) {
			sia91xx_check_status_reg(si_pa);
			si_pa->need_chk_err = false;
		}
#endif /*CONFIG_OPLUS_FEATURE_MM_FEEDBACK*/

		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			si_pa->pstream = 0;
		} else {
			si_pa->cstream = 0;
		}

		//at the stop, I2c ends earlier than i2s.
		//if (si_pa->pstream != 0 || si_pa->cstream != 0)
		if (si_pa->power_mode == false)
			return 0;

		si_pa->power_mode = false;
		//cancel_delayed_work_sync(&si_pa->monitor_work);
		if (sia91xx_smartpa_soft_mute(si_pa)) {
			gpio_set_value(si_pa->rst_pin, 1);
			usleep_range(5000, 6000);
			return -SIPA_ERROR_SOFT_MUTE;
		}
	} else {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			si_pa->pstream = 1;
		} else {
			si_pa->cstream = 1;
		}
		si_pa->power_mode = true;
#ifdef OPLUS_FEATURE_SPEAKER_MUTE
		if (speaker_mute_control) {
			pr_info("[debug][%s] %s: pa is mute on, direct return!\n", LOG_FLAG, __func__);
			return 0;
		}
#endif /* OPLUS_FEATURE_SPEAKER_MUTE */
		//at the start, I2s starts earlier than i2c.
		sipa_reg_init(si_pa);
		sipa_regmap_write_sram(si_pa);
		if (sia91xx_smartpa_start(si_pa, stream))
			return -SIPA_ERROR_SOFT_MUTE;

		sipa_regmap_check_trimming(si_pa);
	}

	return 0;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)
static struct snd_soc_dapm_context *snd_soc_codec_get_dapm(
	struct snd_soc_codec *codec)
{
	return &codec->dapm;
}
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
int sia91xx_component_probe(struct snd_soc_component *component)
{
	struct snd_soc_dapm_widget *widgets;
	struct snd_soc_dapm_context *dapm;
	unsigned int num_dapm_widgets = ARRAY_SIZE(sia91xx_dapm_widgets_common);
	sipa_dev_t *si_pa = snd_soc_component_get_drvdata(component);

	dapm = snd_soc_component_get_dapm(component);

	INIT_DELAYED_WORK(&si_pa->interrupt_work, sia91xx_interrupt);
	//INIT_DELAYED_WORK(&si_pa->monitor_work, sia91xx_monitor);
	sia91xx_register_interrupt(si_pa);

	widgets = devm_kzalloc(&si_pa->client->dev,
							sizeof(struct snd_soc_dapm_widget) *
							ARRAY_SIZE(sia91xx_dapm_widgets_common),
							GFP_KERNEL);
	if (!widgets)
			return 0;

	memcpy(widgets, sia91xx_dapm_widgets_common,
					sizeof(struct snd_soc_dapm_widget) *
							ARRAY_SIZE(sia91xx_dapm_widgets_common));

	sia91xx_append_i2c_address(&si_pa->client->dev,
					si_pa->client,
					widgets,
					num_dapm_widgets,
					NULL,
					0);

	snd_soc_dapm_new_controls(dapm, widgets,
					ARRAY_SIZE(sia91xx_dapm_widgets_common));
	snd_soc_dapm_add_routes(dapm, sia91xx_dapm_routes_common,
					ARRAY_SIZE(sia91xx_dapm_routes_common));

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2024/07/08, Add for smartpa vbatlow err check. */
	si_pa->need_chk_err = false;
	si_pa->vbatlow_cnt = 0;
	si_pa->control_fb = 0;
#endif /* CONFIG_OPLUS_FEATURE_MM_FEEDBACK */

	return 0;
}

#else
int sia91xx_codec_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_widget *widgets;
	struct snd_soc_dapm_context *dapm;
	unsigned int num_dapm_widgets = ARRAY_SIZE(sia91xx_dapm_widgets_common);
	sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);

	si_pa->codec = codec;
	dapm = snd_soc_codec_get_dapm(codec);

	INIT_DELAYED_WORK(&si_pa->interrupt_work, sia91xx_interrupt);
	//INIT_DELAYED_WORK(&si_pa->monitor_work, sia91xx_monitor);
	sia91xx_register_interrupt(si_pa);

	widgets = devm_kzalloc(&si_pa->client->dev,
				sizeof(struct snd_soc_dapm_widget) *
				ARRAY_SIZE(sia91xx_dapm_widgets_common),
				GFP_KERNEL);
	if (!widgets)
		return 0;

	memcpy(widgets, sia91xx_dapm_widgets_common,
			sizeof(struct snd_soc_dapm_widget) *
				ARRAY_SIZE(sia91xx_dapm_widgets_common));

	sia91xx_append_i2c_address(&si_pa->client->dev,
			si_pa->client,
			widgets,
			num_dapm_widgets,
			NULL,
			0);

	snd_soc_dapm_new_controls(dapm, widgets,
			ARRAY_SIZE(sia91xx_dapm_widgets_common));
	snd_soc_dapm_add_routes(dapm, sia91xx_dapm_routes_common,
			ARRAY_SIZE(sia91xx_dapm_routes_common));

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2024/07/08, Add for smartpa vbatlow err check. */
	si_pa->need_chk_err = false;
	si_pa->vbatlow_cnt = 0;
	si_pa->control_fb = 0;
#endif /* CONFIG_OPLUS_FEATURE_MM_FEEDBACK */

	return 0;
}
#endif
