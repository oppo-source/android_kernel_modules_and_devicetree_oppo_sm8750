/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_utils.c
** Description : display driver private utils
** Version : 1.1
** Date : 2024/05/09
** Author : Display
******************************************************************/
#include "oplus_display_utils.h"
#include <soc/oplus/system/boot_mode.h>
#include <soc/oplus/system/oplus_project.h>
#include <soc/oplus/device_info.h>
#include <linux/notifier.h>
#include <linux/module.h>
#include "dsi_display.h"
#include "oplus_debug.h"
#include "oplus_display_panel_cmd.h"

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
#include "oplus_adfr.h"
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
#include "oplus_onscreenfingerprint.h"
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

#define GAMMA_COMPENSATION_READ_RETRY_MAX 5
#define GAMMA_COMPENSATION_PERCENTAGE1 82/100
#define GAMMA_COMPENSATION_PERCENTAGE2 87/100
#define REG_SIZE 256
#define OPLUS_DSI_CMD_PRINT_BUF_SIZE 1024
#define GAMMA_COMPENSATION_READ_LENGTH 6
#define GAMMA_COMPENSATION_READ_REG 0x81
#define GAMMA_COMPENSATION_BAND_REG 0x99
#define GAMMA_COMPENSATION_BAND_VALUE1 0x81
#define GAMMA_COMPENSATION_BAND_VALUE2 0xB1
#define GAMMA_COMPENSATION_BAND_VALUE3 0x8D
#define GAMMA_COMPENSATION_BAND_VALUE4 0xBD

bool g_gamma_regs_read_done = false;
EXPORT_SYMBOL(g_gamma_regs_read_done);

/* log level config */
unsigned int oplus_display_log_level = OPLUS_LOG_LEVEL_INFO;
unsigned int oplus_display_trace_enable = OPLUS_DISPLAY_DISABLE_TRACE;
unsigned int oplus_display_log_type = OPLUS_DEBUG_LOG_DISABLED;

static enum oplus_display_support_list  oplus_display_vendor =
		OPLUS_DISPLAY_UNKNOW;
static BLOCKING_NOTIFIER_HEAD(oplus_display_notifier_list);

static struct dsi_display *primary_display;
static struct dsi_display *secondary_display;
/* add for dual panel */
static struct dsi_display *current_display = NULL;

bool refresh_rate_change = false;

struct dsi_display *get_main_display(void) {
		return primary_display;
}
EXPORT_SYMBOL(get_main_display);

struct dsi_display *get_sec_display(void) {
		return secondary_display;
}
EXPORT_SYMBOL(get_sec_display);

struct dsi_display *oplus_display_get_current_display(void)
{
	return current_display;
}

int oplus_display_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&oplus_display_notifier_list, nb);
}
EXPORT_SYMBOL(oplus_display_register_client);


int oplus_display_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&oplus_display_notifier_list,
			nb);
}
EXPORT_SYMBOL(oplus_display_unregister_client);

bool oplus_is_correct_display(enum oplus_display_support_list lcd_name)
{
	return (oplus_display_vendor == lcd_name ? true : false);
}

bool oplus_is_silence_reboot(void)
{
	OPLUS_DSI_INFO("get_boot_mode = %d\n", get_boot_mode());
	if ((MSM_BOOT_MODE__SILENCE == get_boot_mode())
			|| (MSM_BOOT_MODE__SAU == get_boot_mode())) {
		return true;

	} else {
		return false;
	}
	return false;
}
EXPORT_SYMBOL(oplus_is_silence_reboot);

bool oplus_is_factory_boot(void)
{
	OPLUS_DSI_INFO("get_boot_mode = %d\n", get_boot_mode());
	if ((MSM_BOOT_MODE__FACTORY == get_boot_mode())
			|| (MSM_BOOT_MODE__RF == get_boot_mode())
			|| (MSM_BOOT_MODE__WLAN == get_boot_mode())
			|| (MSM_BOOT_MODE__MOS == get_boot_mode())) {
		return true;
	} else {
		return false;
	}
	return false;
}
EXPORT_SYMBOL(oplus_is_factory_boot);

