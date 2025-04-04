/************************************************************************************
** File: -
** OPLUS_ARCH_EXTENDS
** Copyright (C), 2020-2025, OPLUS Mobile Comm Corp., Ltd
**
** Description:
**     add audio extend driver
** Version: 1.0
** --------------------------- Revision History: --------------------------------
**               <author>                                <date>          <desc>
**
************************************************************************************/

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>

#define AUDIO_EXTEND_DRIVER_NAME "audio-extend-drv"

enum {
	CODEC_VENDOR_NXP = 0,
	CODEC_VENDOR_MAXIM,
	CODEC_VENDOR_AKM,
	CODEC_VENDOR_AWINIC,
	CODEC_VENDOR_END,
	CODEC_VENDOR_MAX = CODEC_VENDOR_END,
};

enum {
	CODEC_I2S_ID = 0,
	CODEC_NAME,
	CODEC_DAI_NAME,
	CODEC_VENDOR,
	CONFIG_PATH,
	CODEC_PROP_END,
	CODEC_PROP_MAX = CODEC_PROP_END,
};

static const char *extend_codec_vendor[CODEC_VENDOR_MAX] = {
	[CODEC_VENDOR_NXP] = "nxp",
	[CODEC_VENDOR_MAXIM] = "maxim",
	[CODEC_VENDOR_AKM] = "akm",
	[CODEC_VENDOR_AWINIC] = "awinic",
};

static const char *extend_speaker_prop[CODEC_PROP_MAX] = {
	[CODEC_I2S_ID] = "oplus,speaker-i2s-id",
	[CODEC_NAME] = "oplus,speaker-codec-name",
	[CODEC_DAI_NAME] = "oplus,speaker-codec-dai-name",
	[CODEC_VENDOR] = "oplus,speaker-vendor",
	[CONFIG_PATH] = "oplus,config-path",
};

static const char *extend_dac_prop[CODEC_PROP_MAX] = {
	[CODEC_I2S_ID] = "oplus,dac-i2s-id",
	[CODEC_NAME] = "oplus,dac-codec-name",
	[CODEC_DAI_NAME] = "oplus,dac-codec-dai-name",
	[CODEC_VENDOR] = "oplus,dac-vendor",
};

struct codec_prop_info {
	int dev_cnt;
	u32 i2s_id;
	const char **codec_name;
	const char **codec_dai_name;
	const char *codec_vendor;
	const char *config_path;
};

struct audio_extend_data {
	struct codec_prop_info *spk_pa_info;
	struct codec_prop_info *hp_dac_info;
	bool use_extern_spk;
	bool use_extern_dac;
};

static struct audio_extend_data *g_extend_pdata = NULL;

static int maxim_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	return 0;
}

static int ak4376_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	struct snd_soc_dapm_context *dapm = NULL;
	struct snd_soc_dai *codec_dai = NULL;
	codec_dai = asoc_rtd_to_codec(rtd, 0);
	component = codec_dai->component;

	if (!component) {
		pr_err("%s: could not find component for bolero_codec\n",
			__func__);
		return 0;
	}

	dapm = snd_soc_component_get_dapm(component);

	pr_info("%s() dev_name %s\n", __func__, dev_name(codec_dai->dev));

	snd_soc_dapm_ignore_suspend(dapm, "AK4376 HPL");
	snd_soc_dapm_ignore_suspend(dapm, "AK4376 HPR");
	snd_soc_dapm_ignore_suspend(dapm, "Playback");

	snd_soc_dapm_sync(dapm);

	return 0;
}

static ssize_t audio_extend_proc_read(struct file *filep, char __user *buf, size_t size, loff_t *ppos)
{
	ssize_t ret = 0;
	struct audio_extend_data *extend_data = NULL;

	if (!size || !filep || !ppos || !buf || *ppos < 0) {
		return -EINVAL;
	}

	extend_data = pde_data(file_inode(filep));
	if (!extend_data) {
		return -EINVAL;
	}

	if (!extend_data->spk_pa_info->config_path) {
		return -EINVAL;
	}

	pr_info("%s: config_path: %s", __func__, extend_data->spk_pa_info->config_path);

	ret = simple_read_from_buffer(buf, size, ppos, extend_data->spk_pa_info->config_path,
		strlen(extend_data->spk_pa_info->config_path));

	return ret;
}

