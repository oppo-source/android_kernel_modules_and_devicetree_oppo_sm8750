/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef __WLAN_POLICY_MGR_UCFG
#define __WLAN_POLICY_MGR_UCFG
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_global_obj.h"
#include "qdf_status.h"
#include "wlan_policy_mgr_public_struct.h"

/**
 * ucfg_policy_mgr_psoc_open() - This API sets CFGs to policy manager context
 * @psoc: pointer to psoc
 *
 * This API pulls policy manager's context from PSOC and initialize the CFG
 * structure of policy manager.
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_policy_mgr_psoc_close() - This API resets CFGs for policy manager ctx
 * @psoc: pointer to psoc
 *
 * This API pulls policy manager's context from PSOC and resets the CFG
 * structure of policy manager.
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
void ucfg_policy_mgr_psoc_close(struct wlan_objmgr_psoc *psoc);
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * ucfg_policy_mgr_get_mcc_scc_switch() - To mcc to scc switch setting from INI
 * @psoc: pointer to psoc
 * @mcc_scc_switch: value to be filled
 *
 * This API pulls mcc to scc switch setting which is given as part of INI and
 * stored in policy manager's CFGs.
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_mcc_scc_switch(struct wlan_objmgr_psoc *psoc,
					      uint8_t *mcc_scc_switch);
#else
static inline
QDF_STATUS ucfg_policy_mgr_get_mcc_scc_switch(struct wlan_objmgr_psoc *psoc,
					      uint8_t *mcc_scc_switch)
{
	return QDF_STATUS_SUCCESS;
}
#endif //FEATURE_WLAN_MCC_TO_SCC_SWITCH

/**
 * ucfg_policy_mgr_get_radio_combinations() - Query the supported radio
 * combinations
 * @psoc: soc object
 * @comb: combination buffer
 * @comb_max: max combination number can be saved to comb buffer
 * @comb_num: returned combination number
 *
 * This function returns the radio combination information supported by target.
 *
 * Return: QDF_STATUS_SUCCESS if query successfully
 */
QDF_STATUS
ucfg_policy_mgr_get_radio_combinations(struct wlan_objmgr_psoc *psoc,
				       struct radio_combination *comb,
				       uint32_t comb_max,
				       uint32_t *comb_num);

