// SPDX-License-Identifier: GPL-2.0
/* aw882xx.c   aw882xx codec module
 *
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

/*#define DEBUG*/
#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/syscalls.h>
#include <sound/control.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#ifdef OPLUS_ARCH_EXTENDS
#include <linux/proc_fs.h>
static int s_speaker_reboot = 0;
#endif /*OPLUS_ARCH_EXTENDS*/

#include "aw882xx.h"
#include "aw882xx_log.h"
#include "aw882xx_dsp.h"
#include "aw882xx_bin_parse.h"
#include "aw882xx_spin.h"

#ifdef OPLUS_ARCH_EXTENDS
#include "aw882xx_calib.h"
#ifdef AW_MTK_PLATFORM_WITH_DSP
#include <mtk-sp-spk-amp.h>
#endif
#endif /*OPLUS_ARCH_EXTENDS*/

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
#include <soc/oplus/system/oplus_mm_kevent_fb.h>
#endif

#define AW882XX_DRIVER_VERSION "v1.13.0.10"
#define AW882XX_I2C_NAME "aw882xx_smartpa"

#define AW_READ_CHIPID_RETRIES		5	/* 5 times */
#define AW_READ_CHIPID_RETRY_DELAY	5	/* 5 ms */

#ifdef OPLUS_ARCH_EXTENDS
#define MAX_CONTROL_NAME        48
extern void mtk_spk_set_type(int spk_type);
/* v1.10 driver do not support f0 calibrate */
#define AW_NEED_CALIB_F0
#define PDE_DATA(inode) pde_data(inode)
static unsigned int g_rf_proc_inited = false;
static char *ch_name[AW_DEV_CH_MAX] = {"pri_l", "pri_r", "sec_l", "sec_r",
						"tert_l", "tert_r", "quat_l", "quat_r"};
#endif /*OPLUS_ARCH_EXTENDS*/

static unsigned int g_aw882xx_dev_cnt = 0;
static unsigned int g_print_dbg = 0;
static unsigned int g_algo_rx_en = true;
static unsigned int g_algo_tx_en = true;
static unsigned int g_algo_copp_en = true;

static DEFINE_MUTEX(g_aw882xx_lock);
struct aw_container *g_awinic_cfg = NULL;

static const char *const aw882xx_switch[] = {"Disable", "Enable"};
static const char *const aw882xx_spin[] = {"spin_0", "spin_90",
					"spin_180", "spin_270"};

/******************************************************
 *
 * aw882xx distinguish between codecs and components by version
 *
 ******************************************************/
#ifdef AW_KERNEL_VER_OVER_4_19_1
static struct aw_componet_codec_ops aw_componet_codec_ops = {
	.kcontrol_codec = snd_soc_kcontrol_component,
	.codec_get_drvdata = snd_soc_component_get_drvdata,
	.add_codec_controls = snd_soc_add_component_controls,
	.unregister_codec = snd_soc_unregister_component,
	.register_codec = snd_soc_register_component,
};
#else
static struct aw_componet_codec_ops aw_componet_codec_ops = {
	.kcontrol_codec = snd_soc_kcontrol_codec,
	.codec_get_drvdata = snd_soc_codec_get_drvdata,
	.add_codec_controls = snd_soc_add_codec_controls,
	.unregister_codec = snd_soc_unregister_codec,
	.register_codec = snd_soc_register_codec,
};
#endif

static aw_snd_soc_codec_t *aw_get_codec(struct snd_soc_dai *dai)
{
#ifdef AW_KERNEL_VER_OVER_4_19_1
	return dai->component;
#else
	return dai->codec;
#endif
}

#ifdef OPLUS_FEATURE_SPEAKER_MUTE
static int speaker_mute_control = 0;
static char const *spk_mute_ctrl_text[] = {"Off", "On"};
static const struct soc_enum spk_mute_ctrl_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_mute_ctrl_text), spk_mute_ctrl_text);
#endif

/******************************************************
 *
 * aw882xx i2c write/read
 *
 ******************************************************/
int aw882xx_get_version(char *buf, int size)
{
	if (size > strlen(AW882XX_DRIVER_VERSION)) {
		memcpy(buf, AW882XX_DRIVER_VERSION, strlen(AW882XX_DRIVER_VERSION));
		return strlen(AW882XX_DRIVER_VERSION);
	} else {
		return -ENOMEM;
	}
}

int aw882xx_get_dev_num(void)
{
	return g_aw882xx_dev_cnt;
}

static int aw882xx_i2c_writes(struct aw882xx *aw882xx,
	unsigned char reg_addr, unsigned char *buf, unsigned int len)
{
	int ret = -1;
	unsigned char *data = NULL;

	data = kmalloc(len+1, GFP_KERNEL);
	if (data == NULL) {
		aw_dev_err(aw882xx->dev, "can not allocate memory");
		return -ENOMEM;
	}

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);

	ret = i2c_master_send(aw882xx->i2c, data, len+1);
	if (ret < 0)
		aw_dev_err(aw882xx->dev, "i2c master send error");

	kfree(data);

	return ret;
}

static int aw882xx_i2c_reads(struct aw882xx *aw882xx,
	unsigned char reg_addr, unsigned char *data_buf, unsigned int data_len)
{
	int ret;
	struct i2c_msg msg[] = {
		[0] = {
			.addr = aw882xx->i2c->addr,
			.flags = 0,
			.len = sizeof(uint8_t),
			.buf = &reg_addr,
			},
		[1] = {
			.addr = aw882xx->i2c->addr,
			.flags = I2C_M_RD,
			.len = data_len,
			.buf = data_buf,
			},
	};

	ret = i2c_transfer(aw882xx->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_dev_err(aw882xx->dev, "transfer failed.");
		return ret;
	} else if (ret != AW882XX_I2C_READ_MSG_NUM) {
		aw_dev_err(aw882xx->dev, "transfer failed(size error).");
		return -ENXIO;
	}

	return 0;
}

int aw882xx_i2c_write(struct aw882xx *aw882xx,
	unsigned char reg_addr, unsigned int reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char buf[2];

	buf[0] = (reg_data&0xff00)>>8;
	buf[1] = (reg_data&0x00ff)>>0;

	while (cnt < AW_I2C_RETRIES) {
		ret = aw882xx_i2c_writes(aw882xx, reg_addr, buf, 2);
		if (ret < 0) {
			aw_dev_err(aw882xx->dev, "i2c_write cnt=%d error=%d",
					cnt, ret);
		} else {
			if (g_print_dbg)
				aw_dev_info(aw882xx->dev, "reg_addr: 0x%02x, reg_data :0x%04x",
						(uint8_t)reg_addr, (uint16_t)reg_data);
			break;
		}
		cnt++;
	}

	return ret;
}

int aw882xx_i2c_read(struct aw882xx *aw882xx,
	unsigned char reg_addr, unsigned int *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char buf[2];

	while (cnt < AW_I2C_RETRIES) {
		ret = aw882xx_i2c_reads(aw882xx, reg_addr, buf, 2);
		if (ret < 0) {
			aw_dev_err(aw882xx->dev, "i2c_read cnt=%d error=%d",
						cnt, ret);
		} else {
			*reg_data = (buf[0]<<8) | (buf[1]<<0);
			if (g_print_dbg)
				aw_dev_info(aw882xx->dev, "reg_addr: 0x%02x, reg_data :0x%04x",
						(uint8_t)reg_addr, (uint16_t)(*reg_data));
			break;
		}
		cnt++;
	}

	return ret;
}

int aw882xx_i2c_write_bits(struct aw882xx *aw882xx,
	unsigned char reg_addr, unsigned int mask, unsigned int reg_data)
{
	int ret = -1;
	unsigned int reg_val = 0;

	ret = aw882xx_i2c_read(aw882xx, reg_addr, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev, "i2c read error, ret=%d", ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= (reg_data & (~mask));
	ret = aw882xx_i2c_write(aw882xx, reg_addr, reg_val);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev, "i2c read error, ret=%d", ret);
		return ret;
	}

	return 0;
}

static void *aw882xx_devm_kstrdup(struct device *dev, char *buf)
{
	char *str = NULL;

	str = devm_kzalloc(dev, strlen(buf) + 1, GFP_KERNEL);
	if (!str)
		return str;

	memcpy(str, buf, strlen(buf));
	return str;
}

/*****************************************************
 *
 * snd_soc_dai_driver ops
 *
 *****************************************************/
static int aw882xx_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

#ifdef OPLUS_ARCH_EXTENDS
	if (aw882xx->speaker_force_mute) {
		aw882xx->allow_pw = false;
	}
#endif
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aw_dev_info(aw882xx->dev, "playback enter");
#ifdef AW_CALI_STORE_EXAMPLE
		/*load cali re*/
		aw882xx_dev_init_cali_re(aw882xx->aw_pa);
#endif
	} else {
		aw_dev_info(aw882xx->dev, "capture enter");
	}
	return 0;
}

static int aw882xx_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	/*struct aw882xx *aw882xx = aw_snd_soc_codec_get_drvdata(dai->codec);*/
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);

	aw_dev_info(codec->dev, "fmt=0x%x", fmt);

	return 0;
}

static int aw882xx_set_dai_sysclk(struct snd_soc_dai *dai,
	int clk_id, unsigned int freq, int dir)
{
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	aw_dev_info(aw882xx->dev, "freq=%d", freq);

	aw882xx->sysclk = freq;
	return 0;
}

static int aw882xx_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		aw_dev_info(aw882xx->dev, "stream capture requested rate: %d, sample size: %d",
				params_rate(params), params_width(params));
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aw_dev_info(aw882xx->dev, "stream playback requested rate: %d, sample size: %d",
				params_rate(params), params_width(params));
	}

	return 0;
}

static void aw882xx_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aw882xx->rate = 0;
		aw_dev_info(aw882xx->dev, "stream playback");
	} else {
		aw_dev_info(aw882xx->dev, "stream capture");
	}
}

