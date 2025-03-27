// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include "cam_debug.h"

unsigned long long oplus_debug_mdl;
module_param(oplus_debug_mdl, ullong, 0644);
/* 0x0 - only logs, 0x1 - only trace, 0x2 - logs + trace */
uint oplus_debug_type;
module_param(oplus_debug_type, uint, 0644);

uint oplus_debug_priority;
module_param(oplus_debug_priority, uint, 0644);




struct dentry *cam_ext_debugfs_root;

void cam_ext_debugfs_init(void)
{
	struct dentry *tmp;

	if (!cam_ext_debugfs_available()) {
		cam_ext_debugfs_root = NULL;
		CAM_EXT_DBG(CAM_EXT_UTIL, "debugfs not available");
		return;
	}

	if (cam_ext_debugfs_root) {
		CAM_EXT_WARN(CAM_EXT_UTIL, "already created debugfs root");
		return;
	}

	tmp = debugfs_create_dir("camera_extension", NULL);
	if (IS_ERR_VALUE(tmp)) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "failed to create debugfs root folder (rc=%d)", PTR_ERR(tmp));
		return;
	}

	cam_ext_debugfs_root = tmp;
	CAM_EXT_DBG(CAM_EXT_UTIL, "successfully created debugfs root");
}

void cam_ext_debugfs_deinit(void)
{
	if (!cam_ext_debugfs_available())
		return;

	debugfs_remove_recursive(cam_ext_debugfs_root);
	cam_ext_debugfs_root = NULL;
}

int cam_ext_debugfs_create_subdir(const char *name, struct dentry **subdir)
{
	struct dentry *tmp;

	if (!cam_ext_debugfs_root) {
		CAM_EXT_WARN(CAM_EXT_UTIL, "debugfs root not created");
		*subdir = NULL;
		return -ENODEV;
	}

	if (!subdir) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "invalid subdir pointer %pK", subdir);
		return -EINVAL;
	}

	tmp = debugfs_create_dir(name, cam_ext_debugfs_root);
	if (IS_ERR_VALUE(tmp)) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "failed to create debugfs subdir (name=%s, rc=%d)", name,
			PTR_ERR(tmp));
		return PTR_ERR(tmp);
	}

	*subdir = tmp;
	return 0;
}

int cam_ext_debugfs_lookup_subdir(const char *name, struct dentry **subdir)
{
	if (!cam_ext_debugfs_root) {
		CAM_EXT_WARN(CAM_EXT_UTIL, "debugfs root not created");
		*subdir = NULL;
		return -ENODEV;
	}

	if (!subdir) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "invalid subdir pointer %pK", subdir);
		return -EINVAL;
	}

	*subdir = debugfs_lookup(name, cam_ext_debugfs_root);
	return (*subdir) ? 0 : -ENOENT;
}

static inline void __cam_ext_print_to_buffer(char *buf, const size_t buf_size, size_t *len,
	unsigned int tag, enum cam_ext_debug_module_id module_id, const char *fmt, va_list args)
{
	size_t buf_len = *len;

	buf_len += scnprintf(buf + buf_len, (buf_size - buf_len), "\n%-8s: %s:\t",
			CAM_EXT_LOG_TAG_NAME(tag), CAM_EXT_DBG_MOD_NAME(module_id));
	buf_len += vscnprintf(buf + buf_len, (buf_size - buf_len), fmt, args);
	*len = buf_len;
}

void cam_ext_print_to_buffer(char *buf, const size_t buf_size, size_t *len, unsigned int tag,
	unsigned long long module_id, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	__cam_ext_print_to_buffer(buf, buf_size, len, tag, module_id, fmt, args);
	va_end(args);
}

static void __cam_ext_print_log(int type, const char *fmt, ...)
{
	va_list args1, args;

	va_start(args, fmt);
	va_copy(args1, args);
	if ((type & CAM_EXT_PRINT_LOG) && (oplus_debug_type != 1))
		vprintk(fmt, args1);

	va_end(args1);
	va_end(args);
}

void cam_ext_print_log(int type, int module, int tag, const char *func,
	int line, const char *fmt, ...)
{
	char buf[CAM_EXT_LOG_BUF_LEN] = {0,};
	va_list args;

	if (!type)
		return;

	va_start(args, fmt);
	vscnprintf(buf, CAM_EXT_LOG_BUF_LEN, fmt, args);
	__cam_ext_print_log(type, __CAM_EXT_LOG_FMT,
		CAM_EXT_LOG_TAG_NAME(tag), CAM_EXT_DBG_MOD_NAME(module), func,
		line, buf);
	va_end(args);
}
