/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _WCD939X_H
#define _WCD939X_H

#include <bindings/audio-codec-port-types.h>

#define WCD939X_MAX_SLAVE_CH_TYPES 13
#define ZERO 0
#define WCD939X_DRV_NAME "wcd939x_codec"

enum {
	WCD9390 = 0,
	WCD9395 = 5,
};

/* from WCD to SWR DMIC events */
enum {
	WCD939X_EVT_SSR_DOWN,
	WCD939X_EVT_SSR_UP,
};

struct swr_slave_ch_map {
	u8 ch_type;
	u8 index;
};

static const struct swr_slave_ch_map swr_slv_tx_ch_idx[] = {
	{ADC1, 0},
	{ADC2, 1},
	{ADC3, 2},
	{ADC4, 3},
	{DMIC0, 4},
	{DMIC1, 5},
	{MBHC, 6},
	{DMIC2, 6},
	{DMIC3, 7},
	{DMIC4, 8},
	{DMIC5, 9},
	{DMIC6, 10},
	{DMIC7, 11},
};

static int swr_master_ch_map[] = {
	ZERO,
	SWRM_TX_PCM_OUT,
	SWRM_TX1_CH1,
	SWRM_TX1_CH2,
	SWRM_TX1_CH3,
	SWRM_TX1_CH4,
	SWRM_TX2_CH1,
	SWRM_TX2_CH2,
	SWRM_TX2_CH3,
	SWRM_TX2_CH4,
	SWRM_TX3_CH1,
	SWRM_TX3_CH2,
	SWRM_TX3_CH3,
	SWRM_TX3_CH4,
	SWRM_TX_PCM_IN,
};

#if IS_ENABLED(CONFIG_SND_SOC_WCD939X)
int wcd939x_info_create_codec_entry(struct snd_info_entry *codec_root,
				    struct snd_soc_component *component);

int wcd939x_get_codec_variant(struct snd_soc_component *component);
int wcd939x_codec_force_enable_micbias_v2(struct snd_soc_component *wcd939x,
					int event, int micb_num);
int wcd939x_swr_dmic_register_notifier(struct snd_soc_component *wcd939x,
                                        struct notifier_block *nblock,
                                        bool enable);
int wcd939x_codec_get_dev_num(struct snd_soc_component *component);

static inline int wcd939x_slave_get_master_ch_val(int ch)
{
	int i;

	for (i = 0; i < WCD939X_MAX_SLAVE_CH_TYPES; i++)
		if (ch == swr_master_ch_map[i])
			return i;
	return 0;
}

static inline int wcd939x_slave_get_master_ch(int idx)
{
	return swr_master_ch_map[idx];
}

static inline int wcd939x_slave_get_slave_ch_val(int ch)
{
	int i;

	for (i = 0; i < WCD939X_MAX_SLAVE_CH_TYPES; i++)
		if (ch == swr_slv_tx_ch_idx[i].ch_type)
			return swr_slv_tx_ch_idx[i].index;

	return -EINVAL;
}
int wcd939x_micb_external_event(struct device *dev, int micb, bool req);
#else
static inline int wcd939x_info_create_codec_entry(
					struct snd_info_entry *codec_root,
					struct snd_soc_component *component)
{
	return 0;
}
static inline int wcd939x_get_codec_variant(struct snd_soc_component *component)
{
	return 0;
}
static inline int wcd939x_codec_force_enable_micbias_v2(
					struct snd_soc_component *wcd939x,
					int event, int micb_num)
{
	return 0;
}

static inline int wcd939x_slave_get_master_ch_val(int ch)
{
	return 0;
}
static inline int wcd939x_slave_get_master_ch(int idx)
{
	return 0;
}
static inline int wcd939x_slave_get_slave_ch_val(int ch)
{
	return 0;
}
static int wcd939x_codec_get_dev_num(struct snd_soc_component *component)
{
	return 0;
}
int wcd939x_micb_external_event(struct device *dev, int micb, bool req)
{
	return 0;
}
#endif /* CONFIG_SND_SOC_WCD939X */
#endif /* _WCD939X_H */
