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
/**
 * DOC: declare internal API related to the fwol component
 */

#ifndef _WLAN_FWOL_UCFG_API_H_
#define _WLAN_FWOL_UCFG_API_H_

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_cmn.h>
#include "wlan_fw_offload_main.h"
#include "wlan_fwol_public_structs.h"

#ifdef WLAN_FW_OFFLOAD
/**
 * ucfg_fwol_psoc_open() - FWOL component Open
 * @psoc: pointer to psoc object
 *
 * Open the FWOL component and initialize the FWOL structure
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_psoc_close() - FWOL component close
 * @psoc: pointer to psoc object
 *
 * Close the FWOL component and clear the FWOL structures
 *
 * Return: None
 */
void ucfg_fwol_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_psoc_enable() - FWOL component enable
 * @psoc: pointer to psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_psoc_disable() - FWOL component disable
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
void ucfg_fwol_psoc_disable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_init() - initialize fwol_ctx context.
 *
 * This function initializes the fwol context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_fwol_init(void);

/**
 * ucfg_fwol_deinit() - De initialize fwol_ctx context.
 *
 * This function De initializes fwol context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
void ucfg_fwol_deinit(void);

#ifdef FW_THERMAL_THROTTLE_SUPPORT
/**
 * ucfg_fwol_thermal_register_callbacks() - Register thermal callbacks
 * to be called by fwol thermal
 * @psoc: psoc object
 * @cb: callback functions
 *
 * Currently only one callback notify_thermal_throttle_handler can be
 * registered to fwol thermal core. The client will be notified by the callback
 * when new thermal throttle level is changed in target.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_fwol_thermal_register_callbacks(
				struct wlan_objmgr_psoc *psoc,
				struct fwol_thermal_callbacks *cb);

/**
 * ucfg_fwol_thermal_unregister_callbacks() - unregister thermal callbacks
 * @psoc: psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_fwol_thermal_unregister_callbacks(
				struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_thermal_get_target_level() - get thermal level based on cached
 *  target thermal throttle level
 * @psoc: psoc object
 * @level: target thermal throttle level info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_fwol_thermal_get_target_level(struct wlan_objmgr_psoc *psoc,
				   enum thermal_throttle_level *level);
#else
static inline QDF_STATUS ucfg_fwol_thermal_register_callbacks(
				struct wlan_objmgr_psoc *psoc,
				struct fwol_thermal_callbacks *cb)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_fwol_thermal_unregister_callbacks(
				struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_fwol_thermal_get_target_level(struct wlan_objmgr_psoc *psoc,
				   enum thermal_throttle_level *level)

{
	return QDF_STATUS_E_FAILURE;
}
#endif

/**
 * ucfg_fwol_get_coex_config_params() - Get coex config params
 * @psoc: Pointer to psoc object
 * @coex_config: Pointer to struct wlan_fwol_coex_config
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_fwol_get_coex_config_params(struct wlan_objmgr_psoc *psoc,
				 struct wlan_fwol_coex_config *coex_config);

/**
 * ucfg_fwol_get_thermal_temp() - Get thermal temperature config params
 * @psoc: Pointer to psoc object
 * @thermal_temp: Pointer to struct wlan_fwol_thermal_temp
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_thermal_temp(struct wlan_objmgr_psoc *psoc,
			   struct wlan_fwol_thermal_temp *thermal_temp);

/**
 * ucfg_fwol_is_neighbor_report_req_supported() - Get neighbor report request
 *                                                supported bit
 * @psoc: Pointer to psoc object
 * @neighbor_report_req: Pointer to return value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_is_neighbor_report_req_supported(struct wlan_objmgr_psoc *psoc,
					   bool *neighbor_report_req);

/**
 * ucfg_fwol_get_ie_allowlist() - Get IE allowlist param value
 * @psoc: Pointer to psoc object
 * @ie_allowlist: Pointer to return the IE allowlist param value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_ie_allowlist(struct wlan_objmgr_psoc *psoc, bool *ie_allowlist);

/**
 * ucfg_fwol_set_ie_allowlist() - Set IE allowlist param value
 * @psoc: Pointer to psoc object
 * @ie_allowlist: Value to set IE allowlist param
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_set_ie_allowlist(struct wlan_objmgr_psoc *psoc, bool ie_allowlist);

/**
 * ucfg_fwol_get_all_allowlist_params() - Get all IE allowlist param values
 * @psoc: Pointer to psoc object
 * @allowlist: Pointer to struct wlan_fwol_ie_allowlist
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_all_allowlist_params(struct wlan_objmgr_psoc *psoc,
				   struct wlan_fwol_ie_allowlist *allowlist);

/**
 * ucfg_fwol_get_ani_enabled() - Assigns the ani_enabled value
 * @psoc: pointer to the psoc object
 * @ani_enabled: pointer to return ani_enabled value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_ani_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *ani_enabled);

/**
 * ucfg_fwol_get_pcie_config() - Assigns the pcie_config value
 * @psoc: pointer to the psoc object
 * @pcie_config: pointer to return pcie_config value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_pcie_config(struct wlan_objmgr_psoc *psoc,
				     uint8_t *pcie_config);

/**
 * ucfg_get_enable_rts_sifsbursting() - Assigns the enable_rts_sifsbursting
 *                                      value
 * @psoc: pointer to the psoc object
 * @enable_rts_sifsbursting: pointer to return enable_rts_sifsbursting value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_enable_rts_sifsbursting(struct wlan_objmgr_psoc *psoc,
					    bool *enable_rts_sifsbursting);

/**
 * ucfg_get_enable_sifs_burst() - Get the enable_sifs_burst value
 * @psoc: pointer to the psoc object
 * @enable_sifs_burst: pointer to return enable_sifs_burst value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_enable_sifs_burst(struct wlan_objmgr_psoc *psoc,
				      uint8_t *enable_sifs_burst);

/**
 * ucfg_get_max_mpdus_inampdu() - Assigns the max_mpdus_inampdu value
 * @psoc: pointer to the psoc object
 * @max_mpdus_inampdu: pointer to return max_mpdus_inampdu value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_max_mpdus_inampdu(struct wlan_objmgr_psoc *psoc,
				      uint8_t *max_mpdus_inampdu);

/**
 * ucfg_get_enable_phy_reg_retention() - Assigns enable_phy_reg_retention value
 * @psoc: pointer to the psoc object
 * @enable_phy_reg_retention: pointer to return enable_phy_reg_retention value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_enable_phy_reg_retention(struct wlan_objmgr_psoc *psoc,
					     uint8_t *enable_phy_reg_retention);

/**
 * ucfg_get_upper_brssi_thresh() - Assigns upper_brssi_thresh value
 * @psoc: pointer to the psoc object
 * @upper_brssi_thresh: pointer to return upper_brssi_thresh value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_upper_brssi_thresh(struct wlan_objmgr_psoc *psoc,
				       uint16_t *upper_brssi_thresh);

/**
 * ucfg_get_lower_brssi_thresh() - Assigns lower_brssi_thresh value
 * @psoc: pointer to the psoc object
 * @lower_brssi_thresh: pointer to return lower_brssi_thresh value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_lower_brssi_thresh(struct wlan_objmgr_psoc *psoc,
				       uint16_t *lower_brssi_thresh);

/**
 * ucfg_get_enable_dtim_1chrx() - Assigns enable_dtim_1chrx value
 * @psoc: pointer to the psoc object
 * @enable_dtim_1chrx: pointer to return enable_dtim_1chrx value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_enable_dtim_1chrx(struct wlan_objmgr_psoc *psoc,
				      bool *enable_dtim_1chrx);

/**
 * ucfg_get_dynamic_bw_switch_value() - Assigns dynamic_bw_switch value
 * @psoc: pointer to the psoc object
 * @dynamic_bw_switch: pointer to return dynamic_bw_switch value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_dynamic_bw_switch_value(struct wlan_objmgr_psoc *psoc,
					    bool *dynamic_bw_switch);

/**
 * ucfg_get_alternative_chainmask_enabled() - Assigns alt chainmask_enabled
 *                                            value
 * @psoc: pointer to the psoc object
 * @alternative_chainmask_enabled: pointer to return
 *                                 alternative_chainmask_enabled value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_get_alternative_chainmask_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *alternative_chainmask_enabled);

/**
 * ucfg_get_smart_chainmask_enabled() - Assigns smart_chainmask_enabled value
 * @psoc: pointer to the psoc object
 * @smart_chainmask_enabled: pointer to return smart_chainmask_enabled value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_smart_chainmask_enabled(struct wlan_objmgr_psoc *psoc,
					    bool *smart_chainmask_enabled);

/**
 * ucfg_fwol_get_rts_profile() - Assigns get_rts_profile value
 * @psoc: pointer to the psoc object
 * @get_rts_profile: pointer to return RTS profile value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_rts_profile(struct wlan_objmgr_psoc *psoc,
				     uint16_t *get_rts_profile);

/**
 * ucfg_fwol_get_enable_fw_log_level() - Assigns enable_fw_log_level value
 * @psoc: pointer to the psoc object
 * @enable_fw_log_level: pointer to return firmware log level enable bitmap
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_enable_fw_log_level(struct wlan_objmgr_psoc *psoc,
					     uint16_t *enable_fw_log_level);

/**
 * ucfg_fwol_get_enable_fw_log_type() - Assigns enable_fw_log_type value
 * @psoc: pointer to the psoc object
 * @enable_fw_log_type: pointer to return firmware log type bitmap
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_enable_fw_log_type(struct wlan_objmgr_psoc *psoc,
					    uint16_t *enable_fw_log_type);

/**
 * ucfg_fwol_get_enable_fw_module_log_level() - Assigns
 * enable_fw_module_log_level string
 * @psoc: pointer to the psoc object
 * @enable_fw_module_log_level:
 * pointer to enable_fw_module_log_level array
 * @enable_fw_module_log_level_num:
 * pointer to enable_fw_module_log_level array element num
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_enable_fw_module_log_level(
				struct wlan_objmgr_psoc *psoc,
				uint8_t **enable_fw_module_log_level,
				uint8_t *enable_fw_module_log_level_num);

/**
 * ucfg_fwol_wow_get_enable_fw_module_log_level() - Assigns
 * enable_fw_module_log_level string
 *
 * @psoc: pointer to the psoc object
 * @enable_fw_wow_module_log_level:
 * pointer to enable_fw_wow_module_log_level array
 * @enable_fw_wow_module_log_level_num:
 * pointer to enable_fw_wow_module_log_level array element num
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_wow_get_enable_fw_module_log_level(
				struct wlan_objmgr_psoc *psoc,
				uint8_t **enable_fw_wow_module_log_level,
				uint8_t *enable_fw_wow_module_log_level_num);

/**
 * ucfg_fwol_get_sap_xlna_bypass() - Assigns sap_xlna_bypass value
 * @psoc: pointer to the psoc object
 * @sap_xlna_bypass: pointer to return sap_xlna_bypass bool
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_sap_xlna_bypass(struct wlan_objmgr_psoc *psoc,
					 bool *sap_xlna_bypass);

#ifdef FEATURE_WLAN_RA_FILTERING
/**
 * ucfg_fwol_set_is_rate_limit_enabled() - Sets the is_rate_limit_enabled value
 * @psoc: pointer to the psoc object
 * @is_rate_limit_enabled: value to set rate limit enabled bool
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_set_is_rate_limit_enabled(struct wlan_objmgr_psoc *psoc,
					       bool is_rate_limit_enabled);

/**
 * ucfg_fwol_get_is_rate_limit_enabled() - Assigns is_rate_limit_enabled value
 * @psoc: pointer to the psoc object
 * @is_rate_limit_enabled: pointer to return rate limit enabled bool
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_is_rate_limit_enabled(struct wlan_objmgr_psoc *psoc,
					       bool *is_rate_limit_enabled);

#endif /* FEATURE_WLAN_RA_FILTERING */

