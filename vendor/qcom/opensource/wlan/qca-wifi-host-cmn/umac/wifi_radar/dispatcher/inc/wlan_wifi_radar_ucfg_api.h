/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _WLAN_WIFI_RADAR_UCFG_API_H_
#define _WLAN_WIFI_RADAR_UCFG_API_H_

#include <wlan_objmgr_pdev_obj.h>
#include <wlan_wifi_radar_public_structs.h>
#include <wlan_wifi_radar_utils_api.h>

/**
 * ucfg_wifi_radar_command() - function to process wifi radar commands
 * @pdev: pointer to pdev object
 * @params: config params to wifi radar
 *
 * Return: status of start capture.
 */
int ucfg_wifi_radar_command(struct wlan_objmgr_pdev *pdev,
			    struct wifi_radar_command_params *params);

/**
 * ucfg_wifi_radar_get_cal_param() - function to retrieve calinfo
 * @pdev: pointer to pdev object
 * @params: pointer to structure which hold cal info
 *
 * Return: status of fetching cal params
 */
int ucfg_wifi_radar_get_cal_param(struct wlan_objmgr_pdev *pdev,
				  struct wifi_radar_cal_info_params *params);
#endif /* _WLAN_WIFI_RADAR_UCFG_API_H_ */
