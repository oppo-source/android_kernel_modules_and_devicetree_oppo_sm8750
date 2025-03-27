/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_bl.c
** Description : oplus display backlight feature implement
** Version : 2.1
** Date : 2024/05/09
** Author : Display
******************************************************************/
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_intf.h"
#include "sde_hw_ctl.h"
#include "sde_formats.h"
#include "sde_encoder.h"
#include "sde_encoder_phys.h"
#include "sde_hw_dsc.h"
#include "sde_hw_vdc.h"
#include "sde_crtc.h"
#include "sde_trace.h"
#include "sde_core_irq.h"
#include "sde_hw_top.h"
#include "sde_hw_qdss.h"
#include "sde_encoder_dce.h"
#include "sde_vm.h"
#include "sde_fence.h"

#include "oplus_bl_ic_ktz8868.h"
#include "oplus_display_bl.h"
#include "oplus_display_ext.h"
#include "oplus_display_interface.h"
#include "oplus_display_effect.h"
#include "oplus_display_pwm.h"
#include "oplus_display_device_ioctl.h"
#include "oplus_onscreenfingerprint.h"
#include "oplus_display_temp_compensation.h"
#include "oplus_adfr.h"
#include "oplus_debug.h"

#define KTZ8868_IC_BL_LEVEL_MAX          (2047)

char oplus_global_hbm_flags = 0x0;
static int enable_hbm_enter_dly_on_flags = 0;
static int enable_hbm_exit_dly_on_flags = 0;
extern u32 oplus_last_backlight;
extern bool refresh_rate_change;
extern int lcd_closebl_flag;
extern u32 oplus_last_backlight;
extern const char *cmd_set_prop_map[];
extern int oplus_debug_max_brightness;
ktime_t lhbm_off_te_timestamp;
bool oplus_temp_compensation_wait_for_vsync_set = false;
/* Add for backlight smooths */
struct oplus_apollo_bk apollo_bk;
u32 g_oplus_save_pcc = 0;
struct dc_apollo_pcc_sync dc_apollo;


int oplus_panel_parse_bl_cfg(struct dsi_panel *panel)
{
	int rc = 0;
	u32 val = 0;
	struct dsi_parser_utils *utils = &panel->utils;

	rc = utils->read_u32(utils->data, "oplus,dsi-bl-normal-max-level", &val);
	if (rc) {
		OPLUS_DSI_INFO("[%s] oplus,dsi-bl-normal-max-level undefined, default to bl max\n",
				panel->oplus_panel.vendor_name);
		panel->oplus_panel.bl_cfg.bl_normal_max_level = panel->bl_config.bl_max_level;
	} else {
		panel->oplus_panel.bl_cfg.bl_normal_max_level = val;
	}
	OPLUS_DSI_INFO("[%s] bl_max_level=%d\n", panel->oplus_panel.vendor_name,
			panel->bl_config.bl_max_level);

	rc = utils->read_u32(utils->data, "oplus,dsi-brightness-normal-max-level",
		&val);
	if (rc) {
		OPLUS_DSI_INFO("[%s] oplus,dsi-brightness-normal-max-level undefined, default to brightness max\n",
				panel->oplus_panel.vendor_name);
		panel->oplus_panel.bl_cfg.brightness_normal_max_level = panel->bl_config.brightness_max_level;
	} else {
		panel->oplus_panel.bl_cfg.brightness_normal_max_level = val;
	}
	OPLUS_DSI_INFO("[%s] brightness_normal_max_level=%d\n",
			panel->oplus_panel.vendor_name,
			panel->oplus_panel.bl_cfg.brightness_normal_max_level);

	rc = utils->read_u32(utils->data, "oplus,dsi-brightness-default-level", &val);
	if (rc) {
		OPLUS_DSI_INFO("[%s] oplus,dsi-brightness-default-level undefined, default to brightness normal max\n",
				panel->oplus_panel.vendor_name);
		panel->oplus_panel.bl_cfg.brightness_default_level = panel->oplus_panel.bl_cfg.brightness_normal_max_level;
	} else {
		panel->oplus_panel.bl_cfg.brightness_default_level = val;
	}
	OPLUS_DSI_INFO("[%s] brightness_default_level=%d\n",
			panel->oplus_panel.vendor_name,
			panel->oplus_panel.bl_cfg.brightness_default_level);

	rc = utils->read_u32(utils->data, "oplus,dsi-dc-backlight-threshold", &val);
	if (rc) {
		OPLUS_DSI_INFO("[%s] oplus,dsi-dc-backlight-threshold undefined, default to 260\n",
				panel->oplus_panel.vendor_name);
		panel->oplus_panel.bl_cfg.dc_backlight_threshold = 260;
		panel->oplus_panel.bl_cfg.oplus_dc_mode = false;
	} else {
		panel->oplus_panel.bl_cfg.dc_backlight_threshold = val;
		panel->oplus_panel.bl_cfg.oplus_dc_mode = true;
	}
	OPLUS_DSI_INFO("[%s] dc_backlight_threshold=%d, oplus_dc_mode=%d\n",
			panel->oplus_panel.vendor_name,
			panel->oplus_panel.bl_cfg.dc_backlight_threshold,
			panel->oplus_panel.bl_cfg.oplus_dc_mode);

	rc = utils->read_u32(utils->data, "oplus,dsi-global-hbm-case-id", &val);
	if (rc) {
		OPLUS_DSI_INFO("[%s] oplus,dsi-global-hbm-case-id undefined, default to 0\n",
				panel->oplus_panel.vendor_name);
		val = GLOBAL_HBM_CASE_NONE;
	} else if (val >= GLOBAL_HBM_CASE_MAX) {
		OPLUS_DSI_ERR("[%s] oplus,dsi-global-hbm-case-id is invalid:%d\n",
				panel->oplus_panel.vendor_name, val);
		val = GLOBAL_HBM_CASE_NONE;
	}
	panel->oplus_panel.bl_cfg.global_hbm_case_id = val;
	OPLUS_DSI_INFO("[%s] global_hbm_case_id=%d\n",
			panel->oplus_panel.vendor_name,
			panel->oplus_panel.bl_cfg.global_hbm_case_id);

	rc = utils->read_u32(utils->data, "oplus,dsi-global-hbm-threshold", &val);
	if (rc) {
		OPLUS_DSI_INFO("[%s] oplus,dsi-global-hbm-threshold undefined, default to brightness normal max + 1\n",
				panel->oplus_panel.vendor_name);
		panel->oplus_panel.bl_cfg.global_hbm_threshold = panel->oplus_panel.bl_cfg.brightness_normal_max_level + 1;
	} else {
		panel->oplus_panel.bl_cfg.global_hbm_threshold = val;
	}
	OPLUS_DSI_INFO("[%s] global_hbm_threshold=%d\n",
			panel->oplus_panel.vendor_name,
			panel->oplus_panel.bl_cfg.global_hbm_threshold);

	panel->oplus_panel.bl_cfg.global_hbm_scale_mapping = utils->read_bool(utils->data,
			"oplus,dsi-global-hbm-scale-mapping");
	OPLUS_DSI_INFO("oplus,dsi-global-hbm-scale-mapping: %s\n",
			panel->oplus_panel.bl_cfg.global_hbm_scale_mapping ? "true" : "false");

	rc = utils->read_u32(utils->data, "oplus,dsi_bl_limit_max_brightness", &val);

	if (rc) {
		OPLUS_DSI_INFO("[%s] oplus,dsi_bl_limit_max_brightness undefined, default to 4094\n",
			panel->oplus_panel.vendor_name);
		panel->oplus_panel.bl_cfg.oplus_limit_max_bl_mode = false;
	} else {
		panel->oplus_panel.bl_cfg.oplus_limit_max_bl_mode = true;
		panel->oplus_panel.bl_cfg.oplus_limit_max_bl = val;
	}

	panel->oplus_panel.bl_cfg.oplus_demura2_offset_support = utils->read_bool(utils->data,
			"oplus,dsi_demura2_offset_support");
	OPLUS_DSI_INFO("oplus,dsi_demura2_offset_support: %s\n",
			panel->oplus_panel.bl_cfg.oplus_demura2_offset_support ? "true" : "false");

	panel->oplus_panel.bl_cfg.backlight_check_disable = utils->read_bool(utils->data,
			"oplus,panel_backlight_check_disable");
	OPLUS_DSI_INFO("oplus,panel_backlight_check_disable: %s\n",
			panel->oplus_panel.bl_cfg.backlight_check_disable ? "true" : "false");

	return 0;
}

