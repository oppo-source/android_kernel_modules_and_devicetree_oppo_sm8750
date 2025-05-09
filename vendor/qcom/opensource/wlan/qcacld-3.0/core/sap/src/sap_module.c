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

/*
 *                     sap_module.c
 *  OVERVIEW:
 *  This software unit holds the implementation of the WLAN SAP modules
 *  functions providing EXTERNAL APIs. It is also where the global SAP module
 *  context gets initialised
 *  DEPENDENCIES:
 *  Are listed for each API below.
 */

/* $Header$ */

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "qdf_trace.h"
#include "qdf_util.h"
#include "qdf_atomic.h"
/* Pick up the sme callback registration API */
#include "sme_api.h"

/* SAP API header file */

#include "sap_internal.h"
#include "sme_inside.h"
#include "cds_ieee80211_common_i.h"
#include "cds_regdomain.h"
#include "wlan_policy_mgr_api.h"
#include <wlan_scan_api.h>
#include "wlan_reg_services_api.h"
#include <wlan_dfs_utils_api.h>
#include <wlan_reg_ucfg_api.h>
#include <wlan_cfg80211_crypto.h>
#include <wlan_crypto_global_api.h>
#include "cfg_ucfg_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include "pld_common.h"
#include "wlan_pre_cac_api.h"
#include "target_if.h"
#include "wlan_hdd_regulatory.h"

#define SAP_DEBUG
static struct sap_context *gp_sap_ctx[SAP_MAX_NUM_SESSION];
static qdf_atomic_t sap_ctx_ref_count[SAP_MAX_NUM_SESSION];
static qdf_mutex_t sap_context_lock;

/**
 * wlansap_global_init() - Initialize SAP globals
 *
 * Initializes the SAP global data structures
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_global_init(void)
{
	uint32_t i;

	if (QDF_IS_STATUS_ERROR(qdf_mutex_create(&sap_context_lock))) {
		sap_err("failed to init sap_context_lock");
		return QDF_STATUS_E_FAULT;
	}

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		gp_sap_ctx[i] = NULL;
		qdf_atomic_init(&sap_ctx_ref_count[i]);
	}

	sap_debug("sap global context initialized");

	return QDF_STATUS_SUCCESS;
}

/**
 * wlansap_global_deinit() - De-initialize SAP globals
 *
 * De-initializes the SAP global data structures
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_global_deinit(void)
{
	uint32_t i;

	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (gp_sap_ctx[i]) {
			sap_err("we could be leaking context:%d", i);
		}
		gp_sap_ctx[i] = NULL;
		qdf_atomic_init(&sap_ctx_ref_count[i]);
	}

	if (QDF_IS_STATUS_ERROR(qdf_mutex_destroy(&sap_context_lock))) {
		sap_err("failed to destroy sap_context_lock");
		return QDF_STATUS_E_FAULT;
	}

	sap_debug("sap global context deinitialized");

	return QDF_STATUS_SUCCESS;
}

/**
 * wlansap_save_context() - Save the context in global SAP context
 * @ctx: SAP context to be stored
 *
 * Stores the given SAP context in the global SAP context array
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wlansap_save_context(struct sap_context *ctx)
{
	uint32_t i;

	qdf_mutex_acquire(&sap_context_lock);
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (!gp_sap_ctx[i]) {
			gp_sap_ctx[i] = ctx;
			qdf_atomic_inc(&sap_ctx_ref_count[i]);
			qdf_mutex_release(&sap_context_lock);
			sap_debug("sap context saved at index: %d", i);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&sap_context_lock);

	sap_err("failed to save sap context");

	return QDF_STATUS_E_FAILURE;
}

/**
 * wlansap_context_get() - Verify SAP context and increment ref count
 * @ctx: Context to be checked
 *
 * Verifies the SAP context and increments the reference count maintained for
 * the corresponding SAP context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlansap_context_get(struct sap_context *ctx)
{
	uint32_t i;

	qdf_mutex_acquire(&sap_context_lock);
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (ctx && (gp_sap_ctx[i] == ctx)) {
			qdf_atomic_inc(&sap_ctx_ref_count[i]);
			qdf_mutex_release(&sap_context_lock);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&sap_context_lock);

	sap_debug("sap session is not valid");
	return QDF_STATUS_E_FAILURE;
}

/**
 * wlansap_context_put() - Check the reference count and free SAP context
 * @ctx: SAP context to be checked and freed
 *
 * Checks the reference count and frees the SAP context
 *
 * Return: None
 */
void wlansap_context_put(struct sap_context *ctx)
{
	uint32_t i;

	if (!ctx)
		return;

	qdf_mutex_acquire(&sap_context_lock);
	for (i = 0; i < SAP_MAX_NUM_SESSION; i++) {
		if (gp_sap_ctx[i] == ctx) {
			if (qdf_atomic_dec_and_test(&sap_ctx_ref_count[i])) {
				if (ctx->freq_list) {
					qdf_mem_free(ctx->freq_list);
					ctx->freq_list = NULL;
					ctx->num_of_channel = 0;
				}
				qdf_mem_free(ctx);
				gp_sap_ctx[i] = NULL;
				sap_debug("sap session freed: %d", i);
			}
			qdf_mutex_release(&sap_context_lock);
			return;
		}
	}
	qdf_mutex_release(&sap_context_lock);
}

struct sap_context *sap_create_ctx(void *link_info)
{
	struct sap_context *sap_ctx;
	QDF_STATUS status;

	sap_ctx = qdf_mem_malloc(sizeof(*sap_ctx));
	if (!sap_ctx)
		return NULL;

	/* Clean up SAP control block, initialize all values */
	/* Save the SAP context pointer */
	status = wlansap_save_context(sap_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("failed to save SAP context");
		qdf_mem_free(sap_ctx);
		return NULL;
	}

	sap_ctx->user_context = link_info;
	sap_debug("Exit");

	return sap_ctx;
} /* sap_create_ctx */

static QDF_STATUS wlansap_owe_init(struct sap_context *sap_ctx)
{
	qdf_list_create(&sap_ctx->owe_pending_assoc_ind_list, 0);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS wlansap_ft_init(struct sap_context *sap_ctx)
{
	qdf_list_create(&sap_ctx->ft_pending_assoc_ind_list, 0);
	qdf_event_create(&sap_ctx->ft_pending_event);

	return QDF_STATUS_SUCCESS;
}

static void wlansap_owe_cleanup(struct sap_context *sap_ctx)
{
	struct mac_context *mac;
	struct owe_assoc_ind *owe_assoc_ind;
	struct assoc_ind *assoc_ind = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	QDF_STATUS status;

	if (!sap_ctx) {
		sap_err("Invalid SAP context");
		return;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return;
	}

	if (QDF_STATUS_SUCCESS !=
	    qdf_list_peek_front(&sap_ctx->owe_pending_assoc_ind_list,
				&node)) {
		sap_debug("Failed to find assoc ind list");
		return;
	}

	while (node) {
		qdf_list_peek_next(&sap_ctx->owe_pending_assoc_ind_list,
				   node, &next_node);
		owe_assoc_ind = qdf_container_of(node, struct owe_assoc_ind,
						 node);
		status = qdf_list_remove_node(
					   &sap_ctx->owe_pending_assoc_ind_list,
					   node);
		if (status == QDF_STATUS_SUCCESS) {
			assoc_ind = owe_assoc_ind->assoc_ind;
			qdf_mem_free(owe_assoc_ind);
			assoc_ind->owe_ie = NULL;
			assoc_ind->owe_ie_len = 0;
			assoc_ind->owe_status = STATUS_UNSPECIFIED_FAILURE;
			status = sme_update_owe_info(mac, assoc_ind);
			qdf_mem_free(assoc_ind);
		} else {
			sap_err("Failed to remove assoc ind");
		}
		node = next_node;
		next_node = NULL;
	}
}

static void wlansap_ft_cleanup(struct sap_context *sap_ctx)
{
	struct mac_context *mac;
	struct ft_assoc_ind *ft_assoc_ind;
	struct assoc_ind *assoc_ind = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	QDF_STATUS status;

	if (!sap_ctx) {
		sap_err("Invalid SAP context");
		return;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return;
	}

	if (QDF_STATUS_SUCCESS !=
	    qdf_list_peek_front(&sap_ctx->ft_pending_assoc_ind_list,
				&node)) {
		sap_debug("Failed to find assoc ind list");
		return;
	}

	while (node) {
		qdf_list_peek_next(&sap_ctx->ft_pending_assoc_ind_list,
				   node, &next_node);
		ft_assoc_ind = qdf_container_of(node, struct ft_assoc_ind,
						node);
		status = qdf_list_remove_node(
				    &sap_ctx->ft_pending_assoc_ind_list, node);
		if (status == QDF_STATUS_SUCCESS) {
			assoc_ind = ft_assoc_ind->assoc_ind;
			qdf_mem_free(ft_assoc_ind);
			assoc_ind->ft_ie = NULL;
			assoc_ind->ft_ie_len = 0;
			assoc_ind->ft_status = STATUS_UNSPECIFIED_FAILURE;
			qdf_mem_free(assoc_ind);
		} else {
			sap_err("Failed to remove assoc ind");
		}
		node = next_node;
		next_node = NULL;
	}
}

static void wlansap_owe_deinit(struct sap_context *sap_ctx)
{
	qdf_list_destroy(&sap_ctx->owe_pending_assoc_ind_list);
}

static void wlansap_ft_deinit(struct sap_context *sap_ctx)
{
	qdf_list_destroy(&sap_ctx->ft_pending_assoc_ind_list);
	qdf_event_destroy(&sap_ctx->ft_pending_event);
}

QDF_STATUS sap_init_ctx(struct sap_context *sap_ctx,
			enum QDF_OPMODE mode,
			uint8_t *addr, uint32_t session_id,
			bool cac_offload, bool reinit)
{
	QDF_STATUS status;
	struct mac_context *mac;

	sap_debug("wlansap_start invoked successfully");

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->csa_reason = CSA_REASON_UNKNOWN;
	qdf_mem_copy(sap_ctx->self_mac_addr, addr, QDF_MAC_ADDR_SIZE);

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_INVAL;
	}

	sap_ctx->dfs_cac_offload = cac_offload;

	status = sap_set_session_param(MAC_HANDLE(mac), sap_ctx, session_id);
	if (QDF_STATUS_SUCCESS != status) {
		sap_err("Calling sap_set_session_param status = %d", status);
		return QDF_STATUS_E_FAILURE;
	}
	/* Register with scan component only during init */
	if (!reinit)
		sap_ctx->req_id =
			wlan_scan_register_requester(mac->psoc, "SAP",
					sap_scan_event_callback, sap_ctx);

	if (!reinit) {
		status = wlansap_owe_init(sap_ctx);
		if (QDF_STATUS_SUCCESS != status) {
			sap_err("OWE init failed");
			return QDF_STATUS_E_FAILURE;
		}
		status = wlansap_ft_init(sap_ctx);
		if (QDF_STATUS_SUCCESS != status) {
			sap_err("FT init failed");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sap_deinit_ctx(struct sap_context *sap_ctx)
{
	struct mac_context *mac;

	/* Sanity check - Extract SAP control block */
	sap_debug("wlansap_stop invoked successfully ");

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	wlansap_ft_cleanup(sap_ctx);
	wlansap_ft_deinit(sap_ctx);
	wlansap_owe_cleanup(sap_ctx);
	wlansap_owe_deinit(sap_ctx);
	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}
	wlan_scan_unregister_requester(mac->psoc, sap_ctx->req_id);

	if (sap_ctx->freq_list) {
		qdf_mem_free(sap_ctx->freq_list);
		sap_ctx->freq_list = NULL;
		sap_ctx->num_of_channel = 0;
	}

	if (sap_ctx->sessionId != WLAN_UMAC_VDEV_ID_MAX) {
		/* empty queues/lists/pkts if any */
		sap_clear_session_param(MAC_HANDLE(mac), sap_ctx,
					sap_ctx->sessionId);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sap_destroy_ctx(struct sap_context *sap_ctx)
{
	sap_debug("Enter");

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->user_context = NULL;

	/* Cleanup SAP control block */
	/*
	 * wlansap_context_put will release actual sap_ctx memory
	 * allocated during sap_create_ctx
	 */
	wlansap_context_put(sap_ctx);

	return QDF_STATUS_SUCCESS;
} /* sap_destroy_ctx */

bool wlansap_is_channel_in_nol_list(struct sap_context *sap_ctx,
				    qdf_freq_t chan_freq,
				    ePhyChanBondState chanBondState)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer from pCtx");
		return QDF_STATUS_E_FAULT;
	}

	return sap_dfs_is_channel_in_nol_list(sap_ctx, chan_freq,
					      chanBondState);
}

static QDF_STATUS wlansap_mark_leaking_channel(struct wlan_objmgr_pdev *pdev,
		uint16_t *leakage_adjusted_lst,
		uint8_t chan_bw)
{

	return utils_dfs_mark_leaking_chan_for_freq(pdev, chan_bw, 1,
						    leakage_adjusted_lst);
}

bool wlansap_is_channel_leaking_in_nol(struct sap_context *sap_ctx,
				       uint16_t chan_freq,
				       uint8_t chan_bw)
{
	struct mac_context *mac_ctx;
	uint16_t leakage_adjusted_lst[1];

	leakage_adjusted_lst[0] = chan_freq;
	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}
	if (QDF_IS_STATUS_ERROR(wlansap_mark_leaking_channel(mac_ctx->pdev,
			leakage_adjusted_lst, chan_bw)))
		return true;

	if (!leakage_adjusted_lst[0])
		return true;

	return false;
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
uint16_t wlansap_check_cc_intf(struct sap_context *sap_ctx)
{
	struct mac_context *mac;
	uint16_t intf_ch_freq;
	eCsrPhyMode phy_mode;
	uint8_t vdev_id;

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return 0;
	}
	phy_mode = sap_ctx->phyMode;
	vdev_id = sap_ctx->sessionId;
	intf_ch_freq = sme_check_concurrent_channel_overlap(
						MAC_HANDLE(mac),
						sap_ctx->chan_freq,
						phy_mode,
						sap_ctx->cc_switch_mode,
						vdev_id);
	return intf_ch_freq;
}
#endif

 /**
  * wlansap_set_scan_acs_channel_params() - Config scan and channel parameters.
  * config:                                Pointer to the SAP config
  * psap_ctx:                               Pointer to the SAP Context.
  *
  * This api function is used to copy Scan and Channel parameters from sap
  * config to sap context.
  *
  * Return:                                 The result code associated with
  *                                         performing the operation
  */
static QDF_STATUS
wlansap_set_scan_acs_channel_params(struct sap_config *config,
				    struct sap_context *psap_ctx)
{
	struct mac_context *mac;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t auto_channel_select_weight;
	bool is_linear_bss_count;
	bool is_linear_rssi;
	bool is_rand;
	bool is_chan_load;
	int16_t linear_rssi_threshold;
	bool early_terminate_en;

	if (!config) {
		sap_err("Invalid config passed ");
		return QDF_STATUS_E_FAULT;
	}

	if (!psap_ctx) {
		sap_err("Invalid config passed ");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_INVAL;
	}

	/* Channel selection is auto or configured */
	wlansap_set_acs_ch_freq(psap_ctx, config->chan_freq);
	psap_ctx->dfs_mode = config->acs_dfs_mode;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	psap_ctx->cc_switch_mode = config->cc_switch_mode;
#endif
	status = ucfg_mlme_get_auto_channel_weight(
					mac->psoc,
					&auto_channel_select_weight);

	if (!QDF_IS_STATUS_SUCCESS(status))
		sap_err("get_auto_channel_weight failed");

	psap_ctx->auto_channel_select_weight = auto_channel_select_weight;
	psap_ctx->enableOverLapCh = config->enOverLapCh;
	psap_ctx->acs_cfg = &config->acs_cfg;
	psap_ctx->ch_width_orig = config->acs_cfg.ch_width;
	psap_ctx->sec_ch_freq = config->sec_ch_freq;
	qdf_mem_copy(psap_ctx->self_mac_addr,
		config->self_macaddr.bytes, QDF_MAC_ADDR_SIZE);

	/* Updating ACS Configuration */
	status = ucfg_mlme_get_acs_linear_bss_status(mac->psoc,
						     &is_linear_bss_count);

	if (!QDF_IS_STATUS_SUCCESS(status))
		sap_err("get_acs_linear_bss_status failed");

	status = ucfg_mlme_get_acs_linear_rssi_status(mac->psoc,
						      &is_linear_rssi);

	if (!QDF_IS_STATUS_SUCCESS(status))
		sap_err("get_acs_linear_rssi_status failed");

	status = ucfg_mlme_get_acs_rssi_threshold_score(mac->psoc,
							&linear_rssi_threshold);

	if (!QDF_IS_STATUS_SUCCESS(status))
		sap_err("get_acs_rssi_threshold_score failed");

	status = ucfg_mlme_get_acs_same_chan_weight_rand_status(mac->psoc,
								&is_rand);

	if (!QDF_IS_STATUS_SUCCESS(status))
		sap_err("get_acs_same_chan_weight_rand_status failed");

	status = ucfg_mlme_get_acs_wifi_non_wifi_load_status(mac->psoc,
							     &is_chan_load);

	if (!QDF_IS_STATUS_SUCCESS(status))
		sap_err("get_acs_wifi_non_wifi_load_status failed");

	status = ucfg_mlme_get_acs_early_terminate_status(mac->psoc,
							  &early_terminate_en);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sap_err("get_acs_early_terminate_status failed");

	psap_ctx->acs_cfg->is_linear_bss_count = is_linear_bss_count;
	psap_ctx->acs_cfg->is_linear_rssi = is_linear_rssi;
	psap_ctx->acs_cfg->linear_rssi_threshold = linear_rssi_threshold;
	psap_ctx->acs_cfg->is_same_weight_rand_enabled = is_rand;
	psap_ctx->acs_cfg->is_wifi_non_wifi_load_score_enabled = is_chan_load;
	psap_ctx->acs_cfg->is_early_terminate_enabled = early_terminate_en;

	return status;
}

eCsrPhyMode wlan_sap_get_phymode(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer from ctx");
		return 0;
	}
	return sap_ctx->phyMode;
}

