/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __VENUS_HFI_RESPONSE_H__
#define __VENUS_HFI_RESPONSE_H__

#include "hfi_packet.h"

int handle_response(struct msm_vidc_core *core,
		    void *response);
int validate_packet(u8 *response_pkt, u8 *core_resp_pkt,
		    u32 core_resp_pkt_size, const char *func);
bool is_valid_port(struct msm_vidc_inst *inst, u32 port,
		   const char *func);
bool is_valid_hfi_buffer_type(struct msm_vidc_inst *inst,
			      u32 buffer_type, const char *func);
int handle_system_error(struct msm_vidc_core *core,
			struct hfi_packet *pkt);
int handle_release_output_buffer(struct msm_vidc_inst *inst,
				 struct hfi_buffer *buffer);
int validate_hdr_packet(struct msm_vidc_core *core,
	struct hfi_header *hdr, const char *function);
int handle_system_response(struct msm_vidc_core *core,
				  struct hfi_header *hdr);
int handle_session_response(struct msm_vidc_inst *inst,
				   struct hfi_header *hdr);

#endif // __VENUS_HFI_RESPONSE_H__
