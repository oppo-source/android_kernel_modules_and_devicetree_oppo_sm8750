/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: csr_cmd_process.c
 *
 * Implementation for processing various commands.
 */
#include "ani_global.h"
#include "csr_inside_api.h"
#include "sme_inside.h"
#include "mac_trace.h"

/**
 * csr_msg_processor() - To process all csr msg
 * @mac_ctx: mac context
 * @msg_buf: message buffer
 *
 * This routine will handle all the message for csr to process
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_msg_processor(struct mac_context *mac_ctx, void *msg_buf)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirSmeRsp *sme_rsp = (tSirSmeRsp *) msg_buf;
	uint8_t vdev_id = sme_rsp->vdev_id;
	enum csr_roam_state cur_state;
	enum csr_roam_substate sub_state;

	cur_state = sme_get_current_roam_state(MAC_HANDLE(mac_ctx), vdev_id);
	sub_state =
		sme_get_current_roam_sub_state(MAC_HANDLE(mac_ctx), vdev_id);
	sme_debug("msg %s[0x%04X] in %s[%s] vdev %d",
		  mac_trace_get_sme_msg_string(sme_rsp->messageType),
		  sme_rsp->messageType,
		  mac_trace_getcsr_roam_state(cur_state),
		  mac_trace_getcsr_roam_sub_state(sub_state), vdev_id);

	/* Process the message based on the state of the roaming states... */
	switch (cur_state) {
	case eCSR_ROAMING_STATE_JOINED:
		/* are we in joined state */
		csr_roam_joined_state_msg_processor(mac_ctx, msg_buf);
		break;
	case eCSR_ROAMING_STATE_JOINING:
		/* are we in roaming states */
		csr_roaming_state_msg_processor(mac_ctx, msg_buf);
		break;
	default:
		if (sme_rsp->messageType == eWNI_SME_UPPER_LAYER_ASSOC_CNF) {
			tSirSmeAssocIndToUpperLayerCnf *upper_layer_assoc_cnf =
				(tSirSmeAssocIndToUpperLayerCnf *)msg_buf;
			if (upper_layer_assoc_cnf->ies)
				qdf_mem_free(upper_layer_assoc_cnf->ies);
			break;
		}

		/*
		 * For all other messages, we ignore it
		 * To work-around an issue where checking for set/remove
		 * key base on connection state is no longer workable
		 * due to failure or finding the condition meets both
		 * SAP and infra requirement.
		 */
		if (sme_rsp->messageType == eWNI_SME_SETCONTEXT_RSP ||
		    sme_rsp->messageType == eWNI_SME_DISCONNECT_DONE_IND)
			csr_roam_check_for_link_status_change(mac_ctx, sme_rsp);
		else
			sme_err("msg %s[0x%04X] not handled in %s[%s] for vdev %d",
				mac_trace_get_sme_msg_string(sme_rsp->messageType),
				sme_rsp->messageType,
				mac_trace_getcsr_roam_state(cur_state),
				mac_trace_getcsr_roam_sub_state(sub_state),
				vdev_id);

		break;
	}

	return status;
}
