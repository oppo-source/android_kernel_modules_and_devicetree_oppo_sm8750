// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "cam_node.h"
#include "cam_hw_mgr_intf.h"
#include "cam_jpeg_hw_mgr_intf.h"
#include "cam_jpeg_dev.h"
#include "cam_debug_util.h"
#include "cam_smmu_api.h"
#include "camera_main.h"
#include "cam_common_util.h"
#include "cam_context_utils.h"
#include "cam_req_mgr_dev.h"

#define CAM_JPEG_DEV_NAME "cam-jpeg"

static struct cam_jpeg_dev g_jpeg_dev;

static int cam_jpeg_dev_evt_inject_cb(void *inject_args)
{
	struct cam_common_inject_evt_param *inject_params = inject_args;
	int i;

	for (i = 0; i < CAM_JPEG_CTX_MAX; i++) {
		if (g_jpeg_dev.ctx[i].dev_hdl == inject_params->dev_hdl) {
			cam_context_add_evt_inject(&g_jpeg_dev.ctx[i],
				&inject_params->evt_params);
			return 0;
		}
	}

	CAM_ERR(CAM_JPEG, "No dev hdl found %d", inject_params->dev_hdl);
	return -EINVAL;
}

static void cam_jpeg_dev_iommu_fault_handler(
	struct cam_smmu_pf_info *pf_smmu_info)
{
	int i, rc;
	struct cam_node *node = NULL;
	struct cam_hw_dump_pf_args pf_args = {0};

	if (!pf_smmu_info || !pf_smmu_info->token) {
		CAM_ERR(CAM_JPEG, "invalid token in page handler cb");
		return;
	}

	node = (struct cam_node *)pf_smmu_info->token;

	pf_args.pf_smmu_info = pf_smmu_info;

	for (i = 0; i < node->ctx_size; i++) {
		cam_context_dump_pf_info(&(node->ctx_list[i]), &pf_args);
		if (pf_args.pf_context_info.ctx_found)
			/* found ctx and packet of the faulted address */
			break;
	}

	if (i == node->ctx_size) {
		/* Faulted ctx not found. But report PF to UMD anyway*/
		rc = cam_context_send_pf_evt(NULL, &pf_args);
		if (rc)
			CAM_ERR(CAM_JPEG,
				"Failed to notify PF event to userspace rc: %d", rc);
	}
}

static void cam_jpeg_dev_mini_dump_cb(void *priv, void *args)
{
	struct cam_context *ctx = NULL;

	if (!priv || !args) {
		CAM_ERR(CAM_JPEG, "Invalid param priv %pK %pK args", priv, args);
		return;
	}

	ctx = (struct cam_context *)priv;
	cam_context_mini_dump_from_hw(ctx, args);
}

static const struct of_device_id cam_jpeg_dt_match[] = {
	{
		.compatible = "qcom,cam-jpeg"
	},
	{ }
};

static int cam_jpeg_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	cam_req_mgr_rwsem_read_op(CAM_SUBDEV_LOCK);

	mutex_lock(&g_jpeg_dev.jpeg_mutex);
	g_jpeg_dev.open_cnt++;
	mutex_unlock(&g_jpeg_dev.jpeg_mutex);
	cam_req_mgr_rwsem_read_op(CAM_SUBDEV_UNLOCK);

	return 0;
}

static int cam_jpeg_subdev_close_internal(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct cam_node *node = v4l2_get_subdevdata(sd);

	mutex_lock(&g_jpeg_dev.jpeg_mutex);
	if (g_jpeg_dev.open_cnt <= 0) {
		CAM_DBG(CAM_JPEG, "JPEG subdev is already closed");
		rc = -EINVAL;
		goto end;
	}

	g_jpeg_dev.open_cnt--;

	if (!node) {
		CAM_ERR(CAM_JPEG, "Node ptr is NULL");
		rc = -EINVAL;
		goto end;
	}

	if (g_jpeg_dev.open_cnt == 0)
		cam_node_shutdown(node);

end:
	mutex_unlock(&g_jpeg_dev.jpeg_mutex);
	return rc;
}

static int cam_jpeg_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	bool crm_active = cam_req_mgr_is_open();

	if (crm_active) {
		CAM_DBG(CAM_JPEG, "CRM is ACTIVE, close should be from CRM");
		return 0;
	}
	return cam_jpeg_subdev_close_internal(sd, fh);
}

static const struct v4l2_subdev_internal_ops cam_jpeg_subdev_internal_ops = {
	.close = cam_jpeg_subdev_close,
	.open = cam_jpeg_subdev_open,
};