int oplus_panel_event_data_notifier_trigger(struct dsi_panel *panel,
		enum panel_event_notification_type notif_type,
		u32 data,
		bool early_trigger)
{
	struct panel_event_notification notifier;
	enum panel_event_notifier_tag panel_type;
	char tag_name[256];

	if (!panel) {
		OPLUS_DSI_ERR("Oplus Features config No panel device\n");
		return -ENODEV;
	}

	if (!strcmp(panel->type, "secondary")) {
		panel_type = PANEL_EVENT_NOTIFICATION_SECONDARY;
	} else {
		panel_type = PANEL_EVENT_NOTIFICATION_PRIMARY;
	}

	snprintf(tag_name, sizeof(tag_name),
		"oplus_panel_event_data_notifier_trigger : [%s] type=0x%X, data=%d, early_trigger=%d",
		panel->type, notif_type, data, early_trigger);
	OPLUS_DSI_TRACE_BEGIN(tag_name);

	OPLUS_DSI_DEBUG("[%s] type=0x%X, data=%d, early_trigger=%d\n",
			panel->type, notif_type, data, early_trigger);

	memset(&notifier, 0, sizeof(notifier));

	notifier.panel = &panel->drm_panel;
	notifier.notif_type = notif_type;
	notifier.notif_data.data = data;
	notifier.notif_data.early_trigger = early_trigger;

	panel_event_notification_trigger(panel_type, &notifier);

	OPLUS_DSI_TRACE_END(tag_name);
	return 0;
}
EXPORT_SYMBOL(oplus_panel_event_data_notifier_trigger);

int oplus_event_data_notifier_trigger(
		enum panel_event_notification_type notif_type,
		u32 data,
		bool early_trigger)
{
	struct dsi_display *display = oplus_display_get_current_display();

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("Oplus Features config No display device\n");
		return -ENODEV;
	}

	oplus_panel_event_data_notifier_trigger(display->panel,
			notif_type, data, early_trigger);

	return 0;
}
EXPORT_SYMBOL(oplus_event_data_notifier_trigger);

int oplus_panel_backlight_notifier(struct dsi_panel *panel, u32 bl_lvl)
{
	u32 threshold = panel->oplus_panel.bl_cfg.dc_backlight_threshold;
	bool dc_mode = panel->oplus_panel.bl_cfg.oplus_dc_mode;

	if (dc_mode && (bl_lvl > 1 && bl_lvl < threshold)) {
		dc_mode = false;
		oplus_panel_event_data_notifier_trigger(panel,
				DRM_PANEL_EVENT_DC_MODE, dc_mode, true);
	} else if (!dc_mode && bl_lvl >= threshold) {
		dc_mode = true;
		oplus_panel_event_data_notifier_trigger(panel,
				DRM_PANEL_EVENT_DC_MODE, dc_mode, true);
	}

	oplus_panel_event_data_notifier_trigger(panel,
			DRM_PANEL_EVENT_BACKLIGHT, bl_lvl, true);

	return 0;
}
EXPORT_SYMBOL(oplus_panel_backlight_notifier);

/* add for dual panel */
void oplus_display_set_current_display(void *dsi_display)
{
	struct dsi_display *display = dsi_display;
	current_display = display;
}

/* update current display when panel is enabled and disabled */
void oplus_display_update_current_display(void)
{
	struct dsi_display *primary_display = get_main_display();
	struct dsi_display *secondary_display = get_sec_display();

	OPLUS_DSI_DEBUG("start\n");

	if ((!primary_display && !secondary_display) || (!primary_display->panel && !secondary_display->panel)) {
		current_display = NULL;
	} else if ((primary_display && !secondary_display) || (primary_display->panel && !secondary_display->panel)) {
		current_display = primary_display;
	} else if ((!primary_display && secondary_display) || (!primary_display->panel && secondary_display->panel)) {
		current_display = secondary_display;
	} else if (primary_display->panel->panel_initialized && !secondary_display->panel->panel_initialized) {
		current_display = primary_display;
	} else if (!primary_display->panel->panel_initialized && secondary_display->panel->panel_initialized) {
		current_display = secondary_display;
	} else if (primary_display->panel->panel_initialized && secondary_display->panel->panel_initialized) {
		current_display = primary_display;
	}

#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	oplus_adfr_update_display_id();
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	if (oplus_ofp_is_supported()) {
		oplus_ofp_update_display_id();
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

	OPLUS_DSI_DEBUG("end\n");

	return;
}

void oplus_check_refresh_rate(const int old_rate, const int new_rate)
{
	if (old_rate != new_rate)
		refresh_rate_change = true;
	else
		refresh_rate_change = false;
}

int oplus_display_set_power(struct drm_connector *connector,
		int power_mode, void *disp)
{
	struct dsi_display *display = disp;
	int rc = 0;

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("display is null\n");
		return -EINVAL;
	}

	display->panel->oplus_panel.power_mode_early = power_mode;

	if (power_mode == SDE_MODE_DPMS_OFF) {
		atomic_set(&display->panel->oplus_panel.esd_pending, 1);
	}

	switch (power_mode) {
	case SDE_MODE_DPMS_LP1:
	case SDE_MODE_DPMS_LP2:
		OPLUS_DSI_INFO("SDE_MODE_DPMS_LP%d\n", power_mode == SDE_MODE_DPMS_LP1 ? 1 : 2);
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
		if (oplus_ofp_is_supported()) {
			oplus_ofp_power_mode_handle(display, power_mode);
		}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
		break;

	case SDE_MODE_DPMS_ON:
		OPLUS_DSI_INFO("SDE_MODE_DPMS_ON\n");
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
		if (oplus_ofp_is_supported()) {
			oplus_ofp_power_mode_handle(display, SDE_MODE_DPMS_ON);
		}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
		break;

	case SDE_MODE_DPMS_OFF:
		OPLUS_DSI_INFO("SDE_MODE_DPMS_OFF\n");
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
		if (oplus_ofp_is_supported()) {
			oplus_ofp_power_mode_handle(display, SDE_MODE_DPMS_OFF);
		}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
		break;

	default:
		return rc;
	}

	OPLUS_DSI_DEBUG("Power mode transition from %d to %d %s\n",
			display->panel->power_mode, power_mode,
			rc ? "failed" : "successful");

	if (!rc) {
		display->panel->power_mode = power_mode;
	}

	return rc;
}
EXPORT_SYMBOL(oplus_display_set_power);

