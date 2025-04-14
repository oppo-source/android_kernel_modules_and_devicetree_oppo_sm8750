/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_ffl.h
** Description : oplus ffl feature
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_FFL_H_
#define _OPLUS_DISPLAY_FFL_H_

#include <linux/kthread.h>

void oplus_ffl_set(int enable);
void oplus_ffl_setting_thread(struct kthread_work *work);
void oplus_start_ffl_thread(void);
void oplus_stop_ffl_thread(void);
int oplus_ffl_thread_init(void);
void oplus_ffl_thread_exit(void);
int oplus_display_panel_set_ffl(void *buf);
int oplus_display_panel_get_ffl(void *buf);
int oplus_panel_parse_ffc_config(struct dsi_panel *panel);

#endif /* _OPLUS_DISPLAY_FFL_H_ */

