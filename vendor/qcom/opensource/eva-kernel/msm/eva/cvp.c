// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/io.h>
#include "msm_cvp_core.h"
#include "msm_cvp_common.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_internal.h"
#include "msm_cvp_res_parse.h"
#include "msm_cvp_resources.h"
#include "msm_cvp_buf.h"
#include "cvp_hfi_api.h"
#include "cvp_private.h"
#include "msm_cvp_clocks.h"
#include "msm_cvp_dsp.h"
#include "msm_cvp.h"
#include "vm/cvp_vm.h"

#define CLASS_NAME              "cvp"
#define DRIVER_NAME             "cvp"

struct msm_cvp_drv *cvp_driver;

static int cvp_open(struct inode *inode, struct file *filp)
{
	struct msm_cvp_inst *inst;

	dprintk(CVP_SESS, "%s\n", __func__);

	inst = msm_cvp_open(MSM_CVP_USER, current);
	if (!inst) {
		dprintk(CVP_ERR, "Failed to create cvp instance\n");
		return -ENOMEM;
	}
	filp->private_data = inst;
	return 0;
}

static int cvp_close(struct inode *inode, struct file *filp)
{
	int rc = 0;
	struct msm_cvp_inst *inst = filp->private_data;

	rc = msm_cvp_close(inst);
	filp->private_data = NULL;
	return rc;
}

static unsigned int cvp_poll(struct file *filp, struct poll_table_struct *p)
{
	int rc = 0;
	struct msm_cvp_inst *inst = filp->private_data;
	unsigned long flags = 0;

	poll_wait(filp, &inst->event_handler.wq, p);

	spin_lock_irqsave(&inst->event_handler.lock, flags);
	if (inst->event_handler.event == EVA_EVENT)
		rc |= POLLPRI;
	if (inst->event_handler.event == CVP_DUMP_EVENT)
		rc |= POLLIN;
	inst->event_handler.event = CVP_NO_EVENT;
	spin_unlock_irqrestore(&inst->event_handler.lock, flags);

	return rc;
}

static const struct file_operations cvp_fops = {
	.owner = THIS_MODULE,
	.open = cvp_open,
	.release = cvp_close,
	.unlocked_ioctl = cvp_unblocked_ioctl,
	.compat_ioctl = cvp_compat_ioctl,
	.poll = cvp_poll,
};

static int read_platform_resources(struct msm_cvp_core *core,
		struct platform_device *pdev)
{
	int rc = 0;

	if (!core || !pdev) {
		dprintk(CVP_ERR, "%s: Invalid params %pK %pK\n",
			__func__, core, pdev);
		return -EINVAL;
	}

	core->hfi_type = CVP_HFI_IRIS;
	core->resources.pdev = pdev;
	if (pdev->dev.of_node) {
		/* Target supports DT, parse from it */
		rc = cvp_read_platform_resources_from_drv_data(core);
		rc = cvp_read_platform_resources_from_dt(core);
	} else {
		dprintk(CVP_ERR, "pdev node is NULL\n");
		rc = -EINVAL;
	}
	return rc;
}

static int msm_cvp_initialize_core(struct platform_device *pdev,
				struct msm_cvp_core *core)
{
	int i = 0;
	int rc = 0;

	if (!core)
		return -EINVAL;
	rc = read_platform_resources(core, pdev);
	if (rc) {
		dprintk(CVP_ERR, "Failed to get platform resources\n");
		return rc;
	}

	INIT_LIST_HEAD(&core->instances);
	mutex_init(&core->lock);
	mutex_init(&core->clk_lock);

	core->state = CVP_CORE_UNINIT;
	for (i = SYS_MSG_INDEX(SYS_MSG_START);
		i <= SYS_MSG_INDEX(SYS_MSG_END); i++) {
		init_completion(&core->completions[i]);
	}

	INIT_WORK(&core->ssr_work, msm_cvp_ssr_handler);
	core->ssr_count = 0;

	return rc;
}

static ssize_t link_name_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct msm_cvp_core *core = dev_get_drvdata(dev);

	if (core)
		if (dev == core->dev)
			return snprintf(buf, PAGE_SIZE, "msm_cvp\n");
		else
			return 0;
	else
		return 0;
}

static DEVICE_ATTR_RO(link_name);

static ssize_t pwr_collapse_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long val = 0;
	int rc = 0;
	struct msm_cvp_core *core = NULL;

	rc = kstrtoul(buf, 0, &val);
	if (rc)
		return rc;
	else if (!val)
		return -EINVAL;

	core = cvp_driver->cvp_core;
	if (!core)
		return -EINVAL;
	core->resources.msm_cvp_pwr_collapse_delay = val;
	return count;
}