static int oplus_display_panel_dly(struct dsi_panel *panel, bool hbm_switch)
{
	if (hbm_switch) {
		if (enable_hbm_enter_dly_on_flags)
			enable_hbm_enter_dly_on_flags++;
		if (0 == oplus_global_hbm_flags) {
			if (dsi_panel_tx_cmd_set(panel, DSI_CMD_DLY_ON, false)) {
				return 0;
			}
			enable_hbm_enter_dly_on_flags = 1;
		} else if (4 == enable_hbm_enter_dly_on_flags) {
			if (dsi_panel_tx_cmd_set(panel, DSI_CMD_DLY_OFF, false)) {
				return 0;
			}
			enable_hbm_enter_dly_on_flags = 0;
		}
	} else {
		if (oplus_global_hbm_flags == 1) {
			if (dsi_panel_tx_cmd_set(panel, DSI_CMD_DLY_ON, false)) {
				return 0;
			}
			enable_hbm_exit_dly_on_flags = 1;
		} else {
			if (enable_hbm_exit_dly_on_flags)
				enable_hbm_exit_dly_on_flags++;
			if (3 == enable_hbm_exit_dly_on_flags) {
				enable_hbm_exit_dly_on_flags = 0;
				if (dsi_panel_tx_cmd_set(panel, DSI_CMD_DLY_OFF, false)) {
					return 0;
				}
			}
		}
	}
	return 0;
}

int oplus_panel_global_hbm_mapping(struct dsi_panel *panel, u32 *backlight_level)
{
	int rc = 0;
	u32 bl_lvl = *backlight_level;
	u32 global_hbm_switch_cmd = 0;
	bool global_hbm_dly = false;
	struct dsi_cmd_desc *cmds;
	size_t tx_len;
	u8 *tx_buf;
	u32 count;
	int i;

	if (bl_lvl > panel->oplus_panel.bl_cfg.bl_normal_max_level) {
		if (!oplus_global_hbm_flags) {
			global_hbm_switch_cmd = DSI_CMD_HBM_ENTER_SWITCH;
		}
	} else if (oplus_global_hbm_flags) {
		global_hbm_switch_cmd = DSI_CMD_HBM_EXIT_SWITCH;
	}

	switch (panel->oplus_panel.bl_cfg.global_hbm_case_id) {
	case GLOBAL_HBM_CASE_1:
		break;
	case GLOBAL_HBM_CASE_2:
		if (bl_lvl > panel->oplus_panel.bl_cfg.bl_normal_max_level) {
			if (panel->oplus_panel.bl_cfg.global_hbm_scale_mapping) {
				bl_lvl = (bl_lvl - panel->oplus_panel.bl_cfg.bl_normal_max_level) * 100000
						/ (panel->bl_config.bl_max_level - panel->oplus_panel.bl_cfg.bl_normal_max_level)
						* (panel->bl_config.bl_max_level - panel->oplus_panel.bl_cfg.global_hbm_threshold)
						/ 100000 + panel->oplus_panel.bl_cfg.global_hbm_threshold;
			} else if (bl_lvl < panel->oplus_panel.bl_cfg.global_hbm_threshold) {
				bl_lvl = panel->oplus_panel.bl_cfg.global_hbm_threshold;
			}
		}
		break;
	case GLOBAL_HBM_CASE_3:
		if (bl_lvl > panel->oplus_panel.bl_cfg.bl_normal_max_level) {
			bl_lvl = bl_lvl + panel->oplus_panel.bl_cfg.global_hbm_threshold
					- panel->oplus_panel.bl_cfg.bl_normal_max_level - 1;
		}
		break;
	case GLOBAL_HBM_CASE_4:
		global_hbm_switch_cmd = 0;
		if (bl_lvl <= PANEL_MAX_NOMAL_BRIGHTNESS) {
			if (oplus_global_hbm_flags) {
				global_hbm_switch_cmd = DSI_CMD_HBM_EXIT_SWITCH;
			}
			bl_lvl = backlight_buf[bl_lvl];
		} else if (bl_lvl > HBM_BASE_600NIT) {
			if (!oplus_global_hbm_flags) {
				global_hbm_switch_cmd = DSI_CMD_HBM_ENTER_SWITCH;
			}
			global_hbm_dly = true;
			bl_lvl = backlight_600_800nit_buf[bl_lvl - HBM_BASE_600NIT];
		} else if (bl_lvl > PANEL_MAX_NOMAL_BRIGHTNESS) {
			if (oplus_global_hbm_flags) {
				global_hbm_switch_cmd = DSI_CMD_HBM_EXIT_SWITCH;
			}
			bl_lvl = backlight_500_600nit_buf[bl_lvl - PANEL_MAX_NOMAL_BRIGHTNESS];
		}
		break;
	default:
		global_hbm_switch_cmd = 0;
		break;
	}

	bl_lvl = bl_lvl < panel->bl_config.bl_max_level ? bl_lvl :
			panel->bl_config.bl_max_level;

	if (global_hbm_switch_cmd > 0) {
		/* Update the 0x51 value when sending hbm enter/exit command */
		cmds = panel->cur_mode->priv_info->cmd_sets[global_hbm_switch_cmd].cmds;
		count = panel->cur_mode->priv_info->cmd_sets[global_hbm_switch_cmd].count;
		for (i = 0; i < count; i++) {
			tx_len = cmds[i].msg.tx_len;
			tx_buf = (u8 *)cmds[i].msg.tx_buf;
			if ((3 == tx_len) && (0x51 == tx_buf[0])) {
				tx_buf[1] = (bl_lvl >> 8) & 0xFF;
				tx_buf[2] = bl_lvl & 0xFF;
				break;
			}
		}

		if (global_hbm_dly) {
			oplus_display_panel_dly(panel, true);
		}

		rc = dsi_panel_tx_cmd_set(panel, global_hbm_switch_cmd, false);
		oplus_global_hbm_flags = (global_hbm_switch_cmd == DSI_CMD_HBM_ENTER_SWITCH);
	}

	*backlight_level = bl_lvl;
	return 0;
}

