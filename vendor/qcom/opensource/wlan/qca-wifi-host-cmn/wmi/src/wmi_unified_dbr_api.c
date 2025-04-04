/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "wmi_unified_priv.h"
#include "qdf_module.h"

QDF_STATUS wmi_unified_dbr_ring_cfg(wmi_unified_t wmi_handle,
				    struct direct_buf_rx_cfg_req *cfg)
{
	if (wmi_handle->ops->send_dbr_cfg_cmd)
		return wmi_handle->ops->send_dbr_cfg_cmd(wmi_handle, cfg);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_dbr_buf_release_fixed(
			wmi_unified_t wmi_handle,
			uint8_t *evt_buf,
			struct direct_buf_rx_rsp *param)
{
	if (wmi_handle->ops->extract_dbr_buf_release_fixed)
		return wmi_handle->ops->extract_dbr_buf_release_fixed(
				wmi_handle,
				evt_buf, param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_dbr_buf_release_entry(
			wmi_unified_t wmi_handle,
			uint8_t *evt_buf, uint8_t idx,
			struct direct_buf_rx_entry *param)
{
	if (wmi_handle->ops->extract_dbr_buf_release_entry)
		return wmi_handle->ops->extract_dbr_buf_release_entry(
				wmi_handle,
				evt_buf, idx, param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_dbr_buf_metadata(
			wmi_unified_t wmi_handle,
			uint8_t *evt_buf, uint8_t idx,
			struct direct_buf_rx_metadata *param)
{
	if (wmi_handle->ops->extract_dbr_buf_metadata)
		return wmi_handle->ops->extract_dbr_buf_metadata(
				wmi_handle,
				evt_buf, idx, param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_dbr_buf_cv_metadata(
			wmi_unified_t wmi_handle,
			uint8_t *evt_buf, uint8_t idx,
			struct direct_buf_rx_cv_metadata *param)
{
	if (wmi_handle->ops->extract_dbr_buf_cv_metadata)
		return wmi_handle->ops->extract_dbr_buf_cv_metadata(
				wmi_handle,
				evt_buf, idx, param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_dbr_buf_cqi_metadata(
			wmi_unified_t wmi_handle,
			uint8_t *evt_buf, uint8_t idx,
			struct direct_buf_rx_cqi_metadata *param)
{
	if (wmi_handle->ops->extract_dbr_buf_cqi_metadata)
		return wmi_handle->ops->extract_dbr_buf_cqi_metadata(
				wmi_handle,
				evt_buf, idx, param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS wmi_extract_dbr_buf_wifi_radar_metadata(
			wmi_unified_t wmi_handle,
			uint8_t *evt_buf, uint8_t idx,
			struct direct_buf_rx_wifi_radar_metadata *param)
{
	if (wmi_handle->ops->extract_dbr_buf_wifi_radar_metadata)
		return wmi_handle->ops->extract_dbr_buf_wifi_radar_metadata(
				wmi_handle,
				evt_buf, idx, param);

	return QDF_STATUS_E_FAILURE;
}
