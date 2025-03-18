// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023-2023 Oplus. All rights reserved.
 */


#ifndef _OPLUS_TRACKPOINT_REPORT_
#define _OPLUS_TRACKPOINT_REPORT_

/*
 * trackpoint include info trackpoint and exception trackpoint
 * info trackpoint is for data stats
 * exception trackpoint is for bug fix, so will upload log
 */
#define TRACKPOINT_TYPE_INFO		1
#define TRACKPOINT_TYPE_EXCEPTION	2

/*
 * the trackpoint_id is the event_id in database at dataserver
 */
#define DRM_TRACKPOINT_EVENTID	12002
#define GPU_TRACKPOINT_EVENTID	12005

#define MESSAGE_MAX_SIZE		512
#define FUNC_LINE_MAX_SIZE		128

enum OPLUS_TRACKPOINT_REPORT_ID {
	/* QCOM ID: 401~499 */
	OPLUS_DISP_Q_ERROR_CMD_TRANS_FAIL = 401,
	OPLUS_DISP_Q_ERROR_POWER_CHECK_FAIL = 406,
	OPLUS_DISP_Q_ERROR_DCDC_CHECK_FAIL = 407,
	OPLUS_DISP_Q_ERROR_ESD_CHECK_FAIL = 408,
	OPLUS_DISP_Q_ERROR_FENCE_TIMEOUT = 413,
	OPLUS_DISP_Q_ERROR_DMA_IRQ_TRIGGER_FAIL = 416,
	OPLUS_DISP_Q_ERROR_PTR_TIMEOUT = 418,
	OPLUS_DISP_Q_ERROR_UNDERRUN = 422,
	OPLUS_DISP_Q_ERROR_CTRL_HW = 424,
	OPLUS_DISP_Q_ERROR_PHY_HW = 425,
	OPLUS_DISP_Q_INFO_DYN_MIPI = 426,
	OPLUS_DISP_Q_INFO_DYN_MIPI_INVALID = 427,
	OPLUS_DISP_Q_INFO_DYN_OSC = 428,
	OPLUS_DISP_Q_INFO_DYN_OSC_INVALID = 429,
	OPLUS_DISP_Q_INFO_TEST = 499,
	/* QCOM ID End */

	/* MTK ID: 501~599 */
	OPLUS_DISP_M_ERROR_OVL_PROBE_FAIL = 501,
	OPLUS_DISP_M_ERROR_UNDERFLOW = 502,
	OPLUS_DISP_M_ERROR_INVALID_VBLANK = 503,
	OPLUS_DISP_M_ERROR_DDP_PROBE_FAIL = 504,
	OPLUS_DISP_M_ERROR_UNDERRUN = 506,
	OPLUS_DISP_M_ERROR_ESD_CHECK_FAIL = 507,
	OPLUS_DISP_M_ERROR_CMDQ_TIMEOUT = 508,
	OPLUS_DISP_M_ERROR_AAL_REG = 510,
	OPLUS_DISP_M_ERROR_TE_CHECK_FAIL = 511,
	OPLUS_DISP_M_INFO_TEST = 599,
	/* MTK ID End */
	OPLUS_DISP_TRACKPOINT_MAX,
};

struct trackpoint {
	int type;
	int event_id;
	int sub_event_id;
	char message[MESSAGE_MAX_SIZE];
	char func_line[FUNC_LINE_MAX_SIZE];
};

int trackpoint_report(struct trackpoint *tp);

#define display_info_trackpoint_report(fmt, ...) \
	do { \
		struct trackpoint tp = { \
			.type = TRACKPOINT_TYPE_INFO, \
			.event_id = DRM_TRACKPOINT_EVENTID, \
			.sub_event_id = 0, \
		}; \
		scnprintf(tp.message, sizeof(tp.message), fmt, ##__VA_ARGS__); \
		scnprintf(tp.func_line, sizeof(tp.func_line), "%s:%d", __func__, __LINE__); \
		trackpoint_report(&tp); \
	} while (0)

#define display_exception_trackpoint_report(fmt, ...) \
	do { \
		struct trackpoint tp = { \
			.type = TRACKPOINT_TYPE_EXCEPTION, \
			.event_id = DRM_TRACKPOINT_EVENTID, \
			.sub_event_id = 0, \
		}; \
		scnprintf(tp.message, sizeof(tp.message), fmt, ##__VA_ARGS__); \
		scnprintf(tp.func_line, sizeof(tp.func_line), "%s:%d", __func__, __LINE__); \
		trackpoint_report(&tp); \
	} while (0)

#define gpu_info_trackpoint_report(fmt, ...) \
	do { \
		struct trackpoint tp = { \
			.type = TRACKPOINT_TYPE_INFO, \
			.event_id = GPU_TRACKPOINT_EVENTID, \
			.sub_event_id = 0, \
		}; \
		scnprintf(tp.message, sizeof(tp.message), fmt, ##__VA_ARGS__); \
		scnprintf(tp.func_line, sizeof(tp.func_line), "%s:%d", __func__, __LINE__); \
		trackpoint_report(&tp); \
	} while (0)

#define gpu_exception_trackpoint_report(fmt, ...) \
	do { \
		struct trackpoint tp = { \
			.type = TRACKPOINT_TYPE_EXCEPTION, \
			.event_id = GPU_TRACKPOINT_EVENTID, \
			.sub_event_id = 0, \
		}; \
		scnprintf(tp.message, sizeof(tp.message), fmt, ##__VA_ARGS__); \
		scnprintf(tp.func_line, sizeof(tp.func_line), "%s:%d", __func__, __LINE__); \
		trackpoint_report(&tp); \
	} while (0)

#define INFO_TRACKPOINT_REPORT(fmt, ...)	\
	do { \
		pr_info("[INFO][TRACKPOINT][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__); \
		display_info_trackpoint_report(fmt, ##__VA_ARGS__); \
	} while (0)

#define EXCEPTION_TRACKPOINT_REPORT(fmt, ...)	\
	do { \
		pr_err("[ERR][TRACKPOINT][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__); \
		display_exception_trackpoint_report(fmt, ##__VA_ARGS__); \
	} while (0)
#endif /* _OPLUS_TRACKPOINT_REPORT_ */

