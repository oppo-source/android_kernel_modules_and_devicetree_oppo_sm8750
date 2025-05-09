// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <media/cam_sync.h>
#include <media/cam_defs.h>
#include <media/cam_ope.h>
#include "cam_sync_api.h"
#include "cam_node.h"
#include "cam_context.h"
#include "cam_context_utils.h"
#include "cam_ope_context.h"
#include "cam_req_mgr_util.h"
#include "cam_mem_mgr.h"
#include "cam_trace.h"
#include "cam_debug_util.h"
#include "cam_packet_util.h"

static const char ope_dev_name[] = "cam-ope";

static int cam_ope_context_dump_active_request(void *data, void *args)
{
	struct cam_context         *ctx = (struct cam_context *)data;
	struct cam_ctx_request     *req = NULL;
	struct cam_ctx_request     *req_temp = NULL;
	struct cam_hw_dump_pf_args *pf_args = (struct cam_hw_dump_pf_args *)args;
	int rc = 0;

	if (!ctx || !pf_args) {
		CAM_ERR(CAM_OPE, "Invalid ctx %pK or pf args %pK",
			ctx, pf_args);
		return -EINVAL;
	}

	CAM_INFO(CAM_OPE, "iommu fault for ope ctx %d state %d",
		ctx->ctx_id, ctx->state);

	list_for_each_entry_safe(req, req_temp,
			&ctx->active_req_list, list) {
		CAM_INFO(CAM_OPE, "Active req_id: %llu ctx_id: %u",
			req->request_id, ctx->ctx_id);

		rc = cam_context_dump_pf_info_to_hw(ctx, pf_args, &req->pf_data);
		if (rc)
			CAM_ERR(CAM_OPE, "Failed to dump pf info ctx_id: %u state: %d",
				ctx->ctx_id, ctx->state);
	}

	if (pf_args->pf_context_info.ctx_found) {
		/* Send PF notification to UMD if PF found on current CTX */
		rc = cam_context_send_pf_evt(ctx, pf_args);
		if (rc)
			CAM_ERR(CAM_OPE,
				"Failed to notify PF event to userspace rc: %d", rc);
	}

	return rc;
}

static int __cam_ope_acquire_dev_in_available(struct cam_context *ctx,
	struct cam_acquire_dev_cmd_unified *args)
{
	int rc;

	rc = cam_context_acquire_dev_to_hw(ctx, args);
	if (!rc) {
		ctx->state = CAM_CTX_ACQUIRED;
		trace_cam_context_state("OPE", ctx);
	}

	return rc;
}

static int __cam_ope_release_dev_in_acquired(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_release_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_OPE, "Unable to release device");

	ctx->state = CAM_CTX_AVAILABLE;
	trace_cam_context_state("OPE", ctx);
	return rc;
}

static int __cam_ope_start_dev_in_acquired(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_start_dev_to_hw(ctx, cmd);
	if (!rc) {
		ctx->state = CAM_CTX_READY;
		trace_cam_context_state("OPE", ctx);
	}

	return rc;
}

static int __cam_ope_flush_dev_in_ready(struct cam_context *ctx,
	struct cam_flush_dev_cmd *cmd)
{
	int rc;
	struct cam_context_utils_flush_args flush_args;

	flush_args.cmd = cmd;
	flush_args.flush_active_req = false;

	rc = cam_context_flush_dev_to_hw(ctx, &flush_args);
	if (rc)
		CAM_ERR(CAM_OPE, "Failed to flush device");

	return rc;
}

static int __cam_ope_dump_dev_in_ready(struct cam_context *ctx,
	struct cam_dump_req_cmd *cmd)
{
	int rc;

	rc = cam_context_dump_dev_to_hw(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_OPE, "Failed to dump device");

	return rc;
}

static int __cam_ope_config_dev_in_ready(struct cam_context *ctx,
	struct cam_config_dev_cmd *cmd)
{
	int rc;
	size_t len;
	uintptr_t packet_addr;

	rc = cam_mem_get_cpu_buf((int32_t) cmd->packet_handle,
		&packet_addr, &len);
	if (rc) {
		CAM_ERR(CAM_OPE, "[%s][%d] Can not get packet address",
			ctx->dev_name, ctx->ctx_id);
		rc = -EINVAL;
		return rc;
	}

	rc = cam_context_prepare_dev_to_hw(ctx, cmd);

	if (rc)
		CAM_ERR(CAM_OPE, "Failed to prepare device");

	cam_mem_put_cpu_buf((int32_t) cmd->packet_handle);
	return rc;
}