int oplus_display_panel_get_global_hbm_status(void)
{
	return oplus_global_hbm_flags;
}

void oplus_display_panel_set_global_hbm_status(int global_hbm_status)
{
	oplus_global_hbm_flags = global_hbm_status;
	OPLUS_DSI_INFO("set oplus_global_hbm_flags = %d\n", global_hbm_status);
}


void oplus_panel_set_lhbm_off_te_timestamp(struct dsi_panel *panel) {
	lhbm_off_te_timestamp = panel->oplus_panel.te_timestamp;
}

void oplus_panel_backlight_demura_dbv_switch(struct dsi_panel *panel, u32 bl_lvl)
{
	int rc = 0;
	u32 bl_demura_last_mode = panel->oplus_panel.bl_demura_mode;
	u32 bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE0;
	char *tx_buf;
	struct dsi_panel_cmd_set custom_cmd_set;

	if (!panel->oplus_panel.oplus_bl_demura_dbv_support)
		return;

	if (bl_lvl == 0 || bl_lvl == 1)
		return;

	if (!strcmp(panel->name, "AA567 P 3 A0004 dsc cmd mode panel")) {
		if ((bl_lvl >= 8) && (bl_lvl <= 1087)) {
			panel->oplus_panel.bl_demura_mode = 0;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE0;
		} else if ((bl_lvl >= 1088) && (bl_lvl < 1768)) {
			panel->oplus_panel.bl_demura_mode = 1;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE1;
		} else if ((bl_lvl >= 1768) && (bl_lvl < 2689)) {
			panel->oplus_panel.bl_demura_mode = 2;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE2;
		} else if (2689 <= bl_lvl) {
			panel->oplus_panel.bl_demura_mode = 3;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE3;
		}
	} else if (!strcmp(panel->name, "P 7 AB715 dsc cmd mode panel")) {
		if (1 <= bl_lvl && bl_lvl < 652) {
			panel->oplus_panel.bl_demura_mode = 0;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE0;
		} else if (652 <= bl_lvl && bl_lvl < 1088) {
			panel->oplus_panel.bl_demura_mode = 1;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE1;
		} else if (1088 <= bl_lvl) {
			panel->oplus_panel.bl_demura_mode = 2;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE2;
		}
	} else {
		if (bl_lvl <= 3515)
			return;

		if (PANEL_LOADING_EFFECT_MODE2 != __oplus_get_seed_mode())
			return;

		if ((bl_lvl > 3515) && (bl_lvl <= 3543)) {
			panel->oplus_panel.bl_demura_mode = 0;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE0;
		} else if ((bl_lvl > 3543) && (bl_lvl <= 3600)) {
			panel->oplus_panel.bl_demura_mode = 1;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE1;
		} else if ((bl_lvl > 3600) && (bl_lvl <= 3680)) {
			panel->oplus_panel.bl_demura_mode = 2;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE2;
		} else if ((bl_lvl > 3680) && (bl_lvl <= 3720)) {
			panel->oplus_panel.bl_demura_mode = 3;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE3;
		} else if ((bl_lvl > 3720) && (bl_lvl <= 3770)) {
			panel->oplus_panel.bl_demura_mode = 4;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE4;
		} else if ((bl_lvl > 3770) && (bl_lvl <= 3860)) {
			panel->oplus_panel.bl_demura_mode = 5;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE5;
		} else if ((bl_lvl > 3860) && (bl_lvl <= 3949)) {
			panel->oplus_panel.bl_demura_mode = 6;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE6;
		}  else if ((bl_lvl > 3949) && (bl_lvl <= 4094)) {
			panel->oplus_panel.bl_demura_mode = 7;
			bl_demura_mode = DSI_CMD_DEMURA_DBV_MODE7;
		}

		custom_cmd_set = panel->cur_mode->priv_info->cmd_sets[bl_demura_mode];
		tx_buf = (char*)custom_cmd_set.cmds[custom_cmd_set.count - 1].msg.tx_buf;

		if (tx_buf[0] == 0x51) {
			tx_buf[1] = (bl_lvl >> 8);
			tx_buf[2] = (bl_lvl & 0xFF);
		} else {
			OPLUS_DSI_INFO("invaild format of cmd %s\n", cmd_set_prop_map[bl_demura_mode]);
		}
	}

	if (panel->oplus_panel.bl_demura_mode != bl_demura_last_mode && panel->power_mode == SDE_MODE_DPMS_ON)
		rc = dsi_panel_tx_cmd_set(panel, bl_demura_mode, false);
	if (rc) {
		DSI_ERR("[%s] failed to send bl_demura_mode, rc=%d\n", panel->name, rc);
		return;
	}
}

int oplus_panel_need_to_set_demura2_offset(struct dsi_panel *panel)
{
	if (!panel) {
		OPLUS_DSI_ERR("Invalid panel params\n");
		return -EINVAL;
	}

	if (panel->oplus_panel.bl_cfg.oplus_demura2_offset_support) {
		panel->oplus_panel.bl_cfg.need_to_set_demura2_offset = true;
	}

	return 0;
}

