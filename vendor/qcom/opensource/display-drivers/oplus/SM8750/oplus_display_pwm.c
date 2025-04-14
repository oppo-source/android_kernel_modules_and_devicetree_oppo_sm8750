/***************************************************************
** Copyright (C),  2024,  OPLUS Mobile Comm Corp.,  Ltd
** File : oplus_display_pwm.c
** Description : oplus high frequency PWM
** Version : 1.0
** Date : 2024/05/09
**
** ------------------------------- Revision History: -----------
**  <author>          <data>        <version >        <desc>
**  Li.Ping        2023/07/11        1.0           Build this moudle
**  Huang Junming  2024/07/28        2.0           Refactor this moudle
******************************************************************/
#include <linux/notifier.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#include "oplus_display_sysfs_attrs.h"
#include "oplus_display_pwm.h"
#include "oplus_display_interface.h"
#include "oplus_display_device_ioctl.h"
#include "oplus_display_bl.h"
#include "oplus_display_panel_cmd.h"
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
#include "oplus_adfr.h"
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

/* -------------------- macro -------------------- */
/* pwm feature bit setting */
#define OPLUS_PWM_SUPPORT									(BIT(0))
#define OPLUS_PWM_SWITCH_SUPPORT							(BIT(1))
#define OPLUS_PWM_SWITCH_CMD_SUPPORT						(BIT(2))
#define OPLUS_PWM_DBV_THRESHOLD_CMD					        (BIT(3))
#define OPLUS_PWM_DBV_THRESHOLD_CMD_WAIT_TE					(BIT(4))
#define OPLUS_PWM_CMD_REPLACE								(BIT(5))
#define OPLUS_PWM_PANEL_ON_EXT_CMD							(BIT(6))
#define OPLUS_PWM_TIMING_SWITCH_EXT_CMD						(BIT(7))
#define OPLUS_PWM_DBV_THREESHOLD_EXT_CMD					(BIT(8))

/* -------------------- extern ---------------------------------- */
extern u32 oplus_last_backlight;
extern bool refresh_rate_change;
extern struct panel_id panel_id;
extern const char *cmd_set_prop_map[];

u32 bl_lvl = 0;

int string_in_array(const char *str, const char *arr[], int size) {
	int i = 0;
	for (i = 0; i < size; i++) {
        if (strcmp(str, arr[i]) == 0) {
			return i;
        }
	}
	return -1;
}

bool oplus_panel_pwm_support(struct dsi_panel *panel)
{
	if (!panel) {
		OPLUS_PWM_ERR("Invalid dsi_panel params\n");
		return false;
	}

	if (!panel->oplus_panel.pwm_params.pwm_config) {
		OPLUS_PWM_DEBUG("Invalid pwm_config params\n");
		return false;
	}
	return (bool)((panel->oplus_panel.pwm_params.pwm_config) & OPLUS_PWM_SUPPORT);
}

bool oplus_panel_pwm_switch_support(struct dsi_panel *panel)
{
	if (!oplus_panel_pwm_support(panel)) {
		OPLUS_PWM_DEBUG("pwm feature was not supported, so pwm switch was not support\n");
		return false;
	}

	return (bool)((panel->oplus_panel.pwm_params.pwm_config) & OPLUS_PWM_SWITCH_SUPPORT);
}

bool oplus_panel_pwm_switch_cmd_support(struct dsi_panel *panel)
{
	if (!oplus_panel_pwm_switch_support(panel)) {
		OPLUS_PWM_DEBUG("pwm switch was not supported, so pwm switch cmd was not support\n");
		return false;
	}

	return (bool)((panel->oplus_panel.pwm_params.pwm_config) & OPLUS_PWM_SWITCH_CMD_SUPPORT);
}


bool oplus_panel_pwm_dbv_threshold_cmd_enabled(struct dsi_panel *panel)
{
	if (!oplus_panel_pwm_support(panel)) {
		OPLUS_PWM_DEBUG("pwm feature was not supported, so pwm dbv threshold cmd was not support\n");
		return false;
	}

	return (bool)((panel->oplus_panel.pwm_params.pwm_config) & OPLUS_PWM_DBV_THRESHOLD_CMD);
}

bool oplus_panel_pwm_dbv_cmd_wait_te(struct dsi_panel *panel)
{
	if (!oplus_panel_pwm_dbv_threshold_cmd_enabled(panel)) {
		OPLUS_PWM_DEBUG("pwm dbv threshold was not supported, so threshold cmd wait te was not support\n");
		return false;
	}

	return (bool)((panel->oplus_panel.pwm_params.pwm_config) & OPLUS_PWM_DBV_THRESHOLD_CMD_WAIT_TE);
}

bool oplus_panel_pwm_cmd_replace_enabled(struct dsi_panel *panel)
{
	if (!oplus_panel_pwm_switch_support(panel)) {
		OPLUS_PWM_DEBUG("pwm switch was not supported, so cmd replace was not support\n");
		return false;
	}

	return (bool)((panel->oplus_panel.pwm_params.pwm_config) & OPLUS_PWM_CMD_REPLACE);
}

bool oplus_panel_pwm_panel_on_ext_cmd_support(struct dsi_panel *panel)
{
	if (!oplus_panel_pwm_support(panel)) {
		OPLUS_PWM_DEBUG("pwm feature was not supported, so pwm panel on ext cmd was not support\n");
		return false;
	}

	return (bool)((panel->oplus_panel.pwm_params.pwm_config) & OPLUS_PWM_PANEL_ON_EXT_CMD);
}

bool oplus_panel_pwm_timing_switch_ext_cmd_enabled(struct dsi_panel *panel)
{
	if (!oplus_panel_pwm_support(panel)) {
		OPLUS_PWM_DEBUG("pwm feature was not supported, so pwm timing switch ext cmd was not support\n");
		return false;
	}

	return (bool)((panel->oplus_panel.pwm_params.pwm_config) & OPLUS_PWM_TIMING_SWITCH_EXT_CMD);
}