static void aw882xx_start_pa(struct aw882xx *aw882xx)
{
	int ret;
	int i;

	aw_dev_info(aw882xx->dev, "enter");

	if (aw882xx->fw_status == AW_DEV_FW_OK) {
		#ifndef OPLUS_FEATURE_SPEAKER_MUTE
		if (aw882xx->allow_pw == false) {
		#else
		if (aw882xx->allow_pw == false || speaker_mute_control) {
		#endif
			aw_dev_info(aw882xx->dev, "dev can not allow power ");
			return;
		}

		for (i = 0; i < AW_START_RETRIES; i++) {
			/*if PA already power ,stop PA then start*/
			if (aw882xx->aw_pa->status) {
				aw_dev_info(aw882xx->dev, "already start");
				return;
			}
			if (aw882xx->que_dela_work) {
				aw_dev_info(aw882xx->dev, " fw no need fw update");
			}else{
			ret = aw882xx_dev_reg_update(aw882xx->aw_pa, aw882xx->phase_sync);
			if (ret) {
				aw_dev_err(aw882xx->dev, "fw update failed, cnt:%d", i);
				continue;
				}
			}
			ret = aw882xx_device_start(aw882xx->aw_pa);
			if (ret) {
				aw_dev_err(aw882xx->dev, "start failed, cnt:%d", i);
				continue;
			} else {
				if (aw882xx->dc_flag)
					queue_delayed_work(aw882xx->work_queue,
						&aw882xx->dc_work,
						msecs_to_jiffies(AW882XX_DC_DELAY_TIME));
				aw_dev_info(aw882xx->dev, "start success");
				break;
			}
		}
	} else {
		aw_dev_info(aw882xx->dev, "dev acf load failed");
	}

}

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
#define AW882XX_CHECK_WORK_DELAY           (8000)
#define OPLUS_AUDIO_EVENTID_SMARTPA_ERR    10041
#define OPLUS_AUDIO_EVENTID_SPK_ERR        10042
#define SMARTPA_ERR_FB_VERSION             "1.0.0"
#define SPK_ERR_FB_VERSION                 "1.1.0"
#define ERROR_INFO_MAX_LEN                 20
#define AW882XX_STATUS_REG                 0x01
#define AW882XX_STATUS_NORMAL_VALUE        0x311
/* 0xCFBB = 1100101110111011b, mask bit0/1/3/4/5/7/8/9/11/14/15 */
#define AW882XX_STATUS_CHECK_MASK          0xCBBB

#define RE_BASE_RANGE                      2500
#define RE_ERR_COUNT_FB                    3
#define RE_ALLOW_NORMAL_CNT                2
#define RE_ERR_TIME_MS                     300000

#define F0_OUT_OF_RANGE_COUNT_FB           3
#define F0_HOLE_BOLCK_COUNT_FB             5
#define F0_HOLE_BOLCK_TIME_FB              1800000
#define F0_MIN                             200
#define F0_MAX                             1900
#define F0_BLOCK_HOLE_INC                  180
#define F0_ALLOW_NORMAL_CNT                2
#define F0_INVALID_VALUE                   2600

#define BYPASS_PA_ERR_FB_10041             0x01
#define BYPASS_SPK_ERR_FB_10042            0x02
#define TEST_PA_ERR_FB_10041               0x04
#define TEST_SPK_ERR_FB_10042              0x08

/* 2024/07/08, Add for smartpa vbatlow err check. */
#define VBAT_LOW_REG_BIT_MASK              0x4000

enum {
    READ_RE_VALUE = 0,
    ENABLE_F0_CALI_MODE,
    READ_F0_VALUE,
    AW_SPKS_MAX_STEPS
};

enum {
	RE_ERR_IV_OR_MEC_DISABLE = -99000,
	RE_ERR_NOT_WORK = -2000,
	RE_ERR_IV_ERR = -1000,
	RE_ERR_SHORT_MIN = -500,
	RE_ERR_SHORT_MAX = 500,
	RE_NORMAL_MIN = 4500,
	RE_NORMAL_MAX = 8500,
	RE_ERR_OPEN = 99000,
};

struct check_status_err {
	int bit;
	uint32_t err_val;
	char info[ERROR_INFO_MAX_LEN];
};
static const struct check_status_err check_err[] = {
	{0,  0, "PllUnlock"},
	{1,  1, "OverTemperature"},
	{3,  1, "BoostOverCurrent"},
	{4,  0, "UnstableClk"},
	{5,  1, "NoClock"},
	{7,  1, "Clipping"},
	{8,  0, "NotSwitch"},
	{9,  0, "NoBoost"},
	{11, 1, "CurrentHigh"},
	{14, 1, "VbatLow"},
	{15, 1, "BoostOVP"},
};

#if 0
const unsigned int g_step_delay[AW_SPKS_MAX_STEPS] ={AW882XX_CHECK_WORK_DELAY, 100, 2000};
#endif

const unsigned char fb_regs_aw88264[] = {0x02, 0x04, 0x05, 0x06, 0x07, 0x09, 0x0a, 0x0b, 0x0c, 0x10, 0x12, 0x13, 0x14, 0x60, 0x61};
const unsigned char fb_regs_aw88265[] = {0x02, 0x04, 0x05, 0x06, 0x07, 0x09, 0x0a, 0x0b, 0x0c, 0x20, 0x21, 0x22, 0x23, 0x60, 0x61};

static bool g_chk_err = false;
static uint32_t g_control_fb = 0;

static int aw882xx_check_status_reg(struct aw882xx *aw882xx)
{
	unsigned int reg_val = 0;
	char fd_buf[MM_KEVENT_MAX_PAYLOAD_SIZE] = {0};
	char info[MM_KEVENT_MAX_PAYLOAD_SIZE] = {0};
	int offset = 0;
	int i = 0;
	int ret = 0;

	if ((aw882xx->last_fb !=0) && ktime_before(ktime_get(), ktime_add_ms(aw882xx->last_fb, MM_FB_KEY_RATELIMIT_1MIN))) {
		return 0;
	}

	ret = aw882xx_i2c_read(aw882xx, AW882XX_STATUS_REG, &reg_val);
	if (ret < 0) {
		offset = strlen(info);
		scnprintf(info + offset, sizeof(info) - offset - 1, \
				"AW882xx SPK%u:failed to read regs 0x%x, ret=%d,", \
				aw882xx->aw_pa->channel + 1, AW882XX_STATUS_REG, ret);
		aw_dev_info(aw882xx->dev, "i2c read error, ret=%d", ret);
	} else {
		aw_dev_info(aw882xx->dev, "read reg[0x%x]=0x%x", AW882XX_STATUS_REG, reg_val);
		if (g_control_fb & TEST_PA_ERR_FB_10041) {
			reg_val = 0xFFFF;
			aw_dev_info(aw882xx->dev, "just for test 10041, change reg[0x%x]=0x%x", AW882XX_STATUS_REG, reg_val);
		}
		/* 2024/07/08, Add for smartpa vbatlow err check. */
		if (reg_val & VBAT_LOW_REG_BIT_MASK) {
			aw882xx->vbatlow_cnt++;
			aw_dev_info(aw882xx->dev, "vbatlow_cnt=%u", aw882xx->vbatlow_cnt);
		}
		if ((AW882XX_STATUS_NORMAL_VALUE & AW882XX_STATUS_CHECK_MASK) != (reg_val & AW882XX_STATUS_CHECK_MASK)) {
			offset = strlen(info);
			scnprintf(info + offset, sizeof(info) - offset - 1, \
					"AW882xx SPK%u:reg[0x%x]=0x%x,", \
					aw882xx->aw_pa->channel + 1, AW882XX_STATUS_REG, reg_val);
			for (i = 0; i < ARRAY_SIZE(check_err); i++) {
				if (check_err[i].err_val == (1 & (reg_val>>check_err[i].bit))) {
					offset = strlen(info);
					scnprintf(info + offset, sizeof(info) - offset - 1, "%s,", check_err[i].info);
				}
			}

			offset = strlen(info);
			scnprintf(info + offset, sizeof(info) - offset - 1, "regs:(");
			if (aw882xx->aw_pa && (PID_1852_ID == aw882xx->aw_pa->chip_id)) {
				for (i = 0; i < sizeof(fb_regs_aw88264); i++) {
					ret = aw882xx_i2c_read(aw882xx, fb_regs_aw88264[i], &reg_val);
					if (ret < 0) {
						break;
					} else {
						offset = strlen(info);
						scnprintf(info + offset, sizeof(info) - offset - 1, "%x,", reg_val);
					}
				}
			} else {
				for (i = 0; i < sizeof(fb_regs_aw88265); i++) {
					ret = aw882xx_i2c_read(aw882xx, fb_regs_aw88265[i], &reg_val);
					if (ret < 0) {
						break;
					} else {
						offset = strlen(info);
						scnprintf(info + offset, sizeof(info) - offset - 1, "%x,", reg_val);
					}
				}
			}
			offset = strlen(info);
			scnprintf(info + offset, sizeof(info) - offset - 1, ")");
		}
	}

	/* feedback the check error */
	offset = strlen(info);
	if ((offset > 0) && (offset < MM_KEVENT_MAX_PAYLOAD_SIZE)) {
		if (g_control_fb & TEST_PA_ERR_FB_10041) {
			scnprintf(fd_buf, sizeof(fd_buf) - 1, "payload@@just for test 10041, ignore");
		} else {
			scnprintf(fd_buf, sizeof(fd_buf) - 1, "payload@@%s", info);
		}
		mm_fb_audio_kevent_named(OPLUS_AUDIO_EVENTID_SMARTPA_ERR,
				MM_FB_KEY_RATELIMIT_5MIN, fd_buf);
		aw882xx->last_fb = ktime_get();
		aw_dev_info(aw882xx->dev, "fd_buf=%s", fd_buf);
	}

	return 1;
}
#if 0
static void aw882xx_check_speaker_f0(struct aw882xx *aw882xx, int f0)
{
	char fd_buf[MAX_PAYLOAD_DATASIZE] = {0};
	aw_f0_err_record_t *p_record = &aw882xx->rd_f0;
	unsigned int i = 0;

	if (F0_INVALID_VALUE == f0) {
		aw_dev_info(aw882xx->dev, "skip invalid F0 value: %d", f0);
		return;
	}

	aw_dev_info(aw882xx->dev, "SPK%u, f0=%d", aw882xx->aw_pa->channel + 1, f0);

	if (f0 <= F0_MIN) {
		if (p_record->rd_id < F0_RECORD_CNT) {
			p_record->f0[p_record->rd_id] = f0;
			p_record->rd_id++;
			p_record->err_cnt++;
		} else {
			p_record->rd_id = F0_RECORD_CNT;
			p_record->err_cnt = p_record->rd_id;
		}

		if (p_record->err_cnt >= F0_OUT_OF_RANGE_COUNT_FB) {
			snprintf(fd_buf, sizeof(fd_buf) - 1, \
				"payload@@AW882xx SPK%u:F0=%d,out of range(%u, %u) smaller,record f0=", \
				aw882xx->aw_pa->channel + 1, f0, F0_MIN, F0_MAX);
			for (i = 0; i < p_record->rd_id; i++) {
				snprintf(fd_buf + strlen(fd_buf), sizeof(fd_buf) - strlen(fd_buf) - 1, "%d,", p_record->f0[i]);
			}
		}
	} else if (f0 > (aw882xx->aw_pa->cali_desc.cali_f0 + F0_BLOCK_HOLE_INC)) {
		if (p_record->rd_id == 0) {
			p_record->tm = ktime_get();
		}
		if (p_record->rd_id < F0_RECORD_CNT) {
			p_record->f0[p_record->rd_id] = f0;
			p_record->rd_id++;
			p_record->err_cnt++;
		} else {
			p_record->rd_id = F0_RECORD_CNT;
			p_record->err_cnt = p_record->rd_id;
		}

		p_record->max_f0 = (p_record->max_f0 < f0) ? f0 : p_record->max_f0;
		if ((p_record->err_cnt >= F0_OUT_OF_RANGE_COUNT_FB) && (p_record->max_f0 >= F0_MAX)) {
			snprintf(fd_buf, sizeof(fd_buf) - 1, \
				"payload@@AW882xx SPK%u:F0=%d,out of range(%u, %u) bigger,max_f0=%d,record f0=", \
				aw882xx->aw_pa->channel + 1, f0, F0_MIN, F0_MAX, p_record->max_f0);
			for (i = 0; i < p_record->rd_id; i++) {
				snprintf(fd_buf + strlen(fd_buf), sizeof(fd_buf) - strlen(fd_buf) - 1, "%d,", p_record->f0[i]);
			}
		} else if ((p_record->err_cnt >= F0_HOLE_BOLCK_COUNT_FB) &&
					ktime_after(ktime_get(), ktime_add_ms(p_record->tm, F0_HOLE_BOLCK_TIME_FB))) {
			snprintf(fd_buf, sizeof(fd_buf) - 1, \
				"payload@@AW882xx SPK%u:F0=%d, larger than %u, maybe speaker hole blocked,max_f0=%d,record f0=", \
				aw882xx->aw_pa->channel + 1, f0, (aw882xx->aw_pa->cali_desc.cali_f0 + F0_BLOCK_HOLE_INC), p_record->max_f0);
			for (i = 0; i < p_record->rd_id; i++) {
				snprintf(fd_buf + strlen(fd_buf), sizeof(fd_buf) - strlen(fd_buf) - 1, "%d,", p_record->f0[i]);
			}
		}
	} else if (p_record->rd_id > 0) {
		if (p_record->rd_id < (p_record->err_cnt + F0_ALLOW_NORMAL_CNT)) {
			if (p_record->rd_id < F0_RECORD_CNT) {
				p_record->f0[p_record->rd_id] = f0;
				p_record->rd_id++;
			} else {
				p_record->err_cnt--;
			}
		} else {
			memset(p_record, 0, sizeof(aw_f0_err_record_t));
		}
	}

	if (p_record->err_cnt > 0) {
		aw_dev_info(aw882xx->dev, "record tm=%lld, now tm=%lld, err_cnt=%u, rd_id=%u, max_f0=%d",
			p_record->tm, ktime_get(), p_record->err_cnt, p_record->rd_id, p_record->max_f0);
	}

	if (strlen(fd_buf) > 0) {
		mm_fb_audio_kevent_named(OPLUS_AUDIO_EVENTID_SPK_ERR, MM_FB_KEY_RATELIMIT_5MIN, fd_buf);
		aw_dev_info(aw882xx->dev, "%s", fd_buf);
		memset(p_record, 0, sizeof(aw_f0_err_record_t));
	}
}
#endif
// macro for record re error value
#define RE_ERR_RECORD(p_record, val)                                           \
	do {                                                                       \
		if (p_record->err_cnt == 0) {                                          \
			p_record->tm = ktime_get();                                        \
		}                                                                      \
		if (p_record->rd_id < RE_RECORD_CNT) {                                 \
			p_record->re[p_record->rd_id] = val;                               \
			p_record->rd_id++;                                                 \
			p_record->err_cnt++;                                               \
		} else {                                                               \
			p_record->rd_id = RE_RECORD_CNT;                                   \
			p_record->err_cnt = p_record->rd_id;                               \
		}                                                                      \
		p_record->max_re = (p_record->max_re < val) ? val : p_record->max_re;  \
		p_record->min_re = (p_record->min_re > val) ? val : p_record->min_re;  \
	} while(0)

static void aw882xx_check_speaker_re(struct aw882xx *aw882xx, int re, int te)
{
	char fd_buf[MM_KEVENT_MAX_PAYLOAD_SIZE] = {0};
	aw_re_err_record_t *p_record = &aw882xx->rd_re;
	int32_t re_normal_min = 0;
	int32_t re_normal_max = 0;
	unsigned int i = 0;

	aw_dev_info(aw882xx->dev, "AW882xx SPK%u: re=%d, te=%d", \
			aw882xx->aw_pa->channel + 1, re, te);

	if (aw882xx->aw_pa->cali_desc.cali_result == CALI_RESULT_NORMAL) {
		re_normal_min = aw882xx->aw_pa->cali_desc.cali_re - RE_BASE_RANGE;
		re_normal_max = aw882xx->aw_pa->cali_desc.cali_re + RE_BASE_RANGE;
	} else {
		re_normal_min = RE_NORMAL_MIN;
		re_normal_max = RE_NORMAL_MAX;
	}

	if (g_control_fb & TEST_SPK_ERR_FB_10042) {
		re = RE_ERR_OPEN;
		aw_dev_info(aw882xx->dev, "just for test 10042, set re=%d", re);
	}

	switch (re) {
	case RE_ERR_IV_OR_MEC_DISABLE:
		RE_ERR_RECORD(p_record, re);
		if (p_record->err_cnt >= RE_ERR_COUNT_FB) {
			scnprintf(fd_buf, sizeof(fd_buf) - 1, \
					"payload@@AW882xx SPK%u:IV or MEC channel disable, re=%d, te=%d", \
					aw882xx->aw_pa->channel + 1, re, te);
		}
		break;
	case RE_ERR_NOT_WORK:
		RE_ERR_RECORD(p_record, re);
		if (p_record->err_cnt >= RE_ERR_COUNT_FB) {
			scnprintf(fd_buf, sizeof(fd_buf) - 1, \
					"payload@@AW882xx SPK%u:equivalent chip not working, re=%d, te=%d", \
					aw882xx->aw_pa->channel + 1, re, te);
		}
		break;
	case RE_ERR_IV_ERR:
		/*2022/07/04, Delete smartpa iv_err feedback.*/
		/*scnprintf(fd_buf, sizeof(fd_buf) - 1, \
				"payload@@AW882xx SPK%u:IV data error(no tx or MEC not open), re=%d, te=%d", \
				aw882xx->aw_pa->channel + 1, re, te);*/
		RE_ERR_RECORD(p_record, re);
		aw_dev_err(aw882xx->dev, "payload@@AW882xx SPK%u:IV data error(no tx or MEC not open), re=%d, te=%d", aw882xx->aw_pa->channel + 1, re, te);
		break;
	case RE_ERR_OPEN:
		RE_ERR_RECORD(p_record, re);
		if (p_record->err_cnt >= RE_ERR_COUNT_FB) {
			scnprintf(fd_buf, sizeof(fd_buf) - 1, \
					"payload@@AW882xx SPK%u:speaker open circuit, re=%d, te=%d", \
					aw882xx->aw_pa->channel + 1, re, te);
		}
		break;
	default:
		if ((re > RE_ERR_SHORT_MIN) && (re < RE_ERR_SHORT_MAX)) {
			RE_ERR_RECORD(p_record, re);
			if (p_record->err_cnt >= RE_ERR_COUNT_FB) {
				scnprintf(fd_buf, sizeof(fd_buf) - 1, \
						"payload@@AW882xx SPK%u:speaker short circuit, re=%d, te=%d", \
						aw882xx->aw_pa->channel + 1, re, te);
			}
		} else if ((re < re_normal_min) || (re > re_normal_max)) {
			RE_ERR_RECORD(p_record, re);
			if ((p_record->err_cnt >= RE_ERR_COUNT_FB) &&
				ktime_after(ktime_get(), ktime_add_ms(p_record->tm, RE_ERR_TIME_MS))) {
				scnprintf(fd_buf, sizeof(fd_buf) - 1, \
						"payload@@AW882xx SPK%u:speaker R0 out of range(%d, %d), re=%d, te=%d", \
						aw882xx->aw_pa->channel + 1, re_normal_min, re_normal_max, re, te);
			}
		} else {
			if ((p_record->rd_id > 0) && (p_record->rd_id < (p_record->err_cnt + RE_ALLOW_NORMAL_CNT))) {
				if (p_record->rd_id < RE_RECORD_CNT) {
					p_record->re[p_record->rd_id] = re;
					p_record->rd_id++;
				} else {
					p_record->err_cnt--;
				}
			} else if (p_record->rd_id >= (p_record->err_cnt + RE_ALLOW_NORMAL_CNT)) {
				memset(p_record, 0, sizeof(aw_re_err_record_t));
			}
		}
		break;
	}

	if (p_record->err_cnt > 0) {
		aw_dev_info(aw882xx->dev, "AW882xx SPK%u: min_re=%d, max_re=%d, rd_id=%u, err_cnt=%u",
				aw882xx->aw_pa->channel + 1, p_record->min_re, p_record->max_re, p_record->rd_id, p_record->err_cnt);
	}

	if (strlen(fd_buf) > 0) {
		snprintf(fd_buf + strlen(fd_buf), sizeof(fd_buf) - strlen(fd_buf) - 1,
				",min_re=%d,max_re=%d,record re=",
				p_record->min_re, p_record->max_re);
		for (i = 0; i < p_record->rd_id; i++) {
			snprintf(fd_buf + strlen(fd_buf), sizeof(fd_buf) - strlen(fd_buf) - 1, "%d,", p_record->re[i]);
		}
		if (g_control_fb & TEST_SPK_ERR_FB_10042) {
			mm_fb_audio_kevent_named(OPLUS_AUDIO_EVENTID_SPK_ERR, MM_FB_KEY_RATELIMIT_5MIN, "payload@@just for test 10042, ignore");
		} else {
			mm_fb_audio_kevent_named(OPLUS_AUDIO_EVENTID_SPK_ERR, MM_FB_KEY_RATELIMIT_5MIN, fd_buf);
		}
		aw_dev_info(aw882xx->dev, "%s", fd_buf);

		memset(p_record, 0, sizeof(aw_re_err_record_t));
	}
}
#if 0
static bool aw882xx_is_cali_f0_valid(struct aw882xx *aw882xx)
{
	int32_t cali_f0 = aw882xx->aw_pa->cali_desc.cali_f0;

	return (aw882xx->need_f0_cali && (cali_f0 > AW_CALI_F0_DEFAULT_MIN) && (cali_f0 < AW_CALI_F0_DEFAULT_MAX));
}
#endif
static int aw882xx_check_speaker_status(struct aw882xx *aw882xx)
{
	int32_t re = 0;
	int32_t te = 0;
#if 0
	int32_t f0 = 0;
	unsigned int delay_ms = AW882XX_CHECK_WORK_DELAY;
#endif
	int32_t ret = 0;

	if (aw882xx_cali_svc_get_cali_status()) {
		aw_dev_info(aw882xx->dev, "done nothing during calibration");
		return 0;
	}

	if (aw882xx->check_step == READ_RE_VALUE) {
		ret = aw882xx_dsp_read_st(aw882xx->aw_pa, &re, &te);
		if (ret) {
			mm_fb_audio_kevent_named(OPLUS_AUDIO_EVENTID_SPK_ERR, MM_FB_KEY_RATELIMIT_5MIN,
					"payload@@AW882xx SPK%u:R0 read data from adsp error, ret=%d,", \
					aw882xx->aw_pa->channel + 1, ret);
		} else {
			aw882xx_check_speaker_re(aw882xx, re, te);
		}
	}
#if 0
	else if (aw882xx->check_step == ENABLE_F0_CALI_MODE) {
		ret = aw_ar_cali_dev_f0_en_for_feedback(aw882xx->aw_pa);
	} else if (aw882xx->check_step == READ_F0_VALUE) {
		ret = aw882xx_dsp_read_f0(aw882xx->aw_pa, &f0);
		if (ret) {
			mm_fb_audio_kevent_named(OPLUS_AUDIO_EVENTID_SPK_ERR, MM_FB_KEY_RATELIMIT_5MIN,
					"payload@@AW882xx SPK%u:F0 read data from adsp error, ret=%d,", \
					aw882xx->aw_pa->channel + 1, ret);
		} else {
			aw882xx_check_speaker_f0(aw882xx, f0);
		}
	}

	if ((ret == 0) && aw882xx_is_cali_f0_valid(aw882xx)) {
		aw882xx->check_step++;
		aw882xx->check_step = aw882xx->check_step % AW_SPKS_MAX_STEPS;
		delay_ms = g_step_delay[aw882xx->check_step];
		aw_dev_info(aw882xx->dev, "AW882xx SPK%u: next step=%d, delay_ms=%u", \
				aw882xx->aw_pa->channel + 1, aw882xx->check_step, delay_ms);
		queue_delayed_work(aw882xx->work_queue,
				&aw882xx->check_work,
				msecs_to_jiffies(delay_ms));
	} else {
		aw882xx->check_step = 0;
	}
#endif

	return ret;
}

static void aw882xx_check_work(struct work_struct *work)
{
	struct aw882xx *aw882xx = container_of(work, struct aw882xx, check_work.work);

	aw_dev_info(aw882xx->dev, "enter, g_control_fb=0x%x", g_control_fb);
	if (g_chk_err && !(g_control_fb & BYPASS_SPK_ERR_FB_10042)) {
		aw882xx_check_speaker_status(aw882xx);
	}
}

static int aw882xx_set_check_feedback(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);
	int need_chk = ucontrol->value.integer.value[0];

	aw_pr_info("%d", need_chk);
	g_chk_err = need_chk;
	if (!g_chk_err) {
		cancel_delayed_work_sync(&aw882xx->check_work);
		aw882xx->check_step = 0;
	}

	return 0;
}

static int aw882xx_get_check_feedback(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = g_chk_err;
	aw_pr_info("%d", g_chk_err);

	return 0;
}

/* 2024/07/08, Add for smartpa vbatlow err check. */
static int aw882xx_get_vbatlow_cnt(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
			aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
			aw_componet_codec_ops.codec_get_drvdata(codec);

	if (g_chk_err && !(g_control_fb & BYPASS_PA_ERR_FB_10041) &&
		aw882xx->allow_pw && aw882xx->pstream) {
		aw882xx_check_status_reg(aw882xx);
	}
	ucontrol->value.integer.value[0] = aw882xx->vbatlow_cnt;
	aw_pr_info("vbatlow_cnt = %u", aw882xx->vbatlow_cnt);
	aw882xx->vbatlow_cnt = 0;

	return 0;
}

static int aw882xx_set_bypass_feedback(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	g_control_fb = ucontrol->value.integer.value[0];
	aw_pr_info("set %u", g_control_fb);
	return 0;
}

static int aw882xx_get_bypass_feedback(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = g_control_fb;
	aw_pr_info("get %u", g_control_fb);

	return 0;
}

static char const *aw882xx_check_feedback_text[] = {"Off", "On"};
static const struct soc_enum aw882xx_check_feedback_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aw882xx_check_feedback_text), aw882xx_check_feedback_text);