int oplus_display_panel_set_demura2_offset(struct dsi_display *display)
{
	u32 bl_lvl = 0;
	int rc = 0;
	struct dsi_panel *panel = NULL;
	static bool last_hbm_status = false;
	bool current_hbm_status = false;
	u32 last_demura2_offset = 0;
	u32 cmd_set_demura2_offset = DSI_CMD_SET_DEMURA2_OFFSET0;

	panel = display->panel;
	if (!panel) {
		DSI_ERR("failed to set demura2 offset, Invalid params\n");
		return -EINVAL;
	}

	if (!panel->oplus_panel.bl_cfg.oplus_demura2_offset_support) {
		DSI_DEBUG("[%s] don't support demura2 offset\n", panel->name);
		return rc;
	}

	last_demura2_offset = panel->oplus_panel.bl_cfg.demura2_offset;
	bl_lvl = panel->bl_config.bl_level;
	current_hbm_status = oplus_ofp_get_hbm_state();

	if (current_hbm_status) {
		/* Enter HBM, bl_lvl set as backlight for entering HBM */
		bl_lvl = 0x0F00;
	} else if (last_hbm_status != current_hbm_status) {
		/* Exit HBM, force update demura2 offset */
		panel->oplus_panel.bl_cfg.need_to_set_demura2_offset = true;
	}

	if ((bl_lvl >= 0x01) && (bl_lvl < 0x47B)) {
		panel->oplus_panel.bl_cfg.demura2_offset = 0;
		cmd_set_demura2_offset = DSI_CMD_SET_DEMURA2_OFFSET0;
	} else if ((bl_lvl >= 0x47B) && (bl_lvl <= 0xFFE)) {
		panel->oplus_panel.bl_cfg.demura2_offset = 1;
		cmd_set_demura2_offset = DSI_CMD_SET_DEMURA2_OFFSET1;
	} else {
		return rc;
	}

	if ((panel->oplus_panel.bl_cfg.demura2_offset != last_demura2_offset
			|| panel->oplus_panel.bl_cfg.need_to_set_demura2_offset)
			&& panel->power_mode == SDE_MODE_DPMS_ON) {
		mutex_lock(&display->display_lock);
		mutex_lock(&panel->panel_lock);

		rc |= dsi_display_override_dma_cmd_trig(display, DSI_TRIGGER_SW_SEOF);
		rc |= dsi_panel_tx_cmd_set(panel, cmd_set_demura2_offset, false);
		rc |= dsi_display_override_dma_cmd_trig(display, DSI_TRIGGER_NONE);

		mutex_unlock(&panel->panel_lock);
		mutex_unlock(&display->display_lock);

		if (rc) {
			DSI_ERR("[%s] failed to send demura2 offset cmd, rc=%d\n", panel->name, rc);
			return rc;
		}
		panel->oplus_panel.bl_cfg.need_to_set_demura2_offset = false;
	}

	last_hbm_status = current_hbm_status;

	return rc;
}

static int get_current_vsync_period(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dsi_display *dsi_display = NULL;

	dsi_display = c_conn->display;

	if (!dsi_display || !dsi_display->panel || !dsi_display->panel->cur_mode
		|| !dsi_display->panel->cur_mode->priv_info) {
		SDE_ERROR("Invalid params(s) dsi_display %pK, panel %pK\n",
			dsi_display,
			((dsi_display) ? dsi_display->panel : NULL));
		return -EINVAL;
	}

	return dsi_display->panel->cur_mode->priv_info->oplus_priv_info.vsync_period;
}

static int get_current_vsync_width(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dsi_display *dsi_display = NULL;

	dsi_display = c_conn->display;

	if (!dsi_display || !dsi_display->panel || !dsi_display->panel->cur_mode
		|| !dsi_display->panel->cur_mode->priv_info) {
		SDE_ERROR("Invalid params(s) dsi_display %pK, panel %pK\n",
			dsi_display,
			((dsi_display) ? dsi_display->panel : NULL));
		return -EINVAL;
	}

	return dsi_display->panel->cur_mode->priv_info->oplus_priv_info.vsync_width;
}

static int get_current_refresh_rate(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dsi_display *dsi_display = NULL;

	dsi_display = c_conn->display;

	if (!dsi_display || !dsi_display->panel || !dsi_display->panel->cur_mode
		|| !dsi_display->panel->cur_mode->priv_info) {
		SDE_ERROR("Invalid params(s) dsi_display %pK, panel %pK\n",
			dsi_display,
			((dsi_display) ? dsi_display->panel : NULL));
		return -EINVAL;
	}

	return dsi_display->panel->cur_mode->priv_info->oplus_priv_info.refresh_rate;
}

static int get_current_display_brightness(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dsi_display *dsi_display = NULL;
	int brightness_level = 0;

	dsi_display = c_conn->display;

	if (!dsi_display || !dsi_display->panel || !dsi_display->panel->cur_mode) {
		SDE_ERROR("Invalid params(s) dsi_display %pK, panel %pK\n",
			dsi_display,
			((dsi_display) ? dsi_display->panel : NULL));
		return -EINVAL;
	}

	brightness_level = dsi_display->panel->bl_config.bl_level;

	return brightness_level;
}