/**
 * ucfg_fwol_get_tsf_gpio_pin() - Assigns tsf_gpio_pin value
 * @psoc: pointer to the psoc object
 * @tsf_gpio_pin: pointer to return TSF GPIO pin value
 *
 * Return: QDF Status
 */

QDF_STATUS ucfg_fwol_get_tsf_gpio_pin(struct wlan_objmgr_psoc *psoc,
				      uint32_t *tsf_gpio_pin);

#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_IRQ
/**
 * ucfg_fwol_get_tsf_irq_host_gpio_pin() - Assigns tsf_irq_host_gpio_pin value
 * @psoc: pointer to the psoc object
 * @tsf_irq_host_gpio_pin: pointer to return the TSF IRQ GPIO pin number
 *
 * Return: QDF Status
 */

QDF_STATUS
ucfg_fwol_get_tsf_irq_host_gpio_pin(struct wlan_objmgr_psoc *psoc,
				    uint32_t *tsf_irq_host_gpio_pin);
#endif

#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC
/**
 * ucfg_fwol_get_tsf_sync_host_gpio_pin() - Assigns tsf_sync_host_gpio_pin value
 * @psoc: pointer to the psoc object
 * @tsf_irq_host_gpio_pin: pointer to return the TSF sync GPIO pin number
 *
 * Return: QDF Status
 */