static const struct proc_ops audio_extend_config_ops = {
	.proc_read = audio_extend_proc_read,
};

static struct proc_dir_entry *config_path_proc_dir = NULL;
static int audio_extend_proc_init(struct audio_extend_data *extend_data)
{
	const char *proc_path = "audio_config";
	struct proc_dir_entry *config_path = NULL;
	int ret = 0;

	if (config_path_proc_dir) {
		pr_info("%s: audio_config_path already register", __func__);
		return 0;
	}

	config_path_proc_dir = proc_mkdir(proc_path, NULL);
	if (IS_ERR(config_path_proc_dir)) {
		ret = PTR_ERR(config_path_proc_dir);
		config_path_proc_dir = NULL;
		return ret;
	}

	config_path = proc_create_data("path",
		S_IRUGO, config_path_proc_dir,
		&audio_extend_config_ops, extend_data);
	if (IS_ERR(config_path)) {
		ret = PTR_ERR(config_path);
		proc_remove(config_path_proc_dir);
		config_path_proc_dir = NULL;
		return ret;
	}

	return 0;
}

static int extend_codec_prop_parse(struct device *dev, const char *codec_prop[], struct codec_prop_info *codec_info)
{
	int ret = 0;

	ret = of_property_read_string(dev->of_node, codec_prop[CODEC_VENDOR], &codec_info->codec_vendor);
	if (ret) {
		pr_warn("%s: Looking up '%s' property in node %s failed\n",
			__func__, codec_prop[CODEC_VENDOR], dev->of_node->full_name);
		return -EINVAL;
	} else {
		pr_info("%s: codec vendor: %s\n", __func__, codec_info->codec_vendor);
	}

	ret = of_property_read_string(dev->of_node, codec_prop[CONFIG_PATH], &codec_info->config_path);
	if (ret) {
		pr_warn("%s: Looking up '%s' property in node %s failed\n",
			__func__, codec_prop[CONFIG_PATH], dev->of_node->full_name);
	} else {
		pr_info("%s: config path: %s\n", __func__, codec_info->config_path);
	}

	ret = of_property_read_u32(dev->of_node, codec_prop[CODEC_I2S_ID], &codec_info->i2s_id);
	if (ret) {
		pr_warn("%s: Looking up '%s' property in node %s failed\n",
			__func__, codec_prop[CODEC_I2S_ID], dev->of_node->full_name);
		return -EINVAL;
	} else {
		pr_info("%s: i2s id: %d\n", __func__, codec_info->i2s_id);
	}

	ret = of_property_count_strings(dev->of_node, codec_prop[CODEC_NAME]);
	if (ret <= 0) {
		pr_warn("%s: Invalid number of codecs, node %s, ret=%d\n",
			__func__, dev->of_node->full_name, ret);
		return -EINVAL;
	} else {
		codec_info->dev_cnt = ret;
		pr_info("%s: dev_cnt %d\n", __func__, codec_info->dev_cnt);
	}

	codec_info->codec_name = devm_kzalloc(dev, codec_info->dev_cnt * sizeof(char *), GFP_KERNEL);
	if (!codec_info->codec_name) {
		pr_warn("%s: kzalloc fail for codec_name!\n", __func__);
		return -ENOMEM;
	}
	ret = of_property_read_string_array(dev->of_node, codec_prop[CODEC_NAME], codec_info->codec_name, codec_info->dev_cnt);
	if (ret < 0) {
		pr_warn("%s: Looking up '%s' property in node %s failed\n",
			__func__, codec_prop[CODEC_NAME], dev->of_node->full_name);
		return -EINVAL;
	}

	codec_info->codec_dai_name = devm_kzalloc(dev, codec_info->dev_cnt * sizeof(char *), GFP_KERNEL);
	if (!codec_info->codec_dai_name) {
		pr_warn("%s: kzalloc fail for codec_dai_name!\n", __func__);
		return -ENOMEM;
	}
	ret = of_property_read_string_array(dev->of_node, codec_prop[CODEC_DAI_NAME], codec_info->codec_dai_name, codec_info->dev_cnt);
	if (ret < 0) {
		pr_warn("%s: Looking up '%s' property in node %s failed\n",
			__func__, codec_prop[CODEC_DAI_NAME], dev->of_node->full_name);
		return -EINVAL;
	}

	return 0;
}