void oplus_display_set_display(void *display)
{
	struct dsi_display *dsi_display = display;

	if (!strcmp(dsi_display->display_type, "primary")) {
		primary_display = dsi_display;
		oplus_display_set_current_display(primary_display);
	} else {
		secondary_display = dsi_display;
	}

	return;
}

int oplus_panel_mipi_err_validate(struct dsi_panel *panel)
{
	int i = 0, tmp = 0;
	u32 len = 0, cmd_count = 0;
	u8 *lenp = NULL;
	u32 data_offset = 0, group_offset = 0, value_offset = 0;
	u32 cmd_index = 0, data_index = 0, group_index = 0;
	u32 match_modes = 0, mode = 0;
	bool matched, group_mode0_matched, group_mode1_matched, group_matched;
	struct oplus_panel_mipi_err_check_config *config;
	char payload[1024] = "";
	u32 cnt = 0;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	config = &(panel->oplus_panel.mipi_err_config);

	match_modes = config->match_modes;
	lenp = config->check_cmds_rlen;
	cmd_count = config->reg_count;

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
						config->check_value[value_offset]) {
					matched = false;
					group_mode0_matched = false;
				}
				else if (mode && config->return_buf[data_offset + data_index] ==
						config->check_value[value_offset]) {
					matched = false;
					tmp++;
				}

				OPLUS_DSI_DEBUG("MIPI check at index/group:[%d/%d] exp:[0x%02X] ret:[0x%02X] mode:[%u] matched:[%d]\n",
						data_offset + data_index,
						group_index,
						config->check_value[value_offset],
						config->return_buf[data_offset + data_index],
						mode,
						matched);
			}

			if (tmp == lenp[cmd_index])
					group_mode1_matched = false;

			data_offset += lenp[cmd_index];
		}
		group_matched = (group_matched || group_mode0_matched) && group_mode1_matched;

		OPLUS_DSI_ERR("mipi err check matching: group:[%d] mode0/mode1/matched:[%d/%d/%d]\n",
				group_index,
				group_mode0_matched,
				group_mode1_matched,
				group_matched);

		group_offset += len;
	}

	cnt += scnprintf(payload + cnt, sizeof(payload) - cnt, "MIPI ERR reg result:");
	for (i = 0; i < len; ++i)
	cnt += scnprintf(payload + cnt, sizeof(payload) - cnt, " [0x%02X]", config->return_buf[i]);

	OPLUS_DSI_INFO("%s; matched [%d]\n", payload, group_matched);

	if (group_matched)
		return 0;

	return -EINVAL;
}

int oplus_panel_mipi_err_reg_read(struct dsi_display_ctrl *ctrl,
		struct dsi_display *display)
{
	int i, rc = 0, ret = 0, start = 0;
	u8 *lenp = NULL;
	struct oplus_panel_mipi_err_check_config *config;
	struct dsi_panel *panel;

	if (!display->panel || !ctrl || !ctrl->ctrl)
		return -EINVAL;

	panel = display->panel;