bool is_support_apollo_bk(struct drm_connector *connector)
{
	struct sde_connector *c_conn = to_sde_connector(connector);
	struct dsi_display *dsi_display = NULL;

	if(c_conn->connector_type == DRM_MODE_CONNECTOR_DSI) {
		dsi_display = c_conn->display;

		if (!dsi_display || !dsi_display->panel || !dsi_display->panel->oplus_panel.vendor_name) {
			SDE_ERROR("Invalid params(s) dsi_display %pK, panel %pK\n",
				dsi_display,
				((dsi_display) ? dsi_display->panel : NULL));
			return -EINVAL;
		}

		if (dsi_display->panel->oplus_panel.is_apollo_support) {
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}

bool is_spread_backlight(struct dsi_display *display, int level)
{
	if ((display != NULL) && (display->panel != NULL)
		&& (level <= display->panel->oplus_panel.sync_brightness_level) && (level >= 2)) {
		return true;
	}
	else if (((display != NULL) && (display->panel != NULL)
		&& (display->panel->oplus_panel.dc_apollo_sync_enable)
		&& (((level <= display->panel->oplus_panel.sync_brightness_level) && (level >= 2))
		|| (level == display->panel->oplus_panel.dc_apollo_sync_brightness_level)))) {
		return true;
	}
	else {
		return false;
	}
}

void update_pending_backlight(struct dsi_display *display, int level)
{
	if (display == NULL) {
		return;
	}

	if (!strcmp(display->display_type, "primary")) {
		apollo_bk.g_pri_bk_level = level;
	} else if (!strcmp(display->display_type, "secondary")) {
		apollo_bk.g_sec_bk_level = level;
	}

	return;
}

int oplus_backlight_wait_vsync(struct drm_encoder *drm_enc)
{
	SDE_ATRACE_BEGIN("wait_vsync");

	if (!drm_enc || !drm_enc->crtc) {
		SDE_ERROR("%s encoder is disabled", __func__);
		return -ENOLINK;
	}

	if (sde_encoder_is_disabled(drm_enc)) {
		SDE_ERROR("%s encoder is disabled", __func__);
		return -EIO;
	}

	sde_encoder_wait_for_event(drm_enc,  MSM_ENC_VBLANK);

	SDE_ATRACE_END("wait_vsync");

	return 0;
}

static int oplus_setbacklight_by_display_type(struct drm_encoder *drm_enc) {
	struct sde_encoder_virt *sde_enc = NULL;
	struct dsi_display *display = NULL;
	struct sde_connector *c_conn = NULL;
	int rc = 0;
	char tag_name[64];

	sde_enc = to_sde_encoder_virt(drm_enc);
	if (sde_enc == NULL)
		return -EFAULT;

	c_conn = to_sde_connector(sde_enc->phys_encs[0]->connector);
	if (c_conn == NULL)
		return -EFAULT;

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		return 0;

	display = c_conn->display;
	if (display == NULL)
		return -EFAULT;

	if (!strcmp(display->display_type, "primary")) {
		snprintf(tag_name, sizeof(tag_name), "primary: %d", apollo_bk.g_pri_bk_level);
		SDE_ATRACE_BEGIN(tag_name);
		rc = c_conn->ops.set_backlight(&c_conn->base, display, apollo_bk.g_pri_bk_level);
		SDE_ATRACE_END(tag_name);
	} else if (!strcmp(display->display_type, "secondary")) {
		snprintf(tag_name, sizeof(tag_name), "secondary: %d", apollo_bk.g_sec_bk_level);
		SDE_ATRACE_BEGIN(tag_name);
		rc = c_conn->ops.set_backlight(&c_conn->base, display, apollo_bk.g_sec_bk_level);
		SDE_ATRACE_END(tag_name);
	}

	return rc;
}

int oplus_sync_panel_brightness(enum oplus_sync_method method, struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct sde_encoder_phys *phys_encoder = NULL;
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display = NULL;
	int rc = 0;
	struct sde_encoder_phys_cmd *cmd_enc = NULL;
	struct sde_encoder_phys_cmd_te_timestamp *te_timestamp;
	s64 us_per_frame;
	u32 vsync_width;
	ktime_t last_te_timestamp;
	s64 delay;

	sde_enc = to_sde_encoder_virt(drm_enc);
	phys_encoder = sde_enc->phys_encs[0];

	if (phys_encoder == NULL)
		return -EFAULT;
	if (phys_encoder->connector == NULL)
		return -EFAULT;

	c_conn = to_sde_connector(phys_encoder->connector);
	if (c_conn == NULL)
		return -EFAULT;

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		return 0;

	display = c_conn->display;
	if (display == NULL)
		return -EFAULT;

	cmd_enc = to_sde_encoder_phys_cmd(phys_encoder);
	if (cmd_enc == NULL) {
		return -EFAULT;
	}

	us_per_frame = get_current_vsync_period(sde_enc->cur_master->connector);
	vsync_width = get_current_vsync_width(sde_enc->cur_master->connector);
	te_timestamp = list_last_entry(&cmd_enc->te_timestamp_list, struct sde_encoder_phys_cmd_te_timestamp, list);
	last_te_timestamp = te_timestamp->timestamp;

	if ((!strcmp(display->display_type, "primary") &&
			is_spread_backlight(display, apollo_bk.g_pri_bk_level) &&
			(apollo_bk.g_pri_bk_level != get_current_display_brightness(sde_enc->cur_master->connector))) ||
		(!strcmp(display->display_type, "secondary") &&
			is_spread_backlight(display, apollo_bk.g_sec_bk_level) &&
			(apollo_bk.g_sec_bk_level != get_current_display_brightness(sde_enc->cur_master->connector)))) {
		SDE_ATRACE_BEGIN("sync_panel_brightness");
		delay = vsync_width - (ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp)) % us_per_frame);
		if (delay > 0) {
			SDE_EVT32(us_per_frame, last_te_timestamp, delay);
			usleep_range(delay, delay + 100);
		}

		if ((ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp)) % us_per_frame) > (us_per_frame - DEBOUNCE_TIME)) {
			SDE_EVT32(us_per_frame, last_te_timestamp);
			usleep_range(DEBOUNCE_TIME + vsync_width, DEBOUNCE_TIME + 100 + vsync_width);
		}
		if (method == OPLUS_PREPARE_KICKOFF_METHOD) {
			rc = oplus_setbacklight_by_display_type(drm_enc);
			c_conn->unset_bl_level = 0;
		} else if (method == OPLUS_POST_KICKOFF_METHOD) {
			rc = oplus_setbacklight_by_display_type(drm_enc);
			c_conn->unset_bl_level = 0;
		} else {
			oplus_backlight_wait_vsync(c_conn->encoder);
			rc = oplus_setbacklight_by_display_type(drm_enc);
			c_conn->unset_bl_level = 0;
		}
		SDE_ATRACE_END("sync_panel_brightness");
	}

	return rc;
}

/*
	Use oplus_set_brightness instead of
	backlight_device_set_brightness to
	avoid time consumption of backlight_generate_event()
*/
int oplus_set_brightness(struct backlight_device *bd,
				    unsigned long brightness)
{
	int rc = -ENXIO;

	mutex_lock(&bd->ops_lock);
	if (bd->ops) {
		if (brightness > bd->props.max_brightness)
			rc = -EINVAL;
		else {
			pr_debug("set brightness to %lu\n", brightness);
			bd->props.brightness = brightness;
			rc = backlight_update_status(bd);
		}
	}
	mutex_unlock(&bd->ops_lock);

	SDE_ATRACE_BEGIN("brightness_notify");
	sysfs_notify(&bd->dev.kobj, NULL, "brightness");
	SDE_ATRACE_END("brightness_notify");

	return rc;
}

