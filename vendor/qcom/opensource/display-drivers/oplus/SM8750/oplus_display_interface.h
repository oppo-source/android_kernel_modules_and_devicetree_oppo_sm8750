/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_interface.h
** Description : oplus display interface
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/

#ifndef __OPLUS_DISPLAY_INTERFACE_H__
#define __OPLUS_DISPLAY_INTERFACE_H__
#include "dsi_ctrl.h"
#include <linux/soc/qcom/panel_event_notifier.h>
#include "dsi_panel.h"
#include "dsi_defs.h"
#include "sde_encoder_phys.h"
#include "oplus_display_utils.h"
#include "oplus_display_sysfs_attrs.h"

#define INTO_OUT_AOD_INTERVOL (45*1000)

struct oplus_display_ops {
	/* backlight update */
	int (*panel_parse_bl_config_post)(struct dsi_panel *panel);
	void (*display_set_backlight_pre)(struct dsi_display *display, int *bl_lvl, int brightness);
	void (*display_set_backlight_post)(struct sde_connector *c_conn, struct dsi_display *display, struct drm_event *event, int brightness, int bl_lvl);
	void (*panel_set_backlight_pre)(struct dsi_display *display, int *bl_lvl);
	void (*panel_set_backlight_post)(struct dsi_panel *panel, u64 bl_lvl);
	void (*panel_update_backlight)(struct dsi_panel *panel,
		struct mipi_dsi_device *dsi, u32 bl_lvl);
	void (*backlight_setup_pre)(struct backlight_properties *props, struct dsi_display *display);
	void (*backlight_setup_post)(struct dsi_display *display);

	/* commit */
	void (*encoder_kickoff)(struct drm_encoder *drm_enc, struct sde_encoder_virt *sde_enc);
	void (*display_validate_mode_change_pre)(struct dsi_display *display);
	void (*display_validate_mode_change_post)(struct dsi_display *dsi_display,
			struct dsi_display_mode *cur_mode,
			struct dsi_display_mode *adj_mode);
	void (*dsi_phy_hw_dphy_enable)(u32 *glbl_str_swi_cal_sel_ctrl, u32 *glbl_hstx_str_ctrl_0);
	void (*connector_update_dirty_properties)(struct sde_connector *c_conn, int idx);
	void (*encoder_off_work)(struct sde_encoder_virt *sde_enc);
	void (*encoder_trigger_start)(struct sde_encoder_phys *cur_master);
	void (*encoder_phys_cmd_te_rd_ptr_irq_pre)(struct sde_encoder_phys *phys_enc,
			struct sde_encoder_phys_cmd_te_timestamp *te_timestamp);
	void (*encoder_phys_cmd_te_rd_ptr_irq_post)(struct sde_encoder_phys *phys_enc);
	void (*setup_dspp_pccv4_pre)(struct drm_msm_pcc *pcc_cfg);
	void (*setup_dspp_pccv4_post)(struct drm_msm_pcc *pcc_cfg);
	void (*kms_drm_check_dpms_pre)(int old_fps, int new_fps);
	void (*kms_drm_check_dpms_post)(struct sde_connector *c_conn, bool is_pre_commit);

	/* power on */
	void (*bridge_pre_enable)(struct dsi_display *display, struct dsi_display_mode *mode);
	void (*display_enable_pre)(struct dsi_display *display);
	void (*display_enable_mid)(struct dsi_display *display);
	void (*display_enable_post)(struct dsi_display *display);
	int (*panel_enable_pre)(struct dsi_panel *panel);
	int (*panel_enable_post)(struct dsi_panel *panel);
	void (*panel_init)(struct dsi_panel *panel);
	int (*panel_set_pinctrl_state)(struct dsi_panel *panel, bool enable);
	int (*panel_power_on)(struct dsi_panel *panel);
	int (*panel_prepare)(struct dsi_panel *panel);
	bool (*panel_pre_prepare)(struct dsi_panel *panel);

	/* power off */
	void (*display_disable_post)(struct dsi_display *display);
	void (*panel_disable_pre)(struct dsi_panel *panel);
	void (*panel_disable_post)(struct dsi_panel *panel);
	int (*panel_power_off)(struct dsi_panel *panel);

	/* timing switch */
	void (*panel_switch_pre)(struct dsi_panel *panel);
	void (*panel_switch_post)(struct dsi_panel *panel);

	/* esd */
	int (*panel_parse_esd_reg_read_configs_post)(struct dsi_panel *panel);
	void (*panel_parse_esd_config_post)(struct dsi_panel *panel);
	int (*display_read_status)(struct dsi_panel *panel);
	bool (*display_check_status_pre)(struct dsi_panel *panel);
	int (*display_check_status_post)(struct dsi_display *display);
	int (*display_validate_status)(struct dsi_display *display);
	void (*connector_check_status_work)(void *dsi_display);

	/* starting up/down */
	void (*display_dev_probe)(struct dsi_display *display);
	void (*display_register)(void);
	int (*display_parse_cmdline_topology)(struct dsi_display *display,
			char *boot_str, unsigned int display_type);
	void (*display_res_init)(struct dsi_display *display);
	void (*display_bind_pre)(struct dsi_display *display);
	void (*display_bind_post)(struct dsi_display *display);
	void (*display_unbind)(struct dsi_display *display);
	int (*panel_pinctrl_init)(struct dsi_panel *panel);
	int (*panel_parse_gpios)(struct dsi_panel *panel);
	void (*panel_get_pre)(struct dsi_panel *panel);
	int (*panel_get_post)(struct dsi_panel *panel);
	int (*panel_get_mode)(struct dsi_display_mode *mode, struct dsi_parser_utils *utils);
	int (*panel_gpio_request)(struct dsi_panel *panel);
	int (*panel_gpio_release)(struct dsi_panel *panel);

	/* tx cmd */
	void (*panel_tx_cmd_set_pre)(struct dsi_panel *panel,
				enum dsi_cmd_set_type *type);
	void (*panel_tx_cmd_set_mid)(struct dsi_panel *panel,
				enum dsi_cmd_set_type type);
	void (*panel_tx_cmd_set_post)(struct dsi_panel *panel,
				enum dsi_cmd_set_type type, int rc);
	void (*dsi_message_tx_pre)(struct dsi_ctrl *dsi_ctrl, struct dsi_cmd_desc *cmd_desc);
	void (*dsi_message_tx_post)(struct dsi_ctrl *dsi_ctrl, struct dsi_cmd_desc *cmd_desc);
	int (*panel_parse_cmd_sets_sub)(struct dsi_panel_cmd_set *cmd, const char *state);

	/* aod */
	void (*panel_set_lp1)(struct dsi_panel *panel);
	void (*panel_set_lp2)(struct dsi_panel *panel);
	void (*panel_set_nolp_pre)(struct dsi_panel *panel);
	void (*panel_set_nolp_post)(struct dsi_panel *panel);

	void (*wait_for_wr_ptr_pre)(struct drm_connector *conn);
	void (*handle_framedone_timeout_pre)(struct drm_connector *conn);
};

extern struct oplus_display_ops oplus_display_ops;
#ifdef OPLUS_FEATURE_TP_BASIC
extern struct oplus_display_notify_tp_ops oplus_display_notify_tp_ops;
#endif /* OPLUS_FEATURE_TP_BASIC */
#endif /* __OPLUS_DISPLAY_INTERFACE_H__ */