bool oplus_panel_pwm_dbv_threshold_ext_cmd_enabled(struct dsi_panel *panel)
{
	if (!oplus_panel_pwm_support(panel)) {
		OPLUS_PWM_DEBUG("pwm dbv threshold was not supported, so threshold ext cmd was not support\n");
		return false;
	}

	return (bool)((panel->oplus_panel.pwm_params.pwm_config) & OPLUS_PWM_DBV_THREESHOLD_EXT_CMD);
}

int oplus_panel_pwm_get_state(struct dsi_panel *panel)
{
	if (!panel) {
		OPLUS_PWM_ERR("Invalid dsi_panel params\n");
		return -EINVAL;
	}

	return panel->oplus_panel.pwm_params.pwm_pulse_state;
}

int oplus_panel_pwm_get_state_last(struct dsi_panel *panel)
{
	if (!panel) {
		OPLUS_PWM_ERR("Invalid dsi_panel params\n");
		return -EINVAL;
	}

	return panel->oplus_panel.pwm_params.pwm_pulse_state_last;
}

/*
  get current panel pwm switch state
  generally, PWM_SWITCH_MODE0 is 3+18 pulse mode, and PWM_SWITCH_MODE1 is 1+18 pulse mode,
  and PWM_SWITCH_MODE2 is 1+1 pulse mode.
*/
int oplus_panel_pwm_get_switch_state(struct dsi_panel *panel)
{
	if (!panel) {
		OPLUS_PWM_ERR("Invalid dsi_panel params\n");
		return -EINVAL;
	}

	return panel->oplus_panel.pwm_params.pwm_switch_state;
}

int oplus_panel_pwm_parse_states_config(struct dsi_panel *panel, enum PWM_SWITCH_STATE mode) {
	struct dsi_parser_utils *utils = NULL;
	int rc = 0;
	int i = 0;
	int states_info_count = 0;
	int last_threshold = 0;
	u32 states_info[PWM_STATE_MAXNUM * 2 - 1];
	const char *dtsi_config;
	unsigned int *states;
	unsigned int *thresholds;
	unsigned int *states_count;

	if (!panel) {
		OPLUS_PWM_ERR("pwm_probe Invalid panel params\n");
		return -EINVAL;
	}

	utils = &panel->utils;

	if (mode == PWM_SWITCH_MODE0) {
		dtsi_config = "oplus,pwm-switch-mode0-states-info";
		states = panel->oplus_panel.pwm_params.pwm_mode0_states;
		thresholds = panel->oplus_panel.pwm_params.pwm_mode0_thresholds;
		states_count = &panel->oplus_panel.pwm_params.pwm_mode0_states_count;
	} else if (mode == PWM_SWITCH_MODE1) {
		dtsi_config = "oplus,pwm-switch-mode1-states-info";
		states = panel->oplus_panel.pwm_params.pwm_mode1_states;
		thresholds = panel->oplus_panel.pwm_params.pwm_mode1_thresholds;
		states_count = &panel->oplus_panel.pwm_params.pwm_mode1_states_count;
	} else {
		dtsi_config = "oplus,pwm-switch-mode2-states-info";
		states = panel->oplus_panel.pwm_params.pwm_mode2_states;
		thresholds = panel->oplus_panel.pwm_params.pwm_mode2_thresholds;
		states_count = &panel->oplus_panel.pwm_params.pwm_mode2_states_count;
	}

	states_info_count = utils->count_u32_elems(utils->data, dtsi_config);
	if (states_info_count <= 0 || states_info_count > PWM_STATE_MAXNUM * 2 - 1
		|| states_info_count % 2 == 0) {
		OPLUS_PWM_ERR("config %s not found or invalid format, will use default states config, threshold: 1087\n", dtsi_config);
		rc = -EINVAL;
	} else {
		rc = utils->read_u32_array(utils->data, dtsi_config, states_info, states_info_count);
		if (rc) {
			OPLUS_PWM_ERR("failed to read %s, will use default states config\n", dtsi_config);
		} else {
			for (i = 0; i < states_info_count; i++) {
				if (i % 2 == 0) {
					states[i / 2] = states_info[i];
				} else {
					thresholds[i / 2] = states_info[i];
					if (last_threshold < states_info[i]) {
						last_threshold = states_info[i];
					} else {
						OPLUS_PWM_ERR("Invalid format of %s: pre threshold is bigger than post threshold, will use default states config\n", dtsi_config);
						rc = -EINVAL;
						break;
					}
				}
			}
			*states_count = (states_info_count + 1) / 2;
		}
	}

	if (rc) {
		*states = PWM_STATE_L3;
		*(states + 1) = PWM_STATE_L1;
		*states_count = 2;
		*thresholds = 1087;
	}

	return rc;
}

