/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_interface.c
** Description : oplus display interface
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#include <drm/drm_print.h>
#include <drm/drm_connector.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include "oplus_display_bl.h"
#include "oplus_display_interface.h"
#include "oplus_display_device_ioctl.h"
#include "oplus_display_sysfs_attrs.h"
#include "oplus_display_pwm.h"
#include "sde_color_processing.h"
#include "sde_encoder_phys.h"
#include "sde_trace.h"
#include "oplus_debug.h"
#include "oplus_display_ext.h"
#include "oplus_display_effect.h"
#include "oplus_display_esd.h"
#include "oplus_display_parse.h"
#include "oplus_display_panel_cmd.h"
#include "oplus_display_power.h"
#include "oplus_display_device.h"
#include "oplus_display_proc.h"

#ifdef OPLUS_FEATURE_TP_BASIC
#include "oplus_display_notify_tp.h"
#endif /* OPLUS_FEATURE_TP_BASIC */

#define OPLUS_BACKLIGHT_WINDOW_SIZE 5

extern bool is_lhbm_panel;
extern int lcd_closebl_flag;
extern const char *cmd_set_prop_map[];
extern const char *cmd_set_state_map[];
extern bool oplus_enhance_mipi_strength;
extern int dc_apollo_enable;
extern struct dc_apollo_pcc_sync dc_apollo;
extern int oplus_display_private_api_init(void);
extern void oplus_display_private_api_exit(void);
extern struct panel_id panel_id;
extern int is_fpga_work_okay(void);

unsigned int oplus_bl_print_window = OPLUS_BACKLIGHT_WINDOW_SIZE;
extern char oplus_global_hbm_flags;
extern int dcc_flags;

extern bool g_gamma_regs_read_done;

static DEFINE_SPINLOCK(g_bk_lock);

struct oplus_display_ops oplus_display_ops = {};
#ifdef OPLUS_FEATURE_TP_BASIC
struct oplus_display_notify_tp_ops oplus_display_notify_tp_ops = {};
#endif /* OPLUS_FEATURE_TP_BASIC */

void oplus_display_set_backlight_pre(struct dsi_display *display, int *bl_lvl, int brightness)
{
	*bl_lvl = oplus_panel_mult_frac(brightness);

	return;
}

void oplus_display_set_backlight_post(struct sde_connector *c_conn,
		struct dsi_display *display, struct drm_event *event, int brightness, int bl_lvl)
{
	int rc = 0;

	if (c_conn->ops.set_backlight) {
		/* skip notifying user space if bl is 0 */
		if (brightness != 0) {
			event->type = DRM_EVENT_SYS_BACKLIGHT;
			event->length = sizeof(u32);
			msm_mode_object_event_notify(&c_conn->base.base,
				c_conn->base.dev, event, (u8 *)&brightness);
		}

		if (display->panel->oplus_panel.is_apollo_support && backlight_smooth_enable) {
			if ((is_spread_backlight(display, bl_lvl)) && !dc_apollo_sync_hbmon(display)) {
				if (display->panel->oplus_panel.dc_apollo_sync_enable) {
					if ((display->panel->bl_config.bl_level >= display->panel->oplus_panel.sync_brightness_level
						&& display->panel->bl_config.bl_level < display->panel->oplus_panel.dc_apollo_sync_brightness_level)
						|| display->panel->bl_config.bl_level == 4) {
						if (bl_lvl == display->panel->oplus_panel.dc_apollo_sync_brightness_level
							&& dc_apollo_enable
							&& dc_apollo.pcc_last == display->panel->oplus_panel.dc_apollo_sync_brightness_level_pcc) {
							rc = wait_event_timeout(dc_apollo.bk_wait, dc_apollo.dc_pcc_updated, msecs_to_jiffies(17));
							if (!rc) {
								OPLUS_DSI_ERR("dc wait timeout\n");
							}
							else {
								oplus_backlight_wait_vsync(c_conn->encoder);
							}
							dc_apollo.dc_pcc_updated = 0;
						}
					}
					else if (display->panel->bl_config.bl_level < display->panel->oplus_panel.sync_brightness_level
							&& display->panel->bl_config.bl_level > 4) {
						if (bl_lvl == display->panel->oplus_panel.dc_apollo_sync_brightness_level
							&& dc_apollo_enable
							&& dc_apollo.pcc_last >= display->panel->oplus_panel.dc_apollo_sync_brightness_level_pcc_min) {
							rc = wait_event_timeout(dc_apollo.bk_wait, dc_apollo.dc_pcc_updated, msecs_to_jiffies(17));
							if (!rc) {
								OPLUS_DSI_ERR("dc wait timeout\n");
							}
							else {
								oplus_backlight_wait_vsync(c_conn->encoder);
							}
							dc_apollo.dc_pcc_updated = 0;
						}
					}
				}
				spin_lock(&g_bk_lock);
				update_pending_backlight(display, bl_lvl);
				spin_unlock(&g_bk_lock);
			} else {
				spin_lock(&g_bk_lock);
				update_pending_backlight(display, bl_lvl);
				spin_unlock(&g_bk_lock);
				rc = c_conn->ops.set_backlight(&c_conn->base,
				c_conn->display, bl_lvl);
				c_conn->unset_bl_level = 0;
			}
		} else {
			rc = c_conn->ops.set_backlight(&c_conn->base,
			c_conn->display, bl_lvl);
			c_conn->unset_bl_level = 0;
		}
	}

