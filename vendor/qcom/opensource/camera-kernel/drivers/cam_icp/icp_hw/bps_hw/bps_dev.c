// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include <linux/timer.h>
#include "bps_core.h"
#include "bps_soc.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_io_util.h"
#include "cam_icp_hw_intf.h"
#include "cam_icp_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "camera_main.h"
#include "cam_mem_mgr_api.h"
#include "cam_req_mgr_dev.h"

static struct cam_icp_hw_intf_data cam_bps_hw_list[CAM_BPS_HW_NUM_MAX];

static struct cam_bps_device_hw_info cam_bps_hw_info = {
	.hw_idx = 0,
	.pwr_ctrl = 0x48,
	.pwr_status = 0x44,
};

static struct cam_bps_device_hw_info cam_bps680_hw_info = {
	.hw_idx = 0,
	.pwr_ctrl = 0x48,
	.pwr_status = 0x44,
};

static struct cam_bps_device_hw_info cam_bps880_hw_info = {
	.hw_idx = 0,
	.pwr_ctrl = 0x48,
	.pwr_status = 0x44,
};

static bool cam_bps_cpas_cb(uint32_t client_handle, void *userdata,
	struct cam_cpas_irq_data *irq_data)
{
	bool error_handled = false;

	if (!irq_data)
		return error_handled;

	switch (irq_data->irq_type) {
	case CAM_CAMNOC_IRQ_IPE_BPS_UBWC_DECODE_ERROR:
		CAM_ERR_RATE_LIMIT(CAM_ICP,
			"IPE/BPS UBWC Decode error type=%d status=%x thr_err=%d, fcl_err=%d, len_md_err=%d, format_err=%d",
			irq_data->irq_type,
			irq_data->u.dec_err.decerr_status.value,
			irq_data->u.dec_err.decerr_status.thr_err,
			irq_data->u.dec_err.decerr_status.fcl_err,
			irq_data->u.dec_err.decerr_status.len_md_err,
			irq_data->u.dec_err.decerr_status.format_err);
		error_handled = true;
		break;
	case CAM_CAMNOC_IRQ_IPE_BPS_UBWC_ENCODE_ERROR:
		CAM_ERR_RATE_LIMIT(CAM_ICP,
			"IPE/BPS UBWC Encode error type=%d status=%x",
			irq_data->irq_type,
			irq_data->u.enc_err.encerr_status.value);
		error_handled = true;
		break;
	default:
		break;
	}

	return error_handled;
}

int cam_bps_register_cpas(struct cam_hw_soc_info *soc_info,
			struct cam_bps_device_core_info *core_info,
			uint32_t hw_idx)
{
	struct cam_cpas_register_params cpas_register_params;
	int rc;

	cpas_register_params.dev = &soc_info->pdev->dev;
	memcpy(cpas_register_params.identifier, "bps", sizeof("bps"));
	cpas_register_params.cam_cpas_client_cb = cam_bps_cpas_cb;
	cpas_register_params.cell_index = hw_idx;
	cpas_register_params.userdata = NULL;

	rc = cam_cpas_register_client(&cpas_register_params);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "failed: %d", rc);
		return rc;
	}
	core_info->cpas_handle = cpas_register_params.client_handle;

	return rc;
}