static int __cam_ope_stop_dev_in_ready(struct cam_context *ctx,
	struct cam_start_stop_dev_cmd *cmd)
{
	int rc;

	rc = cam_context_stop_dev_to_hw(ctx);
	if (rc)
		CAM_ERR(CAM_OPE, "Failed to stop device");

	ctx->state = CAM_CTX_ACQUIRED;
	trace_cam_context_state("OPE", ctx);
	return rc;
}

static int __cam_ope_release_dev_in_ready(struct cam_context *ctx,
	struct cam_release_dev_cmd *cmd)
{
	int rc;

	rc = __cam_ope_stop_dev_in_ready(ctx, NULL);
	if (rc)
		CAM_ERR(CAM_OPE, "Failed to stop device");

	rc = __cam_ope_release_dev_in_acquired(ctx, cmd);
	if (rc)
		CAM_ERR(CAM_OPE, "Failed to release device");

	return rc;
}

static int __cam_ope_handle_buf_done_in_ready(void *ctx,
	uint32_t evt_id, void *done)
{
	return cam_context_buf_done_from_hw(ctx, done, evt_id);
}

static struct cam_ctx_ops
	cam_ope_ctx_state_machine[CAM_CTX_STATE_MAX] = {
	/* Uninit */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Available */
	{
		.ioctl_ops = {
			.acquire_dev = __cam_ope_acquire_dev_in_available,
		},
		.crm_ops = {},
		.irq_ops = NULL,
	},
	/* Acquired */
	{
		.ioctl_ops = {
			.release_dev = __cam_ope_release_dev_in_acquired,
			.start_dev = __cam_ope_start_dev_in_acquired,
			.config_dev = __cam_ope_config_dev_in_ready,
			.flush_dev = __cam_ope_flush_dev_in_ready,
			.dump_dev = __cam_ope_dump_dev_in_ready,
		},
		.crm_ops = {},
		.irq_ops = __cam_ope_handle_buf_done_in_ready,
		.pagefault_ops = cam_ope_context_dump_active_request,
	},
	/* Ready */
	{
		.ioctl_ops = {
			.stop_dev = __cam_ope_stop_dev_in_ready,
			.release_dev = __cam_ope_release_dev_in_ready,
			.config_dev = __cam_ope_config_dev_in_ready,
			.flush_dev = __cam_ope_flush_dev_in_ready,
			.dump_dev = __cam_ope_dump_dev_in_ready,
		},
		.crm_ops = {},
		.irq_ops = __cam_ope_handle_buf_done_in_ready,
		.pagefault_ops = cam_ope_context_dump_active_request,
	},
	/* Flushed */
	{
		.ioctl_ops = {},
	},
	/* Activated */
	{
		.ioctl_ops = {},
		.crm_ops = {},
		.irq_ops = NULL,
		.pagefault_ops = cam_ope_context_dump_active_request,
	},
};

int cam_ope_context_init(struct cam_ope_context *ctx,
	struct cam_hw_mgr_intf *hw_intf, uint32_t ctx_id, int img_iommu_hdl)
{
	int rc;

	if ((!ctx) || (!ctx->base) || (!hw_intf)) {
		CAM_ERR(CAM_OPE, "Invalid params: %pK %pK", ctx, hw_intf);
		rc = -EINVAL;
		goto err;
	}

	rc = cam_context_init(ctx->base, ope_dev_name, CAM_OPE, ctx_id,
		NULL, hw_intf, ctx->req_base, CAM_CTX_REQ_MAX, img_iommu_hdl);
	if (rc) {
		CAM_ERR(CAM_OPE, "Camera Context Base init failed");
		goto err;
	}

	ctx->base->state_machine = cam_ope_ctx_state_machine;
	ctx->base->ctx_priv = ctx;
	ctx->ctxt_to_hw_map = NULL;

	ctx->base->max_hw_update_entries = CAM_CTX_CFG_MAX;
	ctx->base->max_in_map_entries = CAM_CTX_CFG_MAX;
	ctx->base->max_out_map_entries = CAM_CTX_CFG_MAX;
err:
	return rc;
}

int cam_ope_context_deinit(struct cam_ope_context *ctx)
{
	if ((!ctx) || (!ctx->base)) {
		CAM_ERR(CAM_OPE, "Invalid params: %pK", ctx);
		return -EINVAL;
	}

	cam_context_deinit(ctx->base);
	memset(ctx, 0, sizeof(*ctx));

	return 0;
}


