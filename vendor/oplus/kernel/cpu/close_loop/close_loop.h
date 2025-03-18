// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 Oplus. All rights reserved.
 */

#ifndef __CLOSE_LOOP_H__
#define __CLOSE_LOOP_H__

#include <linux/proc_fs.h>

#define for_each_leaf_cfs_rq_safe(rq, cfs_rq, pos)			\
	list_for_each_entry_safe(cfs_rq, pos, &rq->leaf_cfs_rq_list,	\
				 leaf_cfs_rq_list)

#define PROC_CREATE(name, mode, parent, fops)				\
	do {								\
		proc_create(name, mode, parent, &proc_##fops##_fops);	\
	} while (0)

#define PROC_FOPS(cl_config)								\
static ssize_t proc_##cl_config##_write(struct file *file, const char __user *buf,	\
					size_t count, loff_t *ppos)			\
{											\
	char buffer[PROC_NUMBUF];							\
	int err;									\
											\
	memset(buffer, 0, sizeof(buffer));						\
											\
	if (count > sizeof(buffer) - 1)							\
		count = sizeof(buffer) - 1;						\
											\
	if (copy_from_user(buffer, buf, count))						\
		return -EFAULT;								\
											\
	err = kstrtoint(strstrip(buffer), 10, &cl_config);				\
	if (err)									\
		return err;								\
											\
	return count;									\
}											\
											\
static ssize_t proc_##cl_config##_read(struct file *file, char __user *buf,		\
					size_t count, loff_t *ppos)			\
{											\
	char buffer[PROC_NUMBUF];							\
	size_t len = 0;									\
											\
	len = snprintf(buffer, sizeof(buffer), "%d\n", cl_config);			\
											\
	return simple_read_from_buffer(buf, count, ppos, buffer, len);			\
}											\
											\
static const struct proc_ops proc_##cl_config##_fops = {				\
	.proc_write = proc_##cl_config##_write,						\
	.proc_read = proc_##cl_config##_read,						\
	.proc_lseek = default_llseek,							\
};

enum CL_TP_LEVEL {
	CL_TP_CRIT = 1,
	CL_TP_INFO,
	CL_TP_VERBOSE,

	CL_TP_MAX
};

enum CL_TP {
	CL_TP_ENABLE,
	CL_TP_ACTIVE,
	CL_TP_ACTIVE_REASON,
	CL_TP_UTIL_ORIG,
	CL_TP_UTIL_RESULT,
	CL_TP_UTIL_DELTA,
	CL_TP_CPUFREQ_CHANGED,
	CL_TP_TD_MARK,
	CL_TP_WEIGHT_RATIO,
	CL_TP_TD_PERIOD,
	CL_TP_USAGE,
	CL_TP_USAGE_AVG,
	CL_TP_MULTI_SPLIT,
	CL_TP_MULTI_FLOAT,
	CL_TP_GLTHREAD,
	CL_TP_FLUTTER,

	CL_TP_TYPE_MAX
};

enum CL_REASON {
	CL_REASON_ACTIVE,
	CL_REASON_FRAME_MARGIN_BREAK,
	CL_REASON_RENDER_DELAY_BREAK,
	CL_REASON_VSYNC_RESET_BREAK,
	CL_REASON_AWARE_BOOST_BREAK,
	CL_REASON_NOT_ENABLE,
	CL_REASON_AWARE_USAGE_BREAK,
	CL_REASON_MULTI_ENQ_BREAK,
	CL_REASON_ED_TASK_BREAK,
	CL_REASON_LONG_PERIOD_BREAK,
	CL_REASON_GLTHREAD_BREAK,
	CL_REASON_CAMERA_BREAK,
	CL_REASON_BLOCK_BREAK,

	CL_REASON_MAX
};

enum CL_ACCUMULATE {
	CL_ACC_CURR,
	CL_ACC_PREV,
	CL_ACC_MAX
};

enum CL_MULTI_ENQ {
	CL_SPLIT_START = 1,
	CL_SPLIT_END,
	CL_FLOAT_START,
	CL_FLOAT_END
};

enum CL_GLTHREAD {
	CL_MOVE_TO_BACK = 1,
	CL_GLTHREAD_LIST_START,
	CL_GLTHREAD_LIST_END,
	CL_FLUTTER_START,
	CL_FLUTTER_END
};

/* Close Loop Accumulate */
struct cl_accumulate {
	u64 active_reason[CL_REASON_MAX][CL_ACC_MAX];
};

extern unsigned long cl_util(int cpu, unsigned long orig, bool ed_active);
extern unsigned long cl_boost_util(int cpu, unsigned long orig, bool ed_active);
extern struct proc_dir_entry *cl_get_default_proc_dir_entry(void);
extern void cl_chk_margin(int pid);
extern void cl_chk_td_period(bool from_app);
extern void cl_enq_update(int pid);

#endif /* __CLOSE_LOOP_H__ */
