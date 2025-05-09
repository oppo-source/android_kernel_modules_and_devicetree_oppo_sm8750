// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

/*
 * Copyright © 2014 Red Hat.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/version.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fixed.h>
#include <drm/drm_connector.h>
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dp_mst_helper.h>
#else
#include <drm/drm_dp_helper.h>
#include <drm/drm_dp_mst_helper.h>
#endif

#include <linux/version.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "sde_connector.h"
#include "dp_drm.h"
#include "dp_debug.h"
#include "dp_parser.h"

#define DP_MST_DEBUG(fmt, ...) DP_DEBUG(fmt, ##__VA_ARGS__)
#define DP_MST_INFO(fmt, ...) DP_INFO(fmt, ##__VA_ARGS__)
#define DP_MST_DEBUG_V(fmt, ...) DP_DEBUG_V(fmt, ##__VA_ARGS__)
#define DP_MST_INFO_V(fmt, ...) DP_INFO_V(fmt, ##__VA_ARGS__)

#define MAX_DP_MST_DRM_ENCODERS		2
#define MAX_DP_MST_DRM_BRIDGES		2
#define HPD_STRING_SIZE			30
#define DP_MST_CONN_ID(bridge) ((bridge)->connector ? \
		(bridge)->connector->base.id : 0)

struct dp_drm_mst_fw_helper_ops {
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	int (*atomic_find_time_slots)(struct drm_atomic_state *state,
			struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port,
			int pbn);
	int (*update_payload_part1)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_topology_state *mst_state,
			struct drm_dp_mst_atomic_payload *payload);
	int (*update_payload_part2)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_atomic_state *state,
			struct drm_dp_mst_atomic_payload *payload);
#if (KERNEL_VERSION(6, 7, 0) <= LINUX_VERSION_CODE)
	void (*reset_vcpi_slots)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_topology_state *mst_state,
			struct drm_dp_mst_atomic_payload *new_payload);
#elif (KERNEL_VERSION(6, 1, 25) <= LINUX_VERSION_CODE)
	void (*reset_vcpi_slots)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_topology_state *mst_state,
			const struct drm_dp_mst_atomic_payload *old_payload,
			struct drm_dp_mst_atomic_payload *new_payload);
#else
	void (*reset_vcpi_slots)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_topology_state *mst_state,
			struct drm_dp_mst_atomic_payload *payload);
#endif
#else

	int (*atomic_find_vcpi_slots)(struct drm_atomic_state *state,
			struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port,
			int pbn, int pbn_div);
	int (*update_payload_part1)(struct drm_dp_mst_topology_mgr *mgr);
	int (*update_payload_part2)(struct drm_dp_mst_topology_mgr *mgr);
	void (*reset_vcpi_slots)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port);
#endif
	int (*atomic_release_time_slots)(struct drm_atomic_state *state,
			struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port);
	int (*calc_pbn_mode)(struct dp_display_mode *dp_mode);
	int (*find_vcpi_slots)(struct drm_dp_mst_topology_mgr *mgr, int pbn);
	bool (*allocate_vcpi)(struct drm_dp_mst_topology_mgr *mgr,
			      struct drm_dp_mst_port *port,
			      int pbn, int slots);
	int (*check_act_status)(struct drm_dp_mst_topology_mgr *mgr);
	int (*detect_port_ctx)(
		struct drm_connector *connector,
		struct drm_modeset_acquire_ctx *ctx,
		struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port);
	struct edid *(*get_edid)(struct drm_connector *connector,
		struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port);
	int (*topology_mgr_set_mst)(struct drm_dp_mst_topology_mgr *mgr,
		bool mst_state);
	void (*get_vcpi_info)(struct drm_dp_mst_topology_mgr *mgr,
		int vcpi, int *start_slot, int *num_slots);
	void (*deallocate_vcpi)(struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port);
};

struct dp_mst_sim_port_edid {
	u8 port_number;
	u8 edid[SZ_256];
	bool valid;
};

struct dp_mst_bridge {
	struct drm_bridge base;
	struct drm_private_obj obj;
	u32 id;

	bool in_use;

	struct dp_display *display;
	struct drm_encoder *encoder;

	struct drm_display_mode drm_mode;
	struct dp_display_mode dp_mode;
	struct drm_connector *connector;
	void *dp_panel;

	int vcpi;
	int pbn;
	int num_slots;
	int start_slot;

	u32 fixed_port_num;
	bool fixed_port_added;
	struct drm_connector *fixed_connector;
};

struct dp_mst_bridge_state {
	struct drm_private_state base;
	struct drm_connector *connector;
	void *dp_panel;
	int num_slots;
};

struct dp_mst_private {
	bool mst_initialized;
	struct dp_mst_caps caps;
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct dp_mst_bridge mst_bridge[MAX_DP_MST_DRM_BRIDGES];
	struct dp_display *dp_display;
	const struct dp_drm_mst_fw_helper_ops *mst_fw_cbs;
	struct mutex mst_lock;
	struct mutex edid_lock;
	enum dp_drv_state state;
	bool mst_session_state;
};

#define to_dp_mst_bridge(x)     container_of((x), struct dp_mst_bridge, base)
#define to_dp_mst_bridge_priv(x) \
		container_of((x), struct dp_mst_bridge, obj)
#define to_dp_mst_bridge_priv_state(x) \
		container_of((x), struct dp_mst_bridge_state, base)
#define to_dp_mst_bridge_state(x) \
		to_dp_mst_bridge_priv_state((x)->obj.state)

static struct drm_private_state *dp_mst_duplicate_bridge_state(
		struct drm_private_obj *obj)
{
	struct dp_mst_bridge_state *state;

	state = kmemdup(obj->state, sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base);

	return &state->base;
}

static void dp_mst_destroy_bridge_state(struct drm_private_obj *obj,
		struct drm_private_state *state)
{
	struct dp_mst_bridge_state *priv_state =
		to_dp_mst_bridge_priv_state(state);

	kfree(priv_state);
}

static const struct drm_private_state_funcs dp_mst_bridge_state_funcs = {
	.atomic_duplicate_state = dp_mst_duplicate_bridge_state,
	.atomic_destroy_state = dp_mst_destroy_bridge_state,
};

static struct dp_mst_bridge_state *dp_mst_get_bridge_atomic_state(
		struct drm_atomic_state *state, struct dp_mst_bridge *bridge)
{
	struct drm_device *dev = bridge->base.dev;

	WARN_ON(!drm_modeset_is_locked(&dev->mode_config.connection_mutex));

	return to_dp_mst_bridge_priv_state(
			drm_atomic_get_private_obj_state(state, &bridge->obj));
}

static int dp_mst_detect_port(
			struct drm_connector *connector,
			struct drm_modeset_acquire_ctx *ctx,
			struct drm_dp_mst_topology_mgr *mgr,
			struct drm_dp_mst_port *port)
{
	struct dp_mst_private *mst = container_of(mgr,
			struct dp_mst_private, mst_mgr);
	int status = connector_status_disconnected;

	if (mst->mst_session_state)
		status = drm_dp_mst_detect_port(connector, ctx, mgr, port);

	DP_MST_DEBUG("mst port status: %d, session state: %d\n",
		status, mst->mst_session_state);
	return status;
}

static void _dp_mst_get_vcpi_info(
		struct drm_dp_mst_topology_mgr *mgr,
		int vcpi, int *start_slot, int *num_slots)
{
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	struct drm_dp_mst_topology_state *state;
	struct drm_dp_mst_atomic_payload *payload;
#else
	int i;
#endif

	*start_slot = 0;
	*num_slots = 0;

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	state = to_drm_dp_mst_topology_state(mgr->base.state);
	list_for_each_entry(payload, &state->payloads, next) {
		if (payload->vcpi == vcpi) {
			*start_slot = payload->vc_start_slot;
			*num_slots = payload->time_slots;
			break;
		}
	}
#else
	mutex_lock(&mgr->payload_lock);
	for (i = 0; i < mgr->max_payloads; i++) {
		if (mgr->payloads[i].vcpi == vcpi) {
			*start_slot = mgr->payloads[i].start_slot;
			*num_slots = mgr->payloads[i].num_slots;
			break;
		}
	}
	mutex_unlock(&mgr->payload_lock);
#endif
	DP_INFO("vcpi_info. vcpi:%d, start_slot:%d, num_slots:%d\n",
			vcpi, *start_slot, *num_slots);
}

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
/**
 * dp_mst_find_vcpi_slots() - Find VCPI slots for this PBN value
 * @mgr: manager to use
 * @pbn: payload bandwidth to convert into slots.
 *
 * Calculate the number of VCPI slots that will be required for the given PBN
 * value.
 *
 * RETURNS:
 * The total slots required for this port, or error.
 */