static const struct snd_kcontrol_new aw882xx_check_feedback[] = {
	SOC_ENUM_EXT("AW_CHECK_FEEDBACK", aw882xx_check_feedback_enum,
			   aw882xx_get_check_feedback, aw882xx_set_check_feedback),
	SOC_SINGLE_EXT("PA_BYPASS_FEEDBACK", SND_SOC_NOPM, 0, 0xff, 0,
			aw882xx_get_bypass_feedback, aw882xx_set_bypass_feedback),
};
static const struct snd_kcontrol_new aw882xx_vbatlow_controls[] = {
	SOC_SINGLE_EXT("SpkrLeft PA Vbatlow Count", SND_SOC_NOPM, 0, 0xFFFF, 0,
			aw882xx_get_vbatlow_cnt, NULL),
	SOC_SINGLE_EXT("SpkrRight PA Vbatlow Count", SND_SOC_NOPM, 0, 0xFFFF, 0,
			aw882xx_get_vbatlow_cnt, NULL),
};
#endif /*OPLUS_FEATURE_MM_FEEDBACK*/

#ifdef OPLUS_FEATURE_SPEAKER_MUTE
static int aw882xx_spk_mute_ctrl_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = speaker_mute_control;
	return 0;
}

static int aw882xx_spk_mute_ctrl_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct aw_device *local_dev = NULL;
	struct list_head *pos = NULL;
	struct list_head *dev_list = NULL;
	struct aw882xx *aw882xx = NULL;
	int val = ucontrol->value.integer.value[0];

	ret = aw882xx_dev_get_list_head(&dev_list);
	if (ret < 0) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	if (val == speaker_mute_control) {
		aw_pr_info("Speaker mute is already %s\n", val == 1 ? "on" : "off");
		return 1;
	} else {
		aw_pr_info("Speaker mute set to %s\n", val == 1 ? "on" : "off");
		speaker_mute_control = val;
	}

	list_for_each(pos, dev_list) {
		local_dev = container_of(pos, struct aw_device, list_node);
		aw882xx = (struct aw882xx *)local_dev->private_data;
		if (aw882xx->pstream) {
			if (speaker_mute_control == 1) {
				cancel_delayed_work_sync(&aw882xx->dc_work);
				cancel_delayed_work_sync(&aw882xx->start_work);
				mutex_lock(&aw882xx->lock);
				aw882xx_device_stop(aw882xx->aw_pa);
				mutex_unlock(&aw882xx->lock);
				aw_dev_info(aw882xx->dev, "stop pa");
			} else {
				cancel_delayed_work_sync(&aw882xx->start_work);
				mutex_lock(&aw882xx->lock);
				if (aw882xx->fw_status == AW_DEV_FW_OK)
					aw882xx_start_pa(aw882xx);
				else
					aw_dev_info(aw882xx->dev, "fw_load failed ,can not start PA");
				mutex_unlock(&aw882xx->lock);
			}
		}
	}
	return 0;
}

static const struct snd_kcontrol_new aw882xx_snd_control_spk_mute[] = {
	SOC_ENUM_EXT("Speaker_Mute_Switch", spk_mute_ctrl_enum,
					aw882xx_spk_mute_ctrl_get, aw882xx_spk_mute_ctrl_set),
};
#endif

static int aw882xx_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	int ret = 0;
	aw_snd_soc_codec_t *codec = aw_get_codec(dai);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	aw_dev_info(aw882xx->dev, "mute state=%d", mute);

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
	if (mute) {
		if (g_chk_err && !(g_control_fb & BYPASS_PA_ERR_FB_10041)) {
			aw882xx_check_status_reg(aw882xx);
			g_chk_err = false;
		}
		cancel_delayed_work_sync(&aw882xx->check_work);
		aw882xx->check_step = 0;
	}
#endif /*CONFIG_OPLUS_FEATURE_MM_FEEDBACK*/

	if (stream != SNDRV_PCM_STREAM_PLAYBACK) {
		aw_dev_info(aw882xx->dev, "capture");
		return 0;
	}

	if (mute) {
		aw882xx->pstream = false;
		cancel_delayed_work_sync(&aw882xx->dc_work);
		cancel_delayed_work_sync(&aw882xx->start_work);
		mutex_lock(&aw882xx->lock);
		aw882xx_device_stop(aw882xx->aw_pa);
		mutex_unlock(&aw882xx->lock);
	} else {
		if (aw882xx->fw_status == AW_DEV_FW_FAILED) {
			aw_dev_info(aw882xx->dev, "fw_load failed ,can not start PA");
			return 0;
		}
		aw882xx->pstream = true;
		mutex_lock(&aw882xx->lock);
		/*aw882xx_start_pa(aw882xx);*/
#ifndef OPLUS_ARCH_EXTENDS
		queue_delayed_work(aw882xx->work_queue,
				&aw882xx->start_work, 0);
#else /* OPLUS_ARCH_EXTENDS */
		/*aw882xx_device_start(aw882xx->aw_pa);*/
		aw882xx_start_pa(aw882xx);
#endif /* OPLUS_ARCH_EXTENDS */

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
		if (g_chk_err && !(g_control_fb & BYPASS_SPK_ERR_FB_10042)) {
			queue_delayed_work(aw882xx->work_queue,
					&aw882xx->check_work, msecs_to_jiffies(AW882XX_CHECK_WORK_DELAY));
			aw882xx->check_step = 0;
		}
#endif /*CONFIG_OPLUS_FEATURE_MM_FEEDBACK*/
#ifndef OPLUS_ARCH_EXTENDS
		if (aw882xx->aw_pa->channel == 0) {
#else /* OPLUS_ARCH_EXTENDS */
		if ((aw882xx->aw_pa->channel == 0) && aw882xx->spin_flag) {
#endif /* OPLUS_ARCH_EXTENDS */
			ret = aw882xx_spin_set_record_val(aw882xx->aw_pa);
			if (ret)
				aw_dev_err(aw882xx->dev, "set spin error, ret=%d", ret);
		}
		mutex_unlock(&aw882xx->lock);

	}

	return ret;
}

static const struct snd_soc_dai_ops aw882xx_dai_ops = {
	.startup = aw882xx_startup,
	.set_fmt = aw882xx_set_fmt,
	.set_sysclk = aw882xx_set_dai_sysclk,
	.hw_params = aw882xx_hw_params,
	.mute_stream = aw882xx_mute,
	.shutdown = aw882xx_shutdown,
};

/*****************************************************
 *
 * snd_soc_codec_driver | snd_soc_component_driver|
 *
 *****************************************************/

static int aw882xx_profile_info(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_info *uinfo)
{
	int count, ret = 0;
	int res = 0;
	char *name = NULL;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;

	count = aw882xx_dev_get_profile_count(aw882xx->aw_pa);
	if (count <= 0) {
		uinfo->value.enumerated.items = 0;
		aw_dev_err(aw882xx->dev, "get count[%d] failed ", count);
		return 0;
	}

	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	name = uinfo->value.enumerated.name;
	count = uinfo->value.enumerated.item;
	ret = aw88xx_dev_get_profile_name(aw882xx->aw_pa, name, count);
	if (ret) {
		res = strscpy(uinfo->value.enumerated.name, "null", strlen("null") + 1);
		if (res < 0)
			aw_dev_err(aw882xx->dev, "copy enumerated name failed");
	}

	return 0;
}

static int aw882xx_profile_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = aw882xx_dev_get_profile_index(aw882xx->aw_pa);
	aw_dev_dbg(codec->dev, "profile index [%d]", aw882xx_dev_get_profile_index(aw882xx->aw_pa));
	return 0;

}

static int aw882xx_profile_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);
	int cur_index;

	if (aw882xx->dbg_en_prof == false) {
		aw_dev_info(codec->dev, "profile close ");
		return 0;
	}

	/* check value valid */
	ret = aw882xx_dev_check_profile_index(aw882xx->aw_pa, ucontrol->value.integer.value[0]);
	if (ret) {
		aw_dev_info(codec->dev, "unsupported index %d",
				(int)ucontrol->value.integer.value[0]);
		return -EINVAL;
	}

	/*check cur_index == set value*/
	cur_index = aw882xx_dev_get_profile_index(aw882xx->aw_pa);
	if (cur_index == ucontrol->value.integer.value[0]) {
		aw_dev_info(codec->dev, "index no change");
		return 0;
	}

	mutex_lock(&aw882xx->lock);
	aw882xx_dev_set_profile_index(aw882xx->aw_pa, ucontrol->value.integer.value[0]);
	if (aw882xx->que_dela_work) {
		for (int i = 0; i < AW_START_RETRIES; i++) {
			ret = aw882xx_dev_reg_update(aw882xx->aw_pa, aw882xx->phase_sync);
			if (ret) {
				aw_dev_err(aw882xx->dev, "fw update failed, cnt:%d", i);
				continue;
			}
		  }
	}
	/*pstream = 0 no pcm just set status*/
	#ifndef OPLUS_FEATURE_SPEAKER_MUTE
	if (aw882xx->pstream && aw882xx->allow_pw) {
	#else
	if (aw882xx->pstream && aw882xx->allow_pw && !speaker_mute_control) {
	#endif
		aw882xx_device_stop(aw882xx->aw_pa);
		aw882xx_start_pa(aw882xx);
	}
	mutex_unlock(&aw882xx->lock);
	aw_dev_info(codec->dev, "prof id %d", (int)ucontrol->value.integer.value[0]);

	return 1;
}

static int aw882xx_switch_info(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_info *uinfo)
{
	int count = 0;
	int ret = 0;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	count = 2;

	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	ret = strscpy(uinfo->value.enumerated.name,
		aw882xx_switch[uinfo->value.enumerated.item],
		strlen(aw882xx_switch[uinfo->value.enumerated.item]) + 1);
	if (ret < 0)
		aw_pr_err("copy switch name failed");

	return 0;
}

static int aw882xx_switch_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = aw882xx->allow_pw;

	return 0;

}

static int aw882xx_switch_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	if (aw882xx->pstream) {
		if (ucontrol->value.integer.value[0] == 0) {
#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
/*MTK platform set aw_dev_0_switch before pcm close, so check reg here rather than aw882xx_mute */
			if (g_chk_err && !(g_control_fb & BYPASS_PA_ERR_FB_10041)) {
				aw882xx_check_status_reg(aw882xx);
				g_chk_err = false;
			}
			cancel_delayed_work_sync(&aw882xx->check_work);
			aw882xx->check_step = 0;
#endif /* CONFIG_OPLUS_FEATURE_MM_FEEDBACK */
			cancel_delayed_work_sync(&aw882xx->dc_work);
			cancel_delayed_work_sync(&aw882xx->start_work);
			mutex_lock(&aw882xx->lock);
			aw882xx_device_stop(aw882xx->aw_pa);
			aw882xx->allow_pw = false;
			mutex_unlock(&aw882xx->lock);
			aw_dev_info(aw882xx->dev, "stop pa");
		} else {
#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
			cancel_delayed_work_sync(&aw882xx->check_work);
			aw882xx->check_step = 0;
#endif /* CONFIG_OPLUS_FEATURE_MM_FEEDBACK */
			cancel_delayed_work_sync(&aw882xx->start_work);
			mutex_lock(&aw882xx->lock);
			aw882xx->allow_pw = true;
			if (aw882xx->fw_status == AW_DEV_FW_OK)
				aw882xx_start_pa(aw882xx);
			else
				aw_dev_info(aw882xx->dev, "fw_load failed ,can not start PA");
			mutex_unlock(&aw882xx->lock);
		}
	} else {
		mutex_lock(&aw882xx->lock);
		if (ucontrol->value.integer.value[0])
			aw882xx->allow_pw = true;
		else
			aw882xx->allow_pw = false;
		mutex_unlock(&aw882xx->lock);
	}

	return 0;
}

static int aw882xx_monitor_info(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_info *uinfo)
{
	int count = 0;
	int ret = 0;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	count = 2;

	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	ret = strscpy(uinfo->value.enumerated.name,
		aw882xx_switch[uinfo->value.enumerated.item],
		strlen(aw882xx_switch[uinfo->value.enumerated.item]) + 1);
	if (ret < 0)
		aw_pr_err("copy switch name failed");

	return 0;
}

static int aw882xx_monitor_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_device *aw_dev = aw882xx->aw_pa;

	ucontrol->value.integer.value[0] =
			aw_dev->monitor_desc.monitor_cfg.monitor_switch;

	aw_dev_info(aw882xx->dev, "monitor switch is %ld",
				ucontrol->value.integer.value[0]);
	return 0;

}

static int aw882xx_monitor_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_device *aw_dev = aw882xx->aw_pa;
	uint32_t ctrl_value = 0;

	ctrl_value = ucontrol->value.integer.value[0];

	aw_dev_info(aw_dev->dev, "set monitor switch is %d", ctrl_value);

	if (aw_dev->monitor_desc.monitor_cfg.monitor_switch == ctrl_value)
		return 0;

	aw_dev->monitor_desc.monitor_cfg.monitor_switch = ctrl_value;
	if (ctrl_value)
		aw882xx_monitor_start(&aw_dev->monitor_desc);

	return 1;
}

static int aw882xx_hal_monitor_work_info(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = INT_MIN;
	uinfo->value.integer.max = 0;

	return 0;
}

static int aw882xx_hal_monitor_work_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_device *aw_dev = aw882xx->aw_pa;
	uint32_t vmax = 0;

	mutex_lock(&aw882xx->lock);
	aw882xx_dev_monitor_hal_work(aw_dev, &vmax);

	ucontrol->value.integer.value[0] = vmax;
	mutex_unlock(&aw882xx->lock);

	aw_dev_dbg(aw882xx->dev, "vmax is 0x%x", vmax);
	return 0;

}

static int aw882xx_volume_info(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_volume_desc *vol_desc = &aw882xx->aw_pa->volume_desc;

	/* set kcontrol info */
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = vol_desc->mute_volume;

	return 0;
}

static int aw882xx_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_volume_desc *vol_desc = &aw882xx->aw_pa->volume_desc;

	ucontrol->value.integer.value[0] = vol_desc->ctl_volume;

	aw_dev_info(aw882xx->dev, "ucontrol->value.integer.value[0]=%d",
					vol_desc->ctl_volume);

	return 0;
}

static int aw882xx_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int value = 0;
	int compared_vol = 0;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);
	struct aw_volume_desc *vol_desc = &aw882xx->aw_pa->volume_desc;

	value = ucontrol->value.integer.value[0];
	if ((value > vol_desc->mute_volume) || (value < 0)) {
		aw_dev_err(aw882xx->dev, "value over range\n");
		return -EINVAL;
	}

	aw_dev_info(aw882xx->dev, "ucontrol->value.integer.value[0]=%d", value);

	if (vol_desc->ctl_volume == value)
		return 0;

	vol_desc->ctl_volume = value;
	compared_vol = AW_GET_MAX_VALUE(vol_desc->ctl_volume,
						vol_desc->monitor_volume);

	aw882xx_dev_set_volume(aw882xx->aw_pa, compared_vol);

	return 1;
}