int oplus_panel_parse_pwm_config(struct dsi_panel *panel)
{
	int rc = 0;
	int val = 0;
	int i = 0;
	int origin_cmd_index = 0;
	int replace_cmd_index = 0;
	struct dsi_parser_utils *utils = NULL;
	const char *cmd_map[2 * MAX_PWM_CMD] = {0};
	int raw_cmd_map_count = 0;

	if (!panel) {
		OPLUS_PWM_ERR("pwm_probe Invalid panel params\n");
		return -EINVAL;
	}

	utils = &panel->utils;

	rc = utils->read_u32(utils->data, "oplus,pwm-switch-config", &val);
	if (rc) {
		OPLUS_PWM_ERR("failed to read oplus,pwm-switch-config, rc=%d\n", rc);
		/* set default value to disable*/
		panel->oplus_panel.pwm_params.pwm_config = 0x0;
		goto end;
	} else {
		panel->oplus_panel.pwm_params.pwm_config = val;
	}
	panel->oplus_panel.pwm_params.pwm_mode_count = 0;

	panel->oplus_panel.pwm_params.pwm_compatible_mode = utils->read_bool(utils->data, "oplus,pwm-compatible-mode");
	OPLUS_PWM_INFO("oplus,pwm-compatible-mode: %s\n", panel->oplus_panel.pwm_params.pwm_compatible_mode ? "true" : "false");

	if (oplus_panel_pwm_support(panel)) {
		if (oplus_panel_pwm_dbv_threshold_ext_cmd_enabled(panel)) {
			panel->oplus_panel.pwm_params.oplus_pwm_dbv_ext_cmd_wq = create_singlethread_workqueue("oplus_pwm_dbv_ext_cmd");
			INIT_WORK(&panel->oplus_panel.pwm_params.oplus_pwm_dbv_ext_cmd_work, oplus_pwm_dbv_ext_cmd_work_handler);
		}

		rc = oplus_panel_pwm_parse_states_config(panel, PWM_SWITCH_MODE0);
		if (rc) {
			panel->oplus_panel.pwm_params.pwm_config = OPLUS_PWM_SUPPORT;
			goto end;
		}
		panel->oplus_panel.pwm_params.pwm_mode_count++;

		/* if pwm mode switch support, should parse another mode config */
		if (oplus_panel_pwm_switch_support(panel)) {
			rc = oplus_panel_pwm_parse_states_config(panel, PWM_SWITCH_MODE1);
			if (rc) {
				OPLUS_PWM_ERR("failed to init pwm_switch mode1, disabled pwm switch feature\n");
				panel->oplus_panel.pwm_params.pwm_config &= ~OPLUS_PWM_SWITCH_SUPPORT;
				goto end;
			}
			panel->oplus_panel.pwm_params.pwm_mode_count++;

			if (panel->oplus_panel.pwm_params.pwm_compatible_mode) {
				rc = oplus_panel_pwm_parse_states_config(panel, PWM_SWITCH_MODE2);
				if (rc) {
					OPLUS_PWM_ERR("failed to init pwm_switch mode2, disabled pwm switch feature\n");
					panel->oplus_panel.pwm_params.pwm_config &= ~OPLUS_PWM_SWITCH_SUPPORT;
					goto end;
				}
				panel->oplus_panel.pwm_params.pwm_mode_count++;
			}
		}

		/* if pwm mode switch support, should parse another mode config */
		if (oplus_panel_pwm_cmd_replace_enabled(panel)) {
			raw_cmd_map_count = utils->count_strings(utils->data, "oplus,pwm-cmd-switch-map");
			if (raw_cmd_map_count <= 0) {
				OPLUS_PWM_WARN("cmd_switch was configed but oplus,pwm-cmd-switch-map did not specified\n");
			} else if (raw_cmd_map_count % 2 != 0) {
				OPLUS_PWM_ERR("invalid format of oplus,pwm-cmd-switch-map!\n");
			} else {
				OPLUS_PWM_INFO("pwm cmd replace map count: %d\n", raw_cmd_map_count / 2);
				rc = of_property_read_string_array(utils->data, "oplus,pwm-cmd-switch-map", cmd_map, raw_cmd_map_count);
				if (rc < 0) {
					OPLUS_PWM_ERR("failed to read oplus,pwm-cmd-switch-map, rc=%d\n", rc);
					panel->oplus_panel.pwm_params.pwm_cmd_replace_map_count = 0;
					goto end;
				} else {
					for (i = 0; i < raw_cmd_map_count; i += 2) {
						origin_cmd_index = string_in_array(cmd_map[i], cmd_set_prop_map, DSI_CMD_SET_MAX);
						replace_cmd_index = string_in_array(cmd_map[i+1], cmd_set_prop_map, DSI_CMD_SET_MAX);

						if (origin_cmd_index < 0) {
							OPLUS_PWM_ERR("Invalid name %s of cmd, will disable pwm cmd replace feature\n", cmd_map[i]);
							goto end;
						}

						if (replace_cmd_index < 0) {
							OPLUS_PWM_ERR("Invalid name %s of cmd, will disable pwm cmd replace feature\n", cmd_map[i+1]);
							goto end;
						}
						panel->oplus_panel.pwm_params.pwm_cmd_replace_map[0][i / 2] = origin_cmd_index;
						panel->oplus_panel.pwm_params.pwm_cmd_replace_map[1][i / 2] = replace_cmd_index;

						OPLUS_PWM_INFO("replace cmd %s to %s while pwm switch on\n",
							cmd_set_prop_map[panel->oplus_panel.pwm_params.pwm_cmd_replace_map[0][i / 2]],
							cmd_set_prop_map[panel->oplus_panel.pwm_params.pwm_cmd_replace_map[1][i / 2]]);
					}
					panel->oplus_panel.pwm_params.pwm_cmd_replace_map_count = raw_cmd_map_count / 2;
				}
			}
		}
	}

	OPLUS_PWM_INFO("pwm probe successful\n");
end:
	panel->oplus_panel.pwm_params.pwm_power_on = false;
	panel->oplus_panel.pwm_params.pwm_hbm_state = false;

	return rc;
}

int oplus_pwm_set_power_on(struct dsi_panel *panel)
{
	if (!panel) {
		OPLUS_PWM_ERR("Invalid panel params\n");
		return -EINVAL;
	}

	if (!oplus_panel_pwm_support(panel)) {
		panel->oplus_panel.pwm_params.pwm_power_on = true;
	}

	return 0;
}