static int dp_mst_find_vcpi_slots(struct drm_dp_mst_topology_mgr *mgr, int pbn)
{
	int num_slots;
	struct drm_dp_mst_topology_state *state;

	state = to_drm_dp_mst_topology_state(mgr->base.state);
#if (KERNEL_VERSION(6, 8, 0) <= LINUX_VERSION_CODE)
	num_slots = DIV_ROUND_UP(pbn, dfixed_trunc(state->pbn_div));
#else
	num_slots = DIV_ROUND_UP(pbn, state->pbn_div);
#endif

	/* max. time slots - one slot for MTP header */
	if (num_slots > 63)
		return -ENOSPC;
	return num_slots;
}
#endif

static int dp_mst_calc_pbn_mode(struct dp_display_mode *dp_mode)
{
	int pbn, bpp;
	bool dsc_en;
	s64 pbn_fp;
	struct dp_panel_info *pinfo = &dp_mode->timing;

	dsc_en = pinfo->comp_info.enabled;
	bpp = dsc_en ? DSC_BPP(pinfo->comp_info.dsc_info.config) : pinfo->bpp;

#if (KERNEL_VERSION(6, 8, 0) <= LINUX_VERSION_CODE)
	pbn = drm_dp_calc_pbn_mode(pinfo->pixel_clk_khz, bpp << 4);
#elif (KERNEL_VERSION(6, 7, 0) <= LINUX_VERSION_CODE)
	pbn = drm_dp_calc_pbn_mode(pinfo->pixel_clk_khz, bpp << 4, false);
#elif (KERNEL_VERSION(6, 6, 17) <= LINUX_VERSION_CODE)
	pbn = drm_dp_calc_pbn_mode(pinfo->pixel_clk_khz, bpp << 4);
#else
	pbn = drm_dp_calc_pbn_mode(pinfo->pixel_clk_khz, bpp, false);
#endif
	pbn_fp = drm_fixp_from_fraction(pbn, 1);
	pinfo->pbn_no_overhead = pbn;

	if (dsc_en)
		pbn_fp = drm_fixp_mul(pbn_fp, dp_mode->dsc_overhead_fp);

	if (dp_mode->fec_overhead_fp)
		pbn_fp = drm_fixp_mul(pbn_fp, dp_mode->fec_overhead_fp);

	pbn = drm_fixp2int(pbn_fp);
	pinfo->pbn = pbn;

	DP_DEBUG_V("pbn before overhead:%d pbn final:%d, bpp:%d\n", pinfo->pbn_no_overhead, pbn,
			bpp);

	return pbn;
}

static const struct dp_drm_mst_fw_helper_ops drm_dp_mst_fw_helper_ops = {
#if (KERNEL_VERSION(6, 7, 0) <= LINUX_VERSION_CODE)
	.calc_pbn_mode             = dp_mst_calc_pbn_mode,
	.find_vcpi_slots           = dp_mst_find_vcpi_slots,
	.atomic_find_time_slots    = drm_dp_atomic_find_time_slots,
	.update_payload_part1      = drm_dp_add_payload_part1,
	.check_act_status          = drm_dp_check_act_status,
	.update_payload_part2      = drm_dp_add_payload_part2,
	.detect_port_ctx           = dp_mst_detect_port,
	.get_edid                  = drm_dp_mst_get_edid,
	.topology_mgr_set_mst      = drm_dp_mst_topology_mgr_set_mst,
	.get_vcpi_info             = _dp_mst_get_vcpi_info,
	.atomic_release_time_slots = drm_dp_atomic_release_time_slots,
	.reset_vcpi_slots          = drm_dp_remove_payload_part1,
#elif (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	.calc_pbn_mode             = dp_mst_calc_pbn_mode,
	.find_vcpi_slots           = dp_mst_find_vcpi_slots,
	.atomic_find_time_slots    = drm_dp_atomic_find_time_slots,
	.update_payload_part1      = drm_dp_add_payload_part1,
	.check_act_status          = drm_dp_check_act_status,
	.update_payload_part2      = drm_dp_add_payload_part2,
	.detect_port_ctx           = dp_mst_detect_port,
	.get_edid                  = drm_dp_mst_get_edid,
	.topology_mgr_set_mst      = drm_dp_mst_topology_mgr_set_mst,
	.get_vcpi_info             = _dp_mst_get_vcpi_info,
	.atomic_release_time_slots = drm_dp_atomic_release_time_slots,
	.reset_vcpi_slots          = drm_dp_remove_payload,
#else
	.calc_pbn_mode             = dp_mst_calc_pbn_mode,
	.find_vcpi_slots           = drm_dp_find_vcpi_slots,
	.atomic_find_vcpi_slots    = drm_dp_atomic_find_vcpi_slots,
	.allocate_vcpi             = drm_dp_mst_allocate_vcpi,
	.update_payload_part1      = drm_dp_update_payload_part1,
	.check_act_status          = drm_dp_check_act_status,
	.update_payload_part2      = drm_dp_update_payload_part2,
	.detect_port_ctx           = dp_mst_detect_port,
	.get_edid                  = drm_dp_mst_get_edid,
	.topology_mgr_set_mst      = drm_dp_mst_topology_mgr_set_mst,
	.get_vcpi_info             = _dp_mst_get_vcpi_info,
	.atomic_release_time_slots = drm_dp_atomic_release_vcpi_slots,
	.reset_vcpi_slots          = drm_dp_mst_reset_vcpi_slots,
	.deallocate_vcpi           = drm_dp_mst_deallocate_vcpi,
#endif
};

/* DP MST Bridge OPs */

static int dp_mst_bridge_attach(struct drm_bridge *dp_bridge,
				enum drm_bridge_attach_flags flags)
{
	struct dp_mst_bridge *bridge;

	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY);

	if (!dp_bridge) {
		DP_ERR("Invalid params\n");
		return -EINVAL;
	}

	bridge = to_dp_mst_bridge(dp_bridge);

	DP_MST_DEBUG("mst bridge [%d] attached\n", bridge->id);

	return 0;
}

static bool dp_mst_bridge_mode_fixup(struct drm_bridge *drm_bridge,
				  const struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	bool ret = true;
	struct dp_display_mode dp_mode;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;
	struct drm_crtc_state *crtc_state;
	struct dp_mst_bridge_state *bridge_state;

	DP_MST_DEBUG_V("enter\n");

	if (!drm_bridge || !mode || !adjusted_mode) {
		DP_ERR("Invalid params\n");
		ret = false;
		goto end;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, DP_MST_CONN_ID(bridge));

	crtc_state = container_of(mode, struct drm_crtc_state, mode);
	bridge_state = dp_mst_get_bridge_atomic_state(crtc_state->state,
			bridge);
	if (IS_ERR(bridge_state)) {
		DP_ERR("invalid bridge state\n");
		ret = false;
		goto end;
	}

	if (!bridge_state->dp_panel) {
		DP_ERR("Invalid dp_panel\n");
		ret = false;
		goto end;
	}

	dp = bridge->display;

	dp->convert_to_dp_mode(dp, bridge_state->dp_panel, mode, &dp_mode);
	dp->clear_reservation(dp, bridge_state->dp_panel);
	convert_to_drm_mode(&dp_mode, adjusted_mode);

	DP_MST_DEBUG("mst bridge [%d] mode:%s fixup\n", bridge->id, mode->name);
end:
	return ret;
}

static int _dp_mst_compute_config(struct drm_atomic_state *state,
		struct dp_mst_private *mst, struct drm_connector *connector,
		struct dp_display_mode *mode)
{
	int slots = 0, pbn;
	struct sde_connector *c_conn = to_sde_connector(connector);
	int rc = 0;
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	struct drm_dp_mst_topology_state *mst_state;
#endif
#if (KERNEL_VERSION(6, 8, 0) <= LINUX_VERSION_CODE)
	int pbn_div;
#endif

	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, connector->base.id);

	pbn = mst->mst_fw_cbs->calc_pbn_mode(mode);

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	mst_state = to_drm_dp_mst_topology_state(mst->mst_mgr.base.state);

#if (KERNEL_VERSION(6, 8, 0) <= LINUX_VERSION_CODE)
	if (!dfixed_trunc(mst_state->pbn_div)) {
		pbn_div = mst->dp_display->get_mst_pbn_div(mst->dp_display);
		mst_state->pbn_div.full = dfixed_const(pbn_div);
	}
#else
	if (!mst_state->pbn_div)
		mst_state->pbn_div = mst->dp_display->get_mst_pbn_div(mst->dp_display);
#endif
	rc = mst->mst_fw_cbs->atomic_find_time_slots(state, &mst->mst_mgr, c_conn->mst_port, pbn);
	if (rc < 0) {
		DP_ERR("conn:%d failed to find vcpi slots. pbn:%d, rc:%d\n",
				connector->base.id, pbn, rc);
		goto end;
	}

	slots = rc;

	rc = drm_dp_mst_atomic_check(state);
	if (rc) {
		DP_ERR("conn:%d mst atomic check failed: rc=%d\n", connector->base.id, rc);
		slots = 0;
		goto end;
	}
