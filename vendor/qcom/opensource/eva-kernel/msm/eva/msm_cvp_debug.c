// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include "msm_cvp_debug.h"
#include "msm_cvp_common.h"
#include "cvp_core_hfi.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_dsp.h"

#define MAX_SSR_STRING_LEN 10
#ifdef USE_PRESIL
int msm_cvp_debug = CVP_ERR | CVP_WARN | CVP_FW | CVP_DBG;
#else
int msm_cvp_debug = CVP_ERR | CVP_WARN | CVP_FW;
#endif
EXPORT_SYMBOL(msm_cvp_debug);

int msm_cvp_debug_out = CVP_OUT_PRINTK;
EXPORT_SYMBOL(msm_cvp_debug_out);

#ifdef USE_PRESIL
int msm_cvp_fw_low_power_mode = !1;
#else
int msm_cvp_fw_low_power_mode = 1;
#endif
#ifdef USE_PRESIL42
bool msm_cvp_auto_pil = !true;
int msm_cvp_fw_debug = 0x3F;
#else
bool msm_cvp_auto_pil = true;
int msm_cvp_fw_debug = 0x10018;
#endif
int msm_cvp_fw_debug_mode = 1;
bool msm_cvp_fw_coverage = !true;
bool msm_cvp_cacheop_enabled = true;
bool msm_cvp_thermal_mitigation_disabled = !true;
bool msm_cvp_cacheop_disabled = !true;
bool msm_cvp_probe_allowed = true;
int msm_cvp_clock_voting = !1;
bool msm_cvp_syscache_disable = !true;
bool msm_cvp_dsp_disable = !true;
#ifdef CVP_MMRM_ENABLED
bool msm_cvp_mmrm_enabled = true;
#else
bool msm_cvp_mmrm_enabled = !true;
#endif
bool msm_cvp_dcvs_disable = !true;
int msm_cvp_minidump_enable = !1;
int cvp_kernel_fence_enabled = 2;
int msm_cvp_hw_wd_recovery = 1;
int msm_cvp_smmu_fault_recovery = !1;

#define MAX_DBG_BUF_SIZE 4096

struct cvp_core_inst_pair {
	struct msm_cvp_core *core;
	struct msm_cvp_inst *inst;
};

static int core_info_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	dprintk(CVP_INFO, "%s: Enter\n", __func__);
	return 0;
}

static u32 write_str(char *buffer,
		size_t size, const char *fmt, ...)
{
	va_list args;
	u32 len;

	va_start(args, fmt);
	len = vscnprintf(buffer, size, fmt, args);
	va_end(args);
	return len;
}

static ssize_t core_info_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct msm_cvp_core *core = file->private_data;
	struct cvp_hfi_ops *ops_tbl;
	struct cvp_hal_fw_info fw_info = { {0} };
	char *dbuf, *cur, *end;
	int i = 0, rc = 0;
	ssize_t len = 0;

	if (!core || !core->dev_ops) {
		dprintk(CVP_ERR, "Invalid params, core: %pK\n", core);
		return 0;
	}

	dbuf = kzalloc(MAX_DBG_BUF_SIZE, GFP_KERNEL);
	if (!dbuf) {
		dprintk(CVP_ERR, "%s: Allocation failed!\n", __func__);
		return -ENOMEM;
	}
	cur = dbuf;
	end = cur + MAX_DBG_BUF_SIZE;
	ops_tbl = core->dev_ops;

	cur += write_str(cur, end - cur, "===============================\n");
	cur += write_str(cur, end - cur, "CORE %d: %pK\n", 0, core);
	cur += write_str(cur, end - cur, "===============================\n");
	cur += write_str(cur, end - cur, "Core state: %d\n", core->state);
	rc = call_hfi_op(ops_tbl, get_fw_info, ops_tbl->hfi_device_data, &fw_info);
	if (rc) {
		dprintk(CVP_WARN, "Failed to read FW info\n");
		goto err_fw_info;
	}

	cur += write_str(cur, end - cur,
		"FW version : %s\n", &fw_info.version);
	cur += write_str(cur, end - cur,
		"base addr: 0x%x\n", fw_info.base_addr);
	cur += write_str(cur, end - cur,
		"register_base: 0x%x\n", fw_info.register_base);
	cur += write_str(cur, end - cur,
		"register_size: %u\n", fw_info.register_size);
	cur += write_str(cur, end - cur, "irq: %u\n", fw_info.irq);