int oplus_panel_pwm_panel_on_handle(struct dsi_panel *panel)
{
	int rc = 0;
	int pwm_enable_cmd = DSI_CMD_PWM_SWITCH_MODE0_PANEL_ON;

	if (!oplus_panel_pwm_panel_on_ext_cmd_support(panel)) {
		OPLUS_PWM_DEBUG("pwm power on does not support\n");
		return 0;
	}

	switch(panel->oplus_panel.pwm_params.pwm_switch_state) {
	case PWM_SWITCH_MODE0:
		pwm_enable_cmd = DSI_CMD_PWM_SWITCH_MODE0_PANEL_ON;
		break;
	case PWM_SWITCH_MODE1:
		if (panel->oplus_panel.pwm_params.pwm_compatible_mode) {
			panel->oplus_panel.pwm_params.pwm_pulse_state_last = panel->oplus_panel.pwm_params.pwm_pulse_state;
			if (panel->bl_config.bl_level <= panel->oplus_panel.pwm_params.pwm_mode1_thresholds[0]) {
				panel->oplus_panel.pwm_params.pwm_pulse_state = PWM_STATE_L3;
				pwm_enable_cmd = DSI_CMD_PWM_SWITCH_MODE0_PANEL_ON;
			} else {
				panel->oplus_panel.pwm_params.pwm_pulse_state = PWM_STATE_L1;
				pwm_enable_cmd = DSI_CMD_PWM_SWITCH_MODE2_PANEL_ON;
			}
		} else {
			pwm_enable_cmd = DSI_CMD_PWM_SWITCH_MODE1_PANEL_ON;
		}
		break;
	case PWM_SWITCH_MODE2:
		pwm_enable_cmd = DSI_CMD_PWM_SWITCH_MODE2_PANEL_ON;
		break;
	default:
		break;
	}

	rc = dsi_panel_tx_cmd_set(panel, pwm_enable_cmd, false);

	if (rc)
		OPLUS_PWM_ERR("[%s] failed to send pwm power on cmds rc=%d\n", panel->name, rc);

	return rc;
}

void oplus_panel_pwm_cmd_replace_handle(struct dsi_panel *panel, enum dsi_cmd_set_type *type)
{
	int i = 0;

	if (!oplus_panel_pwm_cmd_replace_enabled(panel)) {
		OPLUS_PWM_DEBUG("pwm cmd replace does not support\n");
		return;
	}

	if (panel->oplus_panel.pwm_params.pwm_compatible_mode
			&& oplus_panel_pwm_get_switch_state(panel) == PWM_SWITCH_MODE1) {
		if (oplus_panel_pwm_get_state(panel) == PWM_STATE_L1) {
			for (i = 0; i < panel->oplus_panel.pwm_params.pwm_cmd_replace_map_count; i++) {
				if (panel->oplus_panel.pwm_params.pwm_cmd_replace_map[0][i] == *type) {
					*type = panel->oplus_panel.pwm_params.pwm_cmd_replace_map[1][i];
					OPLUS_PWM_INFO("replace cmd %s to %s\n",
						cmd_set_prop_map[panel->oplus_panel.pwm_params.pwm_cmd_replace_map[0][i]],
						cmd_set_prop_map[panel->oplus_panel.pwm_params.pwm_cmd_replace_map[1][i]]);
					break;
				}
			}
		}
	} else if (oplus_panel_pwm_get_switch_state(panel) == PWM_SWITCH_MODE1
			|| oplus_panel_pwm_get_switch_state(panel) == PWM_SWITCH_MODE2) {
		for (i = 0; i < panel->oplus_panel.pwm_params.pwm_cmd_replace_map_count; i++) {
			if (panel->oplus_panel.pwm_params.pwm_cmd_replace_map[0][i] == *type) {
				*type = panel->oplus_panel.pwm_params.pwm_cmd_replace_map[1][i];
				OPLUS_PWM_INFO("replace cmd %s to %s\n",
					cmd_set_prop_map[panel->oplus_panel.pwm_params.pwm_cmd_replace_map[0][i]],
					cmd_set_prop_map[panel->oplus_panel.pwm_params.pwm_cmd_replace_map[1][i]]);
				break;
			}
		}
	}

	return;
}

int oplus_hbm_pwm_state(struct dsi_panel *panel, bool hbm_state)
{
	if (!panel) {
		OPLUS_PWM_ERR("Invalid panel params\n");
		return -EINVAL;
	}

	if (oplus_panel_pwm_support(panel) && hbm_state) {
		if (oplus_panel_pwm_get_switch_state(panel) == PWM_SWITCH_MODE1) {
			oplus_panel_event_data_notifier_trigger(panel, DRM_PANEL_EVENT_PWM_TURBO, 1, true);
		} else {
			oplus_panel_event_data_notifier_trigger(panel, DRM_PANEL_EVENT_PWM_TURBO, !hbm_state, true);
		}
	}

	if (oplus_panel_pwm_support(panel)) {
		panel->oplus_panel.pwm_params.pwm_hbm_state = hbm_state;

		if (!hbm_state) {
			panel->oplus_panel.pwm_params.pwm_power_on = true;
		}
	}
	OPLUS_PWM_INFO("set oplus pwm_hbm_state = %d\n", hbm_state);
	return 0;
}

void oplus_panel_pwm_wait_te_handle(struct dsi_panel *panel)
{
	unsigned int refresh_rate = 0;
	unsigned int last_refresh_rate = 0;

	refresh_rate = panel->cur_mode->timing.refresh_rate;
	last_refresh_rate = panel->oplus_panel.last_refresh_rate;

	oplus_sde_early_wakeup(panel);
	oplus_wait_for_vsync(panel);
	if (refresh_rate == 60 || refresh_rate == 90 || (refresh_rate == 120 && last_refresh_rate == 90)) {
		oplus_need_to_sync_te(panel);
	} else if (refresh_rate == 120) {
		usleep_range(300, 300);
	}

	return;
}