#else
	slots = mst->mst_fw_cbs->atomic_find_vcpi_slots(state,
			&mst->mst_mgr, c_conn->mst_port, pbn, 0);
	if (slots < 0) {
		DP_ERR("conn:%d failed to find vcpi slots. pbn:%d, slots:%d\n",
				connector->base.id, pbn, slots);
		rc = slots;
		slots = 0;
		goto end;
	}
#endif

end:
	DP_MST_DEBUG("conn:%d pbn:%d slots:%d rc:%d\n", connector->base.id, pbn, slots, rc);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, connector->base.id, pbn, slots, rc);

	return (rc < 0 ? rc : slots);
}

static void _dp_mst_update_timeslots(struct dp_mst_private *mst,
		struct dp_mst_bridge *mst_bridge, struct drm_dp_mst_port *port)
{
	int i;
	struct dp_mst_bridge *dp_bridge;
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_dp_mst_atomic_payload *payload;
	int prev_start = 0;
	int prev_slots = 0;

	mst_state = to_drm_dp_mst_topology_state(mst->mst_mgr.base.state);
	payload = drm_atomic_get_mst_payload_state(mst_state, port);

	if (!payload) {
		DP_ERR("mst bridge [%d] update_timeslots failed, null payload\n",
				mst_bridge->id);
		return;
	}

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		dp_bridge = &mst->mst_bridge[i];
		if (mst_bridge == dp_bridge) {
			/*
			 * When a payload was removed make sure to move any payloads after it
			 * to the left so all payloads are aligned to the left.
			 */
			if (payload->vc_start_slot < 0) {
				// cache the payload
				prev_start = dp_bridge->start_slot;
				prev_slots = dp_bridge->num_slots;
				dp_bridge->pbn = 0;
				dp_bridge->start_slot = 1;
				dp_bridge->num_slots = 0;
				dp_bridge->vcpi = 0;
			} else { //add payload
				dp_bridge->pbn = payload->pbn;
				dp_bridge->start_slot = payload->vc_start_slot;
				dp_bridge->num_slots = payload->time_slots;
				dp_bridge->vcpi = payload->vcpi;
			}
		}
	}

	// Now commit all the updated payloads
	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		dp_bridge = &mst->mst_bridge[i];

		//Shift payloads to the left if there was a removed payload.
		if ((payload->vc_start_slot < 0) && (dp_bridge->start_slot > prev_start)) {
			dp_bridge->start_slot -= prev_slots;
		}

		mst->dp_display->set_stream_info(mst->dp_display, dp_bridge->dp_panel,
				dp_bridge->id, dp_bridge->start_slot, dp_bridge->num_slots,
				dp_bridge->pbn, dp_bridge->vcpi);
		DP_INFO("conn:%d vcpi:%d start_slot:%d num_slots:%d, pbn:%d\n",
			DP_MST_CONN_ID(dp_bridge), dp_bridge->vcpi, dp_bridge->start_slot,
			dp_bridge->num_slots, dp_bridge->pbn);
	}
#else
	int pbn, start_slot, num_slots;

	mst->mst_fw_cbs->update_payload_part1(&mst->mst_mgr);

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		dp_bridge = &mst->mst_bridge[i];

		pbn = 0;
		start_slot = 0;
		num_slots = 0;

		if (dp_bridge->vcpi) {
			mst->mst_fw_cbs->get_vcpi_info(&mst->mst_mgr,
					dp_bridge->vcpi,
					&start_slot, &num_slots);
			pbn = dp_bridge->pbn;
		}

		if (mst_bridge == dp_bridge)
			dp_bridge->num_slots = num_slots;

		mst->dp_display->set_stream_info(mst->dp_display,
				dp_bridge->dp_panel,
				dp_bridge->id, start_slot, num_slots, pbn,
				dp_bridge->vcpi);

		DP_INFO("conn:%d vcpi:%d start_slot:%d num_slots:%d, pbn:%d\n",
			DP_MST_CONN_ID(dp_bridge), dp_bridge->vcpi,
			start_slot, num_slots, pbn);
	}
#endif
}

static void _dp_mst_update_single_timeslot(struct dp_mst_private *mst,
		struct dp_mst_bridge *mst_bridge)
{
	int pbn = 0, start_slot = 0, num_slots = 0;

	if (mst->state == PM_SUSPEND) {
		if (mst_bridge->vcpi) {
			mst->mst_fw_cbs->get_vcpi_info(&mst->mst_mgr,
					mst_bridge->vcpi,
					&start_slot, &num_slots);
			pbn = mst_bridge->pbn;
		}

		mst_bridge->num_slots = num_slots;

		mst->dp_display->set_stream_info(mst->dp_display,
				mst_bridge->dp_panel,
				mst_bridge->id, start_slot, num_slots, pbn,
				mst_bridge->vcpi);
	}
}

static int _dp_mst_bridge_pre_enable_part1(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct sde_connector *c_conn =
		to_sde_connector(dp_bridge->connector);
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct drm_dp_mst_port *port = c_conn->mst_port;
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_dp_mst_atomic_payload *payload;
#endif
	bool ret;
	int pbn, slots;
	int rc = 0;

	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, DP_MST_CONN_ID(dp_bridge));

	/* skip mst specific disable operations during suspend */
	if (mst->state == PM_SUSPEND) {
		dp_display->wakeup_phy_layer(dp_display, true);
		drm_dp_send_power_updown_phy(&mst->mst_mgr, port, true);
		dp_display->wakeup_phy_layer(dp_display, false);
		_dp_mst_update_single_timeslot(mst, dp_bridge);
		return rc;
	}

	pbn = mst->mst_fw_cbs->calc_pbn_mode(&dp_bridge->dp_mode);

	slots = mst->mst_fw_cbs->find_vcpi_slots(&mst->mst_mgr, pbn);

	DP_INFO("conn:%d pbn:%d, slots:%d\n", DP_MST_CONN_ID(dp_bridge), pbn, slots);

	ret = false;
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	mst_state = to_drm_dp_mst_topology_state(mst->mst_mgr.base.state);
	payload = drm_atomic_get_mst_payload_state(mst_state, port);
	if (!payload || payload->time_slots <= 0) {
		DP_ERR("time slots not allocated for conn:%d\n", DP_MST_CONN_ID(dp_bridge));
		rc = -EINVAL;
		goto end;
	}

	drm_dp_mst_update_slots(mst_state, DP_CAP_ANSI_8B10B);

	rc = mst->mst_fw_cbs->update_payload_part1(&mst->mst_mgr,
			mst_state, payload);
	if (rc) {
		DP_ERR("payload allocation failure for conn:%d\n", DP_MST_CONN_ID(dp_bridge));
		goto end;
	}

#else
	ret = mst->mst_fw_cbs->allocate_vcpi(&mst->mst_mgr, port, pbn, slots);
	if (!ret) {
		DP_ERR("mst: failed to allocate vcpi. bridge:%d\n", dp_bridge->id);
		rc = -EINVAL;
		goto end;
	}

	dp_bridge->vcpi = port->vcpi.vcpi;
	dp_bridge->pbn = pbn;
#endif
	_dp_mst_update_timeslots(mst, dp_bridge, port);

end:
	return rc;
}

static void _dp_mst_bridge_pre_enable_part2(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	struct sde_connector *c_conn = to_sde_connector(dp_bridge->connector);
	struct drm_dp_mst_port *port = c_conn->mst_port;
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_dp_mst_atomic_payload *payload;
#endif

	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, DP_MST_CONN_ID(dp_bridge));

	/* skip mst specific disable operations during suspend */
	if (mst->state == PM_SUSPEND)
		return;

	mst->mst_fw_cbs->check_act_status(&mst->mst_mgr);

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	mst_state = to_drm_dp_mst_topology_state(mst->mst_mgr.base.state);
	payload = drm_atomic_get_mst_payload_state(mst_state, port);

	if (!payload) {
		DP_ERR("mst bridge [%d] _pre enable part-2 failed, null payload\n", dp_bridge->id);
		return;
	}

	if (!payload->port) {
		DP_ERR("mst bridge [%d] _pre enable part-2 failed, null port\n", dp_bridge->id);
		return;
	}

	if (!payload->port->connector) {
		DP_ERR("mst bridge [%d] _pre enable part-2 failed, null connector\n",
				dp_bridge->id);
		return;
	}

	if (payload->vc_start_slot == -1) {
		DP_ERR("mst bridge [%d] _pre enable part-2 failed, payload alloc part 1 failed\n",
				dp_bridge->id);
		return;
	}

	mst->mst_fw_cbs->update_payload_part2(&mst->mst_mgr, mst_state->base.state, payload);
#else
	mst->mst_fw_cbs->update_payload_part2(&mst->mst_mgr);
#endif
	DP_MST_DEBUG("mst bridge [%d] _pre enable part-2 complete\n",
			dp_bridge->id);
}