	return;
}

void oplus_panel_set_backlight_pre(struct dsi_display *display, int *bl_lvl)
{
	struct dsi_panel *panel = NULL;

	panel = display->panel;

	if ((*bl_lvl == 0) || (panel->bl_config.bl_level == 0))
		oplus_bl_print_window = OPLUS_BACKLIGHT_WINDOW_SIZE;

	oplus_panel_post_on_backlight(display, panel, *bl_lvl);

	*bl_lvl = oplus_panel_silence_backlight(panel, *bl_lvl);

	if(panel->oplus_panel.bl_ic_ktz8868_used) {
		oplus_printf_backlight_8868_log(display, *bl_lvl);
	} else {
		oplus_printf_backlight_log(display, *bl_lvl);
	}


	return;
}

void oplus_panel_set_backlight_post(struct dsi_panel *panel, u64 bl_lvl)
{
	oplus_panel_backlight_notifier(panel, (u32)bl_lvl);

	if (oplus_bl_print_window > 0)
		oplus_bl_print_window--;

	return;
}

void oplus_bridge_pre_enable(struct dsi_display *display, struct dsi_display_mode *mode)
{
	oplus_panel_switch_vid_mode(display, mode);

	return;
}

void oplus_display_enable_pre(struct dsi_display *display)
{
	oplus_display_update_current_display();
	display->panel->oplus_panel.power_mode_early = SDE_MODE_DPMS_ON;
	display->panel->power_mode = SDE_MODE_DPMS_ON;
	__oplus_read_apl_thread_ctl(true);

	if (display->oplus_display.panel_sn != 0) {
		OPLUS_DSI_INFO("panel serial_number have read in UEFI, serial_number = [%016lX]\n",
					display->oplus_display.panel_sn);
	} else {
		oplus_display_read_serial_number(display, &display->oplus_display.panel_sn);
		OPLUS_DSI_INFO("panel serial_number don't read in UEFI, read panel serial_number = [%016lX]\n",
					display->oplus_display.panel_sn);
	}

	/* Force update of demurra2 offset from UEFI stage to Kernel stage*/
	oplus_panel_need_to_set_demura2_offset(display->panel);

	if (oplus_display_panel_gamma_compensation(display)) {
		OPLUS_DSI_ERR("panel gamma compensation failed\n");
	}

	return;
}

void oplus_display_enable_mid(struct dsi_display *display)
{
	oplus_display_update_current_display();

	/* Force update of demurra2 offset when panel power on*/
	oplus_panel_need_to_set_demura2_offset(display->panel);

	return;
}

void oplus_display_enable_post(struct dsi_display *display)
{
	if (display->panel->oplus_panel.wait_te_config & BIT(0)) {
		oplus_display_wait_for_event(display, MSM_ENC_VBLANK);
		if (display->panel->cur_mode->timing.refresh_rate == 60)
			oplus_need_to_sync_te(display->panel);
	}

	return;
}

int oplus_panel_enable_pre(struct dsi_panel *panel)
{
	int rc = 0;

	OPLUS_DSI_INFO("oplus_panel_enable\n");
	if (panel->oplus_panel.gamma_compensation_support && g_gamma_regs_read_done) {
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_GAMMA_COMPENSATION, false);
		if (rc) {
			OPLUS_DSI_ERR("send DSI_CMD_GAMMA_COMPENSATION failed\n");
		}
	}

	return rc;
}

int oplus_panel_enable_post(struct dsi_panel *panel)
{
	int rc = 0;

	if (panel->oplus_panel.ffc_enabled) {
		oplus_panel_set_ffc_mode_unlock(panel);
	}

	oplus_panel_pwm_panel_on_handle(panel);

	rc = dsi_panel_seed_mode(panel, __oplus_get_seed_mode());
	if (rc) {
		OPLUS_DSI_ERR("Failed to set seed mode: %d\n", __oplus_get_seed_mode());
		return rc;
	}
	if (dcc_flags == 1) {
		OPLUS_DSI_ERR("DCCompensate send dc command\n");
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_DC_ON, false);
		if (rc)
			OPLUS_DSI_ERR("[%s] failed to send DSI_CMD_SET_DC_ON cmds rc=%d\n",
				panel->name, rc);
	}

	panel->oplus_panel.need_power_on_backlight = true;
	panel->power_mode = SDE_MODE_DPMS_ON;

	return rc;
}

