/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_connect_info.c
 *
 * WLAN Host Device Driver implementation to update sysfs with connect
 * information
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include "wlan_hdd_sysfs_connect_info.h"
#include "qwlan_version.h"
#include "wlan_policy_mgr_ucfg.h"
#include "wlan_hdd_object_manager.h"

/**
 * wlan_hdd_version_info() - Populate driver, FW and HW version
 * @hdd_ctx: pointer to hdd context
 * @buf: output buffer to hold version info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
wlan_hdd_version_info(struct hdd_context *hdd_ctx, uint8_t *buf,
		      ssize_t buf_avail_len)
{
	ssize_t length = 0;
	int ret_val;

	ret_val = scnprintf(buf, buf_avail_len,
			    "\nVERSION DETAILS\n");
	if (ret_val <= 0)
		return length;
	length += ret_val;

	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}

	ret_val = scnprintf(buf + length, buf_avail_len - length,
			    "Host Driver Version: %s\n"
			    "Firmware Version: %d.%d.%d.%d.%d.%d\n"
			    "Hardware Version: %s\n",
			    QWLAN_VERSIONSTR,
			    hdd_ctx->fw_version_info.major_spid,
			    hdd_ctx->fw_version_info.minor_spid,
			    hdd_ctx->fw_version_info.siid,
			    hdd_ctx->fw_version_info.rel_id,
			    hdd_ctx->fw_version_info.crmid,
			    hdd_ctx->fw_version_info.sub_id,
			    hdd_ctx->target_hw_name);
	if (ret_val <= 0)
		return length;

	length += ret_val;

	return length;
}

/**
 * wlan_hdd_add_nss_info() - Populate NSS info
 * @conn_info: station connection information
 * @buf: output buffer to hold version info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
wlan_hdd_add_nss_info(struct hdd_connection_info *conn_info,
		      uint8_t *buf, ssize_t buf_avail_len)
{
	ssize_t length = 0;
	int ret_val;

	if (!conn_info->conn_flag.ht_present &&
	    !conn_info->conn_flag.vht_present)
		return length;

	ret_val = scnprintf(buf, buf_avail_len,
			    "nss = %u\n",
			    conn_info->txrate.nss);
	if (ret_val <= 0)
		return length;

	length = ret_val;
	return length;
}

/**
 * wlan_hdd_add_ht_cap_info() - Populate HT info
 * @conn_info: station connection information
 * @buf: output buffer to hold version info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
wlan_hdd_add_ht_cap_info(struct hdd_connection_info *conn_info,
			 uint8_t *buf, ssize_t buf_avail_len)
{
	struct ieee80211_ht_cap *ht_caps;
	ssize_t length = 0;
	int ret;

	if (!conn_info->conn_flag.ht_present)
		return length;

	ht_caps = &conn_info->ht_caps;
	ret = scnprintf(buf, buf_avail_len,
			"ht_cap_info = %x\n"
			"ampdu_params_info = %x\n"
			"extended_ht_cap_info = %x\n"
			"tx_BF_cap_info = %x\n"
			"antenna_selection_info = %x\n"
			"ht_rx_higest = %x\n"
			"ht_tx_params = %x\n",
			ht_caps->cap_info,
			ht_caps->ampdu_params_info,
			ht_caps->extended_ht_cap_info,
			ht_caps->tx_BF_cap_info,
			ht_caps->antenna_selection_info,
			ht_caps->mcs.rx_highest,
			ht_caps->mcs.tx_params);
	if (ret <= 0)
		return length;

	length = ret;
	return length;
}

/**
 * wlan_hdd_add_vht_cap_info() - Populate VHT info
 * @conn_info: station connection information
 * @buf: output buffer to hold version info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
wlan_hdd_add_vht_cap_info(struct hdd_connection_info *conn_info,
			  uint8_t *buf, ssize_t buf_avail_len)
{
	struct ieee80211_vht_cap *vht_caps;
	ssize_t length = 0;
	int ret;

	if (!conn_info->conn_flag.vht_present)
		return length;

	vht_caps = &conn_info->vht_caps;
	ret = scnprintf(buf, buf_avail_len,
			"vht_cap_info = %x\n"
			"rx_mcs_map = %x\n"
			"rx_highest = %x\n"
			"tx_mcs_map = %x\n"
			"tx_highest = %x\n",
			vht_caps->vht_cap_info,
			vht_caps->supp_mcs.rx_mcs_map,
			vht_caps->supp_mcs.rx_highest,
			vht_caps->supp_mcs.tx_mcs_map,
			vht_caps->supp_mcs.tx_highest);
	if (ret <= 0)
		return length;

	length = ret;
	return length;
}

/**
 * hdd_auth_type_str() - Get string for enum csr auth type
 * @auth_type: authentication id
 *
 * Return: Meaningful string for enum csr auth type
 */