static void _dp_mst_bridge_pre_disable_part1(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct sde_connector *c_conn =
		to_sde_connector(dp_bridge->connector);
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct drm_dp_mst_port *port = c_conn->mst_port;
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_dp_mst_atomic_payload *payload;
#endif
	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, DP_MST_CONN_ID(dp_bridge));

	/* skip mst specific disable operations during suspend */
	if (mst->state == PM_SUSPEND) {
		_dp_mst_update_single_timeslot(mst, dp_bridge);
		return;
	}

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	mst_state = to_drm_dp_mst_topology_state(mst->mst_mgr.base.state);
	payload = drm_atomic_get_mst_payload_state(mst_state, port);

	if (!payload) {
		DP_ERR("mst bridge [%d] _pre disable part-1 failed, null payload\n",
				dp_bridge->id);
		return;
	}

#if (KERNEL_VERSION(6, 7, 0) <= LINUX_VERSION_CODE)
	mst->mst_fw_cbs->reset_vcpi_slots(&mst->mst_mgr, mst_state, payload);
#elif (KERNEL_VERSION(6, 1, 25) <= LINUX_VERSION_CODE)
	mst->mst_fw_cbs->reset_vcpi_slots(&mst->mst_mgr, mst_state, payload, payload);
#else
	mst->mst_fw_cbs->reset_vcpi_slots(&mst->mst_mgr, mst_state, payload);
#endif
#else
	mst->mst_fw_cbs->reset_vcpi_slots(&mst->mst_mgr, port);
#endif
	_dp_mst_update_timeslots(mst, dp_bridge, port);

	DP_MST_DEBUG("mst bridge [%d] _pre disable part-1 complete\n",
			dp_bridge->id);
}

static void _dp_mst_bridge_pre_disable_part2(struct dp_mst_bridge *dp_bridge)
{
	struct dp_display *dp_display = dp_bridge->display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct sde_connector *c_conn =
		to_sde_connector(dp_bridge->connector);
	struct drm_dp_mst_port *port = c_conn->mst_port;

	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY,  DP_MST_CONN_ID(dp_bridge));

	/* skip mst specific disable operations during suspend */
	if (mst->state == PM_SUSPEND) {
		dp_display->wakeup_phy_layer(dp_display, true);
		drm_dp_send_power_updown_phy(&mst->mst_mgr, port, false);
		dp_display->wakeup_phy_layer(dp_display, false);
		return;
	}

	mst->mst_fw_cbs->check_act_status(&mst->mst_mgr);

#if (KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE)
	mst->mst_fw_cbs->update_payload_part2(&mst->mst_mgr);

	port->vcpi.vcpi = dp_bridge->vcpi;
	mst->mst_fw_cbs->deallocate_vcpi(&mst->mst_mgr, port);
	dp_bridge->vcpi = 0;
	dp_bridge->pbn = 0;
#endif

	DP_MST_DEBUG("mst bridge [%d] _pre disable part-2 complete\n",
			dp_bridge->id);
}

static void dp_mst_bridge_pre_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;
	struct dp_mst_private *mst;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	DP_MST_DEBUG_V("enter\n");

	bridge = to_dp_mst_bridge(drm_bridge);
	dp = bridge->display;

	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, DP_MST_CONN_ID(bridge));
	mst = dp->dp_mst_prv_info;

	mutex_lock(&mst->mst_lock);

	/* By this point mode should have been validated through mode_fixup */
	rc = dp->set_mode(dp, bridge->dp_panel, &bridge->dp_mode);
	if (rc) {
		DP_ERR("[%d] failed to perform a mode set, rc=%d\n",
		       bridge->id, rc);
		goto end;
	}

	rc = dp->prepare(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("[%d] DP display prepare failed, rc=%d\n",
		       bridge->id, rc);
		goto end;
	}

	rc = _dp_mst_bridge_pre_enable_part1(bridge);
	if (rc) {
		DP_ERR("[%d] DP display pre-enable failed, rc=%d\n", bridge->id, rc);
		dp->unprepare(dp, bridge->dp_panel);
		goto end;
	}

	rc = dp->enable(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("[%d] DP display enable failed, rc=%d\n",
		       bridge->id, rc);
		dp->unprepare(dp, bridge->dp_panel);
		goto end;
	} else {
		_dp_mst_bridge_pre_enable_part2(bridge);
	}

	DP_MST_INFO("conn:%d mode:%s fps:%d dsc:%d vcpi:%d slots:%d to %d\n",
			DP_MST_CONN_ID(bridge), bridge->drm_mode.name,
			drm_mode_vrefresh(&bridge->drm_mode),
			bridge->dp_mode.timing.comp_info.enabled,
			bridge->vcpi, bridge->start_slot,
			bridge->start_slot + bridge->num_slots);
end:
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, DP_MST_CONN_ID(bridge));
	mutex_unlock(&mst->mst_lock);
}

static void dp_mst_bridge_enable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, DP_MST_CONN_ID(bridge));

	dp = bridge->display;

	rc = dp->post_enable(dp, bridge->dp_panel);
	if (rc) {
		DP_ERR("mst bridge [%d] post enable failed, rc=%d\n",
		       bridge->id, rc);
		return;
	}

	DP_MST_INFO("mst bridge:%d conn:%d post enable complete\n",
			bridge->id, DP_MST_CONN_ID(bridge));
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, DP_MST_CONN_ID(bridge));
}

static void dp_mst_bridge_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;
	struct dp_mst_private *mst;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	DP_MST_DEBUG_V("enter\n");

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, DP_MST_CONN_ID(bridge));
	dp = bridge->display;

	mst = dp->dp_mst_prv_info;

	sde_connector_helper_bridge_disable(bridge->connector);

	mutex_lock(&mst->mst_lock);

	_dp_mst_bridge_pre_disable_part1(bridge);

	rc = dp->pre_disable(dp, bridge->dp_panel);
	if (rc)
		DP_ERR("[%d] DP display pre disable failed, rc=%d\n",
		       bridge->id, rc);

	_dp_mst_bridge_pre_disable_part2(bridge);

	DP_MST_INFO("mst bridge:%d conn:%d disable complete\n", bridge->id,
			DP_MST_CONN_ID(bridge));
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, DP_MST_CONN_ID(bridge));
	mutex_unlock(&mst->mst_lock);
}

static void dp_mst_bridge_post_disable(struct drm_bridge *drm_bridge)
{
	int rc = 0, conn = 0;
	struct dp_mst_bridge *bridge;
	struct dp_display *dp;
	struct dp_mst_private *mst;

	if (!drm_bridge) {
		DP_ERR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		DP_ERR("Invalid connector\n");
		return;
	}

	conn = DP_MST_CONN_ID(bridge);

	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, conn);

	dp = bridge->display;
	mst = dp->dp_mst_prv_info;

	rc = dp->disable(dp, bridge->dp_panel);
	if (rc)
		DP_MST_INFO("bridge:%d conn:%d display disable failed, rc=%d\n",
				bridge->id, conn, rc);

	rc = dp->unprepare(dp, bridge->dp_panel);
	if (rc)
		DP_MST_INFO("bridge:%d conn:%d display unprepare failed, rc=%d\n",
				bridge->id, conn, rc);

	bridge->connector = NULL;
	bridge->dp_panel =  NULL;

	DP_MST_INFO("mst bridge:%d conn:%d post disable complete\n",
			bridge->id, conn);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, conn);
}

static void dp_mst_bridge_mode_set(struct drm_bridge *drm_bridge,
				const struct drm_display_mode *mode,
				const struct drm_display_mode *adjusted_mode)
{
	struct dp_mst_bridge *bridge;
	struct dp_mst_bridge_state *dp_bridge_state;
	struct dp_display *dp;

	DP_MST_DEBUG_V("enter\n");

	if (!drm_bridge || !mode || !adjusted_mode) {
		DP_ERR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);

	dp_bridge_state = to_dp_mst_bridge_state(bridge);
	bridge->connector = dp_bridge_state->connector;
	bridge->dp_panel = dp_bridge_state->dp_panel;
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, DP_MST_CONN_ID(bridge));

	dp = bridge->display;

	memset(&bridge->dp_mode, 0x0, sizeof(struct dp_display_mode));
	memcpy(&bridge->drm_mode, adjusted_mode, sizeof(bridge->drm_mode));
	dp->convert_to_dp_mode(dp, bridge->dp_panel, adjusted_mode,
			&bridge->dp_mode);
	dp->clear_reservation(dp, dp_bridge_state->dp_panel);

	DP_MST_INFO("mst bridge:%d conn:%d mode set complete %s\n", bridge->id,
			DP_MST_CONN_ID(bridge), mode->name);
}

/* DP MST Bridge APIs */

static struct drm_connector *
dp_mst_drm_fixed_connector_init(struct dp_display *dp_display,
				struct drm_encoder *encoder);

static const struct drm_bridge_funcs dp_mst_bridge_ops = {
	.attach       = dp_mst_bridge_attach,
	.mode_fixup   = dp_mst_bridge_mode_fixup,
	.pre_enable   = dp_mst_bridge_pre_enable,
	.enable       = dp_mst_bridge_enable,
	.disable      = dp_mst_bridge_disable,
	.post_disable = dp_mst_bridge_post_disable,
	.mode_set     = dp_mst_bridge_mode_set,
};