static int aw882xx_dynamic_create_controls(struct aw882xx *aw882xx)
{
	struct snd_kcontrol_new *aw882xx_dev_control = NULL;
	char *kctl_name = NULL;

	aw882xx_dev_control = devm_kzalloc(aw882xx->codec->dev,
				sizeof(struct snd_kcontrol_new) * AW_KCTL_NUM, GFP_KERNEL);
	if (aw882xx_dev_control == NULL) {
		aw_dev_err(aw882xx->codec->dev, "kcontrol malloc failed!");
		return -ENOMEM;
	}

	kctl_name = devm_kzalloc(aw882xx->codec->dev, AW_NAME_BUF_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;

	snprintf(kctl_name, AW_NAME_BUF_MAX, "aw_dev_%d_prof", aw882xx->aw_pa->channel);

	aw882xx_dev_control[KCTL_TYPE_PROFILE].name = kctl_name;
	aw882xx_dev_control[KCTL_TYPE_PROFILE].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw882xx_dev_control[KCTL_TYPE_PROFILE].info = aw882xx_profile_info;
	aw882xx_dev_control[KCTL_TYPE_PROFILE].get = aw882xx_profile_get;
	aw882xx_dev_control[KCTL_TYPE_PROFILE].put = aw882xx_profile_set;

	kctl_name = devm_kzalloc(aw882xx->codec->dev, AW_NAME_BUF_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;

	snprintf(kctl_name, AW_NAME_BUF_MAX, "aw_dev_%d_switch", aw882xx->aw_pa->channel);

	aw882xx_dev_control[KCTL_TYPE_SWITCH].name = kctl_name;
	aw882xx_dev_control[KCTL_TYPE_SWITCH].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw882xx_dev_control[KCTL_TYPE_SWITCH].info = aw882xx_switch_info;
	aw882xx_dev_control[KCTL_TYPE_SWITCH].get = aw882xx_switch_get;
	aw882xx_dev_control[KCTL_TYPE_SWITCH].put = aw882xx_switch_set;

	kctl_name = devm_kzalloc(aw882xx->codec->dev, AW_NAME_BUF_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;

	snprintf(kctl_name, AW_NAME_BUF_MAX, "aw_dev_%d_monitor", aw882xx->aw_pa->channel);

	aw882xx_dev_control[KCTL_TYPE_MONITOR].name = kctl_name;
	aw882xx_dev_control[KCTL_TYPE_MONITOR].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw882xx_dev_control[KCTL_TYPE_MONITOR].info = aw882xx_monitor_info;
	aw882xx_dev_control[KCTL_TYPE_MONITOR].get = aw882xx_monitor_get;
	aw882xx_dev_control[KCTL_TYPE_MONITOR].put = aw882xx_monitor_set;


	kctl_name = devm_kzalloc(aw882xx->codec->dev, AW_NAME_BUF_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;

	snprintf(kctl_name, AW_NAME_BUF_MAX, "aw_dev_%d_volume", aw882xx->aw_pa->channel);

	aw882xx_dev_control[KCTL_TYPE_VOLUME].name = kctl_name;
	aw882xx_dev_control[KCTL_TYPE_VOLUME].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw882xx_dev_control[KCTL_TYPE_VOLUME].info = aw882xx_volume_info;
	aw882xx_dev_control[KCTL_TYPE_VOLUME].get = aw882xx_volume_get;
	aw882xx_dev_control[KCTL_TYPE_VOLUME].put = aw882xx_volume_put;


	kctl_name = devm_kzalloc(aw882xx->codec->dev, AW_NAME_BUF_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;

	snprintf(kctl_name, AW_NAME_BUF_MAX, "aw_dev_%d_hal_mon_work", aw882xx->aw_pa->channel);

	aw882xx_dev_control[KCTL_TYPE_MON_HAL].name = kctl_name;
	aw882xx_dev_control[KCTL_TYPE_MON_HAL].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw882xx_dev_control[KCTL_TYPE_MON_HAL].access = SNDRV_CTL_ELEM_ACCESS_READ;
	aw882xx_dev_control[KCTL_TYPE_MON_HAL].info = aw882xx_hal_monitor_work_info;
	aw882xx_dev_control[KCTL_TYPE_MON_HAL].get = aw882xx_hal_monitor_work_get;

	aw_componet_codec_ops.add_codec_controls(aw882xx->codec,
						aw882xx_dev_control, AW_KCTL_NUM);

	return 0;
}

static void aw882xx_request_firmware(struct work_struct *work)
{
	int ret = -1;
	struct aw882xx *aw882xx =
			container_of(work, struct aw882xx, fw_work.work);
	const struct firmware *cont = NULL;
	struct aw_container *aw_cfg = NULL;

	aw882xx->fw_status = AW_DEV_FW_FAILED;
	ret = request_firmware(&cont, ACF_BIN_NAME, aw882xx->dev);
	if ((ret) || (!cont)) {
		aw_dev_info(aw882xx->dev, "load [%s] failed!", ACF_BIN_NAME);
		if (aw882xx->fw_retry_cnt == AW_READ_CHIPID_RETRIES) {
			aw882xx->fw_retry_cnt = 0;
		} else {
			aw882xx->fw_retry_cnt++;
			/* sleep 1s */
			msleep(1000);
			aw_dev_info(aw882xx->dev, "load [%s] try [%d]!",
						ACF_BIN_NAME, aw882xx->fw_retry_cnt);
			aw882xx_request_firmware(work);
		}
		return;
	}

	aw_dev_info(aw882xx->dev, "load [%s] , file size: [%zu]",
			ACF_BIN_NAME, cont ? cont->size : 0);

	mutex_lock(&g_aw882xx_lock);
	if (g_awinic_cfg == NULL) {
		aw_cfg = vzalloc(cont->size + sizeof(int));
		if (aw_cfg == NULL) {
			release_firmware(cont);
			aw_dev_err(aw882xx->dev, "malloc failed");
			mutex_unlock(&g_aw882xx_lock);
			return;
		}
		aw_cfg->len = cont->size;
		memcpy(aw_cfg->data, cont->data, cont->size);
		release_firmware(cont);
		ret = aw882xx_dev_parse_check_acf(aw_cfg);
		if (ret) {
			aw_dev_err(aw882xx->dev, "Load [%s] failed ....!", ACF_BIN_NAME);
			vfree(aw_cfg);
			aw_cfg = NULL;
			mutex_unlock(&g_aw882xx_lock);
			return;
		}
		g_awinic_cfg = aw_cfg;
	} else {
		aw_cfg = g_awinic_cfg;
		release_firmware(cont);
		aw_dev_info(aw882xx->dev, "[%s] already loaded...", ACF_BIN_NAME);
	}
	mutex_unlock(&g_aw882xx_lock);

	mutex_lock(&aw882xx->lock);
	/*aw device init*/
	ret = aw882xx_device_init(aw882xx->aw_pa, aw_cfg);
	if (ret < 0) {
		aw_dev_info(aw882xx->dev, "dev init failed");
		mutex_unlock(&aw882xx->lock);
		return;
	}

	/*create kcontrol by profile*/
	aw882xx_dynamic_create_controls(aw882xx);

	aw882xx->fw_status = AW_DEV_FW_OK;
	aw882xx->fw_retry_cnt = 0;

	mutex_unlock(&aw882xx->lock);
	if (aw882xx->que_dela_work) {
		for (int i = 0; i < AW_START_RETRIES; i++) {
			ret = aw882xx_dev_reg_update(aw882xx->aw_pa, aw882xx->phase_sync);
			if (ret) {
				aw_dev_err(aw882xx->dev, "fw update failed, cnt:%d", i);
				continue;
			}
		  }
	}
}

static void aw882xx_startup_work(struct work_struct *work)
{
	struct aw882xx *aw882xx = container_of(work, struct aw882xx, start_work.work);

	aw_dev_info(aw882xx->dev, "enter");

	mutex_lock(&aw882xx->lock);
	aw882xx_start_pa(aw882xx);
	mutex_unlock(&aw882xx->lock);
}

static void aw882xx_dc_prot_work(struct work_struct *work)
{
	struct aw882xx *aw882xx = container_of(work, struct aw882xx, dc_work.work);
	int dc_status = -1;
	int dev_status = aw882xx_dev_status(aw882xx->aw_pa);

	if (aw882xx->dc_flag) {
		if (dev_status) {
			dc_status = aw882xx_dev_dc_status(aw882xx->aw_pa);
			if (dc_status > 0) {
#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
				g_chk_err = false;
				cancel_delayed_work_sync(&aw882xx->check_work);
				aw882xx->check_step = 0;
#endif /* CONFIG_OPLUS_FEATURE_MM_FEEDBACK */
				cancel_delayed_work_sync(&aw882xx->start_work);
				mutex_lock(&aw882xx->lock);
				aw882xx_device_stop(aw882xx->aw_pa);
				mutex_unlock(&aw882xx->lock);
			} else {
				queue_delayed_work(aw882xx->work_queue,
					&aw882xx->dc_work,
					msecs_to_jiffies(AW882XX_DC_DELAY_TIME));
			}
		}
	}
}

#ifdef AW882XX_IRQ_START_FLAG
static void aw882xx_irq_restart(struct aw882xx *aw882xx)
{
	int ret;

	aw_dev_dbg(aw882xx->dev, "enter");

	mutex_lock(&aw882xx->lock);

	/*stop pa*/
	aw882xx_device_stop(aw882xx->aw_pa);

	/*hw reset*/
	aw882xx_hw_reset(aw882xx);

	/*aw reinit*/
	if (aw882xx->fw_status == AW_DEV_FW_OK) {
		ret = aw882xx_device_irq_reinit(aw882xx->aw_pa);
		if (ret < 0) {
			aw_dev_err(aw882xx->dev, "irq reinit failed");
			goto failed_exit;
		}

		#ifndef OPLUS_FEATURE_SPEAKER_MUTE
		if (aw882xx->allow_pw && aw882xx->pstream) {
		#else
		if (aw882xx->allow_pw && aw882xx->pstream && !speaker_mute_control) {
		#endif
			ret = aw882xx_device_start(aw882xx->aw_pa);
			if (ret) {
				aw_dev_err(aw882xx->dev, "start failed");
				goto failed_exit;
			}
		} else {
			aw_dev_info(aw882xx->dev, "allow_pw [%d] ,pstream[%d], not start",
					aw882xx->allow_pw, aw882xx->pstream);
		}
	} else {
		aw_dev_err(aw882xx->dev, "fw not load ,cannot init device");
	}
failed_exit:
	mutex_unlock(&aw882xx->lock);
}

static void aw882xx_interrupt_work(struct work_struct *work)
{
	struct aw882xx *aw882xx = container_of(work, struct aw882xx, interrupt_work.work);
	int16_t reg_value;
	int ret;

	aw_dev_info(aw882xx->dev, "enter");

	/*read reg value*/
	ret = aw882xx_dev_get_int_status(aw882xx->aw_pa, &reg_value);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev, "get init_reg value failed");
	} else {
		aw_dev_info(aw882xx->dev, "int value 0x%x", reg_value);
		if (aw882xx->aw_pa->ops.aw_get_irq_type) {
			ret = aw882xx->aw_pa->ops.aw_get_irq_type(aw882xx->aw_pa, reg_value);
			if (ret != INT_TYPE_NONE) {
				aw882xx_irq_restart(aw882xx);
				return;
			}
		}
	}

	/*clear init reg*/
	aw882xx_dev_clear_int_status(aw882xx->aw_pa);

	/*unmask interrupt*/
	aw882xx_dev_set_intmask(aw882xx->aw_pa, true);
}
#else
static void aw882xx_interrupt_work(struct work_struct *work)
{
	struct aw882xx *aw882xx = container_of(work, struct aw882xx, interrupt_work.work);
	struct aw_device *aw_dev = aw882xx->aw_pa;
	unsigned int reg_value = 0;

	aw882xx_i2c_read(aw882xx, aw_dev->sysst_desc.reg, &reg_value);
	aw_dev_info(aw882xx->dev, "SYSST is 0x%04x", (uint16_t)reg_value);

	aw882xx_i2c_read(aw882xx, aw_dev->int_desc.st_reg, &reg_value);
	aw_dev_info(aw882xx->dev, "SYSINT is 0x%04x", (uint16_t)reg_value);

	/*unmask interrupt*/
	aw882xx_dev_set_intmask(aw882xx->aw_pa, true);
}
#endif

static int aw882xx_set_rx_en(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	uint32_t ctrl_value = 0;
	struct aw_device *aw_dev = NULL;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	aw_dev_dbg(aw882xx->dev, "ucontrol->value.integer.value[0]=%ld",
				ucontrol->value.integer.value[0]);

	aw_dev = aw882xx->aw_pa;
	ctrl_value = ucontrol->value.integer.value[0];

	if (aw882xx->pstream) {
		ret = aw882xx_dev_set_afe_module_en(AW_RX_MODULE, ctrl_value);
		if (ret)
			aw_dev_err(aw882xx->dev, "dsp_msg error, ret=%d", ret);
	} else {
		aw_dev_info(aw882xx->dev, "stream no start only record");
	}

	g_algo_rx_en = ctrl_value;
	aw_dev_info(aw882xx->dev, "set value %d", ctrl_value);
	return 0;
}

static int aw882xx_get_rx_en(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	uint32_t ctrl_value = 0;
	struct aw_device *aw_dev = NULL;

	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	aw_dev = aw882xx->aw_pa;

	if (aw882xx->pstream) {
		ret = aw882xx_dev_get_afe_module_en(AW_RX_MODULE, &ctrl_value);
		if (ret)
			aw_dev_err(aw882xx->dev, "dsp_msg error, ret=%d", ret);

		ucontrol->value.integer.value[0] = ctrl_value;
	} else {
		ucontrol->value.integer.value[0] = g_algo_rx_en;
		aw_dev_info(aw882xx->dev, "no stream, use record value");
	}

	aw_dev_dbg(aw882xx->dev, "aw882xx_rx_enable %ld",
				ucontrol->value.integer.value[0]);
	return 0;
}

static int aw882xx_set_tx_en(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	uint32_t ctrl_value = 0;
	struct aw_device *aw_dev = NULL;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	aw_dev_dbg(aw882xx->dev, "ucontrol->value.integer.value[0]=%ld",
			ucontrol->value.integer.value[0]);

	aw_dev = aw882xx->aw_pa;
	ctrl_value = ucontrol->value.integer.value[0];

	if (aw882xx->pstream) {
		ret = aw882xx_dev_set_afe_module_en(AW_TX_MODULE, ctrl_value);
		if (ret)
			aw_dev_err(aw882xx->dev, "dsp_msg error, ret=%d", ret);
	} else {
		aw_dev_info(aw882xx->dev, "stream no start only record");
	}

	g_algo_tx_en = ctrl_value;
	aw_dev_info(aw882xx->dev, "set value %d", ctrl_value);
	return 0;
}

static int aw882xx_get_tx_en(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	uint32_t ctrl_value = 0;
	struct aw_device *aw_dev = NULL;

	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);
	aw_dev = aw882xx->aw_pa;

	if (aw882xx->pstream) {
		ret = aw882xx_dev_get_afe_module_en(AW_TX_MODULE, &ctrl_value);
		if (ret) {
			aw_dev_err(aw882xx->dev, "dsp_msg error, ret=%d", ret);
			ctrl_value = 0;
		}
		ucontrol->value.integer.value[0] = ctrl_value;
	} else {
		ucontrol->value.integer.value[0] = g_algo_tx_en;
		aw_dev_info(aw882xx->dev, "no stream, use record value");
	}

	aw_dev_dbg(aw882xx->dev, "aw882xx_tx_enable %ld",
				ucontrol->value.integer.value[0]);
	return 0;
}

static int aw882xx_set_copp_en(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	uint32_t ctrl_value = 0;
	struct aw_device *aw_dev = NULL;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	aw_dev_dbg(aw882xx->dev, "ucontrol->value.integer.value[0]=%ld",
			ucontrol->value.integer.value[0]);

	aw_dev = aw882xx->aw_pa;
	ctrl_value = ucontrol->value.integer.value[0];

	if (aw882xx->pstream) {
		ret = aw882xx_dev_set_copp_module_en(ctrl_value);
		if (ret)
			aw_dev_err(aw882xx->dev, "dsp_msg error, ret=%d", ret);
	} else {
		aw_dev_info(aw882xx->dev, "stream no start only record");
	}

	g_algo_copp_en = ctrl_value;
	aw_dev_info(aw882xx->dev, "set value %d", ctrl_value);

	return 0;
}

static int aw882xx_get_copp_en(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = g_algo_copp_en;

	aw_dev_dbg(aw882xx->dev, "done nothing");
	return 0;
}

void aw882xx_kcontorl_set(struct aw882xx *aw882xx)
{
	int ret;

	ret = aw882xx_dev_set_afe_module_en(AW_RX_MODULE, g_algo_rx_en);
	if (ret)
		aw_dev_err(aw882xx->dev, "afe set, ret=%d", ret);

	ret = aw882xx_dev_set_copp_module_en(g_algo_copp_en);
	if (ret)
		aw_dev_err(aw882xx->dev, "copp set error, ret=%d", ret);
}

static int aw882xx_set_spin(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	uint32_t ctrl_value = 0;
	struct aw_device *aw_dev = NULL;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	aw_dev_dbg(aw882xx->dev, "ucontrol->value.integer.value[0]=%ld",
			ucontrol->value.integer.value[0]);

	aw_dev = aw882xx->aw_pa;
	ctrl_value = ucontrol->value.integer.value[0];
	if (ctrl_value >= ARRAY_SIZE(aw882xx_spin)) {
		aw_dev_err(aw_dev->dev, "spin value %d is unsupport", ctrl_value);
		return -EINVAL;
	}

	ret = aw882xx_spin_value_set(aw_dev, ctrl_value, aw882xx->pstream);
	if (ret)
		aw_dev_err(aw882xx->dev, "set spin error, ret = %d", ret);
	return ret;
}

static int aw882xx_get_spin(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret = -EINVAL;
	struct aw_device *aw_dev = NULL;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);
	uint32_t ctrl_value = 0;

	aw_dev = aw882xx->aw_pa;
	ret = aw882xx_spin_value_get(aw_dev, &ctrl_value, aw882xx->pstream);
	if (ret) {
		aw_dev_err(aw882xx->dev, "get spin failed!, ret = %d", ret);
		ctrl_value = 0;
	}
	ucontrol->value.integer.value[0] = ctrl_value;

	aw_dev_dbg(aw882xx->dev, "spin value is %s", aw882xx_spin[ctrl_value]);
	return 0;
}

static int aw882xx_get_fade_in_time(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int time;

	aw882xx_dev_get_fade_time(&time, true);
	ucontrol->value.integer.value[0] = time;

	aw_pr_dbg("step time %ld", ucontrol->value.integer.value[0]);

	return 0;
}

static int aw882xx_set_fade_in_time(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] > mc->max) {
		aw_pr_dbg("set val %ld overflow %d",
			ucontrol->value.integer.value[0], mc->max);
		return 0;
	}
	aw882xx_dev_set_fade_time(ucontrol->value.integer.value[0], true);

	aw_pr_dbg("step time %ld", ucontrol->value.integer.value[0]);
	return 0;
}

static int aw882xx_get_fade_out_time(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int time;

	aw882xx_dev_get_fade_time(&time, false);
	ucontrol->value.integer.value[0] = time;

	aw_pr_dbg("step time %ld", ucontrol->value.integer.value[0]);

	return 0;
}

static int aw882xx_set_fade_out_time(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;

	if (ucontrol->value.integer.value[0] > mc->max) {
		aw_pr_dbg("set val %ld overflow %d",
			ucontrol->value.integer.value[0], mc->max);
		return 0;
	}

	aw882xx_dev_set_fade_time(ucontrol->value.integer.value[0], false);

	aw_pr_dbg("step time %ld", ucontrol->value.integer.value[0]);

	return 0;
}


static int aw882xx_hal_get_monitor_time(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int time;
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	aw882xx_dev_monitor_hal_get_time(aw882xx->aw_pa, &time);
	ucontrol->value.integer.value[0] = time;

	aw_pr_dbg("get monitor time %ld", ucontrol->value.integer.value[0]);

	return 0;
}

static int aw882xx_hal_set_monitor_time(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{

	return 0;
}

static const struct soc_enum aw882xx_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aw882xx_switch), aw882xx_switch),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aw882xx_spin), aw882xx_spin),
};

static struct snd_kcontrol_new aw882xx_controls[] = {
	SOC_ENUM_EXT("aw882xx_rx_switch", aw882xx_snd_enum[0],
		aw882xx_get_rx_en, aw882xx_set_rx_en),
	SOC_ENUM_EXT("aw882xx_tx_switch", aw882xx_snd_enum[0],
		aw882xx_get_tx_en, aw882xx_set_tx_en),
	SOC_ENUM_EXT("aw882xx_copp_switch", aw882xx_snd_enum[0],
		aw882xx_get_copp_en, aw882xx_set_copp_en),
	SOC_SINGLE_EXT("aw882xx_fadein_us", 0, 0, 1000000, 0,
		aw882xx_get_fade_in_time, aw882xx_set_fade_in_time),
	SOC_SINGLE_EXT("aw882xx_fadeout_us", 0, 0, 1000000, 0,
		aw882xx_get_fade_out_time, aw882xx_set_fade_out_time),
	SOC_SINGLE_EXT("aw882xx_hal_monitor_time", 0, 0, 100000, 0,
		aw882xx_hal_get_monitor_time, aw882xx_hal_set_monitor_time),

};