err_fw_info:
	for (i = SYS_MSG_START; i < SYS_MSG_END; i++) {
		cur += write_str(cur, end - cur, "completions[%d]: %s\n", i,
			completion_done(&core->completions[SYS_MSG_INDEX(i)]) ?
			"pending" : "done");
	}
	len = simple_read_from_buffer(buf, count, ppos,
			dbuf, cur - dbuf);

	kfree(dbuf);
	return len;
}

static const struct file_operations core_info_fops = {
	.open = core_info_open,
	.read = core_info_read,
};

static int trigger_ssr_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	dprintk(CVP_INFO, "%s: Enter\n", __func__);
	return 0;
}

static ssize_t trigger_ssr_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned long ssr_trigger_val = 0;
	int rc = 0;
	struct msm_cvp_core *core = filp->private_data;
	size_t size = MAX_SSR_STRING_LEN;
	char kbuf[MAX_SSR_STRING_LEN + 1] = {0};

	if (!buf)
		return -EINVAL;

	if (!count)
		goto exit;

	if (count < size)
		size = count;

	if (copy_from_user(kbuf, buf, size)) {
		dprintk(CVP_WARN, "%s User memory fault\n", __func__);
		rc = -EFAULT;
		goto exit;
	}

	rc = kstrtoul(kbuf, 0, &ssr_trigger_val);
	if (rc) {
		dprintk(CVP_WARN, "returning error err %d\n", rc);
		rc = -EINVAL;
	} else {
		msm_cvp_trigger_ssr(core, ssr_trigger_val);
		rc = count;
	}
exit:
	return rc;
}

static const struct file_operations ssr_fops = {
	.open = trigger_ssr_open,
	.write = trigger_ssr_write,
};

static int cvp_power_get(void *data, u64 *val)
{
	struct cvp_hfi_ops *ops_tbl;
	struct msm_cvp_core *core;
	struct iris_hfi_device *hfi_device;

	core = cvp_driver->cvp_core;
	if (!core)
		return 0;
	ops_tbl = core->dev_ops;
	if (!ops_tbl)
		return 0;

	hfi_device = ops_tbl->hfi_device_data;
	if (!hfi_device)
		return 0;

	*val = hfi_device->power_enabled;
	return 0;
}

#define MIN_PC_INTERVAL 1000
#define MAX_PC_INTERVAL 1000000

static int cvp_power_set(void *data, u64 val)
{
	struct cvp_hfi_ops *ops_tbl;
	struct msm_cvp_core *core;
	struct iris_hfi_device *hfi_device;
	int rc = 0;

	core = cvp_driver->cvp_core;
	if (!core)
		return -EINVAL;

	ops_tbl = core->dev_ops;
	if (!ops_tbl)
		return -EINVAL;

	hfi_device = ops_tbl->hfi_device_data;
	if (!hfi_device)
		return -EINVAL;

	if (val >= MAX_PC_INTERVAL) {
		hfi_device->res->sw_power_collapsible = 0;
	} else if (val > MIN_PC_INTERVAL) {
		hfi_device->res->sw_power_collapsible = 1;
		hfi_device->res->msm_cvp_pwr_collapse_delay =
			(unsigned int)val;
	}

	if (core->state == CVP_CORE_UNINIT)
		return -EINVAL;

	if (val > 0) {
		rc = call_hfi_op(ops_tbl, resume, ops_tbl->hfi_device_data);
		if (rc)
			dprintk(CVP_ERR, "debugfs fail to power on cvp\n");
	}
	return rc;
}

DEFINE_DEBUGFS_ATTRIBUTE(cvp_pwr_fops, cvp_power_get, cvp_power_set, "%llu\n");

struct dentry *msm_cvp_debugfs_init_drv(void)
{
	struct dentry *dir = NULL;

	dir = debugfs_create_dir("msm_cvp", NULL);
	if (IS_ERR_OR_NULL(dir)) {
		dir = NULL;
		goto failed_create_dir;
	}