void oplus_panel_switch_pre(struct dsi_panel *panel)
{
	panel->oplus_panel.ts_timestamp = ktime_get();

	return;
}

void oplus_panel_switch_post(struct dsi_panel *panel)
{
	/* pwm switch due to timming switch */
	oplus_panel_pwm_switch_timing_switch(panel);

	return;
}

void oplus_panel_enable_init(struct dsi_panel *panel)
{
	/* initialize panel status */
	oplus_panel_init(panel);

	return;
}

void oplus_display_disable_post(struct dsi_display *display)
{
	oplus_display_update_current_display();
	display->panel->oplus_panel.power_mode_early = SDE_MODE_DPMS_OFF;

	return;
}

void oplus_panel_disable_post(struct dsi_panel *panel)
{
	oplus_global_hbm_flags = 0;

	return;
}

void oplus_encoder_kickoff(struct drm_encoder *drm_enc, struct sde_encoder_virt *sde_enc)
{
	/* Add for backlight smooths */
	if ((is_support_apollo_bk(sde_enc->cur_master->connector) == true) && backlight_smooth_enable && !dc_apollo_sync_hbmon(get_main_display())) {
		if (sde_enc->num_phys_encs > 0) {
			oplus_sync_panel_brightness(OPLUS_POST_KICKOFF_METHOD, drm_enc);
		}
	} else {
		oplus_sync_panel_brightness_v2(drm_enc);
	}
	oplus_set_osc_status(drm_enc);

	return;
}

int oplus_display_read_status(struct dsi_panel *panel)
{
	int rc = 0;

	oplus_panel_backlight_check(panel);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_ESD_SWITCH_PAGE, false);
	if (rc) {
		DSI_ERR("[%s] failed to send DSI_CMD_ESD_SWITCH_PAGE, rc=%d\n", panel->name, rc);
		return rc;
	}

	return rc;
}

bool oplus_display_check_status_pre(struct dsi_panel *panel)
{
	if (atomic_read(&panel->oplus_panel.esd_pending)) {
		OPLUS_DSI_INFO("Skip the check because esd is pending\n");
		return true;
	}

	if (panel->power_mode != SDE_MODE_DPMS_ON
			|| panel->oplus_panel.power_mode_early != SDE_MODE_DPMS_ON) {
		OPLUS_DSI_INFO("Skip the check because panel power mode isn't power on, "
				"power_mode_early=%d, power_mode=%d\n",
				panel->oplus_panel.power_mode_early, panel->power_mode);
		return true;
	}

	return false;
}

int oplus_display_check_status_post(struct dsi_display *display)
{
	int rc = 0;

	rc = oplus_display_status_check_error_flag(display);

	return rc;
}

int oplus_display_parse_cmdline_topology(struct dsi_display *display,
			char *boot_str, unsigned int display_type)
{
	char *str = NULL;
	unsigned int panel_id = NO_OVERRIDE;
	unsigned long panel_sn = NO_OVERRIDE;

	str = strnstr(boot_str, ":PanelID-0x", strlen(boot_str));

	if (str) {
		if (sscanf(str, ":PanelID-0x%08X", &panel_id) != 1) {
			OPLUS_DSI_ERR("invalid PanelID override: %s\n",
					boot_str);
			return -1;
		}
		display->oplus_display.panel_flag = (panel_id >> 24) & 0xFF;
		display->oplus_display.panel_id1 = (panel_id >> 16) & 0xFF;
		display->oplus_display.panel_id2 = (panel_id >> 8) & 0xFF;
		display->oplus_display.panel_id3 = panel_id & 0xFF;
	}

	OPLUS_DSI_INFO("Parse cmdline display%d PanelID-0x%08X, Flag=0x%02X, ID1=0x%02X, ID2=0x%02X, ID3=0x%02X\n",
			display_type,
			panel_id,
			display->oplus_display.panel_flag,
			display->oplus_display.panel_id1,
			display->oplus_display.panel_id2,
			display->oplus_display.panel_id3);

	str = strnstr(boot_str, ":PanelSN-0x", strlen(boot_str));

	if (str) {
		if (sscanf(str, ":PanelSN-0x%016lX", &panel_sn) != 1) {
			OPLUS_DSI_ERR("invalid PanelSN override: %s\n",
					boot_str);
			return -1;
		}
		display->oplus_display.panel_sn = panel_sn;
	}

	OPLUS_DSI_INFO("Parse cmdline display%d PanelSN-0x%016lX\n",
			display_type,
			panel_sn);

	return 0;
}

void oplus_display_res_init(struct dsi_display *display)
{
	oplus_panel_parse_features_config(display->panel);
}