QDF_STATUS
ucfg_fwol_get_tsf_sync_host_gpio_pin(struct wlan_objmgr_psoc *psoc,
				     uint32_t *tsf_irq_host_gpio_pin);
#endif

#ifdef DHCP_SERVER_OFFLOAD
/**
 * ucfg_fwol_get_enable_dhcp_server_offload()-Assign enable_dhcp_server_offload
 * @psoc: pointer to the psoc object
 * @enable_dhcp_server_offload: pointer to return enable_dhcp_server_offload
 *                              value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_enable_dhcp_server_offload(struct wlan_objmgr_psoc *psoc,
					 bool *enable_dhcp_server_offload);

/**
 * ucfg_fwol_get_dhcp_max_num_clients() - Assigns dhcp_max_num_clients value
 * @psoc: pointer to the psoc object
 * @dhcp_max_num_clients: pointer to return the max number of DHC clients value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_dhcp_max_num_clients(struct wlan_objmgr_psoc *psoc,
					      uint32_t *dhcp_max_num_clients);
#endif

/**
 * ucfg_fwol_get_tsf_sync_enable() - Get TSF sync enabled
 * @psoc: pointer to the psoc object
 * @tsf_sync_enable: Pointer to tsf sync enabled
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_tsf_sync_enable(struct wlan_objmgr_psoc *psoc,
					 bool *tsf_sync_enable);

#ifdef WLAN_FEATURE_TSF_ACCURACY
/**
 * ucfg_fwol_get_tsf_accuracy_configs() - Get TSF accuracy configs
 * @psoc: pointer to the psoc object
 * @config: Pointer to hold TSF Accuracy Feature configs
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_tsf_accuracy_configs(struct wlan_objmgr_psoc *psoc,
					      struct wlan_fwol_tsf_accuracy_configs **config);
#endif

/**
 * ucfg_fwol_get_tsf_ptp_options() - Get TSF Plus feature options
 * @psoc: pointer to the psoc object
 * @tsf_ptp_options: Pointer to return tsf ptp options
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_tsf_ptp_options(struct wlan_objmgr_psoc *psoc,
					 uint32_t *tsf_ptp_options);

/**
 * ucfg_fwol_get_sae_enable() - Get SAE feature enable status
 * @psoc: pointer to the psoc object
 *
 * Return: True if enabled else false
 */