	debugfs_create_x32("debug_level", 0644, dir, &msm_cvp_debug);
	debugfs_create_x32("fw_level", 0644, dir, &msm_cvp_fw_debug);
	debugfs_create_u32("fw_debug_mode", 0644, dir, &msm_cvp_fw_debug_mode);
	debugfs_create_u32("fw_low_power_mode", 0644, dir,
		&msm_cvp_fw_low_power_mode);
	debugfs_create_u32("debug_output", 0644, dir, &msm_cvp_debug_out);
	debugfs_create_u32("minidump_enable", 0644, dir,
			&msm_cvp_minidump_enable);
	debugfs_create_bool("fw_coverage", 0644, dir, &msm_cvp_fw_coverage);
	debugfs_create_bool("auto_pil", 0644, dir, &msm_cvp_auto_pil);
	debugfs_create_u32("kernel_fence", 0644, dir, &cvp_kernel_fence_enabled);
	debugfs_create_bool("disable_thermal_mitigation", 0644, dir,
			&msm_cvp_thermal_mitigation_disabled);
	debugfs_create_bool("enable_cacheop", 0644, dir,
			&msm_cvp_cacheop_enabled);
	debugfs_create_bool("disable_cvp_syscache", 0644, dir,
			&msm_cvp_syscache_disable);
	debugfs_create_bool("disable_dcvs", 0644, dir,
			&msm_cvp_dcvs_disable);

	debugfs_create_file("cvp_power", 0644, dir, NULL, &cvp_pwr_fops);

	return dir;

failed_create_dir:
	if (dir)
		debugfs_remove_recursive(cvp_driver->debugfs_root);

	dprintk(CVP_WARN, "Failed to create debugfs\n");
	return NULL;
}

static int _clk_rate_set(void *data, u64 val)
{
	struct msm_cvp_core *core;
	struct cvp_hfi_ops *ops_tbl;
	struct allowed_clock_rates_table *tbl = NULL;
	unsigned int tbl_size, i;

	core = cvp_driver->cvp_core;
	ops_tbl = core->dev_ops;
	tbl = core->resources.allowed_clks_tbl;
	tbl_size = core->resources.allowed_clks_tbl_size;

	if (val == 0) {
		struct iris_hfi_device *hdev = ops_tbl->hfi_device_data;

		msm_cvp_clock_voting = 0;
		call_hfi_op(ops_tbl, scale_clocks, hdev, hdev->clk_freq);
		return 0;
	}

	for (i = 0; i < tbl_size; i++)
		if (val <= tbl[i].clock_rate)
			break;

	if (i == tbl_size)
		msm_cvp_clock_voting = tbl[tbl_size-1].clock_rate;
	else
		msm_cvp_clock_voting = tbl[i].clock_rate;

	dprintk(CVP_WARN, "Override cvp_clk_rate with %d\n",
			msm_cvp_clock_voting);

	call_hfi_op(ops_tbl, scale_clocks, ops_tbl->hfi_device_data,
		msm_cvp_clock_voting);

	return 0;
}

