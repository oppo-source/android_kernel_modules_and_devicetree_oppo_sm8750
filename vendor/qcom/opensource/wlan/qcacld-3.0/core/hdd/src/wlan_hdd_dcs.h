/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_dcs.h
 *
 * WLAN Host Device Driver dcs related implementation
 *
 */

#if !defined(WLAN_HDD_DCS_H)
#define WLAN_HDD_DCS_H

#include <wlan_hdd_main.h>
#include "wlan_dcs_ucfg_api.h"
#include "wlan_ll_sap_public_structs.h"

#ifdef DCS_INTERFERENCE_DETECTION
/**
 * hdd_dcs_register_cb() - register dcs callback
 * @hdd_ctx: hdd ctx
 *
 * This function is used to register the HDD callbacks with dcs component
 *
 * Return: None
 */
void hdd_dcs_register_cb(struct hdd_context *hdd_ctx);

/**
 * hdd_dcs_hostapd_set_chan() - switch sap channel to dcs channel
 * @hdd_ctx: hdd ctx
 * @vdev_id: vdev id
 * @dcs_ch_freq: dcs channel freq
 *
 * This function is used to switch sap channel to dcs channel using (E)CSA
 *
 * Return: None
 */
QDF_STATUS hdd_dcs_hostapd_set_chan(struct hdd_context *hdd_ctx,
				    uint8_t vdev_id,
				    qdf_freq_t dcs_ch_freq);

/**
 * hdd_dcs_chan_select_complete() - dcs triggered channel select
 * complete handling
 * @adapter: hdd adapter pointer
 *
 * When dcs triggered channel select complete, this function is used to do
 * switch sap to new dcs channel or just to enable interference mitigation
 *
 * Return: None
 */
void hdd_dcs_chan_select_complete(struct hdd_adapter *adapter);

/**
 * hdd_dcs_clear() - clear dcs information
 * @adapter: hdd adapter pointer
 *
 * This function is used to clear sap dcs related information such as
 * im stats and freq ctrl params
 *
 * Return: None
 */
void hdd_dcs_clear(struct hdd_adapter *adapter);

/*
 * hdd_dcs_trigger_csa_for_ll_lt_sap() - trigger csa for ll lt sap
 * @psoc: psoc object
 * @hdd_ctx: hdd context
 * @vdev_id: vdev id
 * @src: CSA source
 *
 * Return: None
 */
void hdd_dcs_trigger_csa_for_ll_lt_sap(struct wlan_objmgr_psoc *psoc,
				       struct hdd_context *hdd_ctx,
				       uint8_t vdev_id,
				       enum ll_sap_csa_source src);

#ifdef WLAN_FEATURE_VDEV_DCS
/**
 * hdd_send_dcs_cmd() - Send dcs command to firmware
 * @psoc: pointer to psoc
 * @mac_id: mac_id
 * @vdev_id: vdev_id
 *
 * Return: None
 */
void hdd_send_dcs_cmd(struct wlan_objmgr_psoc *psoc,
		      uint32_t mac_id, uint8_t vdev_id);
#else
static inline
void hdd_send_dcs_cmd(struct wlan_objmgr_psoc *psoc,
		      uint32_t mac_id, uint8_t vdev_id)
{
	ucfg_wlan_dcs_cmd(psoc, mac_id, true);
}
#endif
#else
static inline void hdd_dcs_register_cb(struct hdd_context *hdd_ctx)
{
}

static inline QDF_STATUS hdd_dcs_hostapd_set_chan(struct hdd_context *hdd_ctx,
						  uint8_t vdev_id,
						  qdf_freq_t dcs_ch_freq)
{
	return QDF_STATUS_SUCCESS;
}

static inline void hdd_dcs_chan_select_complete(struct hdd_adapter *adapter)
{
}

static inline void hdd_dcs_clear(struct hdd_adapter *adapter)
{
}

static inline
void hdd_send_dcs_cmd(struct wlan_objmgr_psoc *psoc,
		      uint32_t mac_id, uint8_t vdev_id)
{
}

static inline
void hdd_dcs_trigger_csa_for_ll_lt_sap(struct wlan_objmgr_psoc *psoc,
				       struct hdd_context *hdd_ctx,
				       uint8_t vdev_id,
				       enum ll_sap_csa_source src)
{
}

#endif
#endif /* end #if !defined(WLAN_HDD_DCS_H) */