#ifdef OPLUS_ARCH_EXTENDS
static char const *spk_l_mute_ctrl_text[] = {"Off", "On"};
static char const *spk_r_mute_ctrl_text[] = {"Off", "On"};
static char const *spk_l_reboot_ctrl_text[] = {"Off", "On"};
static char const *spk_r_reboot_ctrl_text[] = {"Off", "On"};
static const struct soc_enum spk_l_mute_ctrl_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_l_mute_ctrl_text), spk_l_mute_ctrl_text);
static const struct soc_enum spk_r_mute_ctrl_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_r_mute_ctrl_text), spk_r_mute_ctrl_text);
static const struct soc_enum spk_l_reboot_ctrl_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_l_reboot_ctrl_text), spk_l_reboot_ctrl_text);
static const struct soc_enum spk_r_reboot_ctrl_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_r_reboot_ctrl_text), spk_r_reboot_ctrl_text);

static int aw882xx_spk_l_mute_ctrl_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = aw882xx->speaker_force_mute;
	return 0;
}

static int aw882xx_spk_l_mute_ctrl_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	char *prof_name = NULL;
	uint32_t ctrl_value = 0;
	ctrl_value = ucontrol->value.integer.value[0];
	aw_dev_info(aw882xx->dev, "AW_L operating, set spk_l_mute ctrl is %d", ctrl_value);

	if (ctrl_value == aw882xx->speaker_force_mute) {
		aw_dev_info(aw882xx->dev, "AW_L operating, Speaker_L mute is already %d", ctrl_value);
		return 1;
	}
    prof_name = aw882xx_dev_get_prof_name(aw882xx->aw_pa, aw882xx_dev_get_profile_index(aw882xx->aw_pa));
	if (prof_name != NULL) {
		if (strcmp(prof_name, "Receiver") == 0) {
			aw_dev_info(aw882xx->dev, "Speaker_L is a receiver, no need to mute");
			return 1;
		}
	}

	aw_dev_info(aw882xx->dev, "AW_L operating, Speaker_L mute set to %d", ctrl_value);
	aw882xx->speaker_force_mute = ctrl_value;

	if (aw882xx->pstream) {
		if (ucontrol->value.integer.value[0] == 1) {
			aw_dev_info(aw882xx->dev, "AW_L operating, setting mute state...");
#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
			g_chk_err = false;
			cancel_delayed_work_sync(&aw882xx->check_work);
			aw882xx->check_step = 0;
#endif /* CONFIG_OPLUS_FEATURE_MM_FEEDBACK */
			cancel_delayed_work_sync(&aw882xx->dc_work);
			cancel_delayed_work_sync(&aw882xx->start_work);
			mutex_lock(&aw882xx->lock);
			aw882xx_device_stop(aw882xx->aw_pa);
			aw882xx->allow_pw = false;
			mutex_unlock(&aw882xx->lock);
			aw_dev_info(aw882xx->dev, "stop pa");
		} else {
			aw_dev_info(aw882xx->dev, "AW_L operating, setting unmute state...");
			cancel_delayed_work_sync(&aw882xx->start_work);
			mutex_lock(&aw882xx->lock);
			aw882xx->allow_pw = true;
			if (aw882xx->fw_status == AW_DEV_FW_OK)
				aw882xx_start_pa(aw882xx);
			else
				aw_dev_info(aw882xx->dev, "fw_load failed ,can not start PA");
			mutex_unlock(&aw882xx->lock);
		}
	} else {
		mutex_lock(&aw882xx->lock);
		if (!ucontrol->value.integer.value[0])
			aw882xx->allow_pw = true;
		else
			aw882xx->allow_pw = false;
		mutex_unlock(&aw882xx->lock);
	}

	return 0;
}

static int aw882xx_spk_r_mute_ctrl_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = aw882xx->speaker_force_mute;
	return 0;
}

static int aw882xx_spk_r_mute_ctrl_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	uint32_t ctrl_value = 0;
	ctrl_value = ucontrol->value.integer.value[0];
	aw_dev_info(aw882xx->dev, "sAW_R operating, et spk_l_mute ctrl is %d", ctrl_value);

	if (ctrl_value == aw882xx->speaker_force_mute) {
		aw_dev_info(aw882xx->dev, "AW_R operating, Speaker_R mute is already %d", ctrl_value);
		return 1;
	}

	aw_dev_info(aw882xx->dev, "AW_R operating, Speaker_R mute set to %d", ctrl_value);
	aw882xx->speaker_force_mute = ctrl_value;

	if (aw882xx->pstream) {
		if (ucontrol->value.integer.value[0] == 1) {
			aw_dev_info(aw882xx->dev, "AW_R operating, setting mute state...");
#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
			g_chk_err = false;
			cancel_delayed_work_sync(&aw882xx->check_work);
			aw882xx->check_step = 0;
#endif /* CONFIG_OPLUS_FEATURE_MM_FEEDBACK */
			cancel_delayed_work_sync(&aw882xx->dc_work);
			cancel_delayed_work_sync(&aw882xx->start_work);
			mutex_lock(&aw882xx->lock);
			aw882xx_device_stop(aw882xx->aw_pa);
			aw882xx->allow_pw = false;
			mutex_unlock(&aw882xx->lock);
			aw_dev_info(aw882xx->dev, "stop pa");
		} else {
			aw_dev_info(aw882xx->dev, "AW_R operating, setting unmute state...");
			cancel_delayed_work_sync(&aw882xx->start_work);
			mutex_lock(&aw882xx->lock);
			aw882xx->allow_pw = true;
			if (aw882xx->fw_status == AW_DEV_FW_OK)
				aw882xx_start_pa(aw882xx);
			else
				aw_dev_info(aw882xx->dev, "fw_load failed ,can not start PA");
			mutex_unlock(&aw882xx->lock);
		}
	} else {
		mutex_lock(&aw882xx->lock);
		if (!ucontrol->value.integer.value[0])
			aw882xx->allow_pw = true;
		else
			aw882xx->allow_pw = false;
		mutex_unlock(&aw882xx->lock);
	}

	return 0;
}

static int aw882xx_spk_reboot_ctrl_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = s_speaker_reboot;
	return 0;
}

static int aw882xx_spk_reboot_ctrl_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	aw_snd_soc_codec_t *codec =
		aw_componet_codec_ops.kcontrol_codec(kcontrol);
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(codec);

	uint32_t ctrl_value = 0;
	ctrl_value = ucontrol->value.integer.value[0];
	aw_dev_info(aw882xx->dev, "AW rebooting, set spk_reboot ctrl is %d", ctrl_value);

	s_speaker_reboot = ctrl_value;

	if (aw882xx->pstream) {
		if (ucontrol->value.integer.value[0] == 1) {
			aw_dev_info(aw882xx->dev, "AW rebooting, setting mute state...");
#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
			g_chk_err = false;
			cancel_delayed_work_sync(&aw882xx->check_work);
			aw882xx->check_step = 0;
#endif /* CONFIG_OPLUS_FEATURE_MM_FEEDBACK */
			cancel_delayed_work_sync(&aw882xx->dc_work);
			cancel_delayed_work_sync(&aw882xx->start_work);
			mutex_lock(&aw882xx->lock);
			aw882xx_device_stop(aw882xx->aw_pa);
			aw882xx->allow_pw = false;
			mutex_unlock(&aw882xx->lock);
			aw_dev_info(aw882xx->dev, "stop pa");
		} else {
			aw_dev_info(aw882xx->dev, "AW rebooting, setting unmute state...");
			cancel_delayed_work_sync(&aw882xx->start_work);
			mutex_lock(&aw882xx->lock);
			aw882xx->allow_pw = true;
			if (aw882xx->fw_status == AW_DEV_FW_OK)
				aw882xx_start_pa(aw882xx);
			else
				aw_dev_info(aw882xx->dev, "fw_load failed ,can not start PA");
			mutex_unlock(&aw882xx->lock);
		}
	} else {
		mutex_lock(&aw882xx->lock);
		if (!ucontrol->value.integer.value[0])
			aw882xx->allow_pw = true;
		else
			aw882xx->allow_pw = false;
		mutex_unlock(&aw882xx->lock);
	}

	return 0;
}

static const struct snd_kcontrol_new aw882xx_snd_control_spk_l_mute[] = {
	SOC_ENUM_EXT("Speaker_L_Mute_Switch", spk_l_mute_ctrl_enum,
					aw882xx_spk_l_mute_ctrl_get, aw882xx_spk_l_mute_ctrl_put),
};

static const struct snd_kcontrol_new aw882xx_snd_control_spk_r_mute[] = {
	SOC_ENUM_EXT("Speaker_R_Mute_Switch", spk_r_mute_ctrl_enum,
					aw882xx_spk_r_mute_ctrl_get, aw882xx_spk_r_mute_ctrl_put),
};

static const struct snd_kcontrol_new aw882xx_snd_control_spk_l_reboot[] = {
	SOC_ENUM_EXT("Speaker_L_Reboot", spk_l_reboot_ctrl_enum,
					aw882xx_spk_reboot_ctrl_get, aw882xx_spk_reboot_ctrl_put),
};

static const struct snd_kcontrol_new aw882xx_snd_control_spk_r_reboot[] = {
	SOC_ENUM_EXT("Speaker_R_Reboot", spk_r_reboot_ctrl_enum,
					aw882xx_spk_reboot_ctrl_get, aw882xx_spk_reboot_ctrl_put),
};
#endif /* OPLUS_ARCH_EXTENDS */

static struct snd_kcontrol_new aw882xx_spin_control[] = {
	SOC_ENUM_EXT("aw882xx_spin_switch", aw882xx_snd_enum[1],
		aw882xx_get_spin, aw882xx_set_spin),
};

static void aw882xx_add_codec_controls(struct aw882xx *aw882xx)
{
	aw_dev_info(aw882xx->dev, "enter");

	aw_componet_codec_ops.add_codec_controls(aw882xx->codec,
				&aw882xx_controls[0], ARRAY_SIZE(aw882xx_controls));

	if (aw882xx->aw_pa->spin_desc.aw_spin_kcontrol_st == AW_SPIN_KCONTROL_ENABLE)
		aw_componet_codec_ops.add_codec_controls(aw882xx->codec,
				aw882xx_spin_control, ARRAY_SIZE(aw882xx_spin_control));
#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
	/* 2022/02/15, Add for smartpa err feedback.*/
	aw_componet_codec_ops.add_codec_controls(aw882xx->codec,
			&aw882xx_check_feedback[0], ARRAY_SIZE(aw882xx_check_feedback));
#endif
}

#ifdef OPLUS_ARCH_EXTENDS
static void aw882xx_add_codec_mute_controls(struct aw882xx *aw882xx)
{
	if (aw882xx->aw_pa->channel == 0x00) {
		snd_soc_add_component_controls(aw882xx->codec,
				   aw882xx_snd_control_spk_l_mute,
				   ARRAY_SIZE(aw882xx_snd_control_spk_l_mute));
		snd_soc_add_component_controls(aw882xx->codec,
				   aw882xx_snd_control_spk_l_reboot,
				   ARRAY_SIZE(aw882xx_snd_control_spk_l_reboot));
	} else if ((aw882xx->aw_pa->channel == 0x01)||(aw882xx->aw_pa->channel == 0xff)) {
		snd_soc_add_component_controls(aw882xx->codec,
				   aw882xx_snd_control_spk_r_mute,
				   ARRAY_SIZE(aw882xx_snd_control_spk_r_mute));
		snd_soc_add_component_controls(aw882xx->codec,
				   aw882xx_snd_control_spk_r_reboot,
				   ARRAY_SIZE(aw882xx_snd_control_spk_r_reboot));
	}
}
#endif

static int aw882xx_append_i2c_suffix(char *format,
		const char **name, struct aw882xx *aw882xx)
{
	char buf[64];
	int i2cbus = aw882xx->i2c->adapter->nr;
	int i2caddr = aw882xx->i2c->addr;

	snprintf(buf, sizeof(buf), format, *name, i2cbus, i2caddr);
	(*name) = aw882xx_devm_kstrdup(aw882xx->dev, buf);
	if (!(*name))
		return -ENOMEM;

	aw_dev_info(aw882xx->dev, "change name is %s", (*name));
	return 0;
}

static int aw882xx_append_channel_suffix(char *format,
		const char **name, struct aw882xx *aw882xx)
{
	char buf[64];
	int channel = aw882xx->aw_pa->channel;

	snprintf(buf, sizeof(buf), format, *name, channel);
	(*name) = aw882xx_devm_kstrdup(aw882xx->dev, buf);
	if (!(*name))
		return -ENOMEM;

	aw_dev_info(aw882xx->dev, "change name is %s", (*name));
	return 0;
}