void oplus_display_bind_pre(struct dsi_display *display)
{
	if(0 != oplus_display_set_vendor(display)) {
		OPLUS_DSI_ERR("maybe send a null point to oplus display manager\n");
	}

	/* Add for SUA feature request */
	if(oplus_is_silence_reboot()) {
		lcd_closebl_flag = 1;
	}
}

void oplus_display_bind_post(struct dsi_display *display)
{
	if (!strcmp(display->display_type, "primary")) {
		oplus_display_private_api_init();
	}
}

void oplus_display_unbind(struct dsi_display *display)
{
	oplus_display_private_api_exit();
}

void oplus_display_dev_probe(struct dsi_display *display)
{
	oplus_display_set_display(display);
}

void oplus_display_validate_mode_change_pre(struct dsi_display *display)
{
	if (display->panel->oplus_panel.ffc_enabled &&
			display->panel->power_mode == SDE_MODE_DPMS_ON &&
			display->panel->oplus_panel.ffc_delay_frames) {
		oplus_panel_set_ffc_kickoff_lock(display->panel);
	}
}

void oplus_display_validate_mode_change_post(struct dsi_display *display,
			struct dsi_display_mode *cur_mode,
			struct dsi_display_mode *adj_mode)
{
	if (display->panel->oplus_panel.ffc_enabled) {
		oplus_display_update_clk_ffc(display, cur_mode, adj_mode);
	}
}

void oplus_display_register(void)
{
	if(oplus_display_panel_init()) {
		OPLUS_DSI_ERR("oplus_display_panel_init fail\n");
	}

	if(oplus_display_proc_init()) {
		OPLUS_DSI_ERR("oplus_display_proc_init fail\n");
	}
}

void oplus_panel_tx_cmd_set_pre(struct dsi_panel *panel,
				enum dsi_cmd_set_type *type)
{
	oplus_panel_cmd_switch(panel, type);
	oplus_panel_cmdq_sync_handle(panel, *type, true);
	oplus_panel_cmd_print(panel, *type);

	return;
}

void oplus_panel_tx_cmd_set_mid(struct dsi_panel *panel,
				enum dsi_cmd_set_type type)
{
	oplus_panel_cmdq_sync_handle(panel, type, false);

	return;
}

void oplus_panel_tx_cmd_set_post(struct dsi_panel *panel,
				enum dsi_cmd_set_type type, int rc)
{
	if (rc) {
		DSI_ERR("[LCD][%s] failed to send %s, rc=%d\n",
				panel->oplus_panel.vendor_name, cmd_set_prop_map[type], rc);
		WARN_ON(rc);
	}

	return;
}

int oplus_panel_parse_bl_config_post(struct dsi_panel *panel)
{
	int rc = 0;

	rc = oplus_panel_parse_bl_cfg(panel);
	if (rc) {
		DSI_ERR("[%s] failed to parse oplus backlight config, rc=%d\n",
			panel->name, rc);
	}

	return rc;
}

int oplus_panel_parse_esd_reg_read_configs_post(struct dsi_panel *panel)
{
	int rc = 0;

	rc = oplus_panel_parse_esd_reg_read_configs(panel);
	if (rc) {
		DSI_ERR("failed to parse oplus esd reg read config, rc=%d\n", rc);
	}

	return rc;
}

void oplus_panel_parse_esd_config_post(struct dsi_panel *panel)
{
	int ret = 0;
	struct dsi_parser_utils *utils = &panel->utils;

	panel->esd_config.oplus_esd_cfg.esd_error_flag_gpio = utils->get_named_gpio(utils->data,
			"qcom,error-flag-gpio", 0);
	panel->esd_config.oplus_esd_cfg.esd_error_flag_gpio_slave = utils->get_named_gpio(utils->data,
			"qcom,error-flag-gpio-slave", 0);
	ret = utils->read_u32(utils->data, "qcom,error-flag-gpio-expect-value",
			&panel->esd_config.oplus_esd_cfg.esd_error_flag_expect_value);
	if (ret) {
		OPLUS_DSI_INFO("failed to get qcom,error-flag-gpio-expect-value\n");
		panel->esd_config.oplus_esd_cfg.esd_error_flag_expect_value = 1;
	}
	ret = utils->read_u32(utils->data, "qcom,error-flag-gpio-expect-value-slave",
			&panel->esd_config.oplus_esd_cfg.esd_error_flag_expect_value_slave);
	if (ret) {
		OPLUS_DSI_INFO("failed to get qcom,error-flag-gpio-expect-value-slave\n");
		panel->esd_config.oplus_esd_cfg.esd_error_flag_expect_value_slave = 1;
	}
	DSI_INFO("%s:get esd_error_flag_gpio[%d], esd_error_flag_gpio_slave[%d], esd_error_flag_expect_value[%d], esd_error_flag_expect_value_slave[%d]\n",
			__func__, panel->esd_config.oplus_esd_cfg.esd_error_flag_gpio, panel->esd_config.oplus_esd_cfg.esd_error_flag_gpio_slave,
			panel->esd_config.oplus_esd_cfg.esd_error_flag_expect_value, panel->esd_config.oplus_esd_cfg.esd_error_flag_expect_value_slave);
	return;
}