/**
 * ucfg_policy_mgr_get_sys_pref() - to get system preference
 * @psoc: pointer to psoc
 * @sys_pref: value to be filled
 *
 * This API pulls the system preference for policy manager to provide
 * PCL
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_sys_pref(struct wlan_objmgr_psoc *psoc,
					uint8_t *sys_pref);
/**
 * ucfg_policy_mgr_set_sys_pref() - to set system preference
 * @psoc: pointer to psoc
 * @sys_pref: value to be applied as new INI setting
 *
 * This API is meant to override original INI setting for system pref
 * with new value which is used by policy manager to provide PCL
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_set_sys_pref(struct wlan_objmgr_psoc *psoc,
					uint8_t sys_pref);

/**
 * ucfg_policy_mgr_get_conc_rule1() - to find out if conc rule1 is enabled
 * @psoc: pointer to psoc
 * @conc_rule1: value to be filled
 *
 * This API is used to find out if conc rule-1 is enabled by user
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_conc_rule1(struct wlan_objmgr_psoc *psoc,
						uint8_t *conc_rule1);
/**
 * ucfg_policy_mgr_get_conc_rule2() - to find out if conc rule2 is enabled
 * @psoc: pointer to psoc
 * @conc_rule2: value to be filled
 *
 * This API is used to find out if conc rule-2 is enabled by user
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_conc_rule2(struct wlan_objmgr_psoc *psoc,
						uint8_t *conc_rule2);

/**
 * ucfg_policy_mgr_get_chnl_select_plcy() - to get channel selection policy
 * @psoc: pointer to psoc
 * @chnl_select_plcy: value to be filled
 *
 * This API is used to find out which channel selection policy has been
 * configured
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_chnl_select_plcy(struct wlan_objmgr_psoc *psoc,
						uint32_t *chnl_select_plcy);
/**
 * ucfg_policy_mgr_get_mcc_adaptive_sch() - to get mcc adaptive scheduler
 * @psoc: pointer to psoc
 * @enable_mcc_adaptive_sch: value to be filled
 *
 * This API is used to find out if mcc adaptive scheduler enabled or disabled
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_mcc_adaptive_sch(struct wlan_objmgr_psoc *psoc,
				     bool *enable_mcc_adaptive_sch);

/**
 * ucfg_policy_mgr_get_dynamic_mcc_adaptive_sch() - to get dynamic mcc adaptive
 *                                                  scheduler
 * @psoc: pointer to psoc
 * @dynamic_mcc_adaptive_sch: value to be filled
 *
 * This API is used to get dynamic mcc adaptive scheduler
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_dynamic_mcc_adaptive_sch(struct wlan_objmgr_psoc *psoc,
					     bool *dynamic_mcc_adaptive_sch);

/**
 * ucfg_policy_mgr_set_dynamic_mcc_adaptive_sch() - to set dynamic mcc adaptive
 *                                                  scheduler
 * @psoc: pointer to psoc
 * @dynamic_mcc_adaptive_sch: value to be set
 *
 * This API is used to set dynamic mcc adaptive scheduler
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_set_dynamic_mcc_adaptive_sch(struct wlan_objmgr_psoc *psoc,
					     bool dynamic_mcc_adaptive_sch);

/**
 * ucfg_policy_mgr_get_sta_cxn_5g_band() - to get STA's connection in 5G config
 *
 * @psoc: pointer to psoc
 * @enable_sta_cxn_5g_band: value to be filled
 *
 * This API is used to find out if STA connection in 5G band is allowed or
 * disallowed.
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_sta_cxn_5g_band(struct wlan_objmgr_psoc *psoc,
					       uint8_t *enable_sta_cxn_5g_band);
/**
 * ucfg_policy_mgr_get_allow_mcc_go_diff_bi() - to get information on whether GO
 *						can have diff BI than STA in MCC
 * @psoc: pointer to psoc
 * @allow_mcc_go_diff_bi: value to be filled
 *
 * This API is used to find out whether GO's BI can different than STA in MCC
 * scenario
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_allow_mcc_go_diff_bi(struct wlan_objmgr_psoc *psoc,
					 uint8_t *allow_mcc_go_diff_bi);
/**
 * ucfg_policy_mgr_get_dual_mac_feature() - to find out if DUAL MAC feature is
 *					    enabled
 * @psoc: pointer to psoc
 * @dual_mac_feature: value to be filled
 *
 * This API is used to find out whether dual mac (dual radio) specific feature
 * is enabled or not
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_dual_mac_feature(struct wlan_objmgr_psoc *psoc,
						uint8_t *dual_mac_feature);

/**
 * ucfg_policy_mgr_get_dual_sta_feature() - to find out if DUAL STA feature is
 *					    enabled
 * @psoc: pointer to psoc
 *
 * This API is used to find out whether dual sta specific feature is enabled
 * or not.
 *
 * Return: true if feature is enabled, otherwise false.
 */