int get_state_by_dbv(struct dsi_panel *panel, u32 dbv, enum PWM_STATE *cur_state)
{
	unsigned int *states;
	unsigned int *thresholds;
	unsigned int states_count;
	unsigned int threshold_count;
	unsigned int i;

	if (!panel) {
		OPLUS_PWM_ERR("oplus_panel_pwm_switch_tx_cmd Invalid panel params\n");
		return -EINVAL;
	}

	if (panel->oplus_panel.pwm_params.pwm_switch_state == PWM_SWITCH_MODE0) {
		states = panel->oplus_panel.pwm_params.pwm_mode0_states;
		thresholds = panel->oplus_panel.pwm_params.pwm_mode0_thresholds;
		states_count = panel->oplus_panel.pwm_params.pwm_mode0_states_count;
	} else if (panel->oplus_panel.pwm_params.pwm_switch_state == PWM_SWITCH_MODE1) {
		states = panel->oplus_panel.pwm_params.pwm_mode1_states;
		thresholds = panel->oplus_panel.pwm_params.pwm_mode1_thresholds;
		states_count = panel->oplus_panel.pwm_params.pwm_mode1_states_count;
	} else {
		states = panel->oplus_panel.pwm_params.pwm_mode2_states;
		thresholds = panel->oplus_panel.pwm_params.pwm_mode2_thresholds;
		states_count = panel->oplus_panel.pwm_params.pwm_mode2_states_count;
	}

	threshold_count = states_count - 1;
	if (threshold_count > 0) {
		for(i = 0; i < threshold_count; i++) {
			if (dbv <= thresholds[i]) {
				*cur_state = states[i];
				break;
			} else {
				*cur_state = states[i + 1];
			}
		}
	} else {
		*cur_state = states[0];
	}

	return 0;
}

int oplus_panel_pwm_dbv_threshold_switch_tx_cmd(struct dsi_panel *panel)
{
	int rc = 0;
	u32 pwm_switch_cmd = 0;
	u32 last_state = panel->oplus_panel.pwm_params.pwm_pulse_state_last;
	u32 cur_state = panel->oplus_panel.pwm_params.pwm_pulse_state;

	if (!panel) {
		OPLUS_PWM_ERR("oplus_panel_pwm_switch_tx_cmd Invalid panel params\n");
		return -EINVAL;
	}

	if (!oplus_panel_pwm_dbv_threshold_cmd_enabled(panel)) {
		OPLUS_PWM_DEBUG("pwm dbv threshold cmd does not support\n");
		return rc;
	}

	if (panel->oplus_panel.pwm_params.pwm_hbm_state) {
		OPLUS_PWM_INFO("panel pwm_hbm_state true disable pwm switch!\n");
		return rc;
	}

	if ((oplus_panel_pwm_dbv_threshold_ext_cmd_enabled(panel)) && (panel->power_mode != SDE_MODE_DPMS_ON)) {
		OPLUS_PWM_DEBUG("When suppor threshold_ext, the power_mode is not SDE_MODE_DPMS_ON and disable pwm_dbv_threshold_switch\n");
		return rc;
	}

	if (panel->oplus_panel.pwm_params.pwm_power_on && !strcmp(panel->name, "AA590 P 3 A0020 dsc cmd mode panel")) {
		panel->oplus_panel.pwm_params.pwm_power_on = false;
		panel->oplus_panel.pwm_params.pwm_state_changed = false;
		if (panel->oplus_panel.pwm_params.pwm_switch_state == PWM_SWITCH_MODE1 && panel->bl_config.bl_level > 1162) {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_PWM_STATE_L2TOL1, false);
		}
		return rc;
	}

	switch(last_state) {
	case PWM_STATE_L1:
		switch(cur_state) {
		case PWM_STATE_L2:
			pwm_switch_cmd  = DSI_CMD_PWM_STATE_L1TOL2;
			break;
		case PWM_STATE_L3:
			pwm_switch_cmd	= DSI_CMD_PWM_STATE_L1TOL3;
			break;
		}
		break;
	case PWM_STATE_L2:
		switch(cur_state) {
		case PWM_STATE_L1:
			pwm_switch_cmd  = DSI_CMD_PWM_STATE_L2TOL1;
			break;
		case PWM_STATE_L3:
			pwm_switch_cmd	= DSI_CMD_PWM_STATE_L2TOL3;
			break;
		}
		break;
	case PWM_STATE_L3:
		switch(cur_state) {
		case PWM_STATE_L1:
			pwm_switch_cmd  = DSI_CMD_PWM_STATE_L3TOL1;
			break;
		case PWM_STATE_L2:
			pwm_switch_cmd	= DSI_CMD_PWM_STATE_L3TOL2;
			break;
		}
		break;
	}

	if (pwm_switch_cmd == 0) {
		return rc;
	}

	if (oplus_panel_pwm_dbv_cmd_wait_te(panel)) {
		oplus_panel_pwm_wait_te_handle(panel);
	}

	rc = dsi_panel_tx_cmd_set(panel, pwm_switch_cmd, false);

	if (!strcmp(panel->name, "AA590 P 3 A0020 dsc cmd mode panel")) {
		if (pwm_switch_cmd == DSI_CMD_PWM_STATE_L2TOL3 || pwm_switch_cmd == DSI_CMD_PWM_STATE_L3TOL2) {
			oplus_sde_early_wakeup(panel);
			oplus_wait_for_vsync(panel);
		} else if (pwm_switch_cmd == DSI_CMD_PWM_STATE_L1TOL3 || pwm_switch_cmd == DSI_CMD_PWM_STATE_L3TOL1) {
			oplus_sde_early_wakeup(panel);
			oplus_wait_for_vsync(panel);
			oplus_wait_for_vsync(panel);
		}
	}

	if (rc) {
		OPLUS_PWM_ERR("pwm switch cmd %s tx failed!\n, rc = %d", cmd_set_prop_map[pwm_switch_cmd], rc);
	}

	if (oplus_panel_pwm_dbv_threshold_ext_cmd_enabled(panel) && (last_state != cur_state)) {
		queue_work(panel->oplus_panel.pwm_params.oplus_pwm_dbv_ext_cmd_wq, &panel->oplus_panel.pwm_params.oplus_pwm_dbv_ext_cmd_work);
	}

	return rc;
}

void oplus_panel_set_aod_off_te_timestamp(struct dsi_panel *panel) {
	panel->oplus_panel.pwm_params.aod_off_timestamp = ktime_get();
}

