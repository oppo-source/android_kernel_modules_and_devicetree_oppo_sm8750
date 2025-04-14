/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_effect.h
** Description : oplus display panel effect feature
** Version : 1.1
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_EFFECT_H_
#define _OPLUS_DISPLAY_EFFECT_H_

#include <linux/err.h>
#include <dsi_display.h>
#include <dsi_panel.h>

#define PANEL_LOADING_EFFECT_FLAG 100
#define PANEL_LOADING_EFFECT_MODE1 101
#define PANEL_LOADING_EFFECT_MODE2 102
#define PANEL_LOADING_EFFECT_OFF 100
#define PANEL_UIR_ON_LOADING_EFFECT_MODE1 111
#define PANEL_UIR_ON_LOADING_EFFECT_MODE2 112
#define PANEL_UIR_ON_LOADING_EFFECT_MODE3 110
#define PANEL_UIR_OFF_LOADING_EFFECT_MODE1 121
#define PANEL_UIR_OFF_LOADING_EFFECT_MODE2 122
#define PANEL_UIR_OFF_LOADING_EFFECT_MODE3 120
#define PANEL_UIR_LOADING_EFFECT_MODE1 131
#define PANEL_UIR_LOADING_EFFECT_MODE2 132
#define PANEL_UIR_LOADING_EFFECT_MODE3 130

int __oplus_get_seed_mode(void);
int __oplus_set_seed_mode(int mode);
int oplus_display_panel_get_seed(void *data);
int oplus_display_panel_set_seed(void *data);
int dsi_panel_seed_mode(struct dsi_panel *panel, int mode);
int dsi_panel_seed_mode_unlock(struct dsi_panel *panel, int mode);
int dsi_display_seed_mode_lock(struct dsi_display *display, int mode);
int __oplus_read_apl_thread_ctl(bool enable);
int oplus_display_panel_update_apl_value(void *data);
int oplus_get_panel_apl_value(char *buf);

#endif /* _OPLUS_DISPLAY_EFFECT_H_ */

