/*
 * Copyright (c) 2018, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021,2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Implements public API for ipa to interact with target/WMI
 */

#include "wlan_ipa_tgt_api.h"
#include "wlan_ipa_main.h"
#include "wlan_ipa_public_struct.h"
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_lmac_if_def.h>

#ifdef IPA_OFFLOAD
QDF_STATUS tgt_ipa_uc_offload_enable_disable(struct wlan_objmgr_psoc *psoc,
					     struct ipa_uc_offload_control_params *req)
{
	struct wlan_lmac_if_tx_ops *tx_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	IPA_ENTER();

	if (!psoc) {
		ipa_err("NULL psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops)
		return QDF_STATUS_E_NULL_VALUE;

	if (tx_ops->ipa_ops.ipa_uc_offload_control_req)
		status = tx_ops->ipa_ops.ipa_uc_offload_control_req(psoc, req);

	IPA_EXIT();
	return status;
}

QDF_STATUS
tgt_ipa_intrabss_enable_disable(struct wlan_objmgr_psoc *psoc,
				struct ipa_intrabss_control_params *req)
{
	struct wlan_lmac_if_tx_ops *tx_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	IPA_ENTER();

	if (!psoc) {
		ipa_err("NULL psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops)
		return QDF_STATUS_E_NULL_VALUE;

	if (tx_ops->ipa_ops.ipa_intrabss_control_req)
		status = tx_ops->ipa_ops.ipa_intrabss_control_req(psoc, req);

	IPA_EXIT();
	return status;
}
#endif
