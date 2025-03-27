/***************************************************************
** Copyright (C),  2024,  OPLUS Mobile Comm Corp.,  Ltd
** File : oplus_display_pwm.h
** Description : oplus high frequency PWM
** Version : 1.0
** Date : 2024/05/09
**
** ------------------------------- Revision History: -----------
**  <author>        <data>        <version >        <desc>
**  Li.Ping      2023/07/11        1.0           Build this moudle
******************************************************************/

#ifndef _OPLUS_DISPLAY_PWM_H_
#define _OPLUS_DISPLAY_PWM_H_

/* please just only include linux common head file  */
#include <linux/err.h>
#include "dsi_panel.h"

enum oplus_pwm_turbo_log_level {
	OPLUS_PWM_TURBO_LOG_LEVEL_ERR = 0,
	OPLUS_PWM_TURBO_LOG_LEVEL_WARN = 1,
	OPLUS_PWM_TURBO_LOG_LEVEL_INFO = 2,
	OPLUS_PWM_TURBO_LOG_LEVEL_DEBUG = 3,
};

/* -------------------- extern ---------------------------------- */
extern unsigned int oplus_display_log_level;


/* -------------------- pwm turbo debug log-------------------------------------------  */
#define OPLUS_PWM_ERR(fmt, arg...)	\
	do {	\
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_ERR)	\
			pr_err("[PWM_SWITCH][ERR][%s:%d]"pr_fmt(fmt), __func__, __LINE__, ##arg);	\
	} while (0)

#define OPLUS_PWM_WARN(fmt, arg...)	\
	do {	\
		pr_warn("[PWM_SWITCH][WARN][%s:%d]"pr_fmt(fmt), __func__, __LINE__, ##arg);	\
	} while (0)

#define OPLUS_PWM_INFO(fmt, arg...)	\
	do { \
		if(1) \
			pr_info("[PWM_SWITCH][INFO][%s:%d]"pr_fmt(fmt), __func__, __LINE__, ##arg);	\
	} while (0)

#define OPLUS_PWM_DEBUG(fmt, arg...)	\
	do {	\
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_DEBUG || (oplus_display_log_type & OPLUS_DEBUG_LOG_DSI))	 \
			pr_info("[PWM_SWITCH][DEBUG][%s:%d]"pr_fmt(fmt), __func__, __LINE__, ##arg);	\
	} while (0)

/* -------------------- function declaration ---------------------------------------- */
bool oplus_panel_pwm_support(struct dsi_panel *panel);
bool oplus_panel_pwm_switch_support(struct dsi_panel *panel);
bool oplus_panel_pwm_dbv_threshold_cmd_enabled(struct dsi_panel *panel);
bool oplus_panel_pwm_dbv_cmd_wait_te(struct dsi_panel *panel);
bool oplus_panel_pwm_cmd_replace_enabled(struct dsi_panel *panel);
bool oplus_panel_pwm_panel_on_ext_cmd_support(struct dsi_panel *panel);
bool oplus_panel_pwm_timing_switch_ext_cmd_enabled(struct dsi_panel *panel);
int oplus_panel_pwm_get_state(struct dsi_panel *panel);
int oplus_panel_pwm_get_state_last(struct dsi_panel *panel);
int oplus_panel_pwm_get_switch_state(struct dsi_panel *panel);
int get_state_by_dbv(struct dsi_panel *panel, u32 dbv, enum PWM_STATE *cur_state);
int oplus_panel_parse_pwm_config(struct dsi_panel *panel);
int oplus_panel_pwm_switch_timing_switch(struct dsi_panel *panel);
int oplus_panel_pwm_panel_on_handle(struct dsi_panel *panel);
void oplus_panel_pwm_cmd_replace_handle(struct dsi_panel *panel, enum dsi_cmd_set_type *type);
int oplus_panel_pwm_dbv_threshold_handle(struct dsi_panel *panel, u32 backlight_level);
void oplus_pwm_dbv_ext_cmd_work_handler(struct work_struct *work);
int oplus_panel_pwm_compatible_mode_dbv_threshold_handle(struct dsi_panel *panel, u32 backlight_level);
int oplus_panel_send_pwm_switch_dcs_unlock(struct dsi_panel *panel);
int oplus_panel_update_pwm_pulse_lock(struct dsi_panel *panel, uint32_t mode);
int oplus_display_panel_set_pwm_pulse(void *data);
int oplus_display_panel_get_pwm_pulse(void *data);
inline bool oplus_panel_pwm_turbo_switch_state(struct dsi_panel *panel);
inline bool oplus_panel_pwm_turbo_is_enabled(struct dsi_panel *panel);
int oplus_panel_update_pwm_turbo_lock(struct dsi_panel *panel, uint32_t mode);
int oplus_display_panel_set_pwm_turbo(void *data);
int oplus_display_panel_get_pwm_turbo(void *data);
int oplus_pwm_set_power_on(struct dsi_panel *panel);
int oplus_hbm_pwm_state(struct dsi_panel *panel, bool hbm_state);
void oplus_panel_set_aod_off_te_timestamp(struct dsi_panel *panel);
/* -------------------- oplus api nodes ----------------------------------------------- */
ssize_t oplus_get_pwm_turbo_debug(struct kobject *obj, struct kobj_attribute *attr, char *buf);
ssize_t oplus_set_pwm_turbo_debug(struct kobject *obj, struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t oplus_get_pwm_pulse_debug(struct kobject *obj, struct kobj_attribute *attr, char *buf);
ssize_t oplus_set_pwm_pulse_debug(struct kobject *obj, struct kobj_attribute *attr, const char *buf, size_t count);
/* config */

#endif /* _OPLUS_DISPLAY_PWM_H_ */