static
uint8_t *hdd_auth_type_str(uint32_t auth_type)
{
	switch (auth_type) {
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		return "OPEN SYSTEM";
	case eCSR_AUTH_TYPE_SHARED_KEY:
		return "SHARED KEY";
	case eCSR_AUTH_TYPE_SAE:
		return "SAE";
	case eCSR_AUTH_TYPE_AUTOSWITCH:
		return "AUTO SWITCH";
	case eCSR_AUTH_TYPE_WPA:
		return "WPA";
	case eCSR_AUTH_TYPE_WPA_PSK:
		return "WPA PSK";
	case eCSR_AUTH_TYPE_WPA_NONE:
		return "WPA NONE";

	case eCSR_AUTH_TYPE_RSN:
		return "RSN";
	case eCSR_AUTH_TYPE_RSN_PSK:
		return "RSN PSK";
	case eCSR_AUTH_TYPE_FT_RSN:
		return "FT RSN";
	case eCSR_AUTH_TYPE_FT_RSN_PSK:
		return "FT RSN PSK";
#ifdef FEATURE_WLAN_WAPI
	case eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE:
		return "WAPI WAI CERTIFICATE";
	case eCSR_AUTH_TYPE_WAPI_WAI_PSK:
		return "WAPI WAI PSK";
#endif /* FEATURE_WLAN_WAPI */
	case eCSR_AUTH_TYPE_CCKM_WPA:
		return "CCKM WPA";
	case eCSR_AUTH_TYPE_CCKM_RSN:
		return "CCKM RSN";
	case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
		return "RSN PSK SHA256";
	case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
		return "RSN 8021X SHA256";
	case eCSR_AUTH_TYPE_FILS_SHA256:
		return "FILS SHA256";
	case eCSR_AUTH_TYPE_FILS_SHA384:
		return "FILS SHA384";
	case eCSR_AUTH_TYPE_FT_FILS_SHA256:
		return "FT FILS SHA256";
	case eCSR_AUTH_TYPE_FT_FILS_SHA384:
		return "FT FILS SHA384";
	case eCSR_AUTH_TYPE_DPP_RSN:
		return "DPP RSN";
	case eCSR_AUTH_TYPE_OWE:
		return "OWE";
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA256:
		return "SUITEB EAP SHA256";
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA384:
		return "SUITEB EAP SHA384";
	case eCSR_AUTH_TYPE_OSEN:
		return "OSEN";
	case eCSR_AUTH_TYPE_FT_SAE:
		return "FT SAE";
	case eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384:
		return "FT Suite B SHA384";
	case eCSR_NUM_OF_SUPPORT_AUTH_TYPE:
		return "NUM OF SUPPORT AUTH TYPE";
	case eCSR_AUTH_TYPE_FAILED:
		return "FAILED";
	case eCSR_AUTH_TYPE_NONE:
		return "NONE";
	}

	return "UNKNOWN";
}

/**
 * hdd_dot11_mode_str() - Get string for enum csr dot11 mode
 * @dot11mode: dot11 mode ID
 *
 * Return: Meaningful string for enum csr dot11 mode
 */