int oplus_panel_pwm_compatible_mode_aodoff_handle(struct dsi_panel *panel) {
	ktime_t cur_time;
	u32 interval;
	u32 delay;
	int rc = 0;

	/* first power on backlight */
	cur_time = ktime_get();
	interval = ktime_to_us(ktime_sub(cur_time, panel->oplus_panel.pwm_params.aod_off_timestamp));
	if (interval < 51000) {
		/* delay cmd to the next frame of aod_off for preventing screen flash*/
		delay = 51000 - interval;
		OPLUS_PWM_INFO("need to delay for the interval between last aod off and cur time was %d\n", delay);
		usleep_range(delay, delay + 200);
	}
	return rc;
}

int oplus_panel_pwm_compatible_mode_dbv_threshold_handle(struct dsi_panel *panel, u32 backlight_level)
{
	int rc = 0;
	u32 cmd_set_type = DSI_CMD_PWM_SWITCH_LOW_BL;
	u8 cmd;
	char replace_reg[REG_SIZE];
	size_t replace_reg_len;

	if (!panel->oplus_panel.pwm_params.pwm_compatible_mode) {
		return rc;
	}

	if (panel->oplus_panel.pwm_params.pwm_switch_state != PWM_SWITCH_MODE1) {
		return rc;
	}

	if (panel->oplus_panel.pwm_params.pwm_pulse_state == PWM_STATE_L3) {
		cmd_set_type = DSI_CMD_PWM_SWITCH_LOW_BL;
	} else if (panel->oplus_panel.pwm_params.pwm_pulse_state == PWM_STATE_L1) {
		cmd_set_type = DSI_CMD_PWM_SWITCH_HIGH_BL;
	} else {
		return rc;
	}
	/* Update the 0x51 value when sending dsi command */
	memset(replace_reg, 0, sizeof(replace_reg));
	cmd = 0x51;
	replace_reg_len = 2;
	replace_reg[0] = (backlight_level >> 8) & 0xFF;
	replace_reg[1] = backlight_level & 0xFF;
	oplus_panel_cmd_reg_replace(panel, cmd_set_type, cmd, replace_reg, replace_reg_len);

	oplus_panel_pwm_compatible_mode_aodoff_handle(panel);

	rc = dsi_panel_tx_cmd_set(panel, cmd_set_type, false);

	return rc;
}

int oplus_panel_pwm_dbv_threshold_handle(struct dsi_panel *panel, u32 backlight_level)
{
	int rc = 0;
	enum PWM_STATE state;
	bl_lvl = backlight_level;

	if (!panel) {
		OPLUS_PWM_ERR("oplus_panel_pwm_dbv_threshold_switch Invalid panel params\n");
		return -EINVAL;
	}

	if (panel->oplus_panel.pwm_params.pwm_hbm_state) {
		OPLUS_PWM_INFO("panel pwm_hbm_state true disable pwm switch!\n");
		return rc;
	}

	if (panel->power_mode == SDE_MODE_DPMS_OFF) {
		OPLUS_PWM_INFO("pwm switch cmd shound not send ,because the panel is off status\n");
		return rc;
	}

	panel->oplus_panel.pwm_params.pwm_pulse_state_last = panel->oplus_panel.pwm_params.pwm_pulse_state;

	rc = get_state_by_dbv(panel, bl_lvl, &state);
	panel->oplus_panel.pwm_params.pwm_pulse_state = state;

	if ((panel->oplus_panel.pwm_params.pwm_pulse_state_last != panel->oplus_panel.pwm_params.pwm_pulse_state)
		&& (bl_lvl != 0)) {
		panel->oplus_panel.pwm_params.pwm_state_changed = true;
		OPLUS_PWM_INFO("pwm level state is changed from %d to %d\n",
			panel->oplus_panel.pwm_params.pwm_pulse_state_last,
			panel->oplus_panel.pwm_params.pwm_pulse_state);
		oplus_panel_pwm_compatible_mode_dbv_threshold_handle(panel, bl_lvl);
	}

	if (panel->oplus_panel.pwm_params.pwm_state_changed == true
		|| (oplus_last_backlight == 0 && bl_lvl!= 0)
		|| panel->oplus_panel.pwm_params.pwm_power_on) {
		rc = oplus_panel_pwm_dbv_threshold_switch_tx_cmd(panel);
	}

	panel->oplus_panel.pwm_params.pwm_power_on = false;
	panel->oplus_panel.pwm_params.pwm_state_changed = false;
	oplus_panel_event_data_notifier_trigger(panel, DRM_PANEL_EVENT_PWM_TURBO,
		panel->oplus_panel.pwm_params.pwm_pulse_state, true);

	return rc;
}

int oplus_panel_pwm_switch_timing_switch(struct dsi_panel *panel)
{
	int rc = 0;
	u32 pwm_switch_cmd = 0;

	if (!oplus_panel_pwm_timing_switch_ext_cmd_enabled(panel))
		return rc;

	if (panel->oplus_panel.pwm_params.pwm_hbm_state) {
		OPLUS_PWM_INFO("panel pwm_hbm_state true disable pwm switch!\n");
		return rc;
	}

	pwm_switch_cmd = DSI_CMD_PWM_TIMMING_SWITCH_L1 + panel->oplus_panel.pwm_params.pwm_pulse_state;

	rc = dsi_panel_tx_cmd_set(panel, pwm_switch_cmd, false);

	return rc;
}

void oplus_pwm_dbv_ext_cmd_work_handler(struct work_struct *work)
{
	int rc = 0;
	struct dsi_display *display = oplus_display_get_current_display();

	if (!display || !display->panel) {
		OPLUS_PWM_ERR("oplus_pwm_disable_duty_set_work_handler Invalid panel params\n");
		return;
	}

	oplus_panel_pwm_wait_te_handle(display->panel);

	mutex_lock(&display->panel->panel_lock);
	if (display->panel->power_mode != SDE_MODE_DPMS_ON || !display->panel->panel_initialized) {
		OPLUS_PWM_WARN("display panel in off status\n");
		mutex_unlock(&display->panel->panel_lock);
		return;
	}

	rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_PWM_DBV_THRESHOLD_EXTEND, false);

	mutex_unlock(&display->panel->panel_lock);

	if (rc) {
		OPLUS_PWM_ERR("[%s]failed to send pwm dbv threshold extend cmds rc = %d\n", display->panel->name, rc);
	}

	return;
}