static int cam_jpeg_dev_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc;
	int i;
	struct cam_hw_mgr_intf hw_mgr_intf;
	struct cam_node *node;
	int iommu_hdl = -1;
	struct platform_device *pdev = to_platform_device(dev);
	struct timespec64 ts_start, ts_end;
	long microsec = 0;

	CAM_GET_TIMESTAMP(ts_start);
	g_jpeg_dev.sd.internal_ops = &cam_jpeg_subdev_internal_ops;
	g_jpeg_dev.sd.close_seq_prior = CAM_SD_CLOSE_MEDIUM_PRIORITY;
	rc = cam_subdev_probe(&g_jpeg_dev.sd, pdev, CAM_JPEG_DEV_NAME,
		CAM_JPEG_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_JPEG, "JPEG cam_subdev_probe failed %d", rc);
		goto err;
	}
	node = (struct cam_node *)g_jpeg_dev.sd.token;

	rc = cam_jpeg_hw_mgr_init(pdev->dev.of_node,
		(uint64_t *)&hw_mgr_intf, &iommu_hdl,
		cam_jpeg_dev_mini_dump_cb);
	if (rc) {
		CAM_ERR(CAM_JPEG, "Can not initialize JPEG HWmanager %d", rc);
		goto unregister;
	}

	for (i = 0; i < CAM_JPEG_CTX_MAX; i++) {
		rc = cam_jpeg_context_init(&g_jpeg_dev.ctx_jpeg[i],
			&g_jpeg_dev.ctx[i],
			&node->hw_mgr_intf,
			i, iommu_hdl);
		if (rc) {
			CAM_ERR(CAM_JPEG, "JPEG context init failed %d %d",
				i, rc);
			goto ctx_init_fail;
		}
	}

	cam_common_register_evt_inject_cb(cam_jpeg_dev_evt_inject_cb,
		CAM_COMMON_EVT_INJECT_HW_JPEG);

	rc = cam_node_init(node, &hw_mgr_intf, g_jpeg_dev.ctx, CAM_JPEG_CTX_MAX,
		CAM_JPEG_DEV_NAME);
	if (rc) {
		CAM_ERR(CAM_JPEG, "JPEG node init failed %d", rc);
		goto ctx_init_fail;
	}

	node->sd_handler = cam_jpeg_subdev_close_internal;
	cam_smmu_set_client_page_fault_handler(iommu_hdl,
		cam_jpeg_dev_iommu_fault_handler, node);

	mutex_init(&g_jpeg_dev.jpeg_mutex);

	CAM_DBG(CAM_JPEG, "Component bound successfully");
	CAM_GET_TIMESTAMP(ts_end);
	CAM_GET_TIMESTAMP_DIFF_IN_MICRO(ts_start, ts_end, microsec);
	cam_record_bind_latency(pdev->name, microsec);

	return rc;

ctx_init_fail:
	for (--i; i >= 0; i--)
		if (cam_jpeg_context_deinit(&g_jpeg_dev.ctx_jpeg[i]))
			CAM_ERR(CAM_JPEG, "deinit fail %d %d", i, rc);
unregister:
	if (cam_subdev_remove(&g_jpeg_dev.sd))
		CAM_ERR(CAM_JPEG, "remove fail %d", rc);
err:
	return rc;
}

static void cam_jpeg_dev_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc;
	int i;

	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_jpeg_context_deinit(&g_jpeg_dev.ctx_jpeg[i]);
		if (rc)
			CAM_ERR(CAM_JPEG, "JPEG context %d deinit failed %d",
				i, rc);
	}

	rc = cam_subdev_remove(&g_jpeg_dev.sd);
	if (rc)
		CAM_ERR(CAM_JPEG, "Unregister failed %d", rc);
}

const static struct component_ops cam_jpeg_dev_component_ops = {
	.bind = cam_jpeg_dev_component_bind,
	.unbind = cam_jpeg_dev_component_unbind,
};

static int cam_jpeg_dev_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_jpeg_dev_component_ops);
	return 0;
}

static int cam_jpeg_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_JPEG, "Adding JPEG component");
	rc = component_add(&pdev->dev, &cam_jpeg_dev_component_ops);
	if (rc)
		CAM_ERR(CAM_JPEG, "failed to add component rc: %d", rc);

	return rc;

}

struct platform_driver jpeg_driver = {
	.probe = cam_jpeg_dev_probe,
	.remove = cam_jpeg_dev_remove,
	.driver = {
		.name = "cam_jpeg",
		.owner = THIS_MODULE,
		.of_match_table = cam_jpeg_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_jpeg_dev_init_module(void)
{
	return platform_driver_register(&jpeg_driver);
}

void cam_jpeg_dev_exit_module(void)
{
	platform_driver_unregister(&jpeg_driver);
}

MODULE_DESCRIPTION("MSM JPEG driver");
MODULE_LICENSE("GPL v2");