static
uint8_t *hdd_dot11_mode_str(uint32_t dot11mode)
{
	switch (dot11mode) {
	case eCSR_CFG_DOT11_MODE_11A:
		return "DOT11 MODE 11A";
	case eCSR_CFG_DOT11_MODE_11B:
		return "DOT11 MODE 11B";
	case eCSR_CFG_DOT11_MODE_11G:
		return "DOT11 MODE 11G";
	case eCSR_CFG_DOT11_MODE_11N:
		return "DOT11 MODE 11N";
	case eCSR_CFG_DOT11_MODE_11AC:
		return "DOT11 MODE 11AC";
	case eCSR_CFG_DOT11_MODE_AUTO:
		return "DOT11 MODE AUTO";
	case eCSR_CFG_DOT11_MODE_ABG:
		return "DOT11 MODE 11ABG";
	case eCSR_CFG_DOT11_MODE_11AX:
	case eCSR_CFG_DOT11_MODE_11AX_ONLY:
		return "DOT11_MODE_11AX";
	case eCSR_CFG_DOT11_MODE_11BE:
	case eCSR_CFG_DOT11_MODE_11BE_ONLY:
		return "DOT11_MODE_11BE";
	}

	return "UNKNOWN";
}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(CFG80211_11BE_BASIC)
static
uint8_t *hdd_curr_hw_mode_str(uint8_t curr_hw_mode)
{
	switch (curr_hw_mode) {
	case POLICY_MGR_HW_MODE_SINGLE:
		return "HW MODE SINGLE";
	case POLICY_MGR_HW_MODE_DBS:
		return "HW MODE DBS";
	case POLICY_MGR_HW_MODE_SBS_PASSIVE:
		return "HW MODE SBS PASSIVE";
	case POLICY_MGR_HW_MODE_SBS:
		return "HW MODE SBS";
	case POLICY_MGR_HW_MODE_DBS_SBS:
		return "HW MODE DBS SBS";
	case POLICY_MGR_HW_MODE_DBS_OR_SBS:
		return "HW MODE DBS OR SBS";
	case POLICY_MGR_HW_MODE_DBS_2G_5G:
		return "HW MODE DBS 2g/5g";
	case POLICY_MGR_HW_MODE_2G_PHYB:
		return "HW MODE 2g phyB";
	case POLICY_MGR_HW_MODE_EMLSR:
		return "HW MODE EMLSR";
	case POLICY_MGR_HW_MODE_AUX_EMLSR_SINGLE:
		return "HW MODE EMLSR AUX SINGLE";
	case POLICY_MGR_HW_MODE_AUX_EMLSR_SPLIT:
		return "HW MODE EMLSR AUX SPLIT";
	case POLICY_MGR_HW_MODE_INVALID:
		return "HW MODE INVALID";
	}

	return "UNKNOWN";
}