static void extend_codec_be_dailink(struct device *dev, struct codec_prop_info *codec_info, struct snd_soc_dai_link *dailink, size_t size)
{
	int i2s_id = 0;
	int i = 0;
	struct snd_soc_dai_link_component *codecs_comp = NULL;
	bool dais_register_status = false;

	if (!codec_info) {
		pr_err("%s: codec_info param invalid!\n", __func__);
		return;
	}

	if (!dailink) {
		pr_err("%s: dailink param invalid!\n", __func__);
		return;
	}

	i2s_id = codec_info->i2s_id;
	pr_info("%s: i2s_id=%d, size=%zu!\n", __func__, i2s_id, size);
	if ((i2s_id * 2 + 1) >= size) {
		pr_err("%s: i2s_id param invalid!\n", __func__);
		return;
	}
	pr_info("%s: codec vendor: %s, dev_cnt: %d.\n", __func__, codec_info->codec_vendor, codec_info->dev_cnt);
	codecs_comp = devm_kzalloc(dev, sizeof(struct snd_soc_dai_link_component) * codec_info->dev_cnt, GFP_KERNEL);
	if (!codecs_comp) {
		dev_err(dev, "%s: codec component alloc failed\n", __func__);
		return;
	}

	for (i = 0;i < codec_info->dev_cnt; i++) {
		codecs_comp[i].name = codec_info->codec_name[i];
		codecs_comp[i].dai_name = codec_info->codec_dai_name[i];
		if (!snd_soc_find_dai(&codecs_comp[i])) {
			pr_err("%s: dai %s not register, so extend be dailink failed\n", __func__, codecs_comp[i].name);
			dais_register_status = false;
			break;
		} else {
			dais_register_status = true;
			pr_info("%s: dais[%d] name:%s, dai_name:%s\n", __func__, i, codecs_comp[i].name, codecs_comp[i].dai_name);
		}
	}

	if (dais_register_status) {
		pr_info("%s: use %s dailink replace\n", __func__, codec_info->codec_vendor);
		/* RX dailink */
		dailink[i2s_id*2].codecs = codecs_comp;
		dailink[i2s_id*2].num_codecs = codec_info->dev_cnt;
		if (!strcmp(codec_info->codec_vendor, extend_codec_vendor[CODEC_VENDOR_MAXIM])) {
			/* TX dailink */
			dailink[i2s_id*2+1].codecs = codecs_comp;
			dailink[i2s_id*2+1].num_codecs = codec_info->dev_cnt;
			dailink[i2s_id*2+1].be_hw_params_fixup = maxim_be_hw_params_fixup;
		}
		if (!strcmp(codec_info->codec_vendor, extend_codec_vendor[CODEC_VENDOR_AKM])) {
			dailink[i2s_id*2].init = ak4376_audrx_init;
		}
		if (!strcmp(codec_info->codec_vendor, extend_codec_vendor[CODEC_VENDOR_AWINIC])) {
			/* TX dailink */
			dailink[i2s_id*2+1].codecs = codecs_comp;
			dailink[i2s_id*2+1].num_codecs = codec_info->dev_cnt;
		}
	}
}