int oplus_sync_panel_brightness_v2(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	struct sde_encoder_phys *phys_encoder = NULL;
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display = NULL;
	struct sde_connector_state *c_state;
	int rc = 0;
	struct sde_encoder_phys_cmd *cmd_enc = NULL;
	struct sde_encoder_phys_cmd_te_timestamp *te_timestamp;
	s64 us_per_frame;
	u32 vsync_width;
	u32 refresh_rate;
	ktime_t last_te_timestamp;
	s64 delay;
	bool sync_backlight;
	u32 brightness;
	char tag_name[64];
	char vsync_width_name[64];

	sde_enc = to_sde_encoder_virt(drm_enc);
	phys_encoder = sde_enc->phys_encs[0];

	if (phys_encoder == NULL)
		return -EFAULT;
	if (phys_encoder->connector == NULL)
		return -EFAULT;

	c_conn = to_sde_connector(phys_encoder->connector);
	if (c_conn == NULL)
		return -EFAULT;

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		return 0;

	c_state = to_sde_connector_state(c_conn->base.state);

	display = c_conn->display;
	if (display == NULL)
		return -EFAULT;

	cmd_enc = to_sde_encoder_phys_cmd(phys_encoder);
	if (cmd_enc == NULL) {
		return -EFAULT;
	}

	us_per_frame = get_current_vsync_period(sde_enc->cur_master->connector);
	vsync_width = get_current_vsync_width(sde_enc->cur_master->connector);
	refresh_rate = get_current_refresh_rate(sde_enc->cur_master->connector);

	te_timestamp = list_last_entry(&cmd_enc->te_timestamp_list, struct sde_encoder_phys_cmd_te_timestamp, list);
	if (display->panel->oplus_panel.last_us_per_frame == 0 || display->panel->oplus_panel.last_vsync_width == 0) {
		display->panel->oplus_panel.last_us_per_frame = us_per_frame;
		display->panel->oplus_panel.last_vsync_width = vsync_width;
	}

	if (display->panel->oplus_panel.last_refresh_rate != refresh_rate) {
		if ((display->panel->oplus_panel.last_refresh_rate == 120 && refresh_rate == 90) ||
			(display->panel->oplus_panel.last_refresh_rate == 90 && refresh_rate == 120)) {
			display->panel->oplus_panel.work_frame = 1;	/* two frame work */
		} else {
			display->panel->oplus_panel.work_frame = 0;	/* one frame work */
		}
	}

	if (te_timestamp == NULL) {
		return rc;
	}

	last_te_timestamp = te_timestamp->timestamp;
	sync_backlight = c_conn->oplus_conn.bl_need_sync;
	display->panel->oplus_panel.need_sync = sync_backlight;
	c_conn->oplus_conn.bl_need_sync = false;

	/* in debounce time, update last_vsync_width from next frame */
	if (((last_te_timestamp > display->panel->oplus_panel.ts_timestamp && display->panel->oplus_panel.work_frame == 1)
			|| display->panel->oplus_panel.work_frame == 0) && (ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp))
			% display->panel->oplus_panel.last_us_per_frame) > (display->panel->oplus_panel.last_us_per_frame - DEBOUNCE_TIME)) {
		display->panel->oplus_panel.last_vsync_width = vsync_width;
	}
	/* updates the last_us_per_frame & last_vsync_width after timing switch */
	if ((last_te_timestamp - display->panel->oplus_panel.ts_timestamp) >=
			display->panel->oplus_panel.work_frame * display->panel->oplus_panel.last_us_per_frame * 1000) {
		display->panel->oplus_panel.last_us_per_frame = us_per_frame;
		display->panel->oplus_panel.last_vsync_width = vsync_width;
		display->panel->oplus_panel.last_refresh_rate = refresh_rate;
	}

	if (sync_backlight) {
		snprintf(vsync_width_name, sizeof(vsync_width_name), "%d:%d", display->panel->oplus_panel.last_vsync_width,
				display->panel->oplus_panel.last_us_per_frame);
		SDE_ATRACE_BEGIN("sync_panel_brightness");
		SDE_ATRACE_BEGIN(vsync_width_name);
		brightness = sde_connector_get_property(c_conn->base.state, CONNECTOR_PROP_SYNC_BACKLIGHT_LEVEL);
		delay = display->panel->oplus_panel.last_vsync_width - (ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp))
				% display->panel->oplus_panel.last_us_per_frame);
		if (delay > 0) {
			SDE_EVT32(display->panel->oplus_panel.last_us_per_frame, last_te_timestamp, delay);
			usleep_range(delay, delay + 100);
		}
		if ((ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp)) % display->panel->oplus_panel.last_us_per_frame)
				> (display->panel->oplus_panel.last_us_per_frame - DEBOUNCE_TIME)) {
			SDE_EVT32(display->panel->oplus_panel.last_us_per_frame, last_te_timestamp);
			usleep_range(DEBOUNCE_TIME + display->panel->oplus_panel.last_vsync_width,
					DEBOUNCE_TIME + 100 + display->panel->oplus_panel.last_vsync_width);
		}
		snprintf(tag_name, sizeof(tag_name), "%s: %d", display->display_type, brightness);
		SDE_ATRACE_BEGIN(tag_name);
		rc = oplus_set_brightness(c_conn->bl_device, brightness);
		SDE_ATRACE_END(tag_name);
		SDE_ATRACE_END(vsync_width_name);
		SDE_ATRACE_END("sync_panel_brightness");
	}

	return rc;
}


int dc_apollo_sync_hbmon(struct dsi_display *display)
{
	if (display == NULL)
		return 0;
	if (display->panel == NULL)
		return 0;

	if (display->panel->oplus_panel.dc_apollo_sync_enable) {
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
		if (oplus_ofp_is_supported() && !oplus_ofp_oled_capacitive_is_enabled()
				&& !oplus_ofp_ultrasonic_is_enabled()) {
			if (oplus_ofp_get_hbm_state()) {
				return 1;
			}
		}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

		return 0;
	} else {
		return 0;
	}
}