void oplus_panel_get_pre(struct dsi_panel *panel)
{
	return;
}

int oplus_panel_get_post(struct dsi_panel *panel)
{
	int rc = 0;

	rc = oplus_panel_parse_config(panel);
	if (rc)
		DSI_ERR("failed to parse panel config, rc=%d\n", rc);

	return rc;
}

int oplus_panel_get_mode(struct dsi_display_mode *mode, struct dsi_parser_utils *utils)
{
	int rc = 0;

	rc = oplus_panel_parse_vsync_config(mode, utils);
	if (rc) {
		DSI_ERR("failed to parse vsync params, rc=%d\n", rc);
	}

	return rc;
}

void oplus_dsi_phy_hw_dphy_enable(u32 *glbl_str_swi_cal_sel_ctrl,
				u32 *glbl_hstx_str_ctrl_0)
{
	if (oplus_enhance_mipi_strength) {
		*glbl_str_swi_cal_sel_ctrl = 0x01;
		*glbl_hstx_str_ctrl_0 = 0xFF;
	} else {
		*glbl_str_swi_cal_sel_ctrl = 0x00;
		*glbl_hstx_str_ctrl_0 = 0x88;
	}

	return;
}

void oplus_backlight_setup_pre(struct backlight_properties *props, struct dsi_display *display)
{
	props->brightness = display->panel->oplus_panel.bl_cfg.brightness_default_level;

	return;
}

void oplus_backlight_setup_post(struct dsi_display *display)
{
	if (display->panel->oplus_panel.dc_apollo_sync_enable) {
		init_waitqueue_head(&dc_apollo.bk_wait);
		mutex_init(&dc_apollo.lock);
	}

	return;
}

void oplus_connector_update_dirty_properties(struct sde_connector *c_conn, int idx)
{
	switch (idx) {
	case CONNECTOR_PROP_SYNC_BACKLIGHT_LEVEL:
		if (c_conn) {
			c_conn->oplus_conn.bl_need_sync = true;
		}
		break;
	case CONNECTOR_PROP_SET_OSC_STATUS:
		if (c_conn) {
			c_conn->oplus_conn.osc_need_update = true;
		}
		break;
	default:
			/* nothing to do for most properties */
		break;
	}

	return;
}

void oplus_encoder_off_work(struct sde_encoder_virt *sde_enc)
{
	struct sde_connector *c_conn = NULL;

	if (sde_enc->cur_master && sde_enc->cur_master->connector) {
		c_conn = to_sde_connector(sde_enc->cur_master->connector);
		if (c_conn) {
			oplus_panel_cmdq_sync_count_reset(c_conn);
		}
	}

	return;
}

void oplus_encoder_trigger_start(struct sde_encoder_phys *cur_master)
{
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display = NULL;

	if (!cur_master) {
		OPLUS_DSI_ERR("invalid cur_master params\n");
		return;
	}
	c_conn = to_sde_connector(cur_master->connector);
	if (!c_conn) {
		OPLUS_DSI_ERR("invalid c_conn param\n");
		return;
	}
	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		OPLUS_DSI_INFO("connector not in dsi mode");
		return;
	}
	display = c_conn->display;
	if (!display || !display->panel) {
		OPLUS_DSI_ERR("invalid display param\n");
		return;
	}
	/* sending commands asynchronously, it is necessary to ensure that
		   the next frame mipi sends the image */
	oplus_panel_send_asynchronous_cmd(display);

	return;
}

void oplus_encoder_phys_cmd_te_rd_ptr_irq_pre(struct sde_encoder_phys *phys_enc,
			struct sde_encoder_phys_cmd_te_timestamp *te_timestamp)
{
	struct sde_connector *conn;

	conn = to_sde_connector(phys_enc->connector);
	if (conn && te_timestamp) {
		oplus_save_te_timestamp(conn, te_timestamp->timestamp);
	}

	return;
}

void oplus_encoder_phys_cmd_te_rd_ptr_irq_post(struct sde_encoder_phys *phys_enc)
{
	struct sde_connector *conn;

	conn = to_sde_connector(phys_enc->connector);
	if (conn) {
		oplus_panel_cmdq_sync_count_decrease(conn);
	}

	return;
}

void oplus_setup_dspp_pccv4_pre(struct drm_msm_pcc *pcc_cfg)
{
	if (pcc_cfg)
		dc_apollo.pcc_current = pcc_cfg->r.r;

	return;
}

