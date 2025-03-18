/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_panel_cmd.h
** Description : oplus display panel cmd feature
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_PANEL_CMD_H_
#define _OPLUS_DISPLAY_PANEL_CMD_H_

#include <linux/soc/qcom/panel_event_notifier.h>
#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_defs.h"

#define REG_SIZE 256

/**
 * oplus_display_send_dcs_lock() - send dcs with lock
 */
int oplus_display_send_dcs_lock(struct dsi_display *display,
		enum dsi_cmd_set_type type);

/**
 * oplus_panel_cmd_reg_replace() - replace cmd regs
 */
int oplus_panel_cmd_reg_replace(struct dsi_panel *panel, enum dsi_cmd_set_type type,
		u8 cmd, u8 *replace_reg, size_t replace_reg_len);
int oplus_panel_cmd_reg_replace_specific_row(struct dsi_panel *panel, struct dsi_display_mode *mode,
		enum dsi_cmd_set_type type, u8 *replace_reg, size_t replace_reg_len, u32 row);

int oplus_panel_cmdq_sync_handle(void *dsi_panel, enum dsi_cmd_set_type type, bool before_cmd);
int oplus_panel_cmdq_sync_count_reset(void *sde_connector);
int oplus_panel_cmdq_sync_count_decrease(void *sde_connector);

/**
 * oplus_panel_send_asynchronous_cmd() - send commands asynchronously
 */
int oplus_panel_send_asynchronous_cmd(struct dsi_display *display);

/**
 * oplus_panel_cmd_print() - oplus panel command printf
 * @panel: Display panel
 * @type:  Command type
 * Return: Zero on Success
 */
int oplus_panel_cmd_print(struct dsi_panel *panel, enum dsi_cmd_set_type type);

/**
 * oplus_panel_cmd_switch() - oplus panel command switch
 * @panel: Display panel
 * @type:  Pointer of command type
 * Return: Zero on Success
 */
int oplus_panel_cmd_switch(struct dsi_panel *panel, enum dsi_cmd_set_type *type);

/**
 * oplus_ctrl_print_cmd_desc() - oplus command desc printf
 * @dsi_ctrl: Dsi ctrl
 * @cmd: Dsi command set
 * Return: void
 */
void oplus_ctrl_print_cmd_desc(struct dsi_ctrl *dsi_ctrl, struct dsi_cmd_desc *cmd);
#endif /* _OPLUS_DISPLAY_PANEL_CMD_H_ */
