/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_panel.h
** Description : oplus panel header
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_PANEL_H_
#define _OPLUS_PANEL_H_
#include <linux/soc/qcom/panel_event_notifier.h>
#define MAX_PWM_CMD 16
#define MAX_MIPI_ERR_REG_COUNT 32

/* In 120hz general solution, L1, L2, L3 means 1 Pulse 3 Pulse and 18 Pulse */
enum PWM_STATE {
	PWM_STATE_L1 = 0,
	PWM_STATE_L2,
	PWM_STATE_L3,
	PWM_STATE_MAXNUM,
};

enum PWM_SWITCH_STATE {
	PWM_SWITCH_MODE0 = 0,
	PWM_SWITCH_MODE1,
	PWM_SWITCH_MODE2,
	PWM_SWITCH_MODE_MAX,
};

enum oplus_panel_features {
	/* Append new panle features before OPLUS_PANEL_MAX_FEATURES */
	/* Oplus Features start */
	OPLUS_PANLE_DP_SUPPORT,
	OPLUS_PANLE_CABC_SUPPORT,
	OPLUS_PANLE_SERIAL_NUM_SUPPORT,
	OPLUS_PANLE_DC_BACKLIGHT_SUPPORT,
	/* Oplus Features end */
	OPLUS_PANEL_MAX_FEATURES,
};

struct oplus_brightness_alpha {
	u32 brightness;
	u32 alpha;
};

struct oplus_clk_osc {
	u32 clk_rate;
	u32 osc_rate;
};

/***  pwm turbo initialize params dtsi config   *****************
oplus,pwm-turbo-support;
oplus,pwm-turbo-plus-dbv=<0x643>;   			mtk config
oplus,pwm-turbo-wait-te=<1>;
oplus,pwm-switch-backlight-threshold=<0x643>;   qcom config
********************************************************/
/* oplus pwm turbo initialize params ***************/
struct oplus_pwm_turbo_params {
	unsigned int pwm_config;									/* bit(0):enable or disabled, bit(1) */
	unsigned int pwm_dbv_threshold;								/* qcom switch bl plus oplus,pwm-switch-backlight-threshold */
	unsigned int pwm_mode0_states[PWM_STATE_MAXNUM];
	unsigned int pwm_mode1_states[PWM_STATE_MAXNUM];
	unsigned int pwm_mode2_states[PWM_STATE_MAXNUM];
	unsigned int pwm_mode0_thresholds[PWM_STATE_MAXNUM - 1];
	unsigned int pwm_mode1_thresholds[PWM_STATE_MAXNUM - 1];
	unsigned int pwm_mode2_thresholds[PWM_STATE_MAXNUM - 1];
	unsigned int pwm_mode0_states_count;
	unsigned int pwm_mode1_states_count;
	unsigned int pwm_mode2_states_count;
	unsigned int pwm_mode_count;
	unsigned int pwm_switch_state;
	unsigned int pwm_pulse_state;
	unsigned int pwm_pulse_state_last;
	bool pwm_compatible_mode;
	bool pwm_power_on;
	bool pwm_hbm_state;
	bool pwm_state_changed;
	int pwm_cmd_replace_map[2][MAX_PWM_CMD];                      /* Mapping table of cmds unique to PWM case to original cmds  */
	int pwm_cmd_replace_map_count;
	ktime_t aod_off_timestamp;
	struct workqueue_struct *oplus_pwm_dbv_ext_cmd_wq;
	struct work_struct oplus_pwm_dbv_ext_cmd_work;

	ktime_t into_aod_timestamp;
};

struct oplus_backlight_config {
	u32 bl_normal_max_level;
	u32 brightness_normal_max_level;
	u32 brightness_default_level;
	u32 dc_backlight_threshold;
	bool oplus_dc_mode;
	u32 global_hbm_case_id;
	u32 global_hbm_threshold;
	bool global_hbm_scale_mapping;
	bool oplus_limit_max_bl_mode;
	u32 oplus_limit_max_bl;
	bool oplus_demura2_offset_support;
	bool need_to_set_demura2_offset;
	u32 demura2_offset;
	bool backlight_check_disable;
};

/*
 *  dsi_panel.h/dsi_pinctrl_info
 */
struct oplus_pinctrl_info{
	struct pinctrl_state *oplus_panel_active;
	struct pinctrl_state *oplus_panel_suspend;
};

struct oplus_gpio_config{
	int panel_gpio1;
	int panel_gpio2;
	int panel_gpio3;
	int pmic_gpio;
};

struct oplus_serial_number {
	bool serial_number_support;
	bool is_switch_page;
	u32 serial_number_reg;
	int serial_number_index;
	int serial_number_conut;
};