void extend_codec_i2s_be_dailinks(struct device *dev, struct snd_soc_dai_link *dailink, size_t size)
{
	if (!g_extend_pdata) {
		pr_err("%s: No extend data, do nothing.\n", __func__);
		return;
	}

	pr_info("%s: use_extern_spk %d\n", __func__, g_extend_pdata->use_extern_spk);
	if (g_extend_pdata->use_extern_spk && g_extend_pdata->spk_pa_info) {
		extend_codec_be_dailink(dev, g_extend_pdata->spk_pa_info, dailink, size);
	}

	pr_info("%s: use_extern_dac %d\n", __func__, g_extend_pdata->use_extern_dac);
	if (g_extend_pdata->use_extern_dac && g_extend_pdata->hp_dac_info) {
		extend_codec_be_dailink(dev, g_extend_pdata->hp_dac_info, dailink, size);
	}
}
EXPORT_SYMBOL(extend_codec_i2s_be_dailinks);

static int audio_extend_probe(struct platform_device *pdev)
{
	int ret = 0;

	dev_info(&pdev->dev, "%s: dev name %s\n", __func__,
		dev_name(&pdev->dev));

	if (!pdev->dev.of_node) {
		pr_err("%s: No dev node from device tree\n", __func__);
		return -EINVAL;
	}

	g_extend_pdata = devm_kzalloc(&pdev->dev, sizeof(struct audio_extend_data), GFP_KERNEL);
	if (!g_extend_pdata) {
		pr_err("%s: kzalloc mem fail!\n", __func__);
		return -ENOMEM;
	}

	g_extend_pdata->spk_pa_info =  devm_kzalloc(&pdev->dev, sizeof(struct codec_prop_info), GFP_KERNEL);
	if (g_extend_pdata->spk_pa_info) {
		ret = extend_codec_prop_parse(&pdev->dev, extend_speaker_prop, g_extend_pdata->spk_pa_info);
		if (ret == 0) {
			g_extend_pdata->use_extern_spk = true;
		} else {
			g_extend_pdata->use_extern_spk = false;
		}
	} else {
		g_extend_pdata->use_extern_spk = false;
		pr_warn("%s: kzalloc for spk pa info fail!\n", __func__);
	}

	g_extend_pdata->hp_dac_info =  devm_kzalloc(&pdev->dev, sizeof(struct codec_prop_info), GFP_KERNEL);
	if (g_extend_pdata->hp_dac_info) {
		ret = extend_codec_prop_parse(&pdev->dev, extend_dac_prop, g_extend_pdata->hp_dac_info);
		if (ret == 0) {
			g_extend_pdata->use_extern_dac = true;
		} else {
			g_extend_pdata->use_extern_dac = false;
		}
	} else {
		g_extend_pdata->use_extern_dac = false;
		pr_warn("%s: kzalloc for hp dac info fail!\n", __func__);
	}

	if (g_extend_pdata->spk_pa_info->config_path) {
		audio_extend_proc_init(g_extend_pdata);
	}

	return 0;
}

static int audio_extend_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s: dev name %s\n", __func__,
		dev_name(&pdev->dev));

	return 0;
}

static const struct of_device_id audio_extend_of_match[] = {
	{.compatible = "oplus,asoc-audio"},
	{ }
};
MODULE_DEVICE_TABLE(of, audio_extend_of_match);

static struct platform_driver audio_extend_driver = {
	.probe          = audio_extend_probe,
	.remove         = audio_extend_remove,
	.driver         = {
		.name   = AUDIO_EXTEND_DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = audio_extend_of_match,
		.suppress_bind_attrs = true,
	},
};

static int __init audio_extend_init(void)
{
	return platform_driver_register(&audio_extend_driver);
}

static void __exit audio_extend_exit(void)
{
	platform_driver_unregister(&audio_extend_driver);
}

module_init(audio_extend_init);
module_exit(audio_extend_exit);
MODULE_DESCRIPTION("ASoC Oplus Audio Driver");
MODULE_LICENSE("GPL v2");
