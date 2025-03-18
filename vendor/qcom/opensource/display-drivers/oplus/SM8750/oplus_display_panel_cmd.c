/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_panel_cmd.c
** Description : oplus display panel cmd feature
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#include "sde_connector.h"
#include "oplus_display_panel_cmd.h"
#include "oplus_display_pwm.h"
#include "oplus_display_ext.h"
#include "oplus_display_bl.h"
#include "oplus_display_device_ioctl.h"
#include "oplus_debug.h"
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
#include "oplus_adfr.h"
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

#define BACKLIGHT_CACHE_MAX 50

extern u32 bl_lvl;
extern struct panel_id panel_id;

const char *cmd_set_prop_map[DSI_CMD_SET_MAX] = {
	"qcom,mdss-dsi-pre-on-command",
	"qcom,mdss-dsi-on-command",
	"qcom,vid-on-commands",
	"qcom,cmd-on-commands",
	"qcom,mdss-dsi-post-panel-on-command",
	"qcom,mdss-dsi-pre-off-command",
	"qcom,mdss-dsi-off-command",
	"qcom,mdss-dsi-post-off-command",
	"qcom,mdss-dsi-pre-res-switch",
	"qcom,mdss-dsi-res-switch",
	"qcom,mdss-dsi-post-res-switch",
	"qcom,video-mode-switch-in-commands",
	"qcom,video-mode-switch-out-commands",
	"qcom,cmd-mode-switch-in-commands",
	"qcom,cmd-mode-switch-out-commands",
	"qcom,mdss-dsi-panel-status-command",
	"qcom,mdss-dsi-lp1-command",
	"qcom,mdss-dsi-lp2-command",
	"qcom,mdss-dsi-nolp-command",
	"PPS not parsed from DTSI, generated dynamically",
	"ROI not parsed from DTSI, generated dynamically",
	"qcom,mdss-dsi-timing-switch-command",
	"qcom,mdss-dsi-post-mode-switch-on-command",
	"qcom,mdss-dsi-qsync-on-commands",
	"qcom,mdss-dsi-qsync-off-commands",
	"qcom,mdss-dsi-esync-post-on-commands",
	"qcom,mdss-dsi-arp_mode3_hw_te_on-command",
	"qcom,mdss-dsi-arp_mode1_hw_te_off-command",
	"qcom,mdss-dsi-freq-step-pattern1-command",
	"qcom,mdss-dsi-freq-step-pattern2-command",
	"qcom,mdss-dsi-freq-step-pattern3-command",
	"qcom,mdss-dsi-freq-step-pattern4-command",
	"qcom,mdss-dsi-freq-step-pattern5-command",
	"qcom,mdss-dsi-sticky_still_en-command",
	"qcom,mdss-dsi-sticky_still_disable-command",
	"qcom,mdss-dsi-sticky_on_fly-command",
	"qcom,mdss-dsi-trigger_self_refresh-command",
	"qcom,mdss-dsi-fps-switch-command",
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	"qcom,mdss-dsi-adfr-auto-on-command",
	"qcom,mdss-dsi-adfr-auto-off-command",
	"qcom,mdss-dsi-adfr-min-fps-0-command",
	"qcom,mdss-dsi-adfr-min-fps-1-command",
	"qcom,mdss-dsi-adfr-min-fps-2-command",
	"qcom,mdss-dsi-adfr-min-fps-3-command",
	"qcom,mdss-dsi-adfr-min-fps-4-command",
	"qcom,mdss-dsi-adfr-min-fps-5-command",
	"qcom,mdss-dsi-adfr-min-fps-6-command",
	"qcom,mdss-dsi-adfr-min-fps-7-command",
	"qcom,mdss-dsi-adfr-min-fps-8-command",
	"qcom,mdss-dsi-adfr-min-fps-9-command",
	"qcom,mdss-dsi-adfr-min-fps-10-command",
	"qcom,mdss-dsi-adfr-min-fps-11-command",
	"qcom,mdss-dsi-adfr-min-fps-12-command",
	"qcom,mdss-dsi-adfr-min-fps-13-command",
	"qcom,mdss-dsi-adfr-min-fps-14-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-0-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-1-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-2-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-3-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-4-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-5-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-6-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-7-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-8-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-9-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-10-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-11-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-12-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-13-command",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-14-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-0-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-1-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-2-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-3-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-4-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-5-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-6-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-7-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-8-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-9-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-10-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-11-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-12-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-13-command",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-14-command",
	"qcom,mdss-dsi-adfr-pre-switch-command",
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */
#ifdef OPLUS_FEATURE_DISPLAY_HIGH_PRECISION
	"qcom,mdss-dsi-adfr-high-precision-fps-0-command",
	"qcom,mdss-dsi-adfr-high-precision-fps-1-command",
	"qcom,mdss-dsi-adfr-high-precision-fps-2-command",
	"qcom,mdss-dsi-adfr-high-precision-fps-3-command",
	"qcom,mdss-dsi-hpwm-adfr-high-precision-fps-0-command",
	"qcom,mdss-dsi-hpwm-adfr-high-precision-fps-1-command",
	"qcom,mdss-dsi-hpwm-adfr-high-precision-fps-2-command",
	"qcom,mdss-dsi-hpwm-adfr-high-precision-fps-3-command",
	"qcom,mdss-dsi-bigdc-adfr-high-precision-fps-0-command",
	"qcom,mdss-dsi-bigdc-adfr-high-precision-fps-1-command",
	"qcom,mdss-dsi-bigdc-adfr-high-precision-fps-2-command",
	"qcom,mdss-dsi-bigdc-adfr-high-precision-fps-3-command",
	"qcom,mdss-dsi-adfr-high-precision-te-shift-on-command",
	"qcom,mdss-dsi-adfr-high-precision-te-shift-off-command",
#endif /* OPLUS_FEATURE_DISPLAY_HIGH_PRECISION */
#ifdef OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION
	"qcom,mdss-dsi-read-temp-compensation-reg-command",
	"qcom,mdss-dsi-temperature-compensation-command",
#endif /* OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION */
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	"qcom,mdss-dsi-hbm-on-command",
	"qcom,mdss-dsi-hbm-on-onepulse-command",
	"qcom,mdss-dsi-hbm-off-command",
	"qcom,mdss-dsi-lhbm-pressed-icon-gamma-command",
	"qcom,mdss-dsi-lhbm-pressed-icon-grayscale-command",
	"qcom,mdss-dsi-lhbm-pressed-icon-on-command",
	"qcom,mdss-dsi-lhbm-pressed-icon-off-command",
	"qcom,mdss-dsi-lhbm-update-vdc-command",
	"qcom,mdss-dsi-lhbm-dbv-alpha-command",
	"qcom,mdss-dsi-aor-on-command",
	"qcom,mdss-dsi-aor-off-command",
	"qcom,mdss-dsi-aod-high-mode-command",
	"qcom,mdss-dsi-aod-low-mode-command",
	"qcom,mdss-dsi-ultra-low-power-aod-on-command",
	"qcom,mdss-dsi-ultra-low-power-aod-off-command",
	"qcom,mdss-dsi-aod-on-1pulse-command",
	"qcom,mdss-dsi-aod-off-1pulse-command",
	"qcom,mdss-dsi-aod-off-compensation-command",
	"qcom,mdss-dsi-aod-off-compensation-onepulse-command",
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#ifdef OPLUS_FEATURE_DISPLAY
	"qcom,mdss-dsi-post-on-backlight",
	"qcom,mdss-dsi-seed-0-command",
	"qcom,mdss-dsi-seed-1-command",
	"qcom,mdss-dsi-seed-2-command",
	"qcom,mdss-dsi-seed-3-command",
	"qcom,mdss-dsi-seed-4-command",
	"qcom,mdss-dsi-no-seed-native-command",
	"qcom,mdss-dsi-seed-off-command",
	"qcom,mdss-dsi-spr-0-command",
	"qcom,mdss-dsi-spr-1-command",
	"qcom,mdss-dsi-spr-2-command",
	"qcom,mdss-dsi-data-dimming-on-command",
	"qcom,mdss-dsi-data-dimming-off-command",
	"qcom,mdss-dsi-osc-clk-mode0-command",
	"qcom,mdss-dsi-osc-clk-mode1-command",
	"qcom,mdss-dsi-osc-track-on-command",
	"qcom,mdss-dsi-osc-track-off-command",
	"qcom,mdss-dsi-ffc-mode0-command",
	"qcom,mdss-dsi-ffc-mode1-command",
	"qcom,mdss-dsi-ffc-mode2-command",
	"qcom,mdss-dsi-ffc-mode3-command",
	"qcom,mdss-dsi-panel-id1-command",
	"qcom,mdss-dsi-panel-read-register-open-command",
	"qcom,mdss-dsi-panel-read-register-close-command",
	"qcom,mdss-dsi-loading-effect-1-command",
	"qcom,mdss-dsi-loading-effect-2-command",
	"qcom,mdss-dsi-loading-effect-off-command",
	"qcom,mdss-dsi-hbm-enter-switch-command",
	"qcom,mdss-dsi-hbm-exit-switch-command",
	"qcom,mdss-dsi-hbm-max-command",
	"qcom,mdss-dsi-hbm-exit-max-command",
	"qcom,mdss-dsi-change-voltage-before-bl-0-command",
	"qcom,mdss-dsi-pwm-switch-mode0-command",
	"qcom,mdss-dsi-pwm-switch-mode1-command",
	"qcom,mdss-dsi-pwm-switch-mode2-command",
	"qcom,mdss-dsi-pwm-state-l1tol2-command",
	"qcom,mdss-dsi-pwm-state-l1tol3-command",
	"qcom,mdss-dsi-pwm-state-l2tol1-command",
	"qcom,mdss-dsi-pwm-state-l2tol3-command",
	"qcom,mdss-dsi-pwm-state-l3tol1-command",
	"qcom,mdss-dsi-pwm-state-l3tol2-command",
	"qcom,mdss-dsi-pwm-switch-mode0-panel-on-command",
	"qcom,mdss-dsi-pwm-switch-mode1-panel-on-command",
	"qcom,mdss-dsi-pwm-switch-mode2-panel-on-command",
	"qcom,mdss-dsi-pwm-timing-switch-l1-command",
	"qcom,mdss-dsi-pwm-timing-switch-l2-command",
	"qcom,mdss-dsi-pwm-timing-switch-l3-command",
	"qcom,mdss-dsi-timming-pwm-switch-onepulse-command",
	"qcom,mdss-dsi-pwm-dbv-threshold-extend-command",
	"qcom,mdss-dsi-pwm-switch-low-bl-command",
	"qcom,mdss-dsi-pwm-switch-high-bl-command",
	"qcom,mdss-dsi-dly-on-command",
	"qcom,mdss-dsi-dly-off-command",
	"qcom,mdss-dsi-cabc-off-command",
	"qcom,mdss-dsi-cabc-ui-command",
	"qcom,mdss-dsi-cabc-still-image-command",
	"qcom,mdss-dsi-cabc-video-command",
	"qcom,mdss-dsi-esd-switch-page-command",
	"qcom,dsi-panel-date-switch-command",
	"qcom,dsi-panel-btb-switch-command",
	"qcom,mdss-dsi-panel-info-switch-page-command",
	"qcom,mdss-dsi-panel-init-command",
	"qcom,mdss-dsi-optimize-on-command",
	"qcom,mdss-dsi-vid-120hz-switch-command",
	"qcom,mdss-dsi-vid-60hz-switch-command",
	"qcom,mdss-dsi-default-switch-page-command",
	"qcom,mdss-dsi-skipframe-dbv-command",
	"qcom,mdss-dsi-demura-dbv-mode-0-command",
	"qcom,mdss-dsi-demura-dbv-mode-1-command",
	"qcom,mdss-dsi-demura-dbv-mode-2-command",
	"qcom,mdss-dsi-demura-dbv-mode-3-command",
	"qcom,mdss-dsi-demura-dbv-mode-4-command",
	"qcom,mdss-dsi-demura-dbv-mode-5-command",
	"qcom,mdss-dsi-demura-dbv-mode-6-command",
	"qcom,mdss-dsi-demura-dbv-mode-7-command",
	"qcom,mdss-dsi-set-demura2-offset0-command",
	"qcom,mdss-dsi-set-demura2-offset1-command",
	"qcom,mdss-dsi-set-demura2-offset2-command",
	"qcom,mdss-dsi-set-demura2-offset3-command",
	"qcom,mdss-dsi-set-demura2-offset4-command",
	"qcom,mdss-dsi-uir-on-loading-effect-1-command",
	"qcom,mdss-dsi-uir-on-loading-effect-2-command",
	"qcom,mdss-dsi-uir-on-loading-effect-3-command",
	"qcom,mdss-dsi-uir-off-loading-effect-1-command",
	"qcom,mdss-dsi-uir-off-loading-effect-2-command",
	"qcom,mdss-dsi-uir-off-loading-effect-3-command",
	"qcom,mdss-dsi-uir-loading-effect-1-command",
	"qcom,mdss-dsi-uir-loading-effect-2-command",
	"qcom,mdss-dsi-uir-loading-effect-3-command",
	"qcom,mdss-dsi-set-dc-on-command",
	"qcom,mipi-err-check-page-command",
	"oplus,dsi-panel-gamma-compensation-page0-command",
	"oplus,dsi-panel-gamma-compensation-page1-command",
	"oplus,dsi-panel-gamma-compensation-command",
#endif /* OPLUS_FEATURE_DISPLAY */
};

