/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_proc.c
** Description : oplus display proc
** Version : 1.0
** Date : 2024/12/09
** Author : Display
******************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include "oplus_display_proc.h"
#include "oplus_debug.h"

static struct proc_dir_entry *oplus_display_proc_dir;

extern int sde_dbg_debugfs_open(struct inode *inode, struct file *file);
extern ssize_t sde_evtlog_dump_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos);

static const struct proc_ops dump_proc_ops =
{
	.proc_open = sde_dbg_debugfs_open,
	.proc_read = sde_evtlog_dump_read,
};

int oplus_display_proc_init(void)
{
	oplus_display_proc_dir = proc_mkdir(OPLUS_DISPLAY_PROC_DIR, NULL);
	if (!oplus_display_proc_dir) {
		OPLUS_DSI_ERR("Failed to create /proc/%s\n", OPLUS_DISPLAY_PROC_DIR);
		return -ENOMEM;
	}

	proc_create(OPLUS_DISPLAY_PROC_FILE_DUMP, 0644,
			oplus_display_proc_dir, &dump_proc_ops);

	OPLUS_DSI_INFO("/proc/%s created\n", OPLUS_DISPLAY_PROC_DIR);

	return 0;
}