static ssize_t pwr_collapse_delay_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct msm_cvp_core *core = NULL;

	core = cvp_driver->cvp_core;
	if (!core)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%u\n",
		core->resources.msm_cvp_pwr_collapse_delay);
}

static DEVICE_ATTR_RW(pwr_collapse_delay);

static ssize_t sku_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d",
			cvp_driver->sku_version);
}

static DEVICE_ATTR_RO(sku_version);

static ssize_t boot_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int rc = 0, val = 0;
	static int booted;

	rc = kstrtoint(buf, 0, &val);
	if (rc || val < 0) {
		dprintk(CVP_WARN,
			"Invalid boot value: %s\n", buf);
		return -EINVAL;
	}

	if (val == 1 && booted == 0) {
		struct msm_cvp_inst *inst;

		inst = msm_cvp_open(MSM_CVP_BOOT, current);
		if (!inst) {
			dprintk(CVP_ERR,
			"Failed to create cvp instance\n");
			return -ENOMEM;
		}
		rc = msm_cvp_close(inst);
		if (rc) {
			dprintk(CVP_ERR,
			"Failed to close cvp instance\n");
			return rc;
		}
	} else if (val == 2) {
		struct msm_cvp_inst *inst;

		inst = msm_cvp_open(MSM_CVP_USER, current);
		if (!inst) {
			dprintk(CVP_ERR,
			"Failed to create eva instance\n");
			return -ENOMEM;
		}
		rc = msm_cvp_session_create(inst);
		if (rc)
			dprintk(CVP_ERR, "Failed to create eva session\n");

		rc = msm_cvp_close(inst);
		if (rc) {
			dprintk(CVP_ERR,
			"Failed to close eva instance\n");
			return rc;
		}
	}
	booted = 1;
	return count;
}

static DEVICE_ATTR_WO(boot);

static struct attribute *msm_cvp_core_attrs[] = {
		&dev_attr_pwr_collapse_delay.attr,
		&dev_attr_sku_version.attr,
		&dev_attr_link_name.attr,
		&dev_attr_boot.attr,
		NULL
};

static struct attribute_group msm_cvp_core_attr_group = {
		.attrs = msm_cvp_core_attrs,
};

static const struct of_device_id msm_cvp_plat_match[] = {
	{.compatible = "qcom,msm-cvp"},
	{.compatible = "qcom,msm-cvp,context-bank"},
	{.compatible = "qcom,msm-cvp,bus"},
	{.compatible = "qcom,msm-cvp,mem-cdsp"},
	{}
};