const char *cmd_set_state_map[DSI_CMD_SET_MAX] = {
	"qcom,mdss-dsi-pre-on-command-state",
	"qcom,mdss-dsi-on-command-state",
	"qcom,vid-on-commands-state",
	"qcom,cmd-on-commands-state",
	"qcom,mdss-dsi-post-panel-on-command-state",
	"qcom,mdss-dsi-pre-off-command-state",
	"qcom,mdss-dsi-off-command-state",
	"qcom,mdss-dsi-post-off-command-state",
	"qcom,mdss-dsi-pre-res-switch-state",
	"qcom,mdss-dsi-res-switch-state",
	"qcom,mdss-dsi-post-res-switch-state",
	"qcom,video-mode-switch-in-commands-state",
	"qcom,video-mode-switch-out-commands-state",
	"qcom,cmd-mode-switch-in-commands-state",
	"qcom,cmd-mode-switch-out-commands-state",
	"qcom,mdss-dsi-panel-status-command-state",
	"qcom,mdss-dsi-lp1-command-state",
	"qcom,mdss-dsi-lp2-command-state",
	"qcom,mdss-dsi-nolp-command-state",
	"PPS not parsed from DTSI, generated dynamically",
	"ROI not parsed from DTSI, generated dynamically",
	"qcom,mdss-dsi-timing-switch-command-state",
	"qcom,mdss-dsi-post-mode-switch-on-command-state",
	"qcom,mdss-dsi-qsync-on-commands-state",
	"qcom,mdss-dsi-qsync-off-commands-state",
	"qcom,mdss-dsi-esync-post-on-commands-state",
	"qcom,mdss-dsi-arp_mode3_hw_te_on-command-state",
	"qcom,mdss-dsi-arp_mode1_hw_te_off-command-state",
	"qcom,mdss-dsi-freq-step-pattern1-command-state",
	"qcom,mdss-dsi-freq-step-pattern2-command-state",
	"qcom,mdss-dsi-freq-step-pattern3-command-state",
	"qcom,mdss-dsi-freq-step-pattern4-command-state",
	"qcom,mdss-dsi-freq-step-pattern5-command-state",
	"qcom,mdss-dsi-sticky_still_en-command-state",
	"qcom,mdss-dsi-sticky_still_disable-command-state",
	"qcom,mdss-dsi-sticky_on_fly-command-state",
	"qcom,mdss-dsi-trigger_self_refresh-command-state",
	"qcom,mdss-dsi-fps-switch-command-state",
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	"qcom,mdss-dsi-adfr-auto-on-command-state",
	"qcom,mdss-dsi-adfr-auto-off-command-state",
	"qcom,mdss-dsi-adfr-min-fps-0-command-state",
	"qcom,mdss-dsi-adfr-min-fps-1-command-state",
	"qcom,mdss-dsi-adfr-min-fps-2-command-state",
	"qcom,mdss-dsi-adfr-min-fps-3-command-state",
	"qcom,mdss-dsi-adfr-min-fps-4-command-state",
	"qcom,mdss-dsi-adfr-min-fps-5-command-state",
	"qcom,mdss-dsi-adfr-min-fps-6-command-state",
	"qcom,mdss-dsi-adfr-min-fps-7-command-state",
	"qcom,mdss-dsi-adfr-min-fps-8-command-state",
	"qcom,mdss-dsi-adfr-min-fps-9-command-state",
	"qcom,mdss-dsi-adfr-min-fps-10-command-state",
	"qcom,mdss-dsi-adfr-min-fps-11-command-state",
	"qcom,mdss-dsi-adfr-min-fps-12-command-state",
	"qcom,mdss-dsi-adfr-min-fps-13-command-state",
	"qcom,mdss-dsi-adfr-min-fps-14-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-0-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-1-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-2-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-3-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-4-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-5-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-6-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-7-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-8-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-9-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-10-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-11-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-12-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-13-command-state",
	"qcom,mdss-dsi-hpwm-adfr-min-fps-14-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-0-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-1-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-2-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-3-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-4-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-5-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-6-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-7-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-8-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-9-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-10-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-11-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-12-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-13-command-state",
	"qcom,mdss-dsi-bigdc-adfr-min-fps-14-command-state",
	"qcom,mdss-dsi-adfr-pre-switch-command-state",
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */
#ifdef OPLUS_FEATURE_DISPLAY_HIGH_PRECISION
	"qcom,mdss-dsi-adfr-high-precision-fps-0-command-state",
	"qcom,mdss-dsi-adfr-high-precision-fps-1-command-state",
	"qcom,mdss-dsi-adfr-high-precision-fps-2-command-state",
	"qcom,mdss-dsi-adfr-high-precision-fps-3-command-state",
	"qcom,mdss-dsi-hpwm-adfr-high-precision-fps-0-command-state",
	"qcom,mdss-dsi-hpwm-adfr-high-precision-fps-1-command-state",
	"qcom,mdss-dsi-hpwm-adfr-high-precision-fps-2-command-state",
	"qcom,mdss-dsi-hpwm-adfr-high-precision-fps-3-command-state",
	"qcom,mdss-dsi-bigdc-adfr-high-precision-fps-0-command-state",
	"qcom,mdss-dsi-bigdc-adfr-high-precision-fps-1-command-state",
	"qcom,mdss-dsi-bigdc-adfr-high-precision-fps-2-command-state",
	"qcom,mdss-dsi-bigdc-adfr-high-precision-fps-3-command-state",
	"qcom,mdss-dsi-adfr-high-precision-te-shift-on-command-state",
	"qcom,mdss-dsi-adfr-high-precision-te-shift-off-command-state",
#endif /* OPLUS_FEATURE_DISPLAY_HIGH_PRECISION */
#ifdef OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION
	"qcom,mdss-dsi-read-temp-compensation-reg-command-state",
	"qcom,mdss-dsi-temperature-compensation-command-state",
#endif /* OPLUS_FEATURE_DISPLAY_TEMP_COMPENSATION */
#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	"qcom,mdss-dsi-hbm-on-command-state",
	"qcom,mdss-dsi-hbm-on-onepulse-command-state",
	"qcom,mdss-dsi-hbm-off-command-state",
	"qcom,mdss-dsi-lhbm-pressed-icon-gamma-command-state",
	"qcom,mdss-dsi-lhbm-pressed-icon-grayscale-command-state",
	"qcom,mdss-dsi-lhbm-pressed-icon-on-command-state",
	"qcom,mdss-dsi-lhbm-pressed-icon-off-command-state",
	"qcom,mdss-dsi-lhbm-update-vdc-command-state",
	"qcom,mdss-dsi-lhbm-dbv-alpha-command-state",
	"qcom,mdss-dsi-aor-on-command-state",
	"qcom,mdss-dsi-aor-off-command-state",
	"qcom,mdss-dsi-aod-high-mode-command-state",
	"qcom,mdss-dsi-aod-low-mode-command-state",
	"qcom,mdss-dsi-ultra-low-power-aod-on-command-state",
	"qcom,mdss-dsi-ultra-low-power-aod-off-command-state",
	"qcom,mdss-dsi-aod-on-1pulse-command-state",
	"qcom,mdss-dsi-aod-off-1pulse-command-state",
	"qcom,mdss-dsi-aod-off-compensation-command-state",
	"qcom,mdss-dsi-aod-off-compensation-onepulse-command-state",
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */
#ifdef OPLUS_FEATURE_DISPLAY
	"qcom,mdss-dsi-post-on-backlight-state",
	"qcom,mdss-dsi-seed-0-command-state",
	"qcom,mdss-dsi-seed-1-command-state",
	"qcom,mdss-dsi-seed-2-command-state",
	"qcom,mdss-dsi-seed-3-command-state",
	"qcom,mdss-dsi-seed-4-command-state",
	"qcom,mdss-dsi-no-seed-native-command-state",
	"qcom,mdss-dsi-seed-off-command-state",
	"qcom,mdss-dsi-spr-0-command-state",
	"qcom,mdss-dsi-spr-1-command-state",
	"qcom,mdss-dsi-spr-2-command-state",
	"qcom,mdss-dsi-data-dimming-on-command-state",
	"qcom,mdss-dsi-data-dimming-off-command-state",
	"qcom,mdss-dsi-osc-clk-mode0-command-state",
	"qcom,mdss-dsi-osc-clk-mode1-command-state",
	"qcom,mdss-dsi-osc-track-on-command-state",
	"qcom,mdss-dsi-osc-track-off-command-state",
	"qcom,mdss-dsi-ffc-mode0-command-state",
	"qcom,mdss-dsi-ffc-mode1-command-state",
	"qcom,mdss-dsi-ffc-mode2-command-state",
	"qcom,mdss-dsi-ffc-mode3-command-state",
	"qcom,mdss-dsi-panel-id1-command-state",
	"qcom,mdss-dsi-panel-read-register-open-state",
	"qcom,mdss-dsi-panel-read-register-close-state",
	"qcom,mdss-dsi-loading-effect-1-command-state",
	"qcom,mdss-dsi-loading-effect-2-command-state",
	"qcom,mdss-dsi-loading-effect-off-command-state",
	"qcom,mdss-dsi-hbm-enter-switch-command-state",
	"qcom,mdss-dsi-hbm-exit-switch-command-state",
	"qcom,mdss-dsi-hbm-max-command-state",
	"qcom,mdss-dsi-hbm-exit-max-command-state",
	"qcom,mdss-dsi-change-voltage-before-bl-0-command-state",
	"qcom,mdss-dsi-pwm-switch-mode0-command-state",
	"qcom,mdss-dsi-pwm-switch-mode1-command-state",
	"qcom,mdss-dsi-pwm-switch-mode2-command-state",
	"qcom,mdss-dsi-pwm-state-l1tol2-command-state",
	"qcom,mdss-dsi-pwm-state-l1tol3-command-state",
	"qcom,mdss-dsi-pwm-state-l2tol1-command-state",
	"qcom,mdss-dsi-pwm-state-l2tol3-command-state",
	"qcom,mdss-dsi-pwm-state-l3tol1-command-state",
	"qcom,mdss-dsi-pwm-state-l3tol2-command-state",
	"qcom,mdss-dsi-pwm-switch-mode0-panel-on-command-state",
	"qcom,mdss-dsi-pwm-switch-mode1-panel-on-command-state",
	"qcom,mdss-dsi-pwm-switch-mode2-panel-on-command-state",
	"qcom,mdss-dsi-pwm-timing-switch-l1-command-state",
	"qcom,mdss-dsi-pwm-timing-switch-l2-command-state",
	"qcom,mdss-dsi-pwm-timing-switch-l3-command-state",
	"qcom,mdss-dsi-timming-pwm-switch-onepulse-command-state",
	"qcom,mdss-dsi-pwm-dbv-threshold-extend-command-state",
	"qcom,mdss-dsi-pwm-switch-low-bl-command-state",
	"qcom,mdss-dsi-pwm-switch-high-bl-command-state",
	"qcom,mdss-dsi-dly-on-command-state",
	"qcom,mdss-dsi-dly-off-command-state",
	"qcom,mdss-dsi-cabc-off-command-state",
	"qcom,mdss-dsi-cabc-ui-command-state",
	"qcom,mdss-dsi-cabc-still-image-command-state",
	"qcom,mdss-dsi-cabc-video-command-state",
	"qcom,mdss-dsi-esd-switch-page-command-state",
	"qcom,dsi-panel-date-switch-command-state",
	"qcom,dsi-panel-btb-switch-command-state",
	"qcom,mdss-dsi-panel-info-switch-page-command-state",
	"qcom,mdss-dsi-panel-init-command-state",
	"qcom,mdss-dsi-optimize-on-command-state",
	"qcom,mdss-dsi-vid-120hz-switch-command-state",
	"qcom,mdss-dsi-vid-60hz-switch-command-state",
	"qcom,mdss-dsi-default-switch-page-command-state",
	"qcom,mdss-dsi-skipframe-dbv-command-state",
	"qcom,mdss-dsi-demura-dbv-mode-0-command-state",
	"qcom,mdss-dsi-demura-dbv-mode-1-command-state",
	"qcom,mdss-dsi-demura-dbv-mode-2-command-state",
	"qcom,mdss-dsi-demura-dbv-mode-3-command-state",
	"qcom,mdss-dsi-demura-dbv-mode-4-command-state",
	"qcom,mdss-dsi-demura-dbv-mode-5-command-state",
	"qcom,mdss-dsi-demura-dbv-mode-6-command-state",
	"qcom,mdss-dsi-demura-dbv-mode-7-command-state",
	"qcom,mdss-dsi-set-demura2-offset0-command-state",
	"qcom,mdss-dsi-set-demura2-offset1-command-state",
	"qcom,mdss-dsi-set-demura2-offset2-command-state",
	"qcom,mdss-dsi-set-demura2-offset3-command-state",
	"qcom,mdss-dsi-set-demura2-offset4-command-state",
	"qcom,mdss-dsi-uir-on-loading-effect-1-command-state",
	"qcom,mdss-dsi-uir-on-loading-effect-2-command-state",
	"qcom,mdss-dsi-uir-on-loading-effect-3-command-state",
	"qcom,mdss-dsi-uir-off-loading-effect-1-command-state",
	"qcom,mdss-dsi-uir-off-loading-effect-2-command-state",
	"qcom,mdss-dsi-uir-off-loading-effect-3-command-state",
	"qcom,mdss-dsi-uir-loading-effect-1-command-state",
	"qcom,mdss-dsi-uir-loading-effect-2-command-state",
	"qcom,mdss-dsi-uir-loading-effect-3-command-state",
	"qcom,mdss-dsi-set-dc-on-command-state",
	"qcom,mipi-err-check-page-command-state",
	"oplus,dsi-panel-gamma-compensation-page0-command-state",
	"oplus,dsi-panel-gamma-compensation-page1-command-state",
	"oplus,dsi-panel-gamma-compensation-command-state",
#endif /* OPLUS_FEATURE_DISPLAY */
};