int dp_mst_drm_bridge_init(void *data, struct drm_encoder *encoder)
{
	int rc = 0;
	struct dp_mst_bridge *bridge = NULL;
	struct dp_mst_bridge_state *state;
	struct drm_device *dev;
	struct dp_display *display = data;
	struct msm_drm_private *priv = NULL;
	struct dp_mst_private *mst = display->dp_mst_prv_info;
	int i;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (!mst->mst_bridge[i].in_use) {
			bridge = &mst->mst_bridge[i];
			bridge->encoder = encoder;
			bridge->in_use = true;
			bridge->id = i;
			break;
		}
	}

	if (i == MAX_DP_MST_DRM_BRIDGES) {
		DP_ERR("mst supports only %d bridges\n", i);
		rc = -EACCES;
		goto end;
	}

	dev = display->drm_dev;
	bridge->display = display;
	bridge->base.funcs = &dp_mst_bridge_ops;
	bridge->base.encoder = encoder;

	priv = dev->dev_private;

	rc = drm_bridge_attach(encoder, &bridge->base, NULL, 0);
	if (rc) {
		DP_ERR("failed to attach bridge, rc=%d\n", rc);
		goto end;
	}

	priv->bridges[priv->num_bridges++] = &bridge->base;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state == NULL) {
		rc = -ENOMEM;
		goto end;
	}

	drm_atomic_private_obj_init(dev, &bridge->obj,
				    &state->base,
				    &dp_mst_bridge_state_funcs);

	DP_MST_DEBUG("mst drm bridge init. bridge id:%d\n", i);

	/*
	 * If fixed topology port is defined, connector will be created
	 * immediately.
	 */
	rc = display->mst_get_fixed_topology_port(display, bridge->id,
			&bridge->fixed_port_num);
	if (!rc) {
		bridge->fixed_connector =
			dp_mst_drm_fixed_connector_init(display,
				bridge->encoder);
		if (bridge->fixed_connector == NULL) {
			DP_ERR("failed to create fixed connector\n");
			kfree(state);
			rc = -ENOMEM;
			goto end;
		}
	}

	return 0;

end:
	return rc;
}

void dp_mst_drm_bridge_deinit(void *display)
{
	DP_MST_DEBUG("mst bridge deinit\n");
}

/* DP MST Connector OPs */

static int
dp_mst_connector_detect(struct drm_connector *connector,
		struct drm_modeset_acquire_ctx *ctx,
		bool force,
		void *display)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = c_conn->display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct dp_panel *dp_panel;
	enum drm_connector_status status;

	DP_MST_DEBUG_V("enter:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY);

	dp_panel = c_conn->drv_panel;

	if (dp_panel->mst_hide)
		return connector_status_disconnected;

	status = mst->mst_fw_cbs->detect_port_ctx(connector,
			ctx, &mst->mst_mgr, c_conn->mst_port);

	DP_MST_INFO("conn:%d status:%d\n", connector->base.id, status);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, connector->base.id, status);

	return (int)status;
}

void dp_mst_clear_edid_cache(void *dp_display) {
	struct dp_display *dp = dp_display;
	struct dp_mst_private *mst;
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *conn;
	struct sde_connector *c_conn;

	DP_MST_DEBUG_V("enter:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY);

	if (!dp) {
		DP_ERR("invalid input\n");
		return;
	}

	mst = dp->dp_mst_prv_info;

	drm_connector_list_iter_begin(dp->drm_dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		c_conn = to_sde_connector(conn);
		if (!c_conn->mst_port)
			continue;

		mutex_lock(&mst->edid_lock);
		kfree(c_conn->cached_edid);
		c_conn->cached_edid = NULL;
		mutex_unlock(&mst->edid_lock);
	}

	drm_connector_list_iter_end(&conn_iter);

	DP_MST_DEBUG_V("exit:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT);
}

static int dp_mst_connector_get_modes(struct drm_connector *connector,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct dp_display_mode *dp_mode = NULL;
	int rc = 0;
	struct edid *edid = NULL;

	DP_MST_DEBUG_V("enter:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, connector->base.id);

	mutex_lock(&mst->edid_lock);

	if (c_conn->cached_edid)
		goto duplicate_edid;

	mutex_unlock(&mst->edid_lock);

	edid = mst->mst_fw_cbs->get_edid(connector,
			&mst->mst_mgr, c_conn->mst_port);

	if (!edid) {
		DP_WARN("get edid failed. id: %d\n", connector->base.id);
		goto end;
	}

	mutex_lock(&mst->edid_lock);
	c_conn->cached_edid = edid;

duplicate_edid:

	edid = drm_edid_duplicate(c_conn->cached_edid);

	mutex_unlock(&mst->edid_lock);

	if (IS_ERR(edid)) {
		DP_MST_DEBUG("edid duplication failed. id: %d\n",
				connector->base.id);
		goto end;
	}

	rc = dp_display->mst_connector_update_edid(dp_display,
			connector, edid);

end:
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, connector->base.id, rc);
	if (rc <= 0) {
		DP_WARN("conn:%d has no modes, adding failsafe. rc=%d\n", connector->base.id, rc);
		dp_mode = kzalloc(sizeof(*dp_mode),  GFP_KERNEL);
		if (!dp_mode)
			return 0;

		init_failsafe_mode(dp_mode);
		rc = dp_connector_add_custom_mode(connector, dp_mode);
	} else {
		DP_MST_INFO("conn:%d has %d modes\n", connector->base.id, rc);
	}

	return rc;
}

enum drm_mode_status dp_mst_connector_mode_valid(
		struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst;
	struct sde_connector *c_conn;
	struct drm_dp_mst_port *mst_port;
	struct dp_display_mode dp_mode;
	struct dp_panel *dp_panel;
	uint16_t full_pbn, required_pbn;
	int available_slots, required_slots;
	struct dp_mst_bridge_state *dp_bridge_state;
	int i, vrefresh, slots_in_use = 0, active_enc_cnt = 0;
	const u32 tot_slots = 63;

	if (!connector || !mode || !display) {
		DP_ERR("invalid input\n");
		return 0;
	}

	mst = dp_display->dp_mst_prv_info;
	c_conn = to_sde_connector(connector);
	mst_port = c_conn->mst_port;
	dp_panel = c_conn->drv_panel;

	if (!dp_panel || !mst_port)
		return MODE_ERROR;

	vrefresh = drm_mode_vrefresh(mode);

	/* As per spec, failsafe mode should always be present */
	if ((mode->hdisplay == 640) && (mode->vdisplay == 480) && (mode->clock == 25175))
		goto validate_mode;

	if (dp_panel->mode_override && (mode->hdisplay != dp_panel->hdisplay ||
			mode->vdisplay != dp_panel->vdisplay ||
			vrefresh != dp_panel->vrefresh ||
			mode->picture_aspect_ratio != dp_panel->aspect_ratio))
		return MODE_BAD;

	/* dp bridge state is protected by drm_mode_config.connection_mutex */
	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		dp_bridge_state = to_dp_mst_bridge_state(&mst->mst_bridge[i]);
		if (dp_bridge_state->connector &&
				dp_bridge_state->connector != connector) {
			active_enc_cnt++;
			slots_in_use += dp_bridge_state->num_slots;
		}
	}

	if (active_enc_cnt < DP_STREAM_MAX) {
		full_pbn = mst_port->full_pbn;
		available_slots = tot_slots - slots_in_use;
	} else {
		DP_DEBUG("all mst streams are active\n");
		return MODE_BAD;
	}

	dp_display->convert_to_dp_mode(dp_display, c_conn->drv_panel,
			mode, &dp_mode);

	required_pbn = mst->mst_fw_cbs->calc_pbn_mode(&dp_mode);
	required_slots = mst->mst_fw_cbs->find_vcpi_slots(
			&mst->mst_mgr, required_pbn);

	if (required_pbn > full_pbn || required_slots > available_slots) {
		DP_DEBUG("mode:%s not supported. pbn %d vs %d slots %d vs %d\n",
				mode->name, required_pbn, full_pbn,
				required_slots, available_slots);
		return MODE_BAD;
	}

validate_mode:
	return dp_display->validate_mode(dp_display, dp_panel, mode, avail_res);
}

int dp_mst_connector_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_sub_mode *sub_mode,
		struct msm_mode_info *mode_info,
		void *display,
		const struct msm_resource_caps_info *avail_res)
{
	int rc;

	DP_MST_DEBUG_V("enter:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, connector->base.id);

	rc = dp_connector_get_mode_info(connector, drm_mode, NULL, mode_info,
			display, avail_res);

	DP_MST_DEBUG_V("mst connector:%d get mode info. rc:%d\n",
			connector->base.id, rc);

	DP_MST_DEBUG_V("exit:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, connector->base.id);

	return rc;
}

static struct drm_encoder *
dp_mst_atomic_best_encoder(struct drm_connector *connector,
			void *display, struct drm_connector_state *state)
{
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct sde_connector *conn = to_sde_connector(connector);
	struct drm_encoder *enc = NULL;
	struct dp_mst_bridge_state *bridge_state;
	u32 i;

	if (state->best_encoder)
		return state->best_encoder;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		bridge_state = dp_mst_get_bridge_atomic_state(
				state->state, &mst->mst_bridge[i]);
		if (IS_ERR(bridge_state))
			goto end;

		if (bridge_state->connector == connector) {
			enc = mst->mst_bridge[i].encoder;
			goto end;
		}
	}

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (mst->mst_bridge[i].fixed_connector)
			continue;

		bridge_state = dp_mst_get_bridge_atomic_state(
				state->state, &mst->mst_bridge[i]);

		if (!bridge_state->connector) {
			bridge_state->connector = connector;
			bridge_state->dp_panel = conn->drv_panel;
			enc = mst->mst_bridge[i].encoder;
			break;
		}

	}

end:
	if (enc)
		DP_MST_DEBUG("mst connector:%d atomic best encoder:%d\n",
			connector->base.id, i);
	else
		DP_MST_DEBUG("mst connector:%d atomic best encoder failed\n",
				connector->base.id);

	return enc;
}