bool ucfg_fwol_get_sae_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_get_gcmp_enable() - Get GCMP feature enable status
 * @psoc: pointer to the psoc object
 *
 * Return: True if enabled else false
 */
bool ucfg_fwol_get_gcmp_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_get_enable_tx_sch_delay() - Get enable tx sch delay
 * @psoc: pointer to the psoc object
 * @enable_tx_sch_delay: Pointer to return enable_tx_sch_delay value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_enable_tx_sch_delay(struct wlan_objmgr_psoc *psoc,
					     uint8_t *enable_tx_sch_delay);

#ifdef WLAN_FEATURE_OFDM_SCRAMBLER_SEED
/**
 * ucfg_fwol_get_ofdm_scrambler_seed() - Assigns enable_ofdm_scrambler_seed
 * @psoc: pointer to psoc object
 * @enable_ofdm_scrambler_seed: Pointer to return enable_ofdm_scrambler_seed
 * value
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS ucfg_fwol_get_ofdm_scrambler_seed(struct wlan_objmgr_psoc *psoc,
					     bool *enable_ofdm_scrambler_seed);
#else
static inline QDF_STATUS ucfg_fwol_get_ofdm_scrambler_seed(
				struct wlan_objmgr_psoc *psoc,
				bool *enable_ofdm_scrambler_seed)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * ucfg_fwol_get_enable_secondary_rate() - Get enable secondary rate
 * @psoc: pointer to the psoc object
 * @enable_secondary_rate: Pointer to return enable secondary rate value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_enable_secondary_rate(struct wlan_objmgr_psoc *psoc,
					       uint32_t *enable_secondary_rate);
/**
 * ucfg_fwol_get_all_adaptive_dwelltime_params() - Get all adaptive
 *						   dwelltime_params
 * @psoc: Pointer to psoc object
 * @dwelltime_params: Pointer to struct adaptive_dwelltime_params
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_all_adaptive_dwelltime_params(
			struct wlan_objmgr_psoc *psoc,
			struct adaptive_dwelltime_params *dwelltime_params);
/**
 * ucfg_fwol_get_adaptive_dwell_mode_enabled() - API to globally disable/enable
 *                                               the adaptive dwell config.
 * Acceptable values for this:
 * 0: Config is disabled
 * 1: Config is enabled
 *
 * @psoc: pointer to psoc object
 * @adaptive_dwell_mode_enabled: adaptive dwell mode enable/disable
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_adaptive_dwell_mode_enabled(struct wlan_objmgr_psoc *psoc,
					  bool *adaptive_dwell_mode_enabled);

/**
 * ucfg_fwol_get_global_adapt_dwelltime_mode() - API to set default
 *                                               adaptive mode.
 * It will be used if any of the scan dwell mode is set to default.
 * For uses : see enum scan_dwelltime_adaptive_mode
 *
 * @psoc: pointer to psoc object
 * @global_adapt_dwelltime_mode: global adaptive dwell mode value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_global_adapt_dwelltime_mode(struct wlan_objmgr_psoc *psoc,
					  uint8_t *global_adapt_dwelltime_mode);
/**
 * ucfg_fwol_get_adapt_dwell_lpf_weight() - API to get weight to calculate
 *                       the average low pass filter for channel congestion
 * @psoc: pointer to psoc object
 * @adapt_dwell_lpf_weight: adaptive low pass filter weight
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_adapt_dwell_lpf_weight(struct wlan_objmgr_psoc *psoc,
				     uint8_t *adapt_dwell_lpf_weight);

/**
 * ucfg_fwol_get_adapt_dwell_passive_mon_intval() - API to get interval value
 *                      for montitoring wifi activity in passive scan in msec.
 * @psoc: pointer to psoc object
 * @adapt_dwell_passive_mon_intval: adaptive monitor interval in passive scan
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_adapt_dwell_passive_mon_intval(
				struct wlan_objmgr_psoc *psoc,
				uint8_t *adapt_dwell_passive_mon_intval);

/**
 * ucfg_fwol_get_adapt_dwell_wifi_act_threshold - API to get % of wifi activity
 *                                                used in passive scan
 * @psoc: pointer to psoc object
 * @adapt_dwell_wifi_act_threshold: percent of wifi activity in passive scan
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_adapt_dwell_wifi_act_threshold(
				struct wlan_objmgr_psoc *psoc,
				uint8_t *adapt_dwell_wifi_act_threshold);

/**
 * ucfg_fwol_init_adapt_dwelltime_in_cfg - API to initialize adaptive
 *                                         dwell params
 * @psoc: pointer to psoc object
 * @dwelltime_params: pointer to adaptive_dwelltime_params structure
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_fwol_init_adapt_dwelltime_in_cfg(
			struct wlan_objmgr_psoc *psoc,
			struct adaptive_dwelltime_params *dwelltime_params)
{
	return fwol_init_adapt_dwelltime_in_cfg(psoc, dwelltime_params);
}

/**
 * ucfg_fwol_set_adaptive_dwelltime_config - API to set adaptive
 *                                           dwell params config
 * @dwelltime_params: adaptive_dwelltime_params structure
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_fwol_set_adaptive_dwelltime_config(
			struct adaptive_dwelltime_params *dwelltime_params)
{
	return fwol_set_adaptive_dwelltime_config(dwelltime_params);
}

#ifdef WLAN_FEATURE_ELNA
/**
 * ucfg_fwol_set_elna_bypass() - send set eLNA bypass request
 * @vdev: vdev handle
 * @req: set eLNA bypass request
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_fwol_set_elna_bypass(struct wlan_objmgr_vdev *vdev,
				     struct set_elna_bypass_request *req);

/**
 * ucfg_fwol_get_elna_bypass() - send get eLNA bypass request
 * @vdev: vdev handle
 * @req: get eLNA bypass request
 * @callback: get eLNA bypass response callback
 * @context: request manager context
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_fwol_get_elna_bypass(struct wlan_objmgr_vdev *vdev,
				     struct get_elna_bypass_request *req,
				     void (*callback)(void *context,
				     struct get_elna_bypass_response *response),
				     void *context);
#endif /* WLAN_FEATURE_ELNA */

