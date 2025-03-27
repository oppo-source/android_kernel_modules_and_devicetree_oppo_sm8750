/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_esd.h
** Description : oplus esd feature
** Version : 2.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_ESD_H_
#define _OPLUS_DISPLAY_ESD_H_

#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_defs.h"

int oplus_panel_parse_esd_reg_read_configs(struct dsi_panel *panel);
bool oplus_panel_validate_reg_read(struct dsi_panel *panel);
int oplus_display_status_check_error_flag(struct dsi_display *display);

#endif /* _OPLUS_DISPLAY_ESD_H_ */