#ifdef AW_MTK_PLATFORM_WITH_DSP
static const struct snd_soc_dapm_widget aw882xx_dapm_widgets[] = {
	/* playback */
	SND_SOC_DAPM_AIF_IN("AIF_RX", "Speaker_Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUTPUT("audio_out"),
	/* capture */
	SND_SOC_DAPM_AIF_OUT("AIF_TX", "Speaker_Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_INPUT("iv_in"),
};

static const struct snd_soc_dapm_route aw882xx_audio_map[] = {
	{"audio_out", NULL, "AIF_RX"},
	{"AIF_TX", NULL, "iv_in"},
};

#if KERNEL_VERSION(4, 2, 0) > LINUX_VERSION_CODE
static struct snd_soc_dapm_context *snd_soc_codec_get_dapm(struct snd_soc_codec *codec)
{
	return &codec->dapm;
}
#endif
#endif

static void aw882xx_add_widgets(struct aw882xx *aw882xx)
{
#ifdef AW_MTK_PLATFORM_WITH_DSP
	int ret;
	int i = 0;
	struct snd_soc_dapm_widget *aw_widgets = NULL;
	struct snd_soc_dapm_route *aw_route = NULL;
#ifdef AW_KERNEL_VER_OVER_4_19_1
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(aw882xx->codec);
#else
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(aw882xx->codec);
#endif


	/*add widgets*/
	aw_widgets = devm_kzalloc(aw882xx->dev,
				sizeof(struct snd_soc_dapm_widget) * ARRAY_SIZE(aw882xx_dapm_widgets),
				GFP_KERNEL);
	if (!aw_widgets) {
		aw_dev_err(aw882xx->dev, "alloc widget memory failed!");
		return;
	}
	memcpy(aw_widgets, aw882xx_dapm_widgets,
			sizeof(struct snd_soc_dapm_widget) * ARRAY_SIZE(aw882xx_dapm_widgets));

	for (i = 0; i < ARRAY_SIZE(aw882xx_dapm_widgets); i++) {
		if (aw882xx->rename_flag ==  AW_RENAME_ENABLE) {
			if (aw_widgets[i].name) {
				ret = aw882xx_append_channel_suffix("%s_%d",
					&aw_widgets[i].name, aw882xx);
				if (ret) {
					aw_dev_err(aw882xx->dev, "append widget name channel suffix failed!");
					return;
				}
			}

			if (aw_widgets[i].sname) {
				ret = aw882xx_append_channel_suffix("%s_%d",
					&aw_widgets[i].sname, aw882xx);
				if (ret) {
					aw_dev_err(aw882xx->dev, "append widget sname channel suffix failed!");
					return;
				}
			}
		} else {
			if (aw_widgets[i].name) {
				ret = aw882xx_append_i2c_suffix("%s_%d_%x",
					&aw_widgets[i].name, aw882xx);
				if (ret) {
					aw_dev_err(aw882xx->dev, "append widget name i2c suffix failed!");
					return;
				}
			}

			if (aw_widgets[i].sname) {
				ret = aw882xx_append_i2c_suffix("%s_%d_%x",
					&aw_widgets[i].sname, aw882xx);
				if (ret) {
					aw_dev_err(aw882xx->dev, "append widget sname i2c suffix failed!");
					return;
				}
			}
		}

	}

	snd_soc_dapm_new_controls(dapm, aw_widgets, ARRAY_SIZE(aw882xx_dapm_widgets));

	/*add route*/
	aw_route = devm_kzalloc(aw882xx->dev,
				sizeof(struct snd_soc_dapm_route) * ARRAY_SIZE(aw882xx_audio_map),
				GFP_KERNEL);
	if (!aw_route) {
		aw_dev_err(aw882xx->dev, "alloc route memory failed!");
		return;
	}
	memcpy(aw_route, aw882xx_audio_map,
		sizeof(struct snd_soc_dapm_route) * ARRAY_SIZE(aw882xx_audio_map));

	for (i = 0; i < ARRAY_SIZE(aw882xx_audio_map); i++) {
		if (aw882xx->rename_flag ==  AW_RENAME_ENABLE) {
			if (aw_route[i].sink) {
				ret = aw882xx_append_channel_suffix("%s_%d",
					&aw_route[i].sink, aw882xx);
				if (ret < 0) {
					aw_dev_err(aw882xx->dev, "append sink channel suffix failed!");
					return;
				}
			}

			if (aw_route[i].source) {
				ret = aw882xx_append_channel_suffix("%s_%d",
					&aw_route[i].source, aw882xx);
				if (ret < 0) {
					aw_dev_err(aw882xx->dev, "append source channel suffix failed!");
					return;
				}
			}
		} else {
			if (aw_route[i].sink) {
				ret = aw882xx_append_i2c_suffix("%s_%d_%x",
					&aw_route[i].sink, aw882xx);
				if (ret < 0) {
					aw_dev_err(aw882xx->dev, "append sink i2c suffix failed!");
					return;
				}
			}

			if (aw_route[i].source) {
				ret = aw882xx_append_i2c_suffix("%s_%d_%x",
					&aw_route[i].source, aw882xx);
				if (ret < 0) {
					aw_dev_err(aw882xx->dev, "append source i2c suffix failed!");
					return;
				}
			}
		}

	}
	snd_soc_dapm_add_routes(dapm, aw_route, ARRAY_SIZE(aw882xx_audio_map));
#endif
}

static void aw882xx_load_fw(struct aw882xx *aw882xx)
{
	if (aw882xx->sync_load) {
		aw882xx_request_firmware(&aw882xx->fw_work.work);
	} else {
		queue_delayed_work(aw882xx->work_queue,
				&aw882xx->fw_work,
				msecs_to_jiffies(AW882XX_LOAD_FW_DELAY_TIME));
	}
}

static int aw882xx_codec_probe(aw_snd_soc_codec_t *aw_codec)
{
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(aw_codec);
	aw_dev_info(aw882xx->dev, "enter");

	aw882xx->work_queue = create_singlethread_workqueue("aw882xx");
	if (!aw882xx->work_queue) {
		aw_dev_err(aw882xx->dev, "create workqueue failed !");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&aw882xx->start_work, aw882xx_startup_work);
	INIT_DELAYED_WORK(&aw882xx->interrupt_work, aw882xx_interrupt_work);
	INIT_DELAYED_WORK(&aw882xx->dc_work, aw882xx_dc_prot_work);
	INIT_DELAYED_WORK(&aw882xx->fw_work, aw882xx_request_firmware);

	aw882xx->codec = aw_codec;

	if (aw882xx->aw_pa->channel == 0)
		aw882xx_add_codec_controls(aw882xx);

#ifdef OPLUS_ARCH_EXTENDS
	aw882xx_add_codec_mute_controls(aw882xx);
#endif /* OPLUS_ARCH_EXTENDS */
	aw882xx_add_widgets(aw882xx);

	/*load fw bin*/
	aw882xx_load_fw(aw882xx);

#ifdef AW_CALI_STORE_EXAMPLE
	/*load cali re*/
	aw882xx_dev_init_cali_re(aw882xx->aw_pa);
#endif

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
/* 2024/07/08, Add for smartpa vbatlow err check. */
	aw882xx->vbatlow_cnt = 0;
	if (aw882xx->aw_pa->channel < (sizeof(aw882xx_vbatlow_controls) / sizeof(aw882xx_vbatlow_controls[0]))) {
		aw_componet_codec_ops.add_codec_controls(aw882xx->codec,
				&aw882xx_vbatlow_controls[aw882xx->aw_pa->channel], 1);
	}

	aw882xx->last_fb = 0;
	aw882xx->check_step = 0;
	INIT_DELAYED_WORK(&aw882xx->check_work, aw882xx_check_work);
	aw_dev_err(aw882xx->dev, "event_id=%u, version:%s\n", OPLUS_AUDIO_EVENTID_SMARTPA_ERR, SMARTPA_ERR_FB_VERSION);
	aw_dev_err(aw882xx->dev, "event_id=%u, version:%s\n", OPLUS_AUDIO_EVENTID_SPK_ERR, SPK_ERR_FB_VERSION);
#endif /* CONFIG_OPLUS_FEATURE_MM_FEEDBACK */

	#ifdef OPLUS_FEATURE_SPEAKER_MUTE
	snd_soc_add_component_controls(aw882xx->codec,
		aw882xx_snd_control_spk_mute, ARRAY_SIZE(aw882xx_snd_control_spk_mute));
	#endif

	return 0;
}

#ifdef AW_KERNEL_VER_OVER_4_19_1
static void aw882xx_codec_remove(aw_snd_soc_codec_t *aw_codec)
{
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(aw_codec);
	aw_dev_info(aw882xx->dev, "enter");
	aw882xx_dev_deinit(aw882xx->aw_pa);
	destroy_workqueue(aw882xx->work_queue);
	aw882xx->work_queue = NULL;
}
#else
static int aw882xx_codec_remove(aw_snd_soc_codec_t *aw_codec)
{
	struct aw882xx *aw882xx =
		aw_componet_codec_ops.codec_get_drvdata(aw_codec);

	aw_dev_info(aw882xx->dev, "enter");
	aw882xx_dev_deinit(aw882xx->aw_pa);
	destroy_workqueue(aw882xx->work_queue);
	aw882xx->work_queue = NULL;
	return 0;
}
#endif

static int aw882xx_dai_drv_append_suffix(struct aw882xx *aw882xx,
				struct snd_soc_dai_driver *dai_drv,
				int num_dai)
{
	int ret;
	int i;

	if ((dai_drv != NULL) && (num_dai > 0))
		for (i = 0; i < num_dai; i++) {
			if (aw882xx->rename_flag ==  AW_RENAME_ENABLE) {
				ret = aw882xx_append_channel_suffix("%s-%d",
						&dai_drv->name, aw882xx);
				if (ret < 0)
					return ret;
				ret = aw882xx_append_channel_suffix("%s_%d",
						&dai_drv->playback.stream_name, aw882xx);
				if (ret < 0)
					return ret;
				ret = aw882xx_append_channel_suffix("%s_%d",
						&dai_drv->capture.stream_name, aw882xx);
				if (ret < 0)
					return ret;
				dev_set_name(aw882xx->dev, "%s_%d",
					"aw882xx_smartpa", aw882xx->aw_pa->channel);
				aw_dev_info(aw882xx->dev, "change dev_name:%s",
					dev_name(aw882xx->dev));
			} else {
				ret = aw882xx_append_i2c_suffix("%s-%d-%x",
						&dai_drv->name, aw882xx);
				if (ret < 0)
					return ret;
				ret = aw882xx_append_i2c_suffix("%s_%d_%x",
						&dai_drv->playback.stream_name, aw882xx);
				if (ret < 0)
					return ret;
				ret = aw882xx_append_i2c_suffix("%s_%d_%x",
						&dai_drv->capture.stream_name, aw882xx);
				if (ret < 0)
					return ret;
			}

			aw_dev_info(aw882xx->dev, "dai name [%s]", dai_drv[i].name);
			aw_dev_info(aw882xx->dev, "pstream_name name [%s]", dai_drv[i].playback.stream_name);
			aw_dev_info(aw882xx->dev, "cstream_name name [%s]", dai_drv[i].capture.stream_name);
		}

	return 0;
}

static struct snd_soc_dai_driver aw882xx_dai[] = {
	{
		.name = "aw882xx-aif",
		.id = 1,
		.playback = {
			.stream_name = "Speaker_Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_96000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE),
		},
		.capture = {
			.stream_name = "Speaker_Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_96000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S32_LE),
		 },
		.ops = &aw882xx_dai_ops,
		/* .symmetric_rates = 1,*/
	},
};


#ifdef AW_KERNEL_VER_OVER_4_19_1
static const struct snd_soc_component_driver soc_codec_dev_aw882xx = {
	.probe = aw882xx_codec_probe,
	.remove = aw882xx_codec_remove,
};
#else
static const struct snd_soc_codec_driver soc_codec_dev_aw882xx = {
	.probe = aw882xx_codec_probe,
	.remove = aw882xx_codec_remove,
};
#endif

static int aw_componet_codec_register(struct aw882xx *aw882xx)
{
	int ret = 0;
	struct snd_soc_dai_driver *dai_drv = NULL;

	dai_drv = devm_kzalloc(aw882xx->dev, sizeof(aw882xx_dai), GFP_KERNEL);
	if (dai_drv == NULL)
		return -ENOMEM;


	memcpy(dai_drv, aw882xx_dai, sizeof(aw882xx_dai));

	aw882xx_dai_drv_append_suffix(aw882xx, dai_drv, ARRAY_SIZE(aw882xx_dai));

	ret = aw882xx->codec_ops->register_codec(aw882xx->dev,
			&soc_codec_dev_aw882xx,
			dai_drv, ARRAY_SIZE(aw882xx_dai));
	if (ret < 0) {
		aw_dev_err(aw882xx->dev, "failed to register aw882xx: %d", ret);
		return -EINVAL;
	}

	if (aw882xx->rename_flag == AW_RENAME_ENABLE) {
		dev_set_name(aw882xx->dev, "%d-00%x",
			aw882xx->i2c->adapter->nr, aw882xx->i2c->addr);
		aw_dev_info(aw882xx->dev, "reset dev_name:%s",
			dev_name(aw882xx->dev));
	}

	return 0;
}
/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw882xx_parse_gpio_dt(struct aw882xx *aw882xx,
	struct device_node *np)
{
	if (!np) {
		aw882xx->reset_gpio = -1;
		aw882xx->irq_gpio = -1;
		return -EINVAL;
	}

	aw882xx->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw882xx->reset_gpio < 0)
		aw_dev_info(aw882xx->dev, "no reset gpio provided, will not HW reset device");
	else
		aw_dev_info(aw882xx->dev, "reset gpio provided ok");

	aw882xx->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw882xx->irq_gpio < 0)
		aw_dev_info(aw882xx->dev, "no irq gpio provided.");
	else
		aw_dev_info(aw882xx->dev, "irq gpio provided ok.");


	return 0;
}

static struct aw882xx *aw882xx_malloc_init(struct i2c_client *i2c)
{
	struct aw882xx *aw882xx = devm_kzalloc(&i2c->dev, sizeof(struct aw882xx), GFP_KERNEL);

	if (aw882xx == NULL) {
		dev_err(&i2c->dev, "devm_kzalloc failed.");
		return NULL;
	}
	aw882xx->aw_pa = devm_kzalloc(&i2c->dev, sizeof(struct aw_device), GFP_KERNEL);
	if (aw882xx->aw_pa == NULL) {
		dev_err(&i2c->dev, "devm_kzalloc failed.");
		return NULL;
	}

	aw882xx->dev = &i2c->dev;
	aw882xx->i2c = i2c;
	aw882xx->codec = NULL;
	aw882xx->codec_ops = &aw_componet_codec_ops;
	aw882xx->fw_status = AW_DEV_FW_FAILED;
	aw882xx->fw_retry_cnt = 0;
	aw882xx->dbg_en_prof = true;
	aw882xx->allow_pw = true;
	aw882xx->work_queue = NULL;
#ifdef OPLUS_ARCH_EXTENDS
	aw882xx->speaker_force_mute = false;
#endif

	mutex_init(&aw882xx->lock);

	return aw882xx;
}

static int aw882xx_gpio_request(struct aw882xx *aw882xx)
{
	int ret = 0;

	if (gpio_is_valid(aw882xx->reset_gpio)) {
		ret = devm_gpio_request_one(aw882xx->dev, aw882xx->reset_gpio,
			GPIOF_OUT_INIT_LOW, "aw882xx_rst");
		if (ret) {
			aw_dev_err(aw882xx->dev, "rst request failed");
			return ret;
		}
	}

	if (gpio_is_valid(aw882xx->irq_gpio)) {
		ret = devm_gpio_request_one(aw882xx->dev, aw882xx->irq_gpio,
			GPIOF_DIR_IN, "aw882xx_int");
		if (ret) {
			aw_dev_err(aw882xx->dev, "int request failed");
			return ret;
		}
	}

	return 0;
}

static void aw882xx_parse_rename_flag_dt(struct aw882xx *aw882xx)
{
	int ret;
	uint32_t rename_enable = 0;
	struct device_node *np = aw882xx->dev->of_node;

	ret = of_property_read_u32(np, "rename-flag", &rename_enable);
	if (ret)
		aw_dev_info(aw882xx->dev, "read rename flag failed, default rename off!");
	else
		aw_dev_info(aw882xx->dev, "rename-flag = %d", rename_enable);


	aw882xx->rename_flag = rename_enable;

}

static void aw882xx_parse_sync_load_dt(struct aw882xx *aw882xx)
{
	int ret = -1;
	int32_t sync_load = 0;
	struct device_node *np = aw882xx->dev->of_node;

	ret = of_property_read_u32(np, "sync-load", &sync_load);
	if (ret < 0) {
		aw_dev_info(aw882xx->dev,
			"read sync load failed,default async loading fw");
		sync_load = false;
	} else {
		aw_dev_info(aw882xx->dev,
			"sync load is %d", sync_load);
	}

	aw882xx->sync_load = sync_load;
}

#ifdef OPLUS_ARCH_EXTENDS
static void oplus_aw882xx_parse_feature_dt(struct aw882xx *aw882xx)
{
	int ret = -1;
	struct device_node *np = aw882xx->dev->of_node;
	const char *spin_mode_str = NULL;

	ret = of_property_read_string(np, "spin-mode", &spin_mode_str);
	if (ret) {
		aw882xx->spin_flag = false;
	} else {
		if (!strcmp(spin_mode_str, "dsp_spin")) {
			aw882xx->spin_flag = true;
		} else {
			aw882xx->spin_flag = false;
		}
	}

	if (of_property_read_bool(np, "need_f0_cali")) {
		aw882xx->need_f0_cali = true;
	} else {
		aw882xx->need_f0_cali = false;
	}

	if (of_property_read_bool(np, "que_dela_work")) {
		aw882xx->que_dela_work = true;
	} else {
		aw882xx->que_dela_work = false;
	}

	aw_dev_info(aw882xx->dev, "parse dt spin_flag: %d, need_f0_cali:%d, que_dela_work:%d ", aw882xx->spin_flag, aw882xx->need_f0_cali, aw882xx->que_dela_work);

	return;
}
#endif

static int aw882xx_parse_dt(struct device *dev, struct aw882xx *aw882xx,
		struct device_node *np)
{
	int ret;
	int32_t dc_enable = 0;
	int32_t sync_enable = 0;

	/*gpio dts parser*/
	ret = aw882xx_parse_gpio_dt(aw882xx, np);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "dc-flag", &dc_enable);
	if (ret) {
		dc_enable = false;
		aw_dev_info(aw882xx->dev, "close dc protect!");
	} else {
		aw_dev_info(aw882xx->dev, "dc-flag = %d", dc_enable);
	}

	aw882xx->dc_flag = dc_enable;


	ret = of_property_read_u32(np, "sync-flag", &sync_enable);
	if (ret < 0) {
		aw_dev_info(aw882xx->dev,
			"read sync flag failed,default phase sync off");
		sync_enable = false;
	} else {
		aw_dev_info(aw882xx->dev,
			"sync flag is %d", sync_enable);
	}

	aw882xx->phase_sync = sync_enable;

	aw882xx_parse_rename_flag_dt(aw882xx);
	aw882xx_parse_sync_load_dt(aw882xx);
#ifdef OPLUS_ARCH_EXTENDS
	oplus_aw882xx_parse_feature_dt(aw882xx);
#endif

	return 0;
}

int aw882xx_hw_reset(struct aw882xx *aw882xx)
{
	aw_dev_info(aw882xx->dev, "enter");

	if (gpio_is_valid(aw882xx->reset_gpio)) {
		gpio_set_value_cansleep(aw882xx->reset_gpio, 0);
		mdelay(1);
		gpio_set_value_cansleep(aw882xx->reset_gpio, 1);
		mdelay(2);
	} else {
		aw_dev_info(aw882xx->dev, "has no reset gpio");
	}
	return 0;
}

static int aw882xx_read_chipid(struct aw882xx *aw882xx)
{
	int ret = -1;
	unsigned int cnt = 0;
	unsigned int reg_value = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw882xx_i2c_read(aw882xx, AW882XX_CHIP_ID_REG, &reg_value);
		if (ret < 0) {
			aw_dev_err(aw882xx->dev, "failed to read REG_ID: %d", ret);
			return -EIO;
		}
		switch (reg_value) {
		case PID_1852_ID: {
			aw_dev_info(aw882xx->dev, "aw882xx 1852 detected");
			aw882xx->aw_pa->chip_id = reg_value;
			return 0;
		}
		case PID_2013_ID: {
			aw_dev_info(aw882xx->dev, "aw882xx 2013 detected");
			aw882xx->aw_pa->chip_id = reg_value;
			return 0;
		}
		case PID_2032_ID: {
			aw_dev_info(aw882xx->dev, "aw882xx 2032 detected");
			aw882xx->aw_pa->chip_id = reg_value;
			return 0;
		}
		case PID_2055_ID: {
			aw_dev_info(aw882xx->dev, "aw882xx 2055 detected");
			aw882xx->aw_pa->chip_id = reg_value;
			return 0;
		}
		case PID_2071_ID: {
			aw_dev_info(aw882xx->dev, "aw882xx 2071 detected");
			aw882xx->aw_pa->chip_id = reg_value;
			return 0;
		}
		case PID_2113_ID: {
			aw_dev_info(aw882xx->dev, "aw882xx 2113 detected");
			aw882xx->aw_pa->chip_id = reg_value;
			return 0;
		}
		default:
			aw_dev_info(aw882xx->dev, "unsupported device revision (0x%x)",
							reg_value);
			break;
		}
		cnt++;

		msleep(AW_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}

static irqreturn_t aw882xx_irq(int irq, void *data)
{
	struct aw882xx *aw882xx = (struct aw882xx *)data;

	if (!aw882xx) {
		aw_pr_err("pointer is NULL");
		return -EINVAL;
	}
	aw_dev_info(aw882xx->dev, "enter");

	/* mask all irq */
	aw882xx_dev_set_intmask(aw882xx->aw_pa, false);

	/* upload workqueue */
	if (aw882xx->work_queue)
		queue_delayed_work(aw882xx->work_queue, &aw882xx->interrupt_work, 0);

	return IRQ_HANDLED;
}

static int aw882xx_interrupt_init(struct aw882xx *aw882xx)
{
	int ret;
	int irq_flags;

	if (gpio_is_valid(aw882xx->irq_gpio)) {
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(aw882xx->dev,
					gpio_to_irq(aw882xx->irq_gpio),
					NULL, aw882xx_irq, irq_flags,
					"aw882xx", aw882xx);
		if (ret != 0) {
			aw_dev_err(aw882xx->dev, "Failed to request IRQ %d: %d",
					gpio_to_irq(aw882xx->irq_gpio), ret);
			return ret;
		}
	} else {
		aw_dev_info(aw882xx->dev, "gpio invalid");
		/* disable interrupt */
	}

	return 0;
}

static ssize_t reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	unsigned int databuf[2] = {0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		aw882xx_i2c_write(aw882xx, databuf[0], databuf[1]);

	return count;
}

static ssize_t reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned int reg_val = 0;
	int reg_num = aw882xx->aw_pa->ops.aw_get_reg_num();

	for (i = 0; i < reg_num; i++) {
		if (aw882xx->aw_pa->ops.aw_check_rd_access(i)) {
			aw882xx_i2c_read(aw882xx, i, &reg_val);
			len += snprintf(buf+len, PAGE_SIZE-len,
				"reg:0x%02x=0x%04x\n", i, reg_val);
		}
	}

	return len;
}