static int cam_bps_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_info                *bps_dev = NULL;
	struct cam_hw_intf                *bps_dev_intf = NULL;
	const struct of_device_id         *match_dev = NULL;
	struct cam_bps_device_core_info   *core_info = NULL;
	struct cam_bps_device_hw_info     *hw_info = NULL;
	int                                rc = 0, i;
	uint32_t                           hw_idx;
	struct platform_device            *pdev = to_platform_device(dev);
	struct timespec64                  ts_start, ts_end;
	long                               microsec = 0;
	struct cam_bps_soc_private        *bps_soc_priv;

	CAM_GET_TIMESTAMP(ts_start);
	bps_dev_intf = CAM_MEM_ZALLOC(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!bps_dev_intf)
		return -ENOMEM;

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &hw_idx);
	bps_dev_intf->hw_idx = hw_idx;

	bps_dev = CAM_MEM_ZALLOC(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!bps_dev) {
		rc = -ENOMEM;
		goto free_dev_intf;
	}

	bps_dev->soc_info.pdev = pdev;
	bps_dev->soc_info.dev = &pdev->dev;
	bps_dev->soc_info.dev_name = pdev->name;
	bps_dev_intf->hw_priv = bps_dev;
	bps_dev_intf->hw_ops.init = cam_bps_init_hw;
	bps_dev_intf->hw_ops.deinit = cam_bps_deinit_hw;
	bps_dev_intf->hw_ops.process_cmd = cam_bps_process_cmd;
	bps_dev_intf->hw_type = CAM_ICP_DEV_BPS;
	platform_set_drvdata(pdev, &cam_bps_hw_list[hw_idx]);
	bps_dev->core_info = CAM_MEM_ZALLOC(sizeof(struct cam_bps_device_core_info),
					GFP_KERNEL);
	if (!bps_dev->core_info) {
		rc = -ENOMEM;
		goto free_dev;
	}

	core_info = (struct cam_bps_device_core_info *)bps_dev->core_info;
	match_dev = of_match_device(pdev->dev.driver->of_match_table, &pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ICP, "No bps hardware info");
		rc = -EINVAL;
		goto free_core_info;
	}

	hw_info = (struct cam_bps_device_hw_info *)match_dev->data;
	core_info->bps_hw_info = hw_info;

	rc = cam_bps_init_soc_resources(&bps_dev->soc_info, cam_bps_irq,
		bps_dev);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "failed to init_soc");
		goto free_core_info;
	}

	bps_dev->soc_info.hw_id = CAM_HW_ID_BPS + bps_dev->soc_info.index;
	CAM_DBG(CAM_ICP, "soc info : %pK",
		(void *)&bps_dev->soc_info);

	rc = cam_vmvm_populate_hw_instance_info(&bps_dev->soc_info, NULL, NULL);
	if (rc) {
		CAM_ERR(CAM_ICP, " hw instance populate failed: %d", rc);
		goto free_soc_resources;
	}

	rc = cam_bps_register_cpas(&bps_dev->soc_info,
			core_info, bps_dev_intf->hw_idx);
	if (rc < 0)
		goto free_soc_resources;

	bps_dev->hw_state = CAM_HW_STATE_POWER_DOWN;
	bps_soc_priv = bps_dev->soc_info.soc_private;
	cam_bps_hw_list[hw_idx].pid =
		CAM_MEM_ZALLOC_ARRAY(bps_soc_priv->num_pid, sizeof(uint32_t), GFP_KERNEL);
	if (!cam_bps_hw_list[hw_idx].pid) {
		CAM_ERR(CAM_ICP, "Failed at allocating memory for bps hw list");
		rc = -ENOMEM;
		goto free_soc_resources;
	}

	cam_bps_hw_list[hw_idx].hw_intf = bps_dev_intf;
	cam_bps_hw_list[hw_idx].num_pid = bps_soc_priv->num_pid;
	for (i = 0; i < bps_soc_priv->num_pid; i++)
		cam_bps_hw_list[hw_idx].pid[i] = bps_soc_priv->pid[i];

	mutex_init(&bps_dev->hw_mutex);
	spin_lock_init(&bps_dev->hw_lock);
	init_completion(&bps_dev->hw_complete);
	CAM_DBG(CAM_ICP, "BPS:%d component bound successfully",
		bps_dev_intf->hw_idx);
	CAM_GET_TIMESTAMP(ts_end);
	CAM_GET_TIMESTAMP_DIFF_IN_MICRO(ts_start, ts_end, microsec);
	cam_record_bind_latency(pdev->name, microsec);

	return rc;

free_soc_resources:
	cam_bps_deinit_soc_resources(&bps_dev->soc_info);
free_core_info:
	CAM_MEM_FREE(bps_dev->core_info);
free_dev:
	CAM_MEM_FREE(bps_dev);
free_dev_intf:
	CAM_MEM_FREE(bps_dev_intf);
	return rc;
}

static void cam_bps_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_info              *bps_dev = NULL;
	struct cam_hw_intf              *bps_dev_intf = NULL;
	struct cam_bps_device_core_info *core_info = NULL;
	struct platform_device          *pdev = to_platform_device(dev);
	struct cam_icp_hw_intf_data     *bps_intf_data;

	CAM_DBG(CAM_ICP, "Unbinding component: %s", pdev->name);
	bps_intf_data = platform_get_drvdata(pdev);
	if (!bps_intf_data) {
		CAM_ERR(CAM_ICP, "Error No data in pdev");
		return;
	}

	bps_dev_intf = bps_intf_data->hw_intf;
	if (!bps_dev_intf) {
		CAM_ERR(CAM_ICP, "Error No BPS dev interface");
		return;
	}

	bps_dev = bps_dev_intf->hw_priv;
	core_info = (struct cam_bps_device_core_info *)bps_dev->core_info;
	cam_cpas_unregister_client(core_info->cpas_handle);
	cam_bps_deinit_soc_resources(&bps_dev->soc_info);

	CAM_MEM_FREE(bps_dev->core_info);
	CAM_MEM_FREE(bps_dev);
	CAM_MEM_FREE(bps_dev_intf);
}

const static struct component_ops cam_bps_component_ops = {
	.bind = cam_bps_component_bind,
	.unbind = cam_bps_component_unbind,
};

int cam_bps_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_ICP, "Adding BPS component");
	rc = component_add(&pdev->dev, &cam_bps_component_ops);
	if (rc)
		CAM_ERR(CAM_ICP, "failed to add component rc: %d", rc);

	return rc;
}

static int cam_bps_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_bps_component_ops);
	return 0;
}

static const struct of_device_id cam_bps_dt_match[] = {
	{
		.compatible = "qcom,cam-bps",
		.data = &cam_bps_hw_info,
	},
	{
		.compatible = "qcom,cam-bps680",
		.data = &cam_bps680_hw_info,
	},
	{
		.compatible = "qcom,cam-bps880",
		.data = &cam_bps880_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_bps_dt_match);

struct platform_driver cam_bps_driver = {
	.probe = cam_bps_probe,
	.remove = cam_bps_remove,
	.driver = {
		.name = "cam-bps",
		.owner = THIS_MODULE,
		.of_match_table = cam_bps_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_bps_init_module(void)
{
	return platform_driver_register(&cam_bps_driver);
}

void cam_bps_exit_module(void)
{
	platform_driver_unregister(&cam_bps_driver);
}

MODULE_DESCRIPTION("CAM BPS driver");
MODULE_LICENSE("GPL v2");