enum phy_ch_width wlan_sap_get_concurrent_bw(struct wlan_objmgr_pdev *pdev,
					     struct wlan_objmgr_psoc *psoc,
					     qdf_freq_t con_ch_freq,
					     enum phy_ch_width channel_width)
{
	enum hw_mode_bandwidth sta_ch_width;
	enum phy_ch_width sta_chan_width = CH_WIDTH_20MHZ;
	bool scc_sta_present, is_con_chan_dfs = false;
	bool is_con_sta_indoor = false;
	uint8_t sta_vdev_id;
	uint8_t sta_sap_scc_on_dfs_chnl;
	uint8_t sta_count = 0;
	bool is_hw_dbs_capable = false;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(con_ch_freq))
		return channel_width;

	if (wlan_reg_is_6ghz_chan_freq(con_ch_freq))
		return channel_width;

	/* sta_count is to check if there is STA present on any other
	 * channel freq irrespective of concurrent channel.
	 */
	sta_count = policy_mgr_mode_specific_connection_count(
							psoc,
							PM_STA_MODE,
							NULL);
	scc_sta_present = policy_mgr_is_sta_present_on_freq(psoc,
							    &sta_vdev_id,
							    con_ch_freq,
							    &sta_ch_width);
	if (scc_sta_present) {
		sta_chan_width = policy_mgr_get_ch_width(sta_ch_width);
		sap_debug("sta_chan_width:%d, channel_width:%d",
			  sta_chan_width, channel_width);
		if (wlan_reg_is_dfs_for_freq(pdev, con_ch_freq) ||
		    sta_chan_width == CH_WIDTH_160MHZ)
			is_con_chan_dfs = true;
		else if (WLAN_REG_IS_5GHZ_CH_FREQ(con_ch_freq) &&
			 wlan_reg_is_freq_indoor(pdev, con_ch_freq))
			is_con_sta_indoor = true;
	}

	policy_mgr_get_sta_sap_scc_on_dfs_chnl(psoc, &sta_sap_scc_on_dfs_chnl);
	is_hw_dbs_capable = policy_mgr_is_hw_dbs_capable(psoc);
	sap_debug("sta_sap_scc_on_dfs_chnl:%d, is_hw_dbs_capable:%d, sta_count:%d, scc_sta_present:%d",
		  sta_sap_scc_on_dfs_chnl,
		  is_hw_dbs_capable, sta_count, scc_sta_present);

	/*
	 * In indoor concurrency cases, limit the channel width with the STA
	 * interface bandwidth. Since, only the bonded channels are active
	 * channels.
	 */
	if (is_con_sta_indoor) {
		channel_width = QDF_MIN(sta_chan_width, channel_width);
		sap_debug("STA + SAP on indoor channels");
		return channel_width;
	} else if (!is_con_chan_dfs) {
		/* Handle "Active channel" concurrency/standalone SAP */
		sap_debug("STA + SAP/GO or standalone SAP on active channel");
		if (scc_sta_present)
			return  QDF_MAX(sta_chan_width, CH_WIDTH_80MHZ);
		else if (sta_count)
			return  QDF_MIN(channel_width, CH_WIDTH_80MHZ);
		return channel_width;
	}

	/* Handle "DBS/non-DBS + dfs channels" concurrency */
	if (is_con_chan_dfs) {
		switch (sta_sap_scc_on_dfs_chnl) {
		case PM_STA_SAP_ON_DFS_MASTER_MODE_FLEX:
			if (scc_sta_present) {
				sap_debug("STA+SAP/GO: limit the SAP channel width");
				return QDF_MIN(sta_chan_width, channel_width);
			}

			sap_debug("Standalone SAP/GO: set BW coming in start req");
			return channel_width;
		case PM_STA_SAP_ON_DFS_MASTER_MODE_DISABLED:
			if (scc_sta_present) {
				sap_debug("STA present: Limit the SAP channel width");
				channel_width = QDF_MIN(sta_chan_width,
							channel_width);
				return channel_width;
			}
			/*
			 * sta_sap_scc_on_dfs_chnl = 1, DFS master is disabled.
			 * If STA not present (SAP single), the SAP (160Mhz) is
			 * not allowed on DFS, so limit SAP to 80Mhz.
			 */
			sap_debug("Limit Standalone SAP/GO to 80Mhz");
			return QDF_MIN(channel_width, CH_WIDTH_80MHZ);
		case PM_STA_SAP_ON_DFS_DEFAULT:
		default:
			/*
			 * sta_sap_scc_on_dfs_chnl = 0, not allow STA+SAP SCC on DFS.
			 * Limit SAP to 80Mhz if STA present.
			 */
			if (sta_count) {
				sap_debug("STA present, Limit SAP/GO to 80Mhz");
				return QDF_MIN(channel_width, CH_WIDTH_80MHZ);
			}
			break;
		}
	}

	sap_debug("Single SAP/GO: set BW coming in SAP/GO start req");
	return channel_width;

}

uint32_t wlan_sap_get_vht_ch_width(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return 0;
	}

	return sap_ctx->ch_params.ch_width;
}

bool wlan_sap_get_ch_params(struct sap_context *sap_ctx,
			    struct ch_params *ch_params)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return false;
	}

	*ch_params = sap_ctx->ch_params;
	return true;
}

/**
 * wlan_sap_validate_channel_switch() - validate target channel switch w.r.t
 *      concurreny rules set to avoid channel interference.
 * @mac_handle: Opaque handle to the global MAC context
 * @sap_ch_freq: channel to switch
 * @sap_context: sap session context
 *
 * Return: true if there is no channel interference else return false
 */
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
static bool wlan_sap_validate_channel_switch(mac_handle_t mac_handle,
					     uint32_t sap_ch_freq,
					     struct sap_context *sap_context)
{
	return sme_validate_sap_channel_switch(
			mac_handle,
			sap_ch_freq,
			sap_context->phyMode,
			sap_context->cc_switch_mode,
			sap_context->sessionId);
}
#else
static bool wlan_sap_validate_channel_switch(mac_handle_t mac_handle,
					     uint32_t sap_ch_freq,
					     struct sap_context *sap_context)
{
	return true;
}
#endif

void wlan_sap_set_sap_ctx_acs_cfg(struct sap_context *sap_ctx,
				  struct sap_config *sap_config)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return;
	}

	sap_ctx->acs_cfg = &sap_config->acs_cfg;
}

QDF_STATUS wlansap_start_bss(struct sap_context *sap_ctx,
			     sap_event_cb sap_event_cb,
			     struct sap_config *config)
{
	struct sap_sm_event sap_event;        /* State machine event */
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	uint32_t auto_channel_select_weight =
			cfg_default(CFG_AUTO_CHANNEL_SELECT_WEIGHT);
	int reduced_beacon_interval;
	struct mac_context *pmac = NULL;
	int sap_chanswitch_beacon_cnt;
	bool sap_chanswitch_mode;

	if (!sap_ctx) {
		sap_info("Invalid SAP context");
		return QDF_STATUS_E_FAULT;
	}

	pmac = sap_get_mac_context();
	if (!pmac) {
		sap_err("Invalid sap MAC context");
		qdf_status = QDF_STATUS_E_INVAL;
		goto fail;
	}

	sap_ctx->fsm_state = SAP_INIT;
	sap_debug("sap_fsm: vdev %d:  => SAP_INIT", sap_ctx->vdev_id);

	qdf_status = wlan_set_vdev_crypto_prarams_from_ie(
			sap_ctx->vdev,
			config->RSNWPAReqIE,
			config->RSNWPAReqIELength);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		sap_debug("Failed to set crypto params from IE");

	/* Channel selection is auto or configured */
	sap_ctx->chan_freq = config->chan_freq;
	sap_ctx->dfs_mode = config->acs_dfs_mode;
	sap_ctx->ch_params = config->ch_params;
	sap_ctx->ch_width_orig = config->ch_width_orig;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	sap_ctx->cc_switch_mode = config->cc_switch_mode;
#endif

	qdf_status = ucfg_mlme_get_auto_channel_weight(
					pmac->psoc,
					&auto_channel_select_weight);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		sap_err("get_auto_channel_weight failed");

	sap_ctx->auto_channel_select_weight = auto_channel_select_weight;

	sap_ctx->enableOverLapCh = config->enOverLapCh;
	sap_ctx->acs_cfg = &config->acs_cfg;
	sap_ctx->sec_ch_freq = config->sec_ch_freq;
	sap_ctx->isCacStartNotified = false;
	sap_ctx->isCacEndNotified = false;
	sap_ctx->is_chan_change_inprogress = false;
	sap_ctx->disabled_mcs13 = false;
	sap_ctx->phyMode = config->SapHw_mode;
	sap_ctx->csa_reason = CSA_REASON_UNKNOWN;
	sap_ctx->require_h2e = config->require_h2e;
	qdf_mem_copy(sap_ctx->bssid.bytes, config->self_macaddr.bytes,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(sap_ctx->self_mac_addr,
		     config->self_macaddr.bytes, QDF_MAC_ADDR_SIZE);
	/*
	 * Set the DFS Test Mode setting
	 * Set beacon channel count before channel switch
	 */
	qdf_status = ucfg_mlme_get_sap_chn_switch_bcn_count(
						pmac->psoc,
						&sap_chanswitch_beacon_cnt);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		sap_err("ucfg_mlme_get_sap_chn_switch_bcn_count fail, set def");

	pmac->sap.SapDfsInfo.sap_ch_switch_beacon_cnt =
				sap_chanswitch_beacon_cnt;
	pmac->sap.SapDfsInfo.sap_ch_switch_mode =
				sap_chanswitch_beacon_cnt;

	qdf_status = ucfg_mlme_get_sap_channel_switch_mode(
						pmac->psoc,
						&sap_chanswitch_mode);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		sap_err("ucfg_mlme_get_sap_channel_switch_mode, set def");

	pmac->sap.SapDfsInfo.sap_ch_switch_mode = sap_chanswitch_mode;
	pmac->sap.sapCtxList[sap_ctx->sessionId].sap_context = sap_ctx;
	pmac->sap.sapCtxList[sap_ctx->sessionId].sapPersona =
							config->persona;

	qdf_status = ucfg_mlme_get_sap_reduces_beacon_interval(
						pmac->psoc,
						&reduced_beacon_interval);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		sap_err("ucfg_mlme_get_sap_reduces_beacon_interval fail");

	pmac->sap.SapDfsInfo.reduced_beacon_interval =
					reduced_beacon_interval;
	sap_debug("SAP: auth ch select weight:%d chswitch bcn cnt:%d chswitch mode:%d reduced bcn intv:%d",
		  sap_ctx->auto_channel_select_weight,
		  sap_chanswitch_beacon_cnt,
		  pmac->sap.SapDfsInfo.sap_ch_switch_mode,
		  pmac->sap.SapDfsInfo.reduced_beacon_interval);

	/* Copy MAC filtering settings to sap context */
	sap_ctx->eSapMacAddrAclMode = config->SapMacaddr_acl;
	qdf_mem_copy(sap_ctx->acceptMacList, config->accept_mac,
		     sizeof(config->accept_mac));
	sap_ctx->nAcceptMac = config->num_accept_mac;
	sap_sort_mac_list(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	qdf_mem_copy(sap_ctx->denyMacList, config->deny_mac,
		     sizeof(config->deny_mac));
	sap_ctx->nDenyMac = config->num_deny_mac;
	sap_sort_mac_list(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	sap_ctx->beacon_tx_rate = config->beacon_tx_rate;

	/* Fill in the event structure for FSM */
	sap_event.event = eSAP_HDD_START_INFRA_BSS;
	sap_event.params = 0;    /* pSapPhysLinkCreate */

	/* Store the HDD callback in SAP context */
	sap_ctx->sap_event_cb = sap_event_cb;

	sap_ctx->sap_bss_cfg.vdev_id = sap_ctx->sessionId;
	sap_build_start_bss_config(&sap_ctx->sap_bss_cfg, config);
	/* Handle event */
	qdf_status = sap_fsm(sap_ctx, &sap_event);
fail:
	if (QDF_IS_STATUS_ERROR(qdf_status))
		qdf_mem_zero(&sap_ctx->sap_bss_cfg,
			     sizeof(sap_ctx->sap_bss_cfg));
	return qdf_status;
} /* wlansap_start_bss */

QDF_STATUS wlansap_set_mac_acl(struct sap_context *sap_ctx,
			       struct sap_config *config)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	sap_debug("wlansap_set_mac_acl");

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}
	/* Copy MAC filtering settings to sap context */
	sap_ctx->eSapMacAddrAclMode = config->SapMacaddr_acl;

	if (eSAP_DENY_UNLESS_ACCEPTED == sap_ctx->eSapMacAddrAclMode) {
		qdf_mem_copy(sap_ctx->acceptMacList,
			     config->accept_mac,
			     sizeof(config->accept_mac));
		sap_ctx->nAcceptMac = config->num_accept_mac;
		sap_sort_mac_list(sap_ctx->acceptMacList,
			       sap_ctx->nAcceptMac);
	} else if (eSAP_ACCEPT_UNLESS_DENIED == sap_ctx->eSapMacAddrAclMode) {
		qdf_mem_copy(sap_ctx->denyMacList, config->deny_mac,
			     sizeof(config->deny_mac));
		sap_ctx->nDenyMac = config->num_deny_mac;
		sap_sort_mac_list(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	}

	return qdf_status;
} /* wlansap_set_mac_acl */

QDF_STATUS wlansap_stop_bss(struct sap_context *sap_ctx)
{
	struct sap_sm_event sap_event;        /* State machine event */
	QDF_STATUS qdf_status;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	/* Fill in the event structure for FSM */
	sap_event.event = eSAP_HDD_STOP_INFRA_BSS;
	sap_event.params = 0;

	/* Handle event */
	qdf_status = sap_fsm(sap_ctx, &sap_event);

	return qdf_status;
}

/* This routine will set the mode of operation for ACL dynamically*/
QDF_STATUS wlansap_set_acl_mode(struct sap_context *sap_ctx,
				eSapMacAddrACL mode)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->eSapMacAddrAclMode = mode;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_acl_mode(struct sap_context *sap_ctx,
				eSapMacAddrACL *mode)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	*mode = sap_ctx->eSapMacAddrAclMode;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_acl_accept_list(struct sap_context *sap_ctx,
				       struct qdf_mac_addr *pAcceptList,
				       uint16_t *nAcceptList)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	memcpy(pAcceptList, sap_ctx->acceptMacList,
	       (sap_ctx->nAcceptMac * QDF_MAC_ADDR_SIZE));
	*nAcceptList = sap_ctx->nAcceptMac;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_acl_deny_list(struct sap_context *sap_ctx,
				     struct qdf_mac_addr *pDenyList,
				     uint16_t *nDenyList)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer from p_cds_gctx");
		return QDF_STATUS_E_FAULT;
	}