#ifdef WLAN_SEND_DSCP_UP_MAP_TO_FW
/**
 * ucfg_fwol_send_dscp_up_map_to_fw() - send dscp_up map to FW
 * @vdev: vdev handle
 * @dscp_to_up_map: DSCP to UP map array
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_fwol_send_dscp_up_map_to_fw(
		struct wlan_objmgr_vdev *vdev,
		uint32_t *dscp_to_up_map);
#else
static inline
QDF_STATUS ucfg_fwol_send_dscp_up_map_to_fw(
		struct wlan_objmgr_vdev *vdev,
		uint32_t *dscp_to_up_map)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_MDNS_OFFLOAD
/**
 * ucfg_fwol_set_mdns_config() - set mdns config
 * @psoc: pointer to psoc object
 * @mdns_info: mdns config info pointer
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_fwol_set_mdns_config(struct wlan_objmgr_psoc *psoc,
				     struct mdns_config_info *mdns_info);
#endif /* WLAN_FEATURE_MDNS_OFFLOAD */

/**
 * ucfg_fwol_update_fw_cap_info - API to update fwol capability info
 * @psoc: pointer to psoc object
 * @caps: pointer to wlan_fwol_capability_info struct
 *
 * Used to update fwol capability info.
 *
 * Return: void
 */