static int msm_probe_cvp_device(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_cvp_core *core;

	if (!cvp_driver) {
		dprintk(CVP_ERR, "Invalid cvp driver\n");
		return -EINVAL;
	}

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	core->platform_data = cvp_get_drv_data(&pdev->dev);
	dev_set_drvdata(&pdev->dev, core);
	rc = msm_cvp_initialize_core(pdev, core);
	if (rc) {
		dprintk(CVP_ERR, "Failed to init core\n");
		goto err_core_init;
	}

	rc = alloc_chrdev_region(&core->dev_num, 0, 1, DRIVER_NAME);
	if (rc < 0) {
		dprintk(CVP_ERR, "alloc_chrdev_region failed: %d\n",
				rc);
		goto err_alloc_chrdev;
	}

	core->class = class_create(CLASS_NAME);
	if (IS_ERR(core->class)) {
		rc = PTR_ERR(core->class);
		dprintk(CVP_ERR, "class_create failed: %d\n",
				rc);
		goto err_class_create;
	}

	core->dev = device_create(core->class, NULL,
		core->dev_num, NULL, DRIVER_NAME);
	if (IS_ERR(core->dev)) {
		rc = PTR_ERR(core->dev);
		dprintk(CVP_ERR, "device_create failed: %d\n",
				rc);
		goto err_device_create;
	}
	dev_set_drvdata(core->dev, core);

	cdev_init(&core->cdev, &cvp_fops);
	rc = cdev_add(&core->cdev,
			MKDEV(MAJOR(core->dev_num), 0), 1);
	if (rc < 0) {
		dprintk(CVP_ERR, "cdev_add failed: %d\n",
				rc);
		goto error_cdev_add;
	}

	rc = sysfs_create_group(&core->dev->kobj, &msm_cvp_core_attr_group);
	if (rc) {
		dprintk(CVP_ERR,
				"Failed to create attributes\n");
		goto err_cores_exceeded;
	}

	/* VM manager shall be started before HFI init */
	vm_manager.vm_ops->vm_start(core);

	core->dev_ops = cvp_hfi_initialize(core->hfi_type,
				&core->resources, &cvp_handle_cmd_response);
	if (IS_ERR_OR_NULL(core->dev_ops)) {
		mutex_lock(&cvp_driver->lock);
		mutex_unlock(&cvp_driver->lock);

		rc = PTR_ERR(core->dev_ops) ?: -EBADHANDLE;
		if (rc != -EPROBE_DEFER)
			dprintk(CVP_ERR, "Failed to create HFI device\n");
		else
			dprintk(CVP_CORE, "msm_cvp: request probe defer\n");
		goto err_hfi_initialize;
	}

	cvp_synx_ftbl_init(core);

	mutex_lock(&cvp_driver->lock);
	cvp_driver->cvp_core = core;
	mutex_unlock(&cvp_driver->lock);

	cvp_driver->debugfs_root = msm_cvp_debugfs_init_drv();
	if (!cvp_driver->debugfs_root)
		dprintk(CVP_ERR, "Failed to create debugfs for msm_cvp\n");

	core->debugfs_root = msm_cvp_debugfs_init_core(
		core, cvp_driver->debugfs_root);

	cvp_driver->sku_version = core->resources.sku_version;

	dprintk(CVP_CORE, "populating sub devices\n");
	/*
	 * Trigger probe for each sub-device i.e. qcom,msm-cvp,context-bank.
	 * When msm_cvp_probe is called for each sub-device, parse the
	 * context-bank details and store it in core->resources.context_banks
	 * list.
	 */
	rc = of_platform_populate(pdev->dev.of_node, msm_cvp_plat_match, NULL,
			&pdev->dev);
	if (rc) {
		dprintk(CVP_ERR, "Failed to trigger probe for sub-devices\n");
		goto err_fail_sub_device_probe;
	}

	atomic64_set(&core->kernel_trans_id, ARRAY_SIZE(cvp_hfi_defs));

	if (core->resources.dsp_enabled) {
		rc = cvp_dsp_device_init();
		if (rc)
			dprintk(CVP_WARN, "Failed to initialize DSP driver\n");
	} else {
		dprintk(CVP_DSP, "DSP interface not enabled\n");
	}

	return rc;

err_fail_sub_device_probe:
	cvp_hfi_deinitialize(core->hfi_type, core->dev_ops);
	debugfs_remove_recursive(cvp_driver->debugfs_root);
err_hfi_initialize:
err_cores_exceeded:
	cdev_del(&core->cdev);
error_cdev_add:
	device_destroy(core->class, core->dev_num);
err_device_create:
	class_destroy(core->class);
err_class_create:
	unregister_chrdev_region(core->dev_num, 1);
err_alloc_chrdev:
	sysfs_remove_group(&pdev->dev.kobj, &msm_cvp_core_attr_group);
err_core_init:
	dev_set_drvdata(&pdev->dev, NULL);
	kfree(core);
	return rc;
}

static int msm_cvp_probe_mem_cdsp(struct platform_device *pdev)
{
	return cvp_read_mem_cdsp_resources_from_dt(pdev);
}

static int msm_cvp_probe_context_bank(struct platform_device *pdev)
{
	return cvp_read_context_bank_resources_from_dt(pdev);
}

static int msm_cvp_probe_bus(struct platform_device *pdev)
{
	return cvp_read_bus_resources_from_dt(pdev);
}

static int msm_cvp_probe(struct platform_device *pdev)
{
	if (!msm_cvp_probe_allowed)
		return 0;
	/*
	 * Sub devices probe will be triggered by of_platform_populate() towards
	 * the end of the probe function after msm-cvp device probe is
	 * completed. Return immediately after completing sub-device probe.
	 */
	if (of_device_is_compatible(pdev->dev.of_node, "qcom,msm-cvp")) {
		return msm_probe_cvp_device(pdev);
	} else if (of_device_is_compatible(pdev->dev.of_node,
		"qcom,msm-cvp,bus")) {
		return msm_cvp_probe_bus(pdev);
	} else if (of_device_is_compatible(pdev->dev.of_node,
		"qcom,msm-cvp,context-bank")) {
		return msm_cvp_probe_context_bank(pdev);
	} else if (of_device_is_compatible(pdev->dev.of_node,
		"qcom,msm-cvp,mem-cdsp")) {
		return msm_cvp_probe_mem_cdsp(pdev);
	}

	/* How did we end up here? */
	MSM_CVP_ERROR(1);
	return -EINVAL;
}