	memcpy(pDenyList, sap_ctx->denyMacList,
	       (sap_ctx->nDenyMac * QDF_MAC_ADDR_SIZE));
	*nDenyList = sap_ctx->nDenyMac;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_clear_acl(struct sap_context *sap_ctx)
{
	uint16_t i;

	if (!sap_ctx) {
		return QDF_STATUS_E_RESOURCES;
	}

	for (i = 0; i < sap_ctx->nDenyMac; i++) {
		qdf_mem_zero((sap_ctx->denyMacList + i)->bytes,
			     QDF_MAC_ADDR_SIZE);
	}

	sap_print_acl(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	sap_ctx->nDenyMac = 0;

	for (i = 0; i < sap_ctx->nAcceptMac; i++) {
		qdf_mem_zero((sap_ctx->acceptMacList + i)->bytes,
			     QDF_MAC_ADDR_SIZE);
	}

	sap_print_acl(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	sap_ctx->nAcceptMac = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_modify_acl(struct sap_context *sap_ctx,
			      uint8_t *peer_sta_mac,
			      eSapACLType list_type, eSapACLCmdType cmd)
{
	bool sta_allow_list = false, sta_deny_list = false;
	uint16_t staWLIndex, staBLIndex;

	if (!sap_ctx) {
		sap_err("Invalid SAP Context");
		return QDF_STATUS_E_FAULT;
	}
	if (qdf_mem_cmp(sap_ctx->bssid.bytes, peer_sta_mac,
			QDF_MAC_ADDR_SIZE) == 0) {
		sap_err("requested peer mac is "QDF_MAC_ADDR_FMT
			"our own SAP BSSID. Do not denylist or allowlist this BSSID",
			QDF_MAC_ADDR_REF(peer_sta_mac));
		return QDF_STATUS_E_FAULT;
	}
	sap_debug("Modify ACL entered\n" "Before modification of ACL\n"
		  "size of accept and deny lists %d %d", sap_ctx->nAcceptMac,
		  sap_ctx->nDenyMac);
	sap_debug("*** ALLOW LIST ***");
	sap_print_acl(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	sap_debug("*** DENY LIST ***");
	sap_print_acl(sap_ctx->denyMacList, sap_ctx->nDenyMac);

	/* the expectation is a mac addr will not be in both the lists
	 * at the same time. It is the responsibility of userspace to
	 * ensure this
	 */
	sta_allow_list =
		sap_search_mac_list(sap_ctx->acceptMacList, sap_ctx->nAcceptMac,
				 peer_sta_mac, &staWLIndex);
	sta_deny_list =
		sap_search_mac_list(sap_ctx->denyMacList, sap_ctx->nDenyMac,
				 peer_sta_mac, &staBLIndex);

	if (sta_allow_list && sta_deny_list) {
		sap_err("Peer mac " QDF_MAC_ADDR_FMT
			" found in allow and deny lists."
			"Initial lists passed incorrect. Cannot execute this command.",
			QDF_MAC_ADDR_REF(peer_sta_mac));
		return QDF_STATUS_E_FAILURE;

	}
	sap_debug("cmd %d", cmd);

	switch (list_type) {
	case SAP_ALLOW_LIST:
		if (cmd == ADD_STA_TO_ACL || cmd == ADD_STA_TO_ACL_NO_DEAUTH) {
			/* error check */
			/* if list is already at max, return failure */
			if (sap_ctx->nAcceptMac == MAX_ACL_MAC_ADDRESS) {
				sap_err("Allow list is already maxed out. Cannot accept "
					  QDF_MAC_ADDR_FMT,
					  QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
			if (sta_allow_list) {
				/*
				 * Do nothing if already present in allow
				 * list. Just print a warning
				 */
				sap_warn("MAC address already present in allow list "
					 QDF_MAC_ADDR_FMT,
					 QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_SUCCESS;
			}
			if (sta_deny_list) {
				/*
				 * remove it from deny list before adding
				 * to the allow list
				 */
				sap_warn("STA present in deny list so first remove from it");
				sap_remove_mac_from_acl(sap_ctx->denyMacList,
						    &sap_ctx->nDenyMac,
						    staBLIndex);
			}
			sap_debug("... Now add to the allow list");
			sap_add_mac_to_acl(sap_ctx->acceptMacList,
					       &sap_ctx->nAcceptMac,
			       peer_sta_mac);
				sap_debug("size of accept and deny lists %d %d",
					  sap_ctx->nAcceptMac,
					  sap_ctx->nDenyMac);
		} else if (cmd == DELETE_STA_FROM_ACL ||
			   cmd == DELETE_STA_FROM_ACL_NO_DEAUTH) {
			if (sta_allow_list) {

				struct csr_del_sta_params delStaParams;

				sap_info("Delete from allow list");
				sap_remove_mac_from_acl(sap_ctx->acceptMacList,
						    &sap_ctx->nAcceptMac,
						    staWLIndex);
				/* If a client is deleted from allow list and */
				/* it is connected, send deauth
				 */
				if (cmd == DELETE_STA_FROM_ACL) {
					wlansap_populate_del_sta_params(
						peer_sta_mac,
						eCsrForcedDeauthSta,
						SIR_MAC_MGMT_DEAUTH,
						&delStaParams);
					wlansap_deauth_sta(sap_ctx,
							   &delStaParams);
					sap_debug("size of accept and deny lists %d %d",
						  sap_ctx->nAcceptMac,
						  sap_ctx->nDenyMac);
				}
			} else {
				sap_warn("MAC address to be deleted is not present in the allow list "
					 QDF_MAC_ADDR_FMT,
					 QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			sap_err("Invalid cmd type passed");
			return QDF_STATUS_E_FAILURE;
		}
		break;

	case SAP_DENY_LIST:

		if (cmd == ADD_STA_TO_ACL || cmd == ADD_STA_TO_ACL_NO_DEAUTH) {
			struct csr_del_sta_params delStaParams;
			/* error check */
			/* if list is already at max, return failure */
			if (sap_ctx->nDenyMac == MAX_ACL_MAC_ADDRESS) {
				sap_err("Deny list is already maxed out. Cannot accept "
					QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
			if (sta_deny_list) {
				/*
				 * Do nothing if already present in
				 * allow list
				 */
				sap_warn("MAC address already present in deny list "
					 QDF_MAC_ADDR_FMT,
					 QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_SUCCESS;
			}
			if (sta_allow_list) {
				/*
				 * remove it from allow list before adding to
				 * the deny list
				 */
				sap_warn("Present in allow list so first remove from it");
				sap_remove_mac_from_acl(sap_ctx->acceptMacList,
						    &sap_ctx->nAcceptMac,
						    staWLIndex);
			}
			/* If we are adding a client to the deny list; */
			/* if its connected, send deauth
			 */
			if (cmd == ADD_STA_TO_ACL) {
				wlansap_populate_del_sta_params(
					peer_sta_mac,
					eCsrForcedDeauthSta,
					SIR_MAC_MGMT_DEAUTH,
					&delStaParams);
				wlansap_deauth_sta(sap_ctx, &delStaParams);
			}
			sap_info("... Now add to deny list");
			sap_add_mac_to_acl(sap_ctx->denyMacList,
				       &sap_ctx->nDenyMac, peer_sta_mac);
			sap_debug("size of accept and deny lists %d %d",
				  sap_ctx->nAcceptMac,
				  sap_ctx->nDenyMac);
		} else if (cmd == DELETE_STA_FROM_ACL ||
			   cmd == DELETE_STA_FROM_ACL_NO_DEAUTH) {
			if (sta_deny_list) {
				sap_info("Delete from deny list");
				sap_remove_mac_from_acl(sap_ctx->denyMacList,
						    &sap_ctx->nDenyMac,
						    staBLIndex);
				sap_debug("no accept and deny mac %d %d",
					  sap_ctx->nAcceptMac,
					  sap_ctx->nDenyMac);
			} else {
				sap_warn("MAC address to be deleted is not present in the deny list "
					  QDF_MAC_ADDR_FMT,
					  QDF_MAC_ADDR_REF(peer_sta_mac));
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			sap_err("Invalid cmd type passed");
			return QDF_STATUS_E_FAILURE;
		}
		break;

	default:
	{
		sap_err("Invalid list type passed %d", list_type);
		return QDF_STATUS_E_FAILURE;
	}
	}
	sap_debug("After modification of ACL");
	sap_debug("*** ALLOW LIST ***");
	sap_print_acl(sap_ctx->acceptMacList, sap_ctx->nAcceptMac);
	sap_debug("*** DENY LIST ***");
	sap_print_acl(sap_ctx->denyMacList, sap_ctx->nDenyMac);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_disassoc_sta(struct sap_context *sap_ctx,
				struct csr_del_sta_params *params)
{
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	return sme_roam_disconnect_sta(MAC_HANDLE(mac), sap_ctx->sessionId,
				       params);
}

QDF_STATUS wlansap_deauth_sta(struct sap_context *sap_ctx,
			      struct csr_del_sta_params *params)
{
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	return sme_roam_deauth_sta(MAC_HANDLE(mac), sap_ctx->sessionId,
				   params);
}

#if defined(WLAN_FEATURE_11BE)
static enum phy_ch_width
wlansap_get_target_eht_phy_ch_width(void)
{
	uint32_t max_fw_bw = sme_get_eht_ch_width();

	if (max_fw_bw == WNI_CFG_EHT_CHANNEL_WIDTH_320MHZ)
		return CH_WIDTH_320MHZ;
	else if (max_fw_bw == WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
		return CH_WIDTH_160MHZ;
	else
		return CH_WIDTH_80MHZ;
}
#else /* !WLAN_FEATURE_11BE */
static enum phy_ch_width
wlansap_get_target_eht_phy_ch_width(void)
{
	return CH_WIDTH_20MHZ;
}
#endif /* WLAN_FEATURE_11BE */

static enum phy_ch_width
wlansap_5g_original_bw_validate(
	struct sap_context *sap_context,
	uint32_t chan_freq,
	enum phy_ch_width ch_width)
{
	if (sap_context->csa_reason != CSA_REASON_USER_INITIATED &&
	    sap_context->csa_reason != CSA_REASON_PRE_CAC_SUCCESS &&
	    WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq) &&
	    ch_width >= CH_WIDTH_160MHZ &&
	    sap_context->ch_width_orig < CH_WIDTH_160MHZ)
		ch_width = CH_WIDTH_80MHZ;

	return ch_width;
}

/**
 * wlansap_2g_original_bw_validate() - validate bw for sap on 2.4 GHz
 * @sap_context: sap context
 * @chan_freq: channel frequency
 * @ch_width: band width
 * @sec_ch_freq: secondary channel frequency
 *
 * If initial SAP starts on 2.4 GHz HT40/HT20 mode, driver honors it.
 *
 * Return: new bandwidth
 */
static enum phy_ch_width
wlansap_2g_original_bw_validate(struct sap_context *sap_context,
				uint32_t chan_freq,
				enum phy_ch_width ch_width,
				qdf_freq_t *sec_ch_freq)
{
	if (sap_context->csa_reason == CSA_REASON_UNKNOWN &&
	    WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq) &&
	    sap_context->ch_width_orig == CH_WIDTH_40MHZ) {
		ch_width = CH_WIDTH_40MHZ;
		if (sap_context->ch_params.sec_ch_offset == LOW_PRIMARY_CH)
			*sec_ch_freq = chan_freq + 20;
		else if (sap_context->ch_params.sec_ch_offset ==
						HIGH_PRIMARY_CH)
			*sec_ch_freq = chan_freq - 20;
		else
			*sec_ch_freq = 0;
	}

	return ch_width;
}

enum phy_ch_width
wlansap_get_csa_chanwidth_from_phymode(struct sap_context *sap_context,
				       uint32_t chan_freq,
				       struct ch_params *tgt_ch_params)
{
	enum phy_ch_width ch_width, concurrent_bw = 0;
	struct mac_context *mac;
	struct ch_params ch_params = {0};
	uint32_t channel_bonding_mode = 0;
	qdf_freq_t sec_ch_freq = 0;

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return CH_WIDTH_20MHZ;
	}

	if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq)) {
		/*
		 * currently OBSS scan is done in hostapd, so to avoid
		 * SAP coming up in HT40 on channel switch we are
		 * disabling channel bonding in 2.4Ghz.
		 */
		ch_width = wlansap_2g_original_bw_validate(
				sap_context, chan_freq, CH_WIDTH_20MHZ,
				&sec_ch_freq);
	} else {
		wlan_mlme_get_channel_bonding_5ghz(mac->psoc,
						   &channel_bonding_mode);
		if (policy_mgr_is_vdev_ll_lt_sap(mac->psoc,
						 sap_context->vdev_id) ||
		    (WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq) &&
		     !channel_bonding_mode))
			ch_width = CH_WIDTH_20MHZ;
		else
			ch_width = wlansap_get_max_bw_by_phymode(sap_context);

		ch_width = wlansap_5g_original_bw_validate(
				sap_context, chan_freq, ch_width);
		concurrent_bw = wlan_sap_get_concurrent_bw(
				mac->pdev, mac->psoc, chan_freq,
				ch_width);
		ch_width = QDF_MIN(ch_width, concurrent_bw);
		if (tgt_ch_params)
			ch_width = QDF_MIN(ch_width, tgt_ch_params->ch_width);

		if (ch_width == CH_WIDTH_320MHZ &&
		    policy_mgr_is_conn_lead_to_dbs_sbs(mac->psoc,
						       sap_context->vdev_id,
						       chan_freq))
			ch_width = wlan_mlme_get_ap_oper_ch_width(
							sap_context->vdev);
	}
	ch_params.ch_width = ch_width;
	if (sap_phymode_is_eht(sap_context->phyMode))
		wlan_reg_set_create_punc_bitmap(&ch_params, true);
	wlan_reg_set_channel_params_for_pwrmode(mac->pdev, chan_freq,
						sec_ch_freq, &ch_params,
						REG_CURRENT_PWR_MODE);
	ch_width = ch_params.ch_width;
	if (tgt_ch_params)
		*tgt_ch_params = ch_params;
	sap_nofl_debug("csa freq %d bw %d (phymode %d con bw %d tgt bw %d orig %d reason %d) channel bonding 5g %d",
		       chan_freq, ch_width,
		       sap_context->phyMode,
		       concurrent_bw,
		       tgt_ch_params ? tgt_ch_params->ch_width : CH_WIDTH_MAX,
		       sap_context->ch_width_orig,
		       sap_context->csa_reason,
		       channel_bonding_mode);

	return ch_width;
}

/**
 * sap_start_csa_restart() - send csa start event
 * @mac: mac ctx
 * @sap_ctx: SAP context
 *
 * Return: QDF_STATUS
 */
static inline void sap_start_csa_restart(struct mac_context *mac,
					 struct sap_context *sap_ctx)
{
	sme_csa_restart(mac, sap_ctx->sessionId);
}

/**
 * sap_get_csa_reason_str() - Get csa reason in string
 * @reason: sap reason enum value
 *
 * Return: string reason
 */
const char *sap_get_csa_reason_str(enum sap_csa_reason_code reason)
{
	switch (reason) {
	case CSA_REASON_UNKNOWN:
		return "UNKNOWN";
	case CSA_REASON_STA_CONNECT_DFS_TO_NON_DFS:
		return "STA_CONNECT_DFS_TO_NON_DFS";
	case CSA_REASON_USER_INITIATED:
		return "USER_INITIATED";
	case CSA_REASON_PEER_ACTION_FRAME:
		return "PEER_ACTION_FRAME";
	case CSA_REASON_PRE_CAC_SUCCESS:
		return "PRE_CAC_SUCCESS";
	case CSA_REASON_CONCURRENT_STA_CHANGED_CHANNEL:
		return "CONCURRENT_STA_CHANGED_CHANNEL";
	case CSA_REASON_UNSAFE_CHANNEL:
		return "UNSAFE_CHANNEL";
	case CSA_REASON_LTE_COEX:
		return "LTE_COEX";
	case CSA_REASON_CONCURRENT_NAN_EVENT:
		return "CONCURRENT_NAN_EVENT";
	case CSA_REASON_BAND_RESTRICTED:
		return "BAND_RESTRICTED";
	case CSA_REASON_DCS:
		return "DCS";
	case CSA_REASON_CHAN_DISABLED:
		return "DISABLED";
	case CSA_REASON_CHAN_PASSIVE:
		return "PASSIVE";
	case CSA_REASON_GO_BSS_STARTED:
		return "GO_BSS_STARTED";
	case CSA_REASON_SAP_ACS:
		return "CSA_REASON_SAP_ACS";
	case CSA_REASON_SAP_FIX_CH_CONC_WITH_GO:
		return "SAP_FIX_CH_CONC_WITH_GO";
	case CSA_REASON_LL_LT_SAP_EVENT:
		return "LL_LT_SAP_EVENT";
	default:
		return "UNKNOWN";
	}
}

/**
 * wlansap_set_chan_params_for_csa() - Update sap channel parameters
 *    for channel switch
 * @mac: mac ctx
 * @sap_ctx: sap context
 * @target_chan_freq: target channel frequency in MHz
 * @target_bw: target bandwidth
 *
 * Return: QDF_STATUS_SUCCESS for success.
 */
static QDF_STATUS
wlansap_set_chan_params_for_csa(struct mac_context *mac,
				struct sap_context *sap_ctx,
				uint32_t target_chan_freq,
				enum phy_ch_width target_bw)
{
	struct ch_params tmp_ch_params = {0};

	tmp_ch_params.ch_width = target_bw;
	mac->sap.SapDfsInfo.new_chanWidth =
		wlansap_get_csa_chanwidth_from_phymode(sap_ctx,
						       target_chan_freq,
						       &tmp_ch_params);
	/*
	 * Copy the requested target channel
	 * to sap context.
	 */
	mac->sap.SapDfsInfo.target_chan_freq = target_chan_freq;
	mac->sap.SapDfsInfo.new_ch_params.ch_width =
		mac->sap.SapDfsInfo.new_chanWidth;

	/* By this time, the best bandwidth is calculated for
	 * the given target channel. Now, if there was a
	 * request from user to move to a selected bandwidth,
	 * we can see if it can be honored.
	 *
	 * Ex1: BW80 was selected for the target channel and
	 * user wants BW40, it can be allowed
	 * Ex2: BW40 was selected for the target channel and
	 * user wants BW80, it cannot be allowed for the given
	 * target channel.
	 *
	 * So, the MIN of the selected channel bandwidth and
	 * user input is used for the bandwidth
	 */
	if (target_bw != CH_WIDTH_MAX) {
		sap_nofl_debug("SAP CSA: target bw:%d new width:%d",
			       target_bw,
			       mac->sap.SapDfsInfo.new_ch_params.ch_width);
		mac->sap.SapDfsInfo.new_ch_params.ch_width =
			mac->sap.SapDfsInfo.new_chanWidth =
			QDF_MIN(mac->sap.SapDfsInfo.new_ch_params.ch_width,
				target_bw);
	}
	if (sap_phymode_is_eht(sap_ctx->phyMode))
		wlan_reg_set_create_punc_bitmap(&sap_ctx->ch_params, true);
	wlan_reg_set_channel_params_for_pwrmode(
		mac->pdev, target_chan_freq, 0,
		&mac->sap.SapDfsInfo.new_ch_params,
		REG_CURRENT_PWR_MODE);

	return QDF_STATUS_SUCCESS;
}

bool
wlansap_override_csa_strict_for_sap(mac_handle_t mac_handle,
				    struct sap_context *sap_ctx,
				    uint32_t target_chan_freq,
				    bool strict)
{
	uint8_t existing_vdev_id = WLAN_UMAC_VDEV_ID_MAX;
	enum policy_mgr_con_mode existing_vdev_mode = PM_MAX_NUM_OF_MODE;
	uint32_t con_freq;
	enum phy_ch_width ch_width;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	if (!mac_ctx || !sap_ctx->vdev ||
	    wlan_vdev_mlme_get_opmode(sap_ctx->vdev) != QDF_SAP_MODE)
		return strict;

	if (sap_ctx->csa_reason != CSA_REASON_USER_INITIATED)
		return strict;

	if (!policy_mgr_is_force_scc(mac_ctx->psoc))
		return strict;

	existing_vdev_id =
		policy_mgr_fetch_existing_con_info(
				mac_ctx->psoc,
				sap_ctx->sessionId,
				target_chan_freq,
				&existing_vdev_mode,
				&con_freq, &ch_width);
	if (existing_vdev_id < WLAN_UMAC_VDEV_ID_MAX &&
	    (existing_vdev_mode == PM_STA_MODE ||
	     existing_vdev_mode == PM_P2P_CLIENT_MODE))
		return strict;

	return true;
}

QDF_STATUS wlansap_set_channel_change_with_csa(struct sap_context *sap_ctx,
					       uint32_t target_chan_freq,
					       enum phy_ch_width target_bw,
					       bool strict)
{
	struct mac_context *mac;
	mac_handle_t mac_handle;
	bool valid;
	QDF_STATUS status, hw_mode_status;
	bool sta_sap_scc_on_dfs_chan;
	bool is_dfs;
	struct ch_params tmp_ch_params = {0};
	enum channel_state state;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");

		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}
	mac_handle = MAC_HANDLE(mac);

	if (((sap_ctx->acs_cfg && sap_ctx->acs_cfg->acs_mode) ||
	     policy_mgr_restrict_sap_on_unsafe_chan(mac->psoc) ||
	     sap_ctx->csa_reason != CSA_REASON_USER_INITIATED) &&
	    !policy_mgr_is_sap_freq_allowed(mac->psoc,
			wlan_vdev_mlme_get_opmode(sap_ctx->vdev),
			target_chan_freq)) {
		sap_err("%u is unsafe channel freq", target_chan_freq);
		return QDF_STATUS_E_FAULT;
	}
	sap_nofl_debug("SAP CSA: %d BW %d ---> %d BW %d conn on 5GHz:%d, csa_reason:%s(%d) strict %d vdev %d",
		       sap_ctx->chan_freq, sap_ctx->ch_params.ch_width,
		       target_chan_freq, target_bw,
		       policy_mgr_is_any_mode_active_on_band_along_with_session(
		       mac->psoc, sap_ctx->sessionId, POLICY_MGR_BAND_5),
		       sap_get_csa_reason_str(sap_ctx->csa_reason),
		       sap_ctx->csa_reason, strict, sap_ctx->sessionId);

	state = wlan_reg_get_channel_state_for_pwrmode(mac->pdev,
						       target_chan_freq,
						       REG_CURRENT_PWR_MODE);
	if (state == CHANNEL_STATE_DISABLE || state == CHANNEL_STATE_INVALID) {
		sap_nofl_debug("invalid target freq %d state %d",
			       target_chan_freq, state);
		return QDF_STATUS_E_INVAL;
	}

	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(mac->psoc);

	tmp_ch_params.ch_width = target_bw;
	wlansap_get_csa_chanwidth_from_phymode(sap_ctx,
					       target_chan_freq,
					       &tmp_ch_params);
	if (target_bw != CH_WIDTH_MAX) {
		tmp_ch_params.ch_width =
			QDF_MIN(tmp_ch_params.ch_width, target_bw);
		sap_nofl_debug("target ch_width %d to %d ", target_bw,
			       tmp_ch_params.ch_width);
	}

	if (sap_phymode_is_eht(sap_ctx->phyMode))
		wlan_reg_set_create_punc_bitmap(&tmp_ch_params, true);
	wlan_reg_set_channel_params_for_pwrmode(mac->pdev, target_chan_freq, 0,
						&tmp_ch_params,
						REG_CURRENT_PWR_MODE);
	if (sap_ctx->chan_freq == target_chan_freq &&
	    sap_ctx->ch_params.ch_width == tmp_ch_params.ch_width) {
		sap_nofl_debug("target freq and bw %d not changed",
			       tmp_ch_params.ch_width);
		return QDF_STATUS_E_FAULT;
	}
	is_dfs = wlan_mlme_check_chan_param_has_dfs(
			mac->pdev, &tmp_ch_params,
			target_chan_freq);
	/*
	 * Now, validate if the passed channel is valid in the
	 * current regulatory domain.
	 */
	if (!is_dfs ||
	    (!policy_mgr_is_any_mode_active_on_band_along_with_session(
			mac->psoc, sap_ctx->sessionId,
			POLICY_MGR_BAND_5) ||
			sta_sap_scc_on_dfs_chan ||
			sap_ctx->csa_reason == CSA_REASON_DCS)) {
		/*
		 * validate target channel switch w.r.t various concurrency
		 * rules set.
		 */
		if (!strict) {
			valid = wlan_sap_validate_channel_switch(mac_handle,
								 target_chan_freq,
								 sap_ctx);
			if (!valid) {
				sap_err("Channel freq switch to %u is not allowed due to concurrent channel interference",
					target_chan_freq);
				return QDF_STATUS_E_FAULT;
			}
		}
		/*
		 * Post a CSA IE request to SAP state machine with
		 * target channel information and also CSA IE required
		 * flag set in sap_ctx only, if SAP is in SAP_STARTED
		 * state.
		 */
		if (sap_ctx->fsm_state == SAP_STARTED) {
			status = wlansap_set_chan_params_for_csa(
					mac, sap_ctx, target_chan_freq,
					target_bw);
			if (QDF_IS_STATUS_ERROR(status))
				return status;

			hw_mode_status =
			  policy_mgr_check_and_set_hw_mode_for_channel_switch(
				   mac->psoc, sap_ctx->sessionId,
				   target_chan_freq,
				   POLICY_MGR_UPDATE_REASON_CHANNEL_SWITCH_SAP);

			/*
			 * If hw_mode_status is QDF_STATUS_E_FAILURE, mean HW
			 * mode change was required but driver failed to set HW
			 * mode so ignore CSA for the channel.
			 */
			if (hw_mode_status == QDF_STATUS_E_FAILURE) {
				sap_err("HW change required but failed to set hw mode");
				return hw_mode_status;
			}

			status = policy_mgr_reset_chan_switch_complete_evt(
								mac->psoc);
			if (QDF_IS_STATUS_ERROR(status)) {
				policy_mgr_check_n_start_opportunistic_timer(
								mac->psoc);
				return status;
			}

			/*
			 * Set the CSA IE required flag.
			 */
			mac->sap.SapDfsInfo.csaIERequired = true;

			/*
			 * Set the radar found status to allow the channel
			 * change to happen same as in the case of a radar
			 * detection. Since, this will allow SAP to be in
			 * correct state and also resume the netif queues
			 * that were suspended in HDD before the channel
			 * request was issued.
			 */
			sap_ctx->sap_radar_found_status = true;
			sap_cac_reset_notify(mac_handle);

			/*
			 * If hw_mode_status is QDF_STATUS_SUCCESS mean HW mode
			 * change was required and was successfully requested so
			 * the channel switch will continue after HW mode change
			 * completion.
			 */
			if (QDF_IS_STATUS_SUCCESS(hw_mode_status)) {
				sap_info("Channel change will continue after HW mode change");
				return QDF_STATUS_SUCCESS;
			}
			/*
			 * If hw_mode_status is QDF_STATUS_E_NOSUPPORT or
			 * QDF_STATUS_E_ALREADY (not QDF_STATUS_E_FAILURE and
			 * not QDF_STATUS_SUCCESS), mean DBS is not supported or
			 * required HW mode is already set, So contunue with
			 * CSA from here.
			 */
			sap_start_csa_restart(mac, sap_ctx);
		} else {
			sap_err("Failed to request Channel Change, since SAP is not in SAP_STARTED state");
			return QDF_STATUS_E_FAULT;
		}

	} else {
		sap_err("Channel freq = %d is not valid in the current"
			"regulatory domain, is_dfs %d", target_chan_freq,
			is_dfs);

		return QDF_STATUS_E_FAULT;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_sap_getstation_ie_information(struct sap_context *sap_ctx,
					      uint32_t *len, uint8_t *buf)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	uint32_t ie_len = 0;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	if (len) {
		ie_len = *len;
		*len = sap_ctx->nStaWPARSnReqIeLength;
		sap_info("WPAIE len : %x", *len);
		if ((buf) && (ie_len >= sap_ctx->nStaWPARSnReqIeLength)) {
			qdf_mem_copy(buf,
				sap_ctx->pStaWpaRsnReqIE,
				sap_ctx->nStaWPARSnReqIeLength);
			sap_info("WPAIE: "QDF_MAC_ADDR_FMT,
				 QDF_MAC_ADDR_REF(buf));
			qdf_status = QDF_STATUS_SUCCESS;
		}
	}
	return qdf_status;
}

QDF_STATUS wlan_sap_update_next_channel(struct sap_context *sap_ctx,
					uint8_t channel,
					enum phy_ch_width chan_bw)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	sap_ctx->dfs_vendor_channel = channel;
	sap_ctx->dfs_vendor_chan_bw = chan_bw;

	return QDF_STATUS_SUCCESS;
}

void wlansap_get_sec_channel(uint8_t sec_ch_offset,
			     uint32_t op_chan_freq,
			     uint32_t *sec_chan_freq)
{
	switch (sec_ch_offset) {
	case LOW_PRIMARY_CH:
		*sec_chan_freq = op_chan_freq + 20;
		break;
	case HIGH_PRIMARY_CH:
		*sec_chan_freq = op_chan_freq - 20;
		break;
	default:
		*sec_chan_freq = 0;
	}
}

#ifdef WLAN_FEATURE_11BE
static void
wlansap_fill_channel_change_puncture(struct channel_change_req *req,
				     struct ch_params *ch_param)
{
	req->target_punc_bitmap = ch_param->reg_punc_bitmap;
}
#else
static inline void
wlansap_fill_channel_change_puncture(struct channel_change_req *req,
				     struct ch_params *ch_param)
{
}
#endif

/**
 * wlansap_fill_channel_change_request() - Fills the channel change request
 * @sap_ctx: sap context
 * @req: pointer to change channel request
 *
 * This function fills the channel change request for SAP
 *
 * Return: None
 */
static void
wlansap_fill_channel_change_request(struct sap_context *sap_ctx,
				    struct channel_change_req *req)
{
	struct mac_context *mac_ctx = sap_get_mac_context();
	struct bss_dot11_config dot11_cfg = {0};
	uint8_t h2e;

	dot11_cfg.vdev_id = sap_ctx->sessionId;
	dot11_cfg.bss_op_ch_freq = sap_ctx->chan_freq;
	dot11_cfg.phy_mode = sap_ctx->phyMode;
	dot11_cfg.privacy = sap_ctx->sap_bss_cfg.privacy;

	/* Rates configured from start_bss will have
	 * hostapd rates if hostapd chan rates are enabled
	 */
	qdf_mem_copy(dot11_cfg.opr_rates.rate,
		     sap_ctx->sap_bss_cfg.operationalRateSet.rate,
		     sap_ctx->sap_bss_cfg.operationalRateSet.numRates);
	dot11_cfg.opr_rates.numRates =
		sap_ctx->sap_bss_cfg.operationalRateSet.numRates;

	qdf_mem_copy(dot11_cfg.ext_rates.rate,
		     sap_ctx->sap_bss_cfg.extendedRateSet.rate,
		     sap_ctx->sap_bss_cfg.extendedRateSet.numRates);
	dot11_cfg.ext_rates.numRates =
		sap_ctx->sap_bss_cfg.extendedRateSet.numRates;
	sme_get_network_params(mac_ctx, &dot11_cfg);

	req->vdev_id = sap_ctx->sessionId;
	req->target_chan_freq = sap_ctx->chan_freq;
	req->sec_ch_offset = sap_ctx->ch_params.sec_ch_offset;
	req->ch_width =  sap_ctx->ch_params.ch_width;
	req->center_freq_seg0 = sap_ctx->ch_params.center_freq_seg0;
	req->center_freq_seg1 = sap_ctx->ch_params.center_freq_seg1;
	wlansap_fill_channel_change_puncture(req, &sap_ctx->ch_params);

	req->dot11mode = dot11_cfg.dot11_mode;
	req->nw_type = dot11_cfg.nw_type;

	sap_get_cac_dur_dfs_region(sap_ctx,
				   &req->cac_duration_ms,
				   &req->dfs_regdomain,
				   sap_ctx->chan_freq,
				   &sap_ctx->ch_params);
	mlme_set_cac_required(sap_ctx->vdev,
			      !!req->cac_duration_ms);

	/* Update the rates in sap_bss_cfg for subsequent channel switch */
	if (dot11_cfg.opr_rates.numRates) {
		qdf_mem_copy(req->opr_rates.rate,
			     dot11_cfg.opr_rates.rate,
			     dot11_cfg.opr_rates.numRates);
		qdf_mem_copy(sap_ctx->sap_bss_cfg.operationalRateSet.rate,
			     dot11_cfg.opr_rates.rate,
			     dot11_cfg.opr_rates.numRates);
		req->opr_rates.numRates = dot11_cfg.opr_rates.numRates;
		sap_ctx->sap_bss_cfg.operationalRateSet.numRates =
					dot11_cfg.opr_rates.numRates;
	} else {
		qdf_mem_zero(&sap_ctx->sap_bss_cfg.operationalRateSet,
			     sizeof(tSirMacRateSet));
	}

	if (dot11_cfg.ext_rates.numRates) {
		qdf_mem_copy(req->ext_rates.rate,
			     dot11_cfg.ext_rates.rate,
			     dot11_cfg.ext_rates.numRates);
		qdf_mem_copy(sap_ctx->sap_bss_cfg.extendedRateSet.rate,
			     dot11_cfg.ext_rates.rate,
			     dot11_cfg.ext_rates.numRates);
		req->ext_rates.numRates = dot11_cfg.ext_rates.numRates;
		sap_ctx->sap_bss_cfg.extendedRateSet.numRates =
					dot11_cfg.ext_rates.numRates;
	} else {
		qdf_mem_zero(&sap_ctx->sap_bss_cfg.extendedRateSet,
			     sizeof(tSirMacRateSet));
	}

	if (sap_ctx->require_h2e) {
		h2e = WLAN_BASIC_RATE_MASK |
			WLAN_BSS_MEMBERSHIP_SELECTOR_SAE_H2E;
		if (req->ext_rates.numRates < SIR_MAC_MAX_NUMBER_OF_RATES) {
			req->ext_rates.rate[req->ext_rates.numRates] = h2e;
			req->ext_rates.numRates++;
			sap_debug("H2E bss membership add to ext support rate");
		} else if (req->opr_rates.numRates <
						SIR_MAC_MAX_NUMBER_OF_RATES) {
			req->opr_rates.rate[req->opr_rates.numRates] = h2e;
			req->opr_rates.numRates++;
			sap_debug("H2E bss membership add to support rate");
		} else {
			sap_err("rates full, can not add H2E bss membership");
		}
	}
	return;
}

QDF_STATUS wlansap_channel_change_request(struct sap_context *sap_ctx,
					  uint32_t target_chan_freq)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx;
	eCsrPhyMode phy_mode;
	struct ch_params *ch_params;
	struct channel_change_req *ch_change_req;

	if (!target_chan_freq) {
		sap_err("channel 0 requested");
		return QDF_STATUS_E_FAULT;
	}

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	phy_mode = sap_ctx->phyMode;

	/* Update phy_mode if the target channel is in the other band */
	if (WLAN_REG_IS_5GHZ_CH_FREQ(target_chan_freq) &&
	    ((phy_mode == eCSR_DOT11_MODE_11g) ||
	    (phy_mode == eCSR_DOT11_MODE_11g_ONLY)))
		phy_mode = eCSR_DOT11_MODE_11a;
	else if (WLAN_REG_IS_24GHZ_CH_FREQ(target_chan_freq) &&
		 (phy_mode == eCSR_DOT11_MODE_11a))
		phy_mode = eCSR_DOT11_MODE_11g;
	sap_ctx->phyMode = phy_mode;

	if (!sap_ctx->chan_freq) {
		sap_err("Invalid channel list");
		return QDF_STATUS_E_FAULT;
	}
	/*
	 * We are getting channel bonding mode from sapDfsInfor structure
	 * because we've implemented channel width fallback mechanism for DFS
	 * which will result in channel width changing dynamically.
	 */
	ch_params = &mac_ctx->sap.SapDfsInfo.new_ch_params;
	if (sap_phymode_is_eht(sap_ctx->phyMode))
		wlan_reg_set_create_punc_bitmap(ch_params, true);
	wlan_reg_set_channel_params_for_pwrmode(mac_ctx->pdev, target_chan_freq,
						0, ch_params,
						REG_CURRENT_PWR_MODE);
	sap_ctx->ch_params_before_ch_switch = sap_ctx->ch_params;
	sap_ctx->freq_before_ch_switch = sap_ctx->chan_freq;
	/* Update the channel as this will be used to
	 * send event to supplicant
	 */
	sap_ctx->ch_params = *ch_params;
	sap_ctx->chan_freq = target_chan_freq;
	wlansap_get_sec_channel(ch_params->sec_ch_offset, sap_ctx->chan_freq,
				&sap_ctx->sec_ch_freq);
	sap_dfs_set_current_channel(sap_ctx);

	ch_change_req = qdf_mem_malloc(sizeof(struct channel_change_req));
	if (!ch_change_req)
		return QDF_STATUS_E_FAILURE;

	wlansap_fill_channel_change_request(sap_ctx, ch_change_req);

	status = sme_send_channel_change_req(MAC_HANDLE(mac_ctx),
					     ch_change_req);
	qdf_mem_free(ch_change_req);
	sap_debug("chan_freq:%d phy_mode %d width:%d offset:%d seg0:%d seg1:%d",
		  sap_ctx->chan_freq, phy_mode, ch_params->ch_width,
		  ch_params->sec_ch_offset, ch_params->center_freq_seg0,
		  ch_params->center_freq_seg1);
	if (policy_mgr_update_indoor_concurrency(mac_ctx->psoc,
						wlan_vdev_get_id(sap_ctx->vdev),
						sap_ctx->freq_before_ch_switch,
						DISCONNECT_WITH_CONCURRENCY))
		wlan_reg_recompute_current_chan_list(mac_ctx->psoc,
						     mac_ctx->pdev);

	return status;
}

QDF_STATUS wlansap_start_beacon_req(struct sap_context *sap_ctx)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t dfs_cac_wait_status;
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	/* No Radar was found during CAC WAIT, So start Beaconing */
	if (!sap_ctx->sap_radar_found_status) {
		/* CAC Wait done without any Radar Detection */
		dfs_cac_wait_status = true;
		wlan_pre_cac_complete_set(sap_ctx->vdev, false);
		status = sme_roam_start_beacon_req(MAC_HANDLE(mac),
						   sap_ctx->bssid,
						   dfs_cac_wait_status);
	}

	return status;
}

QDF_STATUS wlansap_dfs_send_csa_ie_request(struct sap_context *sap_ctx)
{
	struct mac_context *mac;
	uint32_t new_cac_ms;
	uint32_t dfs_region;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	mac->sap.SapDfsInfo.new_ch_params.ch_width =
				mac->sap.SapDfsInfo.new_chanWidth;
	if (sap_phymode_is_eht(sap_ctx->phyMode))
		wlan_reg_set_create_punc_bitmap(
			&mac->sap.SapDfsInfo.new_ch_params, true);
	wlan_reg_set_channel_params_for_pwrmode(mac->pdev,
			mac->sap.SapDfsInfo.target_chan_freq,
			0, &mac->sap.SapDfsInfo.new_ch_params,
			REG_CURRENT_PWR_MODE);

	sap_get_cac_dur_dfs_region(sap_ctx, &new_cac_ms, &dfs_region,
				   mac->sap.SapDfsInfo.target_chan_freq,
				   &mac->sap.SapDfsInfo.new_ch_params);
	mlme_set_cac_required(sap_ctx->vdev, !!new_cac_ms);
	sap_debug("chan freq:%d req:%d width:%d off:%d cac %d",
		  mac->sap.SapDfsInfo.target_chan_freq,
		  mac->sap.SapDfsInfo.csaIERequired,
		  mac->sap.SapDfsInfo.new_ch_params.ch_width,
		  mac->sap.SapDfsInfo.new_ch_params.sec_ch_offset,
		  new_cac_ms);

	return sme_roam_csa_ie_request(MAC_HANDLE(mac),
				       sap_ctx->bssid,
				       mac->sap.SapDfsInfo.target_chan_freq,
				       mac->sap.SapDfsInfo.csaIERequired,
				       &mac->sap.SapDfsInfo.new_ch_params,
				       new_cac_ms);
}

QDF_STATUS wlansap_get_dfs_ignore_cac(mac_handle_t mac_handle,
				      uint8_t *ignore_cac)
{
	struct mac_context *mac = NULL;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}

	*ignore_cac = mac->sap.SapDfsInfo.ignore_cac;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_set_dfs_ignore_cac(mac_handle_t mac_handle,
				      uint8_t ignore_cac)
{
	struct mac_context *mac = NULL;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac->sap.SapDfsInfo.ignore_cac = (ignore_cac >= true) ?
					  true : false;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_get_dfs_cac_state(mac_handle_t mac_handle,
				     struct sap_context *sapcontext,
				     bool *cac_state)
{
	struct mac_context *mac = NULL;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}
	if (!sapcontext) {
		sap_err("Invalid sapcontext pointer");
		return QDF_STATUS_E_FAULT;
	}

	*cac_state = sap_is_dfs_cac_wait_state(sapcontext);

	return QDF_STATUS_SUCCESS;
}

bool sap_is_auto_channel_select(struct sap_context *sapcontext)
{
	if (!sapcontext) {
		sap_err("Invalid SAP pointer");
		return false;
	}
	return sapcontext->chan_freq == AUTO_CHANNEL_SELECT;
}

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/**
 * wlan_sap_set_channel_avoidance() - sets sap mcc channel avoidance ini param
 * @mac_handle: Opaque handle to the global MAC context
 * @sap_channel_avoidance: ini parameter value
 *
 * sets sap mcc channel avoidance ini param, to be called in sap_start
 *
 * Return: success of failure of operation
 */
QDF_STATUS
wlan_sap_set_channel_avoidance(mac_handle_t mac_handle,
			       bool sap_channel_avoidance)
{
	struct mac_context *mac_ctx = NULL;

	if (mac_handle) {
		mac_ctx = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("mac_handle or mac_ctx pointer NULL");
		return QDF_STATUS_E_FAULT;
	}
	mac_ctx->sap.sap_channel_avoidance = sap_channel_avoidance;
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

QDF_STATUS
wlan_sap_set_acs_with_more_param(mac_handle_t mac_handle,
				 bool acs_with_more_param)
{
	struct mac_context *mac_ctx;

	if (mac_handle) {
		mac_ctx = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("mac_handle or mac_ctx pointer NULL");
		return QDF_STATUS_E_FAULT;
	}
	mac_ctx->sap.acs_with_more_param = acs_with_more_param;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlansap_set_dfs_preferred_channel_location(mac_handle_t mac_handle)
{
	struct mac_context *mac = NULL;
	QDF_STATUS status;
	enum dfs_reg dfs_region;
	uint8_t dfs_preferred_channels_location = 0;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}

	wlan_reg_get_dfs_region(mac->pdev, &dfs_region);

	/*
	 * The Indoor/Outdoor only random channel selection
	 * restriction is currently enforeced only for
	 * JAPAN regulatory domain.
	 */
	ucfg_mlme_get_pref_chan_location(mac->psoc,
					 &dfs_preferred_channels_location);
	sap_debug("dfs_preferred_channels_location %d dfs region %d",
		  dfs_preferred_channels_location, dfs_region);

	if (dfs_region == DFS_MKK_REGION ||
	    dfs_region == DFS_MKKN_REGION) {
		mac->sap.SapDfsInfo.sap_operating_chan_preferred_location =
			dfs_preferred_channels_location;
		sap_debug("sapdfs:Set Preferred Operating Channel location=%d",
			  mac->sap.SapDfsInfo.
			  sap_operating_chan_preferred_location);

		status = QDF_STATUS_SUCCESS;
	} else {
		sap_debug("sapdfs:NOT JAPAN REG, Invalid Set preferred chans location");

		status = QDF_STATUS_E_FAULT;
	}

	return status;
}

QDF_STATUS wlansap_set_dfs_target_chnl(mac_handle_t mac_handle,
				       uint32_t target_chan_freq)
{
	struct mac_context *mac = NULL;

	if (mac_handle) {
		mac = MAC_CONTEXT(mac_handle);
	} else {
		sap_err("Invalid mac_handle pointer");
		return QDF_STATUS_E_FAULT;
	}
	if (target_chan_freq > 0) {
		mac->sap.SapDfsInfo.user_provided_target_chan_freq =
			target_chan_freq;
	} else {
		mac->sap.SapDfsInfo.user_provided_target_chan_freq = 0;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlansap_update_sap_config_add_ie(struct sap_config *config,
				 const uint8_t *pAdditionIEBuffer,
				 uint16_t additionIELength,
				 eUpdateIEsType updateType)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t bufferValid = false;
	uint16_t bufferLength = 0;
	uint8_t *pBuffer = NULL;

	if (!config) {
		return QDF_STATUS_E_FAULT;
	}

	if ((pAdditionIEBuffer) && (additionIELength != 0)) {
		/* initialize the buffer pointer so that pe can copy */
		if (additionIELength > 0) {
			bufferLength = additionIELength;
			pBuffer = qdf_mem_malloc(bufferLength);
			if (!pBuffer)
				return QDF_STATUS_E_NOMEM;

			qdf_mem_copy(pBuffer, pAdditionIEBuffer, bufferLength);
			bufferValid = true;
			sap_debug("update_type: %d", updateType);
			qdf_trace_hex_dump(QDF_MODULE_ID_SAP,
				QDF_TRACE_LEVEL_DEBUG, pBuffer, bufferLength);
		}
	}

	switch (updateType) {
	case eUPDATE_IE_PROBE_BCN:
		if (config->pProbeRespBcnIEsBuffer)
			qdf_mem_free(config->pProbeRespBcnIEsBuffer);
		if (bufferValid) {
			config->probeRespBcnIEsLen = bufferLength;
			config->pProbeRespBcnIEsBuffer = pBuffer;
		} else {
			config->probeRespBcnIEsLen = 0;
			config->pProbeRespBcnIEsBuffer = NULL;
		}
		break;
	case eUPDATE_IE_PROBE_RESP:
		if (config->pProbeRespIEsBuffer)
			qdf_mem_free(config->pProbeRespIEsBuffer);
		if (bufferValid) {
			config->probeRespIEsBufferLen = bufferLength;
			config->pProbeRespIEsBuffer = pBuffer;
		} else {
			config->probeRespIEsBufferLen = 0;
			config->pProbeRespIEsBuffer = NULL;
		}
		break;
	case eUPDATE_IE_ASSOC_RESP:
		if (config->pAssocRespIEsBuffer)
			qdf_mem_free(config->pAssocRespIEsBuffer);
		if (bufferValid) {
			config->assocRespIEsLen = bufferLength;
			config->pAssocRespIEsBuffer = pBuffer;
		} else {
			config->assocRespIEsLen = 0;
			config->pAssocRespIEsBuffer = NULL;
		}
		break;
	default:
		sap_debug("No matching buffer type %d", updateType);
		if (pBuffer)
			qdf_mem_free(pBuffer);
		break;
	}

	return status;
}

QDF_STATUS
wlansap_reset_sap_config_add_ie(struct sap_config *config,
				eUpdateIEsType updateType)
{
	if (!config) {
		sap_err("Invalid Config pointer");
		return QDF_STATUS_E_FAULT;
	}

	switch (updateType) {
	case eUPDATE_IE_ALL:    /*only used to reset */
	case eUPDATE_IE_PROBE_RESP:
		if (config->pProbeRespIEsBuffer) {
			qdf_mem_free(config->pProbeRespIEsBuffer);
			config->probeRespIEsBufferLen = 0;
			config->pProbeRespIEsBuffer = NULL;
		}
		if (eUPDATE_IE_ALL != updateType)
			break;
		fallthrough;
	case eUPDATE_IE_ASSOC_RESP:
		if (config->pAssocRespIEsBuffer) {
			qdf_mem_free(config->pAssocRespIEsBuffer);
			config->assocRespIEsLen = 0;
			config->pAssocRespIEsBuffer = NULL;
		}
		if (eUPDATE_IE_ALL != updateType)
			break;
		fallthrough;
	case eUPDATE_IE_PROBE_BCN:
		if (config->pProbeRespBcnIEsBuffer) {
			qdf_mem_free(config->pProbeRespBcnIEsBuffer);
			config->probeRespBcnIEsLen = 0;
			config->pProbeRespBcnIEsBuffer = NULL;
		}
		if (eUPDATE_IE_ALL != updateType)
			break;
		fallthrough;
	default:
		if (eUPDATE_IE_ALL != updateType)
			sap_err("Invalid buffer type %d", updateType);
		break;
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_SON
QDF_STATUS
wlansap_son_update_sap_config_phymode(struct wlan_objmgr_vdev *vdev,
				      struct sap_config *config,
				      enum qca_wlan_vendor_phy_mode phy_mode)
{
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_channel *des_chan;

	if (!vdev || !config) {
		sap_err("Invalid input parameters");
		return QDF_STATUS_E_FAULT;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		sap_err("Invalid pdev parameters");
		return QDF_STATUS_E_FAULT;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		sap_err("Invalid psoc parameters");
		return QDF_STATUS_E_FAULT;
	}
	des_chan = wlan_vdev_mlme_get_des_chan(vdev);
	if (!des_chan) {
		sap_err("Invalid desired channel");
		return QDF_STATUS_E_FAULT;
	}
	config->sap_orig_hw_mode = config->SapHw_mode;
	config->ch_width_orig = config->ch_params.ch_width;
	switch (phy_mode) {
	case QCA_WLAN_VENDOR_PHY_MODE_11A:
		config->SapHw_mode = eCSR_DOT11_MODE_11a;
		config->ch_params.ch_width = CH_WIDTH_20MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11B:
		config->SapHw_mode = eCSR_DOT11_MODE_11b;
		config->ch_params.ch_width = CH_WIDTH_20MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11G:
		config->SapHw_mode = eCSR_DOT11_MODE_11g;
		config->ch_params.ch_width = CH_WIDTH_20MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11AGN:
	case QCA_WLAN_VENDOR_PHY_MODE_11NG_HT20:
	case QCA_WLAN_VENDOR_PHY_MODE_11NA_HT20:
		config->SapHw_mode = eCSR_DOT11_MODE_11n;
		config->ch_params.ch_width = CH_WIDTH_20MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40PLUS:
	case QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40MINUS:
	case QCA_WLAN_VENDOR_PHY_MODE_11NG_HT40:
	case QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40PLUS:
	case QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40MINUS:
	case QCA_WLAN_VENDOR_PHY_MODE_11NA_HT40:
		config->SapHw_mode = eCSR_DOT11_MODE_11n;
		config->ch_params.ch_width = CH_WIDTH_40MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT20:
		config->SapHw_mode = eCSR_DOT11_MODE_11ac;
		config->ch_params.ch_width = CH_WIDTH_20MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40PLUS:
	case QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40MINUS:
	case QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT40:
		config->SapHw_mode = eCSR_DOT11_MODE_11ac;
		config->ch_params.ch_width = CH_WIDTH_40MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT80:
		config->SapHw_mode = eCSR_DOT11_MODE_11ac;
		config->ch_params.ch_width = CH_WIDTH_80MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11AC_VHT160:
		config->SapHw_mode = eCSR_DOT11_MODE_11ac;
		config->ch_params.ch_width = CH_WIDTH_160MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11AX_HE20:
		config->SapHw_mode = eCSR_DOT11_MODE_11ax;
		config->ch_params.ch_width = CH_WIDTH_20MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40:
	case QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40PLUS:
	case QCA_WLAN_VENDOR_PHY_MODE_11AX_HE40MINUS:
		config->SapHw_mode = eCSR_DOT11_MODE_11ax;
		config->ch_params.ch_width = CH_WIDTH_40MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11AX_HE80:
		config->SapHw_mode = eCSR_DOT11_MODE_11ax;
		config->ch_params.ch_width = CH_WIDTH_80MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_11AX_HE160:
		config->SapHw_mode = eCSR_DOT11_MODE_11ax;
		config->ch_params.ch_width = CH_WIDTH_160MHZ;
		break;
	case QCA_WLAN_VENDOR_PHY_MODE_AUTO:
		config->SapHw_mode = eCSR_DOT11_MODE_AUTO;
		break;
	default:
		sap_err("Invalid phy mode %d to configure", phy_mode);
		break;
	}

	if (sap_phymode_is_eht(config->SapHw_mode))
		wlan_reg_set_create_punc_bitmap(&config->ch_params, true);
	if (config->ch_params.ch_width == CH_WIDTH_80P80MHZ &&
	    ucfg_mlme_get_restricted_80p80_bw_supp(psoc)) {
		if (!((config->ch_params.center_freq_seg0 == 138 &&
		       config->ch_params.center_freq_seg1 == 155) ||
		      (config->ch_params.center_freq_seg1 == 138 &&
		       config->ch_params.center_freq_seg0 == 155))) {
			sap_debug("Falling back to 80 from 80p80 as non supported freq_seq0 %d and freq_seq1 %d",
				  config->ch_params.mhz_freq_seg0,
				  config->ch_params.mhz_freq_seg1);
			config->ch_params.center_freq_seg1 = 0;
			config->ch_params.mhz_freq_seg1 = 0;
			config->ch_width_orig = CH_WIDTH_80MHZ;
			config->ch_params.ch_width = config->ch_width_orig;
		}
	}

	config->chan_freq = des_chan->ch_freq;
	config->sec_ch_freq = 0;
	if (WLAN_REG_IS_24GHZ_CH_FREQ(des_chan->ch_freq) &&
	    config->ch_params.ch_width == CH_WIDTH_40MHZ &&
	    des_chan->ch_width == CH_WIDTH_40MHZ) {
		if (des_chan->ch_cfreq1 == des_chan->ch_freq + BW_10_MHZ)
			config->sec_ch_freq = des_chan->ch_freq + BW_20_MHZ;
		if (des_chan->ch_cfreq1 == des_chan->ch_freq - BW_10_MHZ)
			config->sec_ch_freq = des_chan->ch_freq - BW_20_MHZ;
	}
	wlan_reg_set_channel_params_for_pwrmode(pdev, config->chan_freq,
						config->sec_ch_freq,
						&config->ch_params,
						REG_CURRENT_PWR_MODE);

	return QDF_STATUS_SUCCESS;
}
#endif

#define ACS_WLAN_20M_CH_INC 20
#define ACS_2G_EXTEND ACS_WLAN_20M_CH_INC
#define ACS_5G_EXTEND (ACS_WLAN_20M_CH_INC * 3)

#ifdef CONFIG_BAND_6GHZ
static void wlansap_update_start_range_6ghz(
	uint32_t *start_ch_freq, uint32_t *bandStartChannel)
{
	*bandStartChannel = MIN_6GHZ_CHANNEL;
	*start_ch_freq = (*start_ch_freq - ACS_5G_EXTEND) >
				wlan_reg_ch_to_freq(MIN_6GHZ_CHANNEL) ?
			   (*start_ch_freq - ACS_5G_EXTEND) :
				wlan_reg_ch_to_freq(MIN_6GHZ_CHANNEL);
}

static void wlansap_update_end_range_6ghz(
	uint32_t *end_ch_freq, uint32_t *bandEndChannel)
{
	*bandEndChannel = MAX_6GHZ_CHANNEL;
	*end_ch_freq = (*end_ch_freq + ACS_5G_EXTEND) <=
			     wlan_reg_ch_to_freq(MAX_6GHZ_CHANNEL) ?
			     (*end_ch_freq + ACS_5G_EXTEND) :
			     wlan_reg_ch_to_freq(MAX_6GHZ_CHANNEL);
}
#else
static void wlansap_update_start_range_6ghz(
	uint32_t *start_ch_freq, uint32_t *bandStartChannel)
{
}

static void wlansap_update_end_range_6ghz(
	uint32_t *end_ch_freq, uint32_t *bandEndChannel)
{
}
#endif

/*==========================================================================
   FUNCTION  wlansap_extend_to_acs_range

   DESCRIPTION Function extends give channel range to consider ACS chan bonding

   DEPENDENCIES PARAMETERS

   IN /OUT
   * start_ch_freq : ACS extend start ch
   * end_ch_freq   : ACS extended End ch
   * bandStartChannel: Band start ch
   * bandEndChannel  : Band end ch

   RETURN VALUE NONE

   SIDE EFFECTS
   ============================================================================*/
void wlansap_extend_to_acs_range(mac_handle_t mac_handle,
				 uint32_t *start_ch_freq,
				 uint32_t *end_ch_freq,
				 uint32_t *bandStartChannel,
				 uint32_t *bandEndChannel)
{
	uint32_t tmp_start_ch_freq = 0, tmp_end_ch_freq = 0;
	struct mac_context *mac_ctx;

	mac_ctx = MAC_CONTEXT(mac_handle);
	if (!mac_ctx) {
		sap_err("Invalid mac_ctx");
		return;
	}
	if (*start_ch_freq <= wlan_reg_ch_to_freq(CHAN_ENUM_2484)) {
		*bandStartChannel = CHAN_ENUM_2412;
		tmp_start_ch_freq = *start_ch_freq >
					wlan_reg_ch_to_freq(CHAN_ENUM_2432) ?
					(*start_ch_freq - ACS_2G_EXTEND) :
					wlan_reg_ch_to_freq(CHAN_ENUM_2412);
	} else if (*start_ch_freq <= wlan_reg_ch_to_freq(CHAN_ENUM_5885)) {
		*bandStartChannel = CHAN_ENUM_5180;
		tmp_start_ch_freq = (*start_ch_freq - ACS_5G_EXTEND) >
					wlan_reg_ch_to_freq(CHAN_ENUM_5180) ?
				   (*start_ch_freq - ACS_5G_EXTEND) :
					wlan_reg_ch_to_freq(CHAN_ENUM_5180);
	} else if (WLAN_REG_IS_6GHZ_CHAN_FREQ(*start_ch_freq)) {
		tmp_start_ch_freq = *start_ch_freq;
		wlansap_update_start_range_6ghz(&tmp_start_ch_freq,
						bandStartChannel);
	} else {
		*bandStartChannel = CHAN_ENUM_2412;
		tmp_start_ch_freq = *start_ch_freq >
					wlan_reg_ch_to_freq(CHAN_ENUM_2432) ?
					(*start_ch_freq - ACS_2G_EXTEND) :
					wlan_reg_ch_to_freq(CHAN_ENUM_2412);
		sap_err("unexpected start freq %d",
			*start_ch_freq);
	}

	if (*end_ch_freq <= wlan_reg_ch_to_freq(CHAN_ENUM_2484)) {
		*bandEndChannel = CHAN_ENUM_2484;
		tmp_end_ch_freq = (*end_ch_freq + ACS_2G_EXTEND) <=
					wlan_reg_ch_to_freq(CHAN_ENUM_2484) ?
					(*end_ch_freq + ACS_2G_EXTEND) :
					wlan_reg_ch_to_freq(CHAN_ENUM_2484);
	} else if (*end_ch_freq <= wlan_reg_ch_to_freq(CHAN_ENUM_5885)) {
		*bandEndChannel = CHAN_ENUM_5885;
		tmp_end_ch_freq = (*end_ch_freq + ACS_5G_EXTEND) <=
				     wlan_reg_ch_to_freq(CHAN_ENUM_5885) ?
				     (*end_ch_freq + ACS_5G_EXTEND) :
				     wlan_reg_ch_to_freq(CHAN_ENUM_5885);
	} else if (WLAN_REG_IS_6GHZ_CHAN_FREQ(*end_ch_freq)) {
		tmp_end_ch_freq = *end_ch_freq;
		wlansap_update_end_range_6ghz(&tmp_end_ch_freq,
					      bandEndChannel);
	} else {
		*bandEndChannel = CHAN_ENUM_5885;
		tmp_end_ch_freq = (*end_ch_freq + ACS_5G_EXTEND) <=
				     wlan_reg_ch_to_freq(CHAN_ENUM_5885) ?
				     (*end_ch_freq + ACS_5G_EXTEND) :
				     wlan_reg_ch_to_freq(CHAN_ENUM_5885);

		sap_err("unexpected end freq %d", *end_ch_freq);
	}
	*start_ch_freq = tmp_start_ch_freq;
	*end_ch_freq = tmp_end_ch_freq;
	/* Note if the ACS range include only DFS channels, do not cross range
	* Active scanning in adjacent non DFS channels results in transmission
	* spikes in DFS spectrum channels which is due to emission spill.
	* Remove the active channels from extend ACS range for DFS only range
	*/
	if (wlan_reg_is_dfs_for_freq(mac_ctx->pdev, *start_ch_freq)) {
		while (!wlan_reg_is_dfs_for_freq(
				mac_ctx->pdev,
				tmp_start_ch_freq) &&
		       tmp_start_ch_freq < *start_ch_freq)
			tmp_start_ch_freq += ACS_WLAN_20M_CH_INC;

		*start_ch_freq = tmp_start_ch_freq;
	}
	if (wlan_reg_is_dfs_for_freq(mac_ctx->pdev, *end_ch_freq)) {
		while (!wlan_reg_is_dfs_for_freq(
				mac_ctx->pdev,
				tmp_end_ch_freq) &&
		       tmp_end_ch_freq > *end_ch_freq)
			tmp_end_ch_freq -= ACS_WLAN_20M_CH_INC;

		*end_ch_freq = tmp_end_ch_freq;
	}
}

QDF_STATUS wlan_sap_set_vendor_acs(struct sap_context *sap_context,
				   bool is_vendor_acs)
{
	if (!sap_context) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}
	sap_context->vendor_acs_dfs_lte_enabled = is_vendor_acs;

	return QDF_STATUS_SUCCESS;
}

#ifdef DFS_COMPONENT_ENABLE
QDF_STATUS wlansap_set_dfs_nol(struct sap_context *sap_ctx,
			       eSapDfsNolType conf)
{
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	if (conf == eSAP_DFS_NOL_CLEAR) {
		struct wlan_objmgr_pdev *pdev;

		sap_err("clear the DFS NOL");

		pdev = mac->pdev;
		if (!pdev) {
			sap_err("null pdev");
			return QDF_STATUS_E_FAULT;
		}
		utils_dfs_clear_nol_channels(pdev);
	} else if (conf == eSAP_DFS_NOL_RANDOMIZE) {
		sap_err("Randomize the DFS NOL");

	} else {
		sap_err("unsupported type %d", conf);
	}

	return QDF_STATUS_SUCCESS;
}
#endif

void wlansap_populate_del_sta_params(const uint8_t *mac,
				     uint16_t reason_code,
				     uint8_t subtype,
				     struct csr_del_sta_params *params)
{
	if (!mac)
		qdf_set_macaddr_broadcast(&params->peerMacAddr);
	else
		qdf_mem_copy(params->peerMacAddr.bytes, mac,
			     QDF_MAC_ADDR_SIZE);

	if (reason_code == 0)
		params->reason_code = REASON_DEAUTH_NETWORK_LEAVING;
	else
		params->reason_code = reason_code;

	if (subtype == SIR_MAC_MGMT_DEAUTH || subtype == SIR_MAC_MGMT_DISASSOC)
		params->subtype = subtype;
	else
		params->subtype = SIR_MAC_MGMT_DEAUTH;

	sap_debug("Delete STA with RC:%hu subtype:%hhu MAC::" QDF_MAC_ADDR_FMT,
		  params->reason_code, params->subtype,
		  QDF_MAC_ADDR_REF(params->peerMacAddr.bytes));
}

void sap_undo_acs(struct sap_context *sap_ctx, struct sap_config *sap_cfg)
{
	struct sap_acs_cfg *acs_cfg;

	if (!sap_ctx)
		return;

	acs_cfg = &sap_cfg->acs_cfg;
	if (!acs_cfg)
		return;

	if (acs_cfg->freq_list) {
		sap_debug("Clearing ACS cfg ch freq list");
		qdf_mem_free(acs_cfg->freq_list);
		acs_cfg->freq_list = NULL;
	}
	if (acs_cfg->master_freq_list) {
		sap_debug("Clearing master ACS cfg chan freq list");
		qdf_mem_free(acs_cfg->master_freq_list);
		acs_cfg->master_freq_list = NULL;
	}
	if (sap_ctx->freq_list) {
		sap_debug("Clearing sap context ch freq list");
		qdf_mem_free(sap_ctx->freq_list);
		sap_ctx->freq_list = NULL;
	}
	acs_cfg->ch_list_count = 0;
	acs_cfg->master_ch_list_count = 0;
	acs_cfg->acs_mode = false;
	acs_cfg->master_ch_list_updated = false;
	sap_ctx->num_of_channel = 0;
	wlansap_dcs_set_vdev_wlan_interference_mitigation(sap_ctx, false);
}

QDF_STATUS wlansap_acs_chselect(struct sap_context *sap_context,
				sap_event_cb acs_event_callback,
				struct sap_config *config)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac;

	if (!sap_context) {
		sap_err("Invalid SAP pointer");

		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	sap_context->acs_cfg = &config->acs_cfg;
	sap_context->ch_width_orig = config->acs_cfg.ch_width;
	if (sap_context->fsm_state != SAP_STARTED)
		sap_context->phyMode = config->acs_cfg.hw_mode;

	/*
	 * Now, configure the scan and ACS channel params
	 * to issue a scan request.
	 */
	wlansap_set_scan_acs_channel_params(config, sap_context);

	/*
	 * Copy the HDD callback function to report the
	 * ACS result after scan in SAP context callback function.
	 */
	sap_context->sap_event_cb = acs_event_callback;

	/*
	 * Issue the scan request. This scan request is
	 * issued before the start BSS is done so
	 *
	 * 1. No need to pass the second parameter
	 * as the SAP state machine is not started yet
	 * and there is no need for any event posting.
	 *
	 * 2. Set third parameter to TRUE to indicate the
	 * channel selection function to register a
	 * different scan callback function to process
	 * the results pre start BSS.
	 */
	qdf_status = sap_channel_sel(sap_context);

	if (QDF_STATUS_E_ABORTED == qdf_status) {
		sap_err("DFS not supported in the current operating mode");
		return QDF_STATUS_E_FAILURE;
	} else if (QDF_STATUS_E_CANCELED == qdf_status) {
		/*
		* ERROR is returned when either the SME scan request
		* failed or ACS is overridden due to other constrainst
		* So send selected channel to HDD
		*/
		sap_err("Scan Req Failed/ACS Overridden");
		sap_err("Selected channel frequency = %d",
			sap_context->chan_freq);

		return sap_signal_hdd_event(sap_context, NULL,
				eSAP_ACS_CHANNEL_SELECTED,
				(void *) eSAP_STATUS_SUCCESS);
	}

	return qdf_status;
}

/**
 * wlan_sap_enable_phy_error_logs() - Enable DFS phy error logs
 * @mac_handle: Opaque handle to the global MAC context
 * @enable_log: value to set
 *
 * Since the frequency of DFS phy error is very high, enabling logs for them
 * all the times can cause crash and will also create lot of useless logs
 * causing difficulties in debugging other issue. This function will be called
 * from iwpriv cmd to enable such logs temporarily.
 *
 * Return: void
 */
void wlan_sap_enable_phy_error_logs(mac_handle_t mac_handle,
				    uint32_t enable_log)
{
	int error;

	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	mac_ctx->sap.enable_dfs_phy_error_logs = !!enable_log;
	tgt_dfs_control(mac_ctx->pdev, DFS_SET_DEBUG_LEVEL, &enable_log,
			sizeof(uint32_t), NULL, NULL, &error);
}

#ifdef DFS_PRI_MULTIPLIER
void wlan_sap_set_dfs_pri_multiplier(mac_handle_t mac_handle)
{
	int error;

	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	tgt_dfs_control(mac_ctx->pdev, DFS_SET_PRI_MULTIPILER,
			&mac_ctx->mlme_cfg->dfs_cfg.dfs_pri_multiplier,
			sizeof(uint32_t), NULL, NULL, &error);
}
#endif

uint32_t wlansap_get_chan_width(struct sap_context *sap_ctx)
{
	return wlan_sap_get_vht_ch_width(sap_ctx);
}

enum phy_ch_width
wlansap_get_max_bw_by_phymode(struct sap_context *sap_ctx)
{
	uint32_t max_fw_bw;
	enum phy_ch_width ch_width;

	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return CH_WIDTH_20MHZ;
	}

	if (sap_ctx->phyMode == eCSR_DOT11_MODE_11ac ||
	    sap_ctx->phyMode == eCSR_DOT11_MODE_11ac_ONLY ||
	    sap_ctx->phyMode == eCSR_DOT11_MODE_11ax ||
	    sap_ctx->phyMode == eCSR_DOT11_MODE_11ax_ONLY ||
	    CSR_IS_DOT11_PHY_MODE_11BE(sap_ctx->phyMode) ||
	    CSR_IS_DOT11_PHY_MODE_11BE_ONLY(sap_ctx->phyMode)) {
		max_fw_bw = sme_get_vht_ch_width();
		if (max_fw_bw >= WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
			ch_width = CH_WIDTH_160MHZ;
		else
			ch_width = CH_WIDTH_80MHZ;
		if (CSR_IS_DOT11_PHY_MODE_11BE(sap_ctx->phyMode) ||
		    CSR_IS_DOT11_PHY_MODE_11BE_ONLY(sap_ctx->phyMode))
			ch_width =
			QDF_MAX(wlansap_get_target_eht_phy_ch_width(),
				ch_width);
	} else if (sap_ctx->phyMode == eCSR_DOT11_MODE_11n ||
		   sap_ctx->phyMode == eCSR_DOT11_MODE_11n_ONLY) {
		ch_width = CH_WIDTH_40MHZ;
	} else {
		/* For legacy 11a mode return 20MHz */
		ch_width = CH_WIDTH_20MHZ;
	}

	return ch_width;
}

QDF_STATUS wlansap_set_invalid_session(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAILURE;
	}

	sap_ctx->sessionId = WLAN_UMAC_VDEV_ID_MAX;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_release_vdev_ref(struct sap_context *sap_ctx)
{
	if (!sap_ctx) {
		sap_err("Invalid SAP pointer");
		return QDF_STATUS_E_FAILURE;
	}

	sap_release_vdev_ref(sap_ctx);

	return QDF_STATUS_SUCCESS;
}

void wlansap_cleanup_cac_timer(struct sap_context *sap_ctx)
{
	struct mac_context *mac;

	if (!sap_ctx) {
		sap_debug("Invalid SAP context");
		return;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return;
	}

	if (mac->sap.SapDfsInfo.is_dfs_cac_timer_running) {
		mac->sap.SapDfsInfo.is_dfs_cac_timer_running = 0;
		if (!sap_ctx->dfs_cac_offload) {
			qdf_mc_timer_stop(
				&mac->sap.SapDfsInfo.sap_dfs_cac_timer);
			qdf_mc_timer_destroy(
				&mac->sap.SapDfsInfo.sap_dfs_cac_timer);
		}
		sap_err("sapdfs, force cleanup running dfs cac timer");
	}
}

#define DH_OUI_TYPE	(0x20)
/**
 * wlansap_validate_owe_ie() - validate OWE IE
 * @ie: IE buffer
 * @remaining_ie_len: remaining IE length
 *
 * Return: validated IE length, negative for failure
 */
static int wlansap_validate_owe_ie(const uint8_t *ie, uint32_t remaining_ie_len)
{
	uint8_t ie_id, ie_len, ie_ext_id = 0;

	if (remaining_ie_len < 2) {
		sap_err("IE too short");
		return -EINVAL;
	}

	ie_id = ie[0];
	ie_len = ie[1];

	/* IEs that we are expecting in OWE IEs
	 * - RSN IE
	 * - DH IE
	 */
	switch (ie_id) {
	case DOT11F_EID_RSN:
		if (ie_len < DOT11F_IE_RSN_MIN_LEN ||
		    ie_len > DOT11F_IE_RSN_MAX_LEN) {
			sap_err("Invalid RSN IE len %d", ie_len);
			return -EINVAL;
		}
		ie_len += 2;
		break;
	case DOT11F_EID_DH_PARAMETER_ELEMENT:
		ie_ext_id = ie[2];
		if (ie_ext_id != DH_OUI_TYPE) {
			sap_err("Invalid DH IE ID %d", ie_ext_id);
			return -EINVAL;
		}
		if (ie_len < DOT11F_IE_DH_PARAMETER_ELEMENT_MIN_LEN ||
		    ie_len > DOT11F_IE_DH_PARAMETER_ELEMENT_MAX_LEN) {
			sap_err("Invalid DH IE len %d", ie_len);
			return -EINVAL;
		}
		ie_len += 2;
		break;
	default:
		sap_err("Invalid IE %d", ie_id);
		return -EINVAL;
	}

	if (ie_len > remaining_ie_len) {
		sap_err("Invalid IE len");
		return -EINVAL;
	}

	return ie_len;
}

/**
 * wlansap_validate_owe_ies() - validate OWE IEs
 * @ie: IE buffer
 * @ie_len: IE length
 *
 * Return: true if validated
 */
static bool wlansap_validate_owe_ies(const uint8_t *ie, uint32_t ie_len)
{
	const uint8_t *remaining_ie = ie;
	uint32_t remaining_ie_len = ie_len;
	int validated_len;
	bool validated = true;

	while (remaining_ie_len) {
		validated_len = wlansap_validate_owe_ie(remaining_ie,
							remaining_ie_len);
		if (validated_len < 0) {
			validated = false;
			break;
		}
		remaining_ie += validated_len;
		remaining_ie_len -= validated_len;
	}

	return validated;
}

QDF_STATUS wlansap_update_owe_info(struct sap_context *sap_ctx,
				   uint8_t *peer, const uint8_t *ie,
				   uint32_t ie_len, uint16_t owe_status)
{
	struct mac_context *mac;
	struct owe_assoc_ind *owe_assoc_ind;
	struct assoc_ind *assoc_ind = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!wlansap_validate_owe_ies(ie, ie_len)) {
		sap_err("Invalid OWE IE");
		return QDF_STATUS_E_FAULT;
	}

	if (!sap_ctx) {
		sap_err("Invalid SAP context");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	if (QDF_STATUS_SUCCESS !=
		qdf_list_peek_front(&sap_ctx->owe_pending_assoc_ind_list,
				    &next_node)) {
		sap_err("Failed to find assoc ind list");
		return QDF_STATUS_E_FAILURE;
	}

	do {
		node = next_node;
		owe_assoc_ind = qdf_container_of(node, struct owe_assoc_ind,
						 node);
		if (qdf_mem_cmp(peer,
				owe_assoc_ind->assoc_ind->peerMacAddr,
				QDF_MAC_ADDR_SIZE) == 0) {
			status = qdf_list_remove_node(
					   &sap_ctx->owe_pending_assoc_ind_list,
					   node);
			if (status != QDF_STATUS_SUCCESS) {
				sap_err("Failed to remove assoc ind");
				return status;
			}
			assoc_ind = owe_assoc_ind->assoc_ind;
			qdf_mem_free(owe_assoc_ind);
			break;
		}
	} while (QDF_STATUS_SUCCESS ==
		 qdf_list_peek_next(&sap_ctx->owe_pending_assoc_ind_list,
				    node, &next_node));

	if (assoc_ind) {
		assoc_ind->owe_ie = ie;
		assoc_ind->owe_ie_len = ie_len;
		assoc_ind->owe_status = owe_status;
		status = sme_update_owe_info(mac, assoc_ind);
		qdf_mem_free(assoc_ind);
	} else {
		/*
		 * To cover if owe_pending_assoc_ind_list is not null
		 * on current link, but no node found with mac address
		 * match case , then need to return QDF_STATUS_E_EXISTS
		 * for failure case to look for match node in another link.
		 */
		status = QDF_STATUS_E_EXISTS;
		sap_debug("No match owe node");
	}

	return status;
}

QDF_STATUS wlansap_update_ft_info(struct sap_context *sap_ctx,
				  uint8_t *peer, const uint8_t *ie,
				  uint32_t ie_len, uint16_t ft_status)
{
	struct mac_context *mac;
	struct ft_assoc_ind *ft_assoc_ind;
	struct assoc_ind *assoc_ind = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	QDF_STATUS status;

	if (!sap_ctx) {
		sap_err("Invalid SAP context");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}
	status = qdf_wait_single_event(&sap_ctx->ft_pending_event,
				       500);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sap_err("wait for ft pending event timeout");
		wlansap_ft_cleanup(sap_ctx);
		return QDF_STATUS_E_FAULT;
	}

	if (QDF_STATUS_SUCCESS !=
		qdf_list_peek_front(&sap_ctx->ft_pending_assoc_ind_list,
				    &next_node)) {
		sap_err("Failed to find ft assoc ind list");
		return QDF_STATUS_E_FAILURE;
	}

	do {
		node = next_node;
		ft_assoc_ind = qdf_container_of(node, struct ft_assoc_ind, node);
		if (qdf_mem_cmp(peer,
				ft_assoc_ind->assoc_ind->peerMacAddr,
				QDF_MAC_ADDR_SIZE) == 0) {
			status = qdf_list_remove_node(&sap_ctx->ft_pending_assoc_ind_list,
						      node);
			if (status != QDF_STATUS_SUCCESS) {
				sap_err("Failed to remove ft assoc ind");
				return status;
			}
			assoc_ind = ft_assoc_ind->assoc_ind;
			qdf_mem_free(ft_assoc_ind);
			break;
		}
	} while (QDF_STATUS_SUCCESS ==
		 qdf_list_peek_next(&sap_ctx->ft_pending_assoc_ind_list,
				    node, &next_node));

	if (assoc_ind) {
		assoc_ind->ft_ie = ie;
		assoc_ind->ft_ie_len = ie_len;
		assoc_ind->ft_status = ft_status;
		status = sme_update_ft_info(mac, assoc_ind);
		qdf_mem_free(assoc_ind);
	}
	return status;
}

bool wlansap_is_channel_present_in_acs_list(uint32_t freq,
					    uint32_t *ch_freq_list,
					    uint8_t ch_count)
{
	uint8_t i;

	for(i = 0; i < ch_count; i++)
		if (ch_freq_list[i] == freq)
			return true;

	return false;
}

QDF_STATUS wlansap_filter_ch_based_acs(struct sap_context *sap_ctx,
				       uint32_t *ch_freq_list,
				       uint32_t *ch_cnt)
{
	size_t ch_index;
	size_t target_ch_cnt = 0;

	if (!sap_ctx || !ch_freq_list || !ch_cnt) {
		sap_err("NULL parameters");
		return QDF_STATUS_E_FAULT;
	}

	if (!sap_ctx->acs_cfg->acs_mode) {
		sap_debug("acs not enabled, no filtering required");
		return QDF_STATUS_SUCCESS;
	} else if (!sap_ctx->acs_cfg->master_freq_list ||
		   !sap_ctx->acs_cfg->master_ch_list_count) {
		sap_err("Empty acs channel list");
		return QDF_STATUS_E_FAULT;
	}

	for (ch_index = 0; ch_index < *ch_cnt; ch_index++) {
		if (wlansap_is_channel_present_in_acs_list(
					ch_freq_list[ch_index],
					sap_ctx->acs_cfg->master_freq_list,
					sap_ctx->acs_cfg->master_ch_list_count))
			ch_freq_list[target_ch_cnt++] = ch_freq_list[ch_index];
	}

	*ch_cnt = target_ch_cnt;

	return QDF_STATUS_SUCCESS;
}

bool wlansap_is_6ghz_included_in_acs_range(struct sap_context *sap_ctx)
{
	uint32_t i;
	uint32_t *ch_freq_list;

	if (!sap_ctx || !sap_ctx->acs_cfg ||
	    !sap_ctx->acs_cfg->master_freq_list ||
	    !sap_ctx->acs_cfg->master_ch_list_count) {
		sap_err("NULL parameters");
		return false;
	}
	ch_freq_list = sap_ctx->acs_cfg->master_freq_list;
	for (i = 0; i < sap_ctx->acs_cfg->master_ch_list_count; i++) {
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(ch_freq_list[i]))
			return true;
	}
	return false;
}

#if defined(FEATURE_WLAN_CH_AVOID)
/**
 * wlansap_select_chan_with_best_bandwidth() - Select channel with
 * max possible band width
 * @sap_ctx: sap context
 * @ch_freq_list: candidate channel frequency list
 * @ch_cnt: count of channel frequency in list
 * @selected_freq: selected channel frequency
 * @selected_ch_width: selected channel width
 *
 * Return: QDF_STATUS_SUCCESS if better channel selected
 */
static QDF_STATUS
wlansap_select_chan_with_best_bandwidth(struct sap_context *sap_ctx,
					uint32_t *ch_freq_list,
					uint32_t ch_cnt,
					uint32_t *selected_freq,
					enum phy_ch_width *selected_ch_width)
{
	struct mac_context *mac;
	struct ch_params ch_params = {0};
	enum phy_ch_width ch_width;
	uint32_t center_freq, bw_val, bw_start, bw_end;
	uint16_t i, j;
	uint16_t  unsafe_chan[NUM_CHANNELS] = {0};
	uint16_t  unsafe_chan_cnt = 0;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!selected_ch_width || !selected_freq)
		return QDF_STATUS_E_INVAL;

	if (!qdf_ctx) {
		sap_err("invalid qdf_ctx");
		return QDF_STATUS_E_INVAL;
	}

	if (!sap_ctx) {
		sap_err("invalid sap_ctx");
		return QDF_STATUS_E_INVAL;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_INVAL;
	}

	if (policy_mgr_mode_specific_connection_count(mac->psoc,
						      PM_STA_MODE,
						      NULL) ||
	    policy_mgr_mode_specific_connection_count(mac->psoc,
						      PM_P2P_CLIENT_MODE,
						      NULL) ||
	    policy_mgr_mode_specific_connection_count(mac->psoc,
						      PM_P2P_GO_MODE,
						      NULL)) {
		sap_debug("sta/p2p mode active, skip!");
		return QDF_STATUS_E_INVAL;
	}

	pld_get_wlan_unsafe_channel(qdf_ctx->dev, unsafe_chan,
				    &unsafe_chan_cnt,
				    sizeof(uint16_t) * NUM_CHANNELS);
	unsafe_chan_cnt = QDF_MIN(unsafe_chan_cnt, NUM_CHANNELS);
	if (!unsafe_chan_cnt)
		return QDF_STATUS_E_INVAL;

	ch_width = sap_ctx->ch_width_orig;
next_lower_bw:
	for (i = 0; i < ch_cnt; i++) {
		if (!WLAN_REG_IS_SAME_BAND_FREQS(sap_ctx->chan_freq,
						 ch_freq_list[i]))
			continue;
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(ch_freq_list[i]) &&
		    !WLAN_REG_IS_6GHZ_PSC_CHAN_FREQ(ch_freq_list[i]))
			continue;
		qdf_mem_zero(&ch_params, sizeof(ch_params));
		ch_params.ch_width = ch_width;
		wlan_reg_set_channel_params_for_pwrmode(mac->pdev,
							ch_freq_list[i],
							0, &ch_params,
							REG_CURRENT_PWR_MODE);
		if (!WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq_list[i]) &&
		    wlan_reg_get_5g_bonded_channel_state_for_pwrmode(mac->pdev,
								     ch_freq_list[i],
								     &ch_params,
								     REG_CURRENT_PWR_MODE) !=
				CHANNEL_STATE_ENABLE)
			continue;

		bw_val = wlan_reg_get_bw_value(ch_params.ch_width);
		if (!ch_params.mhz_freq_seg0)
			continue;
		if (bw_val < wlan_reg_get_bw_value(ch_width))
			continue;
		if (ch_params.mhz_freq_seg1)
			center_freq = ch_params.mhz_freq_seg1;
		else
			center_freq = ch_params.mhz_freq_seg0;

		bw_start = center_freq - bw_val / 2 + 10;
		bw_end = center_freq + bw_val / 2 - 10;
		for (j = 0; j < unsafe_chan_cnt; j++)
			if (unsafe_chan[j] >= bw_start &&
			    unsafe_chan[j] <= bw_end)
				break;

		if (j < unsafe_chan_cnt) {
			sap_debug("ch_freq %d bw %d bw start %d, bw end %d unsafe %d",
				  ch_freq_list[i], bw_val, bw_start, bw_end,
				  unsafe_chan[j]);
			continue;
		}
		sap_debug("ch_freq %d bw %d bw start %d, bw end %d",
			  ch_freq_list[i], bw_val, bw_start, bw_end);
		/* found freq/bw pair which is safe for used as sap channel
		 * avoidance csa target channel/bandwidth.
		 */
		*selected_freq = ch_freq_list[i];
		*selected_ch_width = ch_params.ch_width;
		sap_debug("selected freq %d bw %d", *selected_freq,
			  *selected_ch_width);

		return QDF_STATUS_SUCCESS;
	}

	ch_width = wlan_reg_get_next_lower_bandwidth(ch_width);
	if (!(wlan_reg_get_bw_value(ch_width) < 20 ||
	      ch_width == CH_WIDTH_INVALID))
		goto next_lower_bw;

	return QDF_STATUS_E_INVAL;
}

/**
 * wlansap_get_safe_channel() - Get safe channel from current regulatory
 * @sap_ctx: Pointer to SAP context
 * @ch_width: selected channel bandwdith
 * @pref_band: Preferred channel band for sap
 *
 * This function is used to get safe channel from current regulatory valid
 * channels to restart SAP if failed to get safe channel from PCL.
 *
 * Return: Chan freq num to restart SAP in case of success. In case of any
 * failure, the channel number returned is zero.
 */
static uint32_t
wlansap_get_safe_channel(struct sap_context *sap_ctx,
			 enum phy_ch_width *ch_width,
			 enum reg_wifi_band pref_band)
{
	struct mac_context *mac;
	uint32_t pcl_freqs[NUM_CHANNELS];
	QDF_STATUS status;
	mac_handle_t mac_handle;
	uint32_t pcl_len = 0, i;
	uint32_t selected_freq;
	enum policy_mgr_con_mode mode;
	uint32_t first_valid_dfs_5g_freq = 0;
	uint32_t first_valid_non_dfs_5g_freq = 0;
	uint32_t first_valid_6g_freq = 0;
	uint32_t first_valid_6g_psc_freq = 0;
	uint8_t vdev_id;

	if (!sap_ctx) {
		sap_err("NULL parameter");
		return INVALID_CHANNEL_ID;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return INVALID_CHANNEL_ID;
	}
	mac_handle = MAC_HANDLE(mac);

	mode = policy_mgr_qdf_opmode_to_pm_con_mode(mac->psoc,
						    QDF_SAP_MODE,
						    sap_ctx->vdev_id);
	/* get the channel list for current domain */
	status = policy_mgr_get_valid_chans(mac->psoc, pcl_freqs, &pcl_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("Error in getting valid channels");
		return INVALID_CHANNEL_ID;
	}

	status = wlansap_filter_ch_based_acs(sap_ctx, pcl_freqs, &pcl_len);

	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("failed to filter ch from acs %d", status);
		return INVALID_CHANNEL_ID;
	}

	if (pcl_len) {
		vdev_id = sap_ctx->vdev_id;
		status = policy_mgr_get_valid_chans_from_range(mac->psoc,
							       pcl_freqs,
							       &pcl_len,
							       mode, vdev_id);
		if (QDF_IS_STATUS_ERROR(status) || !pcl_len) {
			sap_err("failed to get valid channel: %d len %d",
				status, pcl_len);
			return INVALID_CHANNEL_ID;
		}

		hdd_remove_vlp_depriority_channels(mac->pdev,
						   (uint16_t *)pcl_freqs,
						   &pcl_len);
		status =
		wlansap_select_chan_with_best_bandwidth(sap_ctx,
							pcl_freqs,
							pcl_len,
							&selected_freq,
							ch_width);
		if (QDF_IS_STATUS_SUCCESS(status))
			return selected_freq;

		for (i = 0; i < pcl_len; i++) {
			if (!first_valid_non_dfs_5g_freq &&
			    wlan_reg_is_5ghz_ch_freq(pcl_freqs[i])) {
				if (!wlan_reg_is_dfs_in_secondary_list_for_freq(
							mac->pdev,
							pcl_freqs[i])) {
					first_valid_non_dfs_5g_freq = pcl_freqs[i];
					if (pref_band == REG_BAND_5G)
						break;
				} else if (!first_valid_dfs_5g_freq) {
					first_valid_dfs_5g_freq = pcl_freqs[i];
				}
			}
			if (wlan_reg_is_6ghz_chan_freq(pcl_freqs[i])) {
				if (!first_valid_6g_freq)
					first_valid_6g_freq = pcl_freqs[i];
				if (wlan_reg_is_6ghz_psc_chan_freq(pcl_freqs[i]) &&
				    !first_valid_6g_psc_freq)
					first_valid_6g_psc_freq = pcl_freqs[i];
			}
		}

		selected_freq = pcl_freqs[0];

		if (pref_band == REG_BAND_6G) {
			if (first_valid_6g_psc_freq)
				selected_freq = first_valid_6g_psc_freq;
			else if (first_valid_6g_freq)
				selected_freq = first_valid_6g_freq;
			else if (first_valid_non_dfs_5g_freq)
				selected_freq = first_valid_non_dfs_5g_freq;
			else if (first_valid_dfs_5g_freq)
				selected_freq = first_valid_dfs_5g_freq;
		} else if (pref_band == REG_BAND_5G) {
			if (first_valid_non_dfs_5g_freq)
				selected_freq = first_valid_non_dfs_5g_freq;
			else if (first_valid_dfs_5g_freq)
				selected_freq = first_valid_dfs_5g_freq;
		}

		sap_debug("select %d from valid channel list, pref band = %d",
			  selected_freq, pref_band);
		return selected_freq;
	}

	return INVALID_CHANNEL_ID;
}
#else
/**
 * wlansap_select_chan_with_best_bandwidth() - Select channel with
 * max possible band width
 * @sap_ctx: sap context
 * @ch_freq_list: candidate channel frequency list
 * @ch_cnt: count of channel frequency in list
 * @selected_freq: selected channel frequency
 * @selected_ch_width: selected channel width
 *
 * Return: QDF_STATUS_SUCCESS if better channel selected
 */
static inline QDF_STATUS
wlansap_select_chan_with_best_bandwidth(struct sap_context *sap_ctx,
					uint32_t *ch_freq_list,
					uint32_t ch_cnt,
					uint32_t *selected_freq,
					enum phy_ch_width *selected_ch_width)
{
	return QDF_STATUS_E_NOSUPPORT;
}

/**
 * wlansap_get_safe_channel() - Get safe channel from current regulatory
 * @sap_ctx: Pointer to SAP context
 * @ch_width: selected channel width
 * @pref_band: Preferred channel band for sap
 *
 * This function is used to get safe channel from current regulatory valid
 * channels to restart SAP if failed to get safe channel from PCL.
 *
 * Return: Channel number to restart SAP in case of success. In case of any
 * failure, the channel number returned is zero.
 */
static uint8_t
wlansap_get_safe_channel(struct sap_context *sap_ctx,
			 enum phy_ch_width *ch_width,
			 enum reg_wifi_band pref_band)
{
	return 0;
}
#endif

uint32_t
wlansap_get_safe_channel_from_pcl_and_acs_range(struct sap_context *sap_ctx,
						enum phy_ch_width *ch_width)
{
	struct mac_context *mac;
	struct sir_pcl_list pcl = {0};
	uint32_t pcl_freqs[NUM_CHANNELS] = {0};
	uint32_t select_freq;
	QDF_STATUS status;
	mac_handle_t mac_handle;
	uint32_t pcl_len = 0;
	enum policy_mgr_con_mode mode;

	if (!sap_ctx) {
		sap_err("NULL parameter");
		return INVALID_CHANNEL_ID;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return INVALID_CHANNEL_ID;
	}
	mac_handle = MAC_HANDLE(mac);

	if (policy_mgr_get_connection_count(mac->psoc) == 1) {
		sap_debug("only SAP present return best channel from ACS list");
		return wlansap_get_safe_channel(sap_ctx, ch_width, REG_BAND_6G);
	}

	mode = policy_mgr_qdf_opmode_to_pm_con_mode(mac->psoc, QDF_SAP_MODE,
						    sap_ctx->vdev_id);

	status =
		policy_mgr_get_pcl_for_scc_in_same_mode(mac->psoc, mode,
							pcl_freqs, &pcl_len,
							pcl.weight_list,
							QDF_ARRAY_SIZE(pcl.weight_list),
							sap_ctx->sessionId);

	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("Get PCL failed");
		return INVALID_CHANNEL_ID;
	}

	if (pcl_len) {
		hdd_remove_vlp_depriority_channels(mac->pdev,
						   (uint16_t *)pcl_freqs,
						   &pcl_len);
		status = wlansap_filter_ch_based_acs(sap_ctx, pcl_freqs,
						     &pcl_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			sap_err("failed to filter ch from acs %d", status);
			return INVALID_CHANNEL_ID;
		}

		if (wlansap_select_chan_with_best_bandwidth(sap_ctx,
							    pcl_freqs,
							    pcl_len,
							    &select_freq,
							    ch_width) ==
		    QDF_STATUS_SUCCESS)
			return select_freq;

		if (pcl_len) {
			sap_debug("select %d from valid ch freq list",
				  pcl_freqs[0]);
			return pcl_freqs[0];
		}
		sap_debug("no safe channel from PCL found in ACS range");
	} else {
		sap_debug("pcl length is zero!");
	}

	/*
	 * In some scenarios, like hw dbs disabled, sap+sap case, if operating
	 * channel is unsafe channel, the pcl may be empty, instead of return,
	 * try to choose a safe channel from acs range.
	 */
	return wlansap_get_safe_channel(sap_ctx, ch_width, REG_BAND_6G);
}

static uint32_t wlansap_get_2g_first_safe_chan_freq(struct sap_context *sap_ctx)
{
	uint32_t i;
	uint32_t freq;
	enum channel_state state;
	struct regulatory_channel *cur_chan_list;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	uint32_t *acs_freq_list;
	uint8_t acs_list_count;

	pdev = sap_ctx->vdev->vdev_objmgr.wlan_pdev;
	psoc = pdev->pdev_objmgr.wlan_psoc;

	cur_chan_list = qdf_mem_malloc(NUM_CHANNELS *
			sizeof(struct regulatory_channel));
	if (!cur_chan_list)
		return TWOG_CHAN_6_IN_MHZ;

	if (wlan_reg_get_current_chan_list(pdev, cur_chan_list) !=
					   QDF_STATUS_SUCCESS) {
		freq = TWOG_CHAN_6_IN_MHZ;
		goto err;
	}

	acs_freq_list = sap_ctx->acs_cfg->master_freq_list;
	acs_list_count = sap_ctx->acs_cfg->master_ch_list_count;
	for (i = 0; i < NUM_CHANNELS; i++) {
		freq = cur_chan_list[i].center_freq;
		state = wlan_reg_get_channel_state_for_pwrmode(
							pdev, freq,
							REG_CURRENT_PWR_MODE);
		if (state != CHANNEL_STATE_DISABLE &&
		    state != CHANNEL_STATE_PASSIVE &&
		    state != CHANNEL_STATE_INVALID &&
		    wlan_reg_is_24ghz_ch_freq(freq) &&
		    policy_mgr_is_safe_channel(psoc, freq) &&
		    wlansap_is_channel_present_in_acs_list(freq,
							   acs_freq_list,
							   acs_list_count)) {
			sap_debug("found a 2g channel: %d", freq);
			goto err;
		}
	}

	freq = TWOG_CHAN_6_IN_MHZ;
err:
	qdf_mem_free(cur_chan_list);
	return freq;
}

uint32_t wlansap_get_safe_channel_from_pcl_for_sap(struct sap_context *sap_ctx)
{
	struct wlan_objmgr_pdev *pdev;
	struct mac_context *mac;
	struct sir_pcl_list pcl = {0};
	uint32_t pcl_freqs[NUM_CHANNELS] = {0};
	QDF_STATUS status;
	uint32_t pcl_len = 0;
	enum policy_mgr_con_mode mode;

	if (!sap_ctx) {
		sap_err("NULL parameter");
		return INVALID_CHANNEL_ID;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return INVALID_CHANNEL_ID;
	}

	pdev = sap_ctx->vdev->vdev_objmgr.wlan_pdev;
	if (!pdev) {
		sap_err("NULL pdev");
	}

	mode = policy_mgr_qdf_opmode_to_pm_con_mode(mac->psoc, QDF_SAP_MODE,
						    sap_ctx->vdev_id);

	status = policy_mgr_get_pcl_for_vdev_id(mac->psoc, mode,
						pcl_freqs, &pcl_len,
						pcl.weight_list,
						QDF_ARRAY_SIZE(pcl.weight_list),
						sap_ctx->sessionId);

	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("Get PCL failed");
		return INVALID_CHANNEL_ID;
	}

	if (pcl_len) {
		status = policy_mgr_filter_passive_ch(pdev, pcl_freqs,
						      &pcl_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			sap_err("failed to filter passive channels");
			return INVALID_CHANNEL_ID;
		}

		if (pcl_len) {
			sap_debug("select %d from valid ch freq list",
				  pcl_freqs[0]);
			return pcl_freqs[0];
		}
		sap_debug("no active channels found in PCL");
	} else {
		sap_debug("pcl length is zero!");
	}

	if (mode == PM_LL_LT_SAP_MODE)
		return INVALID_CHANNEL_ID;

	return wlansap_get_2g_first_safe_chan_freq(sap_ctx);
}

int wlansap_update_sap_chan_list(struct sap_config *sap_config,
				 qdf_freq_t *freq_list, uint16_t count)
{
	uint32_t *acs_cfg_freq_list;
	uint32_t *master_freq_list;
	uint32_t i;
	bool old_acs_2g_only = false, acs_2g_only_new = true;

	acs_cfg_freq_list = qdf_mem_malloc(count * sizeof(uint32_t));
	if (!acs_cfg_freq_list)
		return -ENOMEM;
	if (sap_config->acs_cfg.ch_list_count) {
		qdf_mem_free(sap_config->acs_cfg.freq_list);
		sap_config->acs_cfg.freq_list = NULL;
		sap_config->acs_cfg.ch_list_count = 0;
	}
	sap_config->acs_cfg.freq_list = acs_cfg_freq_list;

	master_freq_list = qdf_mem_malloc(count * sizeof(uint32_t));
	if (!master_freq_list)
		return -ENOMEM;

	if (sap_config->acs_cfg.master_ch_list_count) {
		old_acs_2g_only = true;
		for (i = 0; i < sap_config->acs_cfg.master_ch_list_count; i++)
			if (sap_config->acs_cfg.master_freq_list &&
			    !WLAN_REG_IS_24GHZ_CH_FREQ(
				sap_config->acs_cfg.master_freq_list[i])) {
				old_acs_2g_only = false;
				break;
			}
		qdf_mem_free(sap_config->acs_cfg.master_freq_list);
		sap_config->acs_cfg.master_freq_list = NULL;
		sap_config->acs_cfg.master_ch_list_count = 0;
	}
	sap_config->acs_cfg.master_freq_list = master_freq_list;

	qdf_mem_copy(sap_config->acs_cfg.freq_list, freq_list,
		     sizeof(freq_list[0]) * count);
	qdf_mem_copy(sap_config->acs_cfg.master_freq_list, freq_list,
		     sizeof(freq_list[0]) * count);
	sap_config->acs_cfg.master_ch_list_count = count;
	sap_config->acs_cfg.ch_list_count = count;
	for (i = 0; i < sap_config->acs_cfg.master_ch_list_count; i++)
		if (sap_config->acs_cfg.master_freq_list &&
		    !WLAN_REG_IS_24GHZ_CH_FREQ(
				sap_config->acs_cfg.master_freq_list[i])) {
			acs_2g_only_new = false;
			break;
		}
	/* If SAP initially started on world mode, the SAP ACS master channel
	 * list will only contain 2.4 GHz channels. When country code changed
	 * from world mode to non world mode, the master_ch_list will
	 * be updated by this API from userspace and the list will include
	 * 2.4 GHz + 5/6 GHz normally. If this transition happens, we set
	 * master_ch_list_updated flag. And later only if the flag is set,
	 * wlansap_get_chan_band_restrict will be invoked to select new SAP
	 * channel frequency based on PCL.
	 */
	sap_config->acs_cfg.master_ch_list_updated =
			old_acs_2g_only && !acs_2g_only_new;
	sap_dump_acs_channel(&sap_config->acs_cfg);

	return 0;
}

/**
 * wlansap_get_valid_freq() - To get valid freq for sap csa
 * @psoc: psoc object
 * @sap_ctx: sap context
 * @freq: pointer to valid freq
 *
 * If sap master channel list is updated from 2G only to 2G+5G/6G,
 * this API will find new SAP channel frequency likely 5G/6G to have
 * better performance for SAP. This happens when SAP started in
 * world mode, later country code change to non world mode.
 *
 * Return: None
 */
static
void wlansap_get_valid_freq(struct wlan_objmgr_psoc *psoc,
			    struct sap_context *sap_ctx,
			    qdf_freq_t *freq)
{
	uint8_t i, j;
	struct mac_context *mac;
	struct sir_pcl_list pcl = {0};
	uint32_t *pcl_freqs;
	QDF_STATUS status;
	uint32_t pcl_len = 0;

	if (!sap_ctx->acs_cfg || !sap_ctx->acs_cfg->master_ch_list_count)
		return;

	if (!sap_ctx->acs_cfg->master_ch_list_updated)
		return;

	sap_ctx->acs_cfg->master_ch_list_updated = false;

	pcl_freqs =  qdf_mem_malloc(NUM_CHANNELS * sizeof(uint32_t));
	if (!pcl_freqs)
		return;

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		goto done;
	}
	status = policy_mgr_reset_sap_mandatory_channels(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("failed to reset mandatory channels");
		goto done;
	}
	status = policy_mgr_get_pcl_for_vdev_id(mac->psoc, PM_SAP_MODE,
						pcl_freqs, &pcl_len,
						pcl.weight_list,
						NUM_CHANNELS,
						sap_ctx->sessionId);

	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("Get PCL failed for session %d", sap_ctx->sessionId);
		goto done;
	}
	for (i = 0; i < pcl_len; i++) {
		for (j = 0; j < sap_ctx->acs_cfg->master_ch_list_count; j++) {
			/*
			 * To keep valid freq list order same as pcl weightage
			 * order pcl list index is compared with all the freq
			 * provided by set wifi config.
			 */
			if (sap_ctx->acs_cfg->master_freq_list[j] ==
			    pcl_freqs[i]) {
				*freq = pcl_freqs[i];
				goto done;
			}
		}
	}
done:
	qdf_mem_free(pcl_freqs);
	pcl_freqs = NULL;
}

