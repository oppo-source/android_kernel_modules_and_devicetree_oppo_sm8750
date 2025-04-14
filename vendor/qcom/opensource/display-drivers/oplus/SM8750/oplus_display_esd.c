/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_esd.c
** Description : oplus esd feature
** Version : 2.0
** Date : 2024/05/09
** Author : Display
******************************************************************/

#ifdef OPLUS_TRACKPOINT_REPORT
#include <soc/oplus/oplus_trackpoint_report.h>
#endif /* OPLUS_TRACKPOINT_REPORT */
#include "oplus_display_esd.h"
#include "oplus_debug.h"

int oplus_panel_parse_esd_reg_read_configs(struct dsi_panel *panel)
{
	struct drm_panel_esd_config *esd_config;
	int rc = 0;
	u32 tmp;
	struct dsi_parser_utils *utils = &panel->utils;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid Params\n");
		return -EINVAL;
	}

	esd_config = &panel->esd_config;
	if (!esd_config)
		return -EINVAL;

	/*
	 * oplus,mdss-dsi-panel-status-match-modes is a 32-bit
	 * binary flag. Bit value identified how to match the return
	 * value of each register. The value 0(default) means equal,
	 * and the value 1 means not equal.
	 */
	rc = utils->read_u32(utils->data, "oplus,mdss-dsi-panel-status-match-modes", &tmp);
	if (!rc) {
		esd_config->oplus_esd_cfg.status_match_modes = tmp;
		OPLUS_DSI_INFO("Successed to read ESD match modes=0x%08X\n",
				esd_config->oplus_esd_cfg.status_match_modes);
	} else {
		esd_config->oplus_esd_cfg.status_match_modes = 0x0;
		OPLUS_DSI_INFO("Failed to read ESD match modes, set default modes=0x%08X\n",
				esd_config->oplus_esd_cfg.status_match_modes);
	}

	return rc;
}

bool oplus_panel_validate_reg_read(struct dsi_panel *panel)
{
	int i = 0, tmp = 0;
	u32 *lenp, len = 0, cmd_count = 0;
	u32 data_offset = 0, group_offset = 0, value_offset = 0;
	u32 cmd_index = 0, data_index = 0, group_index = 0;
	u32 match_modes = 0, mode = 0;
	bool matched, group_mode0_matched, group_mode1_matched, group_matched;
	struct drm_panel_esd_config *config;
	char payload[1024] = "";
	u32 cnt = 0;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid params\n");
		return false;
	}

	config = &(panel->esd_config);

	match_modes = config->oplus_esd_cfg.status_match_modes;
	lenp = config->status_valid_params ?: config->status_cmds_rlen;
	cmd_count = config->status_cmd.count;

	for (i = 0; i < cmd_count; i++)
		len += lenp[i];

	group_matched = false;
	group_mode1_matched = true;
	for (group_index = 0, group_offset = 0; group_index < config->groups; ++group_index) {
		group_mode0_matched = true;

		for (cmd_index = 0, data_offset = 0; cmd_index < cmd_count; ++cmd_index) {
			mode = (match_modes >> cmd_index) & 0x01;
			tmp = 0;

			for (data_index = 0; data_index < lenp[cmd_index]; ++data_index) {
				matched = true;
				value_offset = group_offset + data_offset + data_index;

				if (!mode && config->return_buf[data_offset + data_index] !=
						config->status_value[value_offset]) {
					matched = false;
					group_mode0_matched = false;
				}
				else if (mode && config->return_buf[data_offset + data_index] ==
						config->status_value[value_offset]) {
					matched = false;
					tmp++;
				}

				OPLUS_DSI_DEBUG("ESD check at index/group:[%d/%d] exp:[0x%02X] ret:[0x%02X] mode:[%u] matched:[%d]\n",
						data_offset + data_index,
						group_index,
						config->status_value[value_offset],
						config->return_buf[data_offset + data_index],
						mode,
						matched);
			}

			if (tmp == lenp[cmd_index])
					group_mode1_matched = false;

			data_offset += lenp[cmd_index];
		}

		group_matched = (group_matched || group_mode0_matched) && group_mode1_matched;

		OPLUS_DSI_DEBUG("ESD check matching: group:[%d] mode0/mode1/matched:[%d/%d/%d]\n",
				group_index,
				group_mode0_matched,
				group_mode1_matched,
				group_matched);

		group_offset += len;
	}

	if (group_matched)
		return true;

	cnt += scnprintf(payload + cnt, sizeof(payload) - cnt, "DisplayDriverID@@%d$$ESD:", OPLUS_DISP_Q_ERROR_ESD_CHECK_FAIL);
	for (i = 0; i < len; ++i)
		cnt += scnprintf(payload + cnt, sizeof(payload) - cnt, " [0x%02X]", config->return_buf[i]);
	EXCEPTION_TRACKPOINT_REPORT("%s, ESD check failed\n", payload);

	return false;
}