	/*
	 * When DSI controller is not in initialized state, we do not want to
	 * report a false MIPI ERR failure and hence we defer until next read
	 * happen.
	 */
	if (!dsi_ctrl_validate_host_state(ctrl->ctrl)) {
		OPLUS_DSI_INFO("dsi ctrl validate state!\n");
		return -EINVAL;
	}

	if (phy_pll_bypass(display)) {
		OPLUS_DSI_INFO("phy pll bypass failed!\n");
		return -EINVAL;
	}

	/* switch to target page */
	if (panel->oplus_panel.mipi_err_config.config & OPLUS_MIPI_ERR_CHECK_PAGE_SWITCH) {
		ret = dsi_panel_tx_cmd_set(panel, DSI_CMD_MIPI_ERR_CHECK_PAGE, false);
		if (ret) {
			OPLUS_DSI_ERR("[%s] failed to send DSI_CMD_MIPI_ERR_TARGET_PAGE, rc=%d\n", panel->name, rc);
			return -EINVAL;
		}
	}

	config = &(panel->oplus_panel.mipi_err_config);
	lenp = config->check_cmds_rlen;

	for (i = 0; i < config->reg_count; ++i) {
		rc = dsi_panel_read_panel_reg_unlock(ctrl, panel, config->check_regs[i],
				config->check_buf, config->check_cmds_rlen[i]);
		if (rc < 0) {
			OPLUS_DSI_ERR("rx cmd transfer failed rc=%d\n", rc);
			return -EINVAL;
		} else {
			memcpy(config->return_buf + start,
				config->check_buf, lenp[i]);
			start += lenp[i];
		}
	}
	/* switch to default page */
	if (panel->oplus_panel.mipi_err_config.config & OPLUS_MIPI_ERR_CHECK_PAGE_SWITCH) {
		ret = dsi_panel_tx_cmd_set(panel, DSI_CMD_DEFAULT_SWITCH_PAGE, false);
		if (ret) {
			OPLUS_DSI_ERR("[%s] failed to send DSI_CMD_DEFAULT_SWITCH_PAGE, rc=%d\n", panel->name, rc);
			return -EINVAL;
		}
	}

	return rc;
}


int oplus_panel_mipi_err_check(struct dsi_panel *panel)
{
	int rc = 0, i;
	struct dsi_display_ctrl *m_ctrl, *ctrl;
	struct dsi_display *display = NULL;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid Params");
		return -EINVAL;
	}

	if(!strcmp(panel->type, "primary")) {
		display = get_main_display();
	} else if (!strcmp(panel->type, "secondary")) {
		display = get_sec_display();
	} else {
		OPLUS_DSI_ERR("[DISP][ERR][%s:%d]dsi_display error\n", __func__, __LINE__);
		return -EINVAL;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	if (display->tx_cmd_buf == NULL) {
		rc = dsi_host_alloc_cmd_tx_buffer(display);
		if (rc) {
			OPLUS_DSI_ERR("failed to allocate cmd tx buffer memory\n");
			rc = -EINVAL;
			goto done;
		}
	}

	rc = oplus_panel_mipi_err_reg_read(m_ctrl, display);
	if (rc <= 0) {
		OPLUS_DSI_ERR("[%s] read status failed on master,rc=%d\n",
		       panel->name, rc);
		rc = -EINVAL;
		goto done;
	} else {
		rc = oplus_panel_mipi_err_validate(panel);
	}

	if (!display->panel->sync_broadcast_en)
		goto done;

	display_for_each_ctrl(i, display) {
		ctrl = &display->ctrl[i];
		if (ctrl == m_ctrl)
			continue;

		rc = oplus_panel_mipi_err_reg_read(ctrl, display);
		if (rc <= 0) {
			OPLUS_DSI_ERR("[%s] read status failed on slave,rc=%d\n",
				panel->name, rc);
			rc = -EINVAL;
			goto done;
		} else {
			rc = oplus_panel_mipi_err_validate(panel);
		}
	}

done:
	return rc;
}