static ssize_t rw_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	unsigned int databuf[2] = {0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw882xx->rw_reg_addr = (unsigned char)databuf[0];
		if (aw882xx->aw_pa->ops.aw_check_rd_access(databuf[0]))
			aw882xx_i2c_write(aw882xx, databuf[0], databuf[1]);
	} else if (kstrtouint(buf, 16, &databuf[0]) == 0) {
		aw882xx->rw_reg_addr = (unsigned char)databuf[0];
	}

	return count;
}

static ssize_t rw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned int reg_val = 0;

	if (aw882xx->aw_pa->ops.aw_check_rd_access(aw882xx->rw_reg_addr)) {
		aw882xx_i2c_read(aw882xx, aw882xx->rw_reg_addr, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
			"reg:0x%02x=0x%04x\n", aw882xx->rw_reg_addr, reg_val);
	}
	return len;
}

static int aw882xx_awrw_write(struct aw882xx *aw882xx, const char *buf, size_t count)
{
	int  i, ret;
	char *data_buf = NULL;
	int str_len, data_len, temp_data;
	char *reg_data = NULL;
	struct aw882xx_i2c_packet *packet = &aw882xx->i2c_packet;

	aw_dev_info(aw882xx->dev, "write:reg_addr[0x%02x], reg_num[%d]",
			packet->reg_addr, packet->reg_num);

	data_len = AWRW_DATA_BYTES * packet->reg_num;

	str_len = count - AWRW_HDR_LEN - 1;

	if ((data_len * 5 - 1) > str_len) {
		aw_dev_err(aw882xx->dev, "data_str_len [%d], requeset len [%d]",
					str_len, (data_len * 5 - 1));
		return -EINVAL;
	}

	data_buf = kmalloc(data_len + 1, GFP_KERNEL);
	if (data_buf == NULL) {
		aw_dev_err(aw882xx->dev, "alloc memory failed");
		return -ENOMEM;
	}

	data_buf[0] = packet->reg_addr;
	reg_data = data_buf + 1;

	aw_dev_dbg(aw882xx->dev, "reg_addr: 0x%02x", data_buf[0]);

	for (i = 0; i < data_len; i++) {
		if (sscanf(buf + AWRW_HDR_LEN + 1 + i * 5, "0x%02x", &temp_data) == 1) {
			reg_data[i] = temp_data;
			aw_dev_dbg(aw882xx->dev, "[%d] : 0x%02x", i, reg_data[i]);
		} else {
			return -EINVAL;
		}
	}

	ret = i2c_master_send(aw882xx->i2c, data_buf, data_len + 1);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev, "write failed");
		kfree(data_buf);
		data_buf = NULL;
		return -EFAULT;
	}

	kfree(data_buf);
	data_buf = NULL;

	aw_dev_info(aw882xx->dev, "write success");
	return 0;
}

static int aw882xx_awrw_data_check(struct aw882xx *aw882xx, int *data)
{
	int reg_num_max = aw882xx->aw_pa->ops.aw_get_reg_num();

	if ((data[AWRW_HDR_ADDR_BYTES] != AWRW_ADDR_BYTES) ||
		(data[AWRW_HDR_DATA_BYTES] != AWRW_DATA_BYTES)) {
		aw_dev_err(aw882xx->dev, "addr_bytes [%d] or data_bytes [%d] unsupport",
				data[AWRW_HDR_ADDR_BYTES], data[AWRW_HDR_DATA_BYTES]);
		return -EINVAL;
	}

	if (data[AWRW_HDR_REG_ADDR] >= reg_num_max) {
		aw_dev_err(aw882xx->dev, "reg_addr[%d] > reg_max[%d]",
				data[AWRW_HDR_REG_ADDR], reg_num_max);
		return -EINVAL;
	}

	return 0;
}

/* flag addr_bytes data_bytes reg_num reg_addr*/
static int aw882xx_awrw_parse_buf(struct aw882xx *aw882xx,
					const char *buf, size_t count)
{
	int ret;
	int data[AWRW_HDR_MAX] = {0};
	struct aw882xx_i2c_packet *packet = &aw882xx->i2c_packet;

	if (sscanf(buf, "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
				&data[AWRW_HDR_WR_FLAG],
				&data[AWRW_HDR_ADDR_BYTES],
				&data[AWRW_HDR_DATA_BYTES],
				&data[AWRW_HDR_REG_NUM],
				&data[AWRW_HDR_REG_ADDR]) == 5) {
		ret = aw882xx_awrw_data_check(aw882xx, data);
		if (ret < 0)
			return ret;

		packet->reg_addr = data[AWRW_HDR_REG_ADDR];
		packet->reg_num = data[AWRW_HDR_REG_NUM];

		switch (data[AWRW_HDR_WR_FLAG]) {
		case AWRW_FLAG_WRITE:
			return aw882xx_awrw_write(aw882xx, buf, count);
		case AWRW_FLAG_READ:
			packet->status = AWRW_I2C_ST_READ;
			aw_dev_info(aw882xx->dev, "read_cmd:reg_addr[0x%02x], reg_num[%d]",
					packet->reg_addr, packet->reg_num);
			return 0;
		default:
			aw_dev_err(aw882xx->dev, "please check str format, unsupport flag %d",
				data[AWRW_HDR_WR_FLAG]);
			return -EINVAL;
		}
	}

	aw_dev_err(aw882xx->dev, "can not parse string");

	return -EINVAL;
}

static ssize_t awrw_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	if (count < AWRW_HDR_LEN) {
		aw_dev_err(dev, "data count too smaller, please check write format");
		aw_dev_err(dev, "string %s", buf);
		return -EINVAL;
	}

	ret = aw882xx_awrw_parse_buf(aw882xx, buf, count);
	if (ret)
		return -EINVAL;

	return count;
}

static ssize_t awrw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret, i;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	struct aw882xx_i2c_packet *packet = &aw882xx->i2c_packet;
	int data_len, len = 0;
	char *reg_data = NULL;

	if (packet->status != AWRW_I2C_ST_READ) {
		aw_dev_err(aw882xx->dev, "please write read cmd first");
		return -EINVAL;
	}

	data_len = AWRW_DATA_BYTES * packet->reg_num;
	reg_data = kmalloc(data_len, GFP_KERNEL);
	if (reg_data == NULL) {
		aw_dev_err(aw882xx->dev, "memory alloc failed");
		ret = -EINVAL;
		goto exit;
	}

	ret = aw882xx_i2c_reads(aw882xx, packet->reg_addr, (char *)reg_data, data_len);
	if (ret < 0) {
		ret = -EFAULT;
		goto exit;
	}

	aw_dev_info(aw882xx->dev, "reg_addr 0x%02x, reg_num %d",
			packet->reg_addr, packet->reg_num);

	for (i = 0; i < data_len; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"0x%02x,", reg_data[i]);
		aw_dev_dbg(aw882xx->dev, "0x%02x", reg_data[i]);
	}

	ret = len;

exit:
	if (reg_data) {
		kfree(reg_data);
		reg_data = NULL;
	}
	packet->status = AWRW_I2C_ST_NONE;
	return ret;
}


static ssize_t drv_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"driver_ver: %s\n", AW882XX_DRIVER_VERSION);

	return len;
}

static ssize_t dsp_re_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	ssize_t len = 0;
	int cali_re;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	ret = aw882xx_dev_get_cali_re(aw882xx->aw_pa, &cali_re);
	if (ret) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"read dsp_re failed!\n");
		return len;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
		"%d\n", cali_re);

	return len;
}

static ssize_t dsp_f0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	ssize_t len = 0;
	int cali_f0;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	ret = aw882xx_dev_get_cali_f0(aw882xx->aw_pa, &cali_f0);
	if (ret) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"read dsp_re failed!\n");
		return len;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
		"%d\n", cali_f0);

	return len;
}

static ssize_t t25_f0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	ssize_t len = 0;
	int t25_f0;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	ret = aw882xx_dev_get_t25_f0(aw882xx->aw_pa, &t25_f0);
	if (ret) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"read t25_f0 failed!\n");
		return len;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
		"%d\n", t25_f0);

	return len;
}


static ssize_t air_pressure_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	unsigned int air_pressure = 0;

	ret = kstrtouint(buf, 32, &air_pressure);
	if (ret < 0)
		return ret;

	aw_dev_info(aw882xx->dev, "set phase sync air_pressure : [%d]", air_pressure);
	aw882xx_dev_set_air_pressure(aw882xx->aw_pa, air_pressure);

	return count;
}

static ssize_t air_pressure_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	ssize_t len = 0;
	int air_pressure;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	ret = aw882xx_dev_get_air_pressure(aw882xx->aw_pa, &air_pressure);
	if (ret) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"read air_pressure failed!\n");
		return len;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
		"%d\n", air_pressure);

	return len;
}

static ssize_t real_time_f0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	ssize_t len = 0;
	int real_time_f0;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	ret = aw882xx_dev_get_real_time_f0(aw882xx->aw_pa, &real_time_f0);
	if (ret) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"read real_time failed!\n");
		return len;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
		"%d\n", real_time_f0);

	return len;
}

static ssize_t fade_step_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	unsigned int databuf[2] = {0};

	/*step 0 - 12*/
	if (kstrtouint(buf, 10, &databuf[0]) == 0) {
		if (databuf[0] > (aw882xx->aw_pa->volume_desc.mute_volume)) {
			aw_dev_info(aw882xx->dev, "step overflow %d Db", databuf[0]);
			return count;
		}
		aw882xx_dev_set_fade_vol_step(aw882xx->aw_pa, databuf[0]);
	}
	aw_dev_info(aw882xx->dev, "set step %d Done", databuf[0]);

	return count;
}

static ssize_t fade_step_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len,
		"step: %d\n", aw882xx_dev_get_fade_vol_step(aw882xx->aw_pa));

	return len;
}

static ssize_t dbg_prof_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	unsigned int databuf[2] = {0};

	if (kstrtouint(buf, 10, &databuf[0]) == 0) {
		if (databuf[0])
			aw882xx->dbg_en_prof = true;
		else
			aw882xx->dbg_en_prof = false;
	}
	aw_dev_info(aw882xx->dev, "en_prof %d Done", databuf[0]);

	return count;
}

static ssize_t dbg_prof_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
		" %d\n", aw882xx->dbg_en_prof);

	return len;
}

static ssize_t phase_sync_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	unsigned int flag = 0;

	ret = kstrtouint(buf, 10, &flag);
	if (ret < 0)
		return ret;

	flag = ((flag == false) ? false : true);

	aw_dev_info(aw882xx->dev, "set phase sync flag : [%d]", flag);

	aw882xx->phase_sync = flag;

	return count;
}

static ssize_t phase_sync_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw882xx *aw882xx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
				"sync flag : %d\n", aw882xx->phase_sync);

	return len;
}

static ssize_t print_dbg_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 0, &g_print_dbg);
	if (ret < 0)
		return ret;

	g_print_dbg = ((g_print_dbg == false) ? false : true);

	aw_dev_info(aw882xx->dev, "set g_print_dbg : [%d]", g_print_dbg);

	return count;
}

static ssize_t print_dbg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
				"g_print_dbg : %d\n", g_print_dbg);

	return len;
}

static ssize_t algo_ver_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	ssize_t len = 0;
	char algo_ver_buf[ALGO_VERSION_MAX] = { 0 };
	struct aw882xx *aw882xx = dev_get_drvdata(dev);

	ret = aw882xx_get_algo_version(aw882xx->aw_pa, algo_ver_buf);
	if (ret < 0) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"read algo version failed!\n");
		return len;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "%s\n", algo_ver_buf);

	return len;
}

static DEVICE_ATTR_RW(reg);
static DEVICE_ATTR_RW(rw);
static DEVICE_ATTR_RW(awrw);
static DEVICE_ATTR_RO(drv_ver);
static DEVICE_ATTR_RO(dsp_re);
static DEVICE_ATTR_RO(dsp_f0);
static DEVICE_ATTR_RW(air_pressure);
static DEVICE_ATTR_RO(t25_f0);
static DEVICE_ATTR_RO(real_time_f0);
static DEVICE_ATTR_RW(fade_step);
static DEVICE_ATTR_RW(dbg_prof);
static DEVICE_ATTR_RW(phase_sync);
static DEVICE_ATTR_RW(print_dbg);
static DEVICE_ATTR_RO(algo_ver);


static struct attribute *aw882xx_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_rw.attr,
	&dev_attr_awrw.attr,
	&dev_attr_drv_ver.attr,
	&dev_attr_fade_step.attr,
	&dev_attr_dbg_prof.attr,
	&dev_attr_dsp_re.attr,
	&dev_attr_dsp_f0.attr,
	&dev_attr_air_pressure.attr,
	&dev_attr_t25_f0.attr,
	&dev_attr_real_time_f0.attr,
	&dev_attr_phase_sync.attr,
	&dev_attr_print_dbg.attr,
	&dev_attr_algo_ver.attr,
	NULL
};

static struct attribute_group aw882xx_attribute_group = {
	.attrs = aw882xx_attributes,
};
#ifdef OPLUS_ARCH_EXTENDS
static ssize_t aw882xx_dbgfs_range_read(struct file *file,
				char __user *user_buf, size_t count,
				loff_t *ppos)
{
	struct i2c_client *i2c = PDE_DATA(file_inode(file));
	struct aw882xx *aw882xx = i2c_get_clientdata(i2c);
	char *str = NULL;
	int ret = 0;
	if (!aw882xx) {
		pr_err("%s aw882xx is null\n", __func__);
		return -EINVAL;
	}
	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		pr_err("[0x%x] memory allocation failed\n", aw882xx->i2c->addr);
		goto range_err;
	}
	ret = snprintf(str, PAGE_SIZE, " Min:%u mOhms, Max:%u mOhms\n",
		aw882xx->aw_pa->re_min, aw882xx->aw_pa->re_max);
	pr_info("%s addr 0x%x, str=%s\n", __func__, aw882xx->i2c->addr, str);
	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
	kfree(str);
range_err:
	return ret;
}

static ssize_t aw882xx_dbgfs_check_re_show(struct file *file,
				char __user *user_buf, size_t count,
				loff_t *ppos)
{
	struct i2c_client *i2c = PDE_DATA(file_inode(file));
	struct aw882xx *aw882xx = i2c_get_clientdata(i2c);
	char *str = NULL;
	int ret = 0;
	if (!aw882xx) {
		pr_err("%s aw882xx is null\n", __func__);
		return -EINVAL;
	}
	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		pr_err("[0x%x] memory allocation failed\n", aw882xx->i2c->addr);
		goto re_show_err;
	}
	ret = oplus_aw_calib_re_show(&i2c->dev, str);
	pr_info("%s addr 0x%x, str=%s\n", __func__, aw882xx->i2c->addr, str);
	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
	kfree(str);
re_show_err:
	return ret;
}

static ssize_t aw882xx_dbgfs_check_cali_re(struct file *file,
				char __user *user_buf, size_t count,
				loff_t *ppos)
{
	struct i2c_client *i2c = PDE_DATA(file_inode(file));
	struct aw882xx *aw882xx = i2c_get_clientdata(i2c);
	char *str = NULL;
	int ret = 0;
	if (!aw882xx) {
		pr_err("%s aw882xx is null\n", __func__);
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/*2024/05/15, Add for speaker f0 err feedback.*/
	g_chk_err = false;
	aw882xx->check_step = 0;
	cancel_delayed_work_sync(&aw882xx->check_work);
#endif

	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		pr_err("[0x%x] memory allocation failed\n", aw882xx->i2c->addr);
		goto cali_re_err;
	}

	ret = oplus_aw_cali_re_f0(&i2c->dev, str);
	pr_info("%s addr 0x%x, str=%s\n", __func__, aw882xx->i2c->addr, str);
	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
	kfree(str);
cali_re_err:
	return ret;

}


static ssize_t aw882xx_dbgfs_f0_range_read(struct file *file,
				char __user *user_buf, size_t count,
				loff_t *ppos)
{
	struct i2c_client *i2c = PDE_DATA(file_inode(file));
	struct aw882xx *aw882xx = i2c_get_clientdata(i2c);
	char *str = NULL;
	int ret = 0;
	if (!aw882xx) {
		pr_err("%s aw882xx is null\n", __func__);
		return -EINVAL;
	}
	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		pr_err("[0x%x] memory allocation failed\n", aw882xx->i2c->addr);
		goto range_err;
	}
	ret = snprintf(str, PAGE_SIZE, "Min:%u hz, Max:%u hz\n",
		aw882xx->aw_pa->f0_min, aw882xx->aw_pa->f0_max);

	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
	pr_info("%s addr 0x%x, str=%s\n, ret = %d \n", __func__, aw882xx->i2c->addr, str, ret);

	kfree(str);
range_err:
	return ret;
}

static ssize_t aw882xx_dbgfs_cali_f0_show(struct file *file,
				char __user *user_buf, size_t count,
				loff_t *ppos)
{
	int ret = 0;
	int i = 0;
	ssize_t len = 0;
	int32_t cali_f0[AW_DEV_CH_MAX] = {0};
	char *str = NULL;

	struct i2c_client *i2c = PDE_DATA(file_inode(file));
	struct aw882xx *aw882xx = i2c_get_clientdata(i2c);
	struct aw_device *aw_dev = aw882xx->aw_pa;

	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		pr_err("[0x%x] memory allocation failed\n", aw882xx->i2c->addr);
		goto range_err;
	}

	ret = aw_cali_svc_get_devs_cali_f0(aw_dev, cali_f0, AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(aw_dev->dev, "get f0 failed ");
		len += snprintf(str+len, PAGE_SIZE-len, "get f0 failed\n");
	} else {
		for (i = 0; i < ret; i++) {
			len += snprintf(str+len, PAGE_SIZE-len, "%s:%d hz", ch_name[i], cali_f0[i]);
		}
		len += snprintf(str+len, PAGE_SIZE-len, "\n");
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, str, len);
	pr_info("%s addr 0x%x, str=%s ret = %d len = %ld \n", __func__, aw882xx->i2c->addr, str, ret, len);
	kfree(str);
range_err:
	return ret;
}

