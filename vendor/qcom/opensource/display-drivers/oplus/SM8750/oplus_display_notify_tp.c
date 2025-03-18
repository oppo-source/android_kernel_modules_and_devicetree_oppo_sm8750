/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_notify_tp.c
** Description : add for display notify tp
** Version : 1.0
** Date : 2024/10/24
** Author : Touchpanel
******************************************************************/
#include <linux/delay.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#include "oplus_display_notify_tp.h"
#include "oplus_debug.h"
#include "oplus_display_power.h"

static bool is_pd_with_guesture = false;
static int panel_esd_check_failed = 0;
static bool panel_need_power_on = false;
extern int (*tp_gesture_enable_notifier)(unsigned int tp_index);

static int oplus_panel_event_notify_for_touch(struct dsi_panel *panel, int blank)
{
	struct panel_event_notification notifier;
	enum panel_event_notifier_tag panel_type;

	if (!panel) {
		OPLUS_DSI_ERR("Oplus Features config No panel device\n");
		return -ENODEV;
	}

	if (!strcmp(panel->type, "secondary")) {
		panel_type = PANEL_EVENT_NOTIFICATION_SECONDARY;
	} else {
		panel_type = PANEL_EVENT_NOTIFICATION_PRIMARY;
	}

	notifier.panel = &panel->drm_panel;
	notifier.notif_type = DRM_PANEL_EVENT_FOR_TOUCH;
	notifier.notif_data.lcd_ctl_blank = &blank;
	panel_event_notification_trigger(panel_type, &notifier);

	return 0;
}

static bool oplus_tp_panel_is_lcd(struct dsi_panel *panel)
{
	if (!strcmp(panel->name, "Dual dsi csot nt36532 video mode panel with DSC") \
		|| !strcmp(panel->name, "XN242 p d dsc video mode panel") \
		|| !strcmp(panel->name, "vtdr6130 amoled video mode dsi visionox panel with DSC") \
		|| OPLUS_TP_ALWAYS_SUPPORT_LCD_FOR_DEBUG) {
		return true;
	}
	return false;
}

static bool oplus_tp_panel_power_on_supply(struct dsi_panel *panel)
{
	bool power_on_supply = true;

	OPLUS_DSI_INFO("[TP] dsi_panel->name = %s\n", panel->name);
	if (oplus_tp_panel_is_lcd(panel)) {
		if ((tp_gesture_enable_notifier && !tp_gesture_enable_notifier(PRIMARY)) || panel_need_power_on) {
			OPLUS_DSI_INFO("[TP] display set power on, when tp gesture is disable.\n");
			power_on_supply = true;
			panel_need_power_on = false;
		} else {
			OPLUS_DSI_INFO("[TP] display already power on, when tp gesture is enable, do nothing.\n");
			power_on_supply = false;
		}
	}
	return power_on_supply;
}

static void oplus_tp_panel_power_on_load_fw(struct dsi_panel *panel)
{
	if (oplus_tp_panel_is_lcd(panel)) {
		OPLUS_DSI_INFO("[TP] notify touch driver to set cs high, then load tp firmware.\n");
		usleep_range(10*1000, (10*1000)+100);
		oplus_panel_event_notify_for_touch(panel, LCD_CTL_CS_ON);
		oplus_panel_event_notify_for_touch(panel, LCD_CTL_TP_LOAD_FW);
	}
}

static void oplus_tp_panel_power_off_cs_off(struct dsi_panel *panel)
{
	OPLUS_DSI_INFO("[TP] dsi_panel->name = %s\n", panel->name);
	is_pd_with_guesture = false;
	if (oplus_tp_panel_is_lcd(panel)) {
		panel_esd_check_failed = atomic_read(&(panel->esd_recovery_pending));
		if(tp_gesture_enable_notifier && tp_gesture_enable_notifier(PRIMARY)) {
			is_pd_with_guesture = true;
		}
		OPLUS_DSI_INFO("[TP] is_pd_with_guesture = %d, panel_esd_check_failed = %d\n", is_pd_with_guesture, panel_esd_check_failed);
		if ((is_pd_with_guesture == false) || !!panel_esd_check_failed) {
			OPLUS_DSI_INFO("[TP] notify touch driver to set cs low, when tp gesture is disable.\n");
			oplus_panel_event_notify_for_touch(panel, LCD_CTL_CS_OFF);
			usleep_range(116*1000, (116*1000)+100);
		}
	}
}

static void oplus_tp_panel_power_off_rst(struct dsi_panel *panel)
{
	if (oplus_tp_panel_is_lcd(panel)) {
		if ((is_pd_with_guesture == true) && !panel_esd_check_failed) {
			OPLUS_DSI_INFO("[TP] notify touch driver to set reset high, when tp gesture is enable.\n");
			/* oplus_panel_event_notify_for_touch(panel, LCD_CTL_RST_ON); */
			gpio_set_value(panel->reset_config.reset_gpio, 1);
		} else {
			OPLUS_DSI_INFO("[TP] notify touch driver to set reset low and disable tp irq, when tp gesture is disable.\n");
			/* oplus_panel_event_notify_for_touch(panel, LCD_CTL_RST_OFF); */
			gpio_set_value(panel->reset_config.reset_gpio, 0);
			if(panel->oplus_panel.bl_ic_ktz8868_used) {
				oplus_bl_ic_ktz8868_power_off(panel);
			}
			oplus_panel_event_notify_for_touch(panel, LCD_CTL_IRQ_OFF);
		}
	} else {
		/* for other projects */
		gpio_set_value(panel->reset_config.reset_gpio, 0);
	}
}

static bool oplus_tp_panel_power_off_supply(struct dsi_panel *panel)
{
	bool power_off_supply = true;

	if (oplus_tp_panel_is_lcd(panel)) {
		if ((is_pd_with_guesture == false) || !!panel_esd_check_failed) {
			OPLUS_DSI_INFO("[TP] display set power off, when tp gesture is disable.\n");
			power_off_supply = true;
			panel_need_power_on = true;
		} else {
			OPLUS_DSI_INFO("[TP] display keep power on, when tp gesture is enable.\n");
			power_off_supply = false;
		}
	}
	return power_off_supply;
}

void oplus_display_notify_tp_ops_init(struct oplus_display_notify_tp_ops *oplus_display_notify_tp_ops)
{
	OPLUS_DSI_INFO("oplus display notify tp ops init\n");

	oplus_display_notify_tp_ops->tp_panel_power_on_supply = oplus_tp_panel_power_on_supply;
	oplus_display_notify_tp_ops->tp_panel_power_on_load_fw = oplus_tp_panel_power_on_load_fw;
	oplus_display_notify_tp_ops->tp_panel_power_off_cs_off = oplus_tp_panel_power_off_cs_off;
	oplus_display_notify_tp_ops->tp_panel_power_off_rst = oplus_tp_panel_power_off_rst;
	oplus_display_notify_tp_ops->tp_panel_power_off_supply = oplus_tp_panel_power_off_supply;
}