bool ucfg_policy_mgr_get_dual_sta_feature(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_policy_mgr_get_force_1x1() - to find out if 1x1 connection is enforced
 *
 * @psoc: pointer to psoc
 * @force_1x1: value to be filled
 *
 * This API is used to find out if 1x1 connection is enforced.
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_force_1x1(struct wlan_objmgr_psoc *psoc,
					 uint8_t *force_1x1);

/**
 * ucfg_policy_mgr_get_max_conc_cxns() - to get configured max concurrent active
 * connection count
 *
 * @psoc: pointer to psoc
 *
 * This API is used to query the configured max concurrent active connection
 * count.
 *
 * Return: max active connection count
 */
uint32_t ucfg_policy_mgr_get_max_conc_cxns(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_policy_mgr_set_max_conc_cxns() - to set supported max concurrent active
 * connection count to policy mgr
 *
 * @psoc: pointer to psoc
 * @max_conc_cxns: max active connection count
 *
 * This API is used to update the max concurrent active connection
 * count to policy mgr
 *
 * Return: QDF_STATUS_SUCCESS if set successfully
 */
QDF_STATUS ucfg_policy_mgr_set_max_conc_cxns(struct wlan_objmgr_psoc *psoc,
					     uint32_t max_conc_cxns);

/**
 * ucfg_policy_mgr_get_sta_sap_scc_on_dfs_chnl() - to find out if STA and SAP
 *						   SCC is allowed on DFS channel
 * @psoc: pointer to psoc
 * @sta_sap_scc_on_dfs_chnl: value to be filled
 *
 * This API is used to find out whether STA and SAP SCC is allowed on
 * DFS channels
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_sta_sap_scc_on_dfs_chnl(struct wlan_objmgr_psoc *psoc,
					    uint8_t *sta_sap_scc_on_dfs_chnl);
/**
 * ucfg_policy_mgr_get_sta_sap_scc_lte_coex_chnl() - to find out if STA & SAP
 *						     SCC is allowed on LTE COEX
 * @psoc: pointer to psoc
 * @sta_sap_scc_lte_coex: value to be filled
 *
 * This API is used to find out whether STA and SAP scc is allowed on LTE COEX
 * channel
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_sta_sap_scc_lte_coex_chnl(struct wlan_objmgr_psoc *psoc,
					      uint8_t *sta_sap_scc_lte_coex);

/**
 * ucfg_policy_mgr_get_dfs_master_dynamic_enabled() - support dfs master or not
 *  AP interface when STA+SAP(GO) concurrency
 * @psoc: pointer to psoc
 * @vdev_id: sap vdev id
 *
 * This API is used to check SAP (GO) dfs master functionality enabled or not
 * when STA+SAP(GO) concurrency.
 * If g_sta_sap_scc_on_dfs_chan is non-zero, the STA+SAP(GO) is allowed on DFS
 * channel SCC and the SAP's DFS master functionality should be enable/disable
 * according to:
 * 1. g_sta_sap_scc_on_dfs_chan is 0: function return true - dfs master
 *     capability enabled.
 * 2. g_sta_sap_scc_on_dfs_chan is 1: function return false - dfs master
 *     capability disabled.
 * 3. g_sta_sap_scc_on_dfs_chan is 2: dfs master capability based on STA on
 *     5G or not:
 *      a. 5G STA active - return false
 *      b. no 5G STA active -return true
 *
 * Return: true if dfs master functionality should be enabled.
 */
bool
ucfg_policy_mgr_get_dfs_master_dynamic_enabled(struct wlan_objmgr_psoc *psoc,
					       uint8_t vdev_id);

/**
 * ucfg_policy_mgr_init_chan_avoidance() - init channel avoidance in policy
 *					   manager
 * @psoc: pointer to psoc
 * @chan_freq_list: channel frequency list
 * @chan_cnt: channel count
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_init_chan_avoidance(struct wlan_objmgr_psoc *psoc,
				    qdf_freq_t *chan_freq_list,
				    uint16_t chan_cnt);

/**
 * ucfg_policy_mgr_get_sap_mandt_chnl() - to find out if SAP mandatory channel
 *					  support is enabled
 * @psoc: pointer to psoc
 * @sap_mandt_chnl: value to be filled
 *
 * This API is used to find out whether SAP's mandatory channel support
 * is enabled
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_sap_mandt_chnl(struct wlan_objmgr_psoc *psoc,
					      uint8_t *sap_mandt_chnl);
/**
 * ucfg_policy_mgr_get_indoor_chnl_marking() - to get if indoor channel can be
 *						marked as disabled
 * @psoc: pointer to psoc
 * @indoor_chnl_marking: value to be filled
 *
 * This API is used to find out whether indoor channel can be marked as disabled
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_indoor_chnl_marking(struct wlan_objmgr_psoc *psoc,
					uint8_t *indoor_chnl_marking);

/**
 * ucfg_policy_mgr_get_sta_sap_scc_on_indoor_chnl() - to get if
 * sta sap scc on indoor channel is allowed
 * @psoc: pointer to psoc
 *
 * This API is used to get the value of  sta+sap scc on indoor channel
 *
 * Return: TRUE or FALSE
 */

bool
ucfg_policy_mgr_get_sta_sap_scc_on_indoor_chnl(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_policy_mgr_is_fw_supports_dbs() - to check whether FW supports DBS or
 * not
 * @psoc: pointer to psoc
 *
 * Return: true if DBS is supported else false
 */
bool ucfg_policy_mgr_is_fw_supports_dbs(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_policy_mgr_get_connection_count() - Get number of connections
 * @psoc: pointer to psoc
 *
 * This API is used to get the count of current connections.
 *
 * Return: connection count
 */
uint32_t ucfg_policy_mgr_get_connection_count(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_policy_mgr_is_hw_sbs_capable() - Check if HW is SBS capable
 * @psoc: pointer to psoc
 *
 * This API is to check if the HW is SBS capable.
 *
 * Return: true if the HW is SBS capable
 */
bool ucfg_policy_mgr_is_hw_sbs_capable(struct wlan_objmgr_psoc *psoc);

/*
 * ucfg_policy_mgr_get_vdev_same_freq_new_conn() - Get vdev_id of the first
 *					           connection that has same
 *					           channel frequency as new_freq
 * @psoc: psoc object pointer
 * @self_vdev_id: self vdev id of the connection with new_freq
 * @new_freq: channel frequency for the new connection
 * @vdev_id: Output parameter to return vdev id of the first existing connection
 *	     that has same channel frequency as @new_freq
 *
 * This function is to return the first connection that has same
 * channel frequency as @new_freq.
 *
 * Return: true if connection that has same channel frequency as
 *	   @new_freq exists. Otherwise false.
 */
bool ucfg_policy_mgr_get_vdev_same_freq_new_conn(struct wlan_objmgr_psoc *psoc,
						 uint8_t self_vdev_id,
						 uint32_t new_freq,
						 uint8_t *vdev_id);
/*
 * ucfg_policy_mgr_get_vdev_diff_freq_new_conn() - Get vdev id of the first
 *						   connection that has different
 *						   channel freq from new_freq
 * @psoc: psoc object pointer
 * @new_freq: channel frequency for the new connection
 * @vdev_id: Output parameter to return vdev id of the first existing connection
 *	     that has different channel frequency from @new_freq
 *
 * This function is to return the first connection that has different
 * channel frequency from @new_freq.
 *
 * Return: true if connection that has different channel frequency from
 *	   @new_freq exists. Otherwise false.
 */
bool ucfg_policy_mgr_get_vdev_diff_freq_new_conn(struct wlan_objmgr_psoc *psoc,
						 uint32_t new_freq,
						 uint8_t *vdev_id);

/**
 * ucfg_policy_mgr_get_dbs_hw_modes() - to get the DBS HW modes
 *
 * @psoc: pointer to psoc
 * @one_by_one_dbs: 1x1 DBS capability of HW
 * @two_by_two_dbs: 2x2 DBS capability of HW
 *
 * Return: Failure in case of error otherwise success
 */
QDF_STATUS ucfg_policy_mgr_get_dbs_hw_modes(struct wlan_objmgr_psoc *psoc,
					    bool *one_by_one_dbs,
					    bool *two_by_two_dbs);

/**
 * ucfg_policy_mgr_wait_chan_switch_complete_evt() - Wait for SAP/GO CSA complete
 * event
 * @psoc: PSOC object information
 *
 * Return: QDF_STATUS_SUCCESS if CSA complete
 */
QDF_STATUS
ucfg_policy_mgr_wait_chan_switch_complete_evt(struct wlan_objmgr_psoc *psoc);

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * ucfg_policy_mgr_pre_ap_start() - handle ap start request
 * @psoc: pointer to psoc
 * @vdev_id: vdev id of starting ap
 *
 * Return: Failure in case of error otherwise success
 */
QDF_STATUS
ucfg_policy_mgr_pre_ap_start(struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id);

/**
 * ucfg_policy_mgr_post_ap_start_failed() - handle ap start
 * failed
 * @psoc: pointer to psoc
 * @vdev_id: vdev id of starting ap
 *
 * Return: Failure in case of error otherwise success
 */
QDF_STATUS
ucfg_policy_mgr_post_ap_start_failed(
			     struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id);

/**
 * ucfg_policy_mgr_pre_sta_p2p_start() - handle STA P2P start request
 * @psoc: pointer to psoc
 * @vdev_id: vdev id of starting sta
 *
 * Return: Failure in case of error otherwise success
 */
QDF_STATUS
ucfg_policy_mgr_pre_sta_p2p_start(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id);

/**
 * ucfg_policy_mgr_post_sta_p2p_start_failed() - handle STA P2P start
 * failed
 * @psoc: pointer to psoc
 * @vdev_id: vdev id of starting sta
 *
 * Return: Failure in case of error otherwise success
 */
QDF_STATUS
ucfg_policy_mgr_post_sta_p2p_start_failed(struct wlan_objmgr_psoc *psoc,
					  uint8_t vdev_id);

/**
 * ucfg_policy_mgr_clear_ml_links_settings_in_fw() - Process
 * QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE in default mode
 * @psoc: objmgr psoc
 * @vdev_id: vdev_id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_policy_mgr_clear_ml_links_settings_in_fw(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id);

/**
 * ucfg_policy_mgr_update_mlo_links_based_on_linkid() - Force active
 * ML links based on user requested coming via
 * QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE
 * @psoc: objmgr psoc
 * @vdev_id: vdev id
 * @num_links: number of links to be forced active
 * @link_id_list: link id(s) list coming from user space
 * @config_state_list: config state list coming from user space
 *
 * Return: success if the command gets processed successfully
 */
QDF_STATUS
ucfg_policy_mgr_update_mlo_links_based_on_linkid(struct wlan_objmgr_psoc *psoc,
						 uint8_t vdev_id,
						 uint8_t num_links,
						 uint8_t *link_id_list,
						 uint32_t *config_state_list);

/**
 * ucfg_policy_mgr_update_active_mlo_num_links() - Force active ML links based
 * on user requested coming via LINK_STATE_MIXED_MODE_ACTIVE_NUM_LINKS
 * @psoc: objmgr psoc
 * @vdev_id: vdev id
 * @num_links: number of links to be forced active
 *
 * Return: success if the command gets processed successfully
 */
QDF_STATUS
ucfg_policy_mgr_update_active_mlo_num_links(struct wlan_objmgr_psoc *psoc,
					    uint8_t vdev_id,
					    uint8_t num_links);

/**
 * ucfg_policy_mgr_find_current_hw_mode() - Find current HW mode
 * @psoc: objmgr psoc
 *
 * Return: policy mgr current HW mode.
 */
enum policy_mgr_curr_hw_mode
ucfg_policy_mgr_find_current_hw_mode(struct wlan_objmgr_psoc *psoc);
#else
static inline QDF_STATUS
ucfg_policy_mgr_pre_ap_start(struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_policy_mgr_post_ap_start_failed(
			     struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_policy_mgr_pre_sta_p2p_start(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_policy_mgr_post_sta_p2p_start_failed(struct wlan_objmgr_psoc *psoc,
					  uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif //__WLAN_POLICY_MGR_UCFG