int oplus_panel_parse_mipi_err_reg_read_config(struct dsi_panel *panel)
{
	struct oplus_panel_mipi_err_check_config *mipi_err_config;
	int rc = 0;
	u32 tmp = 0;
	u32 count = 0;
	u32 match_mode = 0;
	u32 i = 0, test_len = 0;
	u8 *lenp = NULL;
	struct property *data = NULL;
	struct dsi_parser_utils *utils = NULL;
	const u32 *arr = NULL;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid Params\n");
		return  -EINVAL;
	}

	utils = &panel->utils;
	mipi_err_config = &panel->oplus_panel.mipi_err_config;

	if (!mipi_err_config) {
		OPLUS_DSI_ERR("Invalid mipi_err_config params\n");
		return -EINVAL;
	}

	arr = utils->get_property(utils->data, "oplus,mipi-err-check-reg", &count);
	if (!arr) {
		OPLUS_DSI_ERR("oplus,mipi-err-check-reg parsing failed!\n");
		rc = -EINVAL;
		goto error;
	}
	memcpy(mipi_err_config->check_regs, arr, count);
	mipi_err_config->reg_count = count;


	arr = utils->get_property(utils->data, "oplus,mipi-err-check-count", &count);
	if (!arr) {
		OPLUS_DSI_ERR("oplus,mipi-err-check-count parsing failed!\n");
		rc = -EINVAL;
		goto error;
	}
	memcpy(mipi_err_config->check_cmds_rlen, arr, count);

	/*
	* oplus,mipi-err-check-match-modes is a 32-bit
	* binary flag. Bit value identified how to match the return
	* value of each register. The value 0(default) means equal,
	* and the value 1 means not equal.
	*/
	rc = utils->read_u32(utils->data, "oplus,mipi-err-check-match-modes", &match_mode);
	if (!rc) {
		mipi_err_config->match_modes = match_mode;
		OPLUS_DSI_INFO("Successed to read oplus,mipi-err-check-match-modes=0x%08X\n",
				mipi_err_config->match_modes);
	} else {
		mipi_err_config->match_modes = 0x0;
		OPLUS_DSI_ERR("Failed to read oplus,mipi-err-check-match-modes, set default modes=0x%08X\n",
				mipi_err_config->match_modes);
	}

	test_len = 0;
	lenp = mipi_err_config->check_cmds_rlen;
	for (i = 0; i < count; ++i) {
		test_len += lenp[i];
	}
	if (!test_len) {
		rc = -EINVAL;
		goto error;
	}

	/*
	* Some panel may need multiple read commands to properly
	* check panel status. Do a sanity check for proper status
	* value which will be compared with the value read by dsi
	* controller during MIPI ERR check. Also check if multiple read
	* commands are there then, there should be corresponding
	* status check values for each read command.
	*/

	data = utils->find_property(utils->data,
			"oplus,mipi-err-check-value", &tmp);
	tmp /= sizeof(u8);
	if (!IS_ERR_OR_NULL(data) && tmp != 0 && (tmp % test_len) == 0) {
		mipi_err_config->groups = tmp / test_len;
	} else {
		OPLUS_DSI_ERR("error parse mipi_check_value!\n");
		rc = -EINVAL;
		goto error;
	}

	mipi_err_config->check_value =
		kzalloc(sizeof(u32) * test_len * mipi_err_config->groups,
			GFP_KERNEL);
	if (!mipi_err_config->check_value) {
		rc = -ENOMEM;
		goto error1;
	}

	mipi_err_config->return_buf = kcalloc(test_len * mipi_err_config->groups,
			sizeof(unsigned char), GFP_KERNEL);
	if (!mipi_err_config->return_buf) {
		rc = -ENOMEM;
		goto error2;
	}

	mipi_err_config->check_buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!mipi_err_config->check_buf) {
		rc = -ENOMEM;
		goto error3;
	}

	arr = utils->get_property(utils->data, "oplus,mipi-err-check-value", &count);
	if (!arr || (count != mipi_err_config->groups * test_len)) {
		OPLUS_DSI_ERR("error reading oplus,mipi-err-check-value\n");
		memset(mipi_err_config->check_value, 0, mipi_err_config->groups * test_len);
	}
	memcpy(mipi_err_config->check_value, arr, count);

	arr = utils->get_property(utils->data, "oplus,mipi-err-check-value", &count);
	if (!arr) {
		OPLUS_DSI_ERR("error reading oplus,mipi-err-check-value\n");
		memset(mipi_err_config->check_value, 0, mipi_err_config->groups * test_len);
	}
	memcpy(mipi_err_config->check_value, arr, mipi_err_config->groups * test_len);

	for (i = 0; i < mipi_err_config->groups * test_len; i++) {
		OPLUS_DSI_ERR("check_value[%d] == %d\n", i, (mipi_err_config->check_value)[i]);
	}

	return 0;

error3:
	kfree(mipi_err_config->check_buf);
error2:
	kfree(mipi_err_config->return_buf);
error1:
	kfree(mipi_err_config->check_value);
error:
	return rc;
}