struct oplus_btb_sn {
	bool btb_sn_support;
	bool is_reg_lock;
	bool is_switch_page;
	u32 btb_sn_reg;
	int btb_sn_index;
	int btb_sn_conut;
};

struct oplus_drm_panel_esd_config{
	u32 status_match_modes;
	int esd_error_flag_gpio;
	int esd_error_flag_gpio_slave;
	int esd_error_flag_expect_value;
	int esd_error_flag_expect_value_slave;
};

/**
 * oplus_panel_mipi_err_check_config mipi err check config struct
 */
struct oplus_panel_mipi_err_check_config {
	u32 config;
	char check_regs[MAX_MIPI_ERR_REG_COUNT];
	u8 check_cmds_rlen[MAX_MIPI_ERR_REG_COUNT];
	u32 reg_count;
	u8 *check_value;
	u8 *return_buf;
	u8 *check_buf;
	u32 groups;
	u32 match_modes;
};

struct oplus_panel_cmd_set{
	int sync_count;
};

struct oplus_display_mode_priv_info{
	u32 vsync_width;
	u32 vsync_period;
	u32 async_bl_delay;
	u32 refresh_rate;
};

struct oplus_panel {
	/* ---------------- substurcture variate ---------------- */
	struct oplus_pinctrl_info pinctrl_info;
	struct oplus_backlight_config bl_cfg;
	struct oplus_gpio_config gpio_cfg;
	struct oplus_serial_number serial_number;
	struct oplus_btb_sn btb_sn;

	/* ---------------- common variate ---------------- */
	const char *vendor_name;
	const char *manufacture_name;
	bool is_pxlw_iris5;
	bool iris_pw_enable;
	int iris_pw_rst_gpio;
	int iris_pw_0p9_en_gpio;
	bool gpio_pre_on;
	bool panel_id_switch_page;
	bool pinctrl_enabled;
	bool enhance_mipi_strength;
	bool oplus_vreg_ctrl_flag;
	/* add for all wait te demand */
	u32 wait_te_config;
	bool change_voltage_before_panel_bl_0;
	bool interval_time_nolp_pre;

	/* ---------------- feature variate ---------------- */
	bool dp_support;
	bool cabc_enabled;
	bool dre_enabled;
	bool ffc_enabled;
	u32 ffc_delay_frames;
	u32 ffc_mode_count;
	u32 ffc_mode_index;
	struct oplus_clk_osc *clk_osc_seq;
	u32 clk_rate_cur;
	u32 osc_rate_cur;
	bool is_osc_support;
	u32 osc_clk_mode0_rate;
	u32 osc_clk_mode1_rate;
	bool is_apl_read_support;

	/* ---------------- apollo variate ---------------- */
	bool is_apollo_support;
	bool skip_mipi_last_cmd;
	u32 sync_brightness_level;
	int bl_remap_count;
	struct oplus_brightness_alpha *bl_remap;
	bool dc_apollo_sync_enable;
	u32 dc_apollo_sync_brightness_level;
	u32 dc_apollo_sync_brightness_level_pcc;
	u32 dc_apollo_sync_brightness_level_pcc_min;

	/* add for panel id compatibility*/
	bool panel_init_compatibility_enable;
	u32 hbm_max_state;
	bool cmdq_sync_support;
	int cmdq_sync_count;

	bool need_sync;
	u32 disable_delay_bl_count;
	bool oplus_bl_demura_dbv_support;
	int bl_demura_mode;
	bool vid_timming_switch_enabled;

	bool need_power_on_backlight;
	struct oplus_brightness_alpha *dc_ba_seq;
	int dc_ba_count;
	atomic_t esd_pending;

	struct mutex panel_tx_lock;
	struct mutex oplus_ffc_lock;
	ktime_t te_timestamp;
	ktime_t ts_timestamp;

	struct oplus_pwm_turbo_params pwm_params;
	u32 last_us_per_frame;
	u32 last_vsync_width;
	u32 last_refresh_rate;
	u32 work_frame;
	bool bl_ic_ktz8868_used;
	bool need_trigger_event;

	unsigned int power_on_sequence[7][2];
	unsigned int power_off_sequence[7][2];
	unsigned int panel_reset_position;
	/*add for mipi err check */
	struct oplus_panel_mipi_err_check_config mipi_err_config;

	bool fpga_support;
	bool fpga_reset_completed;
	bool skip_panel_recovery;

	bool gamma_compensation_support;
	int power_mode_early;
};

#endif /* _OPLUS_PANEL_H_ */