EXPORT_SYMBOL(cmd_set_prop_map);
EXPORT_SYMBOL(cmd_set_state_map);

void oplus_printf_backlight_cmd_log(u16 bl_lvl) {
	struct dsi_display *display;
	struct timespec64 now;
	struct tm broken_time;
	static time64_t time_last = 0;
	static u32 bl_count;
	static u16 backlight[BACKLIGHT_CACHE_MAX];
	static struct timespec64 past_times[BACKLIGHT_CACHE_MAX];

	int i = 0;
	int len = 0;
	char backlight_log_buf[1024];

	display = get_main_display();
	if(strcmp(display->panel->name, "AA545 P 3 A0005 dsc cmd mode panel"))
		return;
	ktime_get_real_ts64(&now);
	time64_to_tm(now.tv_sec, 0, &broken_time);
	if (now.tv_sec - time_last >= 60) {
		OPLUS_DSI_INFO("<%s> backlight_cmd time:%02d:%02d:%02d.%03ld,cmd:%04x\n",
			display->panel->oplus_panel.vendor_name, broken_time.tm_hour, broken_time.tm_min,
			broken_time.tm_sec, now.tv_nsec / 1000000, bl_lvl);
		time_last = now.tv_sec;
	}

	backlight[bl_count] = bl_lvl;
	past_times[bl_count] = now;
	bl_count++;
	if (bl_count >= BACKLIGHT_CACHE_MAX) {
		bl_count = 0;
		memset(backlight_log_buf, 0, sizeof(backlight_log_buf));
		for (i = 0; i < BACKLIGHT_CACHE_MAX; i++) {
			time64_to_tm(past_times[i].tv_sec, 0, &broken_time);
			len += snprintf(backlight_log_buf + len, sizeof(backlight_log_buf) - len,
				"%02d:%02d:%02d.%03ld:%04x,", broken_time.tm_hour, broken_time.tm_min,
				broken_time.tm_sec, past_times[i].tv_nsec / 1000000, backlight[i]);
		}
		OPLUS_DSI_INFO("<%s> len:%d backlight_cmd %s\n", display->panel->oplus_panel.vendor_name, len, backlight_log_buf);
	}
}