qdf_freq_t wlansap_get_chan_band_restrict(struct sap_context *sap_ctx,
					  enum sap_csa_reason_code *csa_reason)
{
	uint32_t restart_freq;
	uint16_t intf_ch_freq;
	uint32_t phy_mode;
	struct mac_context *mac;
	uint8_t cc_mode;
	uint8_t vdev_id;
	enum reg_wifi_band sap_band;
	enum band_info band;
	bool sta_sap_scc_on_indoor_channel;
	qdf_freq_t freq = 0;
	struct ch_params ch_params = {0};

	if (!sap_ctx) {
		sap_err("sap_ctx NULL parameter");
		return 0;
	}

	if (!csa_reason) {
		sap_err("csa_reason is NULL");
		return 0;
	}

	if (cds_is_driver_recovering())
		return 0;

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac)
		return 0;

	if (ucfg_reg_get_band(mac->pdev, &band) != QDF_STATUS_SUCCESS) {
		sap_err("Failed to get current band config");
		return 0;
	}
	sta_sap_scc_on_indoor_channel =
		policy_mgr_get_sta_sap_scc_allowed_on_indoor_chnl(mac->psoc);
	sap_band = wlan_reg_freq_to_band(sap_ctx->chan_freq);

	sap_debug("SAP/Go current band: %d, pdev band capability: %d, cur freq %d (is valid %d), prev freq %d (is valid %d)",
		  sap_band, band, sap_ctx->chan_freq,
		  wlan_reg_is_enable_in_secondary_list_for_freq(mac->pdev,
							sap_ctx->chan_freq),
		  sap_ctx->chan_freq_before_switch_band,
		  wlan_reg_is_enable_in_secondary_list_for_freq(mac->pdev,
					sap_ctx->chan_freq_before_switch_band));

	if (sap_band == REG_BAND_5G && band == BIT(REG_BAND_2G)) {
		sap_ctx->chan_freq_before_switch_band = sap_ctx->chan_freq;
		sap_ctx->chan_width_before_switch_band =
			sap_ctx->ch_params.ch_width;
		sap_debug("Save chan info before switch: %d, width: %d",
			  sap_ctx->chan_freq, sap_ctx->ch_params.ch_width);
		restart_freq = wlansap_get_2g_first_safe_chan_freq(sap_ctx);
		if (restart_freq == 0) {
			sap_debug("use default chan 6");
			restart_freq = TWOG_CHAN_6_IN_MHZ;
		}
		*csa_reason = CSA_REASON_BAND_RESTRICTED;
	} else if (sap_band == REG_BAND_2G && (band & BIT(REG_BAND_5G)) &&
		   sap_ctx->chan_freq_before_switch_band) {
		if (!wlan_reg_is_disable_in_secondary_list_for_freq(
				mac->pdev,
				sap_ctx->chan_freq_before_switch_band)) {
			restart_freq = sap_ctx->chan_freq_before_switch_band;
			sap_debug("Restore chan freq: %d", restart_freq);
			*csa_reason = CSA_REASON_BAND_RESTRICTED;
		} else {
			enum reg_wifi_band pref_band;

			pref_band = wlan_reg_freq_to_band(
					sap_ctx->chan_freq_before_switch_band);
			restart_freq =
				policy_mgr_get_alternate_channel_for_sap(
							mac->psoc,
							sap_ctx->sessionId,
							sap_ctx->chan_freq,
							pref_band);
			if (restart_freq) {
				sap_debug("restart SAP on freq %d",
					  restart_freq);
				*csa_reason = CSA_REASON_BAND_RESTRICTED;
			} else {
				sap_debug("Did not get valid freq for band %d remain on same channel",
					  pref_band);
				return 0;
			}
		}
	} else if (sap_ctx->acs_cfg &&
			sap_ctx->acs_cfg->master_ch_list_updated) {
		/*
		 * We are sure the master channel list has been changed from
		 * 2.4 GHz only(world reg) to 2.4 GHz + 5/6 GHz(non world reg),
		 * SAP could now choose a better/higher frequency.
		 */
		wlansap_get_valid_freq(mac->psoc, sap_ctx, &freq);
		if (!freq)
			return 0;

		restart_freq = freq;
		sap_debug("restart SAP on freq %d", restart_freq);
		*csa_reason = CSA_REASON_BAND_RESTRICTED;
	} else if (wlan_reg_is_disable_in_secondary_list_for_freq(
							mac->pdev,
							sap_ctx->chan_freq) &&
		   !utils_dfs_is_freq_in_nol(mac->pdev, sap_ctx->chan_freq)) {
		sap_debug("channel is disabled");
		*csa_reason = CSA_REASON_CHAN_DISABLED;
		return wlansap_get_safe_channel_from_pcl_and_acs_range(sap_ctx,
								       NULL);
	} else if (wlan_reg_is_passive_for_freq(mac->pdev,
						sap_ctx->chan_freq))  {
		sap_ctx->chan_freq_before_switch_band = sap_ctx->chan_freq;
		sap_ctx->chan_width_before_switch_band =
			sap_ctx->ch_params.ch_width;
		sap_debug("Save chan info before switch: %d, width: %d",
			  sap_ctx->chan_freq, sap_ctx->ch_params.ch_width);
		sap_debug("channel is passive");
		*csa_reason = CSA_REASON_CHAN_PASSIVE;
		return wlansap_get_safe_channel_from_pcl_for_sap(sap_ctx);
	} else if (!policy_mgr_is_sap_freq_allowed(mac->psoc,
			wlan_vdev_mlme_get_opmode(sap_ctx->vdev),
			sap_ctx->chan_freq)) {
		sap_debug("channel is unsafe");
		*csa_reason = CSA_REASON_UNSAFE_CHANNEL;
		return wlansap_get_safe_channel_from_pcl_and_acs_range(sap_ctx,
								       NULL);
	} else if (sap_band == REG_BAND_6G &&
		   wlan_reg_get_keep_6ghz_sta_cli_connection(mac->pdev)) {
		ch_params.ch_width = sap_ctx->ch_params.ch_width;
		wlan_reg_set_channel_params_for_pwrmode(mac->pdev,
						sap_ctx->chan_freq,
						0, &ch_params,
						REG_CURRENT_PWR_MODE);
		if (sap_ctx->ch_params.ch_width != ch_params.ch_width) {
			sap_debug("Bonded 6GHz channels are disabled");
			*csa_reason = CSA_REASON_BAND_RESTRICTED;
			return wlansap_get_safe_channel_from_pcl_and_acs_range(
								sap_ctx, NULL);
		} else {
			sap_debug("No need switch SAP/Go channel");
			return sap_ctx->chan_freq;
		}
	} else {
		sap_debug("No need switch SAP/Go channel");
		return sap_ctx->chan_freq;
	}
	cc_mode = sap_ctx->cc_switch_mode;
	phy_mode = sap_ctx->phyMode;
	vdev_id = wlan_vdev_get_id(sap_ctx->vdev);
	intf_ch_freq = sme_check_concurrent_channel_overlap(
						       MAC_HANDLE(mac),
						       restart_freq,
						       phy_mode,
						       cc_mode, vdev_id);
	if (intf_ch_freq)
		restart_freq = intf_ch_freq;
	if (restart_freq == sap_ctx->chan_freq)
		restart_freq = 0;

	if (restart_freq)
		sap_debug("vdev: %d, CSA target freq: %d", vdev_id,
			  restart_freq);

	return restart_freq;
}

