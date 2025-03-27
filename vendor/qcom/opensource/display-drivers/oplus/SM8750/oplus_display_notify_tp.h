/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_notify_tp.h
** Description : display send panel event notification to tp
** Version : 1.0
** Date : 2024/10/24
** Author : Touchpanel
******************************************************************/
#ifndef _OPLUS_DISPLAY_NOTIFY_TP_H_
#define _OPLUS_DISPLAY_NOTIFY_TP_H_

#include "dsi_panel.h"
#include "oplus_display_interface.h"

#define OPLUS_TP_ALWAYS_SUPPORT_LCD_FOR_DEBUG 0

enum panel_index {
	PRIMARY   = 0,
	SECONDARY = 1,
	PANEL_MAX = 2
};

enum lcd_event_type {
	LCD_CTL_TP_LOAD_FW = 0x10,
	LCD_CTL_RST_ON,
	LCD_CTL_RST_OFF,
	LCD_CTL_TP_FTM,
	LCD_CTL_TP_FPS60,
	LCD_CTL_TP_FPS90,
	LCD_CTL_TP_FPS120,
	LCD_CTL_TP_FPS180,
	LCD_CTL_TP_FPS240,
	LCD_CTL_CS_ON,
	LCD_CTL_CS_OFF,
	LCD_CTL_IRQ_ON,
	LCD_CTL_IRQ_OFF,
};

struct oplus_display_notify_tp_ops {
	bool (*tp_panel_power_on_supply)(struct dsi_panel *panel);
	void (*tp_panel_power_on_load_fw)(struct dsi_panel *panel);
	void (*tp_panel_power_off_cs_off)(struct dsi_panel *panel);
	void (*tp_panel_power_off_rst)(struct dsi_panel *panel);
	bool (*tp_panel_power_off_supply)(struct dsi_panel *panel);
};

void oplus_display_notify_tp_ops_init(struct oplus_display_notify_tp_ops *oplus_display_notify_tp_ops);
#endif /* _OPLUS_DISPLAY_NOTIFY_TP_H_ */