static const struct proc_ops aw882xx_dbgfs_range_fops = {
	.proc_open = simple_open,
	.proc_read = aw882xx_dbgfs_range_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops aw882xx_dbgfs_re_show_fops = {
	.proc_open = simple_open,
	.proc_read = aw882xx_dbgfs_check_re_show,
	.proc_lseek = default_llseek,
};

static const struct proc_ops aw882xx_dbgfs_cali_re_fops = {
	.proc_open = simple_open,
	.proc_read = aw882xx_dbgfs_check_cali_re,
	.proc_lseek = default_llseek,
};

static const struct proc_ops aw882xx_dbgfs_cali_r_impedance_fops = {
	.proc_open = simple_open,
	.proc_read = aw882xx_dbgfs_check_re_show,
	.proc_lseek = default_llseek,
};

static const struct proc_ops aw882xx_dbgfs_f0_range_fops = {
	.proc_open = simple_open,
	.proc_read = aw882xx_dbgfs_f0_range_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops aw882xx_dbgfs_f0_show_fops = {
	.proc_open = simple_open,
	.proc_read = aw882xx_dbgfs_cali_f0_show,
	.proc_lseek = default_llseek,
};

static void aw882xx_debug_init(struct aw882xx *aw882xx, struct i2c_client *i2c)
{
	char name[50];
	scnprintf(name, MAX_CONTROL_NAME, "%s-%x", i2c->name, i2c->addr);
	aw882xx->dbg_dir = proc_mkdir(name, NULL);
	proc_create_data("range", S_IRUGO, aw882xx->dbg_dir,
					&aw882xx_dbgfs_range_fops, i2c);
	proc_create_data("re_show", S_IRUGO, aw882xx->dbg_dir,
					&aw882xx_dbgfs_re_show_fops, i2c);
	proc_create_data("cali_re", S_IRUGO, aw882xx->dbg_dir,
					&aw882xx_dbgfs_cali_re_fops, i2c);
	proc_create_data("r_impedance", S_IRUGO, aw882xx->dbg_dir,
					&aw882xx_dbgfs_cali_r_impedance_fops, i2c);
#ifdef AW_NEED_CALIB_F0
	if (aw882xx->need_f0_cali) {
		proc_create_data("f0_range", S_IRUGO, aw882xx->dbg_dir,
						&aw882xx_dbgfs_f0_range_fops, i2c);
		proc_create_data("f0_show", S_IRUGO, aw882xx->dbg_dir,
						&aw882xx_dbgfs_f0_show_fops, i2c);
	}
#endif
}

/* copy calibrated re to user space */
static ssize_t aw882xx_read_cali_re_to_bin(struct file *file,
				char __user *user_buf, size_t count,
				loff_t *ppos)
{
	int ret = -1;
	int i = 0;
	ssize_t len = 0;
	struct list_head *dev_list = NULL;
	struct aw_device *local_dev = NULL;
	int32_t cali_re[AW_DEV_CH_MAX + 1] = { 0 };
	char buf[8*(AW_DEV_CH_MAX + 1)] = { 0 };

	ret = aw882xx_dev_get_list_head(&dev_list);
	if (ret) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	if (local_dev == NULL) {
		aw_pr_err("get local_dev failed");
		return -EPERM;
	}

	ret = aw_cali_svc_get_devs_cali_re(local_dev, cali_re, AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "get re failed");
	} else {
		for (i = 0; i < ret; i++) {
			len += snprintf((char *)&buf+len, PAGE_SIZE-len, "%d,", cali_re[i]);
		}
		len += snprintf((char *)&buf+len-1, PAGE_SIZE-len, "\n"); /* -1 delete the last "," */
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, &buf, len);

	return ret;
}

static ssize_t aw882xx_read_cali_pa_cnt(struct file *file,
				char __user *user_buf, size_t count,
				loff_t *ppos)
{
	int ret = 0;
	ssize_t len = 0;
	struct list_head *dev_list = NULL;
	struct aw_device *local_dev = NULL;
	struct list_head *pos = NULL;
	int32_t cali_pa_cnt = 0;
	char buf[8 + 1] = { 0 };

	/*get dev list*/
	ret = aw882xx_dev_get_list_head(&dev_list);
	if (ret) {
		aw_pr_err("get dev list failed %d ", ret);
		cali_pa_cnt = 0;
	} else {
		list_for_each(pos, dev_list) {
			local_dev = container_of(pos, struct aw_device, list_node);
			if ((local_dev != NULL) && (local_dev->channel < AW_DEV_CH_MAX)) {
				cali_pa_cnt++;
			} else {
				aw_pr_err("channel overflow %d.", local_dev->channel);
				cali_pa_cnt = 0;
			}
		}
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "%d", cali_pa_cnt);
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");

	pr_info("%s cali_pa_cnt = %d \n", __func__, cali_pa_cnt);
	ret = simple_read_from_buffer(user_buf, count, ppos, &buf, len);

	return ret;
}

static ssize_t aw882xx_write_cali_re_to_driver(struct file * file,
				const char __user * buffer, size_t count, loff_t * f_pos)
{
	int ret = -1;
	ssize_t len = 0;
	struct list_head *dev_list = NULL;
	struct aw_device *local_dev = NULL;
	char re_buf[8*(AW_DEV_CH_MAX + 1)] = { 0 };

	len = min_t(size_t, count, PAGE_SIZE - 1);

	if (copy_from_user(&re_buf, buffer, len))
		return -EFAULT;

	re_buf[len] = '\0';

	ret = aw882xx_dev_get_list_head(&dev_list);
	if (ret) {
		pr_err("[Awinic] %s: get dev list failed \n", __func__);
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);

	if (local_dev == NULL) {
		aw_pr_err("get local_dev failed");
		return -EPERM;
	}

	ret = aw_cali_svc_set_devs_re_str(local_dev, re_buf);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "set re str %s failed", re_buf);
		return -EPERM;
	}

	return len;
}

#ifdef AW_NEED_CALIB_F0
static ssize_t aw882xx_read_cali_f0_to_bin(struct file *file,
				char __user *user_buf, size_t count,
				loff_t *ppos)
{
	int ret = -1;
	int i = 0;
	ssize_t len = 0;
	struct list_head *dev_list = NULL;
	struct aw_device *local_dev = NULL;
	int32_t cali_f0[AW_DEV_CH_MAX + 1] = { 0 };
	char buf[8*(AW_DEV_CH_MAX + 1)] = { 0 };

	ret = aw882xx_dev_get_list_head(&dev_list);
	if (ret) {
		aw_pr_err("get dev list failed");
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);
	if (local_dev == NULL) {
		aw_pr_err("get local_dev failed");
		return -EPERM;
	}

	ret = aw_cali_svc_get_devs_cali_f0(local_dev, cali_f0, AW_DEV_CH_MAX);
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "get re failed");
	} else {
		for (i = 0; i < ret; i++) {
			len += snprintf((char *)&buf+len, PAGE_SIZE-len, "%d,", cali_f0[i]);
		}
		len += snprintf((char *)&buf+len-1, PAGE_SIZE-len, "\n"); /* -1 delete the last "," */
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, &buf, len);

	return ret;
}

static ssize_t aw882xx_write_cali_f0_to_driver(struct file * file,
				const char __user * buffer, size_t count, loff_t * f_pos)
{
	int ret = -1;
	ssize_t len = 0;
	struct list_head *dev_list = NULL;
	struct aw_device *local_dev = NULL;
	char f0_buf[8*(AW_DEV_CH_MAX + 1)] = { 0 };

	len = min_t(size_t, count, PAGE_SIZE - 1);

	if (copy_from_user(&f0_buf, buffer, len))
		return -EFAULT;

	f0_buf[len] = '\0';

	ret = aw882xx_dev_get_list_head(&dev_list);
	if (ret) {
		pr_err("[Awinic] %s: get dev list failed \n", __func__);
		return ret;
	}

	local_dev = list_first_entry(dev_list, struct aw_device, list_node);
	if (local_dev == NULL) {
		aw_pr_err("get local_dev failed");
		return -EPERM;
	}

	ret = aw_cali_svc_set_devs_f0_str(local_dev, f0_buf); /* TBD in v1.10 */
	if (ret <= 0) {
		aw_dev_err(local_dev->dev, "set re str %s failed", f0_buf);
		return -EPERM;
	}

	return len;
}
#endif

static const struct proc_ops aw882xx_cali_pa_cnt_fops = {
	.proc_open = simple_open,
	.proc_read = aw882xx_read_cali_pa_cnt,
	.proc_lseek = default_llseek,
};

static const struct proc_ops aw882xx_cali_re_to_bin_fops = {
	.proc_open = simple_open,
	.proc_read = aw882xx_read_cali_re_to_bin,
	.proc_lseek = default_llseek,
};

static const struct proc_ops aw882xx_cali_re_to_driver_fops = {
	.proc_open = simple_open,
	.proc_write = aw882xx_write_cali_re_to_driver,
	.proc_lseek = default_llseek,
};

#ifdef AW_NEED_CALIB_F0
static const struct proc_ops aw882xx_cali_f0_to_bin_fops = {
	.proc_open = simple_open,
	.proc_read = aw882xx_read_cali_f0_to_bin,
	.proc_lseek = default_llseek,
};

static const struct proc_ops aw882xx_cali_f0_to_driver_fops = {
	.proc_open = simple_open,
	.proc_write = aw882xx_write_cali_f0_to_driver,
	.proc_lseek = default_llseek,
};
#endif

static void aw882xx_re_f0_save_proc_init(struct aw882xx *aw882xx, struct i2c_client *i2c)
{
	if (!g_rf_proc_inited) {
		g_rf_proc_inited = true;
		aw882xx->dbg_dir = proc_mkdir(i2c->name, NULL);
		proc_create_data("cali_pa_cnt", S_IRUGO | S_IWUGO, aw882xx->dbg_dir,
					&aw882xx_cali_pa_cnt_fops, i2c);
		proc_create_data("cali_re_to_bin", S_IRUGO | S_IWUGO, aw882xx->dbg_dir,
						&aw882xx_cali_re_to_bin_fops, i2c);
		proc_create_data("cali_re_to_driver", S_IRUGO | S_IWUGO, aw882xx->dbg_dir,
						&aw882xx_cali_re_to_driver_fops, i2c);
#ifdef AW_NEED_CALIB_F0
		if (aw882xx->need_f0_cali) {
			proc_create_data("cali_f0_to_bin", S_IRUGO | S_IWUGO, aw882xx->dbg_dir,
							&aw882xx_cali_f0_to_bin_fops, i2c);
			proc_create_data("cali_f0_to_driver", S_IRUGO | S_IWUGO, aw882xx->dbg_dir,
							&aw882xx_cali_f0_to_driver_fops, i2c);
		}
#endif
	}
}
#endif /*OPLUS_ARCH_EXTENDS*/

#ifdef AW_KERNEL_VER_OVER_6_6_0
static int aw882xx_i2c_probe(struct i2c_client *i2c)

#else
static int aw882xx_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
#endif
{
	int ret;
	struct aw882xx *aw882xx = NULL;
	struct device_node *np = i2c->dev.of_node;

	aw_pr_info("enter addr=0x%x", i2c->addr);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		aw_dev_err(&i2c->dev, "check_functionality failed");
		return -EIO;
	}

	/*dev free all auto free*/
	aw882xx = aw882xx_malloc_init(i2c);
	if (aw882xx == NULL) {
		aw_dev_err(&i2c->dev, "malloc aw882xx failed");
		return -ENOMEM;
	}

	i2c_set_clientdata(i2c, aw882xx);

	ret = aw882xx_parse_dt(&i2c->dev, aw882xx, np);
	if (ret) {
		aw_dev_err(&i2c->dev, "failed to parse device tree node");
		return ret;
	}

	/*get gpio resource*/
	ret = aw882xx_gpio_request(aw882xx);
	if (ret)
		return ret;

	/* hardware reset */
	aw882xx_hw_reset(aw882xx);

	/* aw882xx chip id */
	ret = aw882xx_read_chipid(aw882xx);
	if (ret < 0) {
		aw_dev_err(&i2c->dev, "aw882xx_read_chipid failed ret=%d", ret);
		return ret;
	}

	/*aw pa init*/
	ret = aw882xx_init(aw882xx);
	if (ret)
		return ret;

	/*aw882xx irq*/
	aw882xx_interrupt_init(aw882xx);

	/*codec register*/
	ret = aw_componet_codec_register(aw882xx);
	if (ret) {
		aw_dev_err(&i2c->dev, "codec register failde");
		return ret;
	}

	/*create attr*/
	ret = sysfs_create_group(&i2c->dev.kobj, &aw882xx_attribute_group);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev, "error creating sysfs attr files");
		goto err_sysfs;
	}

	/*set aw882xx to dev private*/
	dev_set_drvdata(&i2c->dev, aw882xx);

	/*i2c packet init*/
	aw882xx->i2c_packet.status = AWRW_I2C_ST_NONE;
	aw882xx->i2c_packet.reg_num = 0;
	aw882xx->i2c_packet.reg_addr = 0xff;
	aw882xx->i2c_packet.reg_data = NULL;

	/*add device to total list*/
	mutex_lock(&g_aw882xx_lock);
	g_aw882xx_dev_cnt++;
	mutex_unlock(&g_aw882xx_lock);
	aw_dev_info(&i2c->dev, "dev_cnt %d", g_aw882xx_dev_cnt);
#ifdef OPLUS_ARCH_EXTENDS
	aw882xx->total_pa_number = g_aw882xx_dev_cnt;
	oplus_aw882xx_cali_set_pa_number(aw882xx->total_pa_number);
	aw882xx_debug_init(aw882xx,i2c);
	aw882xx_re_f0_save_proc_init(aw882xx,i2c);
#ifdef AW_MTK_PLATFORM_WITH_DSP
	mtk_spk_set_type(5); // type for dsp calibration
#endif
#endif /*OPLUS_ARCH_EXTENDS*/
	return ret;
err_sysfs:
	aw_componet_codec_ops.unregister_codec(&i2c->dev);

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static void aw882xx_i2c_remove(struct i2c_client *i2c)
#else
static int aw882xx_i2c_remove(struct i2c_client *i2c)
#endif
{
	struct aw882xx *aw882xx = i2c_get_clientdata(i2c);
	struct gpio_desc *desc = NULL;

	aw_dev_info(aw882xx->dev, "enter");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	/*rm irq*/
	if (gpio_to_irq(aw882xx->irq_gpio)){
		desc = gpio_to_desc(aw882xx->irq_gpio);
		devm_gpiod_put(&i2c->dev, desc);
	}

	/*free gpio*/
	if (gpio_is_valid(aw882xx->irq_gpio)){
		desc = gpio_to_desc(aw882xx->irq_gpio);
		devm_gpiod_put(&i2c->dev, desc);
	}

	if (gpio_is_valid(aw882xx->reset_gpio)){
			desc = gpio_to_desc(aw882xx->reset_gpio);
			devm_gpiod_put(&i2c->dev, desc);
	}
#else
	/*rm irq*/
	if (gpio_to_irq(aw882xx->irq_gpio))
		devm_free_irq(&i2c->dev,
			gpio_to_irq(aw882xx->irq_gpio),
			aw882xx);

	/*free gpio*/
	if (gpio_is_valid(aw882xx->irq_gpio))
		devm_gpio_free(&i2c->dev, aw882xx->irq_gpio);
	if (gpio_is_valid(aw882xx->reset_gpio))
			devm_gpio_free(&i2c->dev, aw882xx->reset_gpio);
#endif

	/*rm attr node*/
	sysfs_remove_group(&i2c->dev.kobj, &aw882xx_attribute_group);

	/*free device resource */
	aw882xx_device_remove(aw882xx->aw_pa);

	/*unregister codec*/
	aw882xx->codec_ops->unregister_codec(&i2c->dev);

	/*remove device to total list*/
	mutex_lock(&g_aw882xx_lock);
	g_aw882xx_dev_cnt--;
	if (g_aw882xx_dev_cnt == 0) {
		if (g_awinic_cfg) {
			vfree(g_awinic_cfg);
			g_awinic_cfg = NULL;
		}
	}
	mutex_unlock(&g_aw882xx_lock);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
		return;
#else
		return 0;
#endif
}

static void aw882xx_i2c_shutdown(struct i2c_client *i2c)
{
	struct aw882xx *aw882xx = i2c_get_clientdata(i2c);

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_MM_FEEDBACK)
/* 2022/02/15, Add for smartpa err feedback.*/
	g_chk_err = false;
	cancel_delayed_work_sync(&aw882xx->check_work);
	aw882xx->check_step = 0;
	memset(&aw882xx->rd_re, 0, sizeof(aw_re_err_record_t));
	memset(&aw882xx->rd_f0, 0, sizeof(aw_f0_err_record_t));
#endif /* CONFIG_OPLUS_FEATURE_MM_FEEDBACK */
	aw_dev_info(aw882xx->dev, "enter");
	mutex_lock(&aw882xx->lock);
	aw882xx_device_stop(aw882xx->aw_pa);
	mutex_unlock(&aw882xx->lock);
}

static const struct i2c_device_id aw882xx_i2c_id[] = {
	{ AW882XX_I2C_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, aw882xx_i2c_id);

static const struct of_device_id aw882xx_dt_match[] = {
	{ .compatible = "awinic,aw882xx_smartpa" },
	{ },
};

static struct i2c_driver aw882xx_i2c_driver = {
	.driver = {
		.name = AW882XX_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw882xx_dt_match),
	},
	.probe = aw882xx_i2c_probe,
	.remove = aw882xx_i2c_remove,
	.shutdown = aw882xx_i2c_shutdown,
	.id_table = aw882xx_i2c_id,
};


static int __init aw882xx_i2c_init(void)
{
	int ret = -1;

	aw_pr_info("aw882xx driver version %s", AW882XX_DRIVER_VERSION);

	ret = i2c_add_driver(&aw882xx_i2c_driver);
	if (ret)
		aw_pr_err("fail to add aw882xx device into i2c");

	return ret;
}
module_init(aw882xx_i2c_init);


static void __exit aw882xx_i2c_exit(void)
{
	i2c_del_driver(&aw882xx_i2c_driver);
}
module_exit(aw882xx_i2c_exit);


MODULE_DESCRIPTION("ASoC AW882XX Smart PA Driver");
MODULE_LICENSE("GPL v2");