static int _clk_rate_get(void *data, u64 *val)
{
	struct msm_cvp_core *core;
	struct iris_hfi_device *hdev;

	core = cvp_driver->cvp_core;
	hdev = core->dev_ops->hfi_device_data;
	if (msm_cvp_clock_voting)
		*val = msm_cvp_clock_voting;
	else
		*val = hdev->clk_freq;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(clk_rate_fops, _clk_rate_get, _clk_rate_set, "%llu\n");

static int _dsp_dbg_set(void *data, u64 val)
{
	gfa_cv.debug_mask = (uint32_t)val;

	cvp_dsp_send_debug_mask();

	return 0;
}

static int _dsp_dbg_get(void *data, u64 *val)
{
	*val = gfa_cv.debug_mask;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(dsp_debug_fops, _dsp_dbg_get, _dsp_dbg_set, "%llu\n");

static int _max_ssr_set(void *data, u64 val)
{
	struct msm_cvp_core *core;
	core = cvp_driver->cvp_core;
	if (core) {
		if (val < 1) {
			dprintk(CVP_WARN,
				"Invalid max_ssr_allowed value %llx\n", val);
			return 0;
		}

		core->resources.max_ssr_allowed = (unsigned int)val;
	}
	return 0;
}

static int _max_ssr_get(void *data, u64 *val)
{
	struct msm_cvp_core *core;
	core = cvp_driver->cvp_core;
	if (core)
		*val = core->resources.max_ssr_allowed;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(max_ssr_fops, _max_ssr_get, _max_ssr_set, "%llu\n");

static int _ssr_stall_set(void *data, u64 val)
{
	struct msm_cvp_core *core;
	core = cvp_driver->cvp_core;
	if (core)
		core->resources.fatal_ssr = (val >= 1) ? true : false;

	return 0;
}

static int _ssr_stall_get(void *data, u64 *val)
{
	struct msm_cvp_core *core;
	core = cvp_driver->cvp_core;
	if (core)
		*val = core->resources.fatal_ssr ? 1 : 0;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(ssr_stall_fops, _ssr_stall_get, _ssr_stall_set, "%llu\n");


struct dentry *msm_cvp_debugfs_init_core(struct msm_cvp_core *core,
		struct dentry *parent)
{
	struct dentry *dir = NULL;
	char debugfs_name[MAX_DEBUGFS_NAME];

	if (!core) {
		dprintk(CVP_ERR, "Invalid params, core: %pK\n", core);
		goto failed_create_dir;
	}

	snprintf(debugfs_name, MAX_DEBUGFS_NAME, "core%d", 0);
	dir = debugfs_create_dir(debugfs_name, parent);
	if (IS_ERR_OR_NULL(dir)) {
		dir = NULL;
		dprintk(CVP_ERR, "Failed to create debugfs for msm_cvp\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_file("info", 0444, dir, core, &core_info_fops)) {
		dprintk(CVP_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_file("trigger_ssr", 0200,
			dir, core, &ssr_fops)) {
		dprintk(CVP_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_file("clock_rate", 0644, dir,
			NULL, &clk_rate_fops)) {
		dprintk(CVP_ERR, "debugfs_create_file: clock_rate fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_file("dsp_debug_level", 0644, dir,
			NULL, &dsp_debug_fops)) {
		dprintk(CVP_ERR, "debugfs_create: dsp_debug_level fail\n");
		goto failed_create_dir;
	}

	if (!debugfs_create_file("max_ssr_allowed", 0644, dir,
			NULL, &max_ssr_fops)) {
		dprintk(CVP_ERR, "debugfs_create: max_ssr_allowed fail\n");
		goto failed_create_dir;
	}

	if (!debugfs_create_file("ssr_stall", 0644, dir,
			NULL, &ssr_stall_fops)) {
		dprintk(CVP_ERR, "debugfs_create: ssr_stall fail\n");
		goto failed_create_dir;
	}
	debugfs_create_u32("hw_wd_recovery", 0644, dir,
		&msm_cvp_hw_wd_recovery);
	debugfs_create_u32("smmu_fault_recovery", 0644, dir,
		&msm_cvp_smmu_fault_recovery);
failed_create_dir:
	return dir;
}

static int inst_info_open(struct inode *inode, struct file *file)
{
	dprintk(CVP_INFO, "Open inode ptr: %pK\n", inode->i_private);
	file->private_data = inode->i_private;
	return 0;
}

static int publish_unreleased_reference(struct msm_cvp_inst *inst,
		char **dbuf, char *end)
{
	dprintk(CVP_SESS, "%s deprecated function\n", __func__);
	return 0;
}

static void put_inst_helper(struct kref *kref)
{
	struct msm_cvp_inst *inst;

	if (!kref)
		return;
	inst = container_of(kref, struct msm_cvp_inst, kref);

	msm_cvp_destroy(inst);
}

static ssize_t inst_info_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct cvp_core_inst_pair *idata = file->private_data;
	struct msm_cvp_core *core;
	struct msm_cvp_inst *inst, *temp = NULL;
	char *dbuf, *cur, *end;
	int i;
	ssize_t len = 0;

	if (!idata || !idata->core || !idata->inst) {
		dprintk(CVP_ERR, "%s: Invalid params\n", __func__);
		return 0;
	}

	core = idata->core;
	inst = idata->inst;

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		if (temp == inst)
			break;
	}
	inst = ((temp == inst) && kref_get_unless_zero(&inst->kref)) ?
		inst : NULL;
	mutex_unlock(&core->lock);

	if (!inst) {
		dprintk(CVP_ERR, "%s: Instance has become obsolete", __func__);
		return 0;
	}

	dbuf = kzalloc(MAX_DBG_BUF_SIZE, GFP_KERNEL);
	if (!dbuf) {
		dprintk(CVP_ERR, "%s: Allocation failed!\n", __func__);
		len = -ENOMEM;
		goto failed_alloc;
	}
	cur = dbuf;
	end = cur + MAX_DBG_BUF_SIZE;

	cur += write_str(cur, end - cur, "==============================\n");
	cur += write_str(cur, end - cur, "INSTANCE: %pK (%s)\n", inst,
		inst->session_type == MSM_CVP_USER ? "User" : "Kernel");
	cur += write_str(cur, end - cur, "==============================\n");
	cur += write_str(cur, end - cur, "core: %pK\n", inst->core);
	cur += write_str(cur, end - cur, "state: %d\n", inst->state);
	cur += write_str(cur, end - cur, "secure: %d\n",
		!!(inst->flags & CVP_SECURE));
	for (i = SESSION_MSG_START; i < SESSION_MSG_END; i++) {
		cur += write_str(cur, end - cur, "completions[%d]: %s\n", i,
		completion_done(&inst->completions[SESSION_MSG_INDEX(i)]) ?
		"pending" : "done");
	}

	publish_unreleased_reference(inst, &cur, end);
	len = simple_read_from_buffer(buf, count, ppos,
		dbuf, cur - dbuf);

	kfree(dbuf);
failed_alloc:
	kref_put(&inst->kref, put_inst_helper);
	return len;
}

static int inst_info_release(struct inode *inode, struct file *file)
{
	dprintk(CVP_INFO, "Release inode ptr: %pK\n", inode->i_private);
	file->private_data = NULL;
	return 0;
}

static const struct file_operations inst_info_fops = {
	.open = inst_info_open,
	.read = inst_info_read,
	.release = inst_info_release,
};

struct dentry *msm_cvp_debugfs_init_inst(struct msm_cvp_inst *inst,
		struct dentry *parent)
{
	struct dentry *dir = NULL, *info = NULL;
	char debugfs_name[MAX_DEBUGFS_NAME];
	struct cvp_core_inst_pair *idata = NULL;

	if (!inst) {
		dprintk(CVP_ERR, "Invalid params, inst: %pK\n", inst);
		goto exit;
	}
	snprintf(debugfs_name, MAX_DEBUGFS_NAME, "inst_%pK", inst);

	idata = kzalloc(sizeof(*idata), GFP_KERNEL);
	if (!idata) {
		dprintk(CVP_ERR, "%s: Allocation failed!\n", __func__);
		goto exit;
	}

	idata->core = inst->core;
	idata->inst = inst;

	dir = debugfs_create_dir(debugfs_name, parent);
	if (IS_ERR_OR_NULL(dir)) {
		dir = NULL;
		dprintk(CVP_ERR, "Failed to create debugfs for msm_cvp\n");
		goto failed_create_dir;
	}

	info = debugfs_create_file("info", 0444, dir,
			idata, &inst_info_fops);
	if (!info) {
		dprintk(CVP_ERR, "debugfs_create_file: info fail\n");
		goto failed_create_file;
	}

	dir->d_inode->i_private = info->d_inode->i_private;
	inst->debug.pdata[FRAME_PROCESSING].sampling = true;
	return dir;

failed_create_file:
	debugfs_remove_recursive(dir);
	dir = NULL;
failed_create_dir:
	kfree(idata);
exit:
	return dir;
}

void msm_cvp_debugfs_deinit_inst(struct msm_cvp_inst *inst)
{
	struct dentry *dentry = NULL;

	if (!inst || !inst->debugfs_root)
		return;

	dentry = inst->debugfs_root;
	if (dentry->d_inode) {
		dprintk(CVP_INFO, "Destroy %pK\n", dentry->d_inode->i_private);
		kfree(dentry->d_inode->i_private);
		dentry->d_inode->i_private = NULL;
	}
	debugfs_remove_recursive(dentry);
	inst->debugfs_root = NULL;
}