int oplus_panel_parse_mipi_err_check_config(struct dsi_panel *panel)
{
	int rc = 0;
	struct oplus_panel_mipi_err_check_config *mipi_err_config;
	struct dsi_parser_utils *utils = NULL;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid Params\n");
		return  -EINVAL;
	}

	utils = &panel->utils;
	mipi_err_config = &panel->oplus_panel.mipi_err_config;
	rc = utils->read_u32(utils->data, "oplus,mipi-err-check-config",
				&mipi_err_config->config);
	if (rc) {
		OPLUS_DSI_ERR("failed to get oplus,mipi-err-check-config\n");
		/* default disable mipi err check */
		mipi_err_config->config = 0;
	}

	if (!(mipi_err_config->config & OPLUS_MIPI_ERR_CHECK_ENABLE)) {
		OPLUS_DSI_INFO("mipi err check is disable!\n");
		return 0;
	}

	rc = oplus_panel_parse_mipi_err_reg_read_config(panel);
	if (rc) {
		OPLUS_DSI_ERR("failed to parse mipi err check reg params, rc = %d\n", rc);
		goto error;
	}

	return 0;

error:
	panel->oplus_panel.mipi_err_config.config = false;
	return rc;
}

static int oplus_panel_gamma_compensation_read_reg(struct dsi_panel *panel, struct dsi_display_ctrl *m_ctrl, char *regs, u8 value)
{
	int rc = 0;
	u32 cnt = 0;
	int index = 0;
	u8 cmd = GAMMA_COMPENSATION_BAND_REG;
	size_t replace_reg_len = 1;
	char replace_reg[REG_SIZE] = {0};
	char print_buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};

	memset(replace_reg, 0, sizeof(replace_reg));
	replace_reg[0] = value;
	rc = oplus_panel_cmd_reg_replace(panel, DSI_CMD_GAMMA_COMPENSATION_PAGE1, cmd, replace_reg, replace_reg_len);
	if (rc) {
		OPLUS_DSI_ERR("oplus panel cmd reg replace failed, retry\n");
		return rc;
	}
	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_GAMMA_COMPENSATION_PAGE1, false);
	if (rc) {
		OPLUS_DSI_ERR("send DSI_CMD_GAMMA_COMPENSATION_PAGE1 failed, retry\n");
		return rc;
	}

	rc = dsi_panel_read_panel_reg_unlock(m_ctrl, panel, GAMMA_COMPENSATION_READ_REG,
			regs, GAMMA_COMPENSATION_READ_LENGTH);
	if (rc < 0) {
		OPLUS_DSI_ERR("failed to read GAMMA_COMPENSATION_READ_REG rc=%d\n", rc);
		return rc;
	}
	cnt = 0;
	memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
	for (index = 0; index < GAMMA_COMPENSATION_READ_LENGTH; index++) {
		cnt += snprintf(print_buf + cnt, OPLUS_DSI_CMD_PRINT_BUF_SIZE - cnt, "%02X ", regs[index]);
	}
	OPLUS_DSI_INFO("read regs0x%02X len=%d, buf=[%s]\n", value, GAMMA_COMPENSATION_READ_LENGTH, print_buf);

	return 0;
}

