/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display.h
** Description : oplus display header
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_DISPLAY_H_
#define _OPLUS_DISPLAY_H_

struct oplus_dsi_display{
	u8 panel_flag;
	u8 panel_id1;
	u8 panel_id2;
	u8 panel_id3;
	unsigned long panel_sn;
};

enum oplus_display_id {
	DISPLAY_PRIMARY = 0,
	DISPLAY_SECONDARY = 1,
	DISPLAY_MAX,
};

struct dsi_display *get_main_display(void);

struct dsi_display *get_sec_display(void);

/* Add for implement panel register read */
int dsi_host_alloc_cmd_tx_buffer(struct dsi_display *display);
int dsi_display_cmd_engine_enable(struct dsi_display *display);
int dsi_display_cmd_engine_disable(struct dsi_display *display);


#endif /* _OPLUS_DISPLAY_H_ */
