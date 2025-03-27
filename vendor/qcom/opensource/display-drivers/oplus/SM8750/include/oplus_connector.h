/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_connector.h
** Description : oplus connector header
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_CONNECTOR_H_
#define _OPLUS_CONNECTOR_H_

struct dc_apollo_pcc_sync {
	wait_queue_head_t bk_wait;
	int dc_pcc_updated;
	__u32 pcc;
	__u32 pcc_last;
	__u32 pcc_current;
	struct mutex lock;
	int backlight_pending;
};

struct oplus_connector {
/* Used to indicate whether to update panel backlight in crtc_commit */
	bool bl_need_sync;
	bool osc_need_update;
	atomic_t bl_apl_need_update;
};

#endif /* _OPLUS_CONNECTOR_H_ */
