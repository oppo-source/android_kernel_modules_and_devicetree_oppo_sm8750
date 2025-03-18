/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_ext.h
** Description : oplus display panel ext feature
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_EXT_H_
#define _OPLUS_DISPLAY_EXT_H_

#include "dsi_display.h"
#include "dsi_panel.h"
#include "oplus_display_device_ioctl.h"

void oplus_panel_switch_vid_mode(struct dsi_display *display, struct dsi_display_mode *mode);

/**
 * oplus_panel_init() - oplus panel init
 * @panel: Display panel
 * Return: Zero on Success
 */
int oplus_panel_init(struct dsi_panel *panel);

/**
 * oplus_panel_id_compatibility_init() - oplus panel initialization code of the same screen is compatible through the panel id
 */
int oplus_panel_id_compatibility_init(struct dsi_display *display);

/**
 * oplus_panel_id_compatibility() - oplus panel determine whether compatibility is required
 */
bool oplus_panel_id_compatibility(struct dsi_panel *panel);

/**
 * oplus_check_refresh_rate() - oplus panel refresh rate check, get old_rate new_rate
 */
void oplus_check_refresh_rate(const int old_rate, const int new_rate);

int oplus_set_osc_status(struct drm_encoder *drm_enc);

/**
 * sde_encoder_is_disabled - encoder is disabled
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     bool.
 */
bool sde_encoder_is_disabled(struct drm_encoder *drm_enc);

void oplus_panel_switch_to_sync_te(struct dsi_panel *panel);

int oplus_display_read_serial_number(struct dsi_display *display, unsigned long *serial_number);
int oplus_display_read_panel_id(struct dsi_display *display, struct panel_id *panel_id);

int oplus_panel_register_fpga_notifier(void);

void oplus_panel_check_fpga(struct dsi_panel *panel);
#endif /* _OPLUS_DISPLAY_EXT_H_ */