static int dp_mst_connector_atomic_check(struct drm_connector *connector,
		void *display, struct drm_atomic_state *state)
{
	int rc = 0, slots, i;
	bool vcpi_released = false;
	struct drm_connector_state *old_conn_state;
	struct drm_connector_state *new_conn_state;
	struct drm_crtc *old_crtc;
	struct drm_crtc_state *crtc_state;
	struct dp_mst_bridge *bridge;
	struct dp_mst_bridge_state *bridge_state;
	struct drm_bridge *drm_bridge;
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display_mode dp_mode;

	DP_MST_DEBUG_V("enter:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, connector->base.id);

	if (!state)
		return rc;

	new_conn_state = drm_atomic_get_new_connector_state(state, connector);
	if (!new_conn_state)
		return rc;

	old_conn_state = drm_atomic_get_old_connector_state(state, connector);
	if (!old_conn_state)
		goto mode_set;

	old_crtc = old_conn_state->crtc;
	if (!old_crtc)
		goto mode_set;

	crtc_state = drm_atomic_get_new_crtc_state(state, old_crtc);

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		bridge = &mst->mst_bridge[i];
		DP_MST_DEBUG("bridge id:%d, vcpi:%d, pbn:%d, slots:%d\n",
				bridge->id, bridge->vcpi, bridge->pbn,
				bridge->num_slots);
	}

	/*attempt to release vcpi slots on a modeset change for crtc state*/
	if (drm_atomic_crtc_needs_modeset(crtc_state)) {
		if (WARN_ON(!old_conn_state->best_encoder)) {
			rc = -EINVAL;
			goto end;
		}

		drm_bridge = drm_bridge_chain_get_first_bridge(
				old_conn_state->best_encoder);
		if (WARN_ON(!drm_bridge)) {
			rc = -EINVAL;
			goto end;
		}
		bridge = to_dp_mst_bridge(drm_bridge);

		bridge_state = dp_mst_get_bridge_atomic_state(state, bridge);
		if (IS_ERR(bridge_state)) {
			rc = PTR_ERR(bridge_state);
			goto end;
		}

		if (WARN_ON(bridge_state->connector != connector)) {
			rc = -EINVAL;
			goto end;
		}

		slots = bridge_state->num_slots;
		if (slots > 0) {
			rc = mst->mst_fw_cbs->atomic_release_time_slots(state,
					&mst->mst_mgr, c_conn->mst_port);
			if (rc) {
				DP_ERR("failed releasing %d vcpi slots %d\n",
						slots, rc);
				goto end;
			}
			vcpi_released = true;
		}

		bridge_state->num_slots = 0;

		if (!new_conn_state->crtc && mst->state != PM_SUSPEND) {
			bridge_state->connector = NULL;
			bridge_state->dp_panel = NULL;

			DP_MST_DEBUG("clear best encoder: %d\n", bridge->id);
		}
	}

mode_set:
	if (!new_conn_state->crtc)
		goto end;

	crtc_state = drm_atomic_get_new_crtc_state(state, new_conn_state->crtc);

	if (drm_atomic_crtc_needs_modeset(crtc_state) && crtc_state->active) {
		c_conn = to_sde_connector(connector);

		if (WARN_ON(!new_conn_state->best_encoder)) {
			rc = -EINVAL;
			goto end;
		}

		drm_bridge = drm_bridge_chain_get_first_bridge(
				new_conn_state->best_encoder);
		if (WARN_ON(!drm_bridge)) {
			rc = -EINVAL;
			goto end;
		}
		bridge = to_dp_mst_bridge(drm_bridge);

		bridge_state = dp_mst_get_bridge_atomic_state(state, bridge);
		if (IS_ERR(bridge_state)) {
			rc = PTR_ERR(bridge_state);
			goto end;
		}

		if (WARN_ON(bridge_state->connector != connector)) {
			rc = -EINVAL;
			goto end;
		}

		/*
		 * check if vcpi slots are trying to get allocated in same phase
		 * as deallocation. If so, go to end to avoid allocation.
		 */
		if (vcpi_released) {
			DP_WARN("skipping allocation since vcpi was released in the same state\n");
			goto end;
		}

		if (WARN_ON(bridge_state->num_slots)) {
			rc = -EINVAL;
			goto end;
		}

		dp_display->convert_to_dp_mode(dp_display, c_conn->drv_panel,
				&crtc_state->mode, &dp_mode);

		slots = _dp_mst_compute_config(state, mst, connector, &dp_mode);
		if (slots < 0) {
			rc = slots;
			goto end;
		}

		bridge_state->num_slots = slots;
	}

end:
	DP_MST_DEBUG("mst connector:%d atomic check ret %d\n",
			connector->base.id, rc);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, connector->base.id, rc);
	return rc;
}

static int dp_mst_connector_config_hdr(struct drm_connector *connector,
		void *display, struct sde_connector_state *c_state)
{
	int rc;

	DP_MST_DEBUG_V("enter:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, connector->base.id);

	rc = dp_connector_config_hdr(connector, display, c_state);

	DP_MST_DEBUG("mst connector:%d cfg hdr. rc:%d\n",
			connector->base.id, rc);

	DP_MST_DEBUG_V("exit:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, connector->base.id, rc);

	return rc;
}

static void dp_mst_connector_pre_destroy(struct drm_connector *connector,
		void *display)
{
	struct dp_display *dp_display = display;
	struct sde_connector *c_conn = to_sde_connector(connector);
	u32 conn_id = connector->base.id;

	DP_MST_DEBUG_V("enter:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, conn_id);

	kfree(c_conn->cached_edid);
	c_conn->cached_edid = NULL;

	drm_dp_mst_put_port_malloc(c_conn->mst_port);

	dp_display->mst_connector_uninstall(dp_display, connector);
	DP_MST_DEBUG_V("exit:\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, conn_id);
}

static int dp_mst_connector_post_init(struct drm_connector *connector,
		void *display)
{
	struct dp_display *dp_display = display;
	struct sde_connector *sde_conn = to_sde_connector(connector);

	if (!dp_display || !connector)
		return -EINVAL;

	if (dp_display->dsc_cont_pps)
		sde_conn->ops.update_pps = NULL;

	return 0;
}

/* DRM MST callbacks */

static struct drm_connector *
dp_mst_add_connector(struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port, const char *pathprop)
{
	static const struct sde_connector_ops dp_mst_connector_ops = {
		.post_init  = dp_mst_connector_post_init,
		.detect_ctx = dp_mst_connector_detect,
		.get_modes  = dp_mst_connector_get_modes,
		.mode_valid = dp_mst_connector_mode_valid,
		.get_info   = dp_connector_get_info,
		.get_mode_info  = dp_mst_connector_get_mode_info,
		.atomic_best_encoder = dp_mst_atomic_best_encoder,
		.atomic_check = dp_mst_connector_atomic_check,
		.config_hdr = dp_mst_connector_config_hdr,
		.pre_destroy = dp_mst_connector_pre_destroy,
		.update_pps = dp_connector_update_pps,
		.install_properties = dp_connector_install_properties,
	};
	struct dp_mst_private *dp_mst;
	struct drm_device *dev;
	struct dp_display *dp_display;
	struct drm_connector *connector;
	struct sde_connector *c_conn;
	int rc, i;

	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY);

	dp_mst = container_of(mgr, struct dp_mst_private, mst_mgr);

	dp_display = dp_mst->dp_display;
	dev = dp_display->drm_dev;

	/* make sure connector is not accessed before reset */
	drm_modeset_lock_all(dev);

	connector = sde_connector_init(dev,
				dp_mst->mst_bridge[0].encoder,
				NULL,
				dp_display,
				&dp_mst_connector_ops,
				DRM_CONNECTOR_POLL_HPD,
				DRM_MODE_CONNECTOR_DisplayPort, false);

	if (IS_ERR_OR_NULL(connector)) {
		DP_ERR("mst sde_connector_init failed\n");
		drm_modeset_unlock_all(dev);
		return NULL;
	}

	rc = dp_display->mst_connector_install(dp_display, connector);
	if (rc) {
		DP_ERR("mst connector install failed\n");
		sde_connector_destroy(connector);
		drm_modeset_unlock_all(dev);
		return NULL;
	}

	c_conn = to_sde_connector(connector);
	c_conn->mst_port = port;
	drm_dp_mst_get_port_malloc(c_conn->mst_port);

	if (connector->funcs->reset)
		connector->funcs->reset(connector);

	for (i = 1; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		drm_connector_attach_encoder(connector,
				dp_mst->mst_bridge[i].encoder);
	}

	drm_object_attach_property(&connector->base,
			dev->mode_config.path_property, 0);
	drm_object_attach_property(&connector->base,
			dev->mode_config.tile_property, 0);

	/* unlock connector and make it accessible */
	drm_modeset_unlock_all(dev);

	DP_MST_INFO("add mst connector id:%d\n", connector->base.id);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, connector->base.id);

	return connector;
}

static int
dp_mst_fixed_connector_detect(struct drm_connector *connector,
			struct drm_modeset_acquire_ctx *ctx,
			bool force,
			void *display)
{
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	int i;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (mst->mst_bridge[i].fixed_connector != connector)
			continue;

		if (!mst->mst_bridge[i].fixed_port_added)
			break;

		return dp_mst_connector_detect(connector, ctx, force, display);
	}

	return (int)connector_status_disconnected;
}

static struct drm_encoder *
dp_mst_fixed_atomic_best_encoder(struct drm_connector *connector,
			void *display, struct drm_connector_state *state)
{
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	struct sde_connector *conn = to_sde_connector(connector);
	struct drm_encoder *enc = NULL;
	struct dp_mst_bridge_state *bridge_state;
	u32 i;

	if (state->best_encoder)
		return state->best_encoder;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (mst->mst_bridge[i].fixed_connector == connector) {
			bridge_state = dp_mst_get_bridge_atomic_state(
					state->state, &mst->mst_bridge[i]);
			if (IS_ERR(bridge_state))
				goto end;

			bridge_state->connector = connector;
			bridge_state->dp_panel = conn->drv_panel;
			enc = mst->mst_bridge[i].encoder;
			break;
		}
	}

end:
	if (enc)
		DP_MST_DEBUG("mst connector:%d atomic best encoder:%d\n",
			connector->base.id, i);
	else
		DP_MST_DEBUG("mst connector:%d atomic best encoder failed\n",
				connector->base.id);

	return enc;
}