static inline bool
wlansap_ch_in_avoid_ranges(uint32_t ch_freq,
			   struct pld_ch_avoid_ind_type *ch_avoid_ranges)
{
	uint32_t i;

	for (i = 0; i < ch_avoid_ranges->ch_avoid_range_cnt; i++) {
		if (ch_freq >=
			ch_avoid_ranges->avoid_freq_range[i].start_freq &&
		    ch_freq <=
			ch_avoid_ranges->avoid_freq_range[i].end_freq)
			return true;
	}

	return false;
}

bool wlansap_filter_vendor_unsafe_ch_freq(
	struct sap_context *sap_context, struct sap_config *sap_config)
{
	struct pld_ch_avoid_ind_type ch_avoid_ranges;
	uint32_t i, j;
	int ret;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	struct mac_context *mac;
	uint32_t count;

	if (!qdf_ctx)
		return false;
	mac = sap_get_mac_context();
	if (!mac)
		return false;

	count = policy_mgr_get_sap_mode_count(mac->psoc, NULL);

	if (count != policy_mgr_get_connection_count(mac->psoc))
		return false;

	ch_avoid_ranges.ch_avoid_range_cnt = 0;
	ret = pld_get_wlan_unsafe_channel_sap(qdf_ctx->dev, &ch_avoid_ranges);
	if (ret) {
		sap_debug("failed to get vendor unsafe ch range, ret %d", ret);
		return false;
	}
	if (!ch_avoid_ranges.ch_avoid_range_cnt)
		return false;
	for (i = 0; i < ch_avoid_ranges.ch_avoid_range_cnt; i++) {
		sap_debug("vendor unsafe range[%d] %d %d", i,
			  ch_avoid_ranges.avoid_freq_range[i].start_freq,
			  ch_avoid_ranges.avoid_freq_range[i].end_freq);
	}
	for (i = 0, j = 0; i < sap_config->acs_cfg.ch_list_count; i++) {
		if (!wlansap_ch_in_avoid_ranges(
				sap_config->acs_cfg.freq_list[i],
				&ch_avoid_ranges))
			sap_config->acs_cfg.freq_list[j++] =
				sap_config->acs_cfg.freq_list[i];
	}
	sap_config->acs_cfg.ch_list_count = j;