void oplus_setup_dspp_pccv4_post(struct drm_msm_pcc *pcc_cfg)
{
	static struct drm_msm_pcc *pcc_cfg_last;
	struct dsi_display *display = get_main_display();

	if (display != NULL && display->panel != NULL) {
		if (display->panel->oplus_panel.dc_apollo_sync_enable) {
			mutex_lock(&dc_apollo.lock);
			if (pcc_cfg_last && pcc_cfg) {
				if (dc_apollo.pcc_last != dc_apollo.pcc_current) {
					dc_apollo.pcc_last = dc_apollo.pcc_current;
					dc_apollo.dc_pcc_updated = 1;
				}
			}
			pcc_cfg_last = pcc_cfg;
			mutex_unlock(&dc_apollo.lock);
		}
	}

	return;
}

void oplus_kms_drm_check_dpms_pre(int old_fps, int new_fps)
{
	oplus_check_refresh_rate(old_fps, new_fps);

	return;
}

void oplus_kms_drm_check_dpms_post(struct sde_connector *c_conn, bool is_pre_commit)
{
	struct dsi_display *display;
	struct dsi_panel *panel;

	display = (struct dsi_display *)c_conn->display;
	if (!display) {
		OPLUS_DSI_ERR("Invalid display\n");
		return;
	}

	panel = display->panel;
	if (!panel) {
		OPLUS_DSI_ERR("Invalid panel\n");
		return;
	}

	if (is_pre_commit) {
		panel->oplus_panel.need_trigger_event = false;
	} else {
		panel->oplus_panel.need_trigger_event = true;
	}

	return;
}

void oplus_dsi_message_tx_pre(struct dsi_ctrl *dsi_ctrl, struct dsi_cmd_desc *cmd_desc)
{
	oplus_ctrl_print_cmd_desc(dsi_ctrl, cmd_desc);

	return;
}

void oplus_dsi_message_tx_post(struct dsi_ctrl *dsi_ctrl, struct dsi_cmd_desc *cmd_desc)
{
	return;
}

int oplus_display_validate_status(struct dsi_display *display)
{
	int rc = 0;

	rc = oplus_panel_validate_reg_read(display->panel);

	return rc;
}

void oplus_connector_check_status_work(void *dsi_display)
{
	int rc = 0;
	struct dsi_display *primary_display = get_main_display();

	if (!primary_display || !primary_display->panel) {
		OPLUS_DSI_ERR("primary display or primary_display->panel is null\n");
		return;
	}

	if (!primary_display->panel->oplus_panel.fpga_support) {
		OPLUS_DSI_INFO("panel isn't support fpga\n");
		return;
	}

	primary_display->panel->oplus_panel.skip_panel_recovery = true;
	rc = is_fpga_work_okay();
	if (rc) {
		OPLUS_DSI_ERR("fpga work failed\n");
	} else {
		OPLUS_DSI_INFO("fpga self-check pass\n");
	}
}

int oplus_panel_parse_cmd_sets_sub(struct dsi_panel_cmd_set *cmd, const char *state)
{
	if (!state || !strcmp(state, "dsi_lp_mode")) {
		cmd->state = DSI_CMD_SET_STATE_LP;
		cmd->oplus_cmd_set.sync_count = 0;
	} else if (!strcmp(state, "dsi_hs_mode")) {
		cmd->state = DSI_CMD_SET_STATE_HS;
		cmd->oplus_cmd_set.sync_count = 0;
	} else if (!strcmp(state, "dsi_lp_sync1_mode")) {
		cmd->state = DSI_CMD_SET_STATE_LP;
		cmd->oplus_cmd_set.sync_count = 1;
	} else if (!strcmp(state, "dsi_hs_sync1_mode")) {
		cmd->state = DSI_CMD_SET_STATE_HS;
		cmd->oplus_cmd_set.sync_count = 1;
	} else if (!strcmp(state, "dsi_lp_sync2_mode")) {
		cmd->state = DSI_CMD_SET_STATE_LP;
		cmd->oplus_cmd_set.sync_count = 2;
	} else if (!strcmp(state, "dsi_hs_sync2_mode")) {
		cmd->state = DSI_CMD_SET_STATE_HS;
		cmd->oplus_cmd_set.sync_count = 2;
	} else if (!strcmp(state, "dsi_lp_sync3_mode")) {
		cmd->state = DSI_CMD_SET_STATE_LP;
		cmd->oplus_cmd_set.sync_count = 3;
	} else if (!strcmp(state, "dsi_hs_sync3_mode")) {
		cmd->state = DSI_CMD_SET_STATE_HS;
		cmd->oplus_cmd_set.sync_count = 3;
	} else {
		return -1;
	}

	return 0;
}

void oplus_panel_set_lp1(struct dsi_panel *panel)
{
	panel->oplus_panel.pwm_params.into_aod_timestamp = ktime_get();

	return;
}

void oplus_panel_set_lp2(struct dsi_panel *panel)
{
	return;
}