static u32 dp_mst_find_fixed_port_num(struct drm_dp_mst_branch *mstb,
		struct drm_dp_mst_port *target)
{
	struct drm_dp_mst_port *port;
	u32 port_num = 0;

	/*
	 * search through reversed order of adding sequence, so the port number
	 * will be unique once topology is fixed
	 */
	list_for_each_entry_reverse(port, &mstb->ports, next) {
		if (port->mstb)
			port_num += dp_mst_find_fixed_port_num(port->mstb,
						target);
		else if (!port->input) {
			++port_num;
			if (port == target)
				break;
		}
	}

	return port_num;
}

static struct drm_connector *
dp_mst_find_fixed_connector(struct dp_mst_private *dp_mst,
		struct drm_dp_mst_port *port)
{
	struct dp_display *dp_display = dp_mst->dp_display;
	struct drm_connector *connector = NULL;
	struct sde_connector *c_conn;
	u32 port_num;
	int i;

	mutex_lock(&port->mgr->lock);
	port_num = dp_mst_find_fixed_port_num(port->mgr->mst_primary, port);
	mutex_unlock(&port->mgr->lock);

	if (!port_num)
		return NULL;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (dp_mst->mst_bridge[i].fixed_port_num == port_num) {
			connector = dp_mst->mst_bridge[i].fixed_connector;
			c_conn = to_sde_connector(connector);
			c_conn->mst_port = port;
			dp_display->mst_connector_update_link_info(dp_display,
					connector);
			dp_mst->mst_bridge[i].fixed_port_added = true;
			DP_MST_DEBUG("found fixed connector %d\n",
					DRMID(connector));
			break;
		}
	}

	return connector;
}

static int
dp_mst_find_first_available_encoder_idx(struct dp_mst_private *dp_mst)
{
	int enc_idx = MAX_DP_MST_DRM_BRIDGES;
	int i;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (!dp_mst->mst_bridge[i].fixed_connector) {
			enc_idx = i;
			break;
		}
	}

	return enc_idx;
}

static struct drm_connector *
dp_mst_add_fixed_connector(struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port, const char *pathprop)
{
	struct dp_mst_private *dp_mst;
	struct drm_device *dev;
	struct dp_display *dp_display;
	struct drm_connector *connector;
	int i, enc_idx;

	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY);

	dp_mst = container_of(mgr, struct dp_mst_private, mst_mgr);

	dp_display = dp_mst->dp_display;
	dev = dp_display->drm_dev;

	if (port->input || port->mstb)
		enc_idx = MAX_DP_MST_DRM_BRIDGES;
	else {
		/* if port is already reserved, return immediately */
		connector = dp_mst_find_fixed_connector(dp_mst, port);
		if (connector != NULL)
			return connector;

		/* first available bridge index for non-reserved port */
		enc_idx = dp_mst_find_first_available_encoder_idx(dp_mst);
	}

	/* add normal connector */
	connector = dp_mst_add_connector(mgr, port, pathprop);
	if (!connector) {
		DP_MST_DEBUG("failed to add connector\n");
		return NULL;
	}

	drm_modeset_lock_all(dev);

	/* clear encoder list */
	connector->possible_encoders = 0;

	/* re-attach encoders from first available encoders */
	for (i = enc_idx; i < MAX_DP_MST_DRM_BRIDGES; i++)
		drm_connector_attach_encoder(connector,
				dp_mst->mst_bridge[i].encoder);

	drm_modeset_unlock_all(dev);

	DP_MST_DEBUG("add mst connector:%d\n", connector->base.id);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, connector->base.id);

	return connector;
}

static int dp_mst_fixed_connector_set_info_blob(
		struct drm_connector *connector,
		void *info, void *display, struct msm_mode_info *mode_info)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dp_display *dp_display = display;
	struct dp_mst_private *mst = dp_display->dp_mst_prv_info;
	const char *display_type = NULL;
	int i;

	for (i = 0; i < MAX_DP_MST_DRM_BRIDGES; i++) {
		if (mst->mst_bridge[i].base.encoder != c_conn->encoder)
			continue;

		dp_display->mst_get_fixed_topology_display_type(dp_display,
			mst->mst_bridge[i].id, &display_type);
		sde_kms_info_add_keystr(info, "display type", display_type);

		break;
	}

	return 0;
}

static struct drm_connector *
dp_mst_drm_fixed_connector_init(struct dp_display *dp_display,
			struct drm_encoder *encoder)
{
	static const struct sde_connector_ops dp_mst_connector_ops = {
		.set_info_blob = dp_mst_fixed_connector_set_info_blob,
		.post_init  = dp_mst_connector_post_init,
		.detect_ctx = dp_mst_fixed_connector_detect,
		.get_modes  = dp_mst_connector_get_modes,
		.mode_valid = dp_mst_connector_mode_valid,
		.get_info   = dp_connector_get_info,
		.get_mode_info  = dp_mst_connector_get_mode_info,
		.atomic_best_encoder = dp_mst_fixed_atomic_best_encoder,
		.atomic_check = dp_mst_connector_atomic_check,
		.config_hdr = dp_mst_connector_config_hdr,
		.pre_destroy = dp_mst_connector_pre_destroy,
	};
	struct drm_device *dev;
	struct drm_connector *connector;
	int rc;

	DP_MST_DEBUG_V("enter\n");
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY);

	dev = dp_display->drm_dev;

	connector = sde_connector_init(dev,
				encoder,
				NULL,
				dp_display,
				&dp_mst_connector_ops,
				DRM_CONNECTOR_POLL_HPD,
				DRM_MODE_CONNECTOR_DisplayPort, false);

	if (IS_ERR_OR_NULL(connector)) {
		DP_ERR("mst sde_connector_init failed\n");
		return NULL;
	}

	rc = dp_display->mst_connector_install(dp_display, connector);
	if (rc) {
		DP_ERR("mst connector install failed\n");
		sde_connector_destroy(connector);
		return NULL;
	}

	drm_object_attach_property(&connector->base,
			dev->mode_config.path_property, 0);
	drm_object_attach_property(&connector->base,
			dev->mode_config.tile_property, 0);

	DP_MST_DEBUG("add mst fixed connector:%d\n", connector->base.id);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, connector->base.id);

	return connector;
}

