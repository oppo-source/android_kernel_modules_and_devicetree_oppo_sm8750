
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WMI_UNIFIED_11BE_PARAM_H_
#define _WMI_UNIFIED_11BE_PARAM_H_

#include <wmi_unified_param.h>

#ifdef WLAN_FEATURE_11BE_MLO

#define MAX_LINK_IN_MLO 6
/** struct wmi_mlo_setup_params - MLO setup command params
 * @mld_grp_id: Unique ID to FW for MLD group
 * @pdev_id: pdev id of radio on which this command is sent
 * @num_valid_hw_links: Num of valid links in partner_links array
 * @partner_links[MAX_LINK_IN_MLO]: Partner link IDs
 */
struct wmi_mlo_setup_params {
	uint32_t mld_grp_id;
	uint32_t pdev_id;
	uint8_t num_valid_hw_links;
	uint32_t partner_links[MAX_LINK_IN_MLO];
};

/** struct wmi_mlo_ready_params - MLO ready command params
 * @pdev_id: pdev id of radio on which this command is sent
 */
struct wmi_mlo_ready_params {
	uint32_t pdev_id;
};

/** enum wmi_mlo_teardown_reason - Reason code in WMI MLO teardown command
 * @WMI_HOST_MLO_TEARDOWN_REASON_DOWN: Wifi down
 * @WMI_HOST_MLO_TEARDOWN_REASON_SSR: Wifi Recovery
 * @WMI_HOST_MLO_TEARDOWN_REASON_MODE1_SSR: Recovery Mode1 SSR teardown
 * @WMI_HOST_MLO_TEARDOWN_REASON_STANDBY: Network Standby mode teardown
 * @WMI_HOST_MLO_TEARDOWN_REASON_DYNAMIC_WSI_REMAP: Dynamic WSI remap
 */
enum wmi_mlo_teardown_reason {
	WMI_HOST_MLO_TEARDOWN_REASON_DOWN,
	WMI_HOST_MLO_TEARDOWN_REASON_SSR,
	WMI_HOST_MLO_TEARDOWN_REASON_MODE1_SSR,
	WMI_HOST_MLO_TEARDOWN_REASON_STANDBY,
	WMI_HOST_MLO_TEARDOWN_REASON_DYNAMIC_WSI_REMAP,
};

/** struct wmi_mlo_teardown_params - MLO teardown command params
 * @pdev_id: pdev id of radio on which this command is sent
 * @reason: reason code from enum wmi_mlo_teardown_reason
 * @umac_reset: trigger umac reset for mode1 or not
 */
struct wmi_mlo_teardown_params {
	uint32_t pdev_id;
	enum wmi_mlo_teardown_reason reason;
	bool umac_reset;
	bool standby_active;
};

/** enum wmi_mlo_setup_status - Status code in WMI MLO setup completion event
 * @WMI_HOST_MLO_SETUP_STATUS_SUCCESS: Success
 * @WMI_HOST_MLO_SETUP_STATUS_FAILURE: Failure
 */
enum wmi_mlo_setup_status {
	WMI_HOST_MLO_SETUP_STATUS_SUCCESS,
	WMI_HOST_MLO_SETUP_STATUS_FAILURE,
};

/** struct wmi_mlo_setup_complete_params - MLO setup complete event params
 * @pdev_id: pdev id of radio on which this event is received
 * @status: status code
 * @max_ml_peer_ids: Maximum ML Peer ID's
 */
struct wmi_mlo_setup_complete_params {
	uint32_t pdev_id;
	enum wmi_mlo_setup_status status;
	uint32_t max_ml_peer_ids;
};

/** enum wmi_mlo_teardown_status - Status code in WMI MLO teardown completion
 *                                 event
 * @WMI_HOST_MLO_TEARDOWN_STATUS_SUCCESS: Success
 * @WMI_HOST_MLO_TEARDOWN_STATUS_FAILURE: Failure
 * @WMI_HOST_MLO_TEARDOWN_STATUS_ONGOING: Ongoing
 */
enum wmi_mlo_teardown_status {
	WMI_HOST_MLO_TEARDOWN_STATUS_SUCCESS,
	WMI_HOST_MLO_TEARDOWN_STATUS_FAILURE,
	WMI_HOST_MLO_TEARDOWN_STATUS_ONGOING,
};

/** struct wmi_mlo_teardown_cmpl_params - MLO setup teardown event params
 * @pdev_id: pdev id of radio on which this event is received
 * @status: Teardown status from enum wmi_mlo_teardown_status
 */
struct wmi_mlo_teardown_cmpl_params {
	uint32_t pdev_id;
	enum wmi_mlo_teardown_status status;
};
#endif
#endif