void oplus_ctrl_print_cmd_desc(struct dsi_ctrl *dsi_ctrl, struct dsi_cmd_desc *cmd)
{
	char buf[512];
	int len = 0;
	size_t i;
	const struct mipi_dsi_msg *msg = &cmd->msg;
	char *tx_buf = (char*)msg->tx_buf;
	u16 cmd51;
	memset(buf, 0, sizeof(buf));

	/* Packet Info */
	len += snprintf(buf, sizeof(buf) - len,  "%02X ", msg->type);
	len += snprintf(buf + len, sizeof(buf) - len, "%02X ", 0x00);
	len += snprintf(buf + len, sizeof(buf) - len, "%02X ", msg->channel);
	len += snprintf(buf + len, sizeof(buf) - len, "%02X ", (unsigned int)msg->flags);
	len += snprintf(buf + len, sizeof(buf) - len, "%02X ", cmd->post_wait_ms);
	len += snprintf(buf + len, sizeof(buf) - len, "%02X %02X ", (char)msg->tx_len >> 8, (char)msg->tx_len & 0x00FF);

	/* Packet Payload */
	for (i = 0 ; i < msg->tx_len ; i++) {
		len += snprintf(buf + len, sizeof(buf) - len, "%02X ", tx_buf[i]);
		/* Break to prevent show too long command */
		if (i > 160)
			break;
	}

	if (tx_buf[0] == 0x51 && msg->tx_len == 3) {
		cmd51 = (tx_buf[1] << 8) | tx_buf[2];
		oplus_printf_backlight_cmd_log(cmd51);
	}

	/* DSI_CTRL_ERR(dsi_ctrl, "%s\n", buf); */
	OPLUS_DSI_DEBUG_DCS("dsi_cmd: %s\n", buf);
}