	return true;
}

#ifdef DCS_INTERFERENCE_DETECTION
QDF_STATUS wlansap_dcs_set_vdev_wlan_interference_mitigation(
				struct sap_context *sap_context,
				bool wlan_interference_mitigation_enable)
{
	struct mac_context *mac;

	if (!sap_context) {
		sap_err("Invalid SAP context pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	mac->sap.dcs_info.
		wlan_interference_mitigation_enable[sap_context->sessionId] =
					wlan_interference_mitigation_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlansap_dcs_set_wlan_interference_mitigation_on_band(
					struct sap_context *sap_context,
					struct sap_config *sap_cfg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool wlan_interference_mitigation_enable = false;

	if (!WLAN_REG_IS_24GHZ_CH_FREQ(sap_cfg->acs_cfg.pri_ch_freq))
		wlan_interference_mitigation_enable = true;

	status = wlansap_dcs_set_vdev_wlan_interference_mitigation(
					sap_context,
					wlan_interference_mitigation_enable);
	return status;
}

QDF_STATUS wlansap_dcs_set_vdev_starting(struct sap_context *sap_context,
					 bool vdev_starting)
{
	struct mac_context *mac;

	if (!sap_context) {
		sap_err("Invalid SAP context pointer");
		return QDF_STATUS_E_FAULT;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_FAULT;
	}

	mac->sap.dcs_info.is_vdev_starting[sap_context->sessionId] =
							vdev_starting;

	return QDF_STATUS_SUCCESS;
}

bool wlansap_dcs_is_wlan_interference_mitigation_enabled(
					struct sap_context *sap_context)
{
	struct mac_context *mac;

	if (!sap_context) {
		sap_err("Invalid SAP context pointer");
		return false;
	}

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return false;
	}

	return mac->sap.dcs_info.
		wlan_interference_mitigation_enable[sap_context->sessionId];
}

qdf_freq_t wlansap_dcs_get_freq(struct sap_context *sap_context)
{
	if (!sap_context) {
		sap_err("Invalid SAP context pointer");
		return false;
	}

	return sap_context->dcs_ch_freq;
}

void wlansap_set_acs_ch_freq(struct sap_context *sap_context,
			     qdf_freq_t ch_freq)
{
	if (!sap_context) {
		sap_err("Invalid sap_debug");
		return;
	}

	if (sap_context->fsm_state == SAP_STARTED) {
		sap_context->dcs_ch_freq = ch_freq;
		sap_debug("Selecting DCS freq %d", sap_context->dcs_ch_freq);
	} else {
		sap_context->chan_freq = ch_freq;
		sap_debug("Selecting ACS freq %d", sap_context->chan_freq);
	}
}
#else
void wlansap_set_acs_ch_freq(struct sap_context *sap_context,
			     qdf_freq_t ch_freq)
{
	if (!sap_context) {
		sap_err("Invalid sap_debug");
		return;
	}

	sap_context->chan_freq = ch_freq;
	sap_debug("Selecting ACS freq %d", sap_context->chan_freq);
}
#endif

#ifdef WLAN_FEATURE_11BE
bool sap_phymode_is_eht(eCsrPhyMode phymode)
{
	return CSR_IS_DOT11_PHY_MODE_11BE(phymode) ||
	       CSR_IS_DOT11_PHY_MODE_11BE_ONLY(phymode);
}

bool sap_acs_is_puncture_applicable(struct sap_acs_cfg *acs_cfg)
{
	bool is_eht_bw_80 = false;

	if (!acs_cfg) {
		sap_err("Invalid parameters");
		return is_eht_bw_80;
	}

	switch (acs_cfg->ch_width) {
	case CH_WIDTH_80MHZ:
	case CH_WIDTH_80P80MHZ:
	case CH_WIDTH_160MHZ:
	case CH_WIDTH_320MHZ:
		is_eht_bw_80 = acs_cfg->is_eht_enabled;
		break;
	default:
		break;
	}

	return is_eht_bw_80;
}

void sap_acs_set_puncture_support(struct sap_context *sap_ctx,
				  struct ch_params *ch_params)
{
	if (!sap_ctx || !ch_params) {
		sap_err("Invalid parameters");
		return;
	}

	if (sap_acs_is_puncture_applicable(sap_ctx->acs_cfg))
		ch_params->is_create_punc_bitmap = true;
}
#endif

void wlansap_update_ll_lt_sap_acs_result(struct sap_context *sap_ctx,
					 qdf_freq_t last_acs_freq)
{
	struct mac_context *mac;

	mac = sap_get_mac_context();
	if (!mac) {
		sap_err("Invalid MAC context");
		return;
	}

	if (!sap_ctx) {
		sap_err("Invalid sap context");
		return;
	}

	wlansap_set_acs_ch_freq(sap_ctx, last_acs_freq);
	sap_ctx->acs_cfg->pri_ch_freq = last_acs_freq;
	sap_ctx->acs_cfg->ht_sec_ch_freq = 0;
}

QDF_STATUS wlansap_sort_channel_list(uint8_t vdev_id, qdf_list_t *list,
				     struct sap_sel_ch_info *ch_info)
{
	struct mac_context *mac_ctx;
	QDF_STATUS status;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = sap_sort_channel_list(mac_ctx, vdev_id, list,
				       ch_info, NULL, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("vdev %d failed to sort sap channel list",
			vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void wlansap_free_chan_info(struct sap_sel_ch_info *ch_param)
{
	sap_chan_sel_exit(ch_param);
}

QDF_STATUS wlansap_get_user_config_acs_ch_list(uint8_t vdev_id,
					       struct scan_filter *filter)
{
	struct mac_context *mac_ctx;
	struct sap_context *sap_ctx;
	uint8_t ch_count = 0;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (vdev_id >= SAP_MAX_NUM_SESSION)
		return QDF_STATUS_E_INVAL;

	sap_ctx = mac_ctx->sap.sapCtxList[vdev_id].sap_context;

	if (!sap_ctx) {
		sap_err("vdev %d sap_ctx is NULL", vdev_id);
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!sap_ctx->acs_cfg) {
		sap_err("vdev %d acs_cfg is NULL", vdev_id);
		return QDF_STATUS_E_NULL_VALUE;
	}

	ch_count = sap_ctx->acs_cfg->master_ch_list_count;

	if (!ch_count || ch_count > NUM_CHANNELS)
		return QDF_STATUS_E_INVAL;

	filter->num_of_channels = ch_count;
	qdf_mem_copy(filter->chan_freq_list, sap_ctx->acs_cfg->master_freq_list,
		     filter->num_of_channels *
		     sizeof(filter->chan_freq_list[0]));

	return QDF_STATUS_SUCCESS;
}