static int msm_cvp_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_cvp_core *core;

	if (!pdev) {
		dprintk(CVP_ERR, "%s invalid input %pK", __func__, pdev);
		return -EINVAL;
	}

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,msm-cvp"))
		core = dev_get_drvdata(&pdev->dev);
	else
		core = dev_get_drvdata(pdev->dev.parent);

	if (!core) {
		dprintk(CVP_ERR, "%s invalid core", __func__);
		return -EINVAL;
	}

	cvp_hfi_deinitialize(core->hfi_type, core->dev_ops);
	msm_cvp_free_platform_resources(&core->resources);
	sysfs_remove_group(&pdev->dev.kobj, &msm_cvp_core_attr_group);
	dev_set_drvdata(&pdev->dev, NULL);
	mutex_destroy(&core->lock);
	mutex_destroy(&core->clk_lock);
	kfree(core);
	return rc;
}

static int msm_cvp_pm_suspend(struct device *dev)
{
	int rc = 0;
	struct msm_cvp_core *core;

	/*
	 * Bail out if
	 * - driver possibly not probed yet
	 * - not the main device. We don't support power management on
	 *   subdevices (e.g. context banks)
	 */
	if (!dev || !dev->driver ||
		!of_device_is_compatible(dev->of_node, "qcom,msm-cvp"))
		return 0;

	core = dev_get_drvdata(dev);
	if (!core) {
		dprintk(CVP_ERR, "%s invalid core\n", __func__);
		return -EINVAL;
	}

	rc = msm_cvp_suspend();
	if (rc == -ENOTSUPP)
		rc = 0;
	else if (rc)
		dprintk(CVP_WARN, "Failed to suspend: %d\n", rc);


	return rc;
}

static int msm_cvp_pm_resume(struct device *dev)
{
	dprintk(CVP_INFO, "%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops msm_cvp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(msm_cvp_pm_suspend, msm_cvp_pm_resume)
};

MODULE_DEVICE_TABLE(of, msm_cvp_plat_match);

static struct platform_driver msm_cvp_driver = {
	.probe = msm_cvp_probe,
	.remove = msm_cvp_remove,
	.driver = {
		.name = "msm_cvp",
		.of_match_table = msm_cvp_plat_match,
		.pm = &msm_cvp_pm_ops,
	},
};

static int __init msm_cvp_init(void)
{
	int rc = 0;

	cvp_driver = kzalloc(sizeof(*cvp_driver), GFP_KERNEL);
	if (!cvp_driver) {
		dprintk(CVP_ERR,
			"Failed to allocate memroy for msm_cvp_drv\n");
		return -ENOMEM;
	}

	mutex_init(&cvp_driver->lock);

	rc = platform_driver_register(&msm_cvp_driver);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to register platform driver\n");
		kfree(cvp_driver);
		cvp_driver = NULL;
		return rc;
	}

	cvp_driver->msg_cache.cache = KMEM_CACHE(cvp_session_msg, 0);
	cvp_driver->frame_cache.cache = KMEM_CACHE(msm_cvp_frame, 0);
	cvp_driver->buf_cache.cache = KMEM_CACHE(cvp_internal_buf, 0);
	cvp_driver->smem_cache.cache = KMEM_CACHE(msm_cvp_smem, 0);
	mutex_init(&wncc_buf_pool.lock);

	return rc;
}

static void __exit msm_cvp_exit(void)
{
	cvp_dsp_device_exit();
	kmem_cache_destroy(cvp_driver->msg_cache.cache);
	kmem_cache_destroy(cvp_driver->frame_cache.cache);
	kmem_cache_destroy(cvp_driver->buf_cache.cache);
	kmem_cache_destroy(cvp_driver->smem_cache.cache);

	platform_driver_unregister(&msm_cvp_driver);
	debugfs_remove_recursive(cvp_driver->debugfs_root);
	mutex_destroy(&cvp_driver->lock);
	mutex_destroy(&wncc_buf_pool.lock);
	kfree(cvp_driver);
	cvp_driver = NULL;
}

module_init(msm_cvp_init);
module_exit(msm_cvp_exit);

MODULE_SOFTDEP("pre: msm-mmrm");
MODULE_SOFTDEP("pre: synx-driver");
MODULE_SOFTDEP("pre: frpc-adsprpc");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(DMA_BUF);