int oplus_panel_cmd_print(struct dsi_panel *panel, enum dsi_cmd_set_type type)
{
	u32 count;

	count = panel->cur_mode->priv_info->cmd_sets[type].count;
	if (count == 0) {
		OPLUS_DSI_DEBUG("[%s] dsi_cmd: %s is null\n", panel->oplus_panel.vendor_name,
				cmd_set_prop_map[type]);
		return 0;
	}

	switch (type) {
	case DSI_CMD_SET_ROI:
	case DSI_CMD_ESD_SWITCH_PAGE:
	case DSI_CMD_SKIPFRAME_DBV:
	case DSI_CMD_DEFAULT_SWITCH_PAGE:
		/* Do nothing */
		break;
#ifdef OPLUS_FEATURE_DISPLAY_ADFR
	case DSI_CMD_ADFR_MIN_FPS_0:
	case DSI_CMD_ADFR_MIN_FPS_1:
	case DSI_CMD_ADFR_MIN_FPS_2:
	case DSI_CMD_ADFR_MIN_FPS_3:
	case DSI_CMD_ADFR_MIN_FPS_4:
	case DSI_CMD_ADFR_MIN_FPS_5:
	case DSI_CMD_ADFR_MIN_FPS_6:
	case DSI_CMD_ADFR_MIN_FPS_7:
	case DSI_CMD_ADFR_MIN_FPS_8:
	case DSI_CMD_ADFR_MIN_FPS_9:
	case DSI_CMD_ADFR_MIN_FPS_10:
	case DSI_CMD_ADFR_MIN_FPS_11:
	case DSI_CMD_ADFR_MIN_FPS_12:
	case DSI_CMD_ADFR_MIN_FPS_13:
	case DSI_CMD_ADFR_MIN_FPS_14:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_0:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_1:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_2:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_3:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_4:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_5:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_6:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_7:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_8:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_9:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_10:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_11:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_12:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_13:
	case DSI_CMD_HPWM_ADFR_MIN_FPS_14:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_0:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_1:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_2:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_3:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_4:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_5:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_6:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_7:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_8:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_9:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_10:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_11:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_12:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_13:
	case DSI_CMD_BIGDC_ADFR_MIN_FPS_14:
		if (panel->cur_mode->priv_info->oplus_adfr_idle_min_fps_log) {
			ADFR_DEBUG("[%s] dsi_cmd: %s, count=%d\n", panel->oplus_panel.vendor_name,
					cmd_set_prop_map[type], count);
			panel->cur_mode->priv_info->oplus_adfr_idle_min_fps_log = false;
		} else {
			ADFR_INFO("[%s] dsi_cmd: %s, count=%d\n", panel->oplus_panel.vendor_name,
					cmd_set_prop_map[type], count);
		}
		break;
#endif /* OPLUS_FEATURE_DISPLAY_ADFR */

	default:
		OPLUS_DSI_INFO("[%s] dsi_cmd: %s\n", panel->oplus_panel.vendor_name,
				cmd_set_prop_map[type]);
		break;
	}

	return 0;
}