int oplus_panel_mult_frac(int bright)
{
	int bl_lvl = 0;
	struct dsi_display *display = get_main_display();

	if (!display || !display->drm_conn || !display->drm_conn->state) {
		OPLUS_DSI_ERR("failed to find dsi display\n");
		return false;
	}

	if (oplus_debug_max_brightness) {
		bl_lvl = mult_frac(bright, oplus_debug_max_brightness,
			display->panel->bl_config.brightness_max_level);
	} else if (bright == 0) {
			bl_lvl = 0;
	} else {
		if (display->panel->oplus_panel.bl_remap && display->panel->oplus_panel.bl_remap_count) {
			int i = 0;
			int count = display->panel->oplus_panel.bl_remap_count;
			struct oplus_brightness_alpha *lut = display->panel->oplus_panel.bl_remap;

			for (i = 0; i < display->panel->oplus_panel.bl_remap_count; i++) {
				if (display->panel->oplus_panel.bl_remap[i].brightness >= bright)
					break;
			}

			if (i == 0)
				bl_lvl = lut[0].alpha;
			else if (i == count)
				bl_lvl = lut[count - 1].alpha;
			else
				bl_lvl = oplus_interpolate(bright, lut[i-1].brightness,
						lut[i].brightness, lut[i-1].alpha, lut[i].alpha);
		} else if (bright > display->panel->oplus_panel.bl_cfg.brightness_normal_max_level) {
			bl_lvl = oplus_interpolate(bright,
					display->panel->oplus_panel.bl_cfg.brightness_normal_max_level,
					display->panel->bl_config.brightness_max_level,
					display->panel->oplus_panel.bl_cfg.bl_normal_max_level,
					display->panel->bl_config.bl_max_level);
		} else {
			bl_lvl = mult_frac(bright, display->panel->oplus_panel.bl_cfg.bl_normal_max_level,
					display->panel->oplus_panel.bl_cfg.brightness_normal_max_level);
		}
	}

	return bl_lvl;
}

int oplus_panel_post_on_backlight(void *display, struct dsi_panel *panel, u32 bl_lvl)
{
	struct dsi_display *dsi_display = display;
	int rc = 0;

	if (!panel || !dsi_display) {
		OPLUS_DSI_ERR("oplus post backlight No panel device\n");
		return -ENODEV;
	}

	OPLUS_DSI_DEBUG("[%s] display backlight changed: %d -> %d\n",
			panel->oplus_panel.vendor_name, panel->bl_config.bl_level, bl_lvl);

	/* Add some delay to avoid screen flash */
	if (panel->oplus_panel.need_power_on_backlight && bl_lvl) {
		panel->oplus_panel.need_power_on_backlight = false;
		rc = dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
		rc |= dsi_panel_tx_cmd_set(panel, DSI_CMD_POST_ON_BACKLIGHT, false);
		rc |= dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
		if (rc) {
			OPLUS_DSI_ERR("[%s] failed to send %s, rc=%d\n",
				panel->oplus_panel.vendor_name,
				cmd_set_prop_map[DSI_CMD_POST_ON_BACKLIGHT],
				rc);
		}

		atomic_set(&panel->oplus_panel.esd_pending, 0);
	}
	return 0;
}

u32 oplus_panel_silence_backlight(struct dsi_panel *panel, u32 bl_lvl)
{
	u32 bl_temp = 0;
	if (!panel) {
		OPLUS_DSI_ERR("Oplus Features config No panel device\n");
		return -ENODEV;
	}

	bl_temp = bl_lvl;

	if (lcd_closebl_flag) {
		OPLUS_DSI_INFO("silence reboot we should set backlight to zero\n");
		bl_temp = 0;
	}
	return bl_temp;
}

static int oplus_panel_change_voltage_before_panel_bl_0(struct dsi_panel *panel, u32 oplus_current_backlight, u32 oplus_last_backlight) {
	int rc = 0;
	if (oplus_current_backlight == 0 && oplus_last_backlight > 0) {
		if (panel->oplus_panel.change_voltage_before_panel_bl_0) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_CHANGE_VOLTAGE_BEFORE_BL_0, false);
			oplus_sde_early_wakeup(panel);
			oplus_wait_for_vsync(panel);
			if (rc) {
				OPLUS_DSI_ERR("failed to oplus_panel_change_voltage_before_panel_bl_0\n");
			}
		}
	}
	return 0;
}