/**
 * wlan_hdd_connect_info() - Populate connect info
 * @adapter: pointer to sta adapter for which connect info is required
 * @buf: output buffer to hold version info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t wlan_hdd_connect_info(struct hdd_adapter *adapter, uint8_t *buf,
				     ssize_t buf_avail_len)
{
	struct wlan_hdd_link_info *link_info;
	struct hdd_station_ctx *hdd_sta_ctx;
	uint32_t len = 0;
	ssize_t length = 0;
	struct hdd_connection_info *conn_info;
	uint32_t tx_bit_rate, rx_bit_rate;
	bool is_legacy = false;
	bool is_standby = false;
	uint8_t curr_hw_mode;
	struct wlan_objmgr_vdev *vdev;

	if (!hdd_cm_is_vdev_associated(adapter->deflink)) {
		len = scnprintf(buf, buf_avail_len,
				"STA is not connected\n");
		if (len >= 0)
			return length;
	}

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter->deflink);
	if (adapter->deflink == &adapter->link_info[0] &&
	    hdd_sta_ctx->conn_info.ieee_link_id == WLAN_INVALID_LINK_ID) {
		len = scnprintf(buf + length, buf_avail_len - length,
				"CONNECTION DETAILS: Non-ML connection\n");
		is_legacy = true;
	} else {
		len = scnprintf(buf + length, buf_avail_len - length,
				"CONNECTION DETAILS: ML connection\n");
	}

	if (len <= 0)
		return length;

	length += len;
	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}

	vdev = hdd_objmgr_get_vdev_by_user(adapter->deflink, WLAN_OSIF_CM_ID);
	if (!vdev)
		return length;

	curr_hw_mode = ucfg_policy_mgr_find_current_hw_mode(
						wlan_vdev_get_psoc(vdev));
	hdd_objmgr_put_vdev_by_user(vdev, WLAN_OSIF_CM_ID);

	len = scnprintf(buf + length, buf_avail_len - length,
			"ssid: %s\n"
			"bssid: " QDF_MAC_ADDR_FMT "\n"
			"connect_time: %s\n"
			"auth_time: %s\n"
			"last_auth_type: %s\n"
			"dot11mode: %s\n"
			"current HW mode: %s\n",
			hdd_sta_ctx->conn_info.last_ssid.SSID.ssId,
			QDF_MAC_ADDR_REF(hdd_sta_ctx->conn_info.bssid.bytes),
			hdd_sta_ctx->conn_info.connect_time,
			hdd_sta_ctx->conn_info.auth_time,
			hdd_auth_type_str(hdd_sta_ctx->conn_info.last_auth_type),
			hdd_dot11_mode_str(hdd_sta_ctx->conn_info.dot11mode),
			hdd_curr_hw_mode_str(curr_hw_mode));
	if (len <= 0)
		return length;
	length += len;
	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}

	hdd_adapter_for_each_link_info(adapter, link_info) {
		hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(link_info);
		conn_info = &hdd_sta_ctx->conn_info;

		if(!is_legacy && conn_info->ieee_link_id == WLAN_INVALID_LINK_ID)
			continue;

		if (hdd_cm_is_vdev_roaming(link_info)) {
			len = scnprintf(buf + length, buf_avail_len - length,
					"Roaming is in progress");
			if (len <= 0)
				return length;

			length += len;
		}

		tx_bit_rate = cfg80211_calculate_bitrate(&conn_info->txrate);
		rx_bit_rate = cfg80211_calculate_bitrate(&conn_info->rxrate);

		if (!is_legacy) {
			len = scnprintf(buf + length, buf_avail_len - length,
					"\nlink_id: %d\n",
					conn_info->ieee_link_id);
			if (len <= 0)
				return length;

			length += len;
			if (length >= buf_avail_len) {
				hdd_debug("No sufficient buf_avail_len");
				return buf_avail_len;
			}

			if (link_info->vdev_id == WLAN_INVALID_VDEV_ID &&
			    conn_info->ieee_link_id != WLAN_INVALID_LINK_ID)
				is_standby = true;

			len = scnprintf(buf + length, buf_avail_len - length,
					"stand-by link: %d\n",
					is_standby);
			if (len <= 0)
				return length;

			length += len;
			if (length >= buf_avail_len) {
				hdd_debug("No sufficient buf_avail_len");
				return buf_avail_len;
			}
		}

		len = scnprintf(buf + length, buf_avail_len - length,
				"freq: %u\n"
				"ch_width: %s\n"
				"signal: %ddBm\n"
				"tx_bit_rate: %u\n"
				"rx_bit_rate: %u\n"
				"last_auth_type: %s\n"
				"dot11mode: %s\n",
				conn_info->chan_freq,
				hdd_ch_width_str(conn_info->ch_width),
				conn_info->signal,
				tx_bit_rate,
				rx_bit_rate,
				hdd_auth_type_str(conn_info->last_auth_type),
				hdd_dot11_mode_str(conn_info->dot11mode));

		if (len <= 0)
			return length;

		length += len;
		if (length >= buf_avail_len) {
			hdd_debug("No sufficient buf_avail_len");
			return buf_avail_len;
		}

		length += wlan_hdd_add_nss_info(conn_info, buf + length,
						buf_avail_len - length);
		if (length >= buf_avail_len) {
			hdd_debug("No sufficient buf_avail_len");
			return buf_avail_len;
		}

		length += wlan_hdd_add_ht_cap_info(conn_info, buf + length,
						   buf_avail_len - length);
		if (length >= buf_avail_len) {
			hdd_debug("No sufficient buf_avail_len");
			return buf_avail_len;
		}

		length += wlan_hdd_add_vht_cap_info(conn_info, buf + length,
						    buf_avail_len - length);
		if (is_legacy)
			return length;
	}

	return length;
}
#else
static ssize_t wlan_hdd_connect_info(struct hdd_adapter *adapter, uint8_t *buf,
				     ssize_t buf_avail_len)
{
	ssize_t length = 0;
	struct hdd_station_ctx *hdd_sta_ctx;
	struct hdd_connection_info *conn_info;
	uint32_t tx_bit_rate, rx_bit_rate;
	int ret_val;

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter->deflink);
	if (!hdd_cm_is_vdev_associated(adapter->deflink)) {
		ret_val = scnprintf(buf, buf_avail_len,
				    "\nSTA is not connected\n");
		if (ret_val >= 0)
			length = ret_val;
		return length;
	}

	ret_val = scnprintf(buf, buf_avail_len,
			    "\nCONNECTION DETAILS\n");
	if (ret_val <= 0)
		return length;
	length += ret_val;

	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}

	if (hdd_cm_is_vdev_roaming(adapter->deflink)) {
		ret_val = scnprintf(buf + length, buf_avail_len - length,
				    "Roaming is in progress");
		if (ret_val <= 0)
			return length;
		length += ret_val;
	}

	conn_info = &hdd_sta_ctx->conn_info;
	tx_bit_rate = cfg80211_calculate_bitrate(&conn_info->txrate);
	rx_bit_rate = cfg80211_calculate_bitrate(&conn_info->rxrate);

	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}
	ret_val = scnprintf(buf + length, buf_avail_len - length,
			    "ssid = %s\n"
			    "bssid = " QDF_MAC_ADDR_FMT "\n"
			    "connect_time = %s\n"
			    "auth_time = %s\n"
			    "freq = %u\n"
			    "ch_width = %s\n"
			    "signal = %ddBm\n"
			    "tx_bit_rate = %u\n"
			    "rx_bit_rate = %u\n"
			    "last_auth_type = %s\n"
			    "dot11mode = %s\n",
			    conn_info->last_ssid.SSID.ssId,
			    QDF_MAC_ADDR_REF(conn_info->bssid.bytes),
			    conn_info->connect_time,
			    conn_info->auth_time,
			    conn_info->chan_freq,
			    hdd_ch_width_str(conn_info->ch_width),
			    conn_info->signal,
			    tx_bit_rate,
			    rx_bit_rate,
			    hdd_auth_type_str(conn_info->last_auth_type),
			    hdd_dot11_mode_str(conn_info->dot11mode));

	if (ret_val <= 0)
		return length;
	length += ret_val;

	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}
	length += wlan_hdd_add_nss_info(conn_info, buf + length,
					buf_avail_len - length);

	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}

	length += wlan_hdd_add_ht_cap_info(conn_info, buf + length,
					   buf_avail_len - length);

	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}
	length += wlan_hdd_add_vht_cap_info(conn_info, buf + length,
					    buf_avail_len - length);
	return length;
}
#endif

static ssize_t
wlan_hdd_current_time_info(uint8_t *buf, ssize_t buf_avail_len)
{
	ssize_t length;
	char time_buffer[HDD_TIME_STRING_LEN];
	int ret_val;

	qdf_get_time_of_the_day_in_hr_min_sec_usec(time_buffer,
						   sizeof(time_buffer));
	ret_val = scnprintf(buf, buf_avail_len,
			    "\nTime at which this file generated = %s\n",
			    time_buffer);
	if (ret_val < 0)
		return 0;
	length = ret_val;

	return length;
}

static ssize_t __show_connect_info(struct net_device *net_dev, char *buf,
				   ssize_t buf_avail_len)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_context *hdd_ctx;
	ssize_t len;
	int ret_val;

	hdd_enter_dev(net_dev);

	len = wlan_hdd_current_time_info(buf, buf_avail_len);
	if (len >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		len = buf_avail_len;
		goto exit;
	}

	ret_val = hdd_validate_adapter(adapter);
	if (0 != ret_val)
		return len;

	if (adapter->device_mode != QDF_STA_MODE) {
		ret_val = scnprintf(buf + len, buf_avail_len - len,
				    "Interface is not operating STA Mode\n");
		if (ret_val <= 0)
			goto exit;

		len += ret_val;
		goto exit;
	}

	if (len >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		len = buf_avail_len;
		goto exit;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret_val)
		goto exit;

	len += wlan_hdd_version_info(hdd_ctx, buf + len, buf_avail_len - len);

	if (len >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		len = buf_avail_len;
		goto exit;
	}
	len += wlan_hdd_connect_info(adapter, buf + len, buf_avail_len - len);

exit:
	hdd_exit();
	return len;
}

static ssize_t show_connect_info(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;
	ssize_t buf_avail_len = SYSFS_CONNECT_INFO_BUF_SIZE;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __show_connect_info(net_dev, buf, buf_avail_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(connect_info, 0444, show_connect_info, NULL);

void hdd_sysfs_connect_info_interface_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_connect_info);
	if (error)
		hdd_err("could not create connect_info sysfs file");
}

void hdd_sysfs_connect_info_interface_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_connect_info);
}