int oplus_panel_cmd_switch(struct dsi_panel *panel, enum dsi_cmd_set_type *type)
{
	enum dsi_cmd_set_type type_store = *type;
	u32 count;

	oplus_panel_pwm_cmd_replace_handle(panel, type);
	if (*type == DSI_CMD_SET_ON && oplus_panel_id_compatibility(panel)) {
		*type = DSI_CMD_SET_COMPATIBILITY_ON;
	}

	count = panel->cur_mode->priv_info->cmd_sets[*type].count;
	if (count == 0) {
		OPLUS_DSI_DEBUG("[%s] %s is undefined, restore to %s\n",
				panel->oplus_panel.vendor_name,
				cmd_set_prop_map[*type],
				cmd_set_prop_map[type_store]);
		*type = type_store;
	}

	return 0;
}

int oplus_display_send_dcs_lock(struct dsi_display *display,
		enum dsi_cmd_set_type type)
{
	int rc = 0;

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("invalid display panel\n");
		return -ENODEV;
	}

	if (display->panel->power_mode == SDE_MODE_DPMS_OFF) {
		OPLUS_DSI_WARN("display panel is in off status\n");
		return -EINVAL;
	}

	if (type < DSI_CMD_SET_MAX) {
		mutex_lock(&display->display_lock);
		/* enable the clk vote for CMD mode panels */
		if (display->config.panel_mode == DSI_OP_CMD_MODE) {
			rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
			if (rc) {
				OPLUS_DSI_ERR("failed to enable DSI clocks, rc=%d\n", rc);
				mutex_unlock(&display->display_lock);
				return -EFAULT;
			}
		}

		mutex_lock(&display->panel->panel_lock);
		rc = dsi_panel_tx_cmd_set(display->panel, type, false);
		mutex_unlock(&display->panel->panel_lock);

		/* disable the clk vote for CMD mode panels */
		if (display->config.panel_mode == DSI_OP_CMD_MODE) {
			rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
			if (rc) {
				OPLUS_DSI_ERR("failed to disable DSI clocks, rc=%d\n", rc);
			}
		}
		mutex_unlock(&display->display_lock);
	} else {
		OPLUS_DSI_ERR("dcs[%d] is out of range", type);
		return -EINVAL;
	}

	return rc;
}

