/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_parse.h
** Description : oplus display dts parse header
** Version : 1.1
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPALY_PARSE_H_
#define _OPLUS_DISPALY_PARSE_H_

#include <dsi_panel.h>

#define DSI_PANEL_OPLUS_DUMMY_VENDOR_NAME "PanelVendorDummy"
#define DSI_PANEL_OPLUS_DUMMY_MANUFACTURE_NAME "dummy1024"

/* Add for dts parse*/
int oplus_panel_parse_features_config(struct dsi_panel *panel);
int oplus_panel_parse_config(struct dsi_panel *panel);

/**
 * oplus_panel_parse_vsync_config() - oplus panel parse vsync config
 * @mode:  Display mode
 * @utils: Parser utils
 * Return: Zero on Success
 */
int oplus_panel_parse_vsync_config(
				struct dsi_display_mode *mode,
				struct dsi_parser_utils *utils);

#endif /* _OPLUS_DISPALY_PARSE_H_ */
