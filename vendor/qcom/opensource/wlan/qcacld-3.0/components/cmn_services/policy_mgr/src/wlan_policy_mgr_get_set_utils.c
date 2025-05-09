/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_policy_mgr_get_set_utils.c
 *
 * WLAN Concurrenct Connection Management APIs
 *
 */

/* Include files */
#include "target_if.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_policy_mgr_i.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_nan_api.h"
#include "nan_public_structs.h"
#include "wlan_reg_services_api.h"
#include "wlan_cm_roam_public_struct.h"
#include "wlan_mlme_api.h"
#include "wlan_mlme_main.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include "wlan_mlo_mgr_sta.h"
#include "wlan_cm_ucfg_api.h"
#include "wlan_cm_roam_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_p2p_ucfg_api.h"
#include "wlan_mlo_link_force.h"
#include "wlan_connectivity_logging.h"
#include "wlan_policy_mgr_ll_sap.h"
#include "wlan_nan_api_i.h"

/* invalid channel id. */
#define INVALID_CHANNEL_ID 0

#define IS_FREQ_ON_MAC_ID(freq_range, freq, mac_id) \
	((freq >= freq_range[mac_id].low_2ghz_freq && \
	  freq <= freq_range[mac_id].high_2ghz_freq) || \
	(freq >= freq_range[mac_id].low_5ghz_freq && \
	 freq <= freq_range[mac_id].high_5ghz_freq))

/**
 * policy_mgr_debug_alert() - fatal error alert
 *
 * This function will flush host drv log and
 * disable all level logs.
 * It can be called in fatal error detected in policy
 * manager.
 * This is to avoid host log overwritten in stress
 * test to help issue debug.
 *
 * Return: none
 */
static void
policy_mgr_debug_alert(void)
{
	int module_id;
	int qdf_print_idx;

	policy_mgr_err("fatal error detected to flush and pause host log");
	qdf_logging_flush_logs();
	qdf_print_idx = qdf_get_pidx();
	for (module_id = 0; module_id < QDF_MODULE_ID_MAX; module_id++)
		qdf_print_set_category_verbose(
					qdf_print_idx,
					module_id, QDF_TRACE_LEVEL_NONE,
					0);
}

QDF_STATUS
policy_mgr_get_allow_mcc_go_diff_bi(struct wlan_objmgr_psoc *psoc,
				    uint8_t *allow_mcc_go_diff_bi)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*allow_mcc_go_diff_bi = pm_ctx->cfg.allow_mcc_go_diff_bi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_set_dual_mac_feature(struct wlan_objmgr_psoc *psoc,
					   uint8_t dual_mac_feature)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->cfg.dual_mac_feature = dual_mac_feature;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_dual_mac_feature(struct wlan_objmgr_psoc *psoc,
					   uint8_t *dual_mac_feature)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*dual_mac_feature = pm_ctx->cfg.dual_mac_feature;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_force_1x1(struct wlan_objmgr_psoc *psoc,
				    uint8_t *force_1x1)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*force_1x1 = pm_ctx->cfg.is_force_1x1_enable;

	return QDF_STATUS_SUCCESS;
}

uint32_t policy_mgr_get_max_conc_cxns(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return 0;
	}

	return pm_ctx->cfg.max_conc_cxns;
}

QDF_STATUS policy_mgr_set_max_conc_cxns(struct wlan_objmgr_psoc *psoc,
					uint32_t max_conc_cxns)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_debug("set max_conc_cxns %d old %d", max_conc_cxns,
			 pm_ctx->cfg.max_conc_cxns);
	pm_ctx->cfg.max_conc_cxns = max_conc_cxns;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_set_sta_sap_scc_on_dfs_chnl(struct wlan_objmgr_psoc *psoc,
				       uint8_t sta_sap_scc_on_dfs_chnl)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->cfg.sta_sap_scc_on_dfs_chnl = sta_sap_scc_on_dfs_chnl;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_sta_sap_scc_on_dfs_chnl(struct wlan_objmgr_psoc *psoc,
				       uint8_t *sta_sap_scc_on_dfs_chnl)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*sta_sap_scc_on_dfs_chnl = pm_ctx->cfg.sta_sap_scc_on_dfs_chnl;

	return QDF_STATUS_SUCCESS;
}

bool
policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return false;
	}

	return pm_ctx->cfg.sta_sap_scc_on_indoor_channel;
}

QDF_STATUS
policy_mgr_set_multi_sap_allowed_on_same_band(struct wlan_objmgr_psoc *psoc,
					bool multi_sap_allowed_on_same_band)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->cfg.multi_sap_allowed_on_same_band =
				multi_sap_allowed_on_same_band;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_multi_sap_allowed_on_same_band(struct wlan_objmgr_psoc *psoc,
					bool *multi_sap_allowed_on_same_band)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*multi_sap_allowed_on_same_band =
				pm_ctx->cfg.multi_sap_allowed_on_same_band;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_set_original_bw_for_sap_restart(struct wlan_objmgr_psoc *psoc,
					   bool use_sap_original_bw)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->cfg.use_sap_original_bw = use_sap_original_bw;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_original_bw_for_sap_restart(struct wlan_objmgr_psoc *psoc,
					   bool *use_sap_original_bw)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*use_sap_original_bw = pm_ctx->cfg.use_sap_original_bw;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_dfs_sta_sap_go_scc_movement(struct wlan_objmgr_psoc *psoc,
					   bool *move_sap_go_first)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*move_sap_go_first = pm_ctx->cfg.move_sap_go_1st_on_dfs_sta_csa;

	return QDF_STATUS_SUCCESS;
}

bool
policy_mgr_update_dfs_master_dynamic_enabled(struct wlan_objmgr_psoc *psoc,
					     bool always_update_target,
					     struct wlan_channel *des_chan)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	qdf_freq_t sta_5g_freqs[MAX_NUMBER_OF_CONC_CONNECTIONS];
	qdf_freq_t ap_5g_freqs[MAX_NUMBER_OF_CONC_CONNECTIONS * 2];
	uint16_t ap_ch_flagext[MAX_NUMBER_OF_CONC_CONNECTIONS * 2];
	uint32_t num_5g_sta = 0, num_5g_ap = 0;
	bool sta_on_5g = false;
	bool sta_on_2g = false;
	bool ap_on_5g = false;
	uint32_t i, j;
	bool enable = true;
	bool curr_enabled;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return true;
	}

	if (!pm_ctx->cfg.sta_sap_scc_on_dfs_chnl) {
		enable = true;
		goto end;
	}
	if (pm_ctx->cfg.sta_sap_scc_on_dfs_chnl ==
	    PM_STA_SAP_ON_DFS_MASTER_MODE_DISABLED) {
		enable = false;
		goto end;
	}
	if (pm_ctx->cfg.sta_sap_scc_on_dfs_chnl !=
	    PM_STA_SAP_ON_DFS_MASTER_MODE_FLEX) {
		policy_mgr_debug("sta_sap_scc_on_dfs_chnl %d unknown",
				 pm_ctx->cfg.sta_sap_scc_on_dfs_chnl);
		enable = true;
		goto end;
	}

	if (des_chan && WLAN_REG_IS_5GHZ_CH_FREQ(des_chan->ch_freq)) {
		ap_5g_freqs[num_5g_ap] = des_chan->ch_freq;
		ap_ch_flagext[num_5g_ap++] = des_chan->ch_flagext;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (!pm_conc_connection_list[i].in_use)
			continue;
		if ((pm_conc_connection_list[i].mode == PM_SAP_MODE ||
		     pm_conc_connection_list[i].mode == PM_P2P_GO_MODE) &&
		     WLAN_REG_IS_5GHZ_CH_FREQ(
		     pm_conc_connection_list[i].freq)) {
			ap_5g_freqs[num_5g_ap] =
				pm_conc_connection_list[i].freq;
			ap_ch_flagext[num_5g_ap++] =
				pm_conc_connection_list[i].ch_flagext;
			ap_on_5g = true;
			continue;
		}

		if (!(pm_conc_connection_list[i].mode == PM_STA_MODE ||
		      pm_conc_connection_list[i].mode == PM_P2P_CLIENT_MODE))
			continue;
		if (WLAN_REG_IS_5GHZ_CH_FREQ(pm_conc_connection_list[i].freq)) {
			sta_on_5g = true;
			sta_5g_freqs[num_5g_sta++] =
				pm_conc_connection_list[i].freq;
		} else {
			sta_on_2g = true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (policy_mgr_is_hw_dbs_capable(psoc) && !sta_on_5g)
		enable = true;
	else if (!sta_on_5g && !sta_on_2g)
		enable = true;
	else
		enable = false;

	if (policy_mgr_is_hw_sbs_capable(psoc)) {
		enable = true;
		for (i = 0; i < num_5g_sta; i++) {
			for (j = 0; j < num_5g_ap; j++) {
				if (policy_mgr_2_freq_always_on_same_mac(
				    psoc, sta_5g_freqs[i], ap_5g_freqs[j]) &&
				    ap_ch_flagext[j] & (IEEE80211_CHAN_DFS |
				    IEEE80211_CHAN_DFS_CFREQ2)) {
					policy_mgr_debug("sta %d ap(dfs) %d on same mac",
							 sta_5g_freqs[i],
							 ap_5g_freqs[j]);
					enable = false;
					break;
				}
			}
		}
	}

end:
	curr_enabled = !pm_ctx->dynamic_dfs_master_disabled;
	pm_ctx->dynamic_dfs_master_disabled = !enable;
	if (!enable)
		policy_mgr_debug("sta_sap_scc_on_dfs_chnl %d sta_on_2g %d sta_on_5g %d enable %d curr %d",
				 pm_ctx->cfg.sta_sap_scc_on_dfs_chnl,
				 sta_on_2g, sta_on_5g, enable, curr_enabled);

	if ((curr_enabled != enable) && ap_on_5g)
		always_update_target = true;

	if (always_update_target)
		tgt_dfs_radar_enable(pm_ctx->pdev, 0, 0, enable);

	return enable;
}

bool
policy_mgr_get_dfs_master_dynamic_enabled(
	struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return true;
	}

	if (pm_ctx->dynamic_dfs_master_disabled)
		policy_mgr_debug("sta_sap_scc_on_dfs_chnl %d enable %d",
				 pm_ctx->cfg.sta_sap_scc_on_dfs_chnl,
				 !pm_ctx->dynamic_dfs_master_disabled);

	return !pm_ctx->dynamic_dfs_master_disabled;
}

bool
policy_mgr_get_can_skip_radar_event(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return false;
	}

	return pm_ctx->dynamic_dfs_master_disabled;
}

QDF_STATUS
policy_mgr_get_sta_sap_scc_lte_coex_chnl(struct wlan_objmgr_psoc *psoc,
					 uint8_t *sta_sap_scc_lte_coex)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*sta_sap_scc_lte_coex = pm_ctx->cfg.sta_sap_scc_on_lte_coex_chnl;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_sap_mandt_chnl(struct wlan_objmgr_psoc *psoc,
					 uint8_t *sap_mandt_chnl)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*sap_mandt_chnl = pm_ctx->cfg.sap_mandatory_chnl_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_indoor_chnl_marking(struct wlan_objmgr_psoc *psoc,
				   uint8_t *indoor_chnl_marking)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*indoor_chnl_marking = pm_ctx->cfg.mark_indoor_chnl_disable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_mcc_scc_switch(struct wlan_objmgr_psoc *psoc,
					      uint8_t *mcc_scc_switch)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*mcc_scc_switch = pm_ctx->cfg.mcc_to_scc_switch;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_sys_pref(struct wlan_objmgr_psoc *psoc,
					uint8_t *sys_pref)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*sys_pref = pm_ctx->cfg.sys_pref;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_set_sys_pref(struct wlan_objmgr_psoc *psoc,
				   uint8_t sys_pref)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->cfg.sys_pref = sys_pref;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_conc_rule1(struct wlan_objmgr_psoc *psoc,
						uint8_t *conc_rule1)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*conc_rule1 = pm_ctx->cfg.conc_rule1;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_conc_rule2(struct wlan_objmgr_psoc *psoc,
						uint8_t *conc_rule2)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*conc_rule2 = pm_ctx->cfg.conc_rule2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_chnl_select_plcy(struct wlan_objmgr_psoc *psoc,
					   uint32_t *chnl_select_plcy)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*chnl_select_plcy = pm_ctx->cfg.chnl_select_plcy;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_set_ch_select_plcy(struct wlan_objmgr_psoc *psoc,
					 uint32_t ch_select_policy)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->cfg.chnl_select_plcy = ch_select_policy;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_set_dynamic_mcc_adaptive_sch(
				struct wlan_objmgr_psoc *psoc,
				bool dynamic_mcc_adaptive_sched)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->dynamic_mcc_adaptive_sched = dynamic_mcc_adaptive_sched;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_dynamic_mcc_adaptive_sch(
				struct wlan_objmgr_psoc *psoc,
				bool *dynamic_mcc_adaptive_sched)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*dynamic_mcc_adaptive_sched = pm_ctx->dynamic_mcc_adaptive_sched;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_mcc_adaptive_sch(struct wlan_objmgr_psoc *psoc,
					   bool *enable_mcc_adaptive_sch)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*enable_mcc_adaptive_sch = pm_ctx->cfg.enable_mcc_adaptive_sch;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_sta_cxn_5g_band(struct wlan_objmgr_psoc *psoc,
					  uint8_t *enable_sta_cxn_5g_band)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*enable_sta_cxn_5g_band = pm_ctx->cfg.enable_sta_cxn_5g_band;

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_update_new_hw_mode_index(struct wlan_objmgr_psoc *psoc,
		uint32_t new_hw_mode_index)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	pm_ctx->new_hw_mode_index = new_hw_mode_index;
}

void policy_mgr_update_old_hw_mode_index(struct wlan_objmgr_psoc *psoc,
		uint32_t old_hw_mode_index)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	pm_ctx->old_hw_mode_index = old_hw_mode_index;
}

void policy_mgr_update_hw_mode_index(struct wlan_objmgr_psoc *psoc,
		uint32_t new_hw_mode_index)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	if (POLICY_MGR_DEFAULT_HW_MODE_INDEX == pm_ctx->new_hw_mode_index) {
		pm_ctx->new_hw_mode_index = new_hw_mode_index;
	} else {
		pm_ctx->old_hw_mode_index = pm_ctx->new_hw_mode_index;
		pm_ctx->new_hw_mode_index = new_hw_mode_index;
	}
	policy_mgr_debug("Updated: old_hw_mode_index:%d new_hw_mode_index:%d",
		pm_ctx->old_hw_mode_index, pm_ctx->new_hw_mode_index);
}

/**
 * policy_mgr_get_num_of_setbits_from_bitmask() - to get num of
 * setbits from bitmask
 * @mask: given bitmask
 *
 * This helper function should return number of setbits from bitmask
 *
 * Return: number of setbits from bitmask
 */
static uint32_t policy_mgr_get_num_of_setbits_from_bitmask(uint32_t mask)
{
	uint32_t num_of_setbits = 0;

	while (mask) {
		mask &= (mask - 1);
		num_of_setbits++;
	}
	return num_of_setbits;
}

/**
 * policy_mgr_map_wmi_channel_width_to_hw_mode_bw() - returns
 * bandwidth in terms of hw_mode_bandwidth
 * @width: bandwidth in terms of wmi_channel_width
 *
 * This function returns the bandwidth in terms of hw_mode_bandwidth.
 *
 * Return: BW in terms of hw_mode_bandwidth.
 */
static enum hw_mode_bandwidth policy_mgr_map_wmi_channel_width_to_hw_mode_bw(
		wmi_channel_width width)
{
	switch (width) {
	case WMI_CHAN_WIDTH_20:
		return HW_MODE_20_MHZ;
	case WMI_CHAN_WIDTH_40:
		return HW_MODE_40_MHZ;
	case WMI_CHAN_WIDTH_80:
		return HW_MODE_80_MHZ;
	case WMI_CHAN_WIDTH_160:
		return HW_MODE_160_MHZ;
	case WMI_CHAN_WIDTH_80P80:
		return HW_MODE_80_PLUS_80_MHZ;
	case WMI_CHAN_WIDTH_5:
		return HW_MODE_5_MHZ;
	case WMI_CHAN_WIDTH_10:
		return HW_MODE_10_MHZ;
#ifdef WLAN_FEATURE_11BE
	case WMI_CHAN_WIDTH_320:
		return HW_MODE_320_MHZ;
#endif
	default:
		return HW_MODE_BW_NONE;
	}

	return HW_MODE_BW_NONE;
}

static void policy_mgr_get_hw_mode_params(
		struct wlan_psoc_host_mac_phy_caps *caps,
		struct policy_mgr_mac_ss_bw_info *info)
{
	qdf_freq_t max_5g_freq;

	if (!caps) {
		policy_mgr_err("Invalid capabilities");
		return;
	}

	info->mac_tx_stream = policy_mgr_get_num_of_setbits_from_bitmask(
		QDF_MAX(caps->tx_chain_mask_2G,
		caps->tx_chain_mask_5G));
	info->mac_rx_stream = policy_mgr_get_num_of_setbits_from_bitmask(
		QDF_MAX(caps->rx_chain_mask_2G,
		caps->rx_chain_mask_5G));
	info->mac_bw = policy_mgr_map_wmi_channel_width_to_hw_mode_bw(
		QDF_MAX(caps->max_bw_supported_2G,
		caps->max_bw_supported_5G));
	info->mac_band_cap = caps->supported_bands;

	if (caps->supported_bands & WMI_HOST_WLAN_5G_CAPABILITY) {
		max_5g_freq = wlan_reg_max_6ghz_chan_freq() ?
				wlan_reg_max_6ghz_chan_freq() :
				wlan_reg_max_5ghz_chan_freq();
		max_5g_freq = caps->reg_cap_ext.high_5ghz_chan ?
				QDF_MIN(caps->reg_cap_ext.high_5ghz_chan,
					max_5g_freq) : max_5g_freq;
		info->support_6ghz_band =
			max_5g_freq > wlan_reg_min_6ghz_chan_freq();
	}
}

QDF_STATUS policy_mgr_update_nss_req(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct target_psoc_info *tgt_hdl;
	struct wlan_psoc_host_mac_phy_caps *tmp;
	struct tgt_info *info;
	struct policy_mgr_mac_ss_bw_info mac_ss_bw_info;
	uint8_t i,  mac_found = 0;
	uint32_t lmac_id;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	tgt_hdl = wlan_psoc_get_tgt_if_handle(psoc);
	if (!tgt_hdl) {
		policy_mgr_err("tgt_hdl not found");
		return QDF_STATUS_E_FAILURE;
	}

	info = &tgt_hdl->info;
	lmac_id = policy_mgr_mode_get_macid_by_vdev_id(psoc, vdev_id);
	if (lmac_id == 0xFF) {
		policy_mgr_err("mac_id not found");
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		tmp = &info->mac_phy_cap[i];

		if (tmp->lmac_id  != lmac_id)
			continue;

		policy_mgr_get_hw_mode_params(tmp, &mac_ss_bw_info);
		mac_found = 1;
		break;
	}

	if (!mac_found) {
		policy_mgr_err("mac_id : %d not found", lmac_id);
		return QDF_STATUS_E_FAILURE;
	}

	return pm_ctx->hdd_cbacks.wlan_set_tx_rx_nss_cb(psoc, vdev_id,
							mac_ss_bw_info.mac_tx_stream,
							mac_ss_bw_info.mac_rx_stream);
}

/**
 * policy_mgr_set_hw_mode_params() - sets TX-RX stream,
 * bandwidth and DBS in hw_mode_list
 * @psoc: PSOC object information
 * @mac0_ss_bw_info: TX-RX streams, BW for MAC0
 * @mac1_ss_bw_info: TX-RX streams, BW for MAC1
 * @pos: refers to hw_mode_list array index
 * @hw_mode_id: hw mode id value used by firmware
 * @dbs_mode: dbs_mode for the dbs_hw_mode
 * @sbs_mode: sbs_mode for the sbs_hw_mode
 * @emlsr_mode: emlsr_mode for the emlsr_hw_mode
 *
 * This function sets TX-RX stream, bandwidth and DBS mode in
 * hw_mode_list.
 *
 * Return: none
 */
static void policy_mgr_set_hw_mode_params(struct wlan_objmgr_psoc *psoc,
			struct policy_mgr_mac_ss_bw_info mac0_ss_bw_info,
			struct policy_mgr_mac_ss_bw_info mac1_ss_bw_info,
			uint32_t pos, uint32_t hw_mode_id, uint32_t dbs_mode,
			uint32_t sbs_mode, uint64_t emlsr_mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint64_t legacy_hwmode_lst;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_tx_stream);
	POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_rx_stream);
	POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_bw);
	POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac1_ss_bw_info.mac_tx_stream);
	POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac1_ss_bw_info.mac_rx_stream);
	POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac1_ss_bw_info.mac_bw);
	POLICY_MGR_HW_MODE_DBS_MODE_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		dbs_mode);
	POLICY_MGR_HW_MODE_AGILE_DFS_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		HW_MODE_AGILE_DFS_NONE);
	POLICY_MGR_HW_MODE_SBS_MODE_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		sbs_mode);
	POLICY_MGR_HW_MODE_MAC0_BAND_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_band_cap);
	POLICY_MGR_HW_MODE_ID_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		hw_mode_id);

	legacy_hwmode_lst = pm_ctx->hw_mode.hw_mode_list[pos];
	POLICY_MGR_HW_MODE_EMLSR_MODE_SET(
	    pm_ctx->hw_mode.hw_mode_list[pos],
	    legacy_hwmode_lst, emlsr_mode);
}

QDF_STATUS policy_mgr_get_radio_combinations(struct wlan_objmgr_psoc *psoc,
					     struct radio_combination *comb,
					     uint32_t comb_max,
					     uint32_t *comb_num)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct radio_combination *radio_comb;
	uint32_t i;
	bool dbs_or_sbs_enabled = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	*comb_num = 0;
	if (policy_mgr_is_hw_dbs_capable(psoc) ||
	    policy_mgr_is_hw_sbs_capable(psoc))
		dbs_or_sbs_enabled = true;

	for (i = 0; i < pm_ctx->radio_comb_num; i++) {
		radio_comb = &pm_ctx->radio_combinations[i];
		if (!dbs_or_sbs_enabled && radio_comb->hw_mode != MODE_SMM)
			continue;
		if (*comb_num >= comb_max) {
			policy_mgr_err("out of buffer %d max %d",
				       pm_ctx->radio_comb_num,
				       comb_max);
			return QDF_STATUS_E_FAILURE;
		}
		policy_mgr_debug("radio %d: mode %d mac0 (0x%x, 0x%x), mac1 (0x%x 0x%x)",
				 *comb_num,
				 radio_comb->hw_mode,
				 radio_comb->band_mask[0],
				 radio_comb->antenna[0],
				 radio_comb->band_mask[1],
				 radio_comb->antenna[1]);
		qdf_mem_copy(&comb[*comb_num], radio_comb,
			     sizeof(*radio_comb));
		(*comb_num)++;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_add_radio_comb() - Add radio combination
 * @pm_ctx: bandwidth in terms of wmi_channel_width
 * @radio: radio combination
 *
 * This function adds one radio combination to list
 *
 * Return: void
 */
static void policy_mgr_add_radio_comb(struct policy_mgr_psoc_priv_obj *pm_ctx,
				      struct radio_combination *radio)
{
	uint32_t i;
	struct radio_combination *comb;

	/* don't add duplicated item */
	for (i = 0; i < pm_ctx->radio_comb_num; i++) {
		comb = &pm_ctx->radio_combinations[i];
		if (radio->hw_mode == comb->hw_mode &&
		    radio->band_mask[0] == comb->band_mask[0] &&
		    radio->band_mask[1] == comb->band_mask[1] &&
		    radio->antenna[0] == comb->antenna[0] &&
		    radio->antenna[1] == comb->antenna[1])
			return;
	}
	if (pm_ctx->radio_comb_num == MAX_RADIO_COMBINATION) {
		policy_mgr_err("radio combination overflow %d",
			       pm_ctx->radio_comb_num);
		return;
	}
	policy_mgr_debug("radio %d: mode %d mac0 (0x%x, 0x%x), mac1 (0x%x 0x%x)",
			 pm_ctx->radio_comb_num,
			 radio->hw_mode,
			 radio->band_mask[0],
			 radio->antenna[0],
			 radio->band_mask[1],
			 radio->antenna[1]);

	qdf_mem_copy(&pm_ctx->radio_combinations[pm_ctx->radio_comb_num],
		     radio, sizeof(*radio));
	pm_ctx->radio_comb_num++;
}

#define SET_RADIO(_radio, _mode, _mac0_band, _mac1_band,\
		  _mac0_antenna, _mac1_antenna) \
do { \
	(_radio)->hw_mode = _mode; \
	(_radio)->band_mask[0] = _mac0_band; \
	(_radio)->band_mask[1] = _mac1_band; \
	(_radio)->antenna[0] = _mac0_antenna; \
	(_radio)->antenna[1] = _mac1_antenna; \
} while (0)

/**
 * policy_mgr_update_radio_combination_matrix() - Update radio combination
 * list
 * @psoc: psoc object
 * @mac0_ss_bw_info: mac 0 band/bw info
 * @mac1_ss_bw_info: mac 1 band/bw info
 * @dbs_mode: dbs mode
 * @sbs_mode: sbs mode
 *
 * This function updates radio combination list based on hw mode information.
 *
 * Return: void
 */
static void
policy_mgr_update_radio_combination_matrix(
			struct wlan_objmgr_psoc *psoc,
			struct policy_mgr_mac_ss_bw_info mac0_ss_bw_info,
			struct policy_mgr_mac_ss_bw_info mac1_ss_bw_info,
			uint32_t dbs_mode, uint32_t sbs_mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct radio_combination radio;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	if (!dbs_mode && !sbs_mode) {
		if (mac0_ss_bw_info.mac_band_cap &
					WMI_HOST_WLAN_2G_CAPABILITY) {
			SET_RADIO(&radio, MODE_SMM, BIT(REG_BAND_2G), 0,
				  mac0_ss_bw_info.mac_tx_stream, 0);
			policy_mgr_add_radio_comb(pm_ctx, &radio);
		}
		if (mac0_ss_bw_info.mac_band_cap &
					WMI_HOST_WLAN_5G_CAPABILITY) {
			SET_RADIO(&radio, MODE_SMM, BIT(REG_BAND_5G), 0,
				  mac0_ss_bw_info.mac_tx_stream, 0);
			policy_mgr_add_radio_comb(pm_ctx, &radio);
			if (mac0_ss_bw_info.support_6ghz_band) {
				SET_RADIO(&radio, MODE_SMM, BIT(REG_BAND_6G),
					  0, mac0_ss_bw_info.mac_tx_stream, 0);
				policy_mgr_add_radio_comb(pm_ctx, &radio);
			}
		}
		return;
	}
	if ((mac0_ss_bw_info.mac_band_cap & WMI_HOST_WLAN_2G_CAPABILITY) &&
	    (mac1_ss_bw_info.mac_band_cap & WMI_HOST_WLAN_5G_CAPABILITY)) {
		SET_RADIO(&radio, MODE_DBS, BIT(REG_BAND_2G), BIT(REG_BAND_5G),
			  mac0_ss_bw_info.mac_tx_stream,
			  mac1_ss_bw_info.mac_tx_stream);
		policy_mgr_add_radio_comb(pm_ctx, &radio);
		if (mac1_ss_bw_info.support_6ghz_band) {
			SET_RADIO(&radio, MODE_DBS, BIT(REG_BAND_2G),
				  BIT(REG_BAND_6G),
				  mac0_ss_bw_info.mac_tx_stream,
				  mac1_ss_bw_info.mac_tx_stream);
			policy_mgr_add_radio_comb(pm_ctx, &radio);
		}
	}
	if ((mac0_ss_bw_info.mac_band_cap & WMI_HOST_WLAN_5G_CAPABILITY) &&
	    (mac1_ss_bw_info.mac_band_cap & WMI_HOST_WLAN_2G_CAPABILITY)) {
		SET_RADIO(&radio, MODE_DBS, BIT(REG_BAND_2G), BIT(REG_BAND_5G),
			  mac1_ss_bw_info.mac_tx_stream,
			  mac0_ss_bw_info.mac_tx_stream);
		policy_mgr_add_radio_comb(pm_ctx, &radio);
		if (mac0_ss_bw_info.support_6ghz_band) {
			SET_RADIO(&radio, MODE_DBS, BIT(REG_BAND_2G),
				  BIT(REG_BAND_6G),
				  mac1_ss_bw_info.mac_tx_stream,
				  mac0_ss_bw_info.mac_tx_stream);
			policy_mgr_add_radio_comb(pm_ctx, &radio);
		}
	}
	if ((mac0_ss_bw_info.mac_band_cap & WMI_HOST_WLAN_5G_CAPABILITY) &&
	    (mac1_ss_bw_info.mac_band_cap & WMI_HOST_WLAN_5G_CAPABILITY)) {
		if (mac0_ss_bw_info.support_6ghz_band) {
			SET_RADIO(&radio, MODE_SBS, BIT(REG_BAND_5G),
				  BIT(REG_BAND_6G),
				  mac1_ss_bw_info.mac_tx_stream,
				  mac0_ss_bw_info.mac_tx_stream);
			policy_mgr_add_radio_comb(pm_ctx, &radio);
		} else if (mac1_ss_bw_info.support_6ghz_band) {
			SET_RADIO(&radio, MODE_SBS, BIT(REG_BAND_5G),
				  BIT(REG_BAND_6G),
				  mac0_ss_bw_info.mac_tx_stream,
				  mac1_ss_bw_info.mac_tx_stream);
			policy_mgr_add_radio_comb(pm_ctx, &radio);
		}
	}
}

static void
policy_mgr_update_24Ghz_freq_info(struct policy_mgr_freq_range *mac_range,
				  struct wlan_psoc_host_mac_phy_caps *mac_cap)
{
	mac_range->low_2ghz_freq = QDF_MAX(mac_cap->reg_cap_ext.low_2ghz_chan,
					   wlan_reg_min_24ghz_chan_freq());
	mac_range->high_2ghz_freq = mac_cap->reg_cap_ext.high_2ghz_chan ?
				    QDF_MIN(mac_cap->reg_cap_ext.high_2ghz_chan,
					    wlan_reg_max_24ghz_chan_freq()) :
				    wlan_reg_max_24ghz_chan_freq();
}

static void
policy_mgr_update_5Ghz_freq_info(struct policy_mgr_freq_range *mac_range,
				  struct wlan_psoc_host_mac_phy_caps *mac_cap)
{
	qdf_freq_t max_5g_freq;

	max_5g_freq = wlan_reg_max_6ghz_chan_freq() ?
			wlan_reg_max_6ghz_chan_freq() :
			wlan_reg_max_5ghz_chan_freq();

	mac_range->low_5ghz_freq = QDF_MAX(mac_cap->reg_cap_ext.low_5ghz_chan,
					   wlan_reg_min_5ghz_chan_freq());
	mac_range->high_5ghz_freq = mac_cap->reg_cap_ext.high_5ghz_chan ?
				    QDF_MIN(mac_cap->reg_cap_ext.high_5ghz_chan,
					    max_5g_freq) :
				    max_5g_freq;
}

static void
policy_mgr_update_freq_info(struct policy_mgr_psoc_priv_obj *pm_ctx,
			    struct wlan_psoc_host_mac_phy_caps *mac_cap,
			    enum policy_mgr_mode mode,
			    uint32_t phy_id)
{
	struct policy_mgr_freq_range *mac_range;

	mac_range = &pm_ctx->hw_mode.freq_range_caps[mode][phy_id];
	if (mac_cap->supported_bands & WMI_HOST_WLAN_2G_CAPABILITY)
		policy_mgr_update_24Ghz_freq_info(mac_range, mac_cap);

	if (mac_cap->supported_bands & WMI_HOST_WLAN_5G_CAPABILITY)
		policy_mgr_update_5Ghz_freq_info(mac_range, mac_cap);
}

static QDF_STATUS
policy_mgr_modify_sbs_freq(struct policy_mgr_psoc_priv_obj *pm_ctx,
			   uint8_t phy_id)
{
	uint8_t shared_phy_id;
	struct policy_mgr_freq_range *sbs_mac_range, *shared_mac_range;
	struct policy_mgr_freq_range *non_shared_range;

	sbs_mac_range = &pm_ctx->hw_mode.freq_range_caps[MODE_SBS][phy_id];

	/*
	 * if SBS mac range has both 2.4 and 5 Ghz range, i e shared phy_id
	 * keep the range as it is in SBS
	 */
	if (sbs_mac_range->low_2ghz_freq && sbs_mac_range->low_5ghz_freq)
		return QDF_STATUS_SUCCESS;
	if (sbs_mac_range->low_2ghz_freq && !sbs_mac_range->low_5ghz_freq) {
		policy_mgr_err("Invalid DBS/SBS mode with only 2.4Ghz");
		policy_mgr_dump_freq_range_per_mac(sbs_mac_range, MODE_SBS);
		return QDF_STATUS_E_INVAL;
	}

	non_shared_range = sbs_mac_range;
	/*
	 * if SBS mac range has only 5Ghz then its the non shared phy, so
	 * modify the range as per the shared mac.
	 */
	shared_phy_id = phy_id ? 0 : 1;
	shared_mac_range =
		&pm_ctx->hw_mode.freq_range_caps[MODE_SBS][shared_phy_id];

	if (shared_mac_range->low_5ghz_freq > non_shared_range->low_5ghz_freq) {
		policy_mgr_debug("High 5Ghz shared");
		/*
		 * If the shared mac lower 5Ghz frequency is greater than
		 * non-shared mac lower 5Ghz frequency then the shared mac has
		 * HIGH 5Ghz shared with 2.4Ghz. So non-shared mac's 5Ghz high
		 * freq should be less than the shared mac's low 5Ghz freq.
		 */
		if (non_shared_range->high_5ghz_freq >=
		    shared_mac_range->low_5ghz_freq)
			non_shared_range->high_5ghz_freq =
				QDF_MAX(shared_mac_range->low_5ghz_freq - 10,
					non_shared_range->low_5ghz_freq);
	} else if (shared_mac_range->high_5ghz_freq <
		   non_shared_range->high_5ghz_freq) {
		policy_mgr_debug("LOW 5Ghz shared");
		/*
		 * If the shared mac high 5Ghz frequency is less than
		 * non-shared mac high 5Ghz frequency then the shared mac has
		 * LOW 5Ghz shared with 2.4Ghz So non-shared mac's 5Ghz low
		 * freq should be greater than the shared mac's high 5Ghz freq.
		 */
		if (shared_mac_range->high_5ghz_freq >=
		    non_shared_range->low_5ghz_freq)
			non_shared_range->low_5ghz_freq =
				QDF_MIN(shared_mac_range->high_5ghz_freq + 10,
					non_shared_range->high_5ghz_freq);
	} else {
		policy_mgr_info("Invalid SBS range with all 5Ghz shared");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

static qdf_freq_t
policy_mgr_get_highest_5ghz_freq_frm_range(struct policy_mgr_freq_range *range)
{
	uint8_t phy_id;
	qdf_freq_t highest_freq = 0;
	qdf_freq_t max_5g_freq = wlan_reg_max_6ghz_chan_freq() ?
			wlan_reg_max_6ghz_chan_freq() :
			wlan_reg_max_5ghz_chan_freq();

	for (phy_id = 0; phy_id < MAX_MAC; phy_id++) {
		if (range[phy_id].high_5ghz_freq > highest_freq)
			highest_freq = range[phy_id].high_5ghz_freq;
	}

	return highest_freq ? highest_freq : max_5g_freq;
}

static qdf_freq_t
policy_mgr_get_lowest_5ghz_freq_frm_range(struct policy_mgr_freq_range *range)
{
	uint8_t phy_id;
	qdf_freq_t lowest_freq = 0;

	for (phy_id = 0; phy_id < MAX_MAC; phy_id++) {
		if ((!lowest_freq && range[phy_id].low_5ghz_freq) ||
		    (range[phy_id].low_5ghz_freq < lowest_freq))
			lowest_freq = range[phy_id].low_5ghz_freq;
	}

	return lowest_freq ? lowest_freq : wlan_reg_min_5ghz_chan_freq();
}

static void
policy_mgr_fill_lower_share_sbs_freq(struct policy_mgr_psoc_priv_obj *pm_ctx,
				     uint16_t sbs_range_sep,
				     struct policy_mgr_freq_range *ref_freq)
{
	struct policy_mgr_freq_range *lower_sbs_freq_range;
	uint8_t phy_id;

	lower_sbs_freq_range =
		pm_ctx->hw_mode.freq_range_caps[MODE_SBS_LOWER_SHARE];

	for (phy_id = 0; phy_id < MAX_MAC; phy_id++) {
		lower_sbs_freq_range[phy_id].low_2ghz_freq =
						ref_freq[phy_id].low_2ghz_freq;
		lower_sbs_freq_range[phy_id].high_2ghz_freq =
						ref_freq[phy_id].high_2ghz_freq;

		/* update for shared mac */
		if (lower_sbs_freq_range[phy_id].low_2ghz_freq) {
			lower_sbs_freq_range[phy_id].low_5ghz_freq =
			    policy_mgr_get_lowest_5ghz_freq_frm_range(ref_freq);
			lower_sbs_freq_range[phy_id].high_5ghz_freq =
				sbs_range_sep;
		} else {
			lower_sbs_freq_range[phy_id].low_5ghz_freq =
				sbs_range_sep + 10;
			lower_sbs_freq_range[phy_id].high_5ghz_freq =
			   policy_mgr_get_highest_5ghz_freq_frm_range(ref_freq);
		}
	}
}

static void
policy_mgr_fill_upper_share_sbs_freq(struct policy_mgr_psoc_priv_obj *pm_ctx,
				     uint16_t sbs_range_sep,
				     struct policy_mgr_freq_range *ref_freq)
{
	struct policy_mgr_freq_range *upper_sbs_freq_range;
	uint8_t phy_id;

	upper_sbs_freq_range =
		pm_ctx->hw_mode.freq_range_caps[MODE_SBS_UPPER_SHARE];

	for (phy_id = 0; phy_id < MAX_MAC; phy_id++) {
		upper_sbs_freq_range[phy_id].low_2ghz_freq =
						ref_freq[phy_id].low_2ghz_freq;
		upper_sbs_freq_range[phy_id].high_2ghz_freq =
						ref_freq[phy_id].high_2ghz_freq;

		/* update for shared mac */
		if (upper_sbs_freq_range[phy_id].low_2ghz_freq) {
			upper_sbs_freq_range[phy_id].low_5ghz_freq =
				sbs_range_sep + 10;
			upper_sbs_freq_range[phy_id].high_5ghz_freq =
			   policy_mgr_get_highest_5ghz_freq_frm_range(ref_freq);
		} else {
			upper_sbs_freq_range[phy_id].low_5ghz_freq =
			    policy_mgr_get_lowest_5ghz_freq_frm_range(ref_freq);
			upper_sbs_freq_range[phy_id].high_5ghz_freq =
				sbs_range_sep;
		}
	}
}

static bool
policy_mgr_both_phy_range_updated(struct policy_mgr_psoc_priv_obj *pm_ctx,
				  enum policy_mgr_mode hwmode)
{
	struct policy_mgr_freq_range *mac_range;
	uint8_t phy_id;

	for (phy_id = 0; phy_id < MAX_MAC; phy_id++) {
		mac_range =
			&pm_ctx->hw_mode.freq_range_caps[hwmode][phy_id];
		/* modify SBS/DBS range only when both phy for DBS are filled */
		if (!mac_range->low_2ghz_freq && !mac_range->low_5ghz_freq)
			return false;
	}

	return true;
}

static void
policy_mgr_update_sbs_freq_info(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	uint16_t sbs_range_sep;
	struct policy_mgr_freq_range *mac_range;
	uint8_t phy_id;
	QDF_STATUS status;

	mac_range = pm_ctx->hw_mode.freq_range_caps[MODE_SBS];

	/*
	 * If sbs_lower_band_end_freq has a value Z, then the frequency range
	 * will be split using that value.
	 */
	sbs_range_sep = pm_ctx->hw_mode.sbs_lower_band_end_freq;
	if (sbs_range_sep) {
		policy_mgr_fill_upper_share_sbs_freq(pm_ctx, sbs_range_sep,
						     mac_range);
		policy_mgr_fill_lower_share_sbs_freq(pm_ctx, sbs_range_sep,
						     mac_range);
		/* Reset the SBS range */
		qdf_mem_zero(mac_range, sizeof(*mac_range) * MAX_MAC);
		return;
	}

	/*
	 * If sbs_lower_band_end_freq is not set that means FW will send one
	 * shared mac range and one non-shared mac range. so update that freq.
	 */
	for (phy_id = 0; phy_id < MAX_MAC; phy_id++) {
		status = policy_mgr_modify_sbs_freq(pm_ctx, phy_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			/* Reset the SBS range */
			qdf_mem_zero(mac_range, sizeof(*mac_range) * MAX_MAC);
			break;
		}
	}
}

static void
policy_mgr_update_dbs_freq_info(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	struct policy_mgr_freq_range *mac_range;
	uint8_t phy_id;

	mac_range = pm_ctx->hw_mode.freq_range_caps[MODE_DBS];
	/* Reset 5Ghz range for shared mac for DBS */
	for (phy_id = 0; phy_id < MAX_MAC; phy_id++) {
		if (mac_range[phy_id].low_2ghz_freq &&
		    mac_range[phy_id].low_5ghz_freq) {
			mac_range[phy_id].low_5ghz_freq = 0;
			mac_range[phy_id].high_5ghz_freq = 0;
		}
	}
}

static void
policy_mgr_update_mac_freq_info(struct wlan_objmgr_psoc *psoc,
				struct policy_mgr_psoc_priv_obj *pm_ctx,
				enum wmi_hw_mode_config_type hw_config_type,
				uint32_t phy_id,
				struct wlan_psoc_host_mac_phy_caps *mac_cap)
{
	if (phy_id >= MAX_MAC) {
		policy_mgr_err("mac more than two not supported: %d",
			       phy_id);
		return;
	}

	policy_mgr_debug("hw_mode_cfg: %d mac: %d band: 0x%x, SBS cutoff freq %d, 2Ghz: %d -> %d 5Ghz: %d -> %d",
			 hw_config_type, phy_id, mac_cap->supported_bands,
			 pm_ctx->hw_mode.sbs_lower_band_end_freq,
			 mac_cap->reg_cap_ext.low_2ghz_chan,
			 mac_cap->reg_cap_ext.high_2ghz_chan,
			 mac_cap->reg_cap_ext.low_5ghz_chan,
			 mac_cap->reg_cap_ext.high_5ghz_chan);

	switch (hw_config_type) {
	case WMI_HW_MODE_SINGLE:
		if (phy_id) {
			policy_mgr_debug("MAC Phy 1 is not supported");
			break;
		}
		policy_mgr_update_freq_info(pm_ctx, mac_cap, MODE_SMM, phy_id);
		break;

	case WMI_HW_MODE_DBS:
	case WMI_HW_MODE_DBS_2G_5G:
		if (!policy_mgr_both_phy_range_updated(pm_ctx, MODE_DBS))
			policy_mgr_update_freq_info(pm_ctx, mac_cap,
						    MODE_DBS, phy_id);
		break;
	case WMI_HW_MODE_DBS_SBS:
	case WMI_HW_MODE_DBS_OR_SBS:
		policy_mgr_update_freq_info(pm_ctx, mac_cap, MODE_DBS, phy_id);
		/*
		 * fill SBS only if freq is provided by FW or
		 * pm_ctx->hw_mode.sbs_lower_band_end_freq is set
		 */
		if (pm_ctx->hw_mode.sbs_lower_band_end_freq ||
		    mac_cap->reg_cap_ext.low_5ghz_chan ||
		    mac_cap->reg_cap_ext.low_2ghz_chan)
			policy_mgr_update_freq_info(pm_ctx, mac_cap, MODE_SBS,
						    phy_id);

		/* Modify the DBS list once both phy info are filled */
		if (policy_mgr_both_phy_range_updated(pm_ctx, MODE_DBS))
			policy_mgr_update_dbs_freq_info(pm_ctx);
		/* Modify the SBS list once both phy info are filled */
		if (policy_mgr_both_phy_range_updated(pm_ctx, MODE_SBS))
			policy_mgr_update_sbs_freq_info(pm_ctx);
		break;
	case WMI_HW_MODE_2G_PHYB:
		if (phy_id)
			policy_mgr_update_freq_info(pm_ctx, mac_cap, MODE_SMM,
						    phy_id);
		break;
	case WMI_HW_MODE_SBS:
	case WMI_HW_MODE_SBS_PASSIVE:
		policy_mgr_update_freq_info(pm_ctx, mac_cap, MODE_SBS, phy_id);
		/* Modify the SBS Upper Lower list once both phy are filled */
		if (policy_mgr_both_phy_range_updated(pm_ctx, MODE_SBS))
			policy_mgr_update_sbs_freq_info(pm_ctx);

		break;
	case WMI_HW_MODE_EMLSR:
		policy_mgr_update_freq_info(pm_ctx, mac_cap, MODE_EMLSR,
					    phy_id);
		break;
	case WMI_HW_MODE_AUX_EMLSR_SINGLE:
		if (phy_id) {
			policy_mgr_debug("MAC Phy 1 is not supported");
			break;
		}
		policy_mgr_update_freq_info(pm_ctx, mac_cap, MODE_EMLSR_SINGLE,
					    phy_id);
		break;
	case WMI_HW_MODE_AUX_EMLSR_SPLIT:
		policy_mgr_update_freq_info(pm_ctx, mac_cap, MODE_EMLSR_SPLIT,
					    phy_id);
		break;
	default:
		policy_mgr_err("HW mode not defined %d",
			       hw_config_type);
		break;
	}
}

void
policy_mgr_dump_curr_freq_range(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	uint32_t i;
	struct policy_mgr_freq_range *freq_range;

	freq_range = pm_ctx->hw_mode.cur_mac_freq_range;
	for (i = 0; i < MAX_MAC; i++)
		if (freq_range[i].low_2ghz_freq || freq_range[i].low_5ghz_freq)
			policymgr_nofl_debug("PLCY_MGR_FREQ_RANGE_CUR: mac %d: 2Ghz: %d -> %d, 5Ghz: %d -> %d",
					     i, freq_range[i].low_2ghz_freq,
					     freq_range[i].high_2ghz_freq,
					     freq_range[i].low_5ghz_freq,
					     freq_range[i].high_5ghz_freq);
}

static const char *policy_mgr_hw_mode_to_str(enum policy_mgr_mode hw_mode)
{
	if (hw_mode >= MODE_HW_MAX)
		return "Unknown";

	switch (hw_mode) {
	CASE_RETURN_STRING(MODE_SMM);
	CASE_RETURN_STRING(MODE_DBS);
	CASE_RETURN_STRING(MODE_SBS);
	CASE_RETURN_STRING(MODE_SBS_UPPER_SHARE);
	CASE_RETURN_STRING(MODE_SBS_LOWER_SHARE);
	CASE_RETURN_STRING(MODE_EMLSR);
	CASE_RETURN_STRING(MODE_EMLSR_SINGLE);
	CASE_RETURN_STRING(MODE_EMLSR_SPLIT);
	default:
		return "Unknown";
	}
}

void
policy_mgr_dump_freq_range_per_mac(struct policy_mgr_freq_range *freq_range,
				   enum policy_mgr_mode hw_mode)
{
	uint32_t i;

	for (i = 0; i < MAX_MAC; i++)
		if (freq_range[i].low_2ghz_freq || freq_range[i].low_5ghz_freq)
			policymgr_nofl_debug("PLCY_MGR_FREQ_RANGE: %s(%d): mac %d: 2Ghz: %d -> %d, 5Ghz: %d -> %d",
					     policy_mgr_hw_mode_to_str(hw_mode),
					     hw_mode, i,
					     freq_range[i].low_2ghz_freq,
					     freq_range[i].high_2ghz_freq,
					     freq_range[i].low_5ghz_freq,
					     freq_range[i].high_5ghz_freq);
}

static void
policy_mgr_dump_hw_modes_freq_range(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	uint32_t i;
	struct policy_mgr_freq_range *freq_range;

	for (i = MODE_SMM; i < MODE_HW_MAX; i++) {
		freq_range = pm_ctx->hw_mode.freq_range_caps[i];
		policy_mgr_dump_freq_range_per_mac(freq_range, i);
	}
}

void policy_mgr_dump_sbs_freq_range(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	uint32_t i;
	struct policy_mgr_freq_range *freq_range;

	for (i = MODE_SMM; i < MODE_HW_MAX; i++) {
		if ((i == MODE_SBS) ||
		    (pm_ctx->hw_mode.sbs_lower_band_end_freq &&
		    (i == MODE_SBS_LOWER_SHARE || i == MODE_SBS_UPPER_SHARE))) {
			freq_range = pm_ctx->hw_mode.freq_range_caps[i];
			policy_mgr_dump_freq_range_per_mac(freq_range, i);
		}
	}
}

static bool
policy_mgr_sbs_range_present(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	if (policy_mgr_both_phy_range_updated(pm_ctx, MODE_SBS) ||
	    (pm_ctx->hw_mode.sbs_lower_band_end_freq &&
	     policy_mgr_both_phy_range_updated(pm_ctx, MODE_SBS_LOWER_SHARE) &&
	     policy_mgr_both_phy_range_updated(pm_ctx, MODE_SBS_UPPER_SHARE)))
		return true;

	return false;
}

void
policy_mgr_dump_freq_range(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	policy_mgr_dump_hw_modes_freq_range(pm_ctx);
	policy_mgr_dump_curr_freq_range(pm_ctx);
}

static void
policy_mgr_update_sbs_lowr_band_end_frq(struct policy_mgr_psoc_priv_obj *pm_ctx,
					struct tgt_info *info)
{
	if (wlan_reg_is_5ghz_ch_freq(info->sbs_lower_band_end_freq) ||
	    wlan_reg_is_6ghz_chan_freq(info->sbs_lower_band_end_freq))
		pm_ctx->hw_mode.sbs_lower_band_end_freq =
						info->sbs_lower_band_end_freq;
}

QDF_STATUS policy_mgr_update_hw_mode_list(struct wlan_objmgr_psoc *psoc,
					  struct target_psoc_info *tgt_hdl)
{
	struct wlan_psoc_host_mac_phy_caps *tmp;
	struct wlan_psoc_host_mac_phy_caps_ext2 *cap;
	uint32_t i, j = 0;
	enum wmi_hw_mode_config_type hw_config_type;
	uint32_t dbs_mode, sbs_mode;
	uint64_t emlsr_mode;
	struct policy_mgr_mac_ss_bw_info mac0_ss_bw_info = {0};
	struct policy_mgr_mac_ss_bw_info mac1_ss_bw_info = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct tgt_info *info;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	info = &tgt_hdl->info;
	if (!info->service_ext_param.num_hw_modes) {
		policy_mgr_err("Number of HW modes: %d",
			       info->service_ext_param.num_hw_modes);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * This list was updated as part of service ready event. Re-populate
	 * HW mode list from the device capabilities.
	 */
	if (pm_ctx->hw_mode.hw_mode_list) {
		qdf_mem_free(pm_ctx->hw_mode.hw_mode_list);
		pm_ctx->hw_mode.hw_mode_list = NULL;
		policy_mgr_debug("DBS list is freed");
	}

	/* Reset old freq ranges */
	qdf_mem_zero(pm_ctx->hw_mode.freq_range_caps,
		     sizeof(pm_ctx->hw_mode.freq_range_caps));
	qdf_mem_zero(pm_ctx->hw_mode.cur_mac_freq_range,
		     sizeof(pm_ctx->hw_mode.cur_mac_freq_range));
	pm_ctx->num_dbs_hw_modes = info->service_ext_param.num_hw_modes;
	pm_ctx->hw_mode.hw_mode_list =
		qdf_mem_malloc(sizeof(*pm_ctx->hw_mode.hw_mode_list) *
		pm_ctx->num_dbs_hw_modes);
	if (!pm_ctx->hw_mode.hw_mode_list) {
		pm_ctx->num_dbs_hw_modes = 0;
		return QDF_STATUS_E_NOMEM;
	}
	pm_ctx->radio_comb_num = 0;
	qdf_mem_zero(pm_ctx->radio_combinations,
		     sizeof(pm_ctx->radio_combinations));

	policy_mgr_debug("Updated HW mode list: Num modes:%d",
		pm_ctx->num_dbs_hw_modes);

	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		/* Update for MAC0 */
		tmp = &info->mac_phy_cap[j++];
		policy_mgr_get_hw_mode_params(tmp, &mac0_ss_bw_info);
		dbs_mode = HW_MODE_DBS_NONE;
		sbs_mode = HW_MODE_SBS_NONE;
		emlsr_mode = HW_MODE_EMLSR_NONE;
		mac1_ss_bw_info.mac_tx_stream = 0;
		mac1_ss_bw_info.mac_rx_stream = 0;
		mac1_ss_bw_info.mac_bw = 0;

		hw_config_type = tmp->hw_mode_config_type;
		if (WMI_BECAP_PHY_GET_HW_MODE_CFG(hw_config_type) ==
		    WMI_HW_MODE_EMLSR)
			hw_config_type = WMI_HW_MODE_EMLSR;
		else if (WMI_BECAP_PHY_GET_HW_MODE_CFG(hw_config_type) ==
			 WMI_HW_MODE_AUX_EMLSR_SINGLE)
			hw_config_type = WMI_HW_MODE_AUX_EMLSR_SINGLE;
		else if (WMI_BECAP_PHY_GET_HW_MODE_CFG(hw_config_type) ==
			 WMI_HW_MODE_AUX_EMLSR_SPLIT)
			hw_config_type = WMI_HW_MODE_AUX_EMLSR_SPLIT;

		policy_mgr_update_mac_freq_info(psoc, pm_ctx,
						hw_config_type,
						tmp->phy_id, tmp);

		/* SBS and DBS have dual MAC. Upto 2 MACs are considered. */
		if ((hw_config_type == WMI_HW_MODE_DBS) ||
		    (hw_config_type == WMI_HW_MODE_SBS_PASSIVE) ||
		    (hw_config_type == WMI_HW_MODE_SBS) ||
		    (hw_config_type == WMI_HW_MODE_DBS_OR_SBS)) {
			/* Update for MAC1 */
			tmp = &info->mac_phy_cap[j++];
			policy_mgr_get_hw_mode_params(tmp, &mac1_ss_bw_info);
			policy_mgr_update_mac_freq_info(psoc, pm_ctx,
							hw_config_type,
							tmp->phy_id, tmp);
			if (hw_config_type == WMI_HW_MODE_DBS ||
			    hw_config_type == WMI_HW_MODE_DBS_OR_SBS)
				dbs_mode = HW_MODE_DBS;
			if (policy_mgr_sbs_range_present(pm_ctx) &&
			    ((hw_config_type == WMI_HW_MODE_SBS_PASSIVE) ||
			    (hw_config_type == WMI_HW_MODE_SBS) ||
			    (hw_config_type == WMI_HW_MODE_DBS_OR_SBS)))
				sbs_mode = HW_MODE_SBS;
		} else if (hw_config_type == WMI_HW_MODE_EMLSR ||
			hw_config_type == WMI_HW_MODE_AUX_EMLSR_SPLIT) {
			/* eMLSR mode */
			tmp = &info->mac_phy_cap[j++];
			cap = &info->mac_phy_caps_ext2[i];
			wlan_mlme_set_eml_params(psoc, cap);
			policy_mgr_get_hw_mode_params(tmp, &mac1_ss_bw_info);
			policy_mgr_update_mac_freq_info(psoc, pm_ctx,
							hw_config_type,
							tmp->phy_id, tmp);
			emlsr_mode = HW_MODE_EMLSR;
		} else if (hw_config_type == WMI_HW_MODE_AUX_EMLSR_SINGLE) {
			/* eMLSR mode */
			cap = &info->mac_phy_caps_ext2[i];
			wlan_mlme_set_eml_params(psoc, cap);
			policy_mgr_get_hw_mode_params(tmp, &mac1_ss_bw_info);
			emlsr_mode = HW_MODE_EMLSR;
		}

		/* Updating HW mode list */
		policy_mgr_set_hw_mode_params(psoc, mac0_ss_bw_info,
			mac1_ss_bw_info, i, tmp->hw_mode_id, dbs_mode,
			sbs_mode, emlsr_mode);
		/* Update radio combination info */
		policy_mgr_update_radio_combination_matrix(
			psoc, mac0_ss_bw_info, mac1_ss_bw_info,
			dbs_mode, sbs_mode);
	}

	/*
	 * Initializing Current frequency with SMM frequency.
	 */
	policy_mgr_fill_curr_mac_freq_by_hwmode(pm_ctx, MODE_SMM);
	policy_mgr_dump_freq_range(pm_ctx);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_update_sbs_freq(struct wlan_objmgr_psoc *psoc,
				      struct target_psoc_info *tgt_hdl)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct tgt_info *info;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	info = &tgt_hdl->info;
	policy_mgr_debug("sbs_lower_band_end_freq %d",
			 info->sbs_lower_band_end_freq);
	policy_mgr_update_sbs_lowr_band_end_frq(pm_ctx, info);

	policy_mgr_update_hw_mode_list(psoc, tgt_hdl);

	return QDF_STATUS_SUCCESS;
}

qdf_freq_t policy_mgr_get_sbs_cut_off_freq(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_freq_range *first_mac_range, *second_mac_range;
	qdf_freq_t sbs_cut_off_freq = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}

	if (!policy_mgr_is_hw_sbs_capable(psoc))
		return 0;

	if (pm_ctx->hw_mode.sbs_lower_band_end_freq)
		return pm_ctx->hw_mode.sbs_lower_band_end_freq;
	/*
	 * if cutoff freq is not available from FW (i.e SBS is not dynamic)
	 * get it from SBS freq range
	 */
	first_mac_range = &pm_ctx->hw_mode.freq_range_caps[MODE_SBS][0];

	second_mac_range =
		&pm_ctx->hw_mode.freq_range_caps[MODE_SBS][1];

	/*
	 * SBS range is low 5Ghz shared with 2.4Ghz: The low_5Ghz of shared
	 * mac will be starting of 5Ghz and low_5Ghz of non-shared mac will be
	 * the cutoff freq
	 *
	 * SBS range is high 5Ghz shared with 2.4Ghz: The low_5Ghz of shared
	 * mac will be cutoff freq and low_5Ghz of non-shared mac will be
	 * the starting of 5Ghz
	 *
	 * so, maximum of low_5Ghz will be cutoff freq
	 */
	sbs_cut_off_freq = QDF_MAX(second_mac_range->low_5ghz_freq,
				   first_mac_range->low_5ghz_freq) - 1;
	policy_mgr_debug("sbs cutoff freq %d", sbs_cut_off_freq);

	return sbs_cut_off_freq;
}

void
policy_mgr_init_5g_low_high_cut_freq(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	qdf_freq_t cut_freq;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	cut_freq = policy_mgr_get_sbs_cut_off_freq(psoc);
	/* for dbs rd, cut_freq will be 0 from API
	 * policy_mgr_get_sbs_cut_off_freq. Set a valid cut_freq
	 * to WLAN_REG_MAX_5GHZ_CHAN_FREQ.
	 */
	if (!cut_freq)
		cut_freq = WLAN_REG_MAX_5GHZ_CHAN_FREQ;

	pm_ctx->low_high_cut_off_freq = cut_freq;

	policy_mgr_debug("5g low high cutoff freq %d", cut_freq);
}

qdf_freq_t
policy_mgr_get_5g_low_high_cut_freq(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return WLAN_REG_MAX_5GHZ_CHAN_FREQ;
	}

	return pm_ctx->low_high_cut_off_freq;
}

static bool
policy_mgr_2_freq_same_mac_id_in_freq_range(
				struct policy_mgr_psoc_priv_obj *pm_ctx,
				struct policy_mgr_freq_range *freq_range,
				qdf_freq_t freq_1, qdf_freq_t freq_2,
				uint8_t *mac_id)
{
	uint8_t i;

	for (i = 0; i < MAX_MAC; i++) {
		if (IS_FREQ_ON_MAC_ID(freq_range, freq_1, i) &&
		    IS_FREQ_ON_MAC_ID(freq_range, freq_2, i)) {
			if (mac_id)
				*mac_id = i;
			return true;
		}
	}

	return false;
}

static bool
policy_mgr_2_freq_same_mac_in_freq_range(
				struct policy_mgr_psoc_priv_obj *pm_ctx,
				struct policy_mgr_freq_range *freq_range,
				qdf_freq_t freq_1, qdf_freq_t freq_2)
{
	return policy_mgr_2_freq_same_mac_id_in_freq_range(
			pm_ctx, freq_range,
			freq_1, freq_2, NULL);
}

bool policy_mgr_can_2ghz_share_low_high_5ghz_sbs(
			struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	if (pm_ctx->hw_mode.sbs_lower_band_end_freq)
		return true;

	return false;
}

bool
policy_mgr_sbs_24_shared_with_high_5(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	qdf_freq_t sbs_cut_off_freq;
	struct policy_mgr_freq_range freq_range;
	uint8_t i = 0;

	if (policy_mgr_can_2ghz_share_low_high_5ghz_sbs(pm_ctx))
		return true;

	sbs_cut_off_freq = policy_mgr_get_sbs_cut_off_freq(pm_ctx->psoc);
	if (!sbs_cut_off_freq) {
		policy_mgr_err("Invalid cut off freq");
		return false;
	}

	for (i = 0; i < MAX_MAC; i++) {
		freq_range = pm_ctx->hw_mode.freq_range_caps[MODE_SBS][i];
		/*
		 * if 5 GHZ start freq of this mac is greater than cutoff
		 * return true
		 */
		if (freq_range.low_2ghz_freq && freq_range.low_5ghz_freq) {
			if  (sbs_cut_off_freq < freq_range.low_5ghz_freq)
				return true;
		}
	}

	return false;
}

bool
policy_mgr_sbs_24_shared_with_low_5(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	qdf_freq_t sbs_cut_off_freq;
	struct policy_mgr_freq_range freq_range;
	uint8_t i = 0;

	if (policy_mgr_can_2ghz_share_low_high_5ghz_sbs(pm_ctx))
		return true;

	sbs_cut_off_freq = policy_mgr_get_sbs_cut_off_freq(pm_ctx->psoc);
	if (!sbs_cut_off_freq) {
		policy_mgr_err("Invalid cut off freq");
		return false;
	}

	for (i = 0; i < MAX_MAC; i++) {
		freq_range = pm_ctx->hw_mode.freq_range_caps[MODE_SBS][i];
		if (freq_range.low_2ghz_freq && freq_range.high_5ghz_freq) {
			/*
			 * if 5 GHZ end freq of this mac is less than cutoff
			 * return true
			 */
			if  (sbs_cut_off_freq > freq_range.high_5ghz_freq)
				return true;
		}
	}

	return false;
}

void policy_mgr_init_rd_type(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return;

	pm_ctx->rd_type = pm_rd_none;

	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return;

	pm_ctx->rd_type = pm_rd_dbs;

	if (!policy_mgr_is_hw_sbs_capable(psoc))
		return;

	if (policy_mgr_sbs_24_shared_with_low_5(pm_ctx))
		pm_ctx->rd_type = pm_rd_sbs_low_share;

	if (policy_mgr_sbs_24_shared_with_high_5(pm_ctx)) {
		if (pm_ctx->rd_type == pm_rd_sbs_low_share)
			pm_ctx->rd_type = pm_rd_sbs_switchable;
		else
			pm_ctx->rd_type = pm_rd_sbs_upper_share;
	}

	policy_mgr_debug("rd %d", pm_ctx->rd_type);
}

enum pm_rd_type policy_mgr_get_rd_type(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return pm_rd_none;

	return pm_ctx->rd_type;
}

bool
policy_mgr_2_freq_same_mac_in_dbs(struct policy_mgr_psoc_priv_obj *pm_ctx,
				  qdf_freq_t freq_1, qdf_freq_t freq_2)
{
	struct policy_mgr_freq_range *freq_range;

	/* Return true if non DBS capable HW */
	if (!policy_mgr_is_hw_dbs_capable(pm_ctx->psoc))
		return true;

	freq_range = pm_ctx->hw_mode.freq_range_caps[MODE_DBS];
	return policy_mgr_2_freq_same_mac_in_freq_range(pm_ctx, freq_range,
							 freq_1, freq_2);
}

bool
policy_mgr_2_freq_same_mac_in_sbs(struct policy_mgr_psoc_priv_obj *pm_ctx,
				  qdf_freq_t freq_1, qdf_freq_t freq_2)
{
	struct policy_mgr_freq_range *sbs_low_share;
	struct policy_mgr_freq_range *sbs_uppr_share;
	struct policy_mgr_freq_range *sbs_range;

	/* Return true if non SBS capable HW */
	if (!policy_mgr_is_hw_sbs_capable(pm_ctx->psoc))
		return true;

	if (policy_mgr_can_2ghz_share_low_high_5ghz_sbs(pm_ctx)) {
		sbs_uppr_share =
			pm_ctx->hw_mode.freq_range_caps[MODE_SBS_UPPER_SHARE];
		sbs_low_share =
			pm_ctx->hw_mode.freq_range_caps[MODE_SBS_LOWER_SHARE];
		if (policy_mgr_2_freq_same_mac_in_freq_range(pm_ctx,
							     sbs_low_share,
							     freq_1, freq_2) ||
		    policy_mgr_2_freq_same_mac_in_freq_range(pm_ctx,
							     sbs_uppr_share,
							     freq_1, freq_2))
				return true;

		return false;
	}

	sbs_range = pm_ctx->hw_mode.freq_range_caps[MODE_SBS];

	return policy_mgr_2_freq_same_mac_in_freq_range(pm_ctx, sbs_range,
							freq_1, freq_2);
}

static bool
policy_mgr_is_cur_freq_range_sbs(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_freq_range *freq_range;
	uint8_t i;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return false;

	/* Check if any of the mac is shared */
	for (i = 0 ; i < MAX_MAC; i++) {
		freq_range = &pm_ctx->hw_mode.cur_mac_freq_range[i];
		if (freq_range->low_2ghz_freq && freq_range->low_5ghz_freq)
			return true;
	}

	return false;
}

bool policy_mgr_2_freq_always_on_same_mac(struct wlan_objmgr_psoc *psoc,
					  qdf_freq_t freq_1, qdf_freq_t freq_2)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool is_dbs_mode_same_mac = true;
	bool is_sbs_mode_same_mac = true;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return false;

	is_dbs_mode_same_mac =
		policy_mgr_2_freq_same_mac_in_dbs(pm_ctx, freq_1, freq_2);

	/* if DBS mode leading to same mac, check for SBS mode */
	if (is_dbs_mode_same_mac)
		is_sbs_mode_same_mac =
			policy_mgr_2_freq_same_mac_in_sbs(pm_ctx, freq_1,
							  freq_2);

	policy_mgr_rl_debug("freq1 %d freq2 %d: Same mac:: DBS:%d SBS:%d",
			    freq_1, freq_2, is_dbs_mode_same_mac,
			    is_sbs_mode_same_mac);
	/*
	 * if in SBS and DBS mode, both is leading to freqs on same mac,
	 * return true else return false.
	 */
	if (is_dbs_mode_same_mac && is_sbs_mode_same_mac)
		return true;

	return false;
}

bool policy_mgr_are_2_freq_on_same_mac(struct wlan_objmgr_psoc *psoc,
				       qdf_freq_t freq_1,
				       qdf_freq_t  freq_2)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_freq_range *freq_range;
	struct policy_mgr_hw_mode_params hw_mode;
	QDF_STATUS status;
	bool cur_range_sbs = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return false;

	/* if HW is not DBS return true*/
	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return true;

	/* HW is DBS/SBS capable */
	status = policy_mgr_get_current_hw_mode(psoc, &hw_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("policy_mgr_get_current_hw_mode failed");
		return false;
	}

	if (hw_mode.dbs_cap || hw_mode.sbs_cap)
		cur_range_sbs = policy_mgr_is_cur_freq_range_sbs(psoc);

	freq_range = pm_ctx->hw_mode.cur_mac_freq_range;
	/* current HW is DBS OR SBS check current DBS/SBS freq range */
	if (hw_mode.dbs_cap || hw_mode.sbs_cap) {
		policy_mgr_rl_debug("freq1 %d freq2 %d dbs_cap %d sbs_cap %d, cur range is sbs %d",
				    freq_1, freq_2, hw_mode.dbs_cap,
				    hw_mode.sbs_cap, cur_range_sbs);
		return policy_mgr_2_freq_same_mac_in_freq_range(pm_ctx,
								freq_range,
								freq_1, freq_2);
	}

	/*
	 * If current HW mode is not DBS/SBS, check if in all supported mode
	 * it they will be on same mac
	 */
	return policy_mgr_2_freq_always_on_same_mac(psoc, freq_1, freq_2);
}

static bool
policy_mgr_3_freq_same_mac_in_freq_range(
				struct policy_mgr_psoc_priv_obj *pm_ctx,
				struct policy_mgr_freq_range *freq_range,
				qdf_freq_t freq_1, qdf_freq_t freq_2,
				qdf_freq_t freq_3)
{
	uint8_t i;

	for (i = 0 ; i < MAX_MAC; i++) {
		if (IS_FREQ_ON_MAC_ID(freq_range, freq_1, i) &&
		    IS_FREQ_ON_MAC_ID(freq_range, freq_2, i) &&
		    IS_FREQ_ON_MAC_ID(freq_range, freq_3, i))
			return true;
	}

	return false;
}

static bool
policy_mgr_3_freq_same_mac_in_sbs(struct policy_mgr_psoc_priv_obj *pm_ctx,
				  qdf_freq_t freq_1, qdf_freq_t freq_2,
				  qdf_freq_t freq_3)
{
	struct policy_mgr_freq_range *sbs_low_share;
	struct policy_mgr_freq_range *sbs_uppr_share;
	struct policy_mgr_freq_range *sbs_range;

	/* Return true if non SBS capable HW */
	if (!policy_mgr_is_hw_sbs_capable(pm_ctx->psoc))
		return true;

	if (pm_ctx->hw_mode.sbs_lower_band_end_freq) {
		sbs_uppr_share =
			pm_ctx->hw_mode.freq_range_caps[MODE_SBS_UPPER_SHARE];
		sbs_low_share =
			pm_ctx->hw_mode.freq_range_caps[MODE_SBS_LOWER_SHARE];
		if (policy_mgr_3_freq_same_mac_in_freq_range(pm_ctx,
							     sbs_low_share,
							     freq_1, freq_2,
							     freq_3) ||
		    policy_mgr_3_freq_same_mac_in_freq_range(pm_ctx,
							     sbs_uppr_share,
							     freq_1, freq_2,
							     freq_3))
			return true;

		return false;
	}

	sbs_range = pm_ctx->hw_mode.freq_range_caps[MODE_SBS];
	return policy_mgr_3_freq_same_mac_in_freq_range(pm_ctx, sbs_range,
							freq_1, freq_2, freq_3);
}

bool
policy_mgr_3_freq_always_on_same_mac(struct wlan_objmgr_psoc *psoc,
				     qdf_freq_t freq_1, qdf_freq_t freq_2,
				     qdf_freq_t freq_3)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_freq_range *freq_range;
	bool is_dbs_mode_same_mac = true;
	bool is_sbs_mode_same_mac = true;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return false;

	/* if HW is not DBS return true*/
	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return true;

	/* Check for DBS mode first */
	freq_range = pm_ctx->hw_mode.freq_range_caps[MODE_DBS];
	is_dbs_mode_same_mac =
		policy_mgr_3_freq_same_mac_in_freq_range(pm_ctx, freq_range,
							 freq_1, freq_2,
							 freq_3);

	/* if DBS mode leading to same mac, check for SBS mode */
	if (is_dbs_mode_same_mac)
		is_sbs_mode_same_mac =
			policy_mgr_3_freq_same_mac_in_sbs(pm_ctx, freq_1,
							  freq_2, freq_3);

	policy_mgr_rl_debug("freq1 %d freq2 %d freq3 %d: Same mac:: DBS:%d SBS:%d",
			    freq_1, freq_2, freq_3, is_dbs_mode_same_mac,
			    is_sbs_mode_same_mac);
	/*
	 * if in SBS and DBS mode, both is leading to freqs on same mac,
	 * return true else return false.
	 */
	if (is_dbs_mode_same_mac && is_sbs_mode_same_mac)
		return true;

	return false;
}

bool
policy_mgr_are_3_freq_on_same_mac(struct wlan_objmgr_psoc *psoc,
				  qdf_freq_t freq_1, qdf_freq_t freq_2,
				  qdf_freq_t freq_3)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_freq_range *freq_range;
	QDF_STATUS status;
	struct policy_mgr_hw_mode_params hw_mode;
	bool cur_range_sbs = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return false;

	/* if HW is not DBS return true*/
	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return true;

	/* HW is DBS/SBS capable, get current HW mode */
	status = policy_mgr_get_current_hw_mode(psoc, &hw_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("policy_mgr_get_current_hw_mode failed");
		return false;
	}
	if (hw_mode.dbs_cap || hw_mode.sbs_cap)
		cur_range_sbs = policy_mgr_is_cur_freq_range_sbs(psoc);

	freq_range = pm_ctx->hw_mode.cur_mac_freq_range;

	/* current HW is DBS OR SBS check current DBS/SBS freq range */
	if (hw_mode.dbs_cap || hw_mode.sbs_cap) {
		policy_mgr_rl_debug("freq1 %d freq2 %d freq3 %d dbs_cap %d sbs_cap %d, cur range is sbs %d",
				     freq_1, freq_2, freq_3, hw_mode.dbs_cap,
				     hw_mode.sbs_cap, cur_range_sbs);
		return policy_mgr_3_freq_same_mac_in_freq_range(pm_ctx,
							freq_range,
							freq_1, freq_2, freq_3);
	}
	/*
	 * If current HW mode is not DBS/SBS, check if in all supported mode
	 * it they will be on same mac
	 */
	return policy_mgr_3_freq_always_on_same_mac(psoc, freq_1, freq_2,
						    freq_3);
}

#ifdef FEATURE_FOURTH_CONNECTION
static void
policy_mgr_get_mac_freq_list(struct policy_mgr_freq_range *freq_range,
			     uint8_t mac_id,
			     uint8_t mac_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS],
			     uint8_t mac_mode_list[MAX_NUMBER_OF_CONC_CONNECTIONS],
			     uint8_t *mac_freq_num,
			     qdf_freq_t freq_1, enum policy_mgr_con_mode mode_1,
			     qdf_freq_t freq_2, enum policy_mgr_con_mode mode_2,
			     qdf_freq_t freq_3, enum policy_mgr_con_mode mode_3,
			     qdf_freq_t freq_4, enum policy_mgr_con_mode mode_4)
{
	uint8_t j = 0;

	if (freq_1 && IS_FREQ_ON_MAC_ID(freq_range, freq_1, mac_id)) {
		mac_freq_list[j] = freq_1;
		mac_mode_list[j++] = mode_1;
	}
	if (freq_2 && IS_FREQ_ON_MAC_ID(freq_range, freq_2, mac_id)) {
		mac_freq_list[j] = freq_2;
		mac_mode_list[j++] = mode_2;
	}
	if (freq_3 && IS_FREQ_ON_MAC_ID(freq_range, freq_3, mac_id)) {
		mac_freq_list[j] = freq_3;
		mac_mode_list[j++] = mode_3;
	}
	if (freq_4 && IS_FREQ_ON_MAC_ID(freq_range, freq_4, mac_id)) {
		mac_freq_list[j] = freq_4;
		mac_mode_list[j++] = mode_4;
	}

	*mac_freq_num = j;
}

static bool
policy_mgr_is_supported_hw_mode(struct wlan_objmgr_psoc *psoc,
				struct policy_mgr_psoc_priv_obj *pm_ctx,
				enum policy_mgr_mode hw_mode)
{
	if (hw_mode == MODE_SMM)
		return true;

	if (hw_mode == MODE_DBS)
		return policy_mgr_is_hw_dbs_capable(psoc);

	if (hw_mode == MODE_SBS_UPPER_SHARE ||
	    hw_mode == MODE_SBS_LOWER_SHARE)
		return policy_mgr_is_hw_sbs_capable(psoc) &&
			pm_ctx->hw_mode.sbs_lower_band_end_freq;

	if (hw_mode == MODE_SBS)
		return policy_mgr_is_hw_sbs_capable(psoc);

	return false;
}

static bool
policy_mgr_mac_freq_list_allow(uint8_t mac_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS],
			       uint8_t mac_mode_list[MAX_NUMBER_OF_CONC_CONNECTIONS],
			       uint8_t mac_freq_num, bool force_scc)
{
	uint8_t sta = 0, ap = 0, i;

	switch (mac_freq_num) {
	case 1:
	case 2:
		return true;
	case 3:
		/* If 3 vifs are active in same mac, target only support:
		 * 3 vifs are in SCC and 3 vifs are :
		 * 1 STA + 2 APs, or 3 APs
		 */
		if (!force_scc && (mac_freq_list[0] != mac_freq_list[1] ||
		    mac_freq_list[0] != mac_freq_list[2]))
			return false;
		for (i = 0; i < mac_freq_num; i++) {
			if (mac_mode_list[i] == PM_STA_MODE ||
			    mac_mode_list[i] == PM_P2P_CLIENT_MODE)
				sta++;
			else
				ap++;
		}

		if (sta == 1 && ap == 2)
			return true;
		if (ap == 3)
			return true;
		return false;
	default:
		return false;
	}
}

#ifdef WLAN_FEATURE_11BE_MLO
static void
policy_mgr_ml_sta_active_freq(struct wlan_objmgr_psoc *psoc,
			      qdf_freq_t ch_freq,
			      enum policy_mgr_con_mode mode,
			      uint32_t ext_flags,
			      qdf_freq_t *ml_sta_link0_freq,
			      qdf_freq_t *ml_sta_link1_freq)
{
	uint8_t num_ml_sta = 0, num_disabled_ml_sta = 0;
	uint8_t num_active_ml_sta;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	union conc_ext_flag conc_ext_flags;

	conc_ext_flags.value = ext_flags;
	/* find the two active ml sta home channels */
	policy_mgr_get_ml_sta_info_psoc(psoc, &num_ml_sta,
					&num_disabled_ml_sta,
					ml_sta_vdev_lst, ml_freq_lst,
					NULL, NULL, NULL);
	if (num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_disabled_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_ml_sta <= num_disabled_ml_sta) {
		if (num_ml_sta || num_disabled_ml_sta)
			policy_mgr_rl_debug("unexpected ml sta num %d %d",
					    num_ml_sta, num_disabled_ml_sta);
		return;
	}
	num_active_ml_sta = num_ml_sta;
	if (num_ml_sta >= num_disabled_ml_sta)
		num_active_ml_sta = num_ml_sta - num_disabled_ml_sta;
	if (num_active_ml_sta > 1) {
		*ml_sta_link0_freq = ml_freq_lst[0];
		*ml_sta_link1_freq = ml_freq_lst[1];
	} else if (num_active_ml_sta > 0 && conc_ext_flags.mlo &&
		   mode == PM_STA_MODE) {
		*ml_sta_link0_freq = ml_freq_lst[0];
		*ml_sta_link1_freq = ch_freq;
	}
}
#else
static void
policy_mgr_ml_sta_active_freq(struct wlan_objmgr_psoc *psoc,
			      qdf_freq_t ch_freq,
			      enum policy_mgr_con_mode mode,
			      uint32_t ext_flags,
			      qdf_freq_t *ml_sta_link0_freq,
			      qdf_freq_t *ml_sta_link1_freq)
{
}
#endif

bool
policy_mgr_allow_4th_new_freq(struct wlan_objmgr_psoc *psoc,
			      qdf_freq_t ch_freq,
			      enum policy_mgr_con_mode mode,
			      uint32_t ext_flags)
{
	struct policy_mgr_conc_connection_info *conn = pm_conc_connection_list;
	uint8_t mac_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t mac_mode_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t mac_freq_num;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	qdf_freq_t ml_sta_link0_freq = 0;
	qdf_freq_t ml_sta_link1_freq = 0;
	uint8_t i, j, k, nan = 0, sap = 0, sta = 0, ndi = 0, p2p;
	struct policy_mgr_freq_range *freq_range;
	bool emlsr_links_with_aux = false;
	uint8_t mac_id;
	bool force_scc = policy_mgr_is_3vifs_mcc_to_scc_enabled(psoc);

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	/* if HW is not DBS return false */
	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return false;

	/* Find the two active ml sta home channels */
	policy_mgr_ml_sta_active_freq(psoc, ch_freq, mode, ext_flags,
				      &ml_sta_link0_freq,
				      &ml_sta_link1_freq);
	if (ml_sta_link0_freq &&
	    !WLAN_REG_IS_24GHZ_CH_FREQ(ml_sta_link0_freq) &&
	    ml_sta_link1_freq &&
	    !WLAN_REG_IS_24GHZ_CH_FREQ(ml_sta_link1_freq) &&
	    policy_mgr_is_mlo_in_mode_emlsr(psoc, NULL, NULL) &&
	    wlan_mlme_is_aux_emlsr_support(psoc))
		emlsr_links_with_aux = true;

	/* Check if any hw mode can support the 4th channel frequency
	 * and device mode.
	 */
	for (j = 0; j < MODE_HW_MAX; j++) {
		if (!policy_mgr_is_supported_hw_mode(psoc, pm_ctx, j))
			continue;
		freq_range = pm_ctx->hw_mode.freq_range_caps[j];

		/* If ml sta present, the two links should be in
		 * different mac always. Skip the hw mode which
		 * causes they in same mac.
		 * Emlsr links with aux supported hw, the two links
		 * can be eMLSR mode in mac 1.
		 */
		mac_id = 0;
		if (ml_sta_link0_freq && ml_sta_link1_freq &&
		    policy_mgr_2_freq_same_mac_id_in_freq_range(
				pm_ctx, freq_range, ml_sta_link0_freq,
				ml_sta_link1_freq, &mac_id)) {
			if (!(emlsr_links_with_aux && mac_id == 1))
				continue;
		}

		for (i = 0; i < MAX_MAC; i++) {
			/* Get the freq list which are in the MAC
			 * supported freq range.
			 */
			policy_mgr_get_mac_freq_list(
				freq_range,
				i,
				mac_freq_list, mac_mode_list, &mac_freq_num,
				conn[0].freq, conn[0].mode,
				conn[1].freq, conn[1].mode,
				conn[2].freq, conn[2].mode,
				ch_freq, mode);

			/* Check the freq & mode list support or not in the
			 * MAC.
			 */
			if (!policy_mgr_mac_freq_list_allow(
				mac_freq_list, mac_mode_list, mac_freq_num,
				force_scc))
				break;
		}

		/* If the frequency/mode combination meet requirement in the
		 * hw mode, then the 4th new ch_freq/mode are allowed to start
		 * in this hw mode.
		 */
		if (i == MAX_MAC) {
			policy_mgr_rl_debug("new freq %d mode %s is allowed in hw mode %s",
					    ch_freq,
					    device_mode_to_string(mode),
					    policy_mgr_hw_mode_to_str(j));
			return true;
		}

		sta = 0, p2p = 0, nan = 0, ndi = 0, sap = 0;
		for (i = 0; i < MAX_MAC; i++) {
			if (!wlan_nan_is_sta_p2p_ndp_supported(psoc) &&
			    !wlan_nan_is_sta_sap_nan_allowed(psoc))
				break;
			/* Get the freq list which are in the MAC
			 * supported freq range.
			 */
			policy_mgr_get_mac_freq_list(
				freq_range,
				i,
				mac_freq_list, mac_mode_list, &mac_freq_num,
				conn[0].freq, conn[0].mode,
				conn[1].freq, conn[1].mode,
				conn[2].freq, conn[2].mode,
				ch_freq, mode);

			for (k = 0; k < mac_freq_num; k++) {
				if (mac_mode_list[k] == PM_STA_MODE)
					sta++;
				else if (mac_mode_list[k] ==
							PM_P2P_CLIENT_MODE ||
					 mac_mode_list[k] == PM_P2P_GO_MODE)
					p2p++;
				else if (mac_mode_list[k] == PM_NAN_DISC_MODE)
					nan++;
				else if (mac_mode_list[k] == PM_NDI_MODE)
					ndi++;
				else if (mac_mode_list[k] == PM_SAP_MODE)
					sap++;
			}
		}

		if (wlan_nan_is_sta_p2p_ndp_supported(psoc) &&
		    ((sta == 2 && nan == 1 && p2p == 1) ||
		     (sta == 1 && nan == 1 && ndi == 1 && p2p == 1))) {
			policy_mgr_rl_debug("new freq %d mode %s is allowed in hw mode %s",
					    ch_freq,
					    device_mode_to_string(mode),
					    policy_mgr_hw_mode_to_str(j));
			return true;
		}

		if (((sta == 2 && nan == 1 && sap == 1) ||
		     (sta == 1 && nan == 1 && ndi == 1 && sap == 1)) &&
		    wlan_nan_is_sta_sap_nan_allowed(psoc)) {
			policy_mgr_debug("new freq %d mode %s is allowed in hw mode %s",
					 ch_freq,
					 device_mode_to_string(mode),
					 policy_mgr_hw_mode_to_str(j));
			return true;
		}
	}

	policy_mgr_debug("the 4th new freq %d mode %s is not allowed in any hw mode",
			 ch_freq, device_mode_to_string(mode));

	return false;
}
#endif

bool policy_mgr_are_sbs_chan(struct wlan_objmgr_psoc *psoc, qdf_freq_t freq_1,
			     qdf_freq_t freq_2)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx)
		return false;

	if (!policy_mgr_is_hw_sbs_capable(psoc))
		return false;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(freq_1) ||
	    WLAN_REG_IS_24GHZ_CH_FREQ(freq_2))
		return false;

	return !policy_mgr_2_freq_same_mac_in_sbs(pm_ctx, freq_1, freq_2);
}

bool policy_mgr_is_current_hwmode_sbs(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_hw_mode_params hw_mode;

	if (!policy_mgr_is_hw_sbs_capable(psoc))
		return false;

	if (QDF_STATUS_SUCCESS !=
	    policy_mgr_get_current_hw_mode(psoc, &hw_mode))
		return false;

	if (hw_mode.sbs_cap && policy_mgr_is_cur_freq_range_sbs(psoc))
		return true;

	return false;
}

void policy_mgr_init_dbs_hw_mode(struct wlan_objmgr_psoc *psoc,
		uint32_t num_dbs_hw_modes,
		uint32_t *ev_wlan_dbs_hw_mode_list)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	pm_ctx->num_dbs_hw_modes = num_dbs_hw_modes;
	pm_ctx->hw_mode.hw_mode_list =
		qdf_mem_malloc(sizeof(*pm_ctx->hw_mode.hw_mode_list) *
		pm_ctx->num_dbs_hw_modes);
	if (!pm_ctx->hw_mode.hw_mode_list) {
		pm_ctx->num_dbs_hw_modes = 0;
		return;
	}

	qdf_mem_copy(pm_ctx->hw_mode.hw_mode_list,
		ev_wlan_dbs_hw_mode_list,
		(sizeof(*pm_ctx->hw_mode.hw_mode_list) *
		pm_ctx->num_dbs_hw_modes));

	policy_mgr_dump_dbs_hw_mode(psoc);
}

void policy_mgr_dump_dbs_hw_mode(struct wlan_objmgr_psoc *psoc)
{
	uint32_t i;
	uint32_t param;
	uint32_t param1;

	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	policy_mgr_debug("old_hw_mode_index=%d, new_hw_mode_index=%d",
			 pm_ctx->old_hw_mode_index, pm_ctx->new_hw_mode_index);

	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		param = pm_ctx->hw_mode.hw_mode_list[i];
		param1 = pm_ctx->hw_mode.hw_mode_list[i] >> 32;
		policy_mgr_debug("[%d] 0x%x 0x%x", i, param, param1);
		policy_mgr_debug("[%d]-MAC0: tx_ss:%d rx_ss:%d bw_idx:%d band_cap:%d",
				 i,
				 POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_GET(param),
				 POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_GET(param),
				 POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_GET(param),
				 POLICY_MGR_HW_MODE_MAC0_BAND_GET(param));
		policy_mgr_debug("[%d]-MAC1: tx_ss:%d rx_ss:%d bw_idx:%d",
				 i,
				 POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_GET(param),
				 POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_GET(param),
				 POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_GET(param));
		policy_mgr_debug("[%d] DBS:%d SBS:%d hw_mode_id:%d", i,
				 POLICY_MGR_HW_MODE_DBS_MODE_GET(param),
				 POLICY_MGR_HW_MODE_SBS_MODE_GET(param),
				 POLICY_MGR_HW_MODE_ID_GET(param));
	}
}

void policy_mgr_init_dbs_config(struct wlan_objmgr_psoc *psoc,
		uint32_t scan_config, uint32_t fw_config)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t dual_mac_feature;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	pm_ctx->dual_mac_cfg.cur_scan_config = 0;
	pm_ctx->dual_mac_cfg.cur_fw_mode_config = 0;

	dual_mac_feature = pm_ctx->cfg.dual_mac_feature;
	/* If dual mac features are disabled in the INI, we
	 * need not proceed further
	 */
	if (dual_mac_feature == DISABLE_DBS_CXN_AND_SCAN) {
		policy_mgr_err("Disabling dual mac capabilities");
		/* All capabilities are initialized to 0. We can return */
		goto done;
	}

	/* Initialize concurrent_scan_config_bits with default FW value */
	WMI_DBS_CONC_SCAN_CFG_ASYNC_DBS_SCAN_SET(
		pm_ctx->dual_mac_cfg.cur_scan_config,
		WMI_DBS_CONC_SCAN_CFG_ASYNC_DBS_SCAN_GET(scan_config));
	WMI_DBS_CONC_SCAN_CFG_SYNC_DBS_SCAN_SET(
		pm_ctx->dual_mac_cfg.cur_scan_config,
		WMI_DBS_CONC_SCAN_CFG_SYNC_DBS_SCAN_GET(scan_config));
	WMI_DBS_CONC_SCAN_CFG_DBS_SCAN_SET(
		pm_ctx->dual_mac_cfg.cur_scan_config,
		WMI_DBS_CONC_SCAN_CFG_DBS_SCAN_GET(scan_config));
	WMI_DBS_CONC_SCAN_CFG_AGILE_SCAN_SET(
		pm_ctx->dual_mac_cfg.cur_scan_config,
		WMI_DBS_CONC_SCAN_CFG_AGILE_SCAN_GET(scan_config));
	WMI_DBS_CONC_SCAN_CFG_AGILE_DFS_SCAN_SET(
		pm_ctx->dual_mac_cfg.cur_scan_config,
		WMI_DBS_CONC_SCAN_CFG_AGILE_DFS_SCAN_GET(scan_config));

	/* Initialize fw_mode_config_bits with default FW value */
	WMI_DBS_FW_MODE_CFG_DBS_SET(
		pm_ctx->dual_mac_cfg.cur_fw_mode_config,
		WMI_DBS_FW_MODE_CFG_DBS_GET(fw_config));
	WMI_DBS_FW_MODE_CFG_AGILE_DFS_SET(
		pm_ctx->dual_mac_cfg.cur_fw_mode_config,
		WMI_DBS_FW_MODE_CFG_AGILE_DFS_GET(fw_config));
	WMI_DBS_FW_MODE_CFG_DBS_FOR_CXN_SET(
		pm_ctx->dual_mac_cfg.cur_fw_mode_config,
		WMI_DBS_FW_MODE_CFG_DBS_FOR_CXN_GET(fw_config));
done:
	/* Initialize the previous scan/fw mode config */
	pm_ctx->dual_mac_cfg.prev_scan_config =
		pm_ctx->dual_mac_cfg.cur_scan_config;
	pm_ctx->dual_mac_cfg.prev_fw_mode_config =
		pm_ctx->dual_mac_cfg.cur_fw_mode_config;

	policy_mgr_debug("cur_scan_config:%x cur_fw_mode_config:%x",
		pm_ctx->dual_mac_cfg.cur_scan_config,
		pm_ctx->dual_mac_cfg.cur_fw_mode_config);
}

void policy_mgr_init_sbs_fw_config(struct wlan_objmgr_psoc *psoc,
				   uint32_t fw_config)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool sbs_enabled;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	/*
	 * If SBS is not enabled from ini, no need to set SBS bits in fw config
	 */
	sbs_enabled = pm_ctx->cfg.sbs_enable;
	if (!sbs_enabled) {
		policy_mgr_debug("SBS not enabled from ini");
		return;
	}

	/* Initialize fw_mode_config_bits with default FW value */
	WMI_DBS_FW_MODE_CFG_ASYNC_SBS_SET(
			pm_ctx->dual_mac_cfg.cur_fw_mode_config,
			WMI_DBS_FW_MODE_CFG_ASYNC_SBS_GET(fw_config));

	policy_mgr_debug("fw_mode config updated from %x to %x",
			 pm_ctx->dual_mac_cfg.prev_fw_mode_config,
			 pm_ctx->dual_mac_cfg.cur_fw_mode_config);
	/* Initialize the previous scan/fw mode config */
	pm_ctx->dual_mac_cfg.prev_fw_mode_config =
		pm_ctx->dual_mac_cfg.cur_fw_mode_config;
}

void policy_mgr_update_dbs_scan_config(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	pm_ctx->dual_mac_cfg.prev_scan_config =
		pm_ctx->dual_mac_cfg.cur_scan_config;
	pm_ctx->dual_mac_cfg.cur_scan_config =
		pm_ctx->dual_mac_cfg.req_scan_config;
}

void policy_mgr_update_dbs_fw_config(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	pm_ctx->dual_mac_cfg.prev_fw_mode_config =
		pm_ctx->dual_mac_cfg.cur_fw_mode_config;
	pm_ctx->dual_mac_cfg.cur_fw_mode_config =
		pm_ctx->dual_mac_cfg.req_fw_mode_config;
}

void policy_mgr_update_dbs_req_config(struct wlan_objmgr_psoc *psoc,
		uint32_t scan_config, uint32_t fw_mode_config)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	pm_ctx->dual_mac_cfg.req_scan_config = scan_config;
	pm_ctx->dual_mac_cfg.req_fw_mode_config = fw_mode_config;
}

bool policy_mgr_get_dbs_plus_agile_scan_config(struct wlan_objmgr_psoc *psoc)
{
	uint32_t scan_config;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	if (policy_mgr_is_dual_mac_disabled_in_ini(psoc))
		return false;


	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		/* We take that it is disabled and proceed */
		return false;
	}
	scan_config = pm_ctx->dual_mac_cfg.cur_scan_config;

	return WMI_DBS_CONC_SCAN_CFG_AGILE_SCAN_GET(scan_config);
}

bool policy_mgr_get_single_mac_scan_with_dfs_config(
		struct wlan_objmgr_psoc *psoc)
{
	uint32_t scan_config;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	if (policy_mgr_is_dual_mac_disabled_in_ini(psoc))
		return false;


	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		/* We take that it is disabled and proceed */
		return false;
	}
	scan_config = pm_ctx->dual_mac_cfg.cur_scan_config;

	return WMI_DBS_CONC_SCAN_CFG_AGILE_DFS_SCAN_GET(scan_config);
}

int8_t policy_mgr_get_num_dbs_hw_modes(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return -EINVAL;
	}
	return pm_ctx->num_dbs_hw_modes;
}

bool policy_mgr_find_if_fw_supports_dbs(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wmi_unified *wmi_handle;
	bool dbs_support;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}
	dbs_support =
	wmi_service_enabled(wmi_handle,
			    wmi_service_dual_band_simultaneous_support);

	/* The agreement with FW is that: To know if the target is DBS
	 * capable, DBS needs to be supported both in the HW mode list
	 * and in the service ready event
	 */
	if (!dbs_support)
		return false;

	return true;
}

bool policy_mgr_find_if_hwlist_has_dbs(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t param, i, found = 0;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		param = pm_ctx->hw_mode.hw_mode_list[i];
		if (POLICY_MGR_HW_MODE_DBS_MODE_GET(param)) {
			found = 1;
			break;
		}
	}
	if (found)
		return true;

	return false;
}

static bool policy_mgr_find_if_hwlist_has_sbs(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t param, i;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		param = pm_ctx->hw_mode.hw_mode_list[i];
		if (POLICY_MGR_HW_MODE_SBS_MODE_GET(param)) {
			return true;
		}
	}

	return false;
}

bool policy_mgr_is_dbs_scan_allowed(struct wlan_objmgr_psoc *psoc)
{
	uint8_t dbs_type = DISABLE_DBS_CXN_AND_SCAN;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (!policy_mgr_find_if_fw_supports_dbs(psoc) ||
	    !policy_mgr_find_if_hwlist_has_dbs(psoc))
		return false;

	policy_mgr_get_dual_mac_feature(psoc, &dbs_type);
	/*
	 * If DBS support for scan is disabled through INI then DBS is not
	 * supported for scan.
	 *
	 * For DBS scan check the INI value explicitly
	 */
	switch (dbs_type) {
	case DISABLE_DBS_CXN_AND_SCAN:
	case ENABLE_DBS_CXN_AND_DISABLE_DBS_SCAN:
		return false;
	default:
		return true;
	}
}

bool policy_mgr_is_hw_dbs_capable(struct wlan_objmgr_psoc *psoc)
{
	if (!policy_mgr_is_dbs_enable(psoc))
		return false;

	if (!policy_mgr_find_if_fw_supports_dbs(psoc))
		return false;

	if (!policy_mgr_find_if_hwlist_has_dbs(psoc)) {
		policymgr_nofl_debug("HW mode list has no DBS");
		return false;
	}

	return true;
}

bool policy_mgr_is_hw_emlsr_capable(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint64_t param, i;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		param = pm_ctx->hw_mode.hw_mode_list[i];
		if (POLICY_MGR_HW_MODE_EMLSR_MODE_GET(param))
			return true;
	}

	return false;
}

bool policy_mgr_is_current_hwmode_dbs(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_hw_mode_params hw_mode;

	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return false;

	if (QDF_STATUS_SUCCESS != policy_mgr_get_current_hw_mode(psoc,
								 &hw_mode))
		return false;

	if (!hw_mode.dbs_cap)
		return false;

	/* sbs is not enabled and dbs_cap is set return true */
	if (!policy_mgr_is_hw_sbs_capable(psoc))
		return true;

	/* sbs is enabled and dbs_cap is set then check the freq range */
	if (!policy_mgr_is_cur_freq_range_sbs(psoc))
		return true;

	return false;
}

bool policy_mgr_is_pcl_weightage_required(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct dual_sta_policy *dual_sta_policy;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return true;

	dual_sta_policy = &mlme_obj->cfg.gen.dual_sta_policy;

	if (dual_sta_policy->concurrent_sta_policy  ==
		QCA_WLAN_CONCURRENT_STA_POLICY_PREFER_PRIMARY &&
		dual_sta_policy->primary_vdev_id != WLAN_UMAC_VDEV_ID_MAX) {
		return false;
	}

	return true;
}

bool policy_mgr_is_interband_mcc_supported(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wmi_unified *wmi_handle;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}

	return !wmi_service_enabled(wmi_handle,
				    wmi_service_no_interband_mcc_support);
}

static bool policy_mgr_is_sbs_enable(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	/*
	 * if gEnableSBS is not set then policy_mgr_init_sbs_fw_config won't
	 * enable Async SBS fw config bit
	 */
	if (WMI_DBS_FW_MODE_CFG_ASYNC_SBS_GET(
			pm_ctx->dual_mac_cfg.cur_fw_mode_config))
		return true;

	return false;
}

bool policy_mgr_is_hw_sbs_capable(struct wlan_objmgr_psoc *psoc)
{
	if (!policy_mgr_is_sbs_enable(psoc)) {
		policy_mgr_rl_debug("SBS INI is disabled");
		return false;
	}

	if (!policy_mgr_find_if_fw_supports_dbs(psoc)) {
		policy_mgr_rl_debug("fw doesn't support dual band");
		return false;
	}

	if (!policy_mgr_find_if_hwlist_has_sbs(psoc)) {
		policy_mgr_rl_debug("HW mode list has no SBS");
		return false;
	}

	return true;
}

QDF_STATUS policy_mgr_get_dbs_hw_modes(struct wlan_objmgr_psoc *psoc,
		bool *one_by_one_dbs, bool *two_by_two_dbs)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i;
	int8_t found_one_by_one = -EINVAL, found_two_by_two = -EINVAL;
	uint32_t conf1_tx_ss, conf1_rx_ss;
	uint32_t conf2_tx_ss, conf2_rx_ss;

	*one_by_one_dbs = false;
	*two_by_two_dbs = false;

	if (policy_mgr_is_hw_dbs_capable(psoc) == false) {
		policy_mgr_rl_debug("HW is not DBS capable");
		/* Caller will understand that DBS is disabled */
		return QDF_STATUS_SUCCESS;

	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	/* To check 1x1 capability */
	policy_mgr_get_tx_rx_ss_from_config(HW_MODE_SS_1x1,
			&conf1_tx_ss, &conf1_rx_ss);
	/* To check 2x2 capability */
	policy_mgr_get_tx_rx_ss_from_config(HW_MODE_SS_2x2,
			&conf2_tx_ss, &conf2_rx_ss);

	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		uint32_t t_conf0_tx_ss, t_conf0_rx_ss;
		uint32_t t_conf1_tx_ss, t_conf1_rx_ss;
		uint32_t dbs_mode;

		t_conf0_tx_ss = POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_GET(
				pm_ctx->hw_mode.hw_mode_list[i]);
		t_conf0_rx_ss = POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_GET(
				pm_ctx->hw_mode.hw_mode_list[i]);
		t_conf1_tx_ss = POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_GET(
				pm_ctx->hw_mode.hw_mode_list[i]);
		t_conf1_rx_ss = POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_GET(
				pm_ctx->hw_mode.hw_mode_list[i]);
		dbs_mode = POLICY_MGR_HW_MODE_DBS_MODE_GET(
				pm_ctx->hw_mode.hw_mode_list[i]);

		if (((((t_conf0_tx_ss == conf1_tx_ss) &&
		    (t_conf0_rx_ss == conf1_rx_ss)) ||
		    ((t_conf1_tx_ss == conf1_tx_ss) &&
		    (t_conf1_rx_ss == conf1_rx_ss))) &&
		    (dbs_mode == HW_MODE_DBS)) &&
		    (found_one_by_one < 0)) {
			found_one_by_one = i;
			policy_mgr_debug("1x1 hw_mode index %d found", i);
			/* Once an entry is found, need not check for 1x1
			 * again
			 */
			continue;
		}

		if (((((t_conf0_tx_ss == conf2_tx_ss) &&
		    (t_conf0_rx_ss == conf2_rx_ss)) ||
		    ((t_conf1_tx_ss == conf2_tx_ss) &&
		    (t_conf1_rx_ss == conf2_rx_ss))) &&
		    (dbs_mode == HW_MODE_DBS)) &&
		    (found_two_by_two < 0)) {
			found_two_by_two = i;
			policy_mgr_debug("2x2 hw_mode index %d found", i);
			/* Once an entry is found, need not check for 2x2
			 * again
			 */
			continue;
		}
	}

	if (found_one_by_one >= 0)
		*one_by_one_dbs = true;
	if (found_two_by_two >= 0)
		*two_by_two_dbs = true;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_current_hw_mode(struct wlan_objmgr_psoc *psoc,
		struct policy_mgr_hw_mode_params *hw_mode)
{
	QDF_STATUS status;
	uint32_t old_hw_index = 0, new_hw_index = 0;

	status = policy_mgr_get_old_and_new_hw_index(psoc, &old_hw_index,
			&new_hw_index);
	if (QDF_STATUS_SUCCESS != status) {
		policy_mgr_err("Failed to get HW mode index");
		return QDF_STATUS_E_FAILURE;
	}

	if (new_hw_index == POLICY_MGR_DEFAULT_HW_MODE_INDEX) {
		policy_mgr_err("HW mode is not yet initialized");
		return QDF_STATUS_E_FAILURE;
	}

	status = policy_mgr_get_hw_mode_from_idx(psoc, new_hw_index, hw_mode);
	if (QDF_STATUS_SUCCESS != status) {
		policy_mgr_err("Failed to get HW mode index");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_is_dbs_enable(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	if (policy_mgr_is_dual_mac_disabled_in_ini(psoc)) {
		policy_mgr_rl_debug("DBS is disabled from ini");
		return false;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (WMI_DBS_FW_MODE_CFG_DBS_GET(
			pm_ctx->dual_mac_cfg.cur_fw_mode_config))
		return true;

	return false;
}

bool policy_mgr_is_hw_dbs_2x2_capable(struct wlan_objmgr_psoc *psoc)
{
	struct dbs_nss nss_dbs = {0};
	uint32_t nss;

	nss = policy_mgr_get_hw_dbs_nss(psoc, &nss_dbs);
	if (nss >= HW_MODE_SS_2x2 && (nss_dbs.mac0_ss == nss_dbs.mac1_ss))
		return true;
	else
		return false;
}

bool policy_mgr_is_hw_dbs_required_for_band(struct wlan_objmgr_psoc *psoc,
					    enum hw_mode_mac_band_cap band)
{
	struct dbs_nss nss_dbs = {0};
	uint32_t nss;

	nss = policy_mgr_get_hw_dbs_nss(psoc, &nss_dbs);
	if (nss >= HW_MODE_SS_1x1 && nss_dbs.mac0_ss >= nss_dbs.mac1_ss &&
	    !(nss_dbs.single_mac0_band_cap & band))
		return true;
	else
		return false;
}

bool policy_mgr_is_dp_hw_dbs_capable(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_find_if_hwlist_has_dbs(psoc);
}

/*
 * policy_mgr_is_2x2_1x1_dbs_capable() - check 2x2+1x1 DBS supported or not
 * @psoc: PSOC object data
 *
 * This routine is called to check 2x2 5G + 1x1 2G (DBS1) or
 * 2x2 2G + 1x1 5G (DBS2) support or not.
 * Either DBS1 or DBS2 supported
 *
 * Return: true/false
 */
bool policy_mgr_is_2x2_1x1_dbs_capable(struct wlan_objmgr_psoc *psoc)
{
	struct dbs_nss nss_dbs;
	uint32_t nss;

	nss = policy_mgr_get_hw_dbs_nss(psoc, &nss_dbs);
	if (nss >= HW_MODE_SS_2x2 && (nss_dbs.mac0_ss > nss_dbs.mac1_ss))
		return true;
	else
		return false;
}

/*
 * policy_mgr_is_2x2_5G_1x1_2G_dbs_capable() - check Genoa DBS1 enabled or not
 * @psoc: PSOC object data
 *
 * This routine is called to check support DBS1 or not.
 * Notes: DBS1: 2x2 5G + 1x1 2G.
 * This function will call policy_mgr_get_hw_mode_idx_from_dbs_hw_list to match
 * the HW mode from hw mode list. The parameters will also be matched to
 * 2x2 5G +2x2 2G HW mode. But firmware will not report 2x2 5G + 2x2 2G alone
 * with 2x2 5G + 1x1 2G at same time. So, it is safe to find DBS1 with
 * policy_mgr_get_hw_mode_idx_from_dbs_hw_list.
 *
 * Return: true/false
 */
bool policy_mgr_is_2x2_5G_1x1_2G_dbs_capable(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_is_2x2_1x1_dbs_capable(psoc) &&
		(policy_mgr_get_hw_mode_idx_from_dbs_hw_list(
					psoc,
					HW_MODE_SS_2x2,
					HW_MODE_80_MHZ,
					HW_MODE_SS_1x1, HW_MODE_40_MHZ,
					HW_MODE_MAC_BAND_5G,
					HW_MODE_DBS,
					HW_MODE_AGILE_DFS_NONE,
					HW_MODE_SBS_NONE) >= 0);
}

/*
 * policy_mgr_is_2x2_2G_1x1_5G_dbs_capable() - check Genoa DBS2 enabled or not
 * @psoc: PSOC object data
 *
 * This routine is called to check support DBS2 or not.
 * Notes: DBS2: 2x2 2G + 1x1 5G
 *
 * Return: true/false
 */
bool policy_mgr_is_2x2_2G_1x1_5G_dbs_capable(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_is_2x2_1x1_dbs_capable(psoc) &&
		(policy_mgr_get_hw_mode_idx_from_dbs_hw_list(
					psoc,
					HW_MODE_SS_2x2,
					HW_MODE_40_MHZ,
					HW_MODE_SS_1x1, HW_MODE_40_MHZ,
					HW_MODE_MAC_BAND_2G,
					HW_MODE_DBS,
					HW_MODE_AGILE_DFS_NONE,
					HW_MODE_SBS_NONE) >= 0);
}

uint32_t policy_mgr_get_connection_count(struct wlan_objmgr_psoc *psoc)
{
	uint32_t conn_index, count = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if (pm_conc_connection_list[conn_index].in_use)
			count++;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

uint32_t
policy_mgr_get_connection_count_with_mlo(struct wlan_objmgr_psoc *psoc)
{
	uint32_t conn_index, count = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum policy_mgr_con_mode mode;
	bool is_mlo = false, count_mlo = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if (pm_conc_connection_list[conn_index].in_use) {
			is_mlo = policy_mgr_is_ml_vdev_id(psoc,
				   pm_conc_connection_list[conn_index].vdev_id);
			mode = pm_conc_connection_list[conn_index].mode;
			if (is_mlo && (mode == PM_STA_MODE)) {
				if (!count_mlo) {
					count_mlo = true;
					count++;
				}
			} else {
				count++;
			}
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

uint32_t policy_mgr_mode_specific_vdev_id(struct wlan_objmgr_psoc *psoc,
					  enum policy_mgr_con_mode mode)
{
	uint32_t conn_index = 0;
	uint32_t vdev_id = WLAN_INVALID_VDEV_ID;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return vdev_id;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	/*
	 * Note: This gives you the first vdev id of the mode type in a
	 * sta+sta or sap+sap or p2p + p2p case
	 */
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if ((pm_conc_connection_list[conn_index].mode == mode) &&
			pm_conc_connection_list[conn_index].in_use) {
			vdev_id = pm_conc_connection_list[conn_index].vdev_id;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return vdev_id;
}

uint32_t policy_mgr_mode_get_macid_by_vdev_id(struct wlan_objmgr_psoc *psoc,
					      uint32_t vdev_id)
{
	uint32_t conn_index = 0;
	uint32_t mac_id = 0xFF;
	enum policy_mgr_con_mode mode;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return vdev_id;
	}
	mode = policy_mgr_con_mode_by_vdev_id(psoc, vdev_id);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);

	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if ((pm_conc_connection_list[conn_index].mode == mode) &&
		    pm_conc_connection_list[conn_index].in_use &&
		    vdev_id == pm_conc_connection_list[conn_index].vdev_id) {
			mac_id = pm_conc_connection_list[conn_index].mac;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return mac_id;
}

uint32_t policy_mgr_mode_specific_connection_count(
		struct wlan_objmgr_psoc *psoc,
		enum policy_mgr_con_mode mode,
		uint32_t *list)
{
	uint32_t conn_index = 0, count = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if ((pm_conc_connection_list[conn_index].mode == mode) &&
			pm_conc_connection_list[conn_index].in_use) {
			if (list)
				list[count] = conn_index;
			 count++;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

uint32_t policy_mgr_get_sap_mode_count(struct wlan_objmgr_psoc *psoc,
				       uint32_t *list)
{
	uint32_t count;

	count = policy_mgr_mode_specific_connection_count(psoc, PM_SAP_MODE,
							  list);

	count += policy_mgr_mode_specific_connection_count(
						psoc,
						PM_LL_LT_SAP_MODE,
						list ? &list[count] : NULL);
	return count;
}

uint32_t policy_mgr_get_beaconing_mode_count(struct wlan_objmgr_psoc *psoc,
					     uint32_t *list)
{
	uint32_t count;

	count = policy_mgr_get_sap_mode_count(psoc, list);

	count += policy_mgr_mode_specific_connection_count(
						psoc,
						PM_P2P_GO_MODE,
						list ? &list[count] : NULL);
	return count;
}

QDF_STATUS policy_mgr_check_conn_with_mode_and_vdev_id(
		struct wlan_objmgr_psoc *psoc, enum policy_mgr_con_mode mode,
		uint32_t vdev_id)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return qdf_status;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if ((pm_conc_connection_list[conn_index].mode == mode) &&
		    (pm_conc_connection_list[conn_index].vdev_id == vdev_id)) {
			qdf_status = QDF_STATUS_SUCCESS;
			break;
		}
		conn_index++;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	return qdf_status;
}

void policy_mgr_soc_set_dual_mac_cfg_cb(enum set_hw_mode_status status,
		uint32_t scan_config,
		uint32_t fw_mode_config)
{
	policy_mgr_debug("Status:%d for scan_config:%x fw_mode_config:%x",
			 status, scan_config, fw_mode_config);
}

void policy_mgr_set_dual_mac_scan_config(struct wlan_objmgr_psoc *psoc,
		uint8_t dbs_val,
		uint8_t dbs_plus_agile_scan_val,
		uint8_t single_mac_scan_with_dbs_val)
{
	struct policy_mgr_dual_mac_config cfg;
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	/* Any non-zero positive value is treated as 1 */
	if (dbs_val != 0)
		dbs_val = 1;
	if (dbs_plus_agile_scan_val != 0)
		dbs_plus_agile_scan_val = 1;
	if (single_mac_scan_with_dbs_val != 0)
		single_mac_scan_with_dbs_val = 1;

	status = policy_mgr_get_updated_scan_config(psoc, &cfg.scan_config,
			dbs_val,
			dbs_plus_agile_scan_val,
			single_mac_scan_with_dbs_val);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("policy_mgr_get_updated_scan_config failed %d",
			status);
		return;
	}

	status = policy_mgr_get_updated_fw_mode_config(psoc,
			&cfg.fw_mode_config,
			policy_mgr_get_dbs_config(psoc),
			policy_mgr_get_agile_dfs_config(psoc));
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("policy_mgr_get_updated_fw_mode_config failed %d",
			status);
		return;
	}

	cfg.set_dual_mac_cb = policy_mgr_soc_set_dual_mac_cfg_cb;

	policy_mgr_debug("scan_config:%x fw_mode_config:%x",
			cfg.scan_config, cfg.fw_mode_config);

	status = pm_ctx->sme_cbacks.sme_soc_set_dual_mac_config(cfg);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("sme_soc_set_dual_mac_config failed %d", status);
}

void policy_mgr_set_dual_mac_fw_mode_config(struct wlan_objmgr_psoc *psoc,
			uint8_t dbs, uint8_t dfs)
{
	struct policy_mgr_dual_mac_config cfg;
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	/* Any non-zero positive value is treated as 1 */
	if (dbs != 0)
		dbs = 1;
	if (dfs != 0)
		dfs = 1;

	status = policy_mgr_get_updated_scan_config(psoc, &cfg.scan_config,
			policy_mgr_get_dbs_scan_config(psoc),
			policy_mgr_get_dbs_plus_agile_scan_config(psoc),
			policy_mgr_get_single_mac_scan_with_dfs_config(psoc));
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("policy_mgr_get_updated_scan_config failed %d",
			status);
		return;
	}

	status = policy_mgr_get_updated_fw_mode_config(psoc,
				&cfg.fw_mode_config, dbs, dfs);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("policy_mgr_get_updated_fw_mode_config failed %d",
			status);
		return;
	}

	cfg.set_dual_mac_cb = policy_mgr_soc_set_dual_mac_cfg_cb;

	policy_mgr_debug("scan_config:%x fw_mode_config:%x",
			cfg.scan_config, cfg.fw_mode_config);

	status = pm_ctx->sme_cbacks.sme_soc_set_dual_mac_config(cfg);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("sme_soc_set_dual_mac_config failed %d", status);
}

bool policy_mgr_is_scc_with_this_vdev_id(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i, ch_freq;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	/* Get the channel freq for a given vdev_id */
	status = policy_mgr_get_chan_by_session_id(psoc, vdev_id,
						   &ch_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Failed to get channel for vdev:%d", vdev_id);
		return false;
	}

	/* Compare given vdev_id freq against other vdev_id's */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if ((pm_conc_connection_list[i].vdev_id != vdev_id) &&
		    (pm_conc_connection_list[i].in_use) &&
		    (pm_conc_connection_list[i].freq == ch_freq)) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return false;
}

/*
 * policy_mgr_get_sta_p2p_conn_info() - Get STA+P2P connection info
 * @pm_ctx: pm_ctx ctx
 * @info: Buffer to carry the final connection info
 *
 * STA/P2P-CLI/P2P-GO can do MCC with NAN/NDI and firmware takes care of
 * managing the MCC. Host driver doesn't have to consider NAN/NDI channels or
 * take any decision based on NAN/NDI channels.
 * But SAP and NAN MCC has a restriction as both are beaconing entities and SAP
 * can't go off-channel for much time. So, consider NAN/NDI channels also for
 * MCC checks to take any decision on SAP.
 * This API is to decide whether to include(for SAP) or exclude(for STA/P2P)
 * NAN/NDI and fill the buffer.
 *
 * Return: Number of connections that are in MCC
 */
static uint32_t
policy_mgr_get_sta_p2p_conn_info(struct policy_mgr_psoc_priv_obj *pm_ctx,
				 struct policy_mgr_conc_connection_info *info)
{
	uint32_t num_connections = 0, i;
	bool sap_present;

	sap_present = !!policy_mgr_mode_specific_connection_count(pm_ctx->psoc,
								  PM_SAP_MODE,
								  NULL);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (!pm_conc_connection_list[i].in_use)
			continue;

		if (!sap_present &&
		    ((pm_conc_connection_list[i].mode == PM_NAN_DISC_MODE ||
		      pm_conc_connection_list[i].mode == PM_NDI_MODE) &&
		     wlan_nan_is_sta_p2p_ndp_supported(pm_ctx->psoc)))
			continue;

		info[num_connections++] = pm_conc_connection_list[i];
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return num_connections;
}

bool policy_mgr_is_mcc_with_this_vdev_id(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id, uint8_t *mcc_vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i, ch_freq, num_connections = 0;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct policy_mgr_conc_connection_info info[
			MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};

	if (mcc_vdev_id)
		*mcc_vdev_id = WLAN_INVALID_VDEV_ID;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	/* Get the channel freq for a given vdev_id */
	status = policy_mgr_get_chan_by_session_id(psoc, vdev_id, &ch_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Failed to get channel for vdev:%d", vdev_id);
		return false;
	}

	/* Compare given vdev_id freq against other vdev_id's */
	num_connections = policy_mgr_get_sta_p2p_conn_info(pm_ctx, info);

	for (i = 0; i < num_connections; i++) {
		if (info[i].vdev_id == vdev_id)
			continue;

		if (info[i].freq != ch_freq &&
		    policy_mgr_are_2_freq_on_same_mac(psoc,
						      info[i].freq,
						      ch_freq)) {
			if (mcc_vdev_id)
				*mcc_vdev_id = info[i].vdev_id;

			return true;
		}
	}

	return false;
}

bool policy_mgr_is_mcc_on_any_sta_vdev(struct wlan_objmgr_psoc *psoc)
{
	uint8_t connection_count, i;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};

	connection_count =
		policy_mgr_get_mode_specific_conn_info(psoc, NULL, vdev_id_list,
						       PM_STA_MODE);
	if (!connection_count)
		return false;

	for (i = 0; i < connection_count; i++)
		if (policy_mgr_is_mcc_with_this_vdev_id(psoc, vdev_id_list[i],
							NULL))
			return true;

	return false;
}

bool policy_mgr_current_concurrency_is_scc(struct wlan_objmgr_psoc *psoc)
{
	uint32_t num_connections = 0;
	bool is_scc = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info info[
			MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return is_scc;
	}

	num_connections = policy_mgr_get_sta_p2p_conn_info(pm_ctx, info);

	switch (num_connections) {
	case 1:
		break;
	case 2:
		if (info[0].freq == info[1].freq &&
		    policy_mgr_are_2_freq_on_same_mac(psoc, info[0].freq,
						      info[1].freq))
			is_scc = true;
		break;
	case 3:
		/*
		 * In DBS/SBS mode 2 freq are different and on different mac.
		 * Thus if any of 2 freq are same that mean one of the MAC is
		 * in SCC.
		 * For non DBS/SBS, if all 3 freq are same then its SCC
		 */
		if ((policy_mgr_is_current_hwmode_dbs(psoc) ||
		     policy_mgr_is_current_hwmode_sbs(psoc)) &&
		    (info[0].freq == info[1].freq ||
		     info[0].freq == info[2].freq ||
		     info[1].freq == info[2].freq))
			is_scc = true;
		else if ((info[0].freq == info[1].freq) &&
			 (info[0].freq == info[2].freq))
			is_scc = true;

		break;
	default:
		policy_mgr_debug("unexpected num_connections value %d",
				 num_connections);
		break;
	}

	return is_scc;
}

bool policy_mgr_current_concurrency_is_mcc(struct wlan_objmgr_psoc *psoc)
{
	uint32_t num_connections = 0;
	bool is_mcc = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return is_mcc;
	}

	num_connections = policy_mgr_get_connection_count(psoc);
	if (!num_connections)
		return is_mcc;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	switch (num_connections) {
	case 1:
		break;
	case 2:
		if (pm_conc_connection_list[0].freq !=
		    pm_conc_connection_list[1].freq &&
		    policy_mgr_are_2_freq_on_same_mac(psoc,
			pm_conc_connection_list[0].freq,
			pm_conc_connection_list[1].freq))
			is_mcc = true;
		break;
	case 3:
		/*
		 * Check if any 2 different freq is on same MAC.
		 * Return true if any of the different freq is on same MAC.
		 */
		if ((pm_conc_connection_list[0].freq !=
		     pm_conc_connection_list[1].freq &&
		     policy_mgr_are_2_freq_on_same_mac(psoc,
			pm_conc_connection_list[0].freq,
			pm_conc_connection_list[1].freq)) ||
		    (pm_conc_connection_list[0].freq !=
		     pm_conc_connection_list[2].freq &&
		     policy_mgr_are_2_freq_on_same_mac(psoc,
			pm_conc_connection_list[0].freq,
			pm_conc_connection_list[2].freq)) ||
		    (pm_conc_connection_list[1].freq !=
		     pm_conc_connection_list[2].freq &&
		     policy_mgr_are_2_freq_on_same_mac(psoc,
			pm_conc_connection_list[1].freq,
			pm_conc_connection_list[2].freq)))
			is_mcc = true;
		break;
	default:
		policy_mgr_debug("unexpected num_connections value %d",
				 num_connections);
		break;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return is_mcc;
}

bool policy_mgr_is_sap_p2pgo_on_dfs(struct wlan_objmgr_psoc *psoc)
{
	int index, count;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct policy_mgr_psoc_priv_obj *pm_ctx = NULL;

	if (psoc)
		pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	index = 0;
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	count = policy_mgr_mode_specific_connection_count(psoc,
							  PM_SAP_MODE,
							  list);
	while (index < count) {
		if (pm_conc_connection_list[list[index]].ch_flagext &
		    (IEEE80211_CHAN_DFS | IEEE80211_CHAN_DFS_CFREQ2)) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return true;
		}
		index++;
	}
	count = policy_mgr_mode_specific_connection_count(psoc,
							  PM_P2P_GO_MODE,
							  list);
	index = 0;
	while (index < count) {
		if (pm_conc_connection_list[list[index]].ch_flagext &
		    (IEEE80211_CHAN_DFS | IEEE80211_CHAN_DFS_CFREQ2)) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return true;
		}
		index++;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	return false;
}

/**
 * policy_mgr_set_concurrency_mode() - To set concurrency mode
 * @psoc: PSOC object data
 * @mode: device mode
 *
 * This routine is called to set the concurrency mode
 *
 * Return: NONE
 */
void policy_mgr_set_concurrency_mode(struct wlan_objmgr_psoc *psoc,
				     enum QDF_OPMODE mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return;
	}

	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_MONITOR_MODE:
		pm_ctx->concurrency_mode |= (1 << mode);
		pm_ctx->no_of_open_sessions[mode]++;
		break;
	default:
		break;
	}

	policy_mgr_debug("concurrency_mode = 0x%x Number of open sessions for mode %d = %d",
			 pm_ctx->concurrency_mode, mode,
		pm_ctx->no_of_open_sessions[mode]);
}

/**
 * policy_mgr_clear_concurrency_mode() - To clear concurrency mode
 * @psoc: PSOC object data
 * @mode: device mode
 *
 * This routine is called to clear the concurrency mode
 *
 * Return: NONE
 */
void policy_mgr_clear_concurrency_mode(struct wlan_objmgr_psoc *psoc,
				       enum QDF_OPMODE mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return;
	}

	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_MONITOR_MODE:
		pm_ctx->no_of_open_sessions[mode]--;
		if (!(pm_ctx->no_of_open_sessions[mode]))
			pm_ctx->concurrency_mode &= (~(1 << mode));
		break;
	default:
		break;
	}

	policy_mgr_debug("concurrency_mode = 0x%x Number of open sessions for mode %d = %d",
			 pm_ctx->concurrency_mode, mode,
			 pm_ctx->no_of_open_sessions[mode]);
}

/**
 * policy_mgr_validate_conn_info() - validate conn info list
 * @psoc: PSOC object data
 *
 * This function will check connection list to see duplicated
 * vdev entry existing or not.
 *
 * Return: true if conn list is in abnormal state.
 */
static bool
policy_mgr_validate_conn_info(struct wlan_objmgr_psoc *psoc)
{
	uint32_t i, j, conn_num = 0;
	bool panic = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return true;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].in_use) {
			for (j = i + 1; j < MAX_NUMBER_OF_CONC_CONNECTIONS;
									j++) {
				if (pm_conc_connection_list[j].in_use &&
				    pm_conc_connection_list[i].vdev_id ==
				    pm_conc_connection_list[j].vdev_id) {
					policy_mgr_debug(
					"dup entry %d",
					pm_conc_connection_list[i].vdev_id);
					panic = true;
				}
			}
			conn_num++;
		}
	}
	if (panic)
		policy_mgr_err("dup entry");

	for (i = 0, j = 0; i < QDF_MAX_NO_OF_MODE; i++)
		j += pm_ctx->no_of_active_sessions[i];

	if (j != conn_num) {
		policy_mgr_err("active session/conn count mismatch %d %d",
			       j, conn_num);
		panic = true;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (panic)
		policy_mgr_debug_alert();

	return panic;
}

#ifdef WLAN_FEATURE_11BE_MLO
bool policy_mgr_is_ml_vdev_id(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	bool is_mlo = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev)
		return is_mlo;

	if (wlan_vdev_mlme_is_mlo_vdev(vdev))
		is_mlo = true;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return is_mlo;
}

/*
 * policy_mgr_get_ml_sta_info() - Get number of ML STA vdev ids and freq list
 * @pm_ctx: pm_ctx ctx
 * @num_ml_sta: Return number of ML STA present
 * @num_disabled_ml_sta: Return number of disabled ML STA links
 * @ml_vdev_lst: Return ML STA vdev id list
 * @ml_freq_lst: Return ML STA freq list
 * @num_non_ml: Return number of non-ML STA present
 * @non_ml_vdev_lst: Return non-ML STA vdev id list
 * @non_ml_freq_lst: Return non-ML STA freq list
 *
 * Return: void
 */
static void
policy_mgr_get_ml_sta_info(struct policy_mgr_psoc_priv_obj *pm_ctx,
			   uint8_t *num_ml_sta,
			   uint8_t *num_disabled_ml_sta,
			   uint8_t *ml_vdev_lst,
			   qdf_freq_t *ml_freq_lst,
			   uint8_t *num_non_ml,
			   uint8_t *non_ml_vdev_lst,
			   qdf_freq_t *non_ml_freq_lst)
{
	struct wlan_objmgr_vdev *vdev;
	uint8_t vdev_id, conn_index;
	qdf_freq_t freq;

	*num_ml_sta = 0;
	*num_disabled_ml_sta = 0;
	if (num_non_ml)
		*num_non_ml = 0;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (!pm_conc_connection_list[conn_index].in_use)
			continue;
		if (pm_conc_connection_list[conn_index].mode != PM_STA_MODE)
			continue;
		vdev_id = pm_conc_connection_list[conn_index].vdev_id;
		freq = pm_conc_connection_list[conn_index].freq;

		/* add ml sta vdev and freq list */
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(pm_ctx->psoc,
							    vdev_id,
							    WLAN_POLICY_MGR_ID);
		if (!vdev) {
			policy_mgr_err("invalid vdev for id %d", vdev_id);
			continue;
		}

		if (wlan_vdev_mlme_is_mlo_vdev(vdev)) {
			ml_vdev_lst[*num_ml_sta] = vdev_id;
			ml_freq_lst[(*num_ml_sta)++] = freq;
		} else if (num_non_ml) {
			if (non_ml_vdev_lst)
				non_ml_vdev_lst[*num_non_ml] = vdev_id;
			if (non_ml_freq_lst)
				non_ml_freq_lst[*num_non_ml] = freq;
			(*num_non_ml)++;
		}
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	}
	/* Get disabled link info as well and keep it at last */
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_DISABLE_LINK;
	     conn_index++) {
		if (!pm_disabled_ml_links[conn_index].in_use)
			continue;
		if (pm_disabled_ml_links[conn_index].mode != PM_STA_MODE)
			continue;
		ml_vdev_lst[*num_ml_sta] =
				pm_disabled_ml_links[conn_index].vdev_id;
		ml_freq_lst[(*num_ml_sta)++] =
			pm_disabled_ml_links[conn_index].freq;
		(*num_disabled_ml_sta)++;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
}

void
policy_mgr_get_ml_sta_info_psoc(struct wlan_objmgr_psoc *psoc,
				uint8_t *num_ml_sta,
				uint8_t *num_disabled_ml_sta,
				uint8_t *ml_vdev_lst,
				qdf_freq_t *ml_freq_lst,
				uint8_t *num_non_ml,
				uint8_t *non_ml_vdev_lst,
				qdf_freq_t *non_ml_freq_lst)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm_ctx");
		return;
	}

	return policy_mgr_get_ml_sta_info(pm_ctx,
					  num_ml_sta,
					  num_disabled_ml_sta,
					  ml_vdev_lst,
					  ml_freq_lst,
					  num_non_ml,
					  non_ml_vdev_lst,
					  non_ml_freq_lst);
}

uint32_t policy_mgr_get_disabled_ml_links_count(struct wlan_objmgr_psoc *psoc)
{
	uint32_t i, count = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm_ctx");
		return count;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_DISABLE_LINK; i++) {
		if (pm_disabled_ml_links[i].in_use)
			count++;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

static QDF_STATUS
policy_mgr_delete_from_disabled_links(struct policy_mgr_psoc_priv_obj *pm_ctx,
				      uint8_t vdev_id)
{
	int i;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_DISABLE_LINK; i++) {
		if (pm_disabled_ml_links[i].in_use &&
		    pm_disabled_ml_links[i].vdev_id == vdev_id) {
			pm_disabled_ml_links[i].in_use = false;
			policy_mgr_debug("Disabled link removed for vdev %d",
					 vdev_id);
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	/* Return failure if not found */
	if (i >= MAX_NUMBER_OF_DISABLE_LINK)
		return QDF_STATUS_E_EXISTS;

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_move_vdev_from_disabled_to_connection_tbl(
						struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	QDF_STATUS status;
	enum QDF_OPMODE mode;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm_ctx");
		return;
	}
	mode = wlan_get_opmode_from_vdev_id(pm_ctx->pdev, vdev_id);
	if (mode != QDF_STA_MODE) {
		policy_mgr_err("vdev %d opmode %d is not STA", vdev_id, mode);
		return;
	}

	if (!policy_mgr_is_ml_vdev_id(psoc, vdev_id)) {
		policy_mgr_err("vdev %d is not ML", vdev_id);
		return;
	}

	status = policy_mgr_delete_from_disabled_links(pm_ctx, vdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_debug("Disabled link not found for vdev %d",
				 vdev_id);
		return;
	}

	/*
	 * Add entry to pm_conc_connection_list if remove from disabled links
	 * was success
	 */
	policy_mgr_incr_active_session(psoc, mode, vdev_id);
}

static QDF_STATUS
policy_mgr_add_to_disabled_links(struct policy_mgr_psoc_priv_obj *pm_ctx,
				 qdf_freq_t freq, enum QDF_OPMODE mode,
				 uint8_t vdev_id)
{
	int i;
	enum policy_mgr_con_mode pm_mode;

	pm_mode = policy_mgr_qdf_opmode_to_pm_con_mode(pm_ctx->psoc, mode,
						       vdev_id);

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_DISABLE_LINK; i++) {
		if (pm_disabled_ml_links[i].in_use &&
		    pm_disabled_ml_links[i].vdev_id == vdev_id)
			break;
	}

	if (i < MAX_NUMBER_OF_DISABLE_LINK) {
		pm_disabled_ml_links[i].freq = freq;
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
		policy_mgr_debug("Disabled link already present vdev %d, pm_mode %d, update freq %d",
				 vdev_id, pm_mode, freq);

		return QDF_STATUS_E_EXISTS;
	}

	for (i = 0; i < MAX_NUMBER_OF_DISABLE_LINK; i++) {
		if (!pm_disabled_ml_links[i].in_use) {
			/* add in empty place */
			pm_disabled_ml_links[i].vdev_id = vdev_id;
			pm_disabled_ml_links[i].mode = pm_mode;
			pm_disabled_ml_links[i].in_use = true;
			pm_disabled_ml_links[i].freq = freq;
			policy_mgr_debug("Disabled link added vdev id: %d freq: %d pm_mode %d",
					 vdev_id, freq, pm_mode);
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	if (i >= MAX_NUMBER_OF_DISABLE_LINK) {
		policy_mgr_err("No empty entry found to disable link for vdev %d",
			       vdev_id);
		return QDF_STATUS_E_RESOURCES;
	}

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_move_vdev_from_connection_to_disabled_tbl(
						struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	qdf_freq_t freq;
	enum QDF_OPMODE mode;
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm_ctx");
		return;
	}

	mode = wlan_get_opmode_from_vdev_id(pm_ctx->pdev, vdev_id);
	if (mode != QDF_STA_MODE) {
		policy_mgr_err("vdev %d opmode %d is not STA", vdev_id, mode);
		return;
	}

	if (!policy_mgr_is_ml_vdev_id(psoc, vdev_id)) {
		policy_mgr_err("vdev %d is not ML", vdev_id);
		return;
	}
	freq = wlan_get_operation_chan_freq_vdev_id(pm_ctx->pdev, vdev_id);
	status = policy_mgr_check_conn_with_mode_and_vdev_id(psoc, PM_STA_MODE,
							     vdev_id);
	/*
	 * Remove entry if present in pm_conc_connection_list, if not just add
	 * it in disabled table.
	 */
	if (QDF_IS_STATUS_SUCCESS(status))
		policy_mgr_decr_session_set_pcl(psoc, mode, vdev_id);
	else
		policy_mgr_debug("Connection tbl dont have vdev %d in STA mode, Add it in disabled tbl",
				 vdev_id);

	policy_mgr_add_to_disabled_links(pm_ctx, freq, mode, vdev_id);
	policy_mgr_dump_current_concurrency(psoc);
}

static bool
policy_mgr_vdev_disabled_by_link_force(struct wlan_objmgr_psoc *psoc,
				       struct wlan_objmgr_vdev *vdev,
				       bool peer_assoc)
{
	uint16_t dynamic_inactive = 0, forced_inactive = 0;
	uint16_t link_id;

	if (ml_is_nlink_service_supported(psoc) &&
	    !peer_assoc) {
		ml_nlink_get_dynamic_inactive_links(psoc, vdev,
						    &dynamic_inactive,
						    &forced_inactive);
		link_id = wlan_vdev_get_link_id(vdev);
		if ((forced_inactive | dynamic_inactive) &
		    (1 << link_id)) {
			policy_mgr_debug("vdev %d linkid %d is forced inactived 0x%0x dyn 0x%x",
					 wlan_vdev_get_id(vdev),
					 link_id, forced_inactive,
					 dynamic_inactive);
			return true;
		}
	}

	return false;
}

bool
policy_mgr_ml_link_vdev_need_to_be_disabled(struct wlan_objmgr_psoc *psoc,
					    struct wlan_objmgr_vdev *vdev,
					    bool peer_assoc)
{
	union conc_ext_flag conc_ext_flags;

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		return false;

	/* Check only for link vdev */
	if (!wlan_vdev_mlme_is_mlo_vdev(vdev) ||
	    !wlan_vdev_mlme_is_mlo_link_vdev(vdev))
		return false;

	/* Check vdev is disabled by link force command */
	if (policy_mgr_vdev_disabled_by_link_force(psoc, vdev,
						   peer_assoc))
		return true;

	conc_ext_flags.value = policy_mgr_get_conc_ext_flags(vdev, false);
	/*
	 * For non-assoc link vdev set link as disabled if concurrency is
	 * not allowed
	 */
	return !policy_mgr_is_concurrency_allowed(psoc, PM_STA_MODE,
					wlan_get_operation_chan_freq(vdev),
					HW_MODE_20_MHZ,
					conc_ext_flags.value, NULL);
}

static void
policy_mgr_enable_disable_link_from_vdev_bitmask(struct wlan_objmgr_psoc *psoc,
						 unsigned long enable_vdev_mask,
						 unsigned long disable_vdev_mask,
						 uint8_t start_vdev_id)
{
	uint8_t i;

	/* Enable required link if enable_vdev_mask preset */
	for (i = 0; enable_vdev_mask && i < WLAN_MAX_VDEVS; i++) {
		if (qdf_test_and_clear_bit(i, &enable_vdev_mask))
			policy_mgr_move_vdev_from_disabled_to_connection_tbl(
							psoc,
							i + start_vdev_id);
	}

	/* Disable required link if disable_mask preset */
	for (i = 0; disable_vdev_mask && i < WLAN_MAX_VDEVS; i++) {
		if (qdf_test_and_clear_bit(i, &disable_vdev_mask))
			policy_mgr_move_vdev_from_connection_to_disabled_tbl(
							psoc,
							i + start_vdev_id);
	}
}

static void
policy_mgr_set_link_in_progress(struct policy_mgr_psoc_priv_obj *pm_ctx,
				bool set_link_in_progress)
{
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (set_link_in_progress)
		qdf_atomic_inc(&pm_ctx->link_in_progress);
	else {
		if (qdf_atomic_read(&pm_ctx->link_in_progress) > 0)
			qdf_atomic_dec(&pm_ctx->link_in_progress);
	}

	/* if set link has started reset the event, else complete the event */
	if (qdf_atomic_read(&pm_ctx->link_in_progress))
		qdf_event_reset(&pm_ctx->set_link_update_done_evt);
	else
		qdf_event_set(&pm_ctx->set_link_update_done_evt);

	policy_mgr_debug("link_in_progress %d",
			 qdf_atomic_read(&pm_ctx->link_in_progress));
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
}

static bool
policy_mgr_get_link_in_progress(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	bool value;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	value = qdf_atomic_read(&pm_ctx->link_in_progress);
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	if (value)
		policy_mgr_debug("set_link_in_progress %d", value);

	return value;
}

bool policy_mgr_is_set_link_in_progress(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	return policy_mgr_get_link_in_progress(pm_ctx);
}

/*
 * policy_mgr_trigger_roam_on_link_removal() - Trigger roam on link removal
 * @vdev: vdev object
 *
 * In multilink ML STA, if one link is removed by AP, and no other active
 * link, trigger roam by roaming invoke command.
 *
 * Return: void
 */
static void
policy_mgr_trigger_roam_on_link_removal(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *ml_vdev;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t num_ml_sta = 0, num_disabled_ml_sta = 0;
	uint8_t num_active_ml_sta;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	uint8_t assoc_vdev_id = WLAN_INVALID_VDEV_ID;
	uint8_t removed_vdev_id = WLAN_INVALID_VDEV_ID;
	struct qdf_mac_addr bssid;
	QDF_STATUS status;
	bool ml_sta_is_not_connected = false;
	uint32_t i;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		policy_mgr_err("Failed to get psoc");
		return;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		policy_mgr_err("Failed to get pdev");
		return;
	}
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	policy_mgr_get_ml_sta_info(pm_ctx, &num_ml_sta, &num_disabled_ml_sta,
				   ml_sta_vdev_lst, ml_freq_lst,
				   NULL, NULL, NULL);
	if (!num_ml_sta) {
		policy_mgr_debug("unexpected event, no ml sta");
		return;
	}
	if (num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_disabled_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_ml_sta <= num_disabled_ml_sta) {
		policy_mgr_debug("unexpected ml sta num %d %d",
				 num_ml_sta, num_disabled_ml_sta);
		return;
	}
	num_active_ml_sta = num_ml_sta;
	if (num_ml_sta >= num_disabled_ml_sta)
		num_active_ml_sta = num_ml_sta - num_disabled_ml_sta;

	for (i = 0; i < num_active_ml_sta; i++) {
		if (!wlan_get_vdev_link_removed_flag_by_vdev_id(
					psoc, ml_sta_vdev_lst[i]))
			break;
	}

	/* After link removal, one link is still active, no need invoke
	 * roaming.
	 * For Single link MLO, FW will do roaming automatically.
	 */
	if (i < num_active_ml_sta || num_ml_sta < 2)
		return;

	/* For multi-link MLO STA, if one link is removed and no other active
	 * link, then trigger roaming. the other link may have concurrency
	 * limitation and can't be active.
	 */
	for (i = 0; i < num_ml_sta; i++) {
		if (removed_vdev_id == WLAN_INVALID_VDEV_ID &&
		    wlan_get_vdev_link_removed_flag_by_vdev_id(
			psoc, ml_sta_vdev_lst[i])) {
			policy_mgr_debug("removal link vdev %d is removed ",
					 vdev_id);
			removed_vdev_id = ml_sta_vdev_lst[i];
		}
		ml_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
						pm_ctx->psoc,
						ml_sta_vdev_lst[i],
						WLAN_POLICY_MGR_ID);
		if (!ml_vdev) {
			policy_mgr_err("invalid vdev for id %d",
				       ml_sta_vdev_lst[i]);
			continue;
		}
		if (!wlan_cm_is_vdev_connected(ml_vdev)) {
			policy_mgr_debug("ml sta vdev %d is not connected state",
					 ml_sta_vdev_lst[i]);
			ml_sta_is_not_connected = true;
		}

		wlan_objmgr_vdev_release_ref(ml_vdev, WLAN_POLICY_MGR_ID);

		if (assoc_vdev_id == WLAN_INVALID_VDEV_ID &&
		    !wlan_vdev_mlme_get_is_mlo_link(psoc,
						    ml_sta_vdev_lst[i]))
			assoc_vdev_id = ml_sta_vdev_lst[i];
	}
	if (removed_vdev_id == WLAN_INVALID_VDEV_ID) {
		policy_mgr_debug("no link removed, unexpected");
		return;
	}
	if (assoc_vdev_id == WLAN_INVALID_VDEV_ID) {
		policy_mgr_debug("no find assoc vdev, unexpected");
		return;
	}
	if (ml_sta_is_not_connected) {
		policy_mgr_debug("ml sta is non-connected state, don't trigger roam");
		return;
	}
	/* trigger roaming */
	policy_mgr_debug("link removal detected, try roaming on vdev id: %d",
			 assoc_vdev_id);
	qdf_zero_macaddr(&bssid);
	status = wlan_cm_roam_invoke(pdev, assoc_vdev_id, &bssid, 0,
				     CM_ROAMING_LINK_REMOVAL);
	if (QDF_IS_STATUS_ERROR(status))
		policy_mgr_err("roam invoke failed");
}

/*
 * policy_mgr_update_dynamic_inactive_bitmap() - Update dynamic inactive links
 * @psoc: psoc object
 * @vdev: vdev object
 * @req: req of set link command
 * @resp: resp of set link command
 *
 * If MLO_LINK_FORCE_MODE_INACTIVE_NUM force mode with control flag -
 * "dynamic_force_link_num" enabled was sent to firmware,
 * host will need to select same num of links to be dyamic inactive
 * links. And move corresponding vdevs to disabled policy mgr connection table.
 *
 * Return: void
 */
static void
policy_mgr_update_dynamic_inactive_bitmap(
			struct wlan_objmgr_psoc *psoc,
			struct wlan_objmgr_vdev *vdev,
			struct mlo_link_set_active_req *req,
			struct mlo_link_set_active_resp *resp)
{
	uint32_t candidate_inactive_links, inactive_links;
	uint32_t standby_links;
	uint32_t dyn_inactive_links = 0;
	uint8_t dyn_num = 0, num = 0, i;
	uint8_t link_ids[MAX_MLO_LINK_ID * 2];
	struct ml_link_force_state curr_state = {0};
	uint8_t force_inactive_num = 0;
	uint32_t force_inactive_num_bitmap = 0;
	uint32_t force_inactive_bitmap;

	ml_nlink_get_curr_force_state(psoc, vdev, &curr_state);

	switch (req->param.force_mode) {
	case MLO_LINK_FORCE_MODE_ACTIVE:
	case MLO_LINK_FORCE_MODE_INACTIVE:
	case MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE:
	case MLO_LINK_FORCE_MODE_ACTIVE_NUM:
	case MLO_LINK_FORCE_MODE_NO_FORCE:
		if (!curr_state.curr_dynamic_inactive_bitmap)
			return;

		force_inactive_num = curr_state.force_inactive_num;
		force_inactive_num_bitmap =
			curr_state.force_inactive_num_bitmap;
		force_inactive_bitmap = curr_state.force_inactive_bitmap;
		break;
	case MLO_LINK_FORCE_MODE_INACTIVE_NUM:
		if (!req->param.control_flags.dynamic_force_link_num) {
			if (!curr_state.curr_dynamic_inactive_bitmap)
				return;
			dyn_inactive_links = 0;
			dyn_num = 0;
			goto update;
		}

		force_inactive_num = curr_state.force_inactive_num;
		force_inactive_num_bitmap =
			curr_state.force_inactive_num_bitmap;
		force_inactive_bitmap = curr_state.force_inactive_bitmap;
		break;
	default:
		return;
	}

	/* force inactive num "clear" case, return 0 - no
	 * dynamic inactive links.
	 */
	if (!force_inactive_num) {
		dyn_inactive_links = 0;
		dyn_num = 0;
		goto update;
	}

	/* 1. If standby link is force inactive,
	 * select the standby link as dynamic inactive link firstly.
	 */
	standby_links = ml_nlink_get_standby_link_bitmap(psoc, vdev);
	candidate_inactive_links =
		force_inactive_num_bitmap &
		force_inactive_bitmap &
		standby_links;
	inactive_links = candidate_inactive_links;
	num = ml_nlink_convert_link_bitmap_to_ids(candidate_inactive_links,
						  QDF_ARRAY_SIZE(link_ids),
						  link_ids);

	/* 2. If non standby link is force inactive,
	 *  select the non standby link as second option.
	 */
	if (num < force_inactive_num &&
	    num < QDF_ARRAY_SIZE(link_ids)) {
		candidate_inactive_links =
			force_inactive_num_bitmap &
			force_inactive_bitmap &
			~inactive_links;
		num += ml_nlink_convert_link_bitmap_to_ids(
				candidate_inactive_links,
				QDF_ARRAY_SIZE(link_ids) - num,
				&link_ids[num]);
		inactive_links |= candidate_inactive_links;
	}

	/* 3. If standby link is present (may not be force inactive),
	 * select the standby link as dynamic inactive link.
	 */
	if (num < force_inactive_num &&
	    num < QDF_ARRAY_SIZE(link_ids)) {
		candidate_inactive_links =
			force_inactive_num_bitmap &
			standby_links;
		num += ml_nlink_convert_link_bitmap_to_ids(
				candidate_inactive_links,
				QDF_ARRAY_SIZE(link_ids) - num,
				&link_ids[num]);
		inactive_links |= candidate_inactive_links;
	}

	/* 4. If there are links inactive from fw event's
	 * curr_inactive_linkid_bitmap, select the current inactive
	 * links as last option. note: those link may not be force
	 * inactive.
	 */
	if (num < force_inactive_num &&
	    num < QDF_ARRAY_SIZE(link_ids)) {
		candidate_inactive_links =
			force_inactive_num_bitmap &
			resp->curr_inactive_linkid_bitmap &
			~inactive_links;
		num += ml_nlink_convert_link_bitmap_to_ids(
				candidate_inactive_links,
				QDF_ARRAY_SIZE(link_ids) - num,
				&link_ids[num]);
		inactive_links |= candidate_inactive_links;
	}

	for (i = 0; i < num; i++) {
		if (dyn_num >= force_inactive_num)
			break;
		dyn_inactive_links |= 1 << link_ids[i];
		dyn_num++;
	}

update:
	policy_mgr_debug("inactive link num %d bitmap 0x%x force inactive 0x%x curr 0x%x dyn links 0x%x num %d",
			 force_inactive_num,
			 force_inactive_num_bitmap,
			 resp->inactive_linkid_bitmap,
			 resp->curr_inactive_linkid_bitmap,
			 dyn_inactive_links, dyn_num);

	ml_nlink_set_dynamic_inactive_links(psoc, vdev, dyn_inactive_links);
}

static void
policy_mgr_handle_vdev_active_inactive_resp(
					struct wlan_objmgr_psoc *psoc,
					struct wlan_objmgr_vdev *vdev,
					struct mlo_link_set_active_req *req,
					struct mlo_link_set_active_resp *resp)
{
	uint8_t i;
	uint8_t vdev_id_num = 0;
	uint8_t vdev_ids[WLAN_MLO_MAX_VDEVS] = {0};
	uint32_t assoc_bitmap = 0;
	uint16_t dynamic_inactive_bitmap = 0;
	uint16_t forced_inactive_bitmap = 0;

	/* convert link id to vdev id and update vdev status based
	 * on both inactive and active bitmap.
	 * In link bitmap based WMI event (use_ieee_link_id = true),
	 * target will always indicate current force inactive and
	 * active bitmaps to host. For links in inactive_linkid_bitmap,
	 * they will be moved to policy mgr disable connection table.
	 * for other links, they will be in active tables.
	 */
	ml_nlink_get_dynamic_inactive_links(psoc, vdev,
					    &dynamic_inactive_bitmap,
					    &forced_inactive_bitmap);
	resp->inactive_linkid_bitmap |= dynamic_inactive_bitmap;
	ml_nlink_convert_linkid_bitmap_to_vdev_bitmap(
		psoc, vdev, resp->inactive_linkid_bitmap,
		&assoc_bitmap,
		&resp->inactive_sz, resp->inactive,
		&vdev_id_num, vdev_ids);

	ml_nlink_convert_linkid_bitmap_to_vdev_bitmap(
		psoc, vdev,
		(~resp->inactive_linkid_bitmap) & assoc_bitmap,
		NULL,
		&resp->active_sz, resp->active,
		&vdev_id_num, vdev_ids);
	for (i = 0; i < resp->inactive_sz; i++)
		policy_mgr_enable_disable_link_from_vdev_bitmask(
				psoc, 0, resp->inactive[i], i * 32);
	for (i = 0; i < resp->active_sz; i++)
		policy_mgr_enable_disable_link_from_vdev_bitmask(
				psoc, resp->active[i], 0, i * 32);
}

static void
policy_mgr_handle_force_active_resp(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_vdev *vdev,
				    struct mlo_link_set_active_req *req,
				    struct mlo_link_set_active_resp *resp)
{
	uint8_t i;
	uint32_t assoc_bitmap = 0;

	if (resp->use_ieee_link_id) {
		/* save link force active bitmap */
		ml_nlink_set_curr_force_active_state(
			psoc, vdev, req->param.force_cmd.ieee_link_id_bitmap,
			req->param.control_flags.overwrite_force_active_bitmap ?
			LINK_OVERWRITE : LINK_ADD);

		policy_mgr_update_dynamic_inactive_bitmap(psoc, vdev, req,
							  resp);

		/* update vdev active inactive status */
		policy_mgr_handle_vdev_active_inactive_resp(psoc, vdev, req,
							    resp);
		return;
	}
	ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
		psoc, vdev, req->param.num_vdev_bitmap,
		req->param.vdev_bitmap, &resp->active_linkid_bitmap,
		&assoc_bitmap);
	ml_nlink_set_curr_force_active_state(
		psoc, vdev, resp->active_linkid_bitmap, LINK_ADD);

	for (i = 0; i < resp->active_sz; i++)
		policy_mgr_enable_disable_link_from_vdev_bitmask(
				psoc, resp->active[i], 0, i * 32);
}

static void
policy_mgr_handle_force_inactive_resp(struct wlan_objmgr_psoc *psoc,
				      struct wlan_objmgr_vdev *vdev,
				      struct mlo_link_set_active_req *req,
				      struct mlo_link_set_active_resp *resp)
{
	uint8_t i;
	uint32_t assoc_bitmap = 0;

	if (resp->use_ieee_link_id) {
		/* save link force inactive bitmap */
		ml_nlink_set_curr_force_inactive_state(
			psoc, vdev, req->param.force_cmd.ieee_link_id_bitmap,
			req->param.control_flags.overwrite_force_inactive_bitmap ?
			LINK_OVERWRITE : LINK_ADD);

		policy_mgr_update_dynamic_inactive_bitmap(psoc, vdev, req,
							  resp);

		/* update vdev active inactive status */
		policy_mgr_handle_vdev_active_inactive_resp(psoc, vdev, req,
							    resp);
		return;
	}
	ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
		psoc, vdev, req->param.num_vdev_bitmap,
		req->param.vdev_bitmap, &resp->inactive_linkid_bitmap,
		&assoc_bitmap);
	ml_nlink_set_curr_force_inactive_state(
		psoc, vdev, resp->inactive_linkid_bitmap, LINK_ADD);

	for (i = 0; i < resp->inactive_sz; i++)
		policy_mgr_enable_disable_link_from_vdev_bitmask(
				psoc, 0, resp->inactive[i], i * 32);
}

static void
policy_mgr_handle_force_active_num_resp(struct wlan_objmgr_psoc *psoc,
					struct wlan_objmgr_vdev *vdev,
					struct mlo_link_set_active_req *req,
					struct mlo_link_set_active_resp *resp)
{
	uint8_t i;
	uint32_t assoc_bitmap = 0;
	uint32_t link_bitmap = 0;

	if (resp->use_ieee_link_id) {
		/* save force num and force num bitmap */
		ml_nlink_set_curr_force_active_num_state(
			psoc, vdev, req->param.force_cmd.link_num,
			req->param.force_cmd.ieee_link_id_bitmap);

		policy_mgr_update_dynamic_inactive_bitmap(psoc, vdev, req,
							  resp);

		/* update vdev active inactive status */
		policy_mgr_handle_vdev_active_inactive_resp(psoc, vdev, req,
							    resp);
		return;
	}
	ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
		psoc, vdev, req->param.num_vdev_bitmap,
		req->param.vdev_bitmap,
		&link_bitmap,
		&assoc_bitmap);
	ml_nlink_set_curr_force_active_num_state(
		psoc, vdev, req->param.link_num[0].num_of_link,
		link_bitmap);
	/*
	 * When the host sends a set link command with force link num
	 * and dynamic flag set, FW may not process it immediately.
	 * In this case FW buffer the request and sends a response as
	 * success to the host with VDEV bitmap as zero.
	 * FW ensures that the number of active links will be equal to
	 * the link num sent via WMI_MLO_LINK_SET_ACTIVE_CMDID command.
	 * So the host should also fill the mlo policy_mgr table as per
	 * request.
	 */
	if (req->param.control_flags.dynamic_force_link_num) {
		policy_mgr_debug("Enable ML vdev(s) as sent in req");
		for (i = 0; i < req->param.num_vdev_bitmap; i++)
			policy_mgr_enable_disable_link_from_vdev_bitmask(
				psoc,
				req->param.vdev_bitmap[i], 0, i * 32);
		return;
	}

	/*
	 * MLO_LINK_FORCE_MODE_ACTIVE_NUM return which vdev is active
	 * So XOR of the requested ML vdev and active vdev bit will give
	 * the vdev bits to disable
	 */
	for (i = 0; i < resp->active_sz; i++)
		policy_mgr_enable_disable_link_from_vdev_bitmask(
			psoc, resp->active[i],
			resp->active[i] ^ req->param.vdev_bitmap[i],
			i * 32);
}

static void
policy_mgr_handle_force_inactive_num_resp(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct mlo_link_set_active_req *req,
				struct mlo_link_set_active_resp *resp)
{
	uint8_t i;
	uint32_t assoc_bitmap = 0;
	uint32_t link_bitmap = 0;

	if (resp->use_ieee_link_id) {
		/* save force num and force num bitmap */
		ml_nlink_set_curr_force_inactive_num_state(
			psoc, vdev, req->param.force_cmd.link_num,
			req->param.force_cmd.ieee_link_id_bitmap);
		policy_mgr_update_dynamic_inactive_bitmap(psoc, vdev, req,
							  resp);

		/* update vdev active inactive status */
		policy_mgr_handle_vdev_active_inactive_resp(psoc, vdev, req,
							    resp);
		return;
	}
	ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
		psoc, vdev, req->param.num_vdev_bitmap,
		req->param.vdev_bitmap,
		&link_bitmap,
		&assoc_bitmap);
	ml_nlink_set_curr_force_inactive_num_state(
		psoc, vdev, req->param.link_num[0].num_of_link,
		link_bitmap);

	/*
	 * MLO_LINK_FORCE_MODE_INACTIVE_NUM return which vdev is
	 * inactive So XOR of the requested ML vdev and inactive vdev
	 * bit will give the vdev bits to be enable.
	 */
	for (i = 0; i < resp->inactive_sz; i++)
		policy_mgr_enable_disable_link_from_vdev_bitmask(
			psoc,
			resp->inactive[i] ^ req->param.vdev_bitmap[i],
			resp->inactive[i], i * 32);
}

static void
policy_mgr_handle_force_active_inactive_resp(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct mlo_link_set_active_req *req,
				struct mlo_link_set_active_resp *resp)
{
	uint8_t i;
	uint32_t assoc_bitmap = 0;

	if (resp->use_ieee_link_id) {
		/* save link active/inactive bitmap */
		ml_nlink_set_curr_force_active_state(
			psoc, vdev, req->param.force_cmd.ieee_link_id_bitmap,
			req->param.control_flags.overwrite_force_active_bitmap ?
			LINK_OVERWRITE : LINK_ADD);
		ml_nlink_set_curr_force_inactive_state(
			psoc, vdev, req->param.force_cmd.ieee_link_id_bitmap2,
			req->param.control_flags.overwrite_force_inactive_bitmap ?
			LINK_OVERWRITE : LINK_ADD);

		policy_mgr_update_dynamic_inactive_bitmap(psoc, vdev, req,
							  resp);

		/* update vdev active inactive status */
		policy_mgr_handle_vdev_active_inactive_resp(psoc, vdev, req,
							    resp);
		return;
	}
	ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
		psoc, vdev, req->param.num_inactive_vdev_bitmap,
		req->param.inactive_vdev_bitmap,
		&resp->inactive_linkid_bitmap,
		&assoc_bitmap);
	ml_nlink_set_curr_force_inactive_state(
		psoc, vdev, resp->inactive_linkid_bitmap, LINK_ADD);
	ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
		psoc, vdev, req->param.num_vdev_bitmap,
		req->param.vdev_bitmap,
		&resp->active_linkid_bitmap,
		&assoc_bitmap);
	ml_nlink_set_curr_force_active_state(
		psoc, vdev, resp->active_linkid_bitmap, LINK_ADD);

	for (i = 0; i < resp->inactive_sz; i++)
		policy_mgr_enable_disable_link_from_vdev_bitmask(
				psoc, 0, resp->inactive[i], i * 32);
	for (i = 0; i < resp->active_sz; i++)
		policy_mgr_enable_disable_link_from_vdev_bitmask(
				psoc, resp->active[i], 0, i * 32);
}

static void
policy_mgr_handle_no_force_resp(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct mlo_link_set_active_req *req,
				struct mlo_link_set_active_resp *resp)
{
	uint8_t i;
	uint32_t assoc_bitmap = 0;
	uint32_t link_bitmap = 0;

	if (resp->use_ieee_link_id) {
		/* update link inactive/active bitmap */
		if (req->param.force_cmd.ieee_link_id_bitmap) {
			ml_nlink_set_curr_force_inactive_state(
				psoc, vdev,
				req->param.force_cmd.ieee_link_id_bitmap,
				LINK_CLR);
			ml_nlink_set_curr_force_active_state(
				psoc, vdev,
				req->param.force_cmd.ieee_link_id_bitmap,
				LINK_CLR);
		} else {
			/* special handling for no force to clear all */
			ml_nlink_clr_force_state(psoc, vdev);
		}

		policy_mgr_update_dynamic_inactive_bitmap(psoc, vdev, req,
							  resp);
		/* update vdev active inactive status */
		policy_mgr_handle_vdev_active_inactive_resp(psoc, vdev, req,
							    resp);
		return;
	}

	ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
		psoc, vdev, req->param.num_vdev_bitmap,
		req->param.vdev_bitmap,
		&link_bitmap, &assoc_bitmap);

	ml_nlink_set_curr_force_inactive_state(
		psoc, vdev, link_bitmap, LINK_CLR);
	ml_nlink_set_curr_force_active_state(
		psoc, vdev, link_bitmap, LINK_CLR);
	ml_nlink_set_curr_force_active_num_state(
		psoc, vdev, 0, 0);
	ml_nlink_set_curr_force_inactive_num_state(
		psoc, vdev, 0, 0);

	/* Enable all the ML vdev id sent in request */
	for (i = 0; i < req->param.num_vdev_bitmap; i++)
		policy_mgr_enable_disable_link_from_vdev_bitmask(
			psoc, req->param.vdev_bitmap[i], 0, i * 32);
}

static void
policy_mgr_handle_link_enable_disable_resp(struct wlan_objmgr_vdev *vdev,
					  void *arg,
					  struct mlo_link_set_active_resp *resp)
{
	struct mlo_link_set_active_req *req = arg;
	struct wlan_objmgr_psoc *psoc;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct ml_nlink_change_event data;
	bool reschedule_workqueue = true;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		policy_mgr_err("Psoc is Null");
		return;
	}
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (!req || !resp) {
		policy_mgr_err("arguments or event empty for vdev %d",
			       wlan_vdev_get_id(vdev));
		goto complete_evnt;
	}

	if (resp->status) {
		policy_mgr_err("Set link status %d, for mode %d reason %d vdev bitmask 0x%x",
			       resp->status, req->param.force_mode,
			       req->param.reason, req->param.vdev_bitmap[0]);
		goto complete_evnt;
	}

	policy_mgr_debug("Req mode %d reason %d loop %d bitmask[0] = 0x%x, resp: active %d inactive %d, active[0] 0x%x inactive[0] 0x%x",
			 req->param.force_mode, req->param.reason,
			 req->param.control_flags.post_re_evaluate_loops,
			 req->param.vdev_bitmap[0],
			 resp->active_sz, resp->inactive_sz,
			 resp->active[0], resp->inactive[0]);
	switch (req->param.force_mode) {
	case MLO_LINK_FORCE_MODE_ACTIVE:
		policy_mgr_handle_force_active_resp(psoc, vdev, req, resp);
		break;
	case MLO_LINK_FORCE_MODE_INACTIVE:
		policy_mgr_handle_force_inactive_resp(psoc, vdev, req, resp);
		break;
	case MLO_LINK_FORCE_MODE_ACTIVE_NUM:
		policy_mgr_handle_force_active_num_resp(psoc, vdev, req, resp);
		break;
	case MLO_LINK_FORCE_MODE_INACTIVE_NUM:
		policy_mgr_handle_force_inactive_num_resp(psoc, vdev, req,
							  resp);
		break;
	case MLO_LINK_FORCE_MODE_NO_FORCE:
		policy_mgr_handle_no_force_resp(psoc, vdev, req, resp);
		break;
	case MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE:
		policy_mgr_handle_force_active_inactive_resp(psoc, vdev, req,
							     resp);
		break;
	case MLO_LINK_FORCE_MODE_NON_FORCE_UPDATE:
		break;
	default:
		policy_mgr_err("Invalid request req mode %d",
			       req->param.force_mode);
		break;
	}
	if (req->param.reason == MLO_LINK_FORCE_REASON_LINK_REMOVAL)
		policy_mgr_trigger_roam_on_link_removal(vdev);

complete_evnt:
	policy_mgr_set_link_in_progress(pm_ctx, false);
	if (ml_is_nlink_service_supported(psoc) &&
	    req && resp && !resp->status &&
	    req->param.control_flags.post_re_evaluate) {
		if (req->param.control_flags.post_re_evaluate_loops <
			MAX_RE_EVALUATE_LOOPS) {
			qdf_mem_zero(&data, sizeof(data));
			data.evt.post_set_link.post_re_evaluate_loops++;
			status = ml_nlink_conn_change_notify(
				psoc, wlan_vdev_get_id(vdev),
				ml_nlink_post_set_link_evt, &data);
		} else {
			policy_mgr_err("unexpected post_re_evaluate_loops %d",
				       req->param.control_flags.post_re_evaluate_loops);
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	/* reschedule force scc workqueue after link state changes */
	if (req &&
	    (req->param.control_flags.dont_reschedule_workqueue ||
	     req->param.force_mode == MLO_LINK_FORCE_MODE_NON_FORCE_UPDATE))
		reschedule_workqueue = false;

	if (req && resp && !resp->status &&
	    status == QDF_STATUS_SUCCESS && reschedule_workqueue)
		policy_mgr_check_concurrent_intf_and_restart_sap(psoc, false);
}
#else
static inline QDF_STATUS
policy_mgr_delete_from_disabled_links(struct policy_mgr_psoc_priv_obj *pm_ctx,
				      uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

bool policy_mgr_is_mlo_sta_disconnected(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	bool disconnected;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev)
		return true;
	/* mlo mgr has no corresponding protocol api used in non-osif/hdd
	 * component. Todo: clean up to use internal API
	 */
	disconnected = ucfg_mlo_is_mld_disconnected(vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return disconnected;
}

void policy_mgr_incr_active_session(struct wlan_objmgr_psoc *psoc,
				enum QDF_OPMODE mode,
				uint8_t session_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_6ghz_flag = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	/*
	 * Need to acquire mutex as entire functionality in this function
	 * is in critical section
	 */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_NAN_DISC_MODE:
	case QDF_NDI_MODE:
		pm_ctx->no_of_active_sessions[mode]++;
		break;
	default:
		break;
	}

	if (mode == QDF_NDI_MODE &&
	    pm_ctx->hdd_cbacks.wlan_hdd_indicate_active_ndp_cnt)
		pm_ctx->hdd_cbacks.wlan_hdd_indicate_active_ndp_cnt(
				psoc, session_id,
				pm_ctx->no_of_active_sessions[mode]);

	if (mode != QDF_NAN_DISC_MODE && pm_ctx->dp_cbacks.hdd_v2_flow_pool_map)
		pm_ctx->dp_cbacks.hdd_v2_flow_pool_map(session_id);
	if (mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE)
		policy_mgr_get_ap_6ghz_capable(psoc, session_id,
					       &conn_6ghz_flag);

	policy_mgr_debug("No.# of active sessions for mode %d = %d",
		mode, pm_ctx->no_of_active_sessions[mode]);
	policy_mgr_incr_connection_count(psoc, session_id, mode);

	if ((policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, NULL) > 0) && (mode != QDF_STA_MODE)) {
		/* Send set pcl for all the connected STA vdev */
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
		polic_mgr_send_pcl_to_fw(psoc, mode);
		qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	}

	/* Notify tdls */
	if (pm_ctx->tdls_cbacks.tdls_notify_increment_session)
		pm_ctx->tdls_cbacks.tdls_notify_increment_session(psoc);

	/*
	 * Disable LRO/GRO if P2P or SAP connection has come up or
	 * there are more than one STA connections
	 */
	if ((policy_mgr_mode_specific_connection_count(psoc, PM_STA_MODE, NULL) > 1) ||
	    (policy_mgr_get_beaconing_mode_count(psoc, NULL) > 0) ||
	    (policy_mgr_mode_specific_connection_count(psoc, PM_P2P_CLIENT_MODE, NULL) >
									0) ||
	    (policy_mgr_mode_specific_connection_count(psoc,
						       PM_NDI_MODE,
						       NULL) > 0)) {
		if (pm_ctx->dp_cbacks.hdd_disable_rx_ol_in_concurrency)
			pm_ctx->dp_cbacks.hdd_disable_rx_ol_in_concurrency(true);
	};

	/* Enable RPS if SAP interface has come up */
	if (policy_mgr_get_sap_mode_count(psoc, NULL) == 1) {
		if (pm_ctx->dp_cbacks.hdd_set_rx_mode_rps_cb)
			pm_ctx->dp_cbacks.hdd_set_rx_mode_rps_cb(true);
	}
	if (mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE)
		policy_mgr_init_ap_6ghz_capable(psoc, session_id,
						conn_6ghz_flag);
	if (mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE ||
	    mode == QDF_STA_MODE || mode == QDF_P2P_CLIENT_MODE)
		policy_mgr_update_dfs_master_dynamic_enabled(psoc, false, NULL);

	policy_mgr_dump_current_concurrency(psoc);

	if (policy_mgr_update_indoor_concurrency(psoc, session_id, 0, CONNECT))
		wlan_reg_recompute_current_chan_list(psoc, pm_ctx->pdev);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	if (mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE)
		ml_nlink_conn_change_notify(
			psoc, session_id, ml_nlink_ap_started_evt, NULL);
}

/**
 * policy_mgr_update_sta_scc_info_for_later_check() - function to update sta/sap
 * scc channel frequency and later check flag.
 * @pm_ctx: policy manager context pointer
 * @mode: operation mode
 * @vdev_id: vdev id
 *
 * Return: None
 */
static void policy_mgr_update_sta_scc_info_for_later_check(
		struct policy_mgr_psoc_priv_obj *pm_ctx,
		enum QDF_OPMODE mode,
		uint8_t vdev_id)
{
	uint32_t conn_index = 0;
	qdf_freq_t sta_freq = 0;

	if (mode != QDF_STA_MODE && mode != QDF_P2P_CLIENT_MODE)
		return;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (vdev_id == pm_conc_connection_list[conn_index].vdev_id) {
			sta_freq = pm_conc_connection_list[conn_index].freq;
			break;
		}
		conn_index++;
	}

	if (!sta_freq)
		goto release_mutex;

	/*
	 * When STA disconnected, we need to move DFS SAP
	 * to Non-DFS if g_sta_sap_scc_on_dfs_chan enabled.
	 * The same if g_sta_sap_scc_on_lte_coex_chan enabled,
	 * need to move SAP on unsafe channel to safe channel.
	 * The flag will be checked by
	 * policy_mgr_is_sap_restart_required_after_sta_disconnect.
	 */
	conn_index = 0;
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (pm_conc_connection_list[conn_index].freq == sta_freq &&
		    (pm_conc_connection_list[conn_index].mode == PM_SAP_MODE ||
		    pm_conc_connection_list[conn_index].mode ==
		    PM_P2P_GO_MODE)) {
			pm_ctx->last_disconn_sta_freq = sta_freq;
			break;
		}
		conn_index++;
	}

release_mutex:
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
}

QDF_STATUS policy_mgr_decr_active_session(struct wlan_objmgr_psoc *psoc,
				enum QDF_OPMODE mode,
				uint8_t session_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	QDF_STATUS qdf_status;
	bool mcc_mode;
	uint32_t session_count, cur_freq;
	enum hw_mode_bandwidth max_bw;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
		return QDF_STATUS_E_EMPTY;
	}

	qdf_status = policy_mgr_check_conn_with_mode_and_vdev_id(psoc,
			policy_mgr_qdf_opmode_to_pm_con_mode(psoc, mode,
							     session_id),
			session_id);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		policy_mgr_debug("No connection with mode:%d vdev_id:%d",
			policy_mgr_qdf_opmode_to_pm_con_mode(psoc, mode,
							     session_id),
			session_id);
		/*
		 * In case of disconnect try delete the link from disabled link
		 * as well, if its not present in pm_conc_connection_list,
		 * it can be present in pm_disabled_ml_links.
		 */
		policy_mgr_delete_from_disabled_links(pm_ctx, session_id);
		policy_mgr_dump_current_concurrency(psoc);
		return qdf_status;
	}
	policy_mgr_update_sta_scc_info_for_later_check(pm_ctx,
						       mode,
						       session_id);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_NAN_DISC_MODE:
	case QDF_NDI_MODE:
		if (pm_ctx->no_of_active_sessions[mode])
			pm_ctx->no_of_active_sessions[mode]--;
		break;
	default:
		break;
	}

	policy_mgr_get_chan_by_session_id(psoc, session_id, &cur_freq);

	policy_mgr_decr_connection_count(psoc, session_id);
	session_count = pm_ctx->no_of_active_sessions[mode];
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (mode != QDF_NAN_DISC_MODE &&
	    pm_ctx->dp_cbacks.hdd_v2_flow_pool_unmap)
		pm_ctx->dp_cbacks.hdd_v2_flow_pool_unmap(session_id);

	if (mode == QDF_NDI_MODE &&
	    pm_ctx->hdd_cbacks.wlan_hdd_indicate_active_ndp_cnt)
		pm_ctx->hdd_cbacks.wlan_hdd_indicate_active_ndp_cnt(
				psoc, session_id, session_count);

	policy_mgr_debug("No.# of active sessions for mode %d = %d",
			 mode, session_count);

	/* Notify tdls */
	if (pm_ctx->tdls_cbacks.tdls_notify_decrement_session)
		pm_ctx->tdls_cbacks.tdls_notify_decrement_session(psoc);
	/* Enable LRO/GRO if there no concurrency */
	if ((policy_mgr_get_connection_count(psoc) == 0) ||
	    ((policy_mgr_mode_specific_connection_count(psoc,
							PM_STA_MODE,
							NULL) == 1) &&
	     (policy_mgr_get_beaconing_mode_count(psoc, NULL) == 0) &&
	     (policy_mgr_mode_specific_connection_count(psoc,
							PM_P2P_CLIENT_MODE,
							NULL) == 0) &&
	     (policy_mgr_mode_specific_connection_count(psoc,
							PM_NDI_MODE,
							NULL) == 0))) {
		if (pm_ctx->dp_cbacks.hdd_disable_rx_ol_in_concurrency)
			pm_ctx->dp_cbacks.hdd_disable_rx_ol_in_concurrency(false);
	};

	/* Disable RPS if SAP interface has come up */
	if (policy_mgr_get_sap_mode_count(psoc, NULL) == 0) {
		if (pm_ctx->dp_cbacks.hdd_set_rx_mode_rps_cb)
			pm_ctx->dp_cbacks.hdd_set_rx_mode_rps_cb(false);
	}

	policy_mgr_dump_current_concurrency(psoc);

	/*
	 * Check mode of entry being removed. Update mcc_mode only when STA
	 * or SAP since IPA only cares about these two
	 */
	if (mode == QDF_STA_MODE || mode == QDF_SAP_MODE) {
		mcc_mode = policy_mgr_current_concurrency_is_mcc(psoc);

		if (pm_ctx->dp_cbacks.hdd_ipa_set_mcc_mode_cb)
			pm_ctx->dp_cbacks.hdd_ipa_set_mcc_mode_cb(mcc_mode);

		if (pm_ctx->dp_cbacks.hdd_ipa_set_perf_level_bw) {
			max_bw = policy_mgr_get_connection_max_channel_width(
					psoc);
			policy_mgr_debug("max channel width %d", max_bw);
			pm_ctx->dp_cbacks.hdd_ipa_set_perf_level_bw(max_bw);
		}
	}

	if (mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE ||
	    mode == QDF_STA_MODE || mode == QDF_P2P_CLIENT_MODE)
		policy_mgr_update_dfs_master_dynamic_enabled(psoc, false, NULL);

	if (!pm_ctx->last_disconn_sta_freq) {
		if (policy_mgr_update_indoor_concurrency(psoc, session_id,
		    cur_freq, DISCONNECT_WITHOUT_CONCURRENCY))
			wlan_reg_recompute_current_chan_list(psoc,
							     pm_ctx->pdev);
	}

	if (wlan_reg_get_keep_6ghz_sta_cli_connection(pm_ctx->pdev) &&
	    (mode == QDF_STA_MODE || mode == QDF_P2P_CLIENT_MODE))
		wlan_reg_recompute_current_chan_list(psoc, pm_ctx->pdev);

	return qdf_status;
}

QDF_STATUS policy_mgr_incr_connection_count(struct wlan_objmgr_psoc *psoc,
					    uint32_t vdev_id,
					    enum QDF_OPMODE op_mode)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index;
	struct policy_mgr_vdev_entry_info conn_table_entry = {0};
	enum policy_mgr_chain_mode chain_mask = POLICY_MGR_ONE_ONE;
	uint8_t nss_2g = 0, nss_5g = 0;
	enum policy_mgr_con_mode mode;
	uint32_t ch_freq;
	uint32_t nss = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool update_conn = true;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
		return status;
	}

	conn_index = policy_mgr_get_connection_count(psoc);
	if (pm_ctx->cfg.max_conc_cxns < conn_index) {
		policy_mgr_err("exceeded max connection limit %d",
			pm_ctx->cfg.max_conc_cxns);
		return status;
	}

	if (op_mode == QDF_NAN_DISC_MODE) {
		status = wlan_nan_get_connection_info(psoc, &conn_table_entry);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("Can't get NAN Connection info");
			return status;
		}
	} else {
		status = policy_mgr_get_connection_table_entry_info(
						pm_ctx->pdev,
						vdev_id, &conn_table_entry);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("can't find vdev_id %d in connection table",
				       vdev_id);
			return status;
		}
	}

	mode =  policy_mgr_qdf_opmode_to_pm_con_mode(psoc, op_mode, vdev_id);

	ch_freq = conn_table_entry.mhz;
	status = policy_mgr_get_nss_for_vdev(psoc, mode, &nss_2g, &nss_5g);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if ((WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq) && nss_2g > 1) ||
		    (WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq) && nss_5g > 1))
			chain_mask = POLICY_MGR_TWO_TWO;
		else
			chain_mask = POLICY_MGR_ONE_ONE;
		nss = (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq)) ? nss_2g : nss_5g;
	} else {
		policy_mgr_err("Error in getting nss");
	}

	if (mode == PM_STA_MODE || mode == PM_P2P_CLIENT_MODE)
		update_conn = false;

	/* add the entry */
	policy_mgr_update_conc_list(psoc, conn_index,
			mode,
			ch_freq,
			policy_mgr_get_bw(conn_table_entry.chan_width),
			conn_table_entry.mac_id,
			chain_mask,
			nss, vdev_id, true, update_conn,
			conn_table_entry.ch_flagext);
	policy_mgr_debug("Add at idx:%d vdev %d mac=%d",
		conn_index, vdev_id,
		conn_table_entry.mac_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_decr_connection_count(struct wlan_objmgr_psoc *psoc,
					uint32_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index = 0, next_conn_index = 0;
	bool found = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool panic = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return status;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (vdev_id == pm_conc_connection_list[conn_index].vdev_id) {
			/* debug msg */
			found = true;
			break;
		}
		conn_index++;
	}
	if (!found) {
		policy_mgr_err("can't find vdev_id %d in pm_conc_connection_list",
			vdev_id);
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
		return status;
	}
	next_conn_index = conn_index + 1;
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(next_conn_index)) {
		pm_conc_connection_list[conn_index].vdev_id =
			pm_conc_connection_list[next_conn_index].vdev_id;
		pm_conc_connection_list[conn_index].mode =
			pm_conc_connection_list[next_conn_index].mode;
		pm_conc_connection_list[conn_index].mac =
			pm_conc_connection_list[next_conn_index].mac;
		pm_conc_connection_list[conn_index].freq =
			pm_conc_connection_list[next_conn_index].freq;
		pm_conc_connection_list[conn_index].bw =
			pm_conc_connection_list[next_conn_index].bw;
		pm_conc_connection_list[conn_index].chain_mask =
			pm_conc_connection_list[next_conn_index].chain_mask;
		pm_conc_connection_list[conn_index].original_nss =
			pm_conc_connection_list[next_conn_index].original_nss;
		pm_conc_connection_list[conn_index].in_use =
			pm_conc_connection_list[next_conn_index].in_use;
		pm_conc_connection_list[conn_index].ch_flagext =
			pm_conc_connection_list[next_conn_index].ch_flagext;
		conn_index++;
		next_conn_index++;
	}

	/* clean up the entry */
	qdf_mem_zero(&pm_conc_connection_list[next_conn_index - 1],
		sizeof(*pm_conc_connection_list));

	conn_index = 0;
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (vdev_id == pm_conc_connection_list[conn_index].vdev_id) {
			panic = true;
			break;
		}
		conn_index++;
	}

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	if (panic) {
		policy_mgr_err("dup entry occur");
		policy_mgr_debug_alert();
	}
	if (pm_ctx->conc_cbacks.connection_info_update)
		pm_ctx->conc_cbacks.connection_info_update();

	return QDF_STATUS_SUCCESS;
}

uint32_t policy_mgr_get_mode_specific_conn_info(
		struct wlan_objmgr_psoc *psoc,
		uint32_t *ch_freq_list, uint8_t *vdev_id,
		enum policy_mgr_con_mode mode)
{

	uint32_t count = 0, index = 0;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}

	count = policy_mgr_mode_specific_connection_count(
				psoc, mode, list);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (count == 1) {
		if (ch_freq_list)
			*ch_freq_list =
				pm_conc_connection_list[list[index]].freq;
		if (vdev_id)
			*vdev_id = pm_conc_connection_list[list[index]].vdev_id;
	} else {
		for (index = 0; index < count; index++) {
			if (ch_freq_list)
				ch_freq_list[index] =
			pm_conc_connection_list[list[index]].freq;

			if (vdev_id)
				vdev_id[index] =
				pm_conc_connection_list[list[index]].vdev_id;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

uint32_t policy_mgr_get_sap_mode_info(struct wlan_objmgr_psoc *psoc,
				      uint32_t *ch_freq_list, uint8_t *vdev_id)
{
	uint32_t count;

	count = policy_mgr_get_mode_specific_conn_info(psoc, ch_freq_list,
						       vdev_id, PM_SAP_MODE);

	count += policy_mgr_get_mode_specific_conn_info(
				psoc,
				ch_freq_list ? &ch_freq_list[count] : NULL,
				vdev_id ? &vdev_id[count] : NULL,
				PM_LL_LT_SAP_MODE);
	return count;
}

uint32_t policy_mgr_get_beaconing_mode_info(struct wlan_objmgr_psoc *psoc,
					    uint32_t *ch_freq_list,
					    uint8_t *vdev_id)
{
	uint32_t count;

	count = policy_mgr_get_sap_mode_info(psoc, ch_freq_list, vdev_id);

	count += policy_mgr_get_mode_specific_conn_info(
				psoc,
				ch_freq_list ? &ch_freq_list[count] : NULL,
				vdev_id ? &vdev_id[count] : NULL,
				PM_P2P_GO_MODE);
	return count;
}

void policy_mgr_get_ml_and_non_ml_sta_count(struct wlan_objmgr_psoc *psoc,
					    uint8_t *num_ml, uint8_t *ml_idx,
					    uint8_t *num_non_ml,
					    uint8_t *non_ml_idx,
					    qdf_freq_t *freq_list,
					    uint8_t *vdev_id_list)
{
	uint32_t sta_num = 0;
	uint8_t i;
	struct wlan_objmgr_vdev *temp_vdev;

	*num_ml = 0;
	*num_non_ml = 0;

	sta_num = policy_mgr_get_mode_specific_conn_info(psoc, freq_list,
							 vdev_id_list,
							 PM_STA_MODE);
	if (!sta_num)
		return;

	for (i = 0; i < sta_num; i++) {
		temp_vdev =
			wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
							    vdev_id_list[i],
							    WLAN_POLICY_MGR_ID);
		if (!temp_vdev) {
			policy_mgr_err("invalid vdev for id %d",
				       vdev_id_list[i]);
			*num_ml = 0;
			*num_non_ml = 0;
			return;
		}

		if (wlan_vdev_mlme_is_mlo_vdev(temp_vdev)) {
			if (ml_idx)
				ml_idx[*num_ml] = i;
			(*num_ml)++;
		} else {
			if (non_ml_idx)
				non_ml_idx[*num_non_ml] = i;
			(*num_non_ml)++;
		}

		wlan_objmgr_vdev_release_ref(temp_vdev, WLAN_POLICY_MGR_ID);
	}
}

bool policy_mgr_concurrent_sta_on_different_mac(struct wlan_objmgr_psoc *psoc)
{
	uint8_t num_ml = 0, num_non_ml = 0;
	uint8_t ml_idx[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t non_ml_idx[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool is_different_mac = false;
	int i;

	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_get_ml_and_non_ml_sta_count(psoc, &num_ml, ml_idx,
					       &num_non_ml, non_ml_idx,
					       freq_list, vdev_id_list);
	if (num_ml + num_non_ml < 2 || !num_non_ml)
		goto out;

	/*
	 * If more than 1 Non-ML STA is present, check whether they are
	 * within the same band.
	 */
	for (i = 1; i < num_non_ml; i++) {
		if (!policy_mgr_2_freq_always_on_same_mac(psoc,
							  freq_list[non_ml_idx[i]],
							  freq_list[non_ml_idx[0]])) {
			is_different_mac = true;
			goto out;
		}
	}

	if (num_non_ml >= 2)
		goto out;

	/* ML STA + Non-ML STA */
	for (i = 0; i < num_ml; i++) {
		if (!policy_mgr_2_freq_always_on_same_mac(psoc,
							  freq_list[ml_idx[i]],
							  freq_list[non_ml_idx[0]])) {
			is_different_mac = true;
			goto out;
		}
	}

out:
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_debug("Non-ML STA count %d, ML STA count %d, sta concurrency on different mac %d",
			 num_non_ml, num_ml, is_different_mac);

	return is_different_mac;
}

bool policy_mgr_max_concurrent_connections_reached(
		struct wlan_objmgr_psoc *psoc)
{
	uint8_t i = 0, j = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (pm_ctx) {
		for (i = 0; i < QDF_MAX_NO_OF_MODE; i++)
			if (i != QDF_NAN_DISC_MODE)
				j += pm_ctx->no_of_active_sessions[i];
		return j > (pm_ctx->cfg.max_conc_cxns - 1);
	}

	return false;
}

static bool policy_mgr_is_sub_20_mhz_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	return pm_ctx->user_cfg.sub_20_mhz_enabled;
}

/**
 * policy_mgr_allow_wapi_concurrency() - Check if WAPI concurrency is allowed
 * @pm_ctx: policy_mgr_psoc_priv_obj policy mgr context
 *
 * This routine is called to check vdev security mode allowed in concurrency.
 * At present, WAPI security mode is not allowed to run concurrency with any
 * other vdev if the hardware doesn't support WAPI concurrency.
 *
 * Return: true - allow
 */
static bool
policy_mgr_allow_wapi_concurrency(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	struct wlan_objmgr_pdev *pdev = pm_ctx->pdev;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;

	if (!pdev) {
		policy_mgr_debug("pdev is Null");
		return false;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return false;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}

	if (!wmi_service_enabled(wmi_handle,
				 wmi_service_wapi_concurrency_supported) &&
	    mlme_is_wapi_sta_active(pdev) &&
	    policy_mgr_get_connection_count(pm_ctx->psoc) > 0)
		return false;

	return true;
}

#ifdef FEATURE_FOURTH_CONNECTION
static bool policy_mgr_is_concurrency_allowed_4_port(
					struct wlan_objmgr_psoc *psoc,
					enum policy_mgr_con_mode mode,
					uint32_t ch_freq,
					struct policy_mgr_pcl_list pcl)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx = NULL;
	uint8_t sap_cnt, go_cnt, ll_lt_sap_vdev_id, nan_cnt = 0;
	uint8_t count_sta;

	ll_lt_sap_vdev_id = wlan_policy_mgr_get_ll_lt_sap_vdev_id(psoc);

	if (ll_lt_sap_vdev_id != WLAN_INVALID_VDEV_ID) {
		policy_mgr_debug("LL_LT_SAP vdev %d present avoid 4th port concurrency",
				 ll_lt_sap_vdev_id);
		return false;
	}
	/* new STA may just have ssid, no channel until bssid assigned */
	if (ch_freq == 0 && mode == PM_STA_MODE)
		return true;

	sap_cnt = policy_mgr_mode_specific_connection_count(psoc,
							    PM_SAP_MODE, NULL);

	go_cnt = policy_mgr_mode_specific_connection_count(psoc,
							   PM_P2P_GO_MODE, NULL);

	if (wlan_nan_is_sta_sap_nan_allowed(psoc) ||
	    wlan_nan_is_sta_p2p_ndp_supported(psoc)) {
		nan_cnt = policy_mgr_mode_specific_connection_count(
				psoc,
				PM_NAN_DISC_MODE, NULL);
		count_sta = policy_mgr_mode_specific_connection_count(
				psoc, PM_STA_MODE, NULL);
		go_cnt += policy_mgr_mode_specific_connection_count(psoc,
							PM_P2P_CLIENT_MODE,
							NULL);
	}

	if (sap_cnt || go_cnt || nan_cnt || count_sta) {
		pm_ctx = policy_mgr_get_context(psoc);
		if (!pm_ctx) {
			policy_mgr_err("context is NULL");
			return false;
		}

		if (!policy_mgr_is_force_scc(psoc)) {
			policy_mgr_err("couldn't start 4th port for bad force scc cfg");
			return false;
		}

		if (!policy_mgr_is_dbs_enable(psoc)) {
			policy_mgr_err(
				"Couldn't start 4th port for bad cfg of dual mac");
			return false;
		}

		if ((mode == PM_NAN_DISC_MODE || mode == PM_NDI_MODE) &&
		    (wlan_nan_is_sta_sap_nan_allowed(psoc) ||
		     wlan_nan_is_sta_p2p_ndp_supported(psoc)))
			return true;

		for (i = 0; i < pcl.pcl_len; i++)
			if (ch_freq == pcl.pcl_list[i])
				return true;

		policy_mgr_err("4th port failed on ch freq %d with mode %d",
			       ch_freq, mode);

		return false;
	}

	return true;
}
#else
static inline bool policy_mgr_is_concurrency_allowed_4_port(
				struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				uint32_t ch_freq,
				struct policy_mgr_pcl_list pcl)
{return false; }
#endif

bool
policy_mgr_allow_multiple_sta_connections(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}
	if (!wmi_service_enabled(wmi_handle,
				 wmi_service_sta_plus_sta_support)) {
		policy_mgr_rl_debug("STA+STA is not supported");
		return false;
	}

	return true;
}

#if defined(CONFIG_BAND_6GHZ) && defined(WLAN_FEATURE_11AX)
bool policy_mgr_is_6ghz_conc_mode_supported(
	struct wlan_objmgr_psoc *psoc, enum policy_mgr_con_mode mode)
{
	if (mode == PM_STA_MODE || mode == PM_P2P_CLIENT_MODE ||
	    policy_mgr_is_beaconing_mode(mode))
		return true;
	else
		return false;
}
#endif

/**
 * policy_mgr_is_6g_channel_allowed() - Check new 6Ghz connection
 * allowed or not
 * @psoc: Pointer to soc
 * @mode: new connection mode
 * @ch_freq: channel freq
 *
 * 1. Only STA/SAP are allowed on 6Ghz.
 * 2. If there is DFS beacon entity existing on 5G band, 5G+6G MCC is not
 * allowed.
 *
 *  Return: true if supports else false.
 */
static bool policy_mgr_is_6g_channel_allowed(
	struct wlan_objmgr_psoc *psoc, enum policy_mgr_con_mode mode,
	uint32_t ch_freq)
{
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info *conn;
	bool is_dfs;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	if (!WLAN_REG_IS_6GHZ_CHAN_FREQ(ch_freq))
		return true;

	/* Only STA/SAP is supported on 6Ghz currently */
	if (!policy_mgr_is_6ghz_conc_mode_supported(psoc, mode)) {
		policy_mgr_rl_debug("mode %d for 6ghz not supported", mode);
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
				conn_index++) {
		conn = &pm_conc_connection_list[conn_index];
		if (!conn->in_use)
			continue;
		is_dfs = (conn->ch_flagext &
			(IEEE80211_CHAN_DFS | IEEE80211_CHAN_DFS_CFREQ2)) &&
			WLAN_REG_IS_5GHZ_CH_FREQ(conn->freq);
		if (policy_mgr_is_beaconing_mode(conn->mode) &&
		    is_dfs && (ch_freq != conn->freq &&
			       !policy_mgr_are_sbs_chan(psoc, ch_freq,
							conn->freq))) {
			policy_mgr_rl_debug("don't allow MCC if SAP/GO on DFS channel");
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return false;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return true;
}

#ifdef WLAN_FEATURE_11BE_MLO
QDF_STATUS policy_mgr_restart_emlsr_opportunistic_timer(
		struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *policy_mgr_ctx;

	policy_mgr_ctx = policy_mgr_get_context(psoc);
	if (!policy_mgr_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mc_timer_stop(&policy_mgr_ctx->emlsr_opportunistic_timer);

	status = qdf_mc_timer_start(
			&policy_mgr_ctx->emlsr_opportunistic_timer,
			EMLSR_OPPORTUNISTIC_TIME * 1000);

	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to start emlsr timer, %d", status);
		return status;
	}
	policymgr_nofl_debug("emlsr timer restarted");

	return status;
}

QDF_STATUS policy_mgr_stop_emlsr_opportunistic_timer(
		struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct policy_mgr_psoc_priv_obj *policy_mgr_ctx;

	policy_mgr_ctx = policy_mgr_get_context(psoc);
	if (!policy_mgr_ctx) {
		policy_mgr_err("Invalid context");
		return status;
	}
	if (QDF_TIMER_STATE_RUNNING !=
		qdf_mc_timer_get_current_state(
				&policy_mgr_ctx->emlsr_opportunistic_timer))
		return QDF_STATUS_SUCCESS;

	status = qdf_mc_timer_stop(&policy_mgr_ctx->emlsr_opportunistic_timer);

	if (QDF_IS_STATUS_ERROR(status))
		policy_mgr_err("failed to stop emlsr timer, %d", status);
	else
		policymgr_nofl_debug("emlsr timer stopped");

	return status;
}

static bool policy_mgr_is_acs_2ghz_only_sap(struct wlan_objmgr_psoc *psoc,
					    uint8_t sap_vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t acs_band = QCA_ACS_MODE_IEEE80211ANY;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (pm_ctx->hdd_cbacks.wlan_get_sap_acs_band)
		pm_ctx->hdd_cbacks.wlan_get_sap_acs_band(psoc,
							 sap_vdev_id,
							 &acs_band);

	if (acs_band == QCA_ACS_MODE_IEEE80211B ||
	    acs_band == QCA_ACS_MODE_IEEE80211G)
		return true;

	return false;
}

bool policy_mgr_vdev_is_force_inactive(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id)
{
	bool force_inactive = false;
	uint8_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	/* Get disabled link info as well and keep it at last */
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_DISABLE_LINK;
	     conn_index++) {
		if (pm_disabled_ml_links[conn_index].in_use &&
		    pm_disabled_ml_links[conn_index].mode == PM_STA_MODE &&
		    pm_disabled_ml_links[conn_index].vdev_id == vdev_id) {
			force_inactive = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return force_inactive;
}

/* MCC avoidance priority value for different legacy connection type.
 * Internal macro, not expected used other code.
 * Bigger value have higher priority.
 */
#define PRIORITY_STA            4
#define PRIORITY_2G_ONLY_SAP    3
#define PRIORITY_P2P            2
#define PRIORITY_SAP            1
#define PRIORITY_OTHER          0

uint8_t
policy_mgr_get_legacy_conn_info(struct wlan_objmgr_psoc *psoc,
				uint8_t *vdev_lst,
				qdf_freq_t *freq_lst,
				enum policy_mgr_con_mode *mode_lst,
				uint8_t lst_sz)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t conn_index, j = 0, i, k, n;
	struct wlan_objmgr_vdev *vdev;
	uint8_t vdev_id;
	uint8_t has_priority[MAX_NUMBER_OF_CONC_CONNECTIONS];

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (j >= lst_sz)
			break;
		if (!pm_conc_connection_list[conn_index].in_use)
			continue;
		vdev_id = pm_conc_connection_list[conn_index].vdev_id;
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
				pm_ctx->psoc, vdev_id, WLAN_POLICY_MGR_ID);
		if (!vdev) {
			policy_mgr_err("invalid vdev for id %d", vdev_id);
			continue;
		}

		if (wlan_vdev_mlme_is_mlo_vdev(vdev) &&
		    pm_conc_connection_list[conn_index].mode ==
							PM_STA_MODE) {
			wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
			continue;
		}
		if (pm_conc_connection_list[conn_index].mode !=
							PM_STA_MODE &&
		    pm_conc_connection_list[conn_index].mode !=
							PM_SAP_MODE &&
		    pm_conc_connection_list[conn_index].mode !=
							PM_P2P_CLIENT_MODE &&
		    pm_conc_connection_list[conn_index].mode !=
							PM_P2P_GO_MODE) {
			wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
			continue;
		}

		/* Set mcc avoidance priority value. The bigger value
		 * have higher priority to avoid MCC. In 3 Port concurrency
		 * case, usually we can only meet the higher priority intf's
		 * MCC avoidance by force inactive link.
		 */
		if (pm_conc_connection_list[conn_index].mode == PM_STA_MODE)
			has_priority[j] = PRIORITY_STA;
		else if (pm_conc_connection_list[conn_index].mode ==
							PM_SAP_MODE &&
			 policy_mgr_is_acs_2ghz_only_sap(psoc, vdev_id))
			has_priority[j] = PRIORITY_2G_ONLY_SAP;
		else if ((pm_conc_connection_list[conn_index].mode ==
							PM_P2P_CLIENT_MODE ||
			  pm_conc_connection_list[conn_index].mode ==
							PM_P2P_GO_MODE) &&
			 policy_mgr_is_vdev_high_tput_or_low_latency(
							psoc, vdev_id))
			has_priority[j] = PRIORITY_P2P;
		else if (pm_conc_connection_list[conn_index].mode ==
							PM_SAP_MODE)
			has_priority[j] = PRIORITY_SAP;
		else
			has_priority[j] = PRIORITY_OTHER;

		vdev_lst[j] = vdev_id;
		freq_lst[j] = pm_conc_connection_list[conn_index].freq;
		mode_lst[j] = pm_conc_connection_list[conn_index].mode;
		policy_mgr_debug("vdev %d freq %d mode %s pri %d",
				 vdev_id, freq_lst[j],
				 device_mode_to_string(mode_lst[j]),
				 has_priority[j]);
		j++;
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	}
	/* sort the list based on priority */
	for (i = 0; i < j; i++) {
		uint8_t tmp_vdev_lst;
		qdf_freq_t tmp_freq_lst;
		enum policy_mgr_con_mode tmp_mode_lst;

		n = i;
		for (k = i + 1; k < j; k++) {
			if (has_priority[n] < has_priority[k])
				n = k;
			else if ((has_priority[n] == has_priority[k]) &&
				 (vdev_lst[n] > vdev_lst[k]))
				n = k;
		}
		if (n == i)
			continue;
		tmp_vdev_lst = vdev_lst[i];
		tmp_freq_lst = freq_lst[i];
		tmp_mode_lst = mode_lst[i];

		vdev_lst[i] = vdev_lst[n];
		freq_lst[i] = freq_lst[n];
		mode_lst[i] = mode_lst[n];

		vdev_lst[n] = tmp_vdev_lst;
		freq_lst[n] = tmp_freq_lst;
		mode_lst[n] = tmp_mode_lst;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return j;
}

static void
policy_mgr_fill_ml_active_link_vdev_bitmap(struct mlo_link_set_active_req *req,
					   uint8_t *mlo_vdev_lst,
					   uint32_t num_mlo_vdev)
{
	uint32_t entry_idx, entry_offset, vdev_idx;
	uint8_t vdev_id;

	for (vdev_idx = 0; vdev_idx < num_mlo_vdev; vdev_idx++) {
		vdev_id = mlo_vdev_lst[vdev_idx];
		entry_idx = vdev_id / 32;
		entry_offset = vdev_id % 32;
		if (entry_idx >= MLO_LINK_NUM_SZ) {
			policy_mgr_err("Invalid entry_idx %d num_mlo_vdev %d vdev %d",
				       entry_idx, num_mlo_vdev, vdev_id);
			continue;
		}
		req->param.vdev_bitmap[entry_idx] |= (1 << entry_offset);
		/* update entry number if entry index changed */
		if (req->param.num_vdev_bitmap < entry_idx + 1)
			req->param.num_vdev_bitmap = entry_idx + 1;
	}

	policy_mgr_debug("num_vdev_bitmap %d vdev_bitmap[0] = 0x%x, vdev_bitmap[1] = 0x%x",
			 req->param.num_vdev_bitmap, req->param.vdev_bitmap[0],
			 req->param.vdev_bitmap[1]);
}

static void
policy_mgr_fill_ml_inactive_link_vdev_bitmap(
				struct mlo_link_set_active_req *req,
				uint8_t *mlo_inactive_vdev_lst,
				uint32_t num_mlo_inactive_vdev)
{
	uint32_t entry_idx, entry_offset, vdev_idx;
	uint8_t vdev_id;

	for (vdev_idx = 0; vdev_idx < num_mlo_inactive_vdev; vdev_idx++) {
		vdev_id = mlo_inactive_vdev_lst[vdev_idx];
		entry_idx = vdev_id / 32;
		entry_offset = vdev_id % 32;
		if (entry_idx >= MLO_LINK_NUM_SZ) {
			policy_mgr_err("Invalid entry_idx %d num_mlo_vdev %d vdev %d",
				       entry_idx, num_mlo_inactive_vdev,
				       vdev_id);
			continue;
		}
		req->param.inactive_vdev_bitmap[entry_idx] |=
							(1 << entry_offset);
		/* update entry number if entry index changed */
		if (req->param.num_inactive_vdev_bitmap < entry_idx + 1)
			req->param.num_inactive_vdev_bitmap = entry_idx + 1;
	}

	policy_mgr_debug("num_vdev_bitmap %d inactive_vdev_bitmap[0] = 0x%x, inactive_vdev_bitmap[1] = 0x%x",
			 req->param.num_inactive_vdev_bitmap,
			 req->param.inactive_vdev_bitmap[0],
			 req->param.inactive_vdev_bitmap[1]);
}

/*
 * policy_mgr_handle_ml_sta_link_state_allowed() - Check ml sta connection to
 * allow link state change.
 * @psoc: psoc object
 * @reason: set link state reason
 *
 * If ml sta is not "connected" state, no need to do link state handling.
 * After disconnected, target will clear the force active/inactive state
 * and host will remove the connection entry finally.
 * After roaming done, active/inactive will be re-calculated.
 *
 * Return: QDF_STATUS_SUCCESS if link state is allowed to change
 */
static QDF_STATUS
policy_mgr_handle_ml_sta_link_state_allowed(struct wlan_objmgr_psoc *psoc,
					    enum mlo_link_force_reason reason)
{
	uint8_t num_ml_sta = 0, num_disabled_ml_sta = 0, num_non_ml = 0;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev;
	bool ml_sta_is_not_connected = false;
	bool ml_sta_is_link_removal = false;
	uint8_t i;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_INVAL;
	}

	policy_mgr_get_ml_sta_info(pm_ctx, &num_ml_sta, &num_disabled_ml_sta,
				   ml_sta_vdev_lst, ml_freq_lst, &num_non_ml,
				   NULL, NULL);
	if (!num_ml_sta || num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS) {
		policy_mgr_debug("ml sta num is %d", num_ml_sta);
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < num_ml_sta; i++) {
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(pm_ctx->psoc,
							    ml_sta_vdev_lst[i],
							    WLAN_POLICY_MGR_ID);
		if (!vdev) {
			policy_mgr_err("invalid vdev for id %d",
				       ml_sta_vdev_lst[i]);
			continue;
		}
		if (!wlan_cm_is_vdev_connected(vdev)) {
			policy_mgr_debug("ml sta vdev %d is not connected state",
					 ml_sta_vdev_lst[i]);
			ml_sta_is_not_connected = true;
		}
		if (wlan_get_vdev_link_removed_flag_by_vdev_id(
						psoc, ml_sta_vdev_lst[i])) {
			policy_mgr_debug("ml sta vdev %d link removed",
					 ml_sta_vdev_lst[i]);
			ml_sta_is_link_removal = true;
		}

		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	}

	if (ml_sta_is_not_connected) {
		status = QDF_STATUS_E_FAILURE;
	} else if (reason != MLO_LINK_FORCE_REASON_LINK_REMOVAL) {
		if (ml_sta_is_link_removal)
			status = QDF_STATUS_E_FAILURE;
	}
	policy_mgr_debug("set link reason %d status %d rm %d", reason, status,
			 ml_sta_is_link_removal);

	return status;
}

/*
 * policy_mgr_validate_set_mlo_link_cb() - Callback to check whether
 * it is allowed to set mlo sta link state.
 * @psoc: psoc object
 * @param: set mlo link parameter
 *
 * This api will be used as callback to be called by mlo_link_set_active
 * in serialization context.
 *
 * Return: QDF_STATUS_SUCCESS if set mlo link is allowed
 */
static QDF_STATUS
policy_mgr_validate_set_mlo_link_cb(struct wlan_objmgr_psoc *psoc,
				    struct mlo_link_set_active_param *param)
{
	return policy_mgr_handle_ml_sta_link_state_allowed(psoc,
							   param->reason);
}

uint32_t
policy_mgr_get_active_vdev_bitmap(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_INVAL;
	}

	policy_mgr_debug("active link bitmap value: %d",
			 pm_ctx->active_vdev_bitmap);

	return pm_ctx->active_vdev_bitmap;
}

/**
 * policy_mgr_mlo_sta_set_link_by_linkid() - wrapper API to call set link
 * by link id bitmap API
 * @psoc: psoc object
 * @vdev: vdev object
 * @reason: Reason for which link is forced
 * @mode: Force reason
 * @link_num: valid for MLO_LINK_FORCE_MODE_ACTIVE_NUM and
 *  MLO_LINK_FORCE_MODE_INACTIVE_NUM.
 * @num_mlo_vdev: number of mlo vdev in array mlo_vdev_lst
 * @mlo_vdev_lst: MLO STA vdev list
 * @num_mlo_inactive_vdev: number of mlo vdev in array mlo_inactive_vdev_lst
 * @mlo_inactive_vdev_lst: MLO STA vdev list
 *
 * This is internal wrapper of policy_mgr_mlo_sta_set_nlink to convert
 * vdev based set link to link id based API for being compatible with old code.
 * New code to use policy_mgr_mlo_sta_set_nlink directly as much as possible.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
policy_mgr_mlo_sta_set_link_by_linkid(struct wlan_objmgr_psoc *psoc,
				      struct wlan_objmgr_vdev *vdev,
				      enum mlo_link_force_reason reason,
				      enum mlo_link_force_mode mode,
				      uint8_t link_num,
				      uint8_t num_mlo_vdev,
				      uint8_t *mlo_vdev_lst,
				      uint8_t num_mlo_inactive_vdev,
				      uint8_t *mlo_inactive_vdev_lst)
{
	uint32_t link_bitmap = 0;
	uint32_t link_bitmap2 = 0;
	uint32_t assoc_bitmap = 0;
	uint32_t vdev_bitmap[MLO_VDEV_BITMAP_SZ];
	uint32_t vdev_bitmap2[MLO_VDEV_BITMAP_SZ];
	uint8_t i, idx;
	uint32_t link_control_flags = 0;
	uint8_t vdev_per_bitmap = MLO_MAX_VDEV_COUNT_PER_BIMTAP_ELEMENT;

	qdf_mem_zero(vdev_bitmap, sizeof(vdev_bitmap));
	qdf_mem_zero(vdev_bitmap2, sizeof(vdev_bitmap2));

	for (i = 0; i < num_mlo_vdev; i++) {
		idx = mlo_vdev_lst[i] / vdev_per_bitmap;
		if (idx >= MLO_VDEV_BITMAP_SZ)
			return QDF_STATUS_E_INVAL;
		vdev_bitmap[idx] |= 1 << (mlo_vdev_lst[i] % vdev_per_bitmap);
	}

	for (i = 0; i < num_mlo_inactive_vdev; i++) {
		idx = mlo_inactive_vdev_lst[i] / vdev_per_bitmap;
		if (idx >= MLO_VDEV_BITMAP_SZ)
			return QDF_STATUS_E_INVAL;
		vdev_bitmap2[idx] |=
			1 << (mlo_inactive_vdev_lst[i] % vdev_per_bitmap);
	}

	ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
		psoc, vdev, MLO_VDEV_BITMAP_SZ, vdev_bitmap,
		&link_bitmap, &assoc_bitmap);

	ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
		psoc, vdev, MLO_VDEV_BITMAP_SZ, vdev_bitmap2,
		&link_bitmap2, &assoc_bitmap);

	switch (mode) {
	case MLO_LINK_FORCE_MODE_ACTIVE:
		link_control_flags = link_ctrl_f_overwrite_active_bitmap;
		break;
	case MLO_LINK_FORCE_MODE_INACTIVE:
		if (reason != MLO_LINK_FORCE_REASON_LINK_REMOVAL)
			link_control_flags =
				link_ctrl_f_overwrite_inactive_bitmap;
		break;
	case MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE:
		if (reason != MLO_LINK_FORCE_REASON_LINK_REMOVAL)
			link_control_flags =
				link_ctrl_f_overwrite_active_bitmap |
				link_ctrl_f_overwrite_inactive_bitmap;
		break;
	case MLO_LINK_FORCE_MODE_ACTIVE_NUM:
		link_control_flags = link_ctrl_f_dynamic_force_link_num;
		break;
	case MLO_LINK_FORCE_MODE_INACTIVE_NUM:
		link_control_flags = link_ctrl_f_dynamic_force_link_num;
		break;
	case MLO_LINK_FORCE_MODE_NO_FORCE:
		link_control_flags = 0;
		break;
	default:
		policy_mgr_err("Invalid force mode: %d", mode);
		return QDF_STATUS_E_INVAL;
	}

	return policy_mgr_mlo_sta_set_nlink(psoc, wlan_vdev_get_id(vdev),
					    reason, mode,
					    link_num, link_bitmap,
					    link_bitmap2, link_control_flags);
}

/**
 * policy_mgr_mlo_sta_set_link_ext() - Set links for MLO STA
 * @psoc: psoc object
 * @reason: Reason for which link is forced
 * @mode: Force reason
 * @num_mlo_vdev: number of mlo vdev
 * @mlo_vdev_lst: MLO STA vdev list
 * @num_mlo_inactive_vdev: number of mlo vdev
 * @mlo_inactive_vdev_lst: MLO STA vdev list
 *
 * Interface manager Set links for MLO STA. And it supports to
 * add inactive vdev list.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
policy_mgr_mlo_sta_set_link_ext(struct wlan_objmgr_psoc *psoc,
				enum mlo_link_force_reason reason,
				enum mlo_link_force_mode mode,
				uint8_t num_mlo_vdev, uint8_t *mlo_vdev_lst,
				uint8_t num_mlo_inactive_vdev,
				uint8_t *mlo_inactive_vdev_lst)
{
	struct mlo_link_set_active_req *req;
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev;

	if (!num_mlo_vdev) {
		policy_mgr_err("invalid 0 num_mlo_vdev");
		return QDF_STATUS_E_INVAL;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_INVAL;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	/*
	 * Use one of the ML vdev as, if called from disconnect the caller vdev
	 * may get deleted, and thus flush serialization command.
	 */
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, mlo_vdev_lst[0],
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev %d: invalid vdev", mlo_vdev_lst[0]);
		qdf_mem_free(req);
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_debug("vdev %d: mode %d num_mlo_vdev %d reason %d",
			 wlan_vdev_get_id(vdev), mode, num_mlo_vdev, reason);

	req->ctx.vdev = vdev;
	req->param.reason = reason;
	req->param.force_mode = mode;
	req->ctx.set_mlo_link_cb = policy_mgr_handle_link_enable_disable_resp;
	req->ctx.validate_set_mlo_link_cb =
		policy_mgr_validate_set_mlo_link_cb;
	req->ctx.cb_arg = req;

	/* set MLO vdev bit mask for all case */
	policy_mgr_fill_ml_active_link_vdev_bitmap(req, mlo_vdev_lst,
						   num_mlo_vdev);

	pm_ctx->active_vdev_bitmap = req->param.vdev_bitmap[0];
	pm_ctx->inactive_vdev_bitmap = req->param.vdev_bitmap[1];

	if (mode == MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE)
		policy_mgr_fill_ml_inactive_link_vdev_bitmap(
			req, mlo_inactive_vdev_lst, num_mlo_inactive_vdev);

	/*
	 * Fill number of links for MLO_LINK_FORCE_MODE_ACTIVE_NUM or
	 * MLO_LINK_FORCE_MODE_INACTIVE_NUM mode.
	 */
	if (mode == MLO_LINK_FORCE_MODE_ACTIVE_NUM ||
	    mode == MLO_LINK_FORCE_MODE_INACTIVE_NUM) {
		req->param.num_link_entry = 1;
		req->param.link_num[0].num_of_link = num_mlo_vdev - 1;
	}

	if (ml_is_nlink_service_supported(psoc)) {
		status =
		policy_mgr_mlo_sta_set_link_by_linkid(psoc, vdev, reason,
						      mode,
						      req->param.link_num[0].
						      num_of_link,
						      num_mlo_vdev,
						      mlo_vdev_lst,
						      num_mlo_inactive_vdev,
						      mlo_inactive_vdev_lst);
		qdf_mem_free(req);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

		if (status != QDF_STATUS_E_PENDING) {
			policy_mgr_err("set_link_by_linkid status %d", status);
			return status;
		}
		return QDF_STATUS_SUCCESS;
	}

	policy_mgr_set_link_in_progress(pm_ctx, true);

	status = mlo_ser_set_link_req(req);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("vdev %d: Failed to set link mode %d num_mlo_vdev %d reason %d",
			       wlan_vdev_get_id(vdev), mode, num_mlo_vdev,
			       reason);
		qdf_mem_free(req);
		policy_mgr_set_link_in_progress(pm_ctx, false);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	return status;
}

QDF_STATUS
policy_mgr_mlo_sta_set_link(struct wlan_objmgr_psoc *psoc,
			    enum mlo_link_force_reason reason,
			    enum mlo_link_force_mode mode,
			    uint8_t num_mlo_vdev, uint8_t *mlo_vdev_lst)
{
	return policy_mgr_mlo_sta_set_link_ext(psoc, reason, mode, num_mlo_vdev,
					       mlo_vdev_lst, 0, NULL);
}

QDF_STATUS
policy_mgr_mlo_sta_set_nlink(struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id,
			     enum mlo_link_force_reason reason,
			     enum mlo_link_force_mode mode,
			     uint8_t link_num,
			     uint16_t link_bitmap,
			     uint16_t link_bitmap2,
			     uint32_t link_control_flags)
{
	struct mlo_link_set_active_req *req;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_INVAL;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	vdev =
	wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
					     vdev_id,
					     WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("invalid vdev for id %d",
			       vdev_id);
		qdf_mem_free(req);
		return QDF_STATUS_E_INVAL;
	}

	policy_mgr_set_link_in_progress(pm_ctx, true);

	policy_mgr_debug("vdev %d: mode %d %s reason %d bitmap 0x%x 0x%x ctrl 0x%x",
			 wlan_vdev_get_id(vdev), mode,
			 force_mode_to_string(mode), reason,
			 link_bitmap, link_bitmap2,
			 link_control_flags);

	req->ctx.vdev = vdev;
	req->param.reason = reason;
	req->param.force_mode = mode;
	req->param.use_ieee_link_id = true;
	req->param.force_cmd.ieee_link_id_bitmap = link_bitmap;
	req->param.force_cmd.ieee_link_id_bitmap2 = link_bitmap2;
	req->param.force_cmd.link_num = link_num;
	policy_mgr_update_disallowed_mode_bitmap(psoc,
						 vdev,
						 req,
						 link_control_flags);

	if (link_control_flags & link_ctrl_f_overwrite_active_bitmap)
		req->param.control_flags.overwrite_force_active_bitmap = true;
	if (link_control_flags & link_ctrl_f_overwrite_inactive_bitmap)
		req->param.control_flags.overwrite_force_inactive_bitmap =
									true;
	if (link_control_flags & link_ctrl_f_dynamic_force_link_num)
		req->param.control_flags.dynamic_force_link_num = true;
	if (link_control_flags & link_ctrl_f_post_re_evaluate) {
		req->param.control_flags.post_re_evaluate = true;
		req->param.control_flags.post_re_evaluate_loops =
			GET_POST_RE_EVALUATE_LOOPS(link_control_flags);
	}
	if (link_control_flags & link_ctrl_f_dont_reschedule_workqueue)
		req->param.control_flags.dont_reschedule_workqueue = true;

	status =
	wlan_vdev_get_bss_peer_mld_mac(vdev,
				       &req->param.force_cmd.ap_mld_mac_addr);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("fail to get ap mld addr for vdev %d",
			       wlan_vdev_get_id(vdev));
		goto end;
	}
	if (qdf_is_macaddr_zero(&req->param.force_cmd.ap_mld_mac_addr)) {
		policy_mgr_err("get ap zero mld addr for vdev %d",
			       wlan_vdev_get_id(vdev));
		goto end;
	}

	req->ctx.set_mlo_link_cb = policy_mgr_handle_link_enable_disable_resp;
	req->ctx.validate_set_mlo_link_cb =
		policy_mgr_validate_set_mlo_link_cb;
	req->ctx.cb_arg = req;
	status = mlo_ser_set_link_req(req);
end:
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("vdev %d: Failed to set link mode %d num_mlo_vdev %d reason %d",
			       wlan_vdev_get_id(vdev), mode, link_num,
			       reason);
		qdf_mem_free(req);
		policy_mgr_set_link_in_progress(pm_ctx, false);
	} else {
		status = QDF_STATUS_E_PENDING;
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return status;
}

uint32_t
policy_mgr_get_conc_ext_flags(struct wlan_objmgr_vdev *vdev, bool force_mlo)
{
	struct wlan_objmgr_vdev *assoc_vdev;
	union conc_ext_flag conc_ext_flags;

	conc_ext_flags.value = 0;
	if (!vdev || wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		return conc_ext_flags.value;

	if (!force_mlo && !wlan_vdev_mlme_is_mlo_vdev(vdev))
		return conc_ext_flags.value;

	conc_ext_flags.mlo = 1;
	if (wlan_vdev_mlme_is_mlo_link_vdev(vdev)) {
		assoc_vdev = ucfg_mlo_get_assoc_link_vdev(vdev);
		if (assoc_vdev && ucfg_cm_is_vdev_active(assoc_vdev))
			conc_ext_flags.mlo_link_assoc_connected = 1;
	}

	return conc_ext_flags.value;
}

/**
 * policy_mgr_allow_sta_concurrency() - check whether STA concurrency is allowed
 * @psoc: Pointer to soc
 * @freq: frequency to be checked
 * @ext_flags: extended flags for concurrency check
 *
 *  Return: true if supports else false.
 */
static bool
policy_mgr_allow_sta_concurrency(struct wlan_objmgr_psoc *psoc,
				 qdf_freq_t freq,
				 uint32_t ext_flags)
{
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev;
	bool is_mlo, mlo_sta_present = false;
	uint8_t vdev_id, sta_cnt = 0;
	enum policy_mgr_con_mode mode;
	union conc_ext_flag conc_ext_flags;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	conc_ext_flags.value = ext_flags;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		mode = pm_conc_connection_list[conn_index].mode;
		if (mode != PM_STA_MODE ||
		    !pm_conc_connection_list[conn_index].in_use)
			continue;

		vdev_id = pm_conc_connection_list[conn_index].vdev_id;
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
							    WLAN_POLICY_MGR_ID);
		if (!vdev)
			continue;

		is_mlo = wlan_vdev_mlme_is_mlo_vdev(vdev);

		/* Skip the link vdev for MLO STA */
		if (wlan_vdev_mlme_is_mlo_link_vdev(vdev))
			goto next;

		sta_cnt++;
		if (!is_mlo)
			goto next;

		mlo_sta_present = true;
next:
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	/* Reject if multiple STA connections are not allowed */
	if (sta_cnt &&
	    !policy_mgr_allow_multiple_sta_connections(psoc)) {
		policy_mgr_rl_debug("Disallow Multiple STA connections");
		return false;
	}

	if (mlo_sta_present && conc_ext_flags.mlo_link_assoc_connected) {
		policy_mgr_rl_debug("Allow secondary MLO link");
		return true;
	}

	if (conc_ext_flags.mlo && mlo_sta_present) {
		policy_mgr_rl_debug("Disallow ML STA when ML STA is present");
		return false;
	}

	/*
	 * Reject a 3rd STA.
	 * Treat a MLO STA(including the primary and secondary link vdevs)
	 * as 1 STA here.
	 */
	if (sta_cnt >= 2) {
		policy_mgr_rl_debug("Disallow 3rd STA");
		return false;
	}

	return true;
}

bool
policy_mgr_is_mlo_sap_concurrency_allowed(struct wlan_objmgr_psoc *psoc,
					  bool is_new_vdev_mlo,
					  uint8_t new_vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index;
	bool ret = false;
	struct wlan_objmgr_vdev *vdev;
	uint32_t vdev_id;
	uint8_t mlo_sap_support_link_num;
	uint8_t started_mlo_sap_vdev_num = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return ret;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		 conn_index++) {
		if (!pm_conc_connection_list[conn_index].in_use ||
		    (pm_conc_connection_list[conn_index].mode != PM_SAP_MODE))
			continue;

		vdev_id = pm_conc_connection_list[conn_index].vdev_id;
		if (vdev_id == new_vdev_id)
			continue;
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
							    WLAN_POLICY_MGR_ID);
		if (!vdev) {
			policy_mgr_err("vdev for vdev_id:%d is NULL", vdev_id);
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return ret;
		}

		if (wlan_vdev_mlme_is_mlo_vdev(vdev))
			started_mlo_sap_vdev_num++;

		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	mlo_sap_support_link_num =
		wlan_mlme_get_mlo_sap_support_link(psoc);

	policy_mgr_debug("is_new_vdev_mlo %u started_mlo_sap_vdev_num %u, mlo_sap_support_link_num %u",
			 is_new_vdev_mlo,
			 started_mlo_sap_vdev_num,
			 mlo_sap_support_link_num);
	if (is_new_vdev_mlo &&
	    started_mlo_sap_vdev_num >= mlo_sap_support_link_num)
		ret = false;
	else
		ret = true;

	return ret;
}

QDF_STATUS
policy_mgr_link_switch_notifier_cb(struct wlan_objmgr_vdev *vdev,
				   struct wlan_mlo_link_switch_req *req,
				   enum wlan_mlo_link_switch_notify_reason notify_reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t vdev_id = req->vdev_id;
	uint8_t curr_ieee_link_id = req->curr_ieee_link_id;
	uint8_t new_ieee_link_id = req->new_ieee_link_id;
	uint32_t new_primary_freq = req->new_primary_freq;
	QDF_STATUS status;
	union conc_ext_flag conc_ext_flags;
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	uint8_t num_del = 0;
	struct ml_nlink_change_event data;
	uint16_t dyn_inact_bmap = 0, force_inact_bmap = 0;

	if (notify_reason > MLO_LINK_SWITCH_NOTIFY_REASON_PRE_START_POST_SER)
		return QDF_STATUS_SUCCESS;

	/*
	 * CSA on the SAP/GO would have been allowed based on the
	 * current concurrency combination. Starting a link switch
	 * during an active CSA, could lead to unexpected behavior.
	 */
	if (policy_mgr_is_chan_switch_in_progress(psoc)) {
		policy_mgr_debug("CSA is in progress for SAP/GO, reject the link switch");
		return QDF_STATUS_E_INVAL;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_INVAL;
	}

	policy_mgr_debug("target link %d freq %d curr link %d notify reason %d link switch reason %d vdev %d",
			 new_ieee_link_id, new_primary_freq,
			 curr_ieee_link_id, notify_reason, req->reason,
			 vdev_id);
	qdf_mem_zero(&data, sizeof(data));
	data.evt.link_switch.curr_ieee_link_id = curr_ieee_link_id;
	data.evt.link_switch.new_ieee_link_id = new_ieee_link_id;
	data.evt.link_switch.new_primary_freq = new_primary_freq;
	data.evt.link_switch.reason = req->reason;
	status = ml_nlink_conn_change_notify(psoc, vdev_id,
					     ml_nlink_link_switch_start_evt,
					     &data);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);

	policy_mgr_store_and_del_conn_info_by_vdev_id(
		psoc, vdev_id, info, &num_del);
	conc_ext_flags.value =
	policy_mgr_get_conc_ext_flags(vdev, true);
	ml_nlink_get_dynamic_inactive_links(psoc, vdev, &dyn_inact_bmap,
					    &force_inact_bmap);

	if (!(dyn_inact_bmap & BIT(new_ieee_link_id)) &&
	    !policy_mgr_is_concurrency_allowed(psoc, PM_STA_MODE,
					       new_primary_freq,
					       HW_MODE_20_MHZ,
					       conc_ext_flags.value,
					       NULL)) {
		status = QDF_STATUS_E_INVAL;
		policy_mgr_debug("target link %d freq %d not allowed by conc rule",
				 new_ieee_link_id, new_primary_freq);
	}

	if (num_del > 0)
		policy_mgr_restore_deleted_conn_info(psoc, info, num_del);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

bool policy_mgr_is_non_ml_sta_present(struct wlan_objmgr_psoc *psoc)
{
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev;
	bool non_ml_sta_present = false;
	uint8_t vdev_id;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0;
	     conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].mode != PM_STA_MODE ||
		    !pm_conc_connection_list[conn_index].in_use)
			continue;

		vdev_id = pm_conc_connection_list[conn_index].vdev_id;
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
							    WLAN_POLICY_MGR_ID);
		if (!vdev)
			continue;

		if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
			non_ml_sta_present = true;
			wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
			break;
		}

		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	}

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	return non_ml_sta_present;
}

bool policy_mgr_is_mlo_sta_present(struct wlan_objmgr_psoc *psoc)
{
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev;
	bool mlo_sta_present = false;
	uint8_t vdev_id;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0;
	     conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS && !mlo_sta_present;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].mode != PM_STA_MODE ||
		    !pm_conc_connection_list[conn_index].in_use)
			continue;

		vdev_id = pm_conc_connection_list[conn_index].vdev_id;
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
							    WLAN_POLICY_MGR_ID);
		if (!vdev)
			continue;

		mlo_sta_present = wlan_vdev_mlme_is_mlo_vdev(vdev);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	}

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	return mlo_sta_present;
}

bool policy_mgr_is_mlo_in_mode_sbs(struct wlan_objmgr_psoc *psoc,
				   enum policy_mgr_con_mode mode,
				   uint8_t *mlo_vdev_lst, uint8_t *num_mlo)
{
	uint32_t mode_num = 0;
	uint8_t i, mlo_idx = 0;
	struct wlan_objmgr_vdev *temp_vdev;
	qdf_freq_t mlo_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	bool is_sbs_link = true;

	mode_num = policy_mgr_get_mode_specific_conn_info(psoc, NULL,
							 vdev_id_list, mode);
	if (!mode_num || mode_num < 2)
		return false;

	for (i = 0; i < mode_num; i++) {
		temp_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
							vdev_id_list[i],
							WLAN_POLICY_MGR_ID);
		if (!temp_vdev) {
			policy_mgr_err("invalid vdev for id %d",
				       vdev_id_list[i]);
			return false;
		}

		if (wlan_vdev_mlme_is_mlo_vdev(temp_vdev)) {
			if (mlo_vdev_lst)
				mlo_vdev_lst[mlo_idx] = vdev_id_list[i];
			mlo_freq_list[mlo_idx] =
				wlan_get_operation_chan_freq(temp_vdev);
			if (wlan_reg_is_24ghz_ch_freq(mlo_freq_list[mlo_idx]))
				is_sbs_link = false;
			mlo_idx++;
		}
		wlan_objmgr_vdev_release_ref(temp_vdev, WLAN_POLICY_MGR_ID);
	}

	if (num_mlo)
		*num_mlo = mlo_idx;
	if (mlo_idx < 2)
		is_sbs_link = false;
	if (is_sbs_link &&
	    !policy_mgr_are_sbs_chan(psoc, mlo_freq_list[0],
				     mlo_freq_list[1])) {
		policy_mgr_debug("Freq %d and %d are not SBS, set SBS false",
				 mlo_freq_list[0],
				 mlo_freq_list[1]);
		is_sbs_link = false;
	}

	return is_sbs_link;
}

bool policy_mgr_is_mlo_in_mode_dbs(struct wlan_objmgr_psoc *psoc,
				   enum policy_mgr_con_mode mode,
				   uint8_t *mlo_vdev_lst, uint8_t *num_mlo)
{
	uint32_t mode_num = 0;
	uint8_t i, mlo_idx = 0;
	struct wlan_objmgr_vdev *temp_vdev;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	bool has_2g_link = false;
	bool has_5g_link = false;
	qdf_freq_t mlo_freq;

	mode_num = policy_mgr_get_mode_specific_conn_info(psoc, NULL,
							  vdev_id_list, mode);
	if (!mode_num || mode_num < 2)
		return false;

	for (i = 0; i < mode_num; i++) {
		temp_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
							psoc,
							vdev_id_list[i],
							WLAN_POLICY_MGR_ID);
		if (!temp_vdev) {
			policy_mgr_err("invalid vdev for id %d",
				       vdev_id_list[i]);
			return false;
		}

		if (wlan_vdev_mlme_is_mlo_vdev(temp_vdev)) {
			if (mlo_vdev_lst)
				mlo_vdev_lst[mlo_idx] = vdev_id_list[i];
			mlo_freq =
				wlan_get_operation_chan_freq(temp_vdev);
			if (wlan_reg_is_24ghz_ch_freq(mlo_freq))
				has_2g_link = true;
			else
				has_5g_link = true;
			mlo_idx++;
		}
		wlan_objmgr_vdev_release_ref(temp_vdev, WLAN_POLICY_MGR_ID);
	}

	if (num_mlo)
		*num_mlo = mlo_idx;

	return has_2g_link && has_5g_link;
}

bool policy_mgr_is_curr_hwmode_emlsr(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_hw_mode_params hw_mode;

	if (!policy_mgr_is_hw_emlsr_capable(psoc))
		return false;

	if (QDF_STATUS_SUCCESS != policy_mgr_get_current_hw_mode(psoc,
								 &hw_mode))
		return false;

	if (!hw_mode.emlsr_cap)
		return false;

	return true;
}

bool policy_mgr_is_mlo_in_mode_emlsr(struct wlan_objmgr_psoc *psoc,
				     uint8_t *mlo_vdev_lst, uint8_t *num_mlo)
{
	bool emlsr_connection = false;
	uint32_t mode_num = 0;
	uint8_t i, mlo_idx = 0;
	struct wlan_objmgr_vdev *temp_vdev;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	mode_num = policy_mgr_get_mode_specific_conn_info(psoc, NULL,
							  vdev_id_list,
							  PM_STA_MODE);

	for (i = 0; i < mode_num; i++) {
		temp_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
							psoc, vdev_id_list[i],
							WLAN_POLICY_MGR_ID);
		if (!temp_vdev) {
			policy_mgr_err("invalid vdev for id %d",
				       vdev_id_list[i]);
			goto end;
		}

		if (wlan_vdev_mlme_is_mlo_vdev(temp_vdev)) {
			if (mlo_vdev_lst)
				mlo_vdev_lst[mlo_idx] = vdev_id_list[i];
			mlo_idx++;
		}
		/* Check if existing vdev is eMLSR STA */
		if (wlan_vdev_mlme_cap_get(temp_vdev, WLAN_VDEV_C_EMLSR_CAP))
			emlsr_connection = true;

		wlan_objmgr_vdev_release_ref(temp_vdev, WLAN_POLICY_MGR_ID);
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_DISABLE_LINK; i++) {
		if (!pm_disabled_ml_links[i].in_use)
			continue;
		if (pm_disabled_ml_links[i].mode != PM_STA_MODE)
			continue;
		temp_vdev =
		wlan_objmgr_get_vdev_by_id_from_psoc(
					psoc, pm_disabled_ml_links[i].vdev_id,
					WLAN_POLICY_MGR_ID);
		if (!temp_vdev) {
			policy_mgr_err("invalid inactive vdev for id %d",
				       pm_disabled_ml_links[i].vdev_id);
			continue;
		}
		/* Check if existing vdev is eMLSR STA */
		if (wlan_vdev_mlme_cap_get(temp_vdev, WLAN_VDEV_C_EMLSR_CAP))
			emlsr_connection = true;
		wlan_objmgr_vdev_release_ref(temp_vdev, WLAN_POLICY_MGR_ID);
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
end:
	if (num_mlo)
		*num_mlo = mlo_idx;

	return emlsr_connection;
}

static void policy_mgr_restore_no_force(struct wlan_objmgr_psoc *psoc,
					uint8_t num_mlo,
					uint8_t mlo_vdev_lst[],
					bool conc_con_coming_up)
{
	struct ml_link_force_state force_cmd = {0};
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	if (num_mlo < 1) {
		policy_mgr_err("invalid num_mlo %d",
			       num_mlo);
		return;
	}

	vdev =
	wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
					     mlo_vdev_lst[0],
					     WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("invalid vdev for id %d",
			       mlo_vdev_lst[0]);
		return;
	}

	ml_nlink_get_curr_force_state(psoc, vdev, &force_cmd);
	if (!conc_con_coming_up || force_cmd.force_active_bitmap) {
		if (ml_is_nlink_service_supported(psoc))
			status = policy_mgr_mlo_sta_set_nlink(
					psoc, mlo_vdev_lst[0],
					MLO_LINK_FORCE_REASON_DISCONNECT,
					MLO_LINK_FORCE_MODE_NO_FORCE,
					0, 0, 0, 0);
		else
			status = policy_mgr_mlo_sta_set_link(
					psoc,
					MLO_LINK_FORCE_REASON_DISCONNECT,
					MLO_LINK_FORCE_MODE_NO_FORCE,
					num_mlo, mlo_vdev_lst);
		/* If concurrency vdev is coming up and force active bitmap
		 * is present, we need to wait for the respone of no force
		 * command.
		 */
		if (force_cmd.force_active_bitmap && conc_con_coming_up) {
			if (status == QDF_STATUS_E_PENDING)
				policy_mgr_wait_for_set_link_update(psoc);
			else
				policy_mgr_err("status %d", status);

			ml_nlink_get_curr_force_state(psoc, vdev, &force_cmd);
			ml_nlink_dump_force_state(&force_cmd, "");
		}
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
}

static void
policy_mgr_update_emlsr_inactive_request(struct wlan_objmgr_psoc *psoc,
					 uint8_t num_mlo,
					 uint8_t *mlo_vdev_lst,
					 bool clr)
{
	struct wlan_objmgr_vdev *vdev;
	struct set_link_req req;

	qdf_mem_zero(&req, sizeof(req));
	if (!clr) {
		req.mode = MLO_LINK_FORCE_MODE_INACTIVE_NUM;
		req.reason = MLO_LINK_FORCE_REASON_CONNECT;
		req.force_inactive_num = num_mlo - 1;
		req.force_inactive_num_bitmap =
			ml_nlink_convert_vdev_ids_to_link_bitmap(
					psoc, mlo_vdev_lst, num_mlo);
	}

	vdev =
	wlan_objmgr_get_vdev_by_id_from_psoc(psoc, mlo_vdev_lst[0],
					     WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev not found for vdev_id %d ",
			       mlo_vdev_lst[0]);
		return;
	}

	ml_nlink_update_force_link_request(psoc, vdev, &req,
					   SET_LINK_FROM_CONCURRENCY);
	wlan_objmgr_vdev_release_ref(vdev,
				     WLAN_POLICY_MGR_ID);
}

void policy_mgr_handle_emlsr_sta_concurrency(struct wlan_objmgr_psoc *psoc,
					     bool conc_con_coming_up,
					     bool emlsr_sta_coming_up)
{
	uint8_t num_mlo = 0;
	uint8_t mlo_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	bool is_mlo_emlsr = false;
	uint8_t num_disabled_ml_sta = 0;
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	is_mlo_emlsr = policy_mgr_is_mlo_in_mode_emlsr(psoc, mlo_vdev_lst,
						       &num_mlo);
	policy_mgr_debug("num_mlo %d is_mlo_emlsr %d conc_con_coming_up: %d",
			 num_mlo, is_mlo_emlsr, conc_con_coming_up);

	if (!is_mlo_emlsr)
		return;

	if (num_mlo < 2) {
		policy_mgr_debug("conc_con_coming_up %d num mlo sta links %d",
				 conc_con_coming_up, num_mlo);
		policy_mgr_get_ml_sta_info(pm_ctx, &num_mlo,
					   &num_disabled_ml_sta,
					   mlo_vdev_lst, ml_freq_lst,
					   NULL, NULL, NULL);
		if (policy_mgr_get_connection_count(psoc) != 1 ||
		    !num_disabled_ml_sta)
			return;
	}

	if (conc_con_coming_up ||
	    (emlsr_sta_coming_up &&
	     policy_mgr_get_connection_count(psoc) > 2)) {
		/*
		 * If any force active link bitmap is present, we have to
		 * clear the force active bitmap from target. Otherwise that
		 * will be conflict with the force inactive num bitmap, then
		 * target can't handle force inactive num 1 command to exit
		 * EMLSR.
		 */
		if (conc_con_coming_up)
			policy_mgr_restore_no_force(psoc, num_mlo,
						    mlo_vdev_lst,
						    conc_con_coming_up);
		policy_mgr_update_emlsr_inactive_request(psoc, num_mlo,
							 mlo_vdev_lst,
							 false);

		/*
		 * Force disable one of the links (FW will decide which link) if
		 * 1) EMLSR STA is present and SAP/STA/NAN connection comes up.
		 * 2) There is a legacy connection (SAP/P2P/NAN) and a STA comes
		 * up in EMLSR mode.
		 */
		policy_mgr_mlo_sta_set_link(psoc, MLO_LINK_FORCE_REASON_CONNECT,
					    MLO_LINK_FORCE_MODE_INACTIVE_NUM,
					    num_mlo, mlo_vdev_lst);
		return;
	}

	if (!conc_con_coming_up && emlsr_sta_coming_up) {
		/*
		 * No force i.e. Re-enable the disabled link if-
		 * 1) EMLSR STA is present and new SAP/STA/NAN connection goes
		 *    down. One of the links was disabled while a new connection
		 *    came up.
		 * 2) Legacy connection (SAP/P2P/NAN) goes down and if STA is
		 *    EMLSR capable. One of the links was disabled after EMLSR
		 *    association.
		 */
		policy_mgr_update_emlsr_inactive_request(psoc, num_mlo,
							 mlo_vdev_lst,
							 true);
		policy_mgr_restore_no_force(psoc, num_mlo,
					    mlo_vdev_lst,
					    conc_con_coming_up);
	}
}

bool
policy_mgr_is_emlsr_sta_concurrency_present(struct wlan_objmgr_psoc *psoc)
{
	uint8_t num_mlo = 0;

	if (policy_mgr_is_mlo_in_mode_emlsr(psoc, NULL, &num_mlo) &&
	    num_mlo < policy_mgr_get_connection_count(psoc))
		return true;

	return false;
}

static uint8_t
policy_mgr_get_affected_links_for_sta_sta(struct wlan_objmgr_psoc *psoc,
					  uint8_t num_ml, qdf_freq_t *freq_list,
					  uint8_t *vdev_id_list,
					  uint8_t *ml_vdev_lst,
					  uint8_t *ml_idx, qdf_freq_t freq)
{
	uint8_t i = 0;
	bool same_band_sta_allowed;

	/*
	 * STA freq:      ML STA combo:  SBS Action
	 * ---------------------------------------------------
	 * 2Ghz           2Ghz+5/6Ghz    Disable 2Ghz(Same MAC)
	 * 5Ghz           2Ghz+5/6Ghz    Disable 2.4Ghz if 5Ghz lead to SBS
	 *                               (SBS, same MAC) and same band STA
	 *                               allowed, else disable 5/6Ghz
	 *                               (NON SBS, same MAC)
	 * 5Ghz(lower)    5Ghz+6Ghz      Disable 5Ghz (NON SBS, same MAC)
	 * 5Ghz(higher)   5Ghz+6Ghz      Disable 6Ghz (NON SBS, Same MAC)
	 * 2Ghz           5Ghz+6Ghz      Disable Any
	 */

	/* If non-ML STA is 2.4Ghz disable 2.4Ghz if present OR disable any */
	if (wlan_reg_is_24ghz_ch_freq(freq)) {
		while (i < num_ml) {
			if (wlan_reg_is_24ghz_ch_freq(freq_list[ml_idx[i]])) {
				/* Affected ML STA link on 2.4Ghz */
				ml_vdev_lst[0] = vdev_id_list[ml_idx[i]];
				return 1;
			}
			/* Fill non effected vdev in list */
			ml_vdev_lst[i] = vdev_id_list[ml_idx[i]];
			i++;
		}
		/* No link affected return num_ml to disable any */
		return i;
	}

	/* This mean non-ML STA is 5Ghz */

	/* check if ML STA is DBS */
	i = 0;
	while (i < num_ml &&
	       !wlan_reg_is_24ghz_ch_freq(freq_list[ml_idx[i]]))
		i++;

	same_band_sta_allowed = wlan_cm_same_band_sta_allowed(psoc);

	/*
	 * if ML STA is DBS ie 2.4Ghz link present and if same_band_sta_allowed
	 * is false, disable 5/6Ghz link to make sure we dont have all link
	 * on 5Ghz
	 */
	if (i < num_ml && !same_band_sta_allowed)
		goto check_dbs_ml;

	/* check if any link lead to SBS, so that we can disable the other*/
	i = 0;
	while (i < num_ml &&
	       !policy_mgr_are_sbs_chan(psoc, freq, freq_list[ml_idx[i]]))
		i++;

	/*
	 * if i < num_ml then i is the SBS link, in this case disable the other
	 * non SBS link, this mean ML STA is 5+6 or 2+5/6.
	 */
	if (i < num_ml) {
		i = 0;
		while (i < num_ml) {
			if (!policy_mgr_are_sbs_chan(psoc, freq,
						     freq_list[ml_idx[i]])) {
				/* Affected non SBS ML STA link */
				ml_vdev_lst[0] = vdev_id_list[ml_idx[i]];
				return 1;
			}
			/* Fill non effected vdev in list */
			ml_vdev_lst[i] = vdev_id_list[ml_idx[i]];
			i++;
		}
		/* All link lead to SBS, disable any, This should not happen */
		return i;
	}

check_dbs_ml:
	/*
	 * None of the link can lead to SBS, i.e. its 2+ 5/6 ML STA in this case
	 * disable 5Ghz link.
	 */
	i = 0;
	while (i < num_ml) {
		if (!wlan_reg_is_24ghz_ch_freq(freq_list[ml_idx[i]])) {
			/* Affected 5/6Ghz ML STA link */
			ml_vdev_lst[0] = vdev_id_list[ml_idx[i]];
			return 1;
		}
		/* Fill non effected vdev in list */
		ml_vdev_lst[i] = vdev_id_list[ml_idx[i]];
		i++;
	}

	/* No link affected, This should not happen */
	return i;
}

/*
 * policy_mgr_get_concurrent_num_links() - get links which are affected
 * if no affected then return num ml. Also fills the ml_vdev_lst to send.
 * @num_ml: number of ML vdev
 * @freq_list: freq list of all vdev
 * @vdev_id_list: vdev id list
 * @ml_vdev_lst: ML vdev list
 * @ml_idx: ML index
 * @freq: non ML STA freq
 *
 * Return: number of the affected links, else total link and ml_vdev_lst list.
 */
static uint8_t
policy_mgr_get_concurrent_num_links(struct wlan_objmgr_vdev *vdev,
				    uint8_t num_ml, qdf_freq_t *freq_list,
				    uint8_t *vdev_id_list,
				    uint8_t *ml_vdev_lst,
				    uint8_t *ml_idx, qdf_freq_t freq)
{
	uint8_t i = 0;
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);

	if (!psoc)
		return 0;

	while (i < num_ml && (freq_list[ml_idx[i]] != freq))
		i++;

	if (i < num_ml) {
		/* if one link is SCC then no need to disable any link */
		policy_mgr_debug("vdev %d: ML vdev %d lead to SCC, STA freq %d ML freq %d, no need to disable link",
				 wlan_vdev_get_id(vdev),
				 vdev_id_list[ml_idx[i]],
				 freq, freq_list[ml_idx[i]]);
		return 0;
	}


	return policy_mgr_get_affected_links_for_sta_sta(psoc, num_ml,
							 freq_list,
							 vdev_id_list,
							 ml_vdev_lst,
							 ml_idx, freq);
}

static void
policy_mgr_ml_sta_concurrency_on_connect(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_vdev *vdev,
				    uint8_t num_ml, uint8_t *ml_idx,
				    uint8_t num_non_ml, uint8_t *non_ml_idx,
				    qdf_freq_t *freq_list,
				    uint8_t *vdev_id_list)
{
	qdf_freq_t freq = 0;
	struct wlan_channel *bss_chan;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	uint8_t ml_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t affected_links = 0;
	enum mlo_link_force_mode mode = MLO_LINK_FORCE_MODE_ACTIVE_NUM;

	/* non ML STA doesn't exist, no need to change to link.*/
	if (!num_non_ml)
		return;

	if (wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		freq = freq_list[non_ml_idx[0]];
	} else {
		bss_chan = wlan_vdev_mlme_get_bss_chan(vdev);
		if (bss_chan)
			freq = bss_chan->ch_freq;
	}
	policy_mgr_debug("vdev %d: Freq %d (non ML vdev id %d), is ML STA %d",
			 vdev_id, freq, vdev_id_list[non_ml_idx[0]],
			 wlan_vdev_mlme_is_mlo_vdev(vdev));
	if (!freq)
		return;

	affected_links =
		policy_mgr_get_concurrent_num_links(vdev, num_ml, freq_list,
						    vdev_id_list, ml_vdev_lst,
						    ml_idx, freq);

	if (!affected_links) {
		policy_mgr_debug("vdev %d: no affected link found", vdev_id);
		return;
	}
	policy_mgr_debug("affected link found: %u vdev_id: %u",
			 affected_links, ml_vdev_lst[0]);

	/*
	 * If affected link is less than num_ml, ie not all link are affected,
	 * send MLO_LINK_FORCE_MODE_INACTIVE.
	 */
	if (affected_links < num_ml &&
	    affected_links <= MAX_NUMBER_OF_CONC_CONNECTIONS) {
		if (mlo_is_sta_inactivity_allowed_with_quiet(psoc, vdev_id_list,
							     num_ml, ml_idx,
							     affected_links,
							     ml_vdev_lst)) {
			mode = MLO_LINK_FORCE_MODE_INACTIVE;
		} else {
			policy_mgr_debug("vdev %d: force inactivity is not allowed",
					 ml_vdev_lst[0]);
			return;
		}
	}

	policy_mgr_mlo_sta_set_link(psoc, MLO_LINK_FORCE_REASON_CONNECT,
				    mode, affected_links, ml_vdev_lst);
}

static void
policy_mgr_get_disabled_ml_sta_idx(struct wlan_objmgr_psoc *psoc,
				   uint8_t *ml_sta,
				   uint8_t *ml_idx,
				   qdf_freq_t *freq_list,
				   uint8_t *vdev_id_list, uint8_t next_idx)
{
	uint8_t conn_index, fill_index = next_idx;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	/* Get disabled link info as well and keep it at last */
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_DISABLE_LINK;
	     conn_index++) {
		if (!pm_disabled_ml_links[conn_index].in_use)
			continue;
		if (pm_disabled_ml_links[conn_index].mode != PM_STA_MODE)
			continue;
		if ((fill_index >= MAX_NUMBER_OF_CONC_CONNECTIONS) ||
		    (*ml_sta >= MAX_NUMBER_OF_CONC_CONNECTIONS)) {
			policy_mgr_err("Invalid fill_index: %d or ml_sta: %d",
				       fill_index, *ml_sta);
			break;
		}
		vdev_id_list[fill_index] =
				pm_disabled_ml_links[conn_index].vdev_id;
		freq_list[fill_index] = pm_disabled_ml_links[conn_index].freq;
		ml_idx[(*ml_sta)++] = fill_index++;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
}

/**
 * policy_mgr_handle_ml_sta_link_concurrency() - Handle STA+ML_STA concurrency
 * @psoc: PSOC object information
 * @vdev: vdev of the changed interface caller
 *
 * Return: void
 */
static QDF_STATUS
policy_mgr_handle_ml_sta_link_concurrency(struct wlan_objmgr_psoc *psoc,
					  struct wlan_objmgr_vdev *vdev)
{
	uint8_t num_ml = 0, num_non_ml = 0, next_idx, disabled_links;
	uint8_t ml_idx[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t non_ml_idx[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_INVAL;
	}

	/* Skip non STA connection handling */
	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		return QDF_STATUS_E_INVAL;

	/*
	 * Skip this in case of SAP/P2P Concurrencies, to avoid renable of
	 * the link, disabled by SAP/P2P logic, as this API only consider
	 * STA specific counts and ignore other counts.
	 */
	if (policy_mgr_get_beaconing_mode_count(psoc, NULL) ||
	    policy_mgr_mode_specific_connection_count(psoc,
						      PM_P2P_CLIENT_MODE,
						      NULL)) {
		policy_mgr_debug("SAP/GO/CLI exist ignore this check");
		return QDF_STATUS_E_INVAL;
	}

	policy_mgr_get_ml_and_non_ml_sta_count(psoc, &num_ml, ml_idx,
					       &num_non_ml, non_ml_idx,
					       freq_list, vdev_id_list);
	/* Skip non STA+STA cases */
	if (!num_ml || !num_non_ml)
		return QDF_STATUS_E_INVAL;

	next_idx = num_ml + num_non_ml;
	policy_mgr_get_disabled_ml_sta_idx(psoc, &num_ml, ml_idx,
					   freq_list, vdev_id_list, next_idx);

	disabled_links = num_ml - (next_idx - num_non_ml);
	policy_mgr_debug("vdev %d: num_ml %d num_non_ml %d disabled_links: %d",
			 wlan_vdev_get_id(vdev), num_ml, num_non_ml,
			 disabled_links);

	/* ML STA is not up or not sufficient links to disable */
	if (num_ml < 2 || num_ml > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_ml - disabled_links < 2) {
		policy_mgr_debug("ML STA is not up or not sufficient links to disable");
		return QDF_STATUS_E_INVAL;
	}
	/*
	 * TODO: Check if both link enable/ link switch is possible when
	 * secondary STA switch happens to a new channel due to CSA
	 */

	policy_mgr_ml_sta_concurrency_on_connect(psoc, vdev, num_ml,
						 ml_idx, num_non_ml,
						 non_ml_idx, freq_list,
						 vdev_id_list);
	return QDF_STATUS_SUCCESS;
}

static bool
policy_mgr_is_mode_p2p_sap(enum policy_mgr_con_mode mode)
{
	return (policy_mgr_is_beaconing_mode(mode) ||
		(mode == PM_P2P_CLIENT_MODE));
}

bool
policy_mgr_is_vdev_high_tput_or_low_latency(struct wlan_objmgr_psoc *psoc,
					    uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	bool is_vdev_ll_ht;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("invalid vdev for id %d", vdev_id);
		return false;
	}
	is_vdev_ll_ht = wlan_is_vdev_traffic_ll_ht(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return is_vdev_ll_ht;
}

bool
policy_mgr_check_2ghz_only_sap_affected_link(
			struct wlan_objmgr_psoc *psoc,
			uint8_t sap_vdev_id,
			qdf_freq_t sap_ch_freq,
			uint8_t ml_ch_freq_num,
			qdf_freq_t *ml_freq_lst)
{
	uint8_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE op_mode;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (!WLAN_REG_IS_24GHZ_CH_FREQ(sap_ch_freq))
		return false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
				psoc, sap_vdev_id,
				WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_debug("vdev is null %d", sap_vdev_id);
		return false;
	}
	op_mode = wlan_vdev_mlme_get_opmode(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	if (op_mode != QDF_SAP_MODE)
		return false;

	if (!policy_mgr_is_acs_2ghz_only_sap(psoc, sap_vdev_id))
		return false;

	/* If 2G ml STA exist, force scc will happen, no link
	 * to get affected.
	 */
	for (i = 0; i < ml_ch_freq_num; i++)
		if (WLAN_REG_IS_24GHZ_CH_FREQ(ml_freq_lst[i]))
			return false;

	/* If All ml STA are 5/6 band, force SCC will not happen
	 * for 2G only SAP, so return true to indicate one
	 * link get affected.
	 */
	return true;
}

/*
 * policy_mgr_get_affected_links_for_go_sap_cli() - Check if any of the P2P OR
 * SAP is causing MCC with a ML link and also is configured high tput or low
 * latency
 * @psoc: psoc ctx
 * @num_ml_sta: Number of ML STA present
 * @ml_vdev_lst: ML STA vdev id list
 * @ml_freq_lst: ML STA freq list
 * @num_p2p_sap: Number of P2P and SAP present
 * @p2p_sap_vdev_lst: P2P and SAP vdev id list
 * @p2p_sap_freq_lst: P2P and SAP freq list
 *
 * Return: Number of links causing MCC with any of the P2P or SAP which is
 * configured high tput or low latency
 */
static uint8_t
policy_mgr_get_affected_links_for_go_sap_cli(struct wlan_objmgr_psoc *psoc,
					     uint8_t num_ml_sta,
					     uint8_t *ml_vdev_lst,
					     qdf_freq_t *ml_freq_lst,
					     uint8_t num_p2p_sap,
					     uint8_t *p2p_sap_vdev_lst,
					     qdf_freq_t *p2p_sap_freq_lst)
{
	uint8_t i = 0, k = 0, num_affected_links = 0;

	if (!num_p2p_sap || num_ml_sta < 2)
		return num_affected_links;

	while (i < num_ml_sta) {
		/* if any link is causing MCC with GO/GC/AP, set mcc as true.*/
		for (k = 0; k < num_p2p_sap; k++) {
			/* Continue if SCC */
			if (ml_freq_lst[i] == p2p_sap_freq_lst[k])
				continue;

			/* SAP MCC with MLO STA link is not preferred.
			 * If SAP is 2Ghz only by ACS and two ML link are
			 * 5/6 band, then force SCC may not happen. In such
			 * case inactive one link.
			 */
			if (policy_mgr_check_2ghz_only_sap_affected_link(
					psoc, p2p_sap_vdev_lst[k],
					p2p_sap_freq_lst[k],
					num_ml_sta, ml_freq_lst)) {
				policy_mgr_debug("2G only SAP vdev %d ch freq %d is not SCC with any MLO STA link",
						 p2p_sap_vdev_lst[k],
						 p2p_sap_freq_lst[k]);
				num_affected_links++;
				continue;
			}

			/* Continue if high tput or low latency is not set */
			if (!policy_mgr_is_vdev_high_tput_or_low_latency(
						psoc, p2p_sap_vdev_lst[k]))
				continue;

			/* If both freq are on same mac then its MCC */
			if (policy_mgr_are_2_freq_on_same_mac(psoc,
							ml_freq_lst[i],
							p2p_sap_freq_lst[k])) {
				policy_mgr_debug("ml sta vdev %d (freq %d) and p2p/sap vdev %d (freq %d) are MCC",
						 ml_vdev_lst[i], ml_freq_lst[i],
						 p2p_sap_vdev_lst[k],
						 p2p_sap_freq_lst[k]);
				num_affected_links++;
			}
		}
		i++;
	}

	return num_affected_links;
}

/*
 * policy_mgr_get_ml_sta_and_p2p_cli_go_sap_info() - Get number of ML STA,
 * P2P and SAP interfaces and their vdev ids and freq list
 * @pm_ctx: pm_ctx ctx
 * @num_ml_sta: Return number of ML STA present
 * @num_disabled_ml_sta: Return number of disabled ML STA links
 * @ml_vdev_lst: Return ML STA vdev id list
 * @ml_freq_lst: Return ML STA freq list
 * @num_p2p_sap: Return number of P2P and SAP present
 * @p2p_sap_vdev_lst: Return P2P and SAP vdev id list
 * @p2p_sap_freq_lst: Return P2P and SAP freq list
 *
 * Return: void
 */
static void
policy_mgr_get_ml_sta_and_p2p_cli_go_sap_info(
					struct policy_mgr_psoc_priv_obj *pm_ctx,
					uint8_t *num_ml_sta,
					uint8_t *num_disabled_ml_sta,
					uint8_t *ml_vdev_lst,
					qdf_freq_t *ml_freq_lst,
					uint8_t *num_p2p_sap,
					uint8_t *p2p_sap_vdev_lst,
					qdf_freq_t *p2p_sap_freq_lst)
{
	enum policy_mgr_con_mode mode;
	uint8_t vdev_id, conn_index;
	qdf_freq_t freq;

	*num_p2p_sap = 0;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_get_ml_sta_info(pm_ctx, num_ml_sta, num_disabled_ml_sta,
				   ml_vdev_lst, ml_freq_lst, NULL, NULL, NULL);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (!pm_conc_connection_list[conn_index].in_use)
			continue;
		mode = pm_conc_connection_list[conn_index].mode;
		if (!policy_mgr_is_mode_p2p_sap(mode))
			continue;
		vdev_id = pm_conc_connection_list[conn_index].vdev_id;
		freq = pm_conc_connection_list[conn_index].freq;

		/* add p2p and sap vdev and freq list */
		p2p_sap_vdev_lst[*num_p2p_sap] = vdev_id;
		p2p_sap_freq_lst[(*num_p2p_sap)++] = freq;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
}

/*
 * policy_mgr_is_ml_sta_links_in_mcc() - Check ML links are in MCC or not
 * @psoc: psoc ctx
 * @ml_freq_lst: ML STA freq list
 * @ml_vdev_lst: ML STA vdev id list
 * @ml_linkid_lst: ML STA link id list
 * @num_ml_sta: Number of total ML STA links
 * @affected_linkid_bitmap: link id bitmap which home channels are in MCC
 * with each other
 *
 * Return: true if ML link in MCC else false
 */
bool
policy_mgr_is_ml_sta_links_in_mcc(struct wlan_objmgr_psoc *psoc,
				  qdf_freq_t *ml_freq_lst,
				  uint8_t *ml_vdev_lst,
				  uint8_t *ml_linkid_lst,
				  uint8_t num_ml_sta,
				  uint32_t *affected_linkid_bitmap)
{
	uint8_t i, j;
	uint32_t link_id_bitmap;

	for (i = 0; i < num_ml_sta; i++) {
		link_id_bitmap = 0;
		if (ml_linkid_lst)
			link_id_bitmap = 1 << ml_linkid_lst[i];
		for (j = i + 1; j < num_ml_sta; j++) {
			if (ml_freq_lst[i] != ml_freq_lst[j] &&
			    policy_mgr_2_freq_always_on_same_mac(
					psoc, ml_freq_lst[i], ml_freq_lst[j])) {
				if (ml_vdev_lst)
					policy_mgr_debug("vdev %d and %d are in MCC with freq %d and freq %d",
							 ml_vdev_lst[i],
							 ml_vdev_lst[j],
							 ml_freq_lst[i],
							 ml_freq_lst[j]);
				if (ml_linkid_lst) {
					link_id_bitmap |= 1 << ml_linkid_lst[j];
					policy_mgr_debug("link %d and %d are in MCC with freq %d and freq %d",
							 ml_linkid_lst[i],
							 ml_linkid_lst[j],
							 ml_freq_lst[i],
							 ml_freq_lst[j]);
					if (affected_linkid_bitmap)
						*affected_linkid_bitmap =
							link_id_bitmap;
				}
				return true;
			}
		}
	}

	return false;
}

QDF_STATUS
policy_mgr_is_ml_links_in_mcc_allowed(struct wlan_objmgr_psoc *psoc,
				      struct wlan_objmgr_vdev *vdev,
				      uint8_t *ml_sta_vdev_lst,
				      uint8_t *num_ml_sta)
{
	uint8_t num_disabled_ml_sta = 0;
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_get_ml_sta_info(pm_ctx, num_ml_sta, &num_disabled_ml_sta,
				   ml_sta_vdev_lst, ml_freq_lst,
				   NULL, NULL, NULL);
	if (*num_ml_sta < 2 || *num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_disabled_ml_sta) {
		policy_mgr_debug("num_ml_sta invalid %d or link already disabled%d",
				 *num_ml_sta, num_disabled_ml_sta);
		return QDF_STATUS_E_FAILURE;
	}

	if (!policy_mgr_is_ml_sta_links_in_mcc(psoc, ml_freq_lst,
					       ml_sta_vdev_lst, NULL,
					       *num_ml_sta,
					       NULL))
		return QDF_STATUS_E_FAILURE;

	/*
	 * eMLSR is allowed in MCC mode also. So, don't disable any links
	 * if current connection happens in eMLSR mode.
	 */
	if (policy_mgr_is_mlo_in_mode_emlsr(psoc, NULL, NULL)) {
		policy_mgr_debug("Don't disable eMLSR links");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_handle_mcc_ml_sta() - disables one ML STA link if causing MCC
 * DBS - if ML STA links on 5 GHz + 6 GHz
 * SBS - if both ML STA links on 5 GHz high/5 GHz low
 * non-SBS - any combo (5/6 GHz + 5/6 GHz OR 2 GHz + 5/6 GHz)
 * @psoc: psoc ctx
 * @vdev: Pointer to vdev object
 *
 * Return: Success if MCC link is disabled else failure
 */
static QDF_STATUS
policy_mgr_handle_mcc_ml_sta(struct wlan_objmgr_psoc *psoc,
			     struct wlan_objmgr_vdev *vdev)
{
	uint8_t num_ml_sta = 0;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	QDF_STATUS status;

	if ((wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE))
		return QDF_STATUS_E_FAILURE;

	status = policy_mgr_is_ml_links_in_mcc_allowed(psoc, vdev,
						       ml_sta_vdev_lst,
						       &num_ml_sta);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	policy_mgr_mlo_sta_set_link(psoc, MLO_LINK_FORCE_REASON_CONNECT,
				    MLO_LINK_FORCE_MODE_ACTIVE_NUM,
				    num_ml_sta, ml_sta_vdev_lst);

	return QDF_STATUS_SUCCESS;
}

/*
 * policy_mgr_sta_ml_link_enable_allowed() - Check with given ML links and
 * existing concurrencies, a disabled ml link can be enabled back.
 * @psoc: psoc ctx
 * @num_disabled_ml_sta: Number of existing disabled links
 * @num_ml_sta: Number of total ML STA links
 * @ml_freq_lst: ML STA freq list
 * @ml_vdev_lst: ML STA vdev id list
 *
 * Return: if link can be enabled or not
 */
static bool
policy_mgr_sta_ml_link_enable_allowed(struct wlan_objmgr_psoc *psoc,
				      uint8_t num_disabled_ml_sta,
				      uint8_t num_ml_sta,
				      qdf_freq_t *ml_freq_lst,
				      uint8_t *ml_vdev_lst)
{
	union conc_ext_flag conc_ext_flags;
	uint8_t disabled_link_vdev_id;
	qdf_freq_t disabled_link_freq;
	struct wlan_objmgr_vdev *vdev;

	/* If no link is disabled nothing to do */
	if (!num_disabled_ml_sta || num_ml_sta < 2)
		return false;
	if (policy_mgr_is_ml_sta_links_in_mcc(psoc, ml_freq_lst, ml_vdev_lst,
					      NULL, num_ml_sta,
					      NULL))
		return false;
	/* Disabled link is at the last index */
	disabled_link_vdev_id = ml_vdev_lst[num_ml_sta - 1];
	disabled_link_freq = ml_freq_lst[num_ml_sta - 1];
	policy_mgr_debug("disabled_link_vdev_id %d disabled_link_freq %d",
			 disabled_link_vdev_id, disabled_link_freq);
	if (!disabled_link_freq)
		return false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, disabled_link_vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("invalid vdev for id %d", disabled_link_vdev_id);
		return false;
	}
	conc_ext_flags.value = policy_mgr_get_conc_ext_flags(vdev, false);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return policy_mgr_is_concurrency_allowed(psoc, PM_STA_MODE,
					disabled_link_freq, HW_MODE_20_MHZ,
					conc_ext_flags.value, NULL);
}

/*
 * policy_mgr_re_enable_ml_sta_on_p2p_sap_down() - Handle enable
 * link on P2P/SAP/ML_STA vdev UP or channel change
 * @psoc: objmgr psoc
 * @vdev: vdev which went UP or changed chan
 *
 * Return: void
 */
static void
policy_mgr_handle_sap_cli_go_ml_sta_up_csa(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev)
{
	uint8_t num_ml_sta = 0, num_p2p_sap = 0, num_disabled_ml_sta = 0;
	uint8_t num_affected_link;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t p2p_sap_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t p2p_sap_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	status = policy_mgr_handle_ml_sta_link_state_allowed(
				psoc, MLO_LINK_FORCE_REASON_CONNECT);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	/*
	 * eMLSR API policy_mgr_handle_emlsr_sta_concurrency() takes care of
	 * eMLSR concurrencies. Currently, eMLSR STA can't operate with any
	 * cocurrent mode, i.e. one link gets force-disabled when a new
	 * concurrecy is coming up.
	 */
	if (policy_mgr_is_mlo_in_mode_emlsr(psoc, NULL, NULL)) {
		policy_mgr_debug("STA connected in eMLSR mode, don't enable/disable links");
		return;
	}

	if (QDF_IS_STATUS_SUCCESS(policy_mgr_handle_mcc_ml_sta(psoc, vdev)))
		return;

	status = policy_mgr_handle_ml_sta_link_concurrency(psoc, vdev);
	if (QDF_IS_STATUS_SUCCESS(status))
		return;

	policy_mgr_get_ml_sta_and_p2p_cli_go_sap_info(pm_ctx, &num_ml_sta,
						      &num_disabled_ml_sta,
						      ml_sta_vdev_lst,
						      ml_freq_lst, &num_p2p_sap,
						      p2p_sap_vdev_lst,
						      p2p_sap_freq_lst);

	policy_mgr_debug("vdev %d: num_ml_sta %d disabled %d num_p2p_sap %d",
			 vdev_id, num_ml_sta, num_disabled_ml_sta, num_p2p_sap);
	if (num_ml_sta < 2 || num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_p2p_sap > MAX_NUMBER_OF_CONC_CONNECTIONS)
		return;

	num_affected_link = policy_mgr_get_affected_links_for_go_sap_cli(psoc,
						num_ml_sta, ml_sta_vdev_lst,
						ml_freq_lst, num_p2p_sap,
						p2p_sap_vdev_lst,
						p2p_sap_freq_lst);

	if (!num_affected_link) {
		policy_mgr_debug("vdev %d: no affected link found", vdev_id);
		goto enable_link;
	}

	if (num_disabled_ml_sta) {
		policy_mgr_debug("As a link is already disabled and affected link present (%d), No action required",
				 num_affected_link);
		return;
	}

	policy_mgr_mlo_sta_set_link(psoc, MLO_LINK_FORCE_REASON_CONNECT,
				    MLO_LINK_FORCE_MODE_ACTIVE_NUM,
				    num_ml_sta, ml_sta_vdev_lst);

	return;
enable_link:

	/*
	 * if no affected link and link can be allowed to enable then renable
	 * the disabled link.
	 */
	if (policy_mgr_sta_ml_link_enable_allowed(psoc, num_disabled_ml_sta,
						  num_ml_sta, ml_freq_lst,
						  ml_sta_vdev_lst))
		policy_mgr_mlo_sta_set_link(psoc,
					    MLO_LINK_FORCE_REASON_DISCONNECT,
					    MLO_LINK_FORCE_MODE_NO_FORCE,
					    num_ml_sta, ml_sta_vdev_lst);
}

void
policy_mgr_handle_ml_sta_links_on_vdev_up_csa(struct wlan_objmgr_psoc *psoc,
					      enum QDF_OPMODE mode,
					      uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev %d: invalid vdev", vdev_id);
		return;
	}

	if (mode == QDF_STA_MODE || mode == QDF_SAP_MODE ||
	    mode == QDF_P2P_CLIENT_MODE || mode == QDF_P2P_GO_MODE)
		policy_mgr_handle_sap_cli_go_ml_sta_up_csa(psoc, vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
}

/* Add extra buff if any connection is disconnecting */
#define SET_LINK_TIMEOUT ((STOP_RESPONSE_TIMER) + 6000)

QDF_STATUS policy_mgr_wait_for_set_link_update(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_INVAL;
	}

	if (!policy_mgr_get_link_in_progress(pm_ctx)) {
		policy_mgr_err("link is not in progress");
		return QDF_STATUS_E_FAILURE;
	}

	status =
		qdf_wait_for_event_completion(&pm_ctx->set_link_update_done_evt,
					      SET_LINK_TIMEOUT);

	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_set_link_in_progress(pm_ctx, false);
		policy_mgr_err("wait for set_link_in_progress failed");
	}

	return status;
}

void policy_mgr_handle_ml_sta_link_on_traffic_type_change(
						struct wlan_objmgr_psoc *psoc,
						struct wlan_objmgr_vdev *vdev)
{
	/* Check if any set link is already progress and thus wait */
	policy_mgr_wait_for_set_link_update(psoc);

	ml_nlink_conn_change_notify(
		psoc, wlan_vdev_get_id(vdev),
		ml_nlink_connection_updated_evt, NULL);

	/*
	 * Check if traffic type change lead to set link is progress and
	 * thus wait for it to complete.
	 */
	policy_mgr_wait_for_set_link_update(psoc);
}

static QDF_STATUS
policy_mgr_handle_ml_sta_link_enable_on_sta_down(struct wlan_objmgr_psoc *psoc,
						 struct wlan_objmgr_vdev *vdev)
{
	uint8_t num_ml_sta = 0, num_disabled_ml_sta = 0, num_non_ml = 0;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		return QDF_STATUS_E_INVAL;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_INVAL;
	}

	/* Handle only when non-ML STA is going down and ML STA is active */
	policy_mgr_get_ml_sta_info(pm_ctx, &num_ml_sta, &num_disabled_ml_sta,
				   ml_sta_vdev_lst, ml_freq_lst, &num_non_ml,
				   NULL, NULL);
	policy_mgr_debug("vdev %d: num_ml_sta %d disabled %d num_non_ml: %d",
			 vdev_id, num_ml_sta, num_disabled_ml_sta, num_non_ml);

	/*
	 * No ML STA is present or sinle link ML is present or
	 * more no.of links are active than supported concurrent connections
	 */
	if (num_ml_sta < 2 || num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS)
		return QDF_STATUS_E_INVAL;

	/* STA+STA cases */

	/* One ML/non-ML STA is going down and another non ML STA is present */
	if (num_non_ml) {
		policy_mgr_debug("non-ML STA is present");
		return QDF_STATUS_SUCCESS;
	}

	/*
	 * If no links are disabled or
	 * link can not be allowed to enable then skip checking further.
	 */
	if (!num_disabled_ml_sta ||
	    !policy_mgr_sta_ml_link_enable_allowed(psoc, num_disabled_ml_sta,
						  num_ml_sta, ml_freq_lst,
						  ml_sta_vdev_lst)) {
		if (num_disabled_ml_sta)
			policy_mgr_debug("Not re-enabled due to disallowed concurrency");
		goto done;
	}

	policy_mgr_mlo_sta_set_link(psoc,
				    MLO_LINK_FORCE_REASON_DISCONNECT,
				    MLO_LINK_FORCE_MODE_NO_FORCE,
				    num_ml_sta, ml_sta_vdev_lst);

done:
	return QDF_STATUS_SUCCESS;
}

/*
 * policy_mgr_re_enable_ml_sta_on_p2p_sap_sta_down() - Handle enable
 * link on P2P/SAP/ML_STA vdev down
 * @psoc: objmgr psoc
 * @vdev: vdev which went down
 *
 * Return: void
 */
static void
policy_mgr_re_enable_ml_sta_on_p2p_sap_sta_down(struct wlan_objmgr_psoc *psoc,
						struct wlan_objmgr_vdev *vdev)
{
	uint8_t num_ml_sta = 0, num_p2p_sap = 0, num_disabled_ml_sta = 0;
	uint8_t num_affected_link = 0;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t p2p_sap_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t p2p_sap_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	QDF_STATUS status;

	status = policy_mgr_handle_ml_sta_link_state_allowed(
				psoc, MLO_LINK_FORCE_REASON_DISCONNECT);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	status = policy_mgr_handle_ml_sta_link_enable_on_sta_down(psoc, vdev);
	if (QDF_IS_STATUS_SUCCESS(status))
		return;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	policy_mgr_get_ml_sta_and_p2p_cli_go_sap_info(pm_ctx, &num_ml_sta,
						      &num_disabled_ml_sta,
						      ml_sta_vdev_lst,
						      ml_freq_lst, &num_p2p_sap,
						      p2p_sap_vdev_lst,
						      p2p_sap_freq_lst);

	policy_mgr_debug("vdev %d: num_ml_sta %d disabled %d num_p2p_sap %d",
			 vdev_id, num_ml_sta, num_disabled_ml_sta, num_p2p_sap);

	if (num_ml_sta < 2 || num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_p2p_sap > MAX_NUMBER_OF_CONC_CONNECTIONS)
		return;

	/* If link can not be allowed to enable then skip checking further. */
	if (!policy_mgr_sta_ml_link_enable_allowed(psoc, num_disabled_ml_sta,
						   num_ml_sta, ml_freq_lst,
						   ml_sta_vdev_lst))
		return;

	/*
	 * If num_p2p_sap is non zero, ie p2p or sap still present check if
	 * disable link is still required, if not enable the link.
	 *
	 * If num_p2p_sap is 0, ie only ml sta is present, enable the link.
	 */
	if (num_p2p_sap)
		num_affected_link =
			policy_mgr_get_affected_links_for_go_sap_cli(psoc,
						num_ml_sta, ml_sta_vdev_lst,
						ml_freq_lst, num_p2p_sap,
						p2p_sap_vdev_lst,
						p2p_sap_freq_lst);

	if (num_affected_link)
		policy_mgr_debug("vdev %d: Affected link present, dont reanabe ML link",
				 vdev_id);
	else
		policy_mgr_mlo_sta_set_link(psoc,
					    MLO_LINK_FORCE_REASON_DISCONNECT,
					    MLO_LINK_FORCE_MODE_NO_FORCE,
					    num_ml_sta, ml_sta_vdev_lst);
}

void policy_mgr_handle_ml_sta_links_on_vdev_down(struct wlan_objmgr_psoc *psoc,
						 enum QDF_OPMODE mode,
						 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev %d: invalid vdev", vdev_id);
		return;
	}

	if (mode == QDF_STA_MODE || mode == QDF_SAP_MODE ||
	    mode == QDF_P2P_CLIENT_MODE || mode == QDF_P2P_GO_MODE)
		policy_mgr_re_enable_ml_sta_on_p2p_sap_sta_down(psoc, vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
}

/**
 * policy_mgr_pick_link_vdev_from_inactive_list() - Get inactive vdev
 * which can be activated
 * @psoc: PSOC object information
 * @vdev: vdev object
 * @inactive_vdev_num: inactive vdev num in list
 * @inactive_vdev_lst: inactive vdev list
 * @inactive_freq_lst: inactive vdev frequency list
 * @picked_vdev_id: Picked vdev id
 * @non_removed_vdev_id: not removed inactive vdev id
 *
 * If one link is removed and inactivated, pick one of existing inactive
 * vdev which can be activated by checking concurrency API.
 *
 * Return: void
 */
static void
policy_mgr_pick_link_vdev_from_inactive_list(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_vdev *vdev,
	uint8_t inactive_vdev_num, uint8_t *inactive_vdev_lst,
	qdf_freq_t *inactive_freq_lst, uint8_t *picked_vdev_id,
	uint8_t *non_removed_vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	uint8_t num_del = 0;
	union conc_ext_flag conc_ext_flags = {0};
	uint8_t i;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_store_and_del_conn_info_by_vdev_id(
			psoc, wlan_vdev_get_id(vdev),
			info, &num_del);
	/* pick one inactive parnter link and make it active */
	for (i = 0; i < inactive_vdev_num; i++) {
		struct wlan_objmgr_vdev *partner_vdev;

		if (wlan_get_vdev_link_removed_flag_by_vdev_id(
				psoc, inactive_vdev_lst[i])) {
			policy_mgr_debug("skip removed link vdev %d",
					 inactive_vdev_lst[i]);
			continue;
		}

		partner_vdev =
		wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						     inactive_vdev_lst[i],
						     WLAN_POLICY_MGR_ID);
		if (!partner_vdev) {
			policy_mgr_err("invalid partner_vdev %d ",
				       inactive_vdev_lst[i]);
			continue;
		}
		*non_removed_vdev_id = inactive_vdev_lst[i];

		conc_ext_flags.value =
		policy_mgr_get_conc_ext_flags(partner_vdev, false);

		if (policy_mgr_is_concurrency_allowed(psoc, PM_STA_MODE,
						      inactive_freq_lst[i],
						      HW_MODE_20_MHZ,
						      conc_ext_flags.value,
						      NULL)) {
			*picked_vdev_id = inactive_vdev_lst[i];
			wlan_objmgr_vdev_release_ref(partner_vdev,
						     WLAN_POLICY_MGR_ID);
			break;
		}
		wlan_objmgr_vdev_release_ref(partner_vdev, WLAN_POLICY_MGR_ID);
	}
	/* Restore the connection info */
	policy_mgr_restore_deleted_conn_info(psoc, info, num_del);
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
}

QDF_STATUS
policy_mgr_handle_link_removal_on_standby(struct wlan_objmgr_vdev *vdev,
					  struct ml_rv_info *reconfig_info)
{
	struct mlo_link_info *link_info;
	uint8_t i, link_id;
	uint32_t removal_link_bitmap = 0;
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;

	if (!vdev || !vdev->mlo_dev_ctx) {
		policy_mgr_err("invalid vdev or mlo_dev_ctx");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		policy_mgr_err("psoc is null");
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < reconfig_info->num_links; i++) {
		if (!(reconfig_info->link_info[i].is_ap_removal_timer_p &&
		      reconfig_info->link_info[i].ap_removal_timer))
			continue;

		link_id = reconfig_info->link_info[i].link_id;
		link_info = mlo_mgr_get_ap_link_by_link_id(vdev->mlo_dev_ctx,
							   link_id);
		if (!link_info) {
			policy_mgr_err("link info null, id %d", link_id);
			return QDF_STATUS_E_NULL_VALUE;
		}
		policy_mgr_debug("AP removal tbtt %d vdev %d link %d flag 0x%x STA MAC " QDF_MAC_ADDR_FMT " BSSID " QDF_MAC_ADDR_FMT,
				 reconfig_info->link_info[i].ap_removal_timer,
				 link_info->vdev_id, link_id,
				 (uint32_t)link_info->link_status_flags,
				 QDF_MAC_ADDR_REF(link_info->link_addr.bytes),
				 QDF_MAC_ADDR_REF(link_info->ap_link_addr.bytes));

		if (qdf_is_macaddr_zero(&link_info->ap_link_addr))
			continue;

		if (link_info->vdev_id != WLAN_INVALID_VDEV_ID)
			continue;

		if (qdf_atomic_test_and_set_bit(LS_F_AP_REMOVAL_BIT,
						&link_info->link_status_flags))
			continue;

		removal_link_bitmap |= 1 << link_id;
	}
	if (!removal_link_bitmap)
		return QDF_STATUS_SUCCESS;

	status = policy_mgr_mlo_sta_set_nlink(
			psoc, wlan_vdev_get_id(vdev),
			MLO_LINK_FORCE_REASON_LINK_REMOVAL,
			MLO_LINK_FORCE_MODE_INACTIVE,
			0,
			removal_link_bitmap,
			0,
			0);
	if (status == QDF_STATUS_E_PENDING)
		status = QDF_STATUS_SUCCESS;
	else
		policy_mgr_err("status %d", status);

	return status;
}

void policy_mgr_handle_link_removal_on_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i;
	uint8_t num_ml_sta = 0, num_disabled_ml_sta = 0;
	uint8_t num_active_ml_sta;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	uint8_t non_removal_link_vdev_id = WLAN_INVALID_VDEV_ID;
	uint8_t picked_vdev_id = WLAN_INVALID_VDEV_ID;
	QDF_STATUS status;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		policy_mgr_err("Failed to get psoc");
		return;
	}
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	if (wlan_get_vdev_link_removed_flag_by_vdev_id(psoc, vdev_id)) {
		policy_mgr_debug("removal link vdev %d is removed already",
				 vdev_id);
		return;
	}

	wlan_connectivity_mlo_reconfig_event(vdev);

	/* mark link removed for vdev */
	wlan_set_vdev_link_removed_flag_by_vdev_id(psoc, vdev_id,
						   true);
	status = policy_mgr_handle_ml_sta_link_state_allowed(
			psoc, MLO_LINK_FORCE_REASON_LINK_REMOVAL);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_set_vdev_link_removed_flag_by_vdev_id(psoc, vdev_id,
							   false);
		return;
	}

	policy_mgr_get_ml_sta_info(pm_ctx, &num_ml_sta, &num_disabled_ml_sta,
				   ml_sta_vdev_lst, ml_freq_lst,
				   NULL, NULL, NULL);
	if (!num_ml_sta) {
		policy_mgr_debug("unexpected event, no ml sta");
		return;
	}
	if (num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_disabled_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_ml_sta <= num_disabled_ml_sta) {
		policy_mgr_debug("unexpected ml sta num %d %d",
				 num_ml_sta, num_disabled_ml_sta);
		return;
	}
	/* Single link FW should handle BTM/disassoc and do roaming.
	 * Host will not send inactive command to FW.
	 */
	if (num_ml_sta < 2) {
		policy_mgr_debug("no op for single link mlo, num_ml_sta %d",
				 num_ml_sta);
		return;
	}

	policy_mgr_debug("removal link vdev %d num_ml_sta %d num_disabled_ml_sta %d",
			 vdev_id, num_ml_sta, num_disabled_ml_sta);

	num_active_ml_sta = num_ml_sta;
	if (num_ml_sta >= num_disabled_ml_sta)
		num_active_ml_sta = num_ml_sta - num_disabled_ml_sta;

	for (i = 0; i < num_active_ml_sta; i++)
		if (ml_sta_vdev_lst[i] == vdev_id)
			break;

	if (i == num_active_ml_sta) {
		/* no found in active ml list, it must be in inactive list */
		policy_mgr_debug("removal link vdev %d is inactive already",
				 vdev_id);

		/* send inactive command to fw again with "link removal
		 * reason"
		 */
		policy_mgr_mlo_sta_set_link(
			psoc, MLO_LINK_FORCE_REASON_LINK_REMOVAL,
			MLO_LINK_FORCE_MODE_INACTIVE,
			1, &vdev_id);
		return;
	}

	/* pick one inactive parnter link and make it active */
	if (num_active_ml_sta < num_ml_sta)
		policy_mgr_pick_link_vdev_from_inactive_list(
				psoc, vdev, num_disabled_ml_sta,
				&ml_sta_vdev_lst[num_active_ml_sta],
				&ml_freq_lst[num_active_ml_sta],
				&picked_vdev_id,
				&non_removal_link_vdev_id);
	if (picked_vdev_id != WLAN_INVALID_VDEV_ID) {
		/* find one inactive link can be active, send it to fw with
		 * the removed link together.
		 */
		policy_mgr_debug("active parnter vdev %d, inactive removal vdev %d",
				 picked_vdev_id, vdev_id);
		policy_mgr_mlo_sta_set_link_ext(
				psoc, MLO_LINK_FORCE_REASON_LINK_REMOVAL,
				MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE,
				1, &picked_vdev_id,
				1, &vdev_id);
		return;
	}
	if (num_active_ml_sta < 2) {
		/* For multi-link MLO, one link is removed and
		 * no find one inactive link can be active:
		 * 1. If at least one left link is not link removed state,
		 * host will trigger roaming.
		 * 2. If all left links are link removed state,
		 * FW will trigger roaming based on BTM or disassoc frame
		 */
		if (non_removal_link_vdev_id != WLAN_INVALID_VDEV_ID) {
			policy_mgr_debug("trigger roaming, non_removal_link_vdev_id %d",
					 non_removal_link_vdev_id);
			policy_mgr_trigger_roam_on_link_removal(vdev);
		}
		return;
	}
	/* If active link number >= 2 and one link is removed, then at least
	 * one link is still active, just send inactived command to fw.
	 */
	policy_mgr_mlo_sta_set_link(psoc, MLO_LINK_FORCE_REASON_LINK_REMOVAL,
				    MLO_LINK_FORCE_MODE_INACTIVE,
				    1, &vdev_id);
}

/**
 * policy_mgr_is_restart_sap_required_with_mlo_sta() - Check SAP required to
 * restart for force SCC with MLO STA
 * @psoc: PSOC object information
 * @sap_vdev_id: sap vdev id
 * @sap_ch_freq: sap channel frequency
 *
 * For MLO STA+SAP case, mlo link maybe in inactive state after connected
 * and the hw mode maybe not updated, check MCC/SCC by
 * policy_mgr_are_2_freq_on_same_mac may not match MCC/SCC state
 * after the link is activated by target later. So to check frequency match
 * or not to decide SAP do force SCC or not if MLO STA 2 links are present.
 *
 * Return: true if SAP is required to force SCC with MLO STA
 */
static bool
policy_mgr_is_restart_sap_required_with_mlo_sta(struct wlan_objmgr_psoc *psoc,
						uint8_t sap_vdev_id,
						qdf_freq_t sap_ch_freq)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i;
	bool same_freq_with_mlo_sta = false;
	bool restart_required = false;
	uint8_t num_ml_sta = 0, num_disabled_ml_sta = 0, num_ml_active_sta = 0;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	policy_mgr_get_ml_sta_info(pm_ctx, &num_ml_sta, &num_disabled_ml_sta,
				   ml_sta_vdev_lst, ml_freq_lst,
				   NULL, NULL, NULL);
	if (num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS) {
		policy_mgr_debug("unexpected num_ml_sta %d ", num_ml_sta);
		return false;
	}

	num_ml_active_sta = num_ml_sta;
	if (num_ml_sta >= num_disabled_ml_sta)
		num_ml_active_sta = num_ml_sta - num_disabled_ml_sta;
	for (i = 0; i < num_ml_active_sta; i++) {
		if (ml_freq_lst[i] == sap_ch_freq) {
			same_freq_with_mlo_sta = true;
			break;
		}
	}

	if (num_ml_active_sta >= 2 && !same_freq_with_mlo_sta) {
		policy_mgr_debug("SAP is not SCC with any of active MLO STA link, restart SAP");
		restart_required = true;
	}

	return restart_required;
}

void policy_mgr_activate_mlo_links_nlink(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id, uint8_t num_links,
					 struct qdf_mac_addr active_link_addr[2])
{
	uint8_t *link_mac_addr;
	struct wlan_objmgr_vdev *vdev;
	struct mlo_link_info *link_info;
	bool active_link_present = false;
	uint8_t iter, link;
	uint32_t active_link_bitmap = 0;
	uint32_t inactive_link_bitmap = 0;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev_id: %d vdev not found", vdev_id);
		return;
	}

	if (!wlan_cm_is_vdev_connected(vdev)) {
		policy_mgr_err("vdev is not in connected state");
		goto done;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		policy_mgr_err("vdev is not mlo vdev");
		goto done;
	}

	policy_mgr_debug("Num active links: %d", num_links);
	link_info = &vdev->mlo_dev_ctx->link_ctx->links_info[0];
	for (iter = 0; iter < WLAN_MAX_ML_BSS_LINKS; iter++) {
		if (link_info->link_id == WLAN_INVALID_LINK_ID) {
			link_info++;
			continue;
		}

		link_mac_addr = &link_info->link_addr.bytes[0];
		policy_mgr_debug("link addr: " QDF_MAC_ADDR_FMT,
				 QDF_MAC_ADDR_REF(link_mac_addr));

		for (link = 0; link < num_links; link++) {
			policy_mgr_debug("active addr: " QDF_MAC_ADDR_FMT,
			   QDF_MAC_ADDR_REF(&active_link_addr[link].bytes[0]));
			if (!qdf_mem_cmp(link_mac_addr,
					 &active_link_addr[link].bytes[0],
					 QDF_MAC_ADDR_SIZE)) {
				active_link_bitmap |= 1 << link_info->link_id;
				active_link_present = true;
				policy_mgr_debug("Link address match");
			}
		}
		if (!active_link_present) {
			inactive_link_bitmap |= 1 << link_info->link_id;
			policy_mgr_err("No link address match");
		}
		active_link_present = false;
		link_info++;
	}

	policy_mgr_debug("active link bitmap: 0x%x, inactive link bitmap: 0x%x",
			 active_link_bitmap, inactive_link_bitmap);

	if (!active_link_bitmap) {
		goto done;
	} else if (policy_mgr_is_emlsr_sta_concurrency_present(psoc) &&
		   !wlan_mlme_is_aux_emlsr_support(psoc)) {
		policy_mgr_debug("Concurrency exists, cannot enter EMLSR mode");
		goto done;
	}

	if (active_link_bitmap && inactive_link_bitmap)
		ml_nlink_vendor_command_set_link(
			psoc, vdev_id,
			LINK_CONTROL_MODE_USER,
			MLO_LINK_FORCE_REASON_CONNECT,
			MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE,
			0, active_link_bitmap,
			inactive_link_bitmap);
	else if (active_link_bitmap)
		ml_nlink_vendor_command_set_link(
			psoc, vdev_id,
			LINK_CONTROL_MODE_USER,
			MLO_LINK_FORCE_REASON_CONNECT,
			MLO_LINK_FORCE_MODE_ACTIVE,
			0, active_link_bitmap,
			0);
done:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
}

void policy_mgr_activate_mlo_links(struct wlan_objmgr_psoc *psoc,
				   uint8_t session_id, uint8_t num_links,
				   struct qdf_mac_addr *active_link_addr)
{
	uint8_t idx, link, active_vdev_cnt = 0, inactive_vdev_cnt = 0;
	uint16_t ml_vdev_cnt = 0;
	struct wlan_objmgr_vdev *tmp_vdev_lst[WLAN_UMAC_MLO_MAX_VDEVS] = {0};
	uint8_t active_vdev_lst[WLAN_UMAC_MLO_MAX_VDEVS] = {0};
	uint8_t inactive_vdev_lst[WLAN_UMAC_MLO_MAX_VDEVS] = {0};
	struct wlan_objmgr_vdev *vdev;
	uint8_t *link_mac_addr;
	bool active_vdev_present = false;
	uint16_t active_link_bitmap = 0;
	uint16_t inactive_link_bitmap = 0;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, session_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev_id: %d vdev not found", session_id);
		return;
	}

	if (!wlan_cm_is_vdev_connected(vdev)) {
		policy_mgr_err("vdev is not in connected state");
		goto done;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		policy_mgr_err("vdev is not mlo vdev");
		goto done;
	}

	mlo_get_ml_vdev_list(vdev, &ml_vdev_cnt, tmp_vdev_lst);
	policy_mgr_debug("Num active links: %d, ML vdev cnt: %d", num_links,
			 ml_vdev_cnt);
	for (idx = 0; idx < ml_vdev_cnt; idx++) {
		link_mac_addr = wlan_vdev_mlme_get_macaddr(tmp_vdev_lst[idx]);
		policy_mgr_debug("link addr: " QDF_MAC_ADDR_FMT,
				 QDF_MAC_ADDR_REF(link_mac_addr));
		for (link = 0; link < num_links; link++) {
			policy_mgr_debug("active addr: " QDF_MAC_ADDR_FMT,
			   QDF_MAC_ADDR_REF(&active_link_addr[link].bytes[0]));
			if (!qdf_mem_cmp(link_mac_addr,
					 &active_link_addr[link].bytes[0],
					 QDF_MAC_ADDR_SIZE)) {
				active_vdev_lst[active_vdev_cnt] =
					wlan_vdev_get_id(tmp_vdev_lst[idx]);
				active_link_bitmap |=
				1 << wlan_vdev_get_link_id(tmp_vdev_lst[idx]);
				active_vdev_cnt++;
				active_vdev_present = true;
				policy_mgr_debug("Link address match");
			}
		}
		if (!active_vdev_present) {
			inactive_vdev_lst[inactive_vdev_cnt] =
					wlan_vdev_get_id(tmp_vdev_lst[idx]);
			inactive_link_bitmap |=
			1 << wlan_vdev_get_link_id(tmp_vdev_lst[idx]);

			inactive_vdev_cnt++;
			policy_mgr_err("No link address match");
		}
		active_vdev_present = false;
	}

	policy_mgr_debug("active vdev cnt: %d, inactive vdev cnt: %d",
			 active_vdev_cnt, inactive_vdev_cnt);

	if (active_vdev_cnt &&
	    policy_mgr_is_emlsr_sta_concurrency_present(psoc)) {
		policy_mgr_debug("Concurrency exists, cannot enter EMLSR mode");
		goto ref_release;
	}

	/*
	 * If there are both active and inactive vdev count, then issue a
	 * single WMI with force mode MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE,
	 * else if there is only active vdev count, send single WMI for
	 * all active vdevs with force mode MLO_LINK_FORCE_MODE_ACTIVE.
	 */
	if (active_vdev_cnt && inactive_vdev_cnt)
		policy_mgr_mlo_sta_set_link_ext(
					psoc, MLO_LINK_FORCE_REASON_CONNECT,
					MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE,
					active_vdev_cnt, active_vdev_lst,
					inactive_vdev_cnt, inactive_vdev_lst);
	else if (active_vdev_cnt && !inactive_vdev_cnt)
		policy_mgr_mlo_sta_set_link(psoc,
					    MLO_LINK_FORCE_REASON_DISCONNECT,
					    MLO_LINK_FORCE_MODE_ACTIVE,
					    active_vdev_cnt, active_vdev_lst);
ref_release:
	for (idx = 0; idx < ml_vdev_cnt; idx++)
		mlo_release_vdev_ref(tmp_vdev_lst[idx]);
done:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
}

static QDF_STATUS
policy_mgr_is_link_active_allowed(struct wlan_objmgr_psoc *psoc,
				  struct mlo_link_info *link_info,
				  uint8_t num_links)
{
	uint16_t ch_freq;
	struct wlan_channel *chan_info;

	if (num_links > 1 &&
	    !policy_mgr_is_hw_dbs_capable(psoc)) {
		chan_info = link_info->link_chan_info;
		ch_freq = chan_info->ch_freq;
		if (wlan_reg_freq_to_band((qdf_freq_t)ch_freq) ==
		    REG_BAND_2G) {
			policy_mgr_err("vdev:  Invalid link activation for link: %d at freq: %d in  HW mode: %d",
				       link_info->vdev_id, link_info->link_id,
				       ch_freq,
				       POLICY_MGR_HW_MODE_AUX_EMLSR_SINGLE);
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_update_mlo_links_based_on_linkid_nlink(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					uint8_t num_links,
					uint8_t *link_id_list,
					uint32_t *config_state_list)
{
	uint8_t *link_mac_addr;
	struct wlan_objmgr_vdev *vdev;
	struct mlo_link_info *link_info;
	uint8_t iter, link;
	uint32_t active_link_bitmap = 0;
	uint32_t inactive_link_bitmap = 0;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev_id: %d vdev not found", vdev_id);
		return status;
	}

	if (!wlan_cm_is_vdev_connected(vdev)) {
		policy_mgr_err("vdev is not in connected state");
		goto release_vdev_ref;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		policy_mgr_err("vdev is not mlo vdev");
		goto release_vdev_ref;
	}
	if (policy_mgr_is_emlsr_sta_concurrency_present(psoc) &&
	    !wlan_mlme_is_aux_emlsr_support(psoc)) {
		policy_mgr_debug("Concurrency exists, cannot enter EMLSR mode");
		goto release_vdev_ref;
	}

	policy_mgr_debug("Num active links: %d", num_links);
	link_info = &vdev->mlo_dev_ctx->link_ctx->links_info[0];
	for (iter = 0; iter < WLAN_MAX_ML_BSS_LINKS; iter++) {
		if (link_info->link_id == WLAN_INVALID_LINK_ID) {
			link_info++;
			continue;
		}
		if (qdf_is_macaddr_zero(&link_info->ap_link_addr)) {
			link_info++;
			continue;
		}

		link_mac_addr = &link_info->link_addr.bytes[0];
		policy_mgr_debug("linkid %d freq %d link addr: " QDF_MAC_ADDR_FMT,
				 link_info->link_id,
				 link_info->chan_freq,
				 QDF_MAC_ADDR_REF(link_mac_addr));

		for (link = 0; link < num_links; link++) {
			if (link_id_list[link] != link_info->link_id)
				continue;
			if (config_state_list[link]) {
				if (policy_mgr_is_link_active_allowed(
								psoc,
								link_info,
								num_links) !=
				    QDF_STATUS_SUCCESS)
					goto release_vdev_ref;

				active_link_bitmap |= 1 << link_info->link_id;
				policy_mgr_debug("link id:%d matched to active",
						 link_info->link_id);

			} else {
				inactive_link_bitmap |= 1 << link_info->link_id;
				policy_mgr_debug("link id:%d matched to inactive",
						 link_info->link_id);
			}
		}

		link_info++;
	}

	policy_mgr_debug("active link bitmap: %d, inactive link bitmap: %d",
			 active_link_bitmap, inactive_link_bitmap);
	if (active_link_bitmap && inactive_link_bitmap)
		status = ml_nlink_vendor_command_set_link(
				psoc, vdev_id,
				LINK_CONTROL_MODE_USER,
				MLO_LINK_FORCE_REASON_CONNECT,
				MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE,
				0, active_link_bitmap,
				inactive_link_bitmap);
	else if (active_link_bitmap)
		status = ml_nlink_vendor_command_set_link(
				psoc, vdev_id,
				LINK_CONTROL_MODE_USER,
				MLO_LINK_FORCE_REASON_CONNECT,
				MLO_LINK_FORCE_MODE_ACTIVE,
				0, active_link_bitmap,
				0);

release_vdev_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return status;
}

QDF_STATUS
policy_mgr_update_mlo_links_based_on_linkid(struct wlan_objmgr_psoc *psoc,
					    uint8_t vdev_id,
					    uint8_t num_links,
					    uint8_t *link_id_list,
					    uint32_t *config_state_list)
{
	uint8_t idx, link, active_vdev_cnt = 0, inactive_vdev_cnt = 0;
	uint16_t ml_vdev_cnt = 0;
	struct wlan_objmgr_vdev *vdev_lst[WLAN_UMAC_MLO_MAX_VDEVS] = {0};
	uint8_t active_vdev_lst[WLAN_UMAC_MLO_MAX_VDEVS] = {0};
	uint8_t inactive_vdev_lst[WLAN_UMAC_MLO_MAX_VDEVS] = {0};
	struct wlan_objmgr_vdev *vdev;
	uint8_t link_id, num_links_to_disable = 0, num_matched_linkid = 0;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t num_ml_sta = 0, num_disabled_ml_sta = 0, num_non_ml = 0;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t active_link_bitmap = 0;
	uint32_t inactive_link_bitmap = 0;

	for (idx = 0; idx < num_links; idx++) {
		if (config_state_list[idx] == 0)
			num_links_to_disable++;
	}

	if (num_links_to_disable == num_links) {
		policy_mgr_debug("vdev: %d num_links_to_disable: %d", vdev_id,
				 num_links_to_disable);
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev: %d vdev not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (!wlan_cm_is_vdev_connected(vdev)) {
		policy_mgr_err("vdev: %d is not in connected state", vdev_id);
		goto release_vdev_ref;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		policy_mgr_err("vdev:%d is not mlo vdev", vdev_id);
		goto release_vdev_ref;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm context");
		goto release_vdev_ref;
	}

	mlo_get_ml_vdev_list(vdev, &ml_vdev_cnt, vdev_lst);
	for (idx = 0; idx < ml_vdev_cnt; idx++) {
		link_id = wlan_vdev_get_link_id(vdev_lst[idx]);
		for (link = 0; link < num_links; link++) {
			if (link_id_list[link] == link_id) {
				num_matched_linkid++;
				policy_mgr_debug("link id:%d match", link_id);
				if (config_state_list[link]) {
					active_vdev_lst[active_vdev_cnt] =
						wlan_vdev_get_id(vdev_lst[idx]);
					active_link_bitmap |= 1 << link_id;
					active_vdev_cnt++;
				} else {
					inactive_vdev_lst[inactive_vdev_cnt] =
						wlan_vdev_get_id(vdev_lst[idx]);
					inactive_link_bitmap |= 1 << link_id;
					inactive_vdev_cnt++;
				}
			}
		}
	}

	policy_mgr_debug("vdev: %d, active links: %d, ml count: %d, active count: %d, inactive count: %d",
			 vdev_id, num_links, ml_vdev_cnt, active_vdev_cnt,
			 inactive_vdev_cnt);

	if (num_links != num_matched_linkid) {
		policy_mgr_debug("invalid link id(s), num_matched_linkid: %d",
				 num_matched_linkid);
		goto release_ml_vdev_ref;
	}

	if (active_vdev_cnt &&
	    policy_mgr_is_emlsr_sta_concurrency_present(psoc)) {
		policy_mgr_debug("vdev: %d emlsr sta conn present", vdev_id);
		if (active_vdev_cnt == 1)
			status = QDF_STATUS_SUCCESS;
		goto release_ml_vdev_ref;
	}

	policy_mgr_get_ml_sta_info(pm_ctx, &num_ml_sta, &num_disabled_ml_sta,
				   ml_sta_vdev_lst, ml_freq_lst, &num_non_ml,
				   NULL, NULL);
	policy_mgr_debug("vdev %d: num_ml_sta %d disabled %d num_non_ml: %d",
			 vdev_id, num_ml_sta, num_disabled_ml_sta, num_non_ml);

	/*
	 * No ML STA is present or sinle link ML is present or
	 * more no.of links are active than supported concurrent connections
	 */
	if (!num_ml_sta || num_ml_sta < 2 ||
	    num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS)
		goto release_ml_vdev_ref;

	if (!num_disabled_ml_sta) {
		/*
		 * both link are already enabled and received set link req to
		 * enable both again
		 */
		if (active_vdev_cnt && !inactive_vdev_cnt) {
			status = QDF_STATUS_SUCCESS;
			goto release_ml_vdev_ref;
		}

		/*
		 * both link are already enabled and received set link req
		 * disable one link, disable any
		 */
		if (active_vdev_cnt && inactive_vdev_cnt) {
			status = policy_mgr_mlo_sta_set_link_ext(psoc,
					MLO_LINK_FORCE_REASON_CONNECT,
					MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE,
					active_vdev_cnt, active_vdev_lst,
					inactive_vdev_cnt, inactive_vdev_lst);
			goto release_ml_vdev_ref;
		}
	} else {
		/*
		 * one link is enable and one link is disabled, If disabled
		 * link can not be allowed to enable then send status failure
		 * to upper layer.
		 */
		if (active_vdev_cnt &&
		    !policy_mgr_sta_ml_link_enable_allowed(psoc,
							   num_disabled_ml_sta,
							   num_ml_sta,
							   ml_freq_lst,
							   ml_sta_vdev_lst)) {
			policy_mgr_debug("vdev %d: link enable not allowed",
					 vdev_id);
			goto release_ml_vdev_ref;
		}

		/*
		 * If there are both active and inactive vdev count, then
		 * issue a single WMI with force mode
		 * MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE, else if there is only
		 * active vdev count, send single WMI for all active vdevs
		 * with force mode MLO_LINK_FORCE_MODE_ACTIVE.
		 */
		if (active_vdev_cnt && inactive_vdev_cnt)
			status = policy_mgr_mlo_sta_set_link_ext(psoc,
					MLO_LINK_FORCE_REASON_CONNECT,
					MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE,
					active_vdev_cnt, active_vdev_lst,
					inactive_vdev_cnt, inactive_vdev_lst);
		else if (active_vdev_cnt && !inactive_vdev_cnt)
			status = policy_mgr_mlo_sta_set_link(psoc,
					MLO_LINK_FORCE_REASON_DISCONNECT,
					MLO_LINK_FORCE_MODE_ACTIVE,
					active_vdev_cnt, active_vdev_lst);
	}

release_ml_vdev_ref:
	for (idx = 0; idx < ml_vdev_cnt; idx++)
		mlo_release_vdev_ref(vdev_lst[idx]);
release_vdev_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return status;
}

/**
 * policy_mgr_process_mlo_sta_dynamic_force_num_link() - Set links for MLO STA
 * @psoc: psoc object
 * @reason: Reason for which link is forced
 * @mode: Force reason
 * @num_mlo_vdev: number of mlo vdev
 * @mlo_vdev_lst: MLO STA vdev list
 * @force_active_cnt: number of MLO links to operate in active state as per
 * user req
 *
 * User space provides the desired number of MLO links to operate in active
 * state at any given time. Host validate request as per current concurrency
 * and send SET LINK requests to FW. FW will choose which MLO links should
 * operate in the active state.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
policy_mgr_process_mlo_sta_dynamic_force_num_link(struct wlan_objmgr_psoc *psoc,
				enum mlo_link_force_reason reason,
				enum mlo_link_force_mode mode,
				uint8_t num_mlo_vdev, uint8_t *mlo_vdev_lst,
				uint8_t force_active_cnt)
{
	struct mlo_link_set_active_req *req;
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev;

	if (!num_mlo_vdev) {
		policy_mgr_err("invalid 0 num_mlo_vdev");
		return QDF_STATUS_E_INVAL;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_INVAL;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, mlo_vdev_lst[0],
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev %d: invalid vdev", mlo_vdev_lst[0]);
		qdf_mem_free(req);
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_debug("vdev %d: mode %d num_mlo_vdev %d reason %d",
			 wlan_vdev_get_id(vdev), mode, num_mlo_vdev, reason);

	/*
	 * TODO: this API has to be merged with policy_mgr_mlo_sta_set_link_ext
	 * as part of 3 link FR change as in caller only we have to decide how
	 * many links to disable/enable for MLO_LINK_FORCE_MODE_ACTIVE_NUM or
	 * MLO_LINK_FORCE_MODE_INACTIVE_NUM scenario
	 */
	req->ctx.vdev = vdev;
	req->param.reason = reason;
	req->param.force_mode = mode;
	req->ctx.set_mlo_link_cb = policy_mgr_handle_link_enable_disable_resp;
	req->ctx.validate_set_mlo_link_cb =
		policy_mgr_validate_set_mlo_link_cb;
	req->ctx.cb_arg = req;

	/* set MLO vdev bit mask */
	policy_mgr_fill_ml_active_link_vdev_bitmap(req, mlo_vdev_lst,
						   num_mlo_vdev);

	pm_ctx->active_vdev_bitmap = req->param.vdev_bitmap[0];
	pm_ctx->inactive_vdev_bitmap = req->param.vdev_bitmap[1];

	req->param.num_link_entry = 1;
	req->param.link_num[0].num_of_link = force_active_cnt;
	req->param.control_flags.dynamic_force_link_num = 1;

	if (ml_is_nlink_service_supported(psoc)) {
		status =
		policy_mgr_mlo_sta_set_link_by_linkid(psoc, vdev, reason,
						      mode,
						      req->param.link_num[0].
						      num_of_link,
						      num_mlo_vdev,
						      mlo_vdev_lst,
						      0,
						      NULL);
		qdf_mem_free(req);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

		if (status != QDF_STATUS_E_PENDING) {
			policy_mgr_err("set_link_by_linkid status %d", status);
			return status;
		}
		return QDF_STATUS_SUCCESS;
	}

	policy_mgr_set_link_in_progress(pm_ctx, true);

	status = mlo_ser_set_link_req(req);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("vdev %d: Failed to set link mode %d num_mlo_vdev %d force_active_cnt: %d, reason %d",
			       wlan_vdev_get_id(vdev), mode, num_mlo_vdev,
			       force_active_cnt,
			       reason);
		qdf_mem_free(req);
		policy_mgr_set_link_in_progress(pm_ctx, false);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	return status;
}

QDF_STATUS
policy_mgr_update_active_mlo_num_nlink(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id,
				       uint8_t force_active_cnt)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlo_link_info *link_info;
	uint8_t iter, cnt = 0;
	uint8_t *link_mac_addr;
	uint32_t link_bitmap = 0;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (policy_mgr_is_emlsr_sta_concurrency_present(psoc) &&
	    !wlan_mlme_is_aux_emlsr_support(psoc)) {
		policy_mgr_debug("Concurrency exists, cannot enter EMLSR mode");
		return QDF_STATUS_E_FAILURE;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev_id: %d vdev not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		goto release_vdev_ref;

	if (!wlan_cm_is_vdev_connected(vdev)) {
		policy_mgr_err("vdev is not in connected state");
		goto release_vdev_ref;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		policy_mgr_err("vdev is not mlo vdev");
		goto release_vdev_ref;
	}
	policy_mgr_debug("Num active links: %d", force_active_cnt);
	link_info = &vdev->mlo_dev_ctx->link_ctx->links_info[0];
	for (iter = 0; iter < WLAN_MAX_ML_BSS_LINKS; iter++) {
		if (cnt >= force_active_cnt)
			break;
		if (link_info->link_id == WLAN_INVALID_LINK_ID) {
			link_info++;
			continue;
		}

		if (qdf_is_macaddr_zero(&link_info->ap_link_addr)) {
			link_info++;
			continue;
		}

		if (link_info->vdev_id == WLAN_INVALID_VDEV_ID) {
			link_info++;
			continue;
		}

		link_mac_addr = &link_info->link_addr.bytes[0];
		policy_mgr_debug("linkid %d freq %d link addr: " QDF_MAC_ADDR_FMT,
				 link_info->link_id,
				 link_info->chan_freq,
				 QDF_MAC_ADDR_REF(link_mac_addr));
		link_bitmap |= 1 << link_info->link_id;

		link_info++;
	}

	policy_mgr_debug("link_bitmap: %d, force_active_cnt: %d",
			 link_bitmap, force_active_cnt);

	status = ml_nlink_vendor_command_set_link(
			psoc, vdev_id,
			LINK_CONTROL_MODE_MIXED,
			MLO_LINK_FORCE_REASON_CONNECT,
			MLO_LINK_FORCE_MODE_ACTIVE_NUM,
			force_active_cnt,
			link_bitmap,
			0);

release_vdev_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	return status;
}

QDF_STATUS policy_mgr_update_active_mlo_num_links(struct wlan_objmgr_psoc *psoc,
						  uint8_t vdev_id,
						  uint8_t force_active_cnt)
{
	struct wlan_objmgr_vdev *vdev;
	uint8_t num_ml_sta = 0, num_disabled_ml_sta = 0, num_non_ml = 0;
	uint8_t num_enabled_ml_sta = 0, conn_count;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (policy_mgr_is_emlsr_sta_concurrency_present(psoc)) {
		policy_mgr_debug("Concurrency exists, cannot enter EMLSR mode");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev_id: %d vdev not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		goto release_vdev_ref;

	if (!wlan_cm_is_vdev_connected(vdev)) {
		policy_mgr_err("vdev is not in connected state");
		goto release_vdev_ref;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		policy_mgr_err("vdev is not mlo vdev");
		goto release_vdev_ref;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		goto release_vdev_ref;
	}

	conn_count = policy_mgr_get_connection_count(psoc);
	policy_mgr_get_ml_sta_info(pm_ctx, &num_ml_sta, &num_disabled_ml_sta,
				   ml_sta_vdev_lst, ml_freq_lst, &num_non_ml,
				   NULL, NULL);
	policy_mgr_debug("vdev %d: num_ml_sta %d disabled %d num_non_ml: %d conn cout %d",
			 vdev_id, num_ml_sta, num_disabled_ml_sta, num_non_ml,
			 conn_count);

	/*
	 * No ML STA is present or more no.of links are active than supported
	 * concurrent connections
	 */
	if (!num_ml_sta || num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS)
		goto release_vdev_ref;

	/*
	 * DUT connected with MLO AP, one link is always active, So if
	 * request comes to make one link is active, host sends set link command
	 * with mode MLO_LINK_FORCE_MODE_ACTIVE_NUM, so that FW should restrict
	 * to only one link and avoid switch from MLSR to MLMR.
	 */
	if (force_active_cnt == 1)
		goto set_link;

	/*
	 * num_disabled_ml_sta == 0, means 2 link is already active,
	 * In this case send set link command with num link 2 and mode
	 * MLO_LINK_FORCE_MODE_ACTIVE_NUM, so that FW should restrict to only
	 * in MLMR mode (2 link should be active).
	 * If current enabled links are < 2, and there are concurrent
	 * connection present, force active 2 links, which may be
	 * conflict with concurrent rules, reject it.
	 * If the two enabled links are MCC, don't not force active 2 links.
	 */
	num_enabled_ml_sta = num_ml_sta;
	if (num_ml_sta >= num_disabled_ml_sta)
		num_enabled_ml_sta = num_ml_sta - num_disabled_ml_sta;

	if (force_active_cnt >= 2) {
		if (num_ml_sta < 2) {
			policy_mgr_debug("num_ml_sta %d < 2, can't force active cnt %d",
					 num_ml_sta,
					 force_active_cnt);
			goto release_vdev_ref;
		}
		if (num_enabled_ml_sta < 2 &&
		    conn_count > num_enabled_ml_sta) {
			policy_mgr_debug("enabled link num %d < 2, concurrent conn present %d",
					 num_enabled_ml_sta,
					 conn_count);
			goto release_vdev_ref;
		}
		if (policy_mgr_is_ml_sta_links_in_mcc(
					psoc, ml_freq_lst,
					ml_sta_vdev_lst,
					NULL, num_ml_sta,
					NULL)) {
			policy_mgr_debug("enabled links are mcc, concurrency disallow");
			goto release_vdev_ref;
		}
	}
	if (force_active_cnt == 2 && num_disabled_ml_sta == 0)
		goto set_link;

	/* Link can not be allowed to enable then skip checking further */
	if (!policy_mgr_sta_ml_link_enable_allowed(psoc, num_disabled_ml_sta,
						   num_ml_sta, ml_freq_lst,
						   ml_sta_vdev_lst)) {
		policy_mgr_debug("vdev %d: link enable not allowed", vdev_id);
		goto release_vdev_ref;
	}

set_link:
	/*
	 * TODO: In all scenarios wherever host sends
	 * MLO_LINK_FORCE_MODE_ACTIVE_NUM/MLO_LINK_FORCE_MODE_INACTIVE_NUM to
	 * FW, Host need to send MLO_LINK_FORCE_MODE_NO_FORCE to FW.
	 * So instead of two commands for serialization, take care of this in
	 * single serialization active command.
	 */

	/*
	 * send MLO_LINK_FORCE_MODE_NO_FORCE to FW to clear user mode setting
	 * configured via QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE in FW
	 */
	status = policy_mgr_mlo_sta_set_link(psoc,
				    MLO_LINK_FORCE_REASON_CONNECT,
				    MLO_LINK_FORCE_MODE_NO_FORCE,
				    num_ml_sta, ml_sta_vdev_lst);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_debug("fail to send no force cmd for num_links:%d",
				 num_ml_sta);
		goto release_vdev_ref;
	} else {
		policy_mgr_debug("clear force mode setting for num_links:%d",
				 num_ml_sta);
	}

	status = policy_mgr_process_mlo_sta_dynamic_force_num_link(psoc,
					     MLO_LINK_FORCE_REASON_CONNECT,
					     MLO_LINK_FORCE_MODE_ACTIVE_NUM,
					     num_ml_sta, ml_sta_vdev_lst,
					     force_active_cnt);
	if (QDF_IS_STATUS_SUCCESS(status))
		policy_mgr_debug("vdev %d: link enable allowed", vdev_id);

release_vdev_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	return status;
}

QDF_STATUS
policy_mgr_clear_ml_links_settings_in_fw_nlink(struct wlan_objmgr_psoc *psoc,
					       uint8_t vdev_id)
{
	/* Clear all user vendor command setting for switching to "default" */
	return ml_nlink_vendor_command_set_link(
					psoc, vdev_id,
					LINK_CONTROL_MODE_DEFAULT,
					0, 0, 0, 0, 0);
}

QDF_STATUS
policy_mgr_clear_ml_links_settings_in_fw(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	uint8_t num_ml_sta = 0, num_disabled_ml_sta = 0, num_non_ml = 0;
	uint8_t ml_sta_vdev_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	qdf_freq_t ml_freq_lst[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t num_link_to_no_force = 0, num_active_ml_sta = 0;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("vdev: %d vdev not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		goto release_vdev_ref;

	if (!wlan_cm_is_vdev_connected(vdev)) {
		policy_mgr_err("vdev: %d is not in connected state", vdev_id);
		goto release_vdev_ref;
	}

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		policy_mgr_err("vdev: %d is not mlo vdev", vdev_id);
		goto release_vdev_ref;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("vdev: %d Invalid Context", vdev_id);
		goto release_vdev_ref;
	}

	policy_mgr_get_ml_sta_info(pm_ctx, &num_ml_sta, &num_disabled_ml_sta,
				   ml_sta_vdev_lst, ml_freq_lst, &num_non_ml,
				   NULL, NULL);
	policy_mgr_debug("vdev %d: num_ml_sta %d disabled %d num_non_ml: %d",
			 vdev_id, num_ml_sta, num_disabled_ml_sta, num_non_ml);

	if (!num_ml_sta || num_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS ||
	    num_disabled_ml_sta > MAX_NUMBER_OF_CONC_CONNECTIONS)
		goto release_vdev_ref;

	num_active_ml_sta = num_ml_sta;
	if (num_ml_sta >= num_disabled_ml_sta)
		num_active_ml_sta = num_ml_sta - num_disabled_ml_sta;

	num_link_to_no_force += num_active_ml_sta;

	/* Link can not be allowed to enable then skip checking further */
	if (policy_mgr_sta_ml_link_enable_allowed(psoc, num_disabled_ml_sta,
						  num_ml_sta, ml_freq_lst,
						  ml_sta_vdev_lst)) {
		num_link_to_no_force += num_disabled_ml_sta;
		policy_mgr_debug("Link enable allowed, total num_links: %d",
				 num_link_to_no_force);
	}

	if (num_link_to_no_force < 1 ||
	    num_link_to_no_force > MAX_NUMBER_OF_CONC_CONNECTIONS) {
		policy_mgr_debug("vdev %d: invalid num_link_to_no_force: %d",
				 vdev_id, num_link_to_no_force);
		goto release_vdev_ref;
	}

	/*
	 * send WMI_MLO_LINK_SET_ACTIVE_CMDID to clear user mode setting
	 * configured via QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE in FW
	 */
	status = policy_mgr_mlo_sta_set_link(psoc,
					MLO_LINK_FORCE_REASON_CONNECT,
					MLO_LINK_FORCE_MODE_NO_FORCE,
					num_link_to_no_force,
					ml_sta_vdev_lst);
	if (QDF_IS_STATUS_ERROR(status))
		goto release_vdev_ref;
	else
		policy_mgr_debug("clear user mode setting for num_links:%d",
				 num_link_to_no_force);

	/*
	 * send WMI_MLO_LINK_SET_ACTIVE_CMDID with value of
	 * num_link as 0 to clear dynamic mode setting configured
	 * via vendor attr LINK_STATE_MIXED_MODE_ACTIVE_NUM_LINKS in FW
	 */
	status = policy_mgr_process_mlo_sta_dynamic_force_num_link(psoc,
				MLO_LINK_FORCE_REASON_CONNECT,
				MLO_LINK_FORCE_MODE_ACTIVE_NUM,
				num_link_to_no_force, ml_sta_vdev_lst, 0);
	if (QDF_IS_STATUS_SUCCESS(status))
		policy_mgr_debug("clear mixed mode setting for num_links:%d",
				 num_link_to_no_force);
release_vdev_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	return status;
}

#else
static bool
policy_mgr_allow_sta_concurrency(struct wlan_objmgr_psoc *psoc,
				 qdf_freq_t freq,
				 uint32_t ext_flags)
{
	uint32_t count;

	count = policy_mgr_mode_specific_connection_count(psoc, PM_STA_MODE,
							  NULL);
	if (!count)
		return true;

	if (count >= 2) {
		policy_mgr_rl_debug("Disallow 3rd STA");
		return false;
	}

	if (!policy_mgr_allow_multiple_sta_connections(psoc)) {
		policy_mgr_rl_debug("Multiple STA connections is not allowed");
		return false;
	}

	return true;
}

static bool
policy_mgr_is_restart_sap_required_with_mlo_sta(struct wlan_objmgr_psoc *psoc,
						uint8_t sap_vdev_id,
						qdf_freq_t sap_ch_freq)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_P2P_P2P_STA
bool policy_mgr_is_p2p_p2p_conc_supported(struct wlan_objmgr_psoc *psoc)
{
	return wlan_mlme_get_p2p_p2p_conc_support(psoc);
}
#endif

/**
 * policy_mgr_is_third_conn_sta_p2p_p2p_valid: This API checks the firmware
 * capability and allows STA + P2P + P2P combination. It can be in SCC/MCC/DBS
 * @psoc: psoc pointer
 * @new_conn_mode: third connection mode
 *
 * Return: true if support else false
 */
static bool policy_mgr_is_third_conn_sta_p2p_p2p_valid(
					struct wlan_objmgr_psoc *psoc,
					enum policy_mgr_con_mode new_conn_mode)
{
	int num_sta, num_go, num_cli;

	num_sta = policy_mgr_mode_specific_connection_count(psoc,
							    PM_STA_MODE,
							    NULL);

	num_go = policy_mgr_mode_specific_connection_count(psoc,
							   PM_P2P_GO_MODE,
							   NULL);

	num_cli = policy_mgr_mode_specific_connection_count(psoc,
							    PM_P2P_CLIENT_MODE,
							    NULL);

	if (num_sta + num_go + num_cli != 2)
		return true;

	/* If STA + P2P + another STA comes up then return true
	 * as this API is only for two port P2P + single STA combo
	 * checks
	 */
	if (num_sta == 1 && new_conn_mode == PM_STA_MODE)
		return true;

	if ((((PM_STA_MODE == pm_conc_connection_list[0].mode &&
	       PM_P2P_GO_MODE == pm_conc_connection_list[1].mode) ||
	      (PM_P2P_GO_MODE == pm_conc_connection_list[0].mode &&
	       PM_STA_MODE == pm_conc_connection_list[1].mode))
	      ||
	      (PM_P2P_GO_MODE == pm_conc_connection_list[0].mode &&
	       PM_P2P_GO_MODE == pm_conc_connection_list[1].mode)
	      ||
	      ((PM_STA_MODE == pm_conc_connection_list[0].mode &&
		PM_P2P_CLIENT_MODE == pm_conc_connection_list[1].mode) ||
	       (PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode &&
		PM_STA_MODE == pm_conc_connection_list[1].mode))
	      ||
	      (PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode &&
	       PM_P2P_CLIENT_MODE == pm_conc_connection_list[1].mode)
	      ||
	      ((PM_P2P_GO_MODE == pm_conc_connection_list[0].mode &&
		PM_P2P_CLIENT_MODE == pm_conc_connection_list[1].mode) ||
	       (PM_P2P_CLIENT_MODE == pm_conc_connection_list[0].mode &&
		PM_P2P_GO_MODE == pm_conc_connection_list[1].mode))) &&
	      num_sta <= 1) {
		if ((new_conn_mode == PM_STA_MODE ||
		     new_conn_mode == PM_P2P_CLIENT_MODE ||
		     new_conn_mode == PM_P2P_GO_MODE) &&
		    !policy_mgr_is_p2p_p2p_conc_supported(psoc))
			return false;
	}

	return true;
}

bool policy_mgr_is_concurrency_allowed(struct wlan_objmgr_psoc *psoc,
				       enum policy_mgr_con_mode mode,
				       uint32_t ch_freq,
				       enum hw_mode_bandwidth bw,
				       uint32_t ext_flags,
				       struct policy_mgr_pcl_list *pcl)
{
	uint32_t num_connections = 0, count = 0, index = 0, i;
	bool status = false, match = false;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool sta_sap_scc_on_dfs_chan;
	bool go_force_scc;
	enum channel_state chan_state;
	bool is_dfs_ch = false;
	struct ch_params ch_params;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return status;
	}
	/* find the current connection state from pm_conc_connection_list*/
	num_connections = policy_mgr_get_connection_count(psoc);

	if (num_connections && policy_mgr_is_sub_20_mhz_enabled(psoc)) {
		policy_mgr_rl_debug("dont allow concurrency if Sub 20 MHz is enabled");
		return status;
	}

	if (policy_mgr_max_concurrent_connections_reached(psoc)) {
		policy_mgr_rl_debug("Reached max concurrent connections: %d",
				    pm_ctx->cfg.max_conc_cxns);
		policy_mgr_validate_conn_info(psoc);
		return status;
	}

	if (ch_freq) {
		if (wlan_reg_is_5ghz_ch_freq(ch_freq)) {
			qdf_mem_zero(&ch_params, sizeof(ch_params));
			ch_params.ch_width = policy_mgr_get_ch_width(bw);
			chan_state =
			wlan_reg_get_5g_bonded_channel_state_for_pwrmode(
					pm_ctx->pdev, ch_freq,
					&ch_params, REG_CURRENT_PWR_MODE);
			if (chan_state == CHANNEL_STATE_DFS)
				is_dfs_ch = true;
		}
		/* don't allow 3rd home channel on same MAC
		 * also check for single mac target which doesn't
		 * support interbad MCC as well
		 */
		if (!policy_mgr_allow_new_home_channel(psoc, mode, ch_freq,
						       num_connections,
						       is_dfs_ch,
						       ext_flags))
			return status;

		/*
		 * 1) DFS MCC is not yet supported
		 * 2) If you already have STA connection on 5G channel then
		 *    don't allow any other persona to make connection on DFS
		 *    channel because STA 5G + DFS MCC is not allowed.
		 * 3) If STA is on 2G channel and SAP is coming up on
		 *    DFS channel then allow concurrency but make sure it is
		 *    going to DBS and send PCL to firmware indicating that
		 *    don't allow STA to roam to 5G channels.
		 * 4) On a single MAC device, if a SAP/P2PGO is already on a DFS
		 *    channel, don't allow a 2 channel as it will result
		 *    in MCC which is not allowed.
		 */
		if (!policy_mgr_is_5g_channel_allowed(psoc,
			ch_freq, list, PM_P2P_GO_MODE))
			return status;
		if (!policy_mgr_is_5g_channel_allowed(psoc,
			ch_freq, list, PM_SAP_MODE))
			return status;
		if (!policy_mgr_is_6g_channel_allowed(psoc, mode,
						      ch_freq))
			return status;

		sta_sap_scc_on_dfs_chan =
			policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);
		go_force_scc = policy_mgr_go_scc_enforced(psoc);
		if ((mode == PM_SAP_MODE || mode == PM_P2P_GO_MODE) &&
		    (!sta_sap_scc_on_dfs_chan ||
		     !policy_mgr_is_sta_sap_scc(psoc, ch_freq) ||
		     (!go_force_scc && mode == PM_P2P_GO_MODE))) {
			if (is_dfs_ch)
				match = policy_mgr_disallow_mcc(psoc,
								ch_freq);
		}
		if (true == match) {
			policy_mgr_rl_debug("No MCC, SAP/GO about to come up on DFS channel");
			return status;
		}
		if ((policy_mgr_is_hw_dbs_capable(psoc) != true) &&
		    num_connections) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq)) {
				if (policy_mgr_is_sap_p2pgo_on_dfs(psoc)) {
					policy_mgr_rl_debug("MCC not allowed: SAP/P2PGO on DFS");
					return status;
				}
			}
		}
	}

	if (mode == PM_STA_MODE &&
	    !policy_mgr_allow_sta_concurrency(psoc, ch_freq, ext_flags))
		return status;

	if (!policy_mgr_allow_sap_go_concurrency(psoc, mode, ch_freq,
						 WLAN_INVALID_VDEV_ID)) {
		policy_mgr_rl_debug("This concurrency combination is not allowed");
		return status;
	}

	/*
	 * don't allow two P2P GO on same band, if fw doesn't
	 * support p2p +p2p concurrency
	 */
	if (ch_freq && mode == PM_P2P_GO_MODE && num_connections &&
	    !policy_mgr_is_p2p_p2p_conc_supported(psoc)) {
		index = 0;
		count = policy_mgr_mode_specific_connection_count(
				psoc, PM_P2P_GO_MODE, list);
		qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
		while (index < count) {
			if (WLAN_REG_IS_SAME_BAND_FREQS(
			    ch_freq,
			    pm_conc_connection_list[list[index]].freq)) {
				policy_mgr_rl_debug("Don't allow P2P GO on same band");
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				return status;
			}
			index++;
		}
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	}

	if (!policy_mgr_allow_wapi_concurrency(pm_ctx)) {
		policy_mgr_rl_debug("Don't allow new conn when wapi security conn existing");
		return status;
	}

	/* Allow sta+p2p+p2p only if firmware supports the capability */
	if (!policy_mgr_is_third_conn_sta_p2p_p2p_valid(psoc, mode)) {
		policy_mgr_err("Don't allow third connection as GO or GC or STA with old fw");
		return status;
	}

	/*
	 * Don't allow DFS SAP on non-SCC channels if an ML-STA is already
	 * present. PCL list returns the SCC channels and all channels from
	 * other MAC in case of non-ML/single link STA.
	 */
	if (mode == PM_SAP_MODE && pcl &&
	    wlan_reg_is_dfs_for_freq(pm_ctx->pdev, ch_freq)) {
		for (i = 0; i < pcl->pcl_len; i++)
			if (pcl->pcl_list[i] == ch_freq) {
				status = true;
				break;
			}
		if (!status) {
			policy_mgr_err("SAP channel %d Not present in PCL",
				       ch_freq);
			return status;
		}
	}
	status = true;

	return status;
}

bool policy_mgr_allow_concurrency(struct wlan_objmgr_psoc *psoc,
				  enum policy_mgr_con_mode mode,
				  uint32_t ch_freq,
				  enum hw_mode_bandwidth bw,
				  uint32_t ext_flags, uint8_t vdev_id)
{
	QDF_STATUS status;
	struct policy_mgr_pcl_list pcl;
	bool allowed;
	uint8_t i = 0;

	qdf_mem_zero(&pcl, sizeof(pcl));
	status = policy_mgr_get_pcl(psoc, mode, pcl.pcl_list, &pcl.pcl_len,
				    pcl.weight_list,
				    QDF_ARRAY_SIZE(pcl.weight_list), vdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("disallow connection:%d", status);
		return false;
	}

	allowed = policy_mgr_is_concurrency_allowed(psoc, mode, ch_freq,
						    bw, ext_flags, &pcl);

	/* Fourth connection concurrency check */
	if (allowed && policy_mgr_get_connection_count(psoc) == 3)
		allowed = policy_mgr_is_concurrency_allowed_4_port(
				psoc,
				mode,
				ch_freq,
				pcl);
	/* Fifth connection concurrency check*/
	if (allowed && policy_mgr_get_connection_count(psoc) == 4 &&
	    (wlan_nan_is_sta_sap_nan_allowed(psoc) ||
	     wlan_nan_is_sta_p2p_ndp_supported(psoc))) {
		if (mode == QDF_NDI_MODE) {
			return true;
		} else if (mode == PM_SAP_MODE || mode == PM_P2P_GO_MODE) {
			for (i = 0; i < pcl.pcl_len; i++)
				if (ch_freq == pcl.pcl_list[i])
					return true;
		}
	}
	return allowed;
}

bool
policy_mgr_allow_concurrency_csa(struct wlan_objmgr_psoc *psoc,
				 enum policy_mgr_con_mode mode,
				 uint32_t ch_freq, enum hw_mode_bandwidth bw,
				 uint32_t vdev_id, bool forced,
				 enum sap_csa_reason_code reason)
{
	bool allow = false;
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct policy_mgr_conc_connection_info
			info_sap[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct policy_mgr_conc_connection_info
			info_go[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t num_cxn_del = 0;
	uint8_t num_cxn_del_sap = 0;
	uint8_t num_cxn_del_go = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t old_ch_freq, conc_ext_flags;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return allow;
	}
	policy_mgr_debug("check concurrency_csa vdev:%d ch %d bw %d, forced %d, reason %d",
			 vdev_id, ch_freq, bw, forced, reason);

	status = policy_mgr_get_chan_by_session_id(psoc, vdev_id,
						   &old_ch_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Failed to get channel for vdev:%d",
			       vdev_id);
		return allow;
	}
	qdf_mem_zero(info, sizeof(info));

	/*
	 * Store the connection's parameter and temporarily delete it
	 * from the concurrency table. This way the allow concurrency
	 * check can be used as though a new connection is coming up,
	 * after check, restore the connection to concurrency table.
	 *
	 * In SAP+SAP SCC case, when LTE unsafe event processing,
	 * we should remove the all SAP conn entry on the same ch before
	 * do the concurrency check. Otherwise the left SAP on old channel
	 * will cause the concurrency check failure because of dual beacon
	 * MCC not supported. for the CSA request reason code,
	 * PM_CSA_REASON_UNSAFE_CHANNEL, we remove all the SAP
	 * entry on old channel before do concurrency check.
	 *
	 * The assumption is both SAP should move to the new channel later for
	 * the reason code.
	 */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);

	if (forced && (reason == CSA_REASON_UNSAFE_CHANNEL ||
		       reason == CSA_REASON_DCS))
		policy_mgr_store_and_del_conn_info_by_chan_and_mode(
			psoc, old_ch_freq, mode, info, &num_cxn_del);
	else
		policy_mgr_store_and_del_conn_info_by_vdev_id(
			psoc, vdev_id, info, &num_cxn_del);

	if (policy_mgr_is_3vifs_mcc_to_scc_enabled(psoc)) {
		/* Needs to del all sap/go,
		 * because sap+go+sta SCC is supported.
		 */
		if (reason == CSA_REASON_CONCURRENT_STA_CHANGED_CHANNEL) {
			policy_mgr_store_and_del_conn_info_by_chan_and_mode(
				psoc, old_ch_freq, PM_SAP_MODE, info_sap,
				&num_cxn_del_sap);
			policy_mgr_store_and_del_conn_info_by_chan_and_mode(
				psoc, old_ch_freq, PM_P2P_GO_MODE, info_go,
				&num_cxn_del_go);
		}
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	conc_ext_flags = policy_mgr_get_conc_ext_flags(vdev, false);
	allow = policy_mgr_allow_concurrency(psoc, mode, ch_freq,
					     bw, conc_ext_flags, vdev_id);
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	/* Restore the connection entry */
	if (num_cxn_del > 0)
		policy_mgr_restore_deleted_conn_info(psoc, info, num_cxn_del);
	if (policy_mgr_is_3vifs_mcc_to_scc_enabled(psoc)) {
		if (num_cxn_del_sap > 0)
			policy_mgr_restore_deleted_conn_info(psoc, info_sap,
							     num_cxn_del_sap);
		if (num_cxn_del_go > 0)
			policy_mgr_restore_deleted_conn_info(psoc, info_go,
							     num_cxn_del_go);
	}

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (!allow)
		policy_mgr_err("CSA concurrency check failed");

	return allow;
}

/**
 * policy_mgr_get_concurrency_mode() - return concurrency mode
 * @psoc: PSOC object information
 *
 * This routine is used to retrieve concurrency mode
 *
 * Return: uint32_t value of concurrency mask
 */
uint32_t policy_mgr_get_concurrency_mode(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STA_MASK;
	}

	policy_mgr_debug("concurrency_mode: 0x%x",
			 pm_ctx->concurrency_mode);

	return pm_ctx->concurrency_mode;
}

QDF_STATUS policy_mgr_set_user_cfg(struct wlan_objmgr_psoc *psoc,
				struct policy_mgr_user_cfg *user_cfg)
{

	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}
	if (!user_cfg) {
		policy_mgr_err("Invalid User Config");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->user_cfg = *user_cfg;
	policy_mgr_debug("dbs_selection_plcy 0x%x",
			 pm_ctx->cfg.dbs_selection_plcy);
	policy_mgr_debug("vdev_priority_list 0x%x",
			 pm_ctx->cfg.vdev_priority_list);
	pm_ctx->cur_conc_system_pref = pm_ctx->cfg.sys_pref;

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_will_freq_lead_to_mcc(struct wlan_objmgr_psoc *psoc,
				      qdf_freq_t freq)
{
	bool is_mcc = false;
	uint32_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return is_mcc;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].in_use &&
		    policy_mgr_2_freq_always_on_same_mac(psoc, freq,
		     pm_conc_connection_list[conn_index].freq)) {
			is_mcc = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return is_mcc;
}

/**
 * policy_mgr_is_two_connection_mcc() - Check if MCC scenario
 * when there are two connections
 * @psoc: PSOC object information
 * @info: Connection info to be considered for MCC check
 *
 * If if MCC scenario when there are two connections
 *
 * Return: true or false
 */
static bool
policy_mgr_is_two_connection_mcc(struct wlan_objmgr_psoc *psoc,
				 struct policy_mgr_conc_connection_info *info)
{
	return ((info[0].freq != info[1].freq) &&
		(policy_mgr_are_2_freq_on_same_mac(psoc, info[0].freq,
						   info[1].freq)) &&
		(info[0].freq <= WLAN_REG_MAX_24GHZ_CHAN_FREQ) &&
		(info[1].freq <= WLAN_REG_MAX_24GHZ_CHAN_FREQ)) ? true : false;
}

/**
 * policy_mgr_is_three_connection_mcc() - Check if MCC scenario
 * when there are three connections
 *
 * @info: Connection info to be considered for MCC check
 *
 * If if MCC scenario when there are three connections
 *
 * Return: true or false
 */
static bool
policy_mgr_is_three_connection_mcc(struct policy_mgr_conc_connection_info *info)
{
	return (((info[0].freq != info[1].freq) ||
		 (info[0].freq != info[2].freq) ||
		 (info[1].freq != info[2].freq)) &&
		(info[0].freq <= WLAN_REG_MAX_24GHZ_CHAN_FREQ) &&
		(info[1].freq <= WLAN_REG_MAX_24GHZ_CHAN_FREQ) &&
		(info[2].freq <= WLAN_REG_MAX_24GHZ_CHAN_FREQ)) ? true : false;
}

uint32_t policy_mgr_get_conc_vdev_on_same_mac(struct wlan_objmgr_psoc *psoc,
					      uint32_t vdev_id, uint8_t mac_id)
{
	uint32_t id = WLAN_INVALID_VDEV_ID;
	uint32_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return id;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if ((pm_conc_connection_list[conn_index].in_use) &&
		    (pm_conc_connection_list[conn_index].vdev_id != vdev_id) &&
		    (pm_conc_connection_list[conn_index].mac == mac_id)) {
			id = pm_conc_connection_list[conn_index].vdev_id;
			break;
		}
	}

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return id;
}

bool policy_mgr_is_mcc_in_24G(struct wlan_objmgr_psoc *psoc)
{
	uint32_t num_connections = 0;
	bool is_24G_mcc = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info info[
			MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	num_connections = policy_mgr_get_sta_p2p_conn_info(pm_ctx, info);

	switch (num_connections) {
	case 1:
		break;
	case 2:
		if (policy_mgr_is_two_connection_mcc(psoc, info))
			is_24G_mcc = true;
		break;
	case 3:
		if (policy_mgr_is_three_connection_mcc(info))
			is_24G_mcc = true;
		break;
	default:
		policy_mgr_err("unexpected num_connections value %d",
			num_connections);
		break;
	}

	return is_24G_mcc;
}

bool policy_mgr_check_for_session_conc(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id, uint32_t ch_freq)
{
	enum policy_mgr_con_mode mode;
	bool ret;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev;
	uint32_t conc_ext_flags;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (pm_ctx->hdd_cbacks.get_mode_for_non_connected_vdev) {
		mode = pm_ctx->hdd_cbacks.get_mode_for_non_connected_vdev(
			psoc, vdev_id);
		if (PM_MAX_NUM_OF_MODE == mode) {
			policy_mgr_err("Invalid mode");
			return false;
		}
	} else
		return false;

	if (ch_freq == 0) {
		policy_mgr_err("Invalid channel number 0");
		return false;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);

	/* Take care of 160MHz and 80+80Mhz later */
	conc_ext_flags = policy_mgr_get_conc_ext_flags(vdev, false);
	ret = policy_mgr_allow_concurrency(psoc, mode, ch_freq, HW_MODE_20_MHZ,
					   conc_ext_flags, vdev_id);
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	if (false == ret) {
		policy_mgr_err("Connection failed due to conc check fail");
		return 0;
	}

	return true;
}

/**
 * policy_mgr_change_mcc_go_beacon_interval() - Change MCC beacon interval
 * @psoc: PSOC object information
 * @vdev_id: vdev id
 * @dev_mode: device mode
 *
 * Updates the beacon parameters of the GO in MCC scenario
 *
 * Return: Success or Failure depending on the overall function behavior
 */
QDF_STATUS policy_mgr_change_mcc_go_beacon_interval(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id, enum QDF_OPMODE dev_mode)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_debug("UPDATE Beacon Params");

	if (QDF_SAP_MODE == dev_mode) {
		if (pm_ctx->sme_cbacks.sme_change_mcc_beacon_interval
		    ) {
			status = pm_ctx->sme_cbacks.
				sme_change_mcc_beacon_interval(vdev_id);
			if (status == QDF_STATUS_E_FAILURE) {
				policy_mgr_err("Failed to update Beacon Params");
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			policy_mgr_err("sme_change_mcc_beacon_interval callback is NULL");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

struct policy_mgr_conc_connection_info *policy_mgr_get_conn_info(uint32_t *len)
{
	struct policy_mgr_conc_connection_info *conn_ptr =
		&pm_conc_connection_list[0];
	*len = MAX_NUMBER_OF_CONC_CONNECTIONS;

	return conn_ptr;
}

enum policy_mgr_con_mode
policy_mgr_qdf_opmode_to_pm_con_mode(struct wlan_objmgr_psoc *psoc,
				     enum QDF_OPMODE device_mode,
				     uint8_t vdev_id)
{
	enum policy_mgr_con_mode mode = PM_MAX_NUM_OF_MODE;

	switch (device_mode) {
	case QDF_STA_MODE:
		mode = PM_STA_MODE;
		break;
	case QDF_P2P_CLIENT_MODE:
		mode = PM_P2P_CLIENT_MODE;
		break;
	case QDF_P2P_GO_MODE:
		mode = PM_P2P_GO_MODE;
		break;
	case QDF_SAP_MODE:
#ifdef WLAN_FEATURE_LL_LT_SAP
		if (policy_mgr_is_vdev_ll_lt_sap(psoc, vdev_id))
			mode = PM_LL_LT_SAP_MODE;
		else
#endif
			mode = PM_SAP_MODE;
		break;
	case QDF_NAN_DISC_MODE:
		mode = PM_NAN_DISC_MODE;
		break;
	case QDF_NDI_MODE:
		mode = PM_NDI_MODE;
		break;
	default:
		policy_mgr_debug("Unsupported mode (%d)",
				 device_mode);
	}

	return mode;
}

enum QDF_OPMODE policy_mgr_get_qdf_mode_from_pm(
			enum policy_mgr_con_mode device_mode)
{
	enum QDF_OPMODE mode = QDF_MAX_NO_OF_MODE;

	switch (device_mode) {
	case PM_STA_MODE:
		mode = QDF_STA_MODE;
		break;
	case PM_SAP_MODE:
	case PM_LL_LT_SAP_MODE:
		mode = QDF_SAP_MODE;
		break;
	case PM_P2P_CLIENT_MODE:
		mode = QDF_P2P_CLIENT_MODE;
		break;
	case PM_P2P_GO_MODE:
		mode = QDF_P2P_GO_MODE;
		break;
	case PM_NAN_DISC_MODE:
		mode = QDF_NAN_DISC_MODE;
		break;
	case PM_NDI_MODE:
		mode = QDF_NDI_MODE;
		break;
	default:
		policy_mgr_debug("Unsupported policy mgr mode (%d)",
				 device_mode);
	}
	return mode;
}

QDF_STATUS policy_mgr_mode_specific_num_open_sessions(
		struct wlan_objmgr_psoc *psoc, enum QDF_OPMODE mode,
		uint8_t *num_sessions)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	*num_sessions = pm_ctx->no_of_open_sessions[mode];
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_mode_specific_num_active_sessions(
		struct wlan_objmgr_psoc *psoc, enum QDF_OPMODE mode,
		uint8_t *num_sessions)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	*num_sessions = pm_ctx->no_of_active_sessions[mode];
	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_concurrent_open_sessions_running() - Checks for
 * concurrent open session
 * @psoc: PSOC object information
 *
 * Checks if more than one open session is running for all the allowed modes
 * in the driver
 *
 * Return: True if more than one open session exists, False otherwise
 */
bool policy_mgr_concurrent_open_sessions_running(
	struct wlan_objmgr_psoc *psoc)
{
	uint8_t i = 0;
	uint8_t j = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return false;
	}

	for (i = 0; i < QDF_MAX_NO_OF_MODE; i++)
		j += pm_ctx->no_of_open_sessions[i];

	return j > 1;
}

/**
 * policy_mgr_concurrent_beaconing_sessions_running() - Checks
 * for concurrent beaconing entities
 * @psoc: PSOC object information
 *
 * Checks if multiple beaconing sessions are running i.e., if SAP or GO
 * are beaconing together
 *
 * Return: True if multiple entities are beaconing together, False otherwise
 */
bool policy_mgr_concurrent_beaconing_sessions_running(
	struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return false;
	}

	return (pm_ctx->no_of_open_sessions[QDF_SAP_MODE] +
		pm_ctx->no_of_open_sessions[QDF_P2P_GO_MODE]
		> 1) ? true : false;
}


void policy_mgr_clear_concurrent_session_count(struct wlan_objmgr_psoc *psoc)
{
	uint8_t i = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (pm_ctx) {
		for (i = 0; i < QDF_MAX_NO_OF_MODE; i++)
			pm_ctx->no_of_active_sessions[i] = 0;
	}
}

bool policy_mgr_is_multiple_active_sta_sessions(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, NULL) > 1;
}

bool policy_mgr_is_sta_present_on_dfs_channel(struct wlan_objmgr_psoc *psoc,
					      uint8_t *vdev_id,
					      qdf_freq_t *ch_freq,
					      enum hw_mode_bandwidth *ch_width)
{
	struct policy_mgr_conc_connection_info *conn_info;
	bool status = false;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    (conn_info->mode == PM_STA_MODE ||
		     conn_info->mode == PM_P2P_CLIENT_MODE) &&
		    (wlan_reg_is_dfs_for_freq(pm_ctx->pdev, conn_info->freq) ||
		     (wlan_reg_is_5ghz_ch_freq(conn_info->freq) &&
		      conn_info->bw == HW_MODE_160_MHZ))) {
			*vdev_id = conn_info->vdev_id;
			*ch_freq = pm_conc_connection_list[conn_index].freq;
			*ch_width = conn_info->bw;
			status = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

bool policy_mgr_is_sta_present_on_freq(struct wlan_objmgr_psoc *psoc,
				       uint8_t *vdev_id,
				       qdf_freq_t ch_freq,
				       enum hw_mode_bandwidth *ch_width)
{
	struct policy_mgr_conc_connection_info *conn_info;
	bool status = false;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    (conn_info->mode == PM_STA_MODE ||
		     conn_info->mode == PM_P2P_CLIENT_MODE) &&
		    ch_freq == conn_info->freq) {
			*vdev_id = conn_info->vdev_id;
			*ch_width = conn_info->bw;
			status = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

bool policy_mgr_is_sta_gc_active_on_mac(struct wlan_objmgr_psoc *psoc,
					uint8_t mac_id)
{
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t index, count;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	count = policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, list);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (index = 0; index < count; index++) {
		if (mac_id == pm_conc_connection_list[list[index]].mac) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	count = policy_mgr_mode_specific_connection_count(
		psoc, PM_P2P_CLIENT_MODE, list);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (index = 0; index < count; index++) {
		if (mac_id == pm_conc_connection_list[list[index]].mac) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return false;
}

/**
 * policy_mgr_is_sta_active_connection_exists() - Check if a STA
 * connection is active
 * @psoc: PSOC object information
 *
 * Checks if there is atleast one active STA connection in the driver
 *
 * Return: True if an active STA session is present, False otherwise
 */
bool policy_mgr_is_sta_active_connection_exists(
	struct wlan_objmgr_psoc *psoc)
{
	return (!policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, NULL)) ? false : true;
}

bool policy_mgr_is_any_nondfs_chnl_present(struct wlan_objmgr_psoc *psoc,
					   uint32_t *ch_freq)
{
	bool status = false;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		if (pm_conc_connection_list[conn_index].in_use &&
		    !wlan_reg_is_dfs_for_freq(pm_ctx->pdev,
		    pm_conc_connection_list[conn_index].freq)) {
			*ch_freq = pm_conc_connection_list[conn_index].freq;
			status = true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

uint32_t policy_mgr_get_dfs_beaconing_session_id(
		struct wlan_objmgr_psoc *psoc)
{
	uint32_t session_id = WLAN_UMAC_VDEV_ID_MAX;
	struct policy_mgr_conc_connection_info *conn_info;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return session_id;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(conn_info->freq) &&
		    (conn_info->ch_flagext & (IEEE80211_CHAN_DFS |
					      IEEE80211_CHAN_DFS_CFREQ2)) &&
		    (conn_info->mode == PM_SAP_MODE ||
		     conn_info->mode == PM_P2P_GO_MODE)) {
			session_id =
				pm_conc_connection_list[conn_index].vdev_id;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return session_id;
}

bool policy_mgr_is_any_dfs_beaconing_session_present(
		struct wlan_objmgr_psoc *psoc, qdf_freq_t *ch_freq,
		enum hw_mode_bandwidth *ch_width)
{
	struct policy_mgr_conc_connection_info *conn_info;
	bool status = false;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    (conn_info->mode == PM_SAP_MODE ||
		     conn_info->mode == PM_P2P_GO_MODE) &&
		     (wlan_reg_is_dfs_for_freq(pm_ctx->pdev, conn_info->freq) ||
		      (wlan_reg_is_5ghz_ch_freq(conn_info->freq) &&
		      conn_info->bw == HW_MODE_160_MHZ))) {
			*ch_freq = pm_conc_connection_list[conn_index].freq;
			*ch_width = conn_info->bw;
			status = true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

bool policy_mgr_scan_trim_5g_chnls_for_dfs_ap(struct wlan_objmgr_psoc *psoc,
					      qdf_freq_t *freq)
{
	qdf_freq_t dfs_ch_frq = 0;
	qdf_freq_t dfs_sta_frq = 0;
	uint8_t vdev_id;
	enum hw_mode_bandwidth ch_width;
	enum hw_mode_bandwidth ch_sta_width;
	QDF_STATUS status;
	uint8_t  sta_sap_scc_on_dfs_chnl;

	policy_mgr_is_any_dfs_beaconing_session_present(psoc, &dfs_ch_frq,
							&ch_width);
	if (!dfs_ch_frq)
		return false;

	*freq = dfs_ch_frq;

	status = policy_mgr_get_sta_sap_scc_on_dfs_chnl(psoc,
						&sta_sap_scc_on_dfs_chnl);
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	if (policy_mgr_is_sta_present_on_dfs_channel(psoc, &vdev_id,
						     &dfs_sta_frq,
						     &ch_sta_width) &&
	    !policy_mgr_is_hw_dbs_capable(psoc) &&
	    sta_sap_scc_on_dfs_chnl != PM_STA_SAP_ON_DFS_DEFAULT) {
		policymgr_nofl_err("DFS STA present vdev_id %d ch_feq %d ch_width %d",
				   vdev_id, dfs_sta_frq, ch_sta_width);
		return false;
	}

	/*
	 * 1) if agile & DFS scans are supported
	 * 2) if hardware is DBS capable
	 * 3) if current hw mode is non-dbs
	 * if all above 3 conditions are true then don't skip any
	 * channel from scan list
	 */
	if (policy_mgr_is_hw_dbs_capable(psoc) &&
	    !policy_mgr_is_current_hwmode_dbs(psoc) &&
	    policy_mgr_get_dbs_plus_agile_scan_config(psoc) &&
	    policy_mgr_get_single_mac_scan_with_dfs_config(psoc))
		return false;

	policy_mgr_debug("scan skip 5g chan due to dfs ap(ch %d / ch_width %d) present",
			 dfs_ch_frq, ch_width);

	return true;
}

static void
policy_mgr_fill_trim_chan(struct wlan_objmgr_pdev *pdev,
			  void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = object;
	struct trim_chan_info *trim_info = arg;
	uint16_t sap_peer_count = 0;
	qdf_freq_t chan_freq;

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_SAP_MODE)
		return;

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS)
		return;

	sap_peer_count = wlan_vdev_get_peer_count(vdev);
	policy_mgr_debug("vdev %d - peer count %d",
			 wlan_vdev_get_id(vdev), sap_peer_count);
	if (sap_peer_count <= 1)
		return;

	chan_freq = wlan_get_operation_chan_freq(vdev);
	if (!chan_freq)
		return;

	if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq)) {
		trim_info->trim |= TRIM_CHANNEL_LIST_5G;
	} else if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq)) {
		if (trim_info->sap_count != 1)
			return;

		if ((trim_info->band_capability & BIT(REG_BAND_5G)) ==
		     BIT(REG_BAND_5G))
			return;

		trim_info->trim |= TRIM_CHANNEL_LIST_24G;
	}
}

uint16_t
policy_mgr_scan_trim_chnls_for_connected_ap(struct wlan_objmgr_pdev *pdev)
{
	struct trim_chan_info trim_info;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return TRIM_CHANNEL_LIST_NONE;

	status = wlan_mlme_get_band_capability(psoc,
					       &trim_info.band_capability);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Could not get band capability");
		return TRIM_CHANNEL_LIST_NONE;
	}

	trim_info.sap_count = policy_mgr_mode_specific_connection_count(psoc,
							PM_SAP_MODE, NULL);
	if (!trim_info.sap_count)
		return TRIM_CHANNEL_LIST_NONE;

	trim_info.trim = TRIM_CHANNEL_LIST_NONE;

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  policy_mgr_fill_trim_chan, &trim_info,
					  0, WLAN_POLICY_MGR_ID);
	policy_mgr_debug("band_capability %d, sap_count %d, trim %d",
			 trim_info.band_capability, trim_info.sap_count,
			 trim_info.trim);

	return trim_info.trim;
}

QDF_STATUS policy_mgr_get_nss_for_vdev(struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				uint8_t *nss_2g, uint8_t *nss_5g)
{
	enum QDF_OPMODE dev_mode;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	dev_mode = policy_mgr_get_qdf_mode_from_pm(mode);
	if (dev_mode == QDF_MAX_NO_OF_MODE)
		return  QDF_STATUS_E_FAILURE;
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (pm_ctx->sme_cbacks.sme_get_nss_for_vdev) {
		pm_ctx->sme_cbacks.sme_get_nss_for_vdev(
			dev_mode, nss_2g, nss_5g);

	} else {
		policy_mgr_err("sme_get_nss_for_vdev callback is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_dump_connection_status_info(struct wlan_objmgr_psoc *psoc)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (!pm_conc_connection_list[i].in_use)
			continue;
		policy_mgr_debug("%d: use:%d vdev:%d mode:%d mac:%d freq:%d orig chainmask:%d orig nss:%d bw:%d, ch_flags %0X",
				 i, pm_conc_connection_list[i].in_use,
				 pm_conc_connection_list[i].vdev_id,
				 pm_conc_connection_list[i].mode,
				 pm_conc_connection_list[i].mac,
				 pm_conc_connection_list[i].freq,
				 pm_conc_connection_list[i].chain_mask,
				 pm_conc_connection_list[i].original_nss,
				 pm_conc_connection_list[i].bw,
				 pm_conc_connection_list[i].ch_flagext);
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	policy_mgr_dump_curr_freq_range(pm_ctx);
	policy_mgr_validate_conn_info(psoc);
}

bool policy_mgr_is_any_mode_active_on_band_along_with_session(
						struct wlan_objmgr_psoc *psoc,
						uint8_t session_id,
						enum policy_mgr_band band)
{
	uint32_t i;
	bool status = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		status = false;
		goto send_status;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		switch (band) {
		case POLICY_MGR_BAND_24:
			if ((pm_conc_connection_list[i].vdev_id != session_id)
			&& (pm_conc_connection_list[i].in_use) &&
			(WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[i].freq))) {
				status = true;
				goto release_mutex_and_send_status;
			}
			break;
		case POLICY_MGR_BAND_5:
			if ((pm_conc_connection_list[i].vdev_id != session_id)
			&& (pm_conc_connection_list[i].in_use) &&
			(WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[i].freq))) {
				status = true;
				goto release_mutex_and_send_status;
			}
			break;
		default:
			policy_mgr_err("Invalidband option:%d", band);
			status = false;
			goto release_mutex_and_send_status;
		}
	}
release_mutex_and_send_status:
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
send_status:
	return status;
}

enum phy_ch_width
policy_mgr_get_bw_by_session_id(struct wlan_objmgr_psoc *psoc,
				uint8_t session_id)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum hw_mode_bandwidth bw = HW_MODE_BW_NONE;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return CH_WIDTH_INVALID;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].vdev_id == session_id &&
		    pm_conc_connection_list[i].in_use) {
			bw = pm_conc_connection_list[i].bw;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	return policy_mgr_get_ch_width(bw);
}

QDF_STATUS policy_mgr_get_chan_by_session_id(struct wlan_objmgr_psoc *psoc,
					     uint8_t session_id,
					     uint32_t *ch_freq)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if ((pm_conc_connection_list[i].vdev_id == session_id) &&
		    (pm_conc_connection_list[i].in_use)) {
			*ch_freq = pm_conc_connection_list[i].freq;
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS policy_mgr_get_mac_id_by_session_id(struct wlan_objmgr_psoc *psoc,
					       uint8_t session_id,
					       uint8_t *mac_id)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if ((pm_conc_connection_list[i].vdev_id == session_id) &&
		    (pm_conc_connection_list[i].in_use)) {
			*mac_id = pm_conc_connection_list[i].mac;
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return QDF_STATUS_E_FAILURE;
}

uint32_t policy_mgr_get_sap_go_count_on_mac(struct wlan_objmgr_psoc *psoc,
					    uint32_t *list, uint8_t mac_id)
{
	uint32_t conn_index;
	uint32_t count = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].mac == mac_id &&
		    pm_conc_connection_list[conn_index].in_use &&
		    policy_mgr_is_beaconing_mode(
				pm_conc_connection_list[conn_index].mode)) {
			if (list)
				list[count] =
				    pm_conc_connection_list[conn_index].vdev_id;
			count++;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

uint32_t policy_mgr_get_mcc_operating_channel(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id)
{
	uint8_t mcc_vdev_id;
	QDF_STATUS status;
	uint32_t ch_freq;

	if (!policy_mgr_is_mcc_with_this_vdev_id(psoc, vdev_id, &mcc_vdev_id)) {
		policy_mgr_debug("No concurrent MCC vdev for id:%d", vdev_id);
		return INVALID_CHANNEL_ID;
	}

	status = policy_mgr_get_chan_by_session_id(psoc, mcc_vdev_id, &ch_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Failed to get channel for MCC vdev:%d",
			       mcc_vdev_id);
		return INVALID_CHANNEL_ID;
	}

	return ch_freq;
}

bool policy_mgr_is_dnsc_set(struct wlan_objmgr_vdev *vdev)
{
	bool roffchan;

	if (!vdev) {
		policy_mgr_err("Invalid parameter");
		return false;
	}

	roffchan = wlan_vdev_mlme_cap_get(vdev, WLAN_VDEV_C_RESTRICT_OFFCHAN);

	if (roffchan)
		policy_mgr_debug("Restrict offchannel is set");

	return roffchan;
}

QDF_STATUS policy_mgr_is_chan_ok_for_dnbs(struct wlan_objmgr_psoc *psoc,
					  uint32_t ch_freq, bool *ok)
{
	uint32_t cc_count = 0, i;
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct wlan_objmgr_vdev *vdev;

	if (!ok) {
		policy_mgr_err("Invalid parameter");
		return QDF_STATUS_E_INVAL;
	}

	cc_count = policy_mgr_get_sap_mode_info(psoc,
						&op_ch_freq_list[cc_count],
						&vdev_id[cc_count]);

	if (cc_count < MAX_NUMBER_OF_CONC_CONNECTIONS)
		cc_count = cc_count +
			   policy_mgr_get_mode_specific_conn_info(
					psoc, &op_ch_freq_list[cc_count],
					&vdev_id[cc_count], PM_P2P_GO_MODE);

	if (!cc_count) {
		*ok = true;
		return QDF_STATUS_SUCCESS;
	}

	if (!ch_freq) {
		policy_mgr_err("channel is 0, cc count %d", cc_count);
		return QDF_STATUS_E_INVAL;
	}

	if (cc_count <= MAX_NUMBER_OF_CONC_CONNECTIONS) {
		for (i = 0; i < cc_count; i++) {
			vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
					psoc, vdev_id[i], WLAN_POLICY_MGR_ID);
			if (!vdev) {
				policy_mgr_err("vdev for vdev_id:%d is NULL",
					       vdev_id[i]);
				return QDF_STATUS_E_INVAL;
			}

			/**
			 * If channel passed is same as AP/GO operating
			 * channel, return true.
			 *
			 * If channel is different from operating channel but
			 * in same band, return false.
			 *
			 * If operating channel in different band
			 * (DBS capable), return true.
			 *
			 * If operating channel in different band
			 * (not DBS capable), return false.
			 */
			if (policy_mgr_is_dnsc_set(vdev)) {
				if (op_ch_freq_list[i] == ch_freq) {
					*ok = true;
					wlan_objmgr_vdev_release_ref(
							vdev,
							WLAN_POLICY_MGR_ID);
					break;
				} else if (policy_mgr_2_freq_always_on_same_mac(
					   psoc,
					   op_ch_freq_list[i], ch_freq)) {
					*ok = false;
					wlan_objmgr_vdev_release_ref(
					vdev,
					WLAN_POLICY_MGR_ID);
					break;
				} else if (policy_mgr_is_hw_dbs_capable(psoc)) {
					*ok = true;
					wlan_objmgr_vdev_release_ref(
							vdev,
							WLAN_POLICY_MGR_ID);
					break;
				} else {
					*ok = false;
					wlan_objmgr_vdev_release_ref(
							vdev,
							WLAN_POLICY_MGR_ID);
					break;
				}
			} else {
				*ok = true;
			}
			wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
		}
	}

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_get_hw_dbs_max_bw(struct wlan_objmgr_psoc *psoc,
				  struct dbs_bw *bw_dbs)
{
	uint32_t dbs, sbs, i, param;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		param = pm_ctx->hw_mode.hw_mode_list[i];
		dbs = POLICY_MGR_HW_MODE_DBS_MODE_GET(param);
		sbs = POLICY_MGR_HW_MODE_SBS_MODE_GET(param);

		if (!dbs && !sbs)
			bw_dbs->mac0_bw =
				POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_GET(param);

		if (dbs) {
			bw_dbs->mac0_bw =
				POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_GET(param);
			bw_dbs->mac1_bw =
				POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_GET(param);
		} else {
			continue;
		}
	}
}

uint32_t policy_mgr_get_hw_dbs_nss(struct wlan_objmgr_psoc *psoc,
				   struct dbs_nss *nss_dbs)
{
	int i, param;
	uint32_t dbs, sbs, tx_chain0, rx_chain0, tx_chain1, rx_chain1;
	uint32_t min_mac0_rf_chains, min_mac1_rf_chains;
	uint32_t max_rf_chains, final_max_rf_chains = HW_MODE_SS_0x0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return final_max_rf_chains;
	}

	nss_dbs->single_mac0_band_cap = 0;
	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		param = pm_ctx->hw_mode.hw_mode_list[i];
		dbs = POLICY_MGR_HW_MODE_DBS_MODE_GET(param);
		sbs = POLICY_MGR_HW_MODE_SBS_MODE_GET(param);

		if (!dbs && !sbs && !nss_dbs->single_mac0_band_cap)
			nss_dbs->single_mac0_band_cap =
				POLICY_MGR_HW_MODE_MAC0_BAND_GET(param);

		if (dbs) {
			tx_chain0
				= POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_GET(param);
			rx_chain0
				= POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_GET(param);

			tx_chain1
				= POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_GET(param);
			rx_chain1
				= POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_GET(param);

			min_mac0_rf_chains = QDF_MIN(tx_chain0, rx_chain0);
			min_mac1_rf_chains = QDF_MIN(tx_chain1, rx_chain1);

			max_rf_chains
			= QDF_MAX(min_mac0_rf_chains, min_mac1_rf_chains);

			if (final_max_rf_chains < max_rf_chains) {
				final_max_rf_chains
					= (max_rf_chains == 2)
					? HW_MODE_SS_2x2 : HW_MODE_SS_1x1;

				nss_dbs->mac0_ss
					= (min_mac0_rf_chains == 2)
					? HW_MODE_SS_2x2 : HW_MODE_SS_1x1;

				nss_dbs->mac1_ss
					= (min_mac1_rf_chains == 2)
					? HW_MODE_SS_2x2 : HW_MODE_SS_1x1;
			}
		} else {
			continue;
		}
	}

	return final_max_rf_chains;
}

bool policy_mgr_is_scan_simultaneous_capable(struct wlan_objmgr_psoc *psoc)
{
	uint8_t dual_mac_feature = DISABLE_DBS_CXN_AND_SCAN;

	policy_mgr_get_dual_mac_feature(psoc, &dual_mac_feature);
	if ((dual_mac_feature == DISABLE_DBS_CXN_AND_SCAN) ||
	    (dual_mac_feature == ENABLE_DBS_CXN_AND_DISABLE_DBS_SCAN) ||
	    (dual_mac_feature ==
	     ENABLE_DBS_CXN_AND_DISABLE_SIMULTANEOUS_SCAN) ||
	     !policy_mgr_is_hw_dbs_capable(psoc))
		return false;

	return true;
}

void policy_mgr_set_cur_conc_system_pref(struct wlan_objmgr_psoc *psoc,
		uint8_t conc_system_pref)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	policy_mgr_debug("conc_system_pref %hu", conc_system_pref);
	pm_ctx->cur_conc_system_pref = conc_system_pref;
}

uint8_t policy_mgr_get_cur_conc_system_pref(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return PM_THROUGHPUT;
	}

	policy_mgr_debug("conc_system_pref %hu", pm_ctx->cur_conc_system_pref);
	return pm_ctx->cur_conc_system_pref;
}

QDF_STATUS policy_mgr_get_updated_scan_and_fw_mode_config(
		struct wlan_objmgr_psoc *psoc, uint32_t *scan_config,
		uint32_t *fw_mode_config, uint32_t dual_mac_disable_ini,
		uint32_t channel_select_logic_conc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	*scan_config = pm_ctx->dual_mac_cfg.cur_scan_config;
	*fw_mode_config = pm_ctx->dual_mac_cfg.cur_fw_mode_config;
	switch (dual_mac_disable_ini) {
	case DISABLE_DBS_CXN_AND_ENABLE_DBS_SCAN_WITH_ASYNC_SCAN_OFF:
		policy_mgr_debug("dual_mac_disable_ini:%d async/dbs off",
			dual_mac_disable_ini);
		WMI_DBS_CONC_SCAN_CFG_ASYNC_DBS_SCAN_SET(*scan_config, 0);
		WMI_DBS_FW_MODE_CFG_DBS_FOR_CXN_SET(*fw_mode_config, 0);
		break;
	case DISABLE_DBS_CXN_AND_ENABLE_DBS_SCAN:
		policy_mgr_debug("dual_mac_disable_ini:%d dbs_cxn off",
			dual_mac_disable_ini);
		WMI_DBS_FW_MODE_CFG_DBS_FOR_CXN_SET(*fw_mode_config, 0);
		break;
	case ENABLE_DBS_CXN_AND_ENABLE_SCAN_WITH_ASYNC_SCAN_OFF:
		policy_mgr_debug("dual_mac_disable_ini:%d async off",
			dual_mac_disable_ini);
		WMI_DBS_CONC_SCAN_CFG_ASYNC_DBS_SCAN_SET(*scan_config, 0);
		break;
	case ENABLE_DBS_CXN_AND_DISABLE_DBS_SCAN:
		policy_mgr_debug("dual_mac_disable_ini:%d ",
				 dual_mac_disable_ini);
		WMI_DBS_CONC_SCAN_CFG_DBS_SCAN_SET(*scan_config, 0);
		break;
	default:
		break;
	}

	WMI_DBS_FW_MODE_CFG_DBS_FOR_STA_PLUS_STA_SET(*fw_mode_config,
		PM_CHANNEL_SELECT_LOGIC_STA_STA_GET(channel_select_logic_conc));
	WMI_DBS_FW_MODE_CFG_DBS_FOR_STA_PLUS_P2P_SET(*fw_mode_config,
		PM_CHANNEL_SELECT_LOGIC_STA_P2P_GET(channel_select_logic_conc));

	policy_mgr_debug("*scan_config:%x ", *scan_config);
	policy_mgr_debug("*fw_mode_config:%x ", *fw_mode_config);

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_is_force_scc(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}

	return ((pm_ctx->cfg.mcc_to_scc_switch ==
		QDF_MCC_TO_SCC_SWITCH_FORCE_WITHOUT_DISCONNECTION) ||
		(pm_ctx->cfg.mcc_to_scc_switch ==
		QDF_MCC_TO_SCC_SWITCH_WITH_FAVORITE_CHANNEL) ||
		(pm_ctx->cfg.mcc_to_scc_switch ==
		QDF_MCC_TO_SCC_SWITCH_FORCE_PREFERRED_WITHOUT_DISCONNECTION) ||
		(pm_ctx->cfg.mcc_to_scc_switch ==
		QDF_MCC_TO_SCC_WITH_PREFERRED_BAND));
}

bool policy_mgr_is_sap_allowed_on_dfs_freq(struct wlan_objmgr_pdev *pdev,
					   uint8_t vdev_id, qdf_freq_t ch_freq)
{
	struct wlan_objmgr_psoc *psoc;
	uint32_t sta_sap_scc_on_dfs_chan;
	uint32_t sta_cnt, gc_cnt, idx;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS] = {0};
	struct wlan_objmgr_vdev *vdev;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return false;

	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);
	sta_cnt = policy_mgr_get_mode_specific_conn_info(psoc, NULL,
							 vdev_id_list,
							 PM_STA_MODE);

	if (sta_cnt >= MAX_NUMBER_OF_CONC_CONNECTIONS)
		return false;

	gc_cnt = policy_mgr_get_mode_specific_conn_info(psoc, NULL,
							&vdev_id_list[sta_cnt],
							PM_P2P_CLIENT_MODE);

	policy_mgr_debug("sta_sap_scc_on_dfs_chan %u, sta_cnt %u, gc_cnt %u",
			 sta_sap_scc_on_dfs_chan, sta_cnt, gc_cnt);

	/* if sta_sap_scc_on_dfs_chan ini is set, DFS master capability is
	 * assumed disabled in the driver.
	 */
	if ((wlan_reg_get_channel_state_for_pwrmode(
		pdev, ch_freq, REG_CURRENT_PWR_MODE) == CHANNEL_STATE_DFS) &&
	    !sta_cnt && !gc_cnt && sta_sap_scc_on_dfs_chan &&
	    !policy_mgr_get_dfs_master_dynamic_enabled(psoc, vdev_id)) {
		policy_mgr_err("SAP not allowed on DFS channel if no dfs master capability!!");
		return false;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("Invalid vdev");
		return false;
	}
	/* Allow the current CSA to continue if it's already started. This is
	 * possible when SAP CSA started to move to STA channel but STA got
	 * disconnected.
	 */
	if (!wlan_vdev_mlme_is_init_state(vdev) &&
	    !wlan_vdev_is_up_active_state(vdev)) {
		policy_mgr_debug("SAP is not yet UP: vdev %d", vdev_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
		return true;
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	/*
	 * Check if any of the concurrent STA/ML-STA link/P2P client are in
	 * disconnecting state and disallow current SAP CSA. Concurrencies
	 * would be re-evaluated upon disconnect completion and SAP would be
	 * moved to right channel.
	 */
	for (idx = 0; idx < sta_cnt + gc_cnt; idx++) {
		if (idx >= MAX_NUMBER_OF_CONC_CONNECTIONS) {
			policy_mgr_err("idx:%u is out of bounds", idx);
			return false;
		}

		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
							    vdev_id_list[idx],
							    WLAN_POLICY_MGR_ID);
		if (!vdev) {
			policy_mgr_err("Invalid vdev");
			return false;
		}
		if (wlan_cm_is_vdev_disconnecting(vdev) ||
		    mlo_is_any_link_disconnecting(vdev)) {
			policy_mgr_err("SAP is not allowed to move to DFS channel at this time, vdev %d",
				       vdev_id_list[idx]);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
			return false;
		}
		wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	}

	return true;
}

bool policy_mgr_is_sap_override_dfs_required(struct wlan_objmgr_pdev *pdev,
					     uint32_t chan_freq,
					     enum phy_ch_width ch_width,
					     qdf_freq_t acs_start_freq,
					     qdf_freq_t acs_end_freq,
					     bool puncture_enable,
					     uint32_t *con_vdev_id,
					     uint32_t *con_ch_freq)
{
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;
	bool sap_on_dfs = false, cac_in_progress, on_non_2ghz;
	struct ch_params chan_params = {0};
	qdf_freq_t cut_off_freq;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return false;

	if (!con_vdev_id || !con_ch_freq) {
		policy_mgr_err("Invalid NULL pointer");
		return false;
	}

	*con_vdev_id = policy_mgr_get_dfs_beaconing_session_id(psoc);

	/* No DFS beaconing session exist, no override */
	if (*con_vdev_id == WLAN_UMAC_VDEV_ID_MAX)
		return false;

	status = policy_mgr_get_chan_by_session_id(psoc,
						   *con_vdev_id,
						   con_ch_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Fail to get channel by vdev id %d",
			       *con_vdev_id);
		return false;
	}

	cac_in_progress =
		wlan_util_is_vdev_in_cac_wait(pdev, WLAN_POLICY_MGR_ID);

	on_non_2ghz = chan_freq ? !WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq) :
		      !WLAN_REG_IS_24GHZ_CH_FREQ(acs_start_freq);

	/*
	 * If CAC in progress HW mode cannot change in target,
	 * override if MCC in current HW mode.
	 */
	if (cac_in_progress) {
		if (!policy_mgr_is_current_hwmode_dbs(psoc) &&
		    !policy_mgr_is_current_hwmode_sbs(psoc)) {
			policy_mgr_debug("CAC in progress, current SMM mode");
			return true;
		}

		if (policy_mgr_is_current_hwmode_dbs(psoc) && on_non_2ghz) {
			policy_mgr_debug("CAC in progress, current DBS mode");
			return true;
		}

		/* If doing CAC in SBS mode, same check as below */
	}

	/* SMM capable */
	if (!policy_mgr_is_hw_dbs_capable(psoc) &&
	    !policy_mgr_is_hw_sbs_capable(psoc))
		return true;

	/* DBS capable */
	if (!policy_mgr_is_hw_sbs_capable(psoc)) {
		if (on_non_2ghz) {
			policy_mgr_debug("DBS capable, new SAP on non 2ghz");
			return true;
		}
		return false;
	}

	/* SBS capable */
	cut_off_freq = policy_mgr_get_sbs_cut_off_freq(psoc);
	/*
	 * If call when ACS with SBS capable, don't override
	 * if may select channel on another mac
	 */
	if (!chan_freq && acs_start_freq && acs_end_freq) {
		if (on_non_2ghz && ((*con_ch_freq > cut_off_freq &&
		    acs_start_freq > cut_off_freq) ||
		    (*con_ch_freq < cut_off_freq &&
		    acs_end_freq < cut_off_freq))) {
			policy_mgr_debug("con DFS freq %d acs start %d end %d",
					 *con_ch_freq,
					 acs_start_freq,
					 acs_end_freq);
			return true;
		}
		return false;
	}

	chan_params.ch_width = ch_width;
	if (puncture_enable)
		wlan_reg_set_create_punc_bitmap(&chan_params, true);

	if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq) &&
	    wlan_reg_get_5g_bonded_channel_state_for_pwrmode(
	    pdev, chan_freq, &chan_params, REG_CURRENT_PWR_MODE) ==
	    CHANNEL_STATE_DFS)
		sap_on_dfs = true;

	/* Allowed if new SAP on different mac and not on DFS */
	if (!policy_mgr_2_freq_always_on_same_mac(psoc,
						  chan_freq,
						  *con_ch_freq) &&
	    !sap_on_dfs)
		return false;

	return true;
}

bool
policy_mgr_is_sap_go_interface_allowed_on_indoor(struct wlan_objmgr_pdev *pdev,
						 uint8_t vdev_id,
						 qdf_freq_t ch_freq)
{
	struct wlan_objmgr_psoc *psoc;
	bool is_scc = false, indoor_support = false;
	enum QDF_OPMODE mode;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return true;

	if (!wlan_reg_is_freq_indoor(pdev, ch_freq))
		return true;

	is_scc = policy_mgr_is_sta_sap_scc(psoc, ch_freq);
	mode = wlan_get_opmode_from_vdev_id(pdev, vdev_id);
	ucfg_mlme_get_indoor_channel_support(psoc, &indoor_support);

	/*
	 * Rules for indoor operation:
	 * If gindoor_channel_support is enabled - Allow SAP/GO
	 * If gindoor_channel_support is disabled
	 *      a) Restrict 6 GHz SAP
	 *      b) Restrict standalone 5 GHz SAP
	 *
	 * If p2p_go_on_5ghz_indoor_chan is enabled - Allow GO
	 * with or without concurrency
	 *
	 * If sta_sap_scc_on_indoor_chan is enabled - Allow
	 * SAP/GO with concurrent STA in indoor SCC
	 *
	 * Restrict all other operations on indoor
	 */

	if (indoor_support)
		return true;

	if (WLAN_REG_IS_6GHZ_CHAN_FREQ(ch_freq)) {
		policy_mgr_rl_debug("SAP operation is not allowed on 6 GHz indoor channel");
		return false;
	}

	if (mode == QDF_SAP_MODE) {
		if (is_scc &&
		    policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(psoc))
			return true;
		policy_mgr_rl_debug("SAP operation is not allowed on indoor channel");
		return false;
	}

	if (mode == QDF_P2P_GO_MODE) {
		if (ucfg_p2p_get_indoor_ch_support(psoc) ||
		    (is_scc &&
		    policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(psoc)))
			return true;
		policy_mgr_rl_debug("GO operation is not allowed on indoor channel");
		return false;
	}

	policy_mgr_rl_debug("SAP operation is not allowed on indoor channel");
	return false;
}

bool policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(
		struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t sta_sap_scc_on_dfs_chnl = 0;
	bool status = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return status;
	}

	policy_mgr_get_sta_sap_scc_on_dfs_chnl(psoc,
					       &sta_sap_scc_on_dfs_chnl);
	if (policy_mgr_is_force_scc(psoc) && sta_sap_scc_on_dfs_chnl)
		status = true;

	return status;
}

bool policy_mgr_is_multi_sap_allowed_on_same_band(
					struct wlan_objmgr_pdev *pdev,
					enum policy_mgr_con_mode mode,
					qdf_freq_t ch_freq)
{
	struct wlan_objmgr_psoc *psoc;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool multi_sap_allowed_on_same_band;
	QDF_STATUS status;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (!ch_freq || !policy_mgr_is_sap_mode(mode))
		return true;

	status = policy_mgr_get_multi_sap_allowed_on_same_band(psoc,
					&multi_sap_allowed_on_same_band);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("Failed to get multi_sap_allowed_on_same_band");
		/* Allow multi SAPs started on same band by default. */
		multi_sap_allowed_on_same_band = true;
	}
	if (!multi_sap_allowed_on_same_band) {
		uint32_t ap_cnt, index = 0;
		uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
		struct policy_mgr_conc_connection_info *ap_info;

		ap_cnt = policy_mgr_get_sap_mode_count(psoc, list);
		if (!ap_cnt)
			return true;

		qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
		while (index < ap_cnt) {
			ap_info = &pm_conc_connection_list[list[index]];
			if (WLAN_REG_IS_SAME_BAND_FREQS(ch_freq,
							ap_info->freq)) {
				policy_mgr_rl_debug("Don't allow SAP on same band");
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				return false;
			}
			index++;
		}
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	}

	return true;
}

bool policy_mgr_is_special_mode_active_5g(struct wlan_objmgr_psoc *psoc,
					  enum policy_mgr_con_mode mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index;
	bool ret = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return ret;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].mode == mode &&
		    pm_conc_connection_list[conn_index].freq >=
					WLAN_REG_MIN_5GHZ_CHAN_FREQ &&
		    pm_conc_connection_list[conn_index].in_use)
			ret = true;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return ret;
}

bool policy_mgr_is_sta_connected_2g(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index;
	bool ret = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return ret;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].mode == PM_STA_MODE &&
		    pm_conc_connection_list[conn_index].freq <=
				WLAN_REG_MAX_24GHZ_CHAN_FREQ &&
		    pm_conc_connection_list[conn_index].in_use)
			ret = true;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return ret;
}

bool
policy_mgr_is_connected_sta_5g(struct wlan_objmgr_psoc *psoc, qdf_freq_t *freq)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index;
	bool ret = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return ret;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		*freq = pm_conc_connection_list[conn_index].freq;
		if (pm_conc_connection_list[conn_index].mode == PM_STA_MODE &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(*freq) &&
		    pm_conc_connection_list[conn_index].in_use) {
			ret = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return ret;
}

uint32_t policy_mgr_get_connection_info(struct wlan_objmgr_psoc *psoc,
					struct connection_info *info)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index, count = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
			info[count].vdev_id =
				pm_conc_connection_list[conn_index].vdev_id;
			info[count].mac_id =
				pm_conc_connection_list[conn_index].mac;
			info[count].channel = wlan_reg_freq_to_chan(
				pm_ctx->pdev,
				pm_conc_connection_list[conn_index].freq);
			info[count].ch_freq =
				pm_conc_connection_list[conn_index].freq;
			count++;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

bool policy_mgr_allow_sap_go_concurrency(struct wlan_objmgr_psoc *psoc,
					 enum policy_mgr_con_mode mode,
					 uint32_t ch_freq,
					 uint32_t vdev_id)
{
	enum policy_mgr_con_mode con_mode;
	int id;
	uint32_t vdev, con_freq;
	bool dbs;

	if (mode != PM_SAP_MODE && mode != PM_P2P_GO_MODE)
		return true;
	dbs = policy_mgr_is_hw_dbs_capable(psoc);
	for (id = 0; id < MAX_NUMBER_OF_CONC_CONNECTIONS; id++) {
		if (!pm_conc_connection_list[id].in_use)
			continue;
		vdev = pm_conc_connection_list[id].vdev_id;
		if (vdev_id == vdev)
			continue;
		con_mode = pm_conc_connection_list[id].mode;
		if (con_mode != PM_SAP_MODE && con_mode != PM_P2P_GO_MODE)
			continue;
		con_freq = pm_conc_connection_list[id].freq;

		if (policy_mgr_is_p2p_p2p_conc_supported(psoc) &&
		    (mode == PM_P2P_GO_MODE) && (con_mode == PM_P2P_GO_MODE)) {
			policy_mgr_debug("GO+GO scc is allowed freq = %d ",
					 ch_freq);
			return true;
		}

		if (policy_mgr_dual_beacon_on_single_mac_mcc_capable(psoc) &&
		    (mode == PM_SAP_MODE || mode == PM_P2P_GO_MODE) &&
		    (con_mode == PM_SAP_MODE || con_mode == PM_P2P_GO_MODE))
			return true;

		if (policy_mgr_dual_beacon_on_single_mac_scc_capable(psoc) &&
		    (ch_freq == con_freq)) {
			policy_mgr_debug("SCC enabled, 2 AP on same channel, allow 2nd AP");
			return true;
		}
		if (!dbs) {
			policy_mgr_debug("DBS unsupported, mcc and scc unsupported too, don't allow 2nd AP");
			return false;
		}

		if (policy_mgr_are_2_freq_on_same_mac(psoc, ch_freq,
						      con_freq)) {
			policy_mgr_debug("DBS supported, 2 SAP on same band, reject 2nd AP");
			return false;
		}
	}

	/* Don't block the second interface */
	return true;
}

bool policy_mgr_dual_beacon_on_single_mac_scc_capable(
		struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}

	if (wmi_service_enabled(
			wmi_handle,
			wmi_service_dual_beacon_on_single_mac_scc_support)) {
		policy_mgr_debug("Dual beaconing on same channel on single MAC supported");
		return true;
	}
	policy_mgr_debug("Dual beaconing on same channel on single MAC is not supported");
	return false;
}

bool policy_mgr_dual_beacon_on_single_mac_mcc_capable(
		struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}

	if (wmi_service_enabled(
			wmi_handle,
			wmi_service_dual_beacon_on_single_mac_mcc_support)) {
		policy_mgr_debug("Dual beaconing on different channel on single MAC supported");
		return true;
	}
	policy_mgr_debug("Dual beaconing on different channel on single MAC is not supported");
	return false;
}

bool policy_mgr_sta_sap_scc_on_lte_coex_chan(
	struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t scc_lte_coex = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	policy_mgr_get_sta_sap_scc_lte_coex_chnl(psoc, &scc_lte_coex);

	return scc_lte_coex;
}

#if defined(CONFIG_BAND_6GHZ) && defined(WLAN_FEATURE_11AX)
void policy_mgr_init_ap_6ghz_capable(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id,
				     enum conn_6ghz_flag ap_6ghz_capable)
{
	struct policy_mgr_conc_connection_info *conn_info;
	uint32_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum conn_6ghz_flag conn_6ghz_flag = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    policy_mgr_is_sap_mode(conn_info->mode) &&
		    vdev_id == conn_info->vdev_id) {
			conn_info->conn_6ghz_flag = ap_6ghz_capable;
			conn_info->conn_6ghz_flag |= CONN_6GHZ_FLAG_VALID;
			conn_6ghz_flag = conn_info->conn_6ghz_flag;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_debug("vdev %d init conn_6ghz_flag %x new %x",
			 vdev_id, ap_6ghz_capable, conn_6ghz_flag);
}

void policy_mgr_set_ap_6ghz_capable(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    bool set,
				    enum conn_6ghz_flag ap_6ghz_capable)
{
	struct policy_mgr_conc_connection_info *conn_info;
	uint32_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum conn_6ghz_flag conn_6ghz_flag = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    policy_mgr_is_beaconing_mode(conn_info->mode) &&
		    policy_mgr_is_6ghz_conc_mode_supported(
						psoc, conn_info->mode) &&
		    vdev_id == conn_info->vdev_id) {
			if (set)
				conn_info->conn_6ghz_flag |= ap_6ghz_capable;
			else
				conn_info->conn_6ghz_flag &= ~ap_6ghz_capable;
			conn_info->conn_6ghz_flag |= CONN_6GHZ_FLAG_VALID;
			conn_6ghz_flag = conn_info->conn_6ghz_flag;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_debug("vdev %d %s conn_6ghz_flag %x new %x",
			 vdev_id, set ? "set" : "clr",
			 ap_6ghz_capable, conn_6ghz_flag);
}

bool policy_mgr_get_ap_6ghz_capable(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    uint32_t *conn_flag)
{
	struct policy_mgr_conc_connection_info *conn_info;
	uint32_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum conn_6ghz_flag conn_6ghz_flag = 0;
	bool is_6g_allowed = false;
	uint32_t conn_6ghz_capable = CONN_6GHZ_CAPABLE;

	if (conn_flag)
		*conn_flag = 0;
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    policy_mgr_is_beaconing_mode(conn_info->mode) &&
		    policy_mgr_is_6ghz_conc_mode_supported(
						psoc, conn_info->mode) &&
		    vdev_id == conn_info->vdev_id) {
			conn_6ghz_flag = conn_info->conn_6ghz_flag;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	/* If the vdev connection is not active, policy mgr will query legacy
	 * hdd to get sap acs and security information.
	 * The assumption is no legacy client connected for non active
	 * connection.
	 */
	if (!(conn_6ghz_flag & CONN_6GHZ_FLAG_VALID) &&
	    pm_ctx->hdd_cbacks.hdd_get_ap_6ghz_capable)
		conn_6ghz_flag = pm_ctx->hdd_cbacks.hdd_get_ap_6ghz_capable(
					psoc, vdev_id) |
					CONN_6GHZ_FLAG_NO_LEGACY_CLIENT;

	if (wlan_reg_is_afc_power_event_received(pm_ctx->pdev))
		conn_6ghz_capable &= ~CONN_6GHZ_FLAG_ACS_OR_USR_ALLOWED;

	if ((conn_6ghz_flag & conn_6ghz_capable) == conn_6ghz_capable)
		is_6g_allowed = true;
	policy_mgr_debug("vdev %d conn_6ghz_flag %x 6ghz capable %x 6ghz %s",
			 vdev_id, conn_6ghz_flag, conn_6ghz_capable,
			 is_6g_allowed ? "allowed" : "deny");
	if (conn_flag)
		*conn_flag = conn_6ghz_flag;

	return is_6g_allowed;
}
#endif

bool policy_mgr_is_sta_sap_scc(struct wlan_objmgr_psoc *psoc,
			       uint32_t sap_freq)
{
	uint32_t conn_index;
	bool is_scc = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return is_scc;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if (pm_conc_connection_list[conn_index].in_use &&
				(pm_conc_connection_list[conn_index].mode ==
				PM_STA_MODE ||
				pm_conc_connection_list[conn_index].mode ==
				PM_P2P_CLIENT_MODE) && (sap_freq ==
				pm_conc_connection_list[conn_index].freq)) {
			is_scc = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return is_scc;
}

bool policy_mgr_go_scc_enforced(struct wlan_objmgr_psoc *psoc)
{
	uint32_t mcc_to_scc_switch;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	mcc_to_scc_switch = policy_mgr_get_mcc_to_scc_switch_mode(psoc);
	if (mcc_to_scc_switch ==
	    QDF_MCC_TO_SCC_SWITCH_FORCE_PREFERRED_WITHOUT_DISCONNECTION)
		return true;

	if (pm_ctx->cfg.go_force_scc && policy_mgr_is_force_scc(psoc))
		return true;

	return false;
}

uint8_t
policy_mgr_fetch_existing_con_info(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id, uint32_t freq,
				   enum policy_mgr_con_mode *mode,
				   uint32_t *existing_con_freq,
				   enum phy_ch_width *existing_ch_width)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return WLAN_UMAC_VDEV_ID_MAX;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if ((policy_mgr_is_beaconing_mode(
				pm_conc_connection_list[conn_index].mode) ||
		    pm_conc_connection_list[conn_index].mode ==
		    PM_P2P_CLIENT_MODE ||
		    pm_conc_connection_list[conn_index].mode ==
		    PM_STA_MODE) &&
		    pm_conc_connection_list[conn_index].in_use &&
		    policy_mgr_are_2_freq_on_same_mac(
			psoc, freq, pm_conc_connection_list[conn_index].freq) &&
		    freq != pm_conc_connection_list[conn_index].freq &&
		    vdev_id != pm_conc_connection_list[conn_index].vdev_id) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			policy_mgr_debug(
				"Existing vdev_id for mode %d is %d",
				pm_conc_connection_list[conn_index].mode,
				pm_conc_connection_list[conn_index].vdev_id);
			*mode = pm_conc_connection_list[conn_index].mode;
			*existing_con_freq =
				pm_conc_connection_list[conn_index].freq;
			*existing_ch_width = policy_mgr_get_ch_width(
					pm_conc_connection_list[conn_index].bw);
			return pm_conc_connection_list[conn_index].vdev_id;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return WLAN_UMAC_VDEV_ID_MAX;
}

#ifdef WLAN_FEATURE_P2P_P2P_STA
bool policy_mgr_is_go_scc_strict(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool ret = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		ret = false;
		goto return_val;
	}
	if (pm_ctx->cfg.go_force_scc & GO_FORCE_SCC_STRICT) {
		ret = true;
		goto return_val;
	}
	ret = false;
return_val:
	policy_mgr_debug("ret val is %d", ret);
	return ret;
}
#endif

QDF_STATUS policy_mgr_update_nan_vdev_mac_info(struct wlan_objmgr_psoc *psoc,
					       uint8_t nan_vdev_id,
					       uint8_t mac_id)
{
	struct policy_mgr_hw_mode_params hw_mode = {0};
	struct policy_mgr_vdev_mac_map vdev_mac_map = {0};
	QDF_STATUS status;

	vdev_mac_map.vdev_id = nan_vdev_id;
	vdev_mac_map.mac_id = mac_id;

	status = policy_mgr_get_current_hw_mode(psoc, &hw_mode);

	if (QDF_IS_STATUS_SUCCESS(status))
		policy_mgr_update_hw_mode_conn_info(psoc, 1, &vdev_mac_map,
						    hw_mode, 0, NULL);

	return status;
}

bool policy_mgr_is_sap_go_on_2g(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index;
	bool ret = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return ret;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		 conn_index++) {
		if ((pm_conc_connection_list[conn_index].mode == PM_SAP_MODE ||
		     pm_conc_connection_list[conn_index].mode == PM_P2P_GO_MODE) &&
			 pm_conc_connection_list[conn_index].freq <=
				WLAN_REG_MAX_24GHZ_CHAN_FREQ &&
			 pm_conc_connection_list[conn_index].in_use)
			ret = true;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return ret;
}

static inline bool
policy_mgr_is_chan_eligible_for_sap(struct policy_mgr_psoc_priv_obj *pm_ctx,
				    uint8_t vdev_id, qdf_freq_t freq)
{
	struct wlan_objmgr_vdev *vdev;
	enum channel_state ch_state;
	enum reg_6g_ap_type sta_connected_pwr_type;
	uint32_t ap_power_type_6g = 0;
	bool is_eligible = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(pm_ctx->psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev)
		return false;

	ch_state = wlan_reg_get_channel_state_for_pwrmode(pm_ctx->pdev,
							  freq,
							  REG_CURRENT_PWR_MODE);
	sta_connected_pwr_type = mlme_get_best_6g_power_type(vdev);
	wlan_reg_get_cur_6g_ap_pwr_type(pm_ctx->pdev, &ap_power_type_6g);

	/*
	 * If the SAP user configured frequency is 6 GHz,
	 * move the SAP to STA SCC in 6 GHz only if:
	 * a) The channel is PSC
	 * b) The channel supports AP in VLP power type
	 * c) The DUT is configured to operate SAP in VLP only
	 * d) The STA is connected to the 6 GHz AP in
	 *    either VLP or LPI.
	 *    - If the STA is in LPI, then lim_update_tx_power()
	 *	would move the STA to VLP.
	 */
	if (WLAN_REG_IS_6GHZ_PSC_CHAN_FREQ(freq) &&
	    ap_power_type_6g == REG_VERY_LOW_POWER_AP &&
	    ch_state == CHANNEL_STATE_ENABLE &&
	    (sta_connected_pwr_type == REG_VERY_LOW_POWER_AP ||
	     sta_connected_pwr_type == REG_INDOOR_AP))
		is_eligible = true;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
	return is_eligible;
}

bool policy_mgr_is_restart_sap_required(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					qdf_freq_t freq,
					tQDF_MCC_TO_SCC_SWITCH_MODE scc_mode)
{
	uint8_t i;
	bool restart_required = false;
	bool is_sta_p2p_cli;
	bool sap_on_dfs = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info *connection;
	bool sta_sap_scc_on_dfs_chan, sta_sap_scc_allowed_on_indoor_ch;
	qdf_freq_t user_config_freq;
	bool sap_found = false;
	uint8_t num_mcc_conn = 0;
	uint8_t num_scc_conn = 0;
	uint8_t num_5_or_6_conn = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid psoc");
		return false;
	}

	if (policy_mgr_is_vdev_ll_lt_sap(psoc, vdev_id)) {
		if (policy_mgr_is_ll_lt_sap_restart_required(psoc))
			return true;
		return false;
	}

	if (scc_mode == QDF_MCC_TO_SCC_SWITCH_DISABLE) {
		policy_mgr_debug("No scc required");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	connection = pm_conc_connection_list;
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (!connection[i].in_use)
			continue;
		if (connection[i].vdev_id == vdev_id) {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(connection[i].freq) &&
			    (connection[i].ch_flagext & (IEEE80211_CHAN_DFS |
					      IEEE80211_CHAN_DFS_CFREQ2)))
				sap_on_dfs = true;
			sap_found = true;
		} else {
			if (connection[i].freq == freq)
				num_scc_conn++;
			else
				num_mcc_conn++;

			if (!WLAN_REG_IS_24GHZ_CH_FREQ(connection[i].freq))
				num_5_or_6_conn++;
		}
	}
	if (!sap_found) {
		policy_mgr_err("Invalid vdev id: %d", vdev_id);
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
		return false;
	}
	/* Current hw mode is SBS low share. STA 5180, SAP 1 2412,
	 * SAP 2 5745, but SAP 1 is 2G only, can't move to STA 5180,
	 * SAP 2 is SBS with STA, policy_mgr_are_2_freq_on_same_mac
	 * return false for 5745 and 5180 and finally this function
	 * return false, no force SCC on SAP2.
	 * Add mcc conntion count check for SAP2, if SAP 2 channel
	 * is different from all of exsting 2 or more connections, then
	 * try to force SCC on SAP 2.
	 */
	if (num_mcc_conn > 1 && !num_scc_conn) {
		policy_mgr_debug("sap vdev %d has chan %d diff with %d exsting conn",
				 vdev_id, freq, num_mcc_conn);
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
		return true;
	}
	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);

	sta_sap_scc_allowed_on_indoor_ch =
		policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(psoc);

	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (!connection[i].in_use)
			continue;

		if (scc_mode == QDF_MCC_TO_SCC_SWITCH_WITH_FAVORITE_CHANNEL &&
		    connection[i].mode == PM_P2P_GO_MODE &&
		    connection[i].vdev_id != vdev_id &&
		    policy_mgr_2_freq_always_on_same_mac(psoc, freq,
							 connection[i].freq)) {
			policy_mgr_debug("SAP:%d and GO:%d on same mac. Restart SAP ",
					 freq, connection[i].freq);
			restart_required = true;
			break;
		}

		is_sta_p2p_cli =
			(connection[i].mode == PM_STA_MODE ||
			 connection[i].mode == PM_P2P_CLIENT_MODE);
		if (!is_sta_p2p_cli)
			continue;

		if (connection[i].freq != freq &&
		    policy_mgr_are_2_freq_on_same_mac(psoc, freq,
						      connection[i].freq)) {
			policy_mgr_debug("SAP:%d and STA:%d on same mac. Restart SAP ",
					 freq, connection[i].freq);
			restart_required = true;
			break;
		}
		if (connection[i].freq == freq &&
		    !sta_sap_scc_on_dfs_chan && sap_on_dfs) {
			policy_mgr_debug("Move SAP out of DFS ch:%d", freq);
			restart_required = true;
			break;
		}

		if (connection[i].freq == freq &&
		    !sta_sap_scc_allowed_on_indoor_ch &&
		    wlan_reg_is_freq_indoor(pm_ctx->pdev, connection[i].freq)) {
			policy_mgr_debug("Move SAP out of indoor ch:%d", freq);
			restart_required = true;
			break;
		}

		/*
		 * Existing connection:
		 * 1. "STA in DFS ch and SoftAP in 2.4 GHz channel, and then
		 * STA moves to 5 GHz non-DFS channel
		 *
		 * 2. "STA in indoor channel and sta_sap_scc_on_indoor_ch
		 * ini is false & SAP has moved to 2.4 GHz channel"
		 * STA moves back to 5 GHZ non indoor/non DFS channel
		 *
		 * Now SAP has to move to STA 5 GHz channel if SAP
		 * was started on 5 GHz channel initially.
		 */
		user_config_freq =
			policy_mgr_get_user_config_sap_freq(psoc, vdev_id);

		if (connection[i].freq != freq &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(freq) &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(connection[i].freq) &&
		    !wlan_reg_is_dfs_for_freq(pm_ctx->pdev,
					      connection[i].freq) &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(user_config_freq)) {
			policy_mgr_debug("Move SAP from:%d to STA ch:%d  (sap start freq:%d)",
					 freq, connection[i].freq,
					 user_config_freq);
			restart_required = true;

			if (wlan_reg_is_freq_indoor(pm_ctx->pdev,
						    connection[i].freq) &&
			    !sta_sap_scc_allowed_on_indoor_ch)
				restart_required = false;
			break;
		}

		/*
		 * SAP has to move away from indoor only channel
		 * when STA moves out of indoor only channel and
		 * SAP standalone support on indoor only
		 * channel ini is disabled
		 **/
		if (connection[i].freq != freq &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(connection[i].freq) &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(freq) &&
		    !policy_mgr_is_sap_go_interface_allowed_on_indoor(
							pm_ctx->pdev,
							vdev_id, freq)) {
			policy_mgr_debug("SAP in indoor freq: sta:%d sap:%d",
					 connection[i].freq, freq);
			restart_required = true;
		}

		if (scc_mode == QDF_MCC_TO_SCC_SWITCH_WITH_FAVORITE_CHANNEL &&
		    WLAN_REG_IS_24GHZ_CH_FREQ(freq) && user_config_freq) {
			if (connection[i].freq == freq && !num_5_or_6_conn &&
			    !WLAN_REG_IS_24GHZ_CH_FREQ(user_config_freq)) {
				policy_mgr_debug("SAP move to user configure %d from %d",
						 user_config_freq, freq);
				restart_required = true;
			} else if (connection[i].freq != freq &&
				   WLAN_REG_IS_6GHZ_CHAN_FREQ(user_config_freq) &&
				   policy_mgr_is_chan_eligible_for_sap(pm_ctx,
								       connection[i].vdev_id,
								       connection[i].freq)) {
				policy_mgr_debug("Move SAP to STA 6 GHz channel");
				restart_required = true;
			}
		}
	}

	if (!restart_required &&
	    policy_mgr_is_restart_sap_required_with_mlo_sta(
					psoc, vdev_id, freq))
		restart_required = true;

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return restart_required;
}

uint8_t policy_mgr_get_roam_enabled_sta_session_id(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id)
{
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t index, count;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wlan_objmgr_vdev *vdev, *assoc_vdev;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return WLAN_UMAC_VDEV_ID_MAX;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_POLICY_MGR_ID);
	if (!vdev) {
		policy_mgr_err("Invalid vdev");
		return WLAN_UMAC_VDEV_ID_MAX;
	}

	if (wlan_vdev_mlme_is_link_sta_vdev(vdev)) {
		assoc_vdev = ucfg_mlo_get_assoc_link_vdev(vdev);
		if (assoc_vdev && ucfg_cm_is_vdev_active(assoc_vdev)) {
			policy_mgr_debug("replace link vdev %d with assoc vdev %d",
					 vdev_id, wlan_vdev_get_id(assoc_vdev));
			vdev_id = wlan_vdev_get_id(assoc_vdev);
		}
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);

	count = policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, list);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);

	for (index = 0; index < count; index++) {
		if (vdev_id == pm_conc_connection_list[list[index]].vdev_id)
			continue;
		if (MLME_IS_ROAM_INITIALIZED(
			psoc, pm_conc_connection_list[list[index]].vdev_id)) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return pm_conc_connection_list[list[index]].vdev_id;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return WLAN_UMAC_VDEV_ID_MAX;
}

bool policy_mgr_is_sta_mon_concurrency(struct wlan_objmgr_psoc *psoc)
{
	uint32_t conc_mode;

	if (wlan_mlme_is_sta_mon_conc_supported(psoc)) {
		conc_mode = policy_mgr_get_concurrency_mode(psoc);
		if (conc_mode & QDF_STA_MASK &&
		    conc_mode & QDF_MONITOR_MASK) {
			policy_mgr_err("STA + MON mode is UP");
			return true;
		}
	}
	return false;
}

QDF_STATUS policy_mgr_check_mon_concurrency(struct wlan_objmgr_psoc *psoc)
{
	uint8_t num_open_session = 0;

	if (policy_mgr_mode_specific_num_open_sessions(
				psoc,
				QDF_MONITOR_MODE,
				&num_open_session) != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_INVAL;

	if (num_open_session) {
		policy_mgr_err("monitor mode already exists, only one is possible");
		return QDF_STATUS_E_BUSY;
	}

	num_open_session = policy_mgr_get_sap_mode_count(psoc, NULL);

	if (num_open_session) {
		policy_mgr_err("cannot add monitor mode, due to SAP concurrency");
		return QDF_STATUS_E_INVAL;
	}

	num_open_session = policy_mgr_mode_specific_connection_count(
					psoc,
					PM_P2P_CLIENT_MODE,
					NULL);

	if (num_open_session) {
		policy_mgr_err("cannot add monitor mode, due to P2P CLIENT concurrency");
		return QDF_STATUS_E_INVAL;
	}

	num_open_session = policy_mgr_mode_specific_connection_count(
					psoc,
					PM_P2P_GO_MODE,
					NULL);

	if (num_open_session) {
		policy_mgr_err("cannot add monitor mode, due to P2P GO concurrency");
		return QDF_STATUS_E_INVAL;
	}

	num_open_session = policy_mgr_mode_specific_connection_count(
					psoc,
					PM_NAN_DISC_MODE,
					NULL);

	if (num_open_session) {
		policy_mgr_err("cannot add monitor mode, due to NAN concurrency");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_is_hwmode_offload_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_err("Invalid WMI handle");
		return false;
	}

	return wmi_service_enabled(wmi_handle,
				   wmi_service_hw_mode_policy_offload_support);
}

bool policy_mgr_is_ap_ap_mcc_allow(struct wlan_objmgr_psoc *psoc,
				   struct wlan_objmgr_pdev *pdev,
				   struct wlan_objmgr_vdev *vdev,
				   uint32_t ch_freq,
				   enum phy_ch_width ch_width,
				   uint8_t *con_vdev_id,
				   uint32_t *con_freq)
{
	enum QDF_OPMODE mode;
	enum policy_mgr_con_mode con_mode;
	union conc_ext_flag conc_ext_flags;
	uint32_t cc_count, i, j, ap_index;
	bool found = false;
	uint32_t op_freq[MAX_NUMBER_OF_CONC_CONNECTIONS * 2];
	uint8_t vdev_id[MAX_NUMBER_OF_CONC_CONNECTIONS * 2];
	QDF_STATUS status;
	struct policy_mgr_pcl_list pcl;

	if (!psoc || !vdev || !pdev) {
		policy_mgr_debug("psoc or vdev or pdev is NULL");
		return false;
	}

	cc_count = policy_mgr_get_mode_specific_conn_info(psoc,
							  &op_freq[0],
							  &vdev_id[0],
							  PM_SAP_MODE);
	if (cc_count < MAX_NUMBER_OF_CONC_CONNECTIONS)
		cc_count = cc_count +
				policy_mgr_get_mode_specific_conn_info(
					psoc,
					&op_freq[cc_count],
					&vdev_id[cc_count],
					PM_P2P_GO_MODE);
	if (!cc_count)
		return true;

	mode = wlan_vdev_mlme_get_opmode(vdev);
	con_mode = policy_mgr_qdf_opmode_to_pm_con_mode(
				psoc, mode, wlan_vdev_get_id(vdev));
	qdf_mem_zero(&pcl, sizeof(pcl));
	status = policy_mgr_get_pcl(psoc, con_mode, pcl.pcl_list, &pcl.pcl_len,
				    pcl.weight_list,
				    QDF_ARRAY_SIZE(pcl.weight_list),
				    wlan_vdev_get_id(vdev));
	if (!pcl.pcl_len)
		return true;
	ap_index = cc_count;
	for (i = 0 ; i < pcl.pcl_len; i++) {
		for (j = 0; j < cc_count; j++) {
			if (op_freq[j] == pcl.pcl_list[i])
				break;
		}
		if (j >= cc_count)
			continue;
		if (ch_freq == op_freq[j]) {
			ap_index = j;
			break;
		}
		if (!policy_mgr_is_hw_dbs_capable(psoc)) {
			ap_index = j;
			break;
		}
		if (wlan_reg_is_same_band_freqs(ch_freq, op_freq[j]) &&
		    !policy_mgr_are_sbs_chan(psoc, ch_freq, op_freq[j])) {
			ap_index = j;
			break;
		}
		if (wlan_reg_is_same_band_freqs(ch_freq, op_freq[j]) &&
		    policy_mgr_get_connection_count(psoc) > 2) {
			ap_index = j;
			break;
		}
		if (policy_mgr_get_connection_count(psoc) >= 3 && !found) {
			if (ch_freq == pcl.pcl_list[i])
				found = true;
		}
	}

	/* For fourth connect check, if SAP setup freq not found in
	 * pcl.pcl_list, set ap_index 0 avoid return true, then
	 * SAP can start on ap_index's home channel instead of
	 * start failure.
	 */
	if (policy_mgr_get_connection_count(psoc) >= 3 && !found)
		ap_index = 0;

	/* If same band MCC SAP/GO not present, return true,
	 * no AP to AP channel override
	 */
	if (ap_index >= cc_count)
		return true;

	*con_freq = op_freq[ap_index];
	*con_vdev_id = vdev_id[ap_index];
	/*
	 * For 3Vif concurrency we only support SCC in same MAC
	 * in below combination:
	 * 2 beaconing entities with STA in SCC.
	 * 3 beaconing entities in SCC.
	 */
	conc_ext_flags.value = policy_mgr_get_conc_ext_flags(vdev, false);
	if (!policy_mgr_allow_concurrency(
			psoc, con_mode, ch_freq,
			policy_mgr_get_bw(ch_width),
			conc_ext_flags.value,
			wlan_vdev_get_id(vdev))) {
		policy_mgr_debug("AP AP mcc not allowed, try to override 2nd SAP/GO chan");
		return false;
	}
	/* For SCC case & bandwdith > 20, the center frequency have to be
	 * same to avoid target MCC on different center frequency even though
	 * primary channel are same.
	 */
	if (*con_freq == ch_freq && wlan_reg_get_bw_value(ch_width) > 20)
		return false;

	return true;
}

bool policy_mgr_any_other_vdev_on_same_mac_as_freq(
				struct wlan_objmgr_psoc *psoc,
				uint32_t freq, uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index = 0;
	bool same_mac = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (!pm_conc_connection_list[conn_index].in_use)
			continue;

		if (pm_conc_connection_list[conn_index].vdev_id == vdev_id)
			continue;

		if (policy_mgr_are_2_freq_on_same_mac(
				psoc,
				pm_conc_connection_list[conn_index].freq,
				freq)) {
			same_mac = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return same_mac;
}

QDF_STATUS policy_mgr_get_sbs_cfg(struct wlan_objmgr_psoc *psoc, bool *sbs)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*sbs = pm_ctx->cfg.sbs_enable;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_SR
bool policy_mgr_sr_same_mac_conc_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;
	bool sr_conc_enabled;

	if (!psoc) {
		mlme_err("PSOC is NULL");
		return false;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		mlme_err("wmi_handle is null");
		return false;
	}

	sr_conc_enabled = policy_mgr_get_same_mac_conc_sr_status(psoc);

	return (sr_conc_enabled &&
		wmi_service_enabled(wmi_handle,
				    wmi_service_obss_per_packet_sr_support));
}
#endif

/**
 * _policy_mgr_get_ll_sap_freq()- Function to get LL sap freq if it's present
 * for provided type
 * @psoc: PSOC object
 * @ap_type: low latency ap type
 *
 * Return: freq if LL SAP otherwise return 0
 *
 */
static qdf_freq_t _policy_mgr_get_ll_sap_freq(struct wlan_objmgr_psoc *psoc,
					      enum ll_ap_type ap_type)
{
	struct wlan_objmgr_vdev *sap_vdev;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_idx = 0, vdev_id;
	bool is_ll_sap_present = false;
	qdf_freq_t freq = 0;
	enum host_concurrent_ap_policy profile =
					HOST_CONCURRENT_AP_POLICY_UNSPECIFIED;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return 0;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_idx = 0; conn_idx < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_idx++) {
		if (!(policy_mgr_is_sap_mode(
				pm_conc_connection_list[conn_idx].mode) &&
		      pm_conc_connection_list[conn_idx].in_use))
			continue;

		vdev_id = pm_conc_connection_list[conn_idx].vdev_id;
		freq = pm_conc_connection_list[conn_idx].freq;

		sap_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
				psoc,
				vdev_id,
				WLAN_POLICY_MGR_ID);

		if (!sap_vdev) {
			policy_mgr_err("vdev %d: not a sap vdev", vdev_id);
			continue;
		}

		profile = wlan_mlme_get_ap_policy(sap_vdev);
		wlan_objmgr_vdev_release_ref(sap_vdev,
					     WLAN_POLICY_MGR_ID);
		switch (ap_type) {
		case LL_AP_TYPE_HT:
			if (profile == HOST_CONCURRENT_AP_POLICY_XR)
				is_ll_sap_present = true;
		break;
		case LL_AP_TYPE_LT:
			if (profile == HOST_CONCURRENT_AP_POLICY_GAMING_AUDIO ||
			    profile ==
			    HOST_CONCURRENT_AP_POLICY_LOSSLESS_AUDIO_STREAMING)
				is_ll_sap_present = true;
		break;
		case LL_AP_TYPE_ANY:
			if (profile == HOST_CONCURRENT_AP_POLICY_GAMING_AUDIO ||
			    profile == HOST_CONCURRENT_AP_POLICY_XR ||
			    profile ==
			    HOST_CONCURRENT_AP_POLICY_LOSSLESS_AUDIO_STREAMING)
				is_ll_sap_present = true;
		break;
		default:
		break;
		}
		if (!is_ll_sap_present)
			continue;

		policy_mgr_rl_debug("LL SAP %d present with vdev_id %d and freq %d",
				    ap_type, vdev_id, freq);

		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
		return freq;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	return 0;
}

qdf_freq_t policy_mgr_get_ll_sap_freq(struct wlan_objmgr_psoc *psoc)
{
	return _policy_mgr_get_ll_sap_freq(psoc, LL_AP_TYPE_ANY);
}

qdf_freq_t policy_mgr_get_ll_ht_sap_freq(struct wlan_objmgr_psoc *psoc)
{
	return _policy_mgr_get_ll_sap_freq(psoc, LL_AP_TYPE_HT);
}

qdf_freq_t policy_mgr_get_ll_lt_sap_freq(struct wlan_objmgr_psoc *psoc)
{
	return _policy_mgr_get_ll_sap_freq(psoc, LL_AP_TYPE_LT);
}

bool
policy_mgr_update_indoor_concurrency(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id,
				     uint32_t discon_freq,
				     enum indoor_conc_update_type type)
{
	uint32_t ch_freq;
	enum QDF_OPMODE mode;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum phy_ch_width ch_width = CH_WIDTH_INVALID;
	bool indoor_support = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm context");
		return false;
	}

	ucfg_mlme_get_indoor_channel_support(psoc, &indoor_support);
	if (indoor_support ||
	    !policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(psoc))
		return false;

	mode = wlan_get_opmode_from_vdev_id(pm_ctx->pdev, vdev_id);

	/**
	 * DISCONNECT_WITH_CONCURRENCY update comes after SAP/GO CSA.
	 * Whereas, all other updates come from STA/GC operation.
	 */
	if (type != DISCONNECT_WITH_CONCURRENCY &&
	    (mode != QDF_STA_MODE && mode != QDF_P2P_CLIENT_MODE)) {
		return false;
	} else if (type == DISCONNECT_WITH_CONCURRENCY &&
		 (mode != QDF_SAP_MODE && mode != QDF_P2P_GO_MODE)) {
		return false;
	}

	switch (type) {
	case CONNECT:
	case SWITCH_WITHOUT_CONCURRENCY:
	case SWITCH_WITH_CONCURRENCY:
		policy_mgr_get_chan_by_session_id(psoc, vdev_id, &ch_freq);
		ch_width = policy_mgr_get_bw_by_session_id(psoc, vdev_id);
		break;
	case DISCONNECT_WITHOUT_CONCURRENCY:
	case DISCONNECT_WITH_CONCURRENCY:
		ch_freq = discon_freq;
		break;
	default:
		return false;
	}

	if (type != SWITCH_WITHOUT_CONCURRENCY &&
	    !(WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq) &&
	    wlan_reg_is_freq_indoor(pm_ctx->pdev, ch_freq))) {
		return false;
	} else if (type == SWITCH_WITHOUT_CONCURRENCY) {
		/* Either the previous frequency or the current
		 * frequency can be indoor. Or both can be indoor.
		 * Therefore, atleast one of the frequency must be
		 * indoor in order to proceed for the update.
		 */
		if (!((WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq) &&
		       wlan_reg_is_freq_indoor(pm_ctx->pdev, ch_freq)) ||
		      (WLAN_REG_IS_5GHZ_CH_FREQ(discon_freq) &&
		       wlan_reg_is_freq_indoor(pm_ctx->pdev, discon_freq))))
			return false;
	}

	switch (type) {
	case CONNECT:
		wlan_reg_modify_indoor_concurrency(pm_ctx->pdev, vdev_id,
						   ch_freq, ch_width, true);
		break;
	case DISCONNECT_WITHOUT_CONCURRENCY:
		wlan_reg_modify_indoor_concurrency(pm_ctx->pdev, vdev_id,
						   0, CH_WIDTH_INVALID, false);
		break;
	case SWITCH_WITHOUT_CONCURRENCY:
		wlan_reg_modify_indoor_concurrency(pm_ctx->pdev, vdev_id, 0,
						   CH_WIDTH_INVALID, false);
		if (wlan_reg_is_freq_indoor(pm_ctx->pdev, ch_freq))
			wlan_reg_modify_indoor_concurrency(pm_ctx->pdev,
							   vdev_id, ch_freq,
							   ch_width, true);
		break;
	case DISCONNECT_WITH_CONCURRENCY:
		/*If there are other sessions, do not change current chan list*/
		if (policy_mgr_get_connection_count_with_ch_freq(ch_freq) > 1)
			return false;
		wlan_reg_modify_indoor_concurrency(pm_ctx->pdev,
						   INVALID_VDEV_ID, ch_freq,
						   CH_WIDTH_INVALID, false);
		break;
	case SWITCH_WITH_CONCURRENCY:
		wlan_reg_modify_indoor_concurrency(pm_ctx->pdev, vdev_id,
						   ch_freq, ch_width, true);
		/*
		 * The previous frequency removal and current channel list
		 * recomputation will happen after SAP CSA
		 */
		return false;
	}
	return true;
}

bool policy_mgr_is_conc_sap_present_on_sta_freq(struct wlan_objmgr_psoc *psoc,
						enum policy_mgr_con_mode mode,
						uint32_t ch_freq)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t i;
	bool sap_go_exists = false;
	enum policy_mgr_con_mode cmode;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid pm context");
		return false;
	}

	if (mode != PM_STA_MODE && mode != PM_P2P_CLIENT_MODE)
		return false;

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		cmode = pm_conc_connection_list[i].mode;
		if (pm_conc_connection_list[i].in_use &&
		    ch_freq == pm_conc_connection_list[i].freq &&
		    (cmode == PM_SAP_MODE || cmode == PM_P2P_GO_MODE)) {
			sap_go_exists = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return sap_go_exists;
}

bool policy_mgr_is_sap_mode(enum policy_mgr_con_mode mode)
{
	if (mode == PM_SAP_MODE || mode == PM_LL_LT_SAP_MODE)
		return true;

	return false;
}

bool policy_mgr_is_beaconing_mode(enum policy_mgr_con_mode mode)
{
	if (mode == PM_SAP_MODE || mode == PM_LL_LT_SAP_MODE ||
	    mode == PM_P2P_GO_MODE)
		return true;

	return false;
}

bool policy_mgr_get_nan_sap_scc_on_lte_coex_chnl(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return 0;
	}
	return pm_ctx->cfg.nan_sap_scc_on_lte_coex_chnl;
}

QDF_STATUS
policy_mgr_reset_sap_mandatory_channels(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->sap_mandatory_channels_len = 0;
	qdf_mem_zero(pm_ctx->sap_mandatory_channels,
		     QDF_ARRAY_SIZE(pm_ctx->sap_mandatory_channels) *
		     sizeof(*pm_ctx->sap_mandatory_channels));

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_is_freq_on_mac_id(struct policy_mgr_freq_range *freq_range,
				  qdf_freq_t freq, uint8_t mac_id)
{
	return IS_FREQ_ON_MAC_ID(freq_range, freq, mac_id);
}

bool policy_mgr_get_vdev_same_freq_new_conn(struct wlan_objmgr_psoc *psoc,
					    uint8_t self_vdev_id,
					    uint32_t new_freq,
					    uint8_t *vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool match = false;
	uint32_t i;

	pm_ctx = policy_mgr_get_context(psoc);
	if (qdf_unlikely(!pm_ctx)) {
		policy_mgr_err("Invalid pm_ctx");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].in_use &&
		    pm_conc_connection_list[i].freq == new_freq &&
		    pm_conc_connection_list[i].vdev_id != self_vdev_id) {
			match = true;
			*vdev_id = pm_conc_connection_list[i].vdev_id;
			policy_mgr_debug("new_freq %d matched with vdev_id %d",
					 new_freq, *vdev_id);
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return match;
}

bool policy_mgr_get_vdev_diff_freq_new_conn(struct wlan_objmgr_psoc *psoc,
					    uint32_t new_freq,
					    uint8_t *vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool match = false;
	uint32_t i;

	pm_ctx = policy_mgr_get_context(psoc);
	if (qdf_unlikely(!pm_ctx)) {
		policy_mgr_err("Invalid pm_ctx");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].in_use &&
		    pm_conc_connection_list[i].freq != new_freq) {
			match = true;
			*vdev_id = pm_conc_connection_list[i].vdev_id;
			policy_mgr_debug("new_freq %d matched with vdev_id %d",
					 new_freq, *vdev_id);
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return match;
}

enum hw_mode_bandwidth
policy_mgr_get_connection_max_channel_width(struct wlan_objmgr_psoc *psoc)
{
	enum hw_mode_bandwidth bw = HW_MODE_20_MHZ;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return HW_MODE_20_MHZ;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);

	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].in_use &&
		    pm_conc_connection_list[conn_index].bw > bw)
			bw = pm_conc_connection_list[conn_index].bw;
	}

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return bw;
}

#ifdef WLAN_FEATURE_11BE_MLO
bool policy_mgr_allow_non_force_link_bitmap(
			struct wlan_objmgr_psoc *psoc,
			struct wlan_objmgr_vdev *vdev,
			uint16_t no_forced_bitmap,
			uint16_t force_inactive_bitmap)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t vdev_id_num = 0;
	uint8_t vdev_ids[WLAN_MLO_MAX_VDEVS];
	uint32_t vdev_id_bitmap_sz;
	uint32_t vdev_id_bitmap[MLO_VDEV_BITMAP_SZ];
	uint8_t i;
	union conc_ext_flag conc_ext_flags;
	struct wlan_objmgr_vdev *ml_vdev;
	bool allow = true;
	qdf_freq_t freq = 0;
	struct wlan_channel *bss_chan;
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	uint8_t num_del, num_del_total = 0;
	uint8_t vdev_id_num2 = 0;
	uint8_t vdev_ids2[WLAN_MLO_MAX_VDEVS];
	uint32_t standby_link_bitmap;
	qdf_freq_t standby_freq = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	/* Check standby link is in no_forced_bitmap */
	standby_link_bitmap = ml_nlink_get_standby_link_bitmap(psoc, vdev);
	standby_link_bitmap &= no_forced_bitmap;
	if (standby_link_bitmap)
		standby_freq =
		ml_nlink_get_standby_link_freq(psoc, vdev,
					       standby_link_bitmap);

	ml_nlink_convert_linkid_bitmap_to_vdev_bitmap(
		psoc, vdev, no_forced_bitmap, NULL, &vdev_id_bitmap_sz,
		vdev_id_bitmap,	&vdev_id_num, vdev_ids);

	if (!vdev_id_num && !standby_freq)
		return true;

	vdev_id_num2 = 0;
	if (force_inactive_bitmap)
		ml_nlink_convert_linkid_bitmap_to_vdev_bitmap(
			psoc, vdev, force_inactive_bitmap, NULL,
			&vdev_id_bitmap_sz,
			vdev_id_bitmap, &vdev_id_num2, vdev_ids2);

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	/* remove the force inactive link's vdev from connection
	 * table.
	 */
	for (i = 0; i < vdev_id_num2; i++) {
		if (num_del_total >= QDF_ARRAY_SIZE(info))
			break;
		num_del = 0;
		policy_mgr_store_and_del_conn_info_by_vdev_id(
			psoc, vdev_ids2[i], &info[num_del_total],
			&num_del);
		num_del_total += num_del;
	}

	/* Check standby link allowed to be active (no forced) */
	if (standby_freq) {
		conc_ext_flags.value = 0;
		conc_ext_flags.mlo = true;
		conc_ext_flags.mlo_link_assoc_connected = true;

		if (!policy_mgr_is_concurrency_allowed(psoc, PM_STA_MODE,
						       standby_freq,
						       HW_MODE_20_MHZ,
						       conc_ext_flags.value,
						       NULL)) {
			allow = false;
			policy_mgr_debug("not allow - standby link 0x%x freq %d active due to conc",
					 standby_link_bitmap, standby_freq);
			goto restore_conn;
		}
	}

	/* Check inactive vdev is allowed to active (no forced) */
	for (i = 0; i < vdev_id_num; i++) {
		ml_vdev =
		wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						     vdev_ids[i],
						     WLAN_MLO_MGR_ID);
		if (!ml_vdev) {
			policy_mgr_err("invalid vdev id %d ", vdev_ids[i]);
			continue;
		}

		/* If link is active, no need to check allow conc */
		if (!policy_mgr_vdev_is_force_inactive(psoc, vdev_ids[i])) {
			wlan_objmgr_vdev_release_ref(ml_vdev,
						     WLAN_MLO_MGR_ID);
			continue;
		}

		conc_ext_flags.value =
		policy_mgr_get_conc_ext_flags(ml_vdev, true);

		bss_chan = wlan_vdev_mlme_get_bss_chan(ml_vdev);
		if (bss_chan)
			freq = bss_chan->ch_freq;

		if (!policy_mgr_is_concurrency_allowed(psoc, PM_STA_MODE,
						       freq,
						       HW_MODE_20_MHZ,
						       conc_ext_flags.value,
						       NULL)) {
			wlan_objmgr_vdev_release_ref(ml_vdev,
						     WLAN_MLO_MGR_ID);
			break;
		}
		wlan_objmgr_vdev_release_ref(ml_vdev, WLAN_MLO_MGR_ID);
	}

	if (i < vdev_id_num) {
		policy_mgr_debug("not allow - vdev %d freq %d active due to conc",
				 vdev_ids[i], freq);
		allow = false;
	}

restore_conn:
	if (num_del_total > 0)
		policy_mgr_restore_deleted_conn_info(psoc, info,
						     num_del_total);
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return allow;
}

bool
policy_mgr_update_disallowed_mode_bitmap(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_vdev *vdev,
					 struct mlo_link_set_active_req *req,
					 uint32_t link_control_flags)
{
	struct wlan_mlo_dev_context *mlo_dev_ctx;
	uint32_t emlsr_disable_req;

	if (!vdev)
		return false;

	if (!req)
		return false;

	mlo_dev_ctx = vdev->mlo_dev_ctx;
	if (!mlo_dev_ctx)
		return false;

	if (!policy_mgr_is_mlo_in_mode_emlsr(psoc, NULL, NULL) ||
	    !wlan_mlme_is_aux_emlsr_support(psoc))
		return false;

	/* If emlsr is disabled by ap start/conn start/csa/opportunistic
	 * timer, use ml_nlink_populate_disallow_modes to populate the
	 * disallow bitmap which will consider the current disable requests.
	 */
	emlsr_disable_req = ml_nlink_get_emlsr_mode_disable_req(psoc, vdev);
	emlsr_disable_req &= ML_EMLSR_DISABLE_MASK_ALL &
	    ~ML_EMLSR_DISALLOW_BY_CONCURENCY;

	policy_mgr_init_disallow_mode_bmap(req);
	if (!emlsr_disable_req &&
	    policy_mgr_get_connection_count_with_mlo(psoc) == 1) {
		ml_nlink_clr_emlsr_mode_disable_req(
				psoc, vdev,
				ML_EMLSR_DISALLOW_BY_CONCURENCY);
		ml_nlink_populate_disallow_modes(psoc, vdev, req,
						 link_control_flags);
	} else {
		ml_nlink_populate_disallow_modes(psoc, vdev, req,
						 link_control_flags);
	}

	return true;
}

bool
policy_mgr_init_disallow_mode_bmap(struct mlo_link_set_active_req *req)
{
	uint8_t i;

	if (!req)
		return false;

	/* set link id to invalid */
	for (i = 0; i < MAX_DISALLOW_BMAP_COMB ; i++)
		req->param.disallow_mode_link_bmap[i].ieee_link_id_comb = MLO_INVALID_LINK_BMAP;

	return true;
}

enum policy_mgr_curr_hw_mode
policy_mgr_find_current_hw_mode(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	uint32_t old_hw_index = 0, new_hw_index = 0;
	struct policy_mgr_hw_mode_params hw_mode;

	status = policy_mgr_get_old_and_new_hw_index(psoc, &old_hw_index,
						     &new_hw_index);
	if (QDF_STATUS_SUCCESS != status) {
		policy_mgr_err("Failed to get HW mode index");
		goto end;
	}

	if (new_hw_index == POLICY_MGR_DEFAULT_HW_MODE_INDEX) {
		policy_mgr_err("HW mode is not yet initialized");
		goto end;
	}

	status = policy_mgr_get_hw_mode_from_idx(psoc, new_hw_index, &hw_mode);
	if (QDF_STATUS_SUCCESS != status) {
		policy_mgr_err("Failed to get HW mode index");
		goto end;
	}

	policy_mgr_debug("Old hw_mode: %d New HW mode: %d", old_hw_index, new_hw_index);
	if (new_hw_index >= POLICY_MGR_HW_MODE_SINGLE &&
	    new_hw_index <= POLICY_MGR_HW_MODE_AUX_EMLSR_SPLIT)
		return new_hw_index;
end:
	return POLICY_MGR_HW_MODE_INVALID;
}
#endif

bool
policy_mgr_allow_concurrency_sta_csa(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id,
				     enum QDF_OPMODE mode,
				     qdf_freq_t csa_freq,
				     enum phy_ch_width new_ch_width)
{
	bool is_allowed = true;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS] = { {0} };
	uint8_t num_del = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_store_and_del_conn_info_by_vdev_id(
			psoc,
			vdev_id,
			info, &num_del);

	is_allowed =
	policy_mgr_is_concurrency_allowed(psoc,
		policy_mgr_qdf_opmode_to_pm_con_mode(psoc,
						     mode,
						     vdev_id),
		csa_freq,
		policy_mgr_get_bw(new_ch_width),
		0,
		NULL);

	/* Restore the connection info */
	if (num_del > 0)
		policy_mgr_restore_deleted_conn_info(psoc,
						     info,
						     num_del);
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	return is_allowed;
}

#ifdef AUTO_PLATFORM
bool policy_mgr_is_3vifs_mcc_to_scc_enabled(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_is_force_scc(psoc);
}
#endif