void ucfg_fwol_update_fw_cap_info(struct wlan_objmgr_psoc *psoc,
				  struct wlan_fwol_capability_info *caps);

#ifdef THERMAL_STATS_SUPPORT
QDF_STATUS ucfg_fwol_send_get_thermal_stats_cmd(struct wlan_objmgr_psoc *psoc,
				      enum thermal_stats_request_type req_type,
				      void (*callback)(void *context,
				      struct thermal_throttle_info *response),
				      void *context);
#endif /* THERMAL_STATS_SUPPORT */

/**
 * ucfg_fwol_configure_global_params - API to configure global params
 * @psoc: pointer to psoc object
 * @pdev: pointer to pdev object
 *
 * Used to configure global firmware params. This is invoked from hdd during
 * bootup.
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_configure_global_params(struct wlan_objmgr_psoc *psoc,
					     struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_fwol_set_ilp_config - API to configure Interface Low Power (ILP)
 * @psoc: pointer to psoc object
 * @pdev: pointer to pdev object
 * @enable: enable
 *
 * This API is used to enable/disable Interface Low Power (IPL) feature.
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_set_ilp_config(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_pdev *pdev,
				    uint32_t enable);

/**
 * ucfg_fwol_configure_vdev_params - API to configure vdev specific params
 * @psoc: pointer to psoc object
 * @vdev: pointer to vdev object
 *
 * Used to configure per vdev firmware params based on device mode. This is
 * invoked from hdd during vdev creation.
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_configure_vdev_params(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev);
#else
static inline QDF_STATUS ucfg_fwol_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void ucfg_fwol_psoc_close(struct wlan_objmgr_psoc *psoc)
{
}

static inline QDF_STATUS ucfg_fwol_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void ucfg_fwol_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
}

static inline QDF_STATUS ucfg_fwol_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline void ucfg_fwol_deinit(void)
{
}

static inline QDF_STATUS ucfg_fwol_thermal_register_callbacks(
				struct wlan_objmgr_psoc *psoc,
				struct fwol_thermal_callbacks *cb)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_fwol_thermal_unregister_callbacks(
				struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_fwol_thermal_get_target_level(struct wlan_objmgr_psoc *psoc,
				   enum thermal_throttle_level *level)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_coex_config_params(struct wlan_objmgr_psoc *psoc,
				 struct wlan_fwol_coex_config *coex_config)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_thermal_temp(struct wlan_objmgr_psoc *psoc,
			   struct wlan_fwol_thermal_temp *thermal_temp)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_is_neighbor_report_req_supported(struct wlan_objmgr_psoc *psoc,
					   bool *neighbor_report_req)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_ie_allowlist(struct wlan_objmgr_psoc *psoc, bool *ie_allowlist)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_set_ie_allowlist(struct wlan_objmgr_psoc *psoc, bool ie_allowlist)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_all_allowlist_params(struct wlan_objmgr_psoc *psoc,
				   struct wlan_fwol_ie_allowlist *allowlist)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_ani_enabled(struct wlan_objmgr_psoc *psoc,
			  bool *ani_enabled)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_pcie_config(struct wlan_objmgr_psoc *psoc,
			  uint8_t *pcie_config)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_enable_rts_sifsbursting(struct wlan_objmgr_psoc *psoc,
				 bool *enable_rts_sifsbursting)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_max_mpdus_inampdu(struct wlan_objmgr_psoc *psoc,
			   uint8_t *max_mpdus_inampdu)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_enable_phy_reg_retention(struct wlan_objmgr_psoc *psoc,
				  uint8_t *enable_phy_reg_retention)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_upper_brssi_thresh(struct wlan_objmgr_psoc *psoc,
			    uint16_t *upper_brssi_thresh)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_lower_brssi_thresh(struct wlan_objmgr_psoc *psoc,
			    uint16_t *lower_brssi_thresh)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_enable_dtim_1chrx(struct wlan_objmgr_psoc *psoc,
			   bool *enable_dtim_1chrx)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_dynamic_bw_switch_value(struct wlan_objmgr_psoc *psoc,
				 bool *enable_dtim_1chrx)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_alternative_chainmask_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *alternative_chainmask_enabled)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_smart_chainmask_enabled(struct wlan_objmgr_psoc *psoc,
				 bool *smart_chainmask_enabled)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_rts_profile(struct wlan_objmgr_psoc *psoc,
			  uint16_t *get_rts_profile)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_enable_fw_log_level(struct wlan_objmgr_psoc *psoc,
				  uint16_t *enable_fw_log_level)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_enable_fw_log_type(struct wlan_objmgr_psoc *psoc,
				 uint16_t *enable_fw_log_type)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_enable_fw_module_log_level(
				struct wlan_objmgr_psoc *psoc,
				uint8_t **enable_fw_module_log_level,
				uint8_t *enable_fw_module_log_level_num)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_sap_xlna_bypass(struct wlan_objmgr_psoc *psoc,
			      uint8_t *sap_xlna_bypass)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_tsf_gpio_pin(struct wlan_objmgr_psoc *psoc,
			   uint32_t *tsf_gpio_pin)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_tsf_ptp_options(struct wlan_objmgr_psoc *psoc,
			      uint32_t *tsf_ptp_options)
{
	return QDF_STATUS_E_FAILURE;
}

static inline bool ucfg_fwol_get_sae_enable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool ucfg_fwol_get_gcmp_enable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline QDF_STATUS
ucfg_fwol_get_enable_tx_sch_delay(struct wlan_objmgr_psoc *psoc,
				  uint8_t *enable_tx_sch_delay)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_enable_secondary_rate(struct wlan_objmgr_psoc *psoc,
				    uint32_t *enable_secondary_rate)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_all_adaptive_dwelltime_params(
			struct wlan_objmgr_psoc *psoc,
			struct adaptive_dwelltime_params *dwelltime_params)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_adaptive_dwell_mode_enabled(struct wlan_objmgr_psoc *psoc,
					  bool *adaptive_dwell_mode_enabled)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_global_adapt_dwelltime_mode(struct wlan_objmgr_psoc *psoc,
					  uint8_t *global_adapt_dwelltime_mode)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_adapt_dwell_lpf_weight(struct wlan_objmgr_psoc *psoc,
				     uint8_t *adapt_dwell_lpf_weight)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_adapt_dwell_passive_mon_intval(
				struct wlan_objmgr_psoc *psoc,
				uint8_t *adapt_dwell_passive_mon_intval)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_adapt_dwell_wifi_act_threshold(
				struct wlan_objmgr_psoc *psoc,
				uint8_t *adapt_dwell_wifi_act_threshold)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_init_adapt_dwelltime_in_cfg(
			struct wlan_objmgr_psoc *psoc,
			struct adaptive_dwelltime_params *dwelltime_params)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_set_adaptive_dwelltime_config(
			struct adaptive_dwelltime_params *dwelltime_params)
{
	return QDF_STATUS_E_FAILURE;
}

#ifdef FEATURE_WLAN_RA_FILTERING
static inline QDF_STATUS
ucfg_fwol_set_is_rate_limit_enabled(struct wlan_objmgr_psoc *psoc,
				    bool is_rate_limit_enabled)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_is_rate_limit_enabled(struct wlan_objmgr_psoc *psoc,
				    bool *is_rate_limit_enabled)
{
	return QDF_STATUS_E_FAILURE;
}
#endif /* FEATURE_WLAN_RA_FILTERING */

static inline QDF_STATUS
ucfg_fwol_configure_global_params(struct wlan_objmgr_psoc *psoc,
				  struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_configure_vdev_params(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_FAILURE;
}

#endif /* WLAN_FW_OFFLOAD */

#endif /* _WLAN_FWOL_UCFG_API_H_ */
