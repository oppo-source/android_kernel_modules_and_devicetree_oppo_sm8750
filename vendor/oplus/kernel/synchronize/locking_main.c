// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "oplus_locking_strategy:" fmt

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/of.h>

#include "locking_main.h"

#define FEATURE_ENABLE 0xFFFFFFFF

unsigned int g_opt_enable;
unsigned int g_opt_debug;
EXPORT_SYMBOL(g_opt_enable);
unsigned int dynamic_switch;

void parse_dts_switch(void)
{
	struct device_node *np = NULL;
	int ret;

	np = of_find_node_by_name(NULL, "oplus_sync_ipc");

	if(np) {
		ret = of_property_read_u32(np, "disable", &dynamic_switch);
		if(ret) {
			pr_err("no oplus_sync_ipc disable!");
		} else {
			pr_info("oplus_sync_ipc : %d", dynamic_switch);
			return;
		}
	}

	pr_err("no oplus_sync_ipc node!");
	dynamic_switch = FEATURE_ENABLE;
}

static int __init locking_opt_init(void)
{
	int ret = 0;

	parse_dts_switch();

	g_opt_enable |= LK_MUTEX_ENABLE;
	g_opt_enable |= LK_RWSEM_ENABLE;
	g_opt_enable |= LK_FUTEX_ENABLE;
#ifdef CONFIG_OPLUS_LOCKING_OSQ
	g_opt_enable |= LK_OSQ_ENABLE;
#endif

#ifdef CONFIG_LOCKING_PROTECT
	g_opt_enable |= LK_PROTECT_ENABLE;
	g_opt_enable |= LK_SCHED_ENABLE;
	g_opt_enable &= dynamic_switch;
	sched_assist_locking_init();
#endif
	lk_sysfs_init();
	register_rwsem_vendor_hooks();
	register_mutex_vendor_hooks();
	register_futex_vendor_hooks();
#ifdef CONFIG_OPLUS_LOCKING_MONITOR
	g_opt_enable |= LK_MONITOR_ENABLE;
	kern_lstat_init();
#endif
	krn_reliab_init();
	pr_info("g_opt_enable : %d\n", g_opt_enable);
	return ret;
}

static void __exit locking_opt_exit(void)
{
	g_opt_enable = 0;

	unregister_rwsem_vendor_hooks();
	unregister_mutex_vendor_hooks();
	unregister_futex_vendor_hooks();
	lk_sysfs_exit();
#ifdef CONFIG_OPLUS_LOCKING_MONITOR
	kern_lstat_exit();
#endif
}

module_init(locking_opt_init);
module_exit(locking_opt_exit);
module_param_named(locking_enable, g_opt_enable, uint, 0660);
module_param_named(locking_debug, g_opt_debug, uint, 0660);
MODULE_DESCRIPTION("Oplus Locking Strategy Vender Hooks Driver");
MODULE_LICENSE("GPL v2");