void oplus_panel_set_nolp_pre(struct dsi_panel *panel)
{
	unsigned int time_interval = 0;
	unsigned int aod_sleep_time = 0;

	DSI_INFO("debug for dsi_panel_set_nolp\n");
	if (panel->oplus_panel.interval_time_nolp_pre) {
		time_interval = ktime_to_us(ktime_sub(ktime_get(), panel->oplus_panel.pwm_params.into_aod_timestamp));
		DSI_DEBUG("aod in and off time_interval us = %d\n", time_interval);
		if (time_interval < INTO_OUT_AOD_INTERVOL) {
			aod_sleep_time = INTO_OUT_AOD_INTERVOL - time_interval;
			usleep_range(aod_sleep_time, aod_sleep_time+100);
		}
	}
	return;
}

void oplus_panel_set_nolp_post(struct dsi_panel *panel)
{
	oplus_panel_set_aod_off_te_timestamp(panel);

	return;
}

void oplus_sde_encoder_handle_framedone_timeout_pre(struct drm_connector *conn)
{
	struct sde_connector *sde_conn = NULL;
	struct dsi_display *display = NULL;
	struct dsi_panel *panel = NULL;

	if (!conn) {
		OPLUS_DSI_ERR("drm_connector is null\n");
		return;
	}
	sde_conn = to_sde_connector(conn);
	if (!sde_conn) {
		OPLUS_DSI_ERR("sde_connector is null\n");
		return;
	}
	display = _sde_connector_get_display(sde_conn);
	if (!display) {
		OPLUS_DSI_ERR("display is null\n");
		return;
	}
	panel = display->panel;
	if (!panel) {
		OPLUS_DSI_ERR("panel is null\n");
		return;
	}

	OPLUS_DSI_ERR("framedone timeout\n");
	oplus_panel_check_fpga(panel);

	return;
}
void oplus_sde_encoder_phys_cmd_wait_for_wr_ptr_pre(struct drm_connector *conn)
{
	struct sde_connector *sde_conn = NULL;
	struct dsi_display *display = NULL;
	struct dsi_panel *panel = NULL;

	if (!conn) {
		OPLUS_DSI_ERR("drm_connector is null\n");
		return;
	}
	sde_conn = to_sde_connector(conn);
	if (!sde_conn) {
		OPLUS_DSI_ERR("sde_connector is null\n");
		return;
	}
	display = _sde_connector_get_display(sde_conn);
	if (!display) {
		OPLUS_DSI_ERR("display is null\n");
		return;
	}
	panel = display->panel;
	if (!panel) {
		OPLUS_DSI_ERR("panel is null\n");
		return;
	}

	OPLUS_DSI_ERR("wait for wr_ptr timeout\n");
	oplus_panel_check_fpga(panel);

	return;
}