int oplus_display_panel_gamma_compensation(struct dsi_display *display)
{
	u32 retry_count = 0;
	u32 index = 0;
	int rc = 0;
	u32 cnt = 0;
	u32 reg_tmp = 0;
	struct dsi_display_mode *mode = NULL;
	char print_buf[OPLUS_DSI_CMD_PRINT_BUF_SIZE] = {0};
	struct dsi_display_ctrl *m_ctrl = NULL;
	struct dsi_panel *panel = display->panel;
	char regs1[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs2[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs3[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs4[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs1_last[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs2_last[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs3_last[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	char regs4_last[GAMMA_COMPENSATION_READ_LENGTH] = {0};
	const char reg_base[GAMMA_COMPENSATION_READ_LENGTH] = {0};

	if (!panel) {
		OPLUS_DSI_ERR("panel is null\n");
		return  -EINVAL;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	if (!m_ctrl) {
		OPLUS_DSI_ERR("ctrl is null\n");
		return -EINVAL;
	}

	if (!panel->oplus_panel.gamma_compensation_support) {
		OPLUS_DSI_INFO("panel gamma compensation isn't supported\n");
		return rc;
	}

	if (display->panel->power_mode != SDE_MODE_DPMS_ON) {
		OPLUS_DSI_ERR("display panel in off status\n");
		return -EINVAL;
	}
	if (!display->panel->panel_initialized) {
		OPLUS_DSI_ERR("panel initialized = false\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);
	while(!g_gamma_regs_read_done && retry_count < GAMMA_COMPENSATION_READ_RETRY_MAX) {
		OPLUS_DSI_INFO("read gamma compensation regs, retry_count=%d\n", retry_count);
		memset(regs1, 0, GAMMA_COMPENSATION_READ_LENGTH);
		memset(regs2, 0, GAMMA_COMPENSATION_READ_LENGTH);
		memset(regs3, 0, GAMMA_COMPENSATION_READ_LENGTH);
		memset(regs4, 0, GAMMA_COMPENSATION_READ_LENGTH);

		rc = oplus_panel_gamma_compensation_read_reg(panel, m_ctrl, regs1, GAMMA_COMPENSATION_BAND_VALUE1);
		if (rc) {
			OPLUS_DSI_ERR("panel read reg1 failed\n");
			retry_count++;
			continue;
		}
		rc = oplus_panel_gamma_compensation_read_reg(panel, m_ctrl, regs2, GAMMA_COMPENSATION_BAND_VALUE2);
		if (rc) {
			OPLUS_DSI_ERR("panel read reg1 failed\n");
			retry_count++;
			continue;
		}
		rc = oplus_panel_gamma_compensation_read_reg(panel, m_ctrl, regs3, GAMMA_COMPENSATION_BAND_VALUE3);
		if (rc) {
			OPLUS_DSI_ERR("panel read reg1 failed\n");
			retry_count++;
			continue;
		}
		rc = oplus_panel_gamma_compensation_read_reg(panel, m_ctrl, regs4, GAMMA_COMPENSATION_BAND_VALUE4);
		if (rc) {
			OPLUS_DSI_ERR("panel read reg1 failed\n");
			retry_count++;
			continue;
		}

		if (!memcmp(regs1, reg_base, sizeof(reg_base)) || !memcmp(regs2, reg_base, sizeof(reg_base)) ||
				!memcmp(regs3, reg_base, sizeof(reg_base)) || !memcmp(regs4, reg_base, sizeof(reg_base)) ||
				memcmp(regs1, regs1_last, sizeof(regs1_last)) || memcmp(regs2, regs2_last, sizeof(regs2_last)) ||
				memcmp(regs3, regs3_last, sizeof(regs1_last)) || memcmp(regs4, regs4_last, sizeof(regs2_last))) {
			OPLUS_DSI_WARN("gamma compensation regs is invalid, retry\n");
			memcpy(regs1_last, regs1, GAMMA_COMPENSATION_READ_LENGTH);
			memcpy(regs2_last, regs2, GAMMA_COMPENSATION_READ_LENGTH);
			memcpy(regs3_last, regs3, GAMMA_COMPENSATION_READ_LENGTH);
			memcpy(regs4_last, regs4, GAMMA_COMPENSATION_READ_LENGTH);
			retry_count++;
			continue;
		}

		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_GAMMA_COMPENSATION_PAGE0, false);
		if (rc) {
			OPLUS_DSI_ERR("send DSI_CMD_GAMMA_COMPENSATION_PAGE0 failed\n");
		}

		g_gamma_regs_read_done = true;
		OPLUS_DSI_INFO("gamma compensation read success");
		break;
	}
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	if (!g_gamma_regs_read_done) {
		return -EFAULT;
	}

	for (index = 0; index < (GAMMA_COMPENSATION_READ_LENGTH - 1); index = index+2) {
		reg_tmp = regs1[index] << 8 | regs1[index+1];
		regs1[index] = (reg_tmp*GAMMA_COMPENSATION_PERCENTAGE1) >> 8 & 0xFF;
		regs1[index+1] = (reg_tmp*GAMMA_COMPENSATION_PERCENTAGE1) & 0xFF;

		reg_tmp = regs2[index] << 8 | regs2[index+1];
		regs2[index] = (reg_tmp*GAMMA_COMPENSATION_PERCENTAGE1) >> 8 & 0xFF;
		regs2[index+1] = (reg_tmp*GAMMA_COMPENSATION_PERCENTAGE1) & 0xFF;

		reg_tmp = regs3[index] << 8 | regs3[index+1];
		regs3[index] = (reg_tmp*GAMMA_COMPENSATION_PERCENTAGE2) >> 8 & 0xFF;
		regs3[index+1] = (reg_tmp*GAMMA_COMPENSATION_PERCENTAGE2) & 0xFF;

		reg_tmp = regs4[index] << 8 | regs4[index+1];
		regs4[index] = (reg_tmp*GAMMA_COMPENSATION_PERCENTAGE2) >> 8 & 0xFF;
		regs4[index+1] = (reg_tmp*GAMMA_COMPENSATION_PERCENTAGE2) & 0xFF;
	}

	cnt = 0;
	memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
	for (index = 0; index < GAMMA_COMPENSATION_READ_LENGTH; index++) {
		cnt += snprintf(print_buf + cnt, OPLUS_DSI_CMD_PRINT_BUF_SIZE - cnt, "%02X ", regs1[index]);
	}
	OPLUS_DSI_INFO("compensation regs0x%02X len=%d, buf=[%s]\n", GAMMA_COMPENSATION_BAND_VALUE1,
			GAMMA_COMPENSATION_READ_LENGTH, print_buf);

	cnt = 0;
	memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
	for (index = 0; index < GAMMA_COMPENSATION_READ_LENGTH; index++) {
		cnt += snprintf(print_buf + cnt, OPLUS_DSI_CMD_PRINT_BUF_SIZE - cnt, "%02X ", regs2[index]);
	}
	OPLUS_DSI_INFO("compensation regs0x%02X len=%d, buf=[%s]\n", GAMMA_COMPENSATION_BAND_VALUE2,
			GAMMA_COMPENSATION_READ_LENGTH, print_buf);

	cnt = 0;
	memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
	for (index = 0; index < GAMMA_COMPENSATION_READ_LENGTH; index++) {
		cnt += snprintf(print_buf + cnt, OPLUS_DSI_CMD_PRINT_BUF_SIZE - cnt, "%02X ", regs3[index]);
	}
	OPLUS_DSI_INFO("compensation regs0x%02X len=%d, buf=[%s]\n", GAMMA_COMPENSATION_BAND_VALUE3,
			GAMMA_COMPENSATION_READ_LENGTH, print_buf);

	cnt = 0;
	memset(print_buf, 0, OPLUS_DSI_CMD_PRINT_BUF_SIZE);
	for (index = 0; index < GAMMA_COMPENSATION_READ_LENGTH; index++) {
		cnt += snprintf(print_buf + cnt, OPLUS_DSI_CMD_PRINT_BUF_SIZE - cnt, "%02X ", regs4[index]);
	}
	OPLUS_DSI_INFO("compensation regs0x%02X len=%d, buf=[%s]\n", GAMMA_COMPENSATION_BAND_VALUE4,
			GAMMA_COMPENSATION_READ_LENGTH, print_buf);

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);
	for (index = 0; index < display->panel->num_display_modes; index++) {
		mode = &display->modes[index];
		if (!mode) {
			OPLUS_DSI_INFO("mode is null\n");
			continue;
		}
		rc = oplus_panel_cmd_reg_replace_specific_row(panel, mode, DSI_CMD_GAMMA_COMPENSATION, regs1,
			GAMMA_COMPENSATION_READ_LENGTH, 5/* rows of cmd */);
		if (rc) {
			OPLUS_DSI_ERR("DSI_CMD_GAMMA_COMPENSATION reg1 replace failed\n");
			g_gamma_regs_read_done = false;
			return -EFAULT;
		}
		rc = oplus_panel_cmd_reg_replace_specific_row(panel, mode, DSI_CMD_GAMMA_COMPENSATION, regs2,
				GAMMA_COMPENSATION_READ_LENGTH, 7/* rows of cmd */);
		if (rc) {
			OPLUS_DSI_ERR("DSI_CMD_GAMMA_COMPENSATION reg2 replace failed\n");
			g_gamma_regs_read_done = false;
			return -EFAULT;
		}
		rc = oplus_panel_cmd_reg_replace_specific_row(panel, mode, DSI_CMD_GAMMA_COMPENSATION, regs3,
				GAMMA_COMPENSATION_READ_LENGTH, 9/* rows of cmd */);
		if (rc) {
			OPLUS_DSI_ERR("DSI_CMD_GAMMA_COMPENSATION reg3 replace failed\n");
			g_gamma_regs_read_done = false;
			return -EFAULT;
		}
		rc = oplus_panel_cmd_reg_replace_specific_row(panel, mode, DSI_CMD_GAMMA_COMPENSATION, regs4,
				GAMMA_COMPENSATION_READ_LENGTH, 11/* rows of cmd */);
		if (rc) {
			OPLUS_DSI_ERR("DSI_CMD_GAMMA_COMPENSATION reg4 replace failed\n");
			g_gamma_regs_read_done = false;
			return -EFAULT;
		}
		OPLUS_DSI_INFO("display mode%d had completed gamma compensation\n", index);
	}
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return rc;
}