int oplus_panel_send_pwm_switch_dcs_unlock(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		OPLUS_PWM_ERR("oplus_panel_send_pwm_switch_dcs_unlock Invalid panel params\n");
		return -EINVAL;
	}

	if (panel->oplus_panel.pwm_params.pwm_switch_state == PWM_SWITCH_MODE2) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_PWM_SWITCH_MODE2, false);
	} else if (panel->oplus_panel.pwm_params.pwm_switch_state == PWM_SWITCH_MODE0) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_PWM_SWITCH_MODE0, false);
	} else {
		if (panel->oplus_panel.pwm_params.pwm_compatible_mode) {
			panel->oplus_panel.pwm_params.pwm_pulse_state_last = panel->oplus_panel.pwm_params.pwm_pulse_state;
			if (panel->bl_config.bl_level <= panel->oplus_panel.pwm_params.pwm_mode1_thresholds[0]) {
				panel->oplus_panel.pwm_params.pwm_pulse_state = PWM_STATE_L3;
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_PWM_SWITCH_MODE0, false);
			} else {
				panel->oplus_panel.pwm_params.pwm_pulse_state = PWM_STATE_L1;
				rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_PWM_SWITCH_MODE2, false);
			}
		} else {
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_PWM_SWITCH_MODE1, false);
		}
	}

	return rc;
}

inline bool oplus_panel_pwm_turbo_switch_state(struct dsi_panel *panel)
{
	if (!panel) {
		OPLUS_PWM_ERR("panel is NULL\n");
		return false;
	}

	return (bool)oplus_panel_pwm_get_state(panel);
}

inline bool oplus_panel_pwm_turbo_is_enabled(struct dsi_panel *panel)
{
	return oplus_panel_pwm_switch_support(panel);
}

int oplus_panel_update_pwm_turbo_lock(struct dsi_panel *panel, uint32_t mode)
{
	int rc = 0;

	if (!panel) {
		OPLUS_PWM_ERR("oplus_panel_update_pwm_turbo_lock Invalid panel params\n");
		return -EINVAL;
	}

	oplus_panel_event_data_notifier_trigger(panel,
			DRM_PANEL_EVENT_PWM_TURBO, mode, true);

	mutex_lock(&panel->panel_lock);

	panel->oplus_panel.pwm_params.pwm_switch_state = mode;
	if(panel->power_mode != SDE_MODE_DPMS_OFF)
		rc = oplus_panel_send_pwm_switch_dcs_unlock(panel);
	else
		OPLUS_PWM_ERR("Skip send pwm turbo dcs, because display panel is off\n");

	mutex_unlock(&panel->panel_lock);

	return rc;
}

