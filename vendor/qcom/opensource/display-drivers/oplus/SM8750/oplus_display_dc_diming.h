/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_dc_diming.h

** Description : oplus dc_diming feature
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_DC_DIMING_H_
#define _OPLUS_DISPLAY_DC_DIMING_H_

#include <drm/drm_connector.h>
#include <dsi_panel.h>
#include <dsi_defs.h>

struct dsi_panel_cmd_set *oplus_panel_update_seed_backlight(
	struct dsi_panel *panel, int brightness, enum dsi_cmd_set_type type);
int sde_connector_update_backlight(struct drm_connector *connector, bool post);
int oplus_seed_bright_to_alpha(int brightness);
int oplus_display_panel_get_dim_alpha(void *buf);
int oplus_display_panel_set_dim_alpha(void *buf);
int oplus_display_panel_get_dim_dc_alpha(void *buf);
int oplus_display_panel_get_dimlayer_enable(void *data);
int oplus_display_panel_set_dimlayer_enable(void *data);
int oplus_panel_parse_dc_config(struct dsi_panel *panel);
int oplus_bl_to_alpha_dc(int brightness);
int oplus_get_panel_brightness(void);
int oplus_find_index_invmaplist(uint32_t bl_level);

#endif /* _OPLUS_DISPLAY_DC_DIMING_H_ */
