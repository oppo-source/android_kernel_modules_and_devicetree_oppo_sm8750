/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_AUX_H_
#define _DP_AUX_H_

#include "dp_catalog.h"
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0))
#include <drm/display/drm_dp_helper.h>
#else
#include <drm/drm_dp_helper.h>
#endif
#include "dp_aux_bridge.h"

#define DP_STATE_NOTIFICATION_SENT          BIT(0)
#define DP_STATE_TRAIN_1_STARTED            BIT(1)
#define DP_STATE_TRAIN_1_SUCCEEDED          BIT(2)
#define DP_STATE_TRAIN_1_FAILED             BIT(3)
#define DP_STATE_TRAIN_2_STARTED            BIT(4)
#define DP_STATE_TRAIN_2_SUCCEEDED          BIT(5)
#define DP_STATE_TRAIN_2_FAILED             BIT(6)
#define DP_STATE_CTRL_POWERED_ON            BIT(7)
#define DP_STATE_CTRL_POWERED_OFF           BIT(8)
#define DP_STATE_LINK_MAINTENANCE_STARTED   BIT(9)
#define DP_STATE_LINK_MAINTENANCE_COMPLETED BIT(10)
#define DP_STATE_LINK_MAINTENANCE_FAILED    BIT(11)
#define DP_STATE_AUX_TIMEOUT                BIT(12)
#define DP_STATE_PLL_LOCKED                 BIT(13)

enum dp_aux_switch_type {
	DP_AUX_SWITCH_BYPASS,
	DP_AUX_SWITCH_FSA4480,
	DP_AUX_SWITCH_WCD939x,
};

enum dp_aux_error {
	DP_AUX_ERR_NONE	= 0,
	DP_AUX_ERR_ADDR	= -1,
	DP_AUX_ERR_TOUT	= -2,
	DP_AUX_ERR_NACK	= -3,
	DP_AUX_ERR_DEFER	= -4,
	DP_AUX_ERR_NACK_DEFER	= -5,
	DP_AUX_ERR_PHY	= -6,
};

struct dp_aux {
	u32 state;

	bool read;

	struct mutex *access_lock;
	void *ipc_log_context;

	struct drm_dp_aux *drm_aux;
	int (*drm_aux_register)(struct dp_aux *aux, struct drm_device *drm_dev);
	void (*drm_aux_deregister)(struct dp_aux *aux);
	void (*isr)(struct dp_aux *aux);
	void (*init)(struct dp_aux *aux, struct dp_aux_cfg *aux_cfg);
	void (*deinit)(struct dp_aux *aux);
	void (*reconfig)(struct dp_aux *aux);
	void (*abort)(struct dp_aux *aux, bool abort);
	void (*set_sim_mode)(struct dp_aux *aux, struct dp_aux_bridge *sim_bridge);
	int (*switch_configure)(struct dp_aux *aux, bool enable, int orientation);
	int (*switch_register_notifier)(struct notifier_block *nb, struct device_node *node);
	int (*switch_unregister_notifier)(struct notifier_block *nb, struct device_node *node);
};

struct dp_aux *dp_aux_get(struct device *dev, struct dp_catalog_aux *catalog,
		struct dp_parser *parser, struct device_node *aux_switch,
		struct dp_aux_bridge *aux_bridge, void *ipc_log_context,
		enum dp_aux_switch_type switch_type);
void dp_aux_put(struct dp_aux *aux);

#endif /*__DP_AUX_H_*/