int oplus_display_panel_get_pwm_turbo(void *data)
{
	int rc = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;
	uint32_t *mode = data;

	if (!display || !display->panel) {
		OPLUS_PWM_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}

	panel = display->panel;

	if (!oplus_panel_pwm_switch_support(panel)) {
		OPLUS_PWM_ERR("Falied to get pwm turbo status, because it is unsupport\n");
		rc = -EFAULT;
		return rc;
	}

	/*hummer temp solve fac change pusle*/
	if (!strcmp(panel->name, "AA590 P 3 A0020 dsc cmd mode panel")) {
		OPLUS_PWM_INFO("1pulse mode will skip pwm turbo operation\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&panel->panel_lock);

	*mode = panel->oplus_panel.pwm_params.pwm_switch_state;

	mutex_unlock(&panel->panel_lock);
	mutex_unlock(&display->display_lock);
	OPLUS_PWM_INFO("Get pwm turbo status: %d\n", *mode);

	return rc;
}

int oplus_display_panel_set_pwm_turbo(void *data)
{
	int rc = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;
	uint32_t *mode = data;

	if (!display || !display->panel) {
		OPLUS_PWM_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}

	panel = display->panel;

	if (!oplus_panel_pwm_switch_support(panel)) {
		OPLUS_PWM_WARN("Falied to set pwm turbo status, because it is unsupport\n");
		rc = -EFAULT;
		return rc;
	}


	/*hummer temp solve fac change pusle*/
	if (!strcmp(panel->name, "AA590 P 3 A0020 dsc cmd mode panel")) {
		OPLUS_PWM_INFO("1pulse mode will skip pwm turbo operation\n");
		return -EINVAL;
	}

	OPLUS_PWM_INFO("Set pwm turbo status: %d\n", *mode);

	if (*mode == panel->oplus_panel.pwm_params.pwm_switch_state) {
		OPLUS_PWM_WARN("Skip setting duplicate pwm turbo status: %d\n", *mode);
		rc = -EFAULT;
		return rc;
	}

	mutex_lock(&display->display_lock);
	rc = oplus_panel_update_pwm_turbo_lock(panel, *mode);
	mutex_unlock(&display->display_lock);

	return rc;
}

ssize_t oplus_get_pwm_turbo_debug(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	int rc = 0;
	u32 mode = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;

	if (!display || !display->panel) {
		OPLUS_PWM_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}

	panel = display->panel;

	if (!oplus_panel_pwm_switch_support(panel)) {
		OPLUS_PWM_ERR("Falied to get pwm turbo status, because it is unsupport\n");
		rc = -EFAULT;
		return rc;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&panel->panel_lock);

	mode = panel->oplus_panel.pwm_params.pwm_switch_state;

	mutex_unlock(&panel->panel_lock);
	mutex_unlock(&display->display_lock);
	OPLUS_PWM_INFO("Get pwm turbo status: %d\n", mode);

	return sprintf(buf, "%d\n", mode);
}

ssize_t oplus_set_pwm_turbo_debug(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int rc = 0;
	u32 mode = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;

	if (!display || !display->panel) {
		OPLUS_PWM_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}

	panel = display->panel;

	if (!oplus_panel_pwm_switch_support(panel)) {
		OPLUS_PWM_ERR("Falied to set pwm turbo status, because it is unsupport\n");
		rc = -EFAULT;
		return rc;
	}

	sscanf(buf, "%du", &mode);
	OPLUS_PWM_INFO("Set pwm turbo status: %d\n", mode);

	mutex_lock(&display->display_lock);
	oplus_panel_update_pwm_turbo_lock(panel, mode);
	mutex_unlock(&display->display_lock);

	return count;
}

int oplus_panel_update_pwm_pulse_lock(struct dsi_panel *panel, uint32_t mode)
{
	int rc = 0;
	enum PWM_STATE state;

	mutex_lock(&panel->panel_lock);
	panel->oplus_panel.pwm_params.pwm_switch_state = mode;
	if (!oplus_panel_pwm_dbv_threshold_cmd_enabled(panel)
		|| oplus_panel_pwm_switch_cmd_support(panel)) {
		/* generally, doesn't need to set dbv pulse cmd if pwm pulse is changed by pwm mode switch cmd */
		rc = get_state_by_dbv(panel, bl_lvl, &state);
		panel->oplus_panel.pwm_params.pwm_pulse_state = state;
		panel->oplus_panel.pwm_params.pwm_pulse_state_last = state;
		rc = oplus_panel_send_pwm_switch_dcs_unlock(panel);
	}
	mutex_unlock(&panel->panel_lock);

	return rc;
}

int oplus_display_panel_get_pwm_pulse(void *data)
{
	int rc = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;
	uint32_t *mode = data;

	if (!display || !display->panel) {
		OPLUS_PWM_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}

	panel = display->panel;

	if (!oplus_panel_pwm_switch_support(panel)) {
		OPLUS_PWM_WARN("Falied to get pwm switch status, because it is unsupport\n");
		rc = -EFAULT;
		return rc;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&panel->panel_lock);

	*mode = panel->oplus_panel.pwm_params.pwm_switch_state;

	mutex_unlock(&panel->panel_lock);
	mutex_unlock(&display->display_lock);
	OPLUS_PWM_INFO("Get pwm switch state: %d\n", *mode);

	return panel->oplus_panel.pwm_params.pwm_mode_count;
}

int oplus_display_panel_set_pwm_pulse(void *data)
{
	int rc = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;
	uint32_t *mode = data;

	if (!display || !display->panel) {
		OPLUS_PWM_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}

	panel = display->panel;

	if (!oplus_panel_pwm_switch_support(panel)) {
		OPLUS_PWM_WARN("Falied to set pwm switch status, because it is unsupport\n");
		rc = -EFAULT;
		return rc;
	}

	if (panel->power_mode != SDE_MODE_DPMS_ON) {
		OPLUS_PWM_WARN("Skip set pwm switch, because display panel isn't power on\n");
		rc = -EFAULT;
		return rc;
	}

	if (*mode >= panel->oplus_panel.pwm_params.pwm_mode_count) {
		OPLUS_PWM_ERR("unsupport mode: %u\n", *mode);
		rc = -EFAULT;
		return rc;
	}

	OPLUS_PWM_INFO("Set pwm switch state: %d\n", *mode);

	if (*mode == panel->oplus_panel.pwm_params.pwm_switch_state) {
		OPLUS_PWM_WARN("Skip setting duplicate pwm switch status: %d\n", *mode);
		rc = -EFAULT;
		return rc;
	}

	mutex_lock(&display->display_lock);
	rc = oplus_panel_update_pwm_pulse_lock(panel, *mode);
	mutex_unlock(&display->display_lock);

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	oplus_adfr_set_min_fps_updated(panel);
#endif

	return rc;
}
/* end for pwm  switch */
/* add onepulse switch debug */
ssize_t oplus_get_pwm_pulse_debug(struct kobject *obj,
	struct kobj_attribute *attr, char *buf)
{
	int rc = 0;
	u32 mode = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;

	if (!display || !display->panel) {
		OPLUS_PWM_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}

	panel = display->panel;

	if (!oplus_panel_pwm_switch_support(panel)) {
		OPLUS_PWM_ERR("Falied to get pwm pulse status, because it is unsupport\n");
		rc = -EFAULT;
		return rc;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&panel->panel_lock);

	mode = panel->oplus_panel.pwm_params.pwm_switch_state;

	mutex_unlock(&panel->panel_lock);
	mutex_unlock(&display->display_lock);
	OPLUS_PWM_INFO("Get pwm pulse status: %d\n", mode);

	return sysfs_emit(buf, "%d\n", mode);
}

ssize_t oplus_set_pwm_pulse_debug(struct kobject *obj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int rc = 0;
	u32 mode = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;

	if (!display || !display->panel) {
		OPLUS_PWM_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}

	panel = display->panel;

	if (!oplus_panel_pwm_switch_support(panel)) {
		OPLUS_PWM_ERR("Falied to set pwm onepulse status, because it is unsupport\n");
		rc = -EFAULT;
		return rc;
	}

	if (panel->power_mode != SDE_MODE_DPMS_ON) {
		OPLUS_PWM_WARN("Skip set pwm switch, because display panel isn't power on\n");
		rc = -EFAULT;
		return rc;
	}

	rc = kstrtou32(buf, 10, &mode);
	if (rc) {
		OPLUS_PWM_WARN("%s cannot be converted to u32", buf);
		return count;
	}
	OPLUS_PWM_INFO("Set pwm onepulse status: %d\n", mode);

	if (mode >= panel->oplus_panel.pwm_params.pwm_mode_count) {
		OPLUS_PWM_ERR("unsupport mode: %u\n", mode);
		rc = -EFAULT;
		return rc;
	}

	mutex_lock(&display->display_lock);
	oplus_panel_update_pwm_pulse_lock(panel, mode);
	mutex_unlock(&display->display_lock);

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	oplus_adfr_set_min_fps_updated(panel);
#endif

	return count;
}
/* end onepulse switch debug */