int oplus_display_status_check_error_flag(struct dsi_display *display)
{
	int rc = 1;
	int no_check = 1;
	int read_value = 0;
	int read_value_slave = 0;
	struct drm_panel_esd_config *esd_config;

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("[ESD] Invalid params\n");
		return rc;
	}

	esd_config = &display->panel->esd_config;
	if (!esd_config) {
		return rc;
	}

	if (gpio_is_valid(esd_config->oplus_esd_cfg.esd_error_flag_gpio)) {
		rc = gpio_request(esd_config->oplus_esd_cfg.esd_error_flag_gpio, "error-flag-gpio");
		if (rc < 0) {
			OPLUS_DSI_ERR("[ESD]: request esd_error_flag_gpio[%d] fail, rc=%d\n",
				esd_config->oplus_esd_cfg.esd_error_flag_gpio, rc);
			return no_check;
		}
		rc = gpio_direction_input(esd_config->oplus_esd_cfg.esd_error_flag_gpio);
		if (rc < 0) {
			OPLUS_DSI_ERR("[ESD]: input esd_error_flag_gpio[%d] fail, rc=%d\n",
				esd_config->oplus_esd_cfg.esd_error_flag_gpio, rc);
			return no_check;
		}
		read_value = gpio_get_value(esd_config->oplus_esd_cfg.esd_error_flag_gpio);
		gpio_free(esd_config->oplus_esd_cfg.esd_error_flag_gpio);
		OPLUS_DSI_INFO("[ESD]: read: master=%d, mipi_err_expect_value=%d\n",
			read_value, esd_config->oplus_esd_cfg.esd_error_flag_expect_value);
		if (read_value != esd_config->oplus_esd_cfg.esd_error_flag_expect_value) {
			OPLUS_DSI_ERR("[ESD]: check failed! rc=%d\n", rc);
			return -EINVAL;
		}
	}

	if (gpio_is_valid(esd_config->oplus_esd_cfg.esd_error_flag_gpio_slave)) {
		rc = gpio_request(esd_config->oplus_esd_cfg.esd_error_flag_gpio_slave, "error-flag-gpio-slave");
		if (rc < 0) {
			OPLUS_DSI_ERR("[ESD]: request esd_error_flag_gpio_slave[%d] fail, rc=%d\n",
				esd_config->oplus_esd_cfg.esd_error_flag_gpio_slave, rc);
			return no_check;
		}
		rc = gpio_direction_input(esd_config->oplus_esd_cfg.esd_error_flag_gpio_slave);
		if (rc < 0) {
			OPLUS_DSI_ERR("[ESD]: input esd_error_flag_gpio_slave[%d] fail, rc=%d\n",
				esd_config->oplus_esd_cfg.esd_error_flag_gpio_slave, rc);
			return no_check;
		}
		read_value_slave = gpio_get_value(esd_config->oplus_esd_cfg.esd_error_flag_gpio_slave);
		gpio_free(esd_config->oplus_esd_cfg.esd_error_flag_gpio_slave);
		OPLUS_DSI_INFO("[ESD]: read: slave=%d, mipi_err_expect_value_slave=%d\n",
			read_value_slave, esd_config->oplus_esd_cfg.esd_error_flag_expect_value_slave);
		if (read_value_slave != esd_config->oplus_esd_cfg.esd_error_flag_expect_value_slave) {
			OPLUS_DSI_ERR("[ESD]: check failed! rc=%d\n", rc);
			return -EINVAL;
		}
	}

	return no_check;
}
