/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_power.h
** Description : oplus display panel power control
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_POWER_H_
#define _OPLUS_DISPLAY_POWER_H_

#include "dsi_panel.h"
#include "dsi_display.h"
#include "oplus_display.h"

#define PANEL_VOLTAGE_VALUE_COUNT 4

enum PANEL_VOLTAGE_ENUM {
	PANEL_VOLTAGE_ID_VDDI = 0,
	PANEL_VOLTAGE_ID_VDDR,
	PANEL_VOLTAGE_ID_VG_BASE,
	PANEL_VOLTAGE_ID_MAX,
};

enum PANEL_POWER_SUPPLY_ENUM {
	PANEL_POWER_SUPPLY_MESSAGE = 0,
	PANEL_POWER_SUPPLY_VDDIO,
	PANEL_POWER_SUPPLY_VCI,
	PANEL_POWER_SUPPLY_VDD,
	PANEL_POWER_SUPPLY_GPIO1,
	PANEL_POWER_SUPPLY_GPIO2,
	PANEL_POWER_SUPPLY_GPIO3,
	PANEL_POWER_SUPPLY_MAX,
};

enum PANEL_POWER_MESSAGE_ENUM {
	PANEL_POWER_SUPPLY_ORDER = 0,
	PANEL_POWER_SUPPLY_POST_MS,
	PANEL_POWER_SUPPLY_MESSAGE_MAX,
};

enum PANEL_RESET_POSITION_ENUM {
	PANEL_RESET_POSITION0 = 0,
	PANEL_RESET_POSITION1,
	PANEL_RESET_POSITION2,
};

struct panel_vol_set {
	uint32_t panel_id;
	uint32_t panel_vol;
};

struct panel_vol_get {
	uint32_t panel_id;
	uint32_t panel_min;
	uint32_t panel_cur;
	uint32_t panel_max;
};

typedef struct panel_voltage_bak {
	u32 voltage_id;
	u32 voltage_min;
	u32 voltage_current;
	u32 voltage_max;
	char pwr_name[20];
} PANEL_VOLTAGE_BAK;

int oplus_panel_parse_power_config(struct dsi_panel *panel);
int oplus_display_panel_set_pwr(void *data);
int oplus_display_panel_get_pwr(void *data);
int oplus_display_panel_get_power_status(void *data);
int oplus_display_panel_set_power_status(void *data);
int oplus_display_panel_regulator_control(void *data);
int __oplus_set_request_power_status(int status);
int oplus_panel_parse_gpios(struct dsi_panel *panel);

/**
 * oplus_panel_gpio_request() - oplus panel config gpio request
 * @panel: Display panel
 * Return: Zero on Success
 */
int oplus_panel_gpio_request(struct dsi_panel *panel);

/**
 * oplus_panel_gpio_release() - oplus panel config gpio release
 * @panel: Display panel
 * Return: Zero on Success
 */
int oplus_panel_gpio_release(struct dsi_panel *panel);

/**
 * oplus_panel_set_pinctrl_state() - oplus panel set pinctrl state
 * @panel:  Display panel
 * @enable: Pinctrl state
 * Return: Zero on Success
 */
int oplus_panel_set_pinctrl_state(struct dsi_panel *panel, bool enable);

/**
 * oplus_panel_pinctrl_init() - oplus panel pinctrl init
 * @panel: Display panel
 * Return: Zero on Success
 */
int oplus_panel_pinctrl_init(struct dsi_panel *panel);
int oplus_panel_parse_power_sequence_config(struct dsi_panel *panel);
int oplus_panel_power_on(struct dsi_panel *panel);
int oplus_panel_power_off(struct dsi_panel *panel);
int oplus_panel_prepare(struct dsi_panel *panel);
bool oplus_panel_pre_prepare(struct dsi_panel *panel);
void oplus_panel_register_supply_notifier(void);
int oplus_bl_ic_ktz8868_power_on(struct dsi_panel *panel);
void oplus_bl_ic_ktz8868_power_off(struct dsi_panel *panel);
#endif /* _OPLUS_DISPLAY_POWER_H_ */