void oplus_panel_update_backlight(struct dsi_panel *panel,
		struct mipi_dsi_device *dsi, u32 bl_lvl)
{
	int rc = 0;
	u64 inverted_dbv_bl_lvl = 0;

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	if (oplus_adfr_osync_backlight_filter(panel, bl_lvl)) {
		return;
	}
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	if (oplus_ofp_is_supported()) {
		oplus_ofp_lhbm_backlight_update(NULL, panel, &bl_lvl);
		if (oplus_ofp_backlight_filter(panel, bl_lvl)) {
			return;
		}
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#ifdef OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION
	if (oplus_temp_compensation_is_supported()) {
		oplus_temp_compensation_cmd_set(panel, OPLUS_TEMP_COMPENSATION_BACKLIGHT_SETTING);
	}
#endif /* OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION */

#ifdef OPLUS_FEATURE_DISPLAY
	if (panel->oplus_panel.bl_cfg.oplus_limit_max_bl_mode) {
		if (bl_lvl > panel->oplus_panel.bl_cfg.oplus_limit_max_bl)
			bl_lvl = panel->oplus_panel.bl_cfg.oplus_limit_max_bl;
	}

	oplus_panel_change_voltage_before_panel_bl_0(panel, bl_lvl, oplus_last_backlight);
#endif

	oplus_temp_compensation_wait_for_vsync_set = false;

	/* backlight value mapping */
	oplus_panel_global_hbm_mapping(panel, &bl_lvl);

	/* pwm switch due to backlight change*/
	oplus_panel_pwm_dbv_threshold_handle(panel, bl_lvl);

	oplus_panel_backlight_demura_dbv_switch(panel, bl_lvl);

	if (!panel->oplus_panel.need_sync && panel->cur_mode->priv_info->oplus_priv_info.async_bl_delay) {
		if (panel->oplus_panel.disable_delay_bl_count > 0) {
			panel->oplus_panel.disable_delay_bl_count--;
		} else if (panel->oplus_panel.disable_delay_bl_count == 0) {
			oplus_apollo_async_bl_delay(panel);
		} else {
			DSI_INFO("invalid disable_delay_bl_count\n");
			panel->oplus_panel.disable_delay_bl_count = 0;
		}
	}
	/* will inverted display brightness value */
	if (panel->bl_config.bl_inverted_dbv)
		inverted_dbv_bl_lvl = (((bl_lvl & 0xff) << 8) | (bl_lvl >> 8));
	else
		inverted_dbv_bl_lvl = bl_lvl;

	mutex_lock(&panel->oplus_panel.panel_tx_lock);
	rc = mipi_dsi_dcs_set_display_brightness(dsi, inverted_dbv_bl_lvl);
	mutex_unlock(&panel->oplus_panel.panel_tx_lock);
	if (rc < 0)
		OPLUS_DSI_ERR("failed to update dcs backlight:%d\n", bl_lvl);

#ifdef OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION
	if (oplus_temp_compensation_is_supported()) {
		oplus_temp_compensation_first_half_frame_cmd_set(panel);
	}
#endif /* OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION */

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	if (oplus_ofp_is_supported()) {
		oplus_ofp_lhbm_dbv_vdc_update(panel, bl_lvl, false);
		oplus_ofp_lhbm_dbv_alpha_update(panel, bl_lvl, false);
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

	OPLUS_DSI_DEBUG("[%s] panel backlight changed: %d -> %d\n",
			panel->oplus_panel.vendor_name, oplus_last_backlight, bl_lvl);

	oplus_last_backlight = bl_lvl;
}

void oplus_printf_backlight_8868_log(struct dsi_display *display, u32 bl_lvl) {
	struct timespec64 now;
	struct tm broken_time;
	static time64_t time_last = 0;
	struct backlight_8868_log *map_bl_log;
	u32 mapping_value = 0;
	int i = 0;
	int len = 0;
	char map_backlight_log_buf[1548];

	if (bl_lvl > KTZ8868_IC_BL_LEVEL_MAX) {
		mapping_value = backlight_map[KTZ8868_IC_BL_LEVEL_MAX];
	} else {
		mapping_value = backlight_map[bl_lvl];
	}

	ktime_get_real_ts64(&now);
	time64_to_tm(now.tv_sec, 0, &broken_time);
	if (now.tv_sec - time_last >= 60) {
		OPLUS_DSI_INFO("<%s> dsi_display_set_backlight time:%02d:%02d:%02d.%03ld,bl_lvl:%d, mapping_value ;%d\n",
			display->panel->oplus_panel.vendor_name, broken_time.tm_hour, broken_time.tm_min,
			broken_time.tm_sec, now.tv_nsec / 1000000, bl_lvl, mapping_value);
		time_last = now.tv_sec;
	}

	map_bl_log = &oplus_bl_8868_log[DISPLAY_PRIMARY];
	map_bl_log->backlight[map_bl_log->bl_count] = bl_lvl;
	map_bl_log->Map_backlight[map_bl_log->bl_count] = mapping_value;
	map_bl_log->past_times[map_bl_log->bl_count] = now;
	map_bl_log->bl_count++;
	if (map_bl_log->bl_count >= BACKLIGHT_CACHE_MAX) {
		map_bl_log->bl_count = 0;
		memset(map_backlight_log_buf, 0, sizeof(map_backlight_log_buf));
		for (i = 0; i < BACKLIGHT_CACHE_MAX; i++) {
			time64_to_tm(map_bl_log->past_times[i].tv_sec, 0, &broken_time);
			len += snprintf(map_backlight_log_buf + len, sizeof(map_backlight_log_buf) - len,
				"%02d:%02d:%02d.%03ld:Map:%d,Bl:%d,", broken_time.tm_hour, broken_time.tm_min,
				broken_time.tm_sec, map_bl_log->past_times[i].tv_nsec / 1000000, map_bl_log->Map_backlight[i], map_bl_log->backlight[i]);
		}
		OPLUS_DSI_INFO("<%s> len:%d dsi_display_set_backlight_8868 %s\n", display->panel->oplus_panel.vendor_name, len, map_backlight_log_buf);
	}
}

void oplus_printf_backlight_log(struct dsi_display *display, u32 bl_lvl) {
	struct timespec64 now;
	struct tm broken_time;
	static time64_t time_last = 0;
	struct backlight_log *bl_log;
	int i = 0;
	int len = 0;
	char backlight_log_buf[1024];

	ktime_get_real_ts64(&now);
	time64_to_tm(now.tv_sec, 0, &broken_time);
	if (now.tv_sec - time_last >= 60) {
		OPLUS_DSI_INFO("<%s> dsi_display_set_backlight time:%02d:%02d:%02d.%03ld,bl_lvl:%d\n",
			display->panel->oplus_panel.vendor_name, broken_time.tm_hour, broken_time.tm_min,
			broken_time.tm_sec, now.tv_nsec / 1000000, bl_lvl);
		time_last = now.tv_sec;
	}

	if (!strcmp(display->display_type, "secondary")) {
		bl_log = &oplus_bl_log[DISPLAY_SECONDARY];
	} else {
		bl_log = &oplus_bl_log[DISPLAY_PRIMARY];
	}

	bl_log->backlight[bl_log->bl_count] = bl_lvl;
	bl_log->past_times[bl_log->bl_count] = now;
	bl_log->bl_count++;
	if (bl_log->bl_count >= BACKLIGHT_CACHE_MAX) {
		bl_log->bl_count = 0;
		memset(backlight_log_buf, 0, sizeof(backlight_log_buf));
		for (i = 0; i < BACKLIGHT_CACHE_MAX; i++) {
			time64_to_tm(bl_log->past_times[i].tv_sec, 0, &broken_time);
			len += snprintf(backlight_log_buf + len, sizeof(backlight_log_buf) - len,
				"%02d:%02d:%02d.%03ld:%d,", broken_time.tm_hour, broken_time.tm_min,
				broken_time.tm_sec, bl_log->past_times[i].tv_nsec / 1000000, bl_log->backlight[i]);
		}
		OPLUS_DSI_INFO("<%s> len:%d dsi_display_set_backlight %s\n", display->panel->oplus_panel.vendor_name, len, backlight_log_buf);
	}
}

int oplus_panel_backlight_check(struct dsi_panel *panel)
{
	int rc = 0;
	u32 bl_lvl = 0;
	u32 bl_52_lvl = 0;
	struct dsi_display_ctrl *m_ctrl = NULL;
	struct dsi_display *display = NULL;
	unsigned char read[30];

	if (panel->oplus_panel.bl_cfg.backlight_check_disable) {
		OPLUS_DSI_DEBUG("panel backlight check disable\n");
		return rc;
	}

	display = to_dsi_display(panel->host);

	if (!display) {
		OPLUS_DSI_ERR("display is NULL\n");
		return -EINVAL;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	rc = dsi_panel_read_panel_reg_unlock(m_ctrl, panel, 0x52, read, 2);
	if (rc < 0) {
		OPLUS_DSI_ERR("failed to read 52 ret=%d\n", rc);
		return -EINVAL;
	}

	bl_52_lvl = (read[0] << 8) | read[1];
	bl_lvl = panel->bl_config.bl_level;

	OPLUS_DSI_INFO("bl_lvl=%u, read panel reg of 52=%u\n", bl_lvl, bl_52_lvl);
	if ((bl_lvl != 0) && (bl_52_lvl == 0)) {
		OPLUS_DSI_ERR("The backlight sent by AP is not effective on the panel\n");
	}

	return rc;
}