int oplus_panel_cmd_reg_replace(struct dsi_panel *panel, enum dsi_cmd_set_type type,
		u8 cmd, u8 *replace_reg, size_t replace_reg_len)
{
	int rc = 0;
	struct dsi_cmd_desc *cmds;
	size_t tx_len;
	u8 *tx_buf;
	u32 count;
	u8 *payload;
	u32 size;
	int i;

	if(!panel) {
		OPLUS_DSI_ERR("invalid display panel\n");
		return -ENODEV;
	}
	if(!replace_reg) {
		OPLUS_DSI_ERR("invalid cmd reg\n");
		return -ENODEV;
	}

	cmds = panel->cur_mode->priv_info->cmd_sets[type].cmds;
	count = panel->cur_mode->priv_info->cmd_sets[type].count;
	for (i = 0; i < count; i++) {
		tx_len = cmds[i].msg.tx_len;
		tx_buf = (u8 *)cmds[i].msg.tx_buf;
		if (cmd == tx_buf[0]) {
			if ((tx_len - 1) != replace_reg_len) {
				tx_len = replace_reg_len + 1;
				size = tx_len * sizeof(u8);
				payload = kzalloc(size, GFP_KERNEL);
				if (!payload) {
					rc = -ENOMEM;
					return rc;
				}
				payload[0] = tx_buf[0];
				if (tx_buf) {
					kfree(tx_buf);
				}
				tx_buf = payload;
				cmds[i].msg.tx_len = tx_len;
			}
			tx_buf++;
			memcpy(tx_buf, replace_reg, replace_reg_len);
			break;
		}
	}

	return 0;
}

int oplus_panel_cmd_reg_replace_specific_row(struct dsi_panel *panel, struct dsi_display_mode *mode,
		enum dsi_cmd_set_type type, u8 *replace_reg, size_t replace_reg_len, u32 row)
{
	int rc = 0;
	struct dsi_cmd_desc *cmds = NULL;
	size_t tx_len = 0;
	u8 *tx_buf = NULL;
	u32 count = 0;
	u8 *payload = NULL;
	u32 size = 0;
	u32 index = 0;

	if(!panel) {
		OPLUS_DSI_ERR("invalid display panel\n");
		return -ENODEV;
	}
	if(!replace_reg) {
		OPLUS_DSI_ERR("invalid cmd reg\n");
		return -ENODEV;
	}