static void dp_mst_hpd_event_notify(struct dp_mst_private *mst, bool hpd_status)
{
	struct drm_device *dev = mst->dp_display->drm_dev;
	char event_string[] = "HOTPLUG=1";
	char status[HPD_STRING_SIZE];
	char *envp[3];

	if (hpd_status)
		snprintf(status, HPD_STRING_SIZE, "status=connected");
	else
		snprintf(status, HPD_STRING_SIZE, "status=disconnected");

	envp[0] = event_string;
	envp[1] = status;
	envp[2] = NULL;

	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE, envp);

	DP_MST_INFO("%s finished. hpd_status:%d\n", __func__, hpd_status);
	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, hpd_status);
}

/* DP Driver Callback OPs */

static int dp_mst_display_set_mgr_state(void *dp_display, bool state)
{
	int rc;
	struct dp_display *dp = dp_display;
	struct dp_mst_private *mst = dp->dp_mst_prv_info;

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, state);
	/*
	 * on hpd high, set_mgr_state is called before hotplug event is sent
	 * to usermode and mst_session_state should be updated here.
	 * on hpd_low, set_mgr_state is called after hotplug event is sent and
	 * the session_state was already updated prior to that.
	 */
	if (state)
		mst->mst_session_state = state;

	dp_mst_clear_edid_cache(dp);
	mst->mst_fw_cbs = &drm_dp_mst_fw_helper_ops;

	rc = mst->mst_fw_cbs->topology_mgr_set_mst(&mst->mst_mgr, state);
	if (rc < 0) {
		DP_ERR("failed to set topology mgr state to %d. rc %d\n",
				state, rc);
	}

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_EXIT, rc);

	return rc;
}

static void dp_mst_display_hpd(void *dp_display, bool hpd_status)
{
	struct dp_display *dp = dp_display;
	struct dp_mst_private *mst = dp->dp_mst_prv_info;

	/*
	 * on hpd high, set_mgr_state is called before hotplug event is sent
	 * to usermode and mst_session_state was already updated there.
	 * on hpd_low, hotplug event is sent before set_mgr_state and the
	 * session state should be unset here for the connection status to be
	 * updated accordingly.
	 */
	if (!hpd_status)
		mst->mst_session_state = hpd_status;

	dp_mst_hpd_event_notify(mst, hpd_status);
}

static void dp_mst_display_hpd_irq(void *dp_display)
{
	int rc;
	struct dp_display *dp = dp_display;
	struct dp_mst_private *mst = dp->dp_mst_prv_info;
#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
	u8 ack[8] = {};
#endif
	u8 esi[14];
	unsigned int esi_res = DP_SINK_COUNT_ESI + 1;
	bool handled;

	if (!mst->mst_session_state) {
		DP_ERR("mst_hpd_irq received before mst session start\n");
		return;
	}

	rc = drm_dp_dpcd_read(mst->caps.drm_aux, DP_SINK_COUNT_ESI,
		esi, 14);
	if (rc != 14) {
		DP_ERR("dpcd sink status read failed, rlen=%d\n", rc);
		return;
	}

	DP_MST_DEBUG("mst irq: esi1[0x%x] esi2[0x%x] esi3[%x]\n",
			esi[1], esi[2], esi[3]);

#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
	rc = drm_dp_mst_hpd_irq_handle_event(&mst->mst_mgr, esi, ack, &handled);

	/* ack the request */
	if (handled) {
		rc = drm_dp_dpcd_writeb(mst->caps.drm_aux, esi_res, ack[1]);

		if (ack[1] & DP_UP_REQ_MSG_RDY)
			dp_mst_clear_edid_cache(dp);

		if (rc != 1)
			DP_ERR("dpcd esi_res failed. rc=%d\n", rc);

		drm_dp_mst_hpd_irq_send_new_request(&mst->mst_mgr);
	}
#else
	rc = drm_dp_mst_hpd_irq(&mst->mst_mgr, esi, &handled);

	/* ack the request */
	if (handled) {
		rc = drm_dp_dpcd_write(mst->caps.drm_aux, esi_res, &esi[1], 3);

		if (esi[1] & DP_UP_REQ_MSG_RDY)
			dp_mst_clear_edid_cache(dp);

		if (rc != 3)
			DP_ERR("dpcd esi_res failed. rlen=%d\n", rc);
	}
#endif

	DP_MST_DEBUG("mst display hpd_irq handled:%d rc:%d\n", handled, rc);
}

static void dp_mst_set_state(void *dp_display, enum dp_drv_state mst_state)
{
	struct dp_display *dp = dp_display;
	struct dp_mst_private *mst = dp->dp_mst_prv_info;

	if (!mst) {
		DP_DEBUG("mst not initialized\n");
		return;
	}

	SDE_EVT32_EXTERNAL(SDE_EVTLOG_FUNC_ENTRY, mst_state);
	mst->state = mst_state;
	DP_MST_INFO("mst power state:%d\n", mst_state);
}

static void dp_mst_display_set_mst_mode_params(void *dp_display, struct dp_display_mode *mode)
{
	// update pbn values that will later be used for rg calculation
	dp_mst_calc_pbn_mode(mode);
}

/* DP MST APIs */

static const struct dp_mst_drm_cbs dp_mst_display_cbs = {
	.hpd = dp_mst_display_hpd,
	.hpd_irq = dp_mst_display_hpd_irq,
	.set_drv_state = dp_mst_set_state,
	.set_mgr_state = dp_mst_display_set_mgr_state,
	.set_mst_mode_params = dp_mst_display_set_mst_mode_params,
};

static const struct drm_dp_mst_topology_cbs dp_mst_drm_cbs = {
	.add_connector = dp_mst_add_connector,
};

static const struct drm_dp_mst_topology_cbs dp_mst_fixed_drm_cbs = {
	.add_connector = dp_mst_add_fixed_connector,
};

int dp_mst_init(struct dp_display *dp_display)
{
	struct drm_device *dev;
	int conn_base_id = 0;
	int ret;
	struct dp_mst_drm_install_info install_info;
	struct dp_mst_private *dp_mst;

	if (!dp_display) {
		DP_ERR("invalid params\n");
		return 0;
	}

	dev = dp_display->drm_dev;

	dp_mst = devm_kzalloc(dev->dev, sizeof(*dp_mst), GFP_KERNEL);
	if (!dp_mst)
		return -ENOMEM;

	/* register with DP driver */
	install_info.dp_mst_prv_info = dp_mst;
	install_info.cbs = &dp_mst_display_cbs;
	dp_display->mst_install(dp_display, &install_info);

	dp_display->get_mst_caps(dp_display, &dp_mst->caps);

	if (!dp_mst->caps.has_mst) {
		DP_MST_DEBUG("mst not supported\n");
		return 0;
	}

	dp_mst->mst_fw_cbs = &drm_dp_mst_fw_helper_ops;

	memset(&dp_mst->mst_mgr, 0, sizeof(dp_mst->mst_mgr));
	dp_mst->mst_mgr.cbs = &dp_mst_drm_cbs;
	conn_base_id = dp_display->base_connector->base.id;
	dp_mst->dp_display = dp_display;

	mutex_init(&dp_mst->mst_lock);
	mutex_init(&dp_mst->edid_lock);

/*
 * Upstream driver modified drm_dp_mst_topology_mgr_init signature
 * in 5.15 kernel and reverted it back in 6.1
 */
#if ((KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE) && \
		(KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE))
	ret = drm_dp_mst_topology_mgr_init(&dp_mst->mst_mgr, dev,
					dp_mst->caps.drm_aux,
					dp_mst->caps.max_dpcd_transaction_bytes,
					dp_mst->caps.max_streams_supported,
					4, DP_MAX_LINK_CLK_KHZ, conn_base_id);
#else
	ret = drm_dp_mst_topology_mgr_init(&dp_mst->mst_mgr, dev,
					dp_mst->caps.drm_aux,
					dp_mst->caps.max_dpcd_transaction_bytes,
					dp_mst->caps.max_streams_supported,
					conn_base_id);
#endif
	if (ret) {
		DP_ERR("dp drm mst topology manager init failed\n");
		goto error;
	}

	dp_mst->mst_initialized = true;

	/* choose fixed callback function if fixed topology is found */
	if (!dp_display->mst_get_fixed_topology_port(dp_display, 0, NULL))
		dp_mst->mst_mgr.cbs = &dp_mst_fixed_drm_cbs;

	DP_MST_INFO("dp drm mst topology manager init completed\n");

	return ret;

error:
	mutex_destroy(&dp_mst->mst_lock);
	mutex_destroy(&dp_mst->edid_lock);
	return ret;
}

void dp_mst_deinit(struct dp_display *dp_display)
{
	struct dp_mst_private *mst;

	if (!dp_display) {
		DP_ERR("invalid params\n");
		return;
	}

	mst = dp_display->dp_mst_prv_info;

	if (!mst->mst_initialized)
		return;

	dp_display->mst_uninstall(dp_display);

	drm_dp_mst_topology_mgr_destroy(&mst->mst_mgr);

	mst->mst_initialized = false;

	mutex_destroy(&mst->mst_lock);
	mutex_destroy(&mst->edid_lock);

	DP_MST_INFO("dp drm mst topology manager deinit completed\n");
}