void oplus_display_ops_init(struct oplus_display_ops *oplus_display_ops)
{
	DRM_INFO("oplus display ops init\n");

	/* backlight update */
	oplus_display_ops->panel_parse_bl_config_post = oplus_panel_parse_bl_config_post;
	oplus_display_ops->display_set_backlight_pre = oplus_display_set_backlight_pre;
	oplus_display_ops->display_set_backlight_post = oplus_display_set_backlight_post;
	oplus_display_ops->panel_set_backlight_pre = oplus_panel_set_backlight_pre;
	oplus_display_ops->panel_set_backlight_post = oplus_panel_set_backlight_post;
	oplus_display_ops->panel_update_backlight = oplus_panel_update_backlight;
	oplus_display_ops->backlight_setup_pre = oplus_backlight_setup_pre;
	oplus_display_ops->backlight_setup_post = oplus_backlight_setup_post;

	/* commit */
	oplus_display_ops->encoder_kickoff = oplus_encoder_kickoff;
	oplus_display_ops->display_validate_mode_change_pre = oplus_display_validate_mode_change_pre;
	oplus_display_ops->display_validate_mode_change_post = oplus_display_validate_mode_change_post;
	oplus_display_ops->dsi_phy_hw_dphy_enable = oplus_dsi_phy_hw_dphy_enable;
	oplus_display_ops->connector_update_dirty_properties = oplus_connector_update_dirty_properties;
	oplus_display_ops->encoder_off_work = oplus_encoder_off_work;
	oplus_display_ops->encoder_trigger_start = oplus_encoder_trigger_start;
	oplus_display_ops->encoder_phys_cmd_te_rd_ptr_irq_pre = oplus_encoder_phys_cmd_te_rd_ptr_irq_pre;
	oplus_display_ops->encoder_phys_cmd_te_rd_ptr_irq_post = oplus_encoder_phys_cmd_te_rd_ptr_irq_post;
	oplus_display_ops->setup_dspp_pccv4_pre = oplus_setup_dspp_pccv4_pre;
	oplus_display_ops->setup_dspp_pccv4_post = oplus_setup_dspp_pccv4_post;
	oplus_display_ops->kms_drm_check_dpms_pre = oplus_kms_drm_check_dpms_pre;
	oplus_display_ops->kms_drm_check_dpms_post = oplus_kms_drm_check_dpms_post;

	/* power on */
	oplus_display_ops->bridge_pre_enable = oplus_bridge_pre_enable;
	oplus_display_ops->display_enable_pre = oplus_display_enable_pre;
	oplus_display_ops->display_enable_mid = oplus_display_enable_mid;
	oplus_display_ops->display_enable_post = oplus_display_enable_post;
	oplus_display_ops->panel_enable_pre = oplus_panel_enable_pre;
	oplus_display_ops->panel_enable_post = oplus_panel_enable_post;
	oplus_display_ops->panel_init = oplus_panel_enable_init;
	oplus_display_ops->panel_set_pinctrl_state = oplus_panel_set_pinctrl_state;
	oplus_display_ops->panel_prepare = oplus_panel_prepare;
	oplus_display_ops->panel_power_on = oplus_panel_power_on;
	oplus_display_ops->panel_pre_prepare = oplus_panel_pre_prepare;

	/* power off */
	oplus_display_ops->display_disable_post = oplus_display_disable_post;
	oplus_display_ops->panel_disable_post = oplus_panel_disable_post;
	oplus_display_ops->panel_power_off = oplus_panel_power_off;

	/* timing switch */
	oplus_display_ops->panel_switch_pre = oplus_panel_switch_pre;
	oplus_display_ops->panel_switch_post = oplus_panel_switch_post;

	/* esd */
	oplus_display_ops->panel_parse_esd_reg_read_configs_post = oplus_panel_parse_esd_reg_read_configs_post;
	oplus_display_ops->panel_parse_esd_config_post = oplus_panel_parse_esd_config_post;
	oplus_display_ops->display_read_status = oplus_display_read_status;
	oplus_display_ops->display_check_status_pre = oplus_display_check_status_pre;
	oplus_display_ops->display_check_status_post = oplus_display_check_status_post;
	oplus_display_ops->display_validate_status = oplus_display_validate_status;
	oplus_display_ops->connector_check_status_work = oplus_connector_check_status_work;

	/* starting up/down */
	oplus_display_ops->display_parse_cmdline_topology = oplus_display_parse_cmdline_topology;
	oplus_display_ops->display_res_init = oplus_display_res_init;
	oplus_display_ops->display_dev_probe = oplus_display_dev_probe;
	oplus_display_ops->display_register = oplus_display_register;
	oplus_display_ops->panel_get_pre = oplus_panel_get_pre;
	oplus_display_ops->panel_get_post = oplus_panel_get_post;
	oplus_display_ops->panel_get_mode = oplus_panel_get_mode;
	oplus_display_ops->display_bind_pre = oplus_display_bind_pre;
	oplus_display_ops->display_bind_post = oplus_display_bind_post;
	oplus_display_ops->panel_pinctrl_init = oplus_panel_pinctrl_init;
	oplus_display_ops->panel_parse_gpios = oplus_panel_parse_gpios;
	oplus_display_ops->panel_gpio_request = oplus_panel_gpio_request;
	oplus_display_ops->panel_gpio_release = oplus_panel_gpio_release;
	oplus_display_ops->display_unbind = oplus_display_unbind;

	/* tx cmd */
	oplus_display_ops->panel_tx_cmd_set_pre = oplus_panel_tx_cmd_set_pre;
	oplus_display_ops->panel_tx_cmd_set_mid = oplus_panel_tx_cmd_set_mid;
	oplus_display_ops->panel_tx_cmd_set_post = oplus_panel_tx_cmd_set_post;
	oplus_display_ops->dsi_message_tx_pre = oplus_dsi_message_tx_pre;
	oplus_display_ops->dsi_message_tx_post = oplus_dsi_message_tx_post;
	oplus_display_ops->panel_parse_cmd_sets_sub = oplus_panel_parse_cmd_sets_sub;

	/* aod */
	oplus_display_ops->panel_set_lp1 = oplus_panel_set_lp1;
	oplus_display_ops->panel_set_lp2 = oplus_panel_set_lp2;
	oplus_display_ops->panel_set_nolp_pre = oplus_panel_set_nolp_pre;
	oplus_display_ops->panel_set_nolp_post = oplus_panel_set_nolp_post;

	oplus_display_ops->wait_for_wr_ptr_pre = oplus_sde_encoder_phys_cmd_wait_for_wr_ptr_pre;
	oplus_display_ops->handle_framedone_timeout_pre = oplus_sde_encoder_handle_framedone_timeout_pre;

#ifdef OPLUS_FEATURE_TP_BASIC /* tp notifier */
	oplus_display_notify_tp_ops_init(&oplus_display_notify_tp_ops);
#endif /* OPLUS_FEATURE_TP_BASIC */
}