	if (!mode) {
		OPLUS_DSI_INFO("mode is null, use panel cur_mode\n");
		mode = panel->cur_mode;
	}
	cmds = mode->priv_info->cmd_sets[type].cmds;
	count = mode->priv_info->cmd_sets[type].count;

	if (row > count) {
		OPLUS_DSI_ERR("Exceeding the number of rows of the command\n");
		return -EFAULT;
	}
	index = row - 1;

	tx_len = cmds[index].msg.tx_len;
	tx_buf = (u8 *)cmds[index].msg.tx_buf;
	if ((tx_len - 1) != replace_reg_len) {
		tx_len = replace_reg_len + 1;
		size = tx_len * sizeof(u8);
		payload = kzalloc(size, GFP_KERNEL);
		if (!payload) {
			rc = -ENOMEM;
			return rc;
		}
		payload[0] = tx_buf[0];
		if (tx_buf) {
			kfree(tx_buf);
		}
		tx_buf = payload;
		cmds[index].msg.tx_len = tx_len;
	}
	tx_buf++;
	memcpy(tx_buf, replace_reg, replace_reg_len);

	return 0;
}

int oplus_panel_cmdq_sync_handle(void *dsi_panel, enum dsi_cmd_set_type type, bool before_cmd)
{
	struct dsi_panel *panel = dsi_panel;
	struct dsi_display_mode *mode;
	int count = 0;

	OPLUS_DSI_DEBUG("start\n");

	if (!panel || !panel->cur_mode) {
		OPLUS_DSI_ERR("invalid panel param\n");
		return -EINVAL;
	}
	mode = panel->cur_mode;

	if(!panel->oplus_panel.cmdq_sync_support || !mode->priv_info->cmd_sets[type].oplus_cmd_set.sync_count) {
		return 0;
	}

	OPLUS_DSI_TRACE_BEGIN("oplus_panel_cmdq_sync_handle");
	OPLUS_DSI_TRACE_INT("oplus_dsi_cmd_set_type", type);
	OPLUS_DSI_TRACE_INT("before_cmd", before_cmd);

	if (before_cmd) {
		if (panel->oplus_panel.cmdq_sync_count) {
			OPLUS_DSI_INFO("[%s] dsi_cmd: %s block to the next %d frame\n",
					panel->oplus_panel.vendor_name,
					cmd_set_prop_map[type],
					panel->oplus_panel.cmdq_sync_count);
			oplus_sde_early_wakeup(panel);
			for (count = panel->oplus_panel.cmdq_sync_count; count > 0; count--) {
				oplus_wait_for_vsync(panel);
				if (panel->cur_mode->timing.refresh_rate == 60) {
					oplus_need_to_sync_te(panel);
				}
			}
		}
	} else {
		panel->oplus_panel.cmdq_sync_count = mode->priv_info->cmd_sets[type].oplus_cmd_set.sync_count;
	}

	OPLUS_DSI_TRACE_END("oplus_panel_cmdq_sync_handle");
	OPLUS_DSI_DEBUG("end\n");

	return 0;
}

int oplus_panel_cmdq_sync_count_reset(void *sde_connector)
{
	struct sde_connector *c_conn = sde_connector;
	struct dsi_display *display = NULL;

	OPLUS_DSI_DEBUG("start\n");

	if (!c_conn) {
		OPLUS_DSI_ERR("invalid c_conn param\n");
		return -EINVAL;
	}

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		OPLUS_DSI_DEBUG("not in dsi mode, should not reset cmdq sync status\n");
		return 0;
	}

	display = c_conn->display;
	if (!display || !display->panel) {
		OPLUS_DSI_DEBUG("invalid display params\n");
		return -EINVAL;
	}

	if(!display->panel->oplus_panel.cmdq_sync_support) {
		return 0;
	}

	display->panel->oplus_panel.cmdq_sync_count = 0;

	OPLUS_DSI_DEBUG("cmdq sync count is %d\n", display->panel->oplus_panel.cmdq_sync_count);
	SDE_ATRACE_INT("oplus_panel_cmdq_sync_count_reset",
			display->panel->oplus_panel.cmdq_sync_count);

	OPLUS_DSI_DEBUG("end\n");

	return 0;
}

int oplus_panel_cmdq_sync_count_decrease(void *sde_connector)
{
	struct sde_connector *c_conn = sde_connector;
	struct dsi_display *display = NULL;

	OPLUS_DSI_DEBUG("start\n");

	if (!c_conn) {
		OPLUS_DSI_ERR("invalid c_conn param\n");
		return -EINVAL;
	}

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI) {
		OPLUS_DSI_DEBUG("not in dsi mode, should not reset cmdq sync count\n");
		return 0;
	}

	display = c_conn->display;
	if (!display || !display->panel) {
		OPLUS_DSI_DEBUG("invalid display params\n");
		return -EINVAL;
	}

	if(!display->panel->oplus_panel.cmdq_sync_support) {
		return 0;
	}

	if (display->panel->oplus_panel.cmdq_sync_count > 0) {
		display->panel->oplus_panel.cmdq_sync_count--;
	}

	OPLUS_DSI_DEBUG("cmdq sync count is %d\n", display->panel->oplus_panel.cmdq_sync_count);
	SDE_ATRACE_INT("oplus_panel_cmdq_sync_count_decrease",
			display->panel->oplus_panel.cmdq_sync_count);

	OPLUS_DSI_DEBUG("end\n");

	return 0;
}

int oplus_panel_send_asynchronous_cmd(struct dsi_display *display)
{
	int rc = 0;

	rc = oplus_display_panel_set_demura2_offset(display);

	return rc;
}
