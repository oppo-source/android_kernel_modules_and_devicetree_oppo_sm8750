/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_debug.h
** Description : oplus debug header
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#ifndef _OPLUS_DEBUG_H_
#define _OPLUS_DEBUG_H_

#include "sde_trace.h"

enum oplus_log_level {
	OPLUS_LOG_LEVEL_NONE = 0,
	OPLUS_LOG_LEVEL_ERR,
	OPLUS_LOG_LEVEL_WARN,
	OPLUS_LOG_LEVEL_INFO,
	OPLUS_LOG_LEVEL_DEBUG,
};

/**
 * enum oplus_debug_log --       flags to control debug log; 1->enbale  0->disable
 * @OPLUS_DEBUG_LOG_DISABLED:    disable all debug log
 * @OPLUS_DEBUG_LOG_DCS:         dump register log
 * @OPLUS_DEBUG_LOG_DSI:         DSI log
 * @OPLUS_DEBUG_LOG_OFP:         OFP log
 * @OPLUS_DEBUG_LOG_ADFR:        ADFR log
 * @OPLUS_DEBUG_LOG_TEMP_COMPENSATION:temp compensation log
 * @OPLUS_DEBUG_LOG_ALL:         enable all debug log
 */
enum oplus_debug_log_flag {
	OPLUS_DEBUG_LOG_DISABLED = 0,
	OPLUS_DEBUG_LOG_DCS = BIT(0),
	OPLUS_DEBUG_LOG_DSI = BIT(1),
	OPLUS_DEBUG_LOG_OFP = BIT(2),
	OPLUS_DEBUG_LOG_ADFR = BIT(3),
	OPLUS_DEBUG_LOG_TEMP_COMPENSATION = BIT(4),
	OPLUS_DEBUG_LOG_ALL = 0xFFFF,
};

enum oplus_display_trace_enable {
	OPLUS_DISPLAY_DISABLE_TRACE = 0,
	OPLUS_DISPLAY_DSI_TRACE_ENABLE = BIT(0),
	OPLUS_DISPLAY_OFP_TRACE_ENABLE = BIT(1),
	OPLUS_DISPLAY_ADFR_TRACE_ENABLE = BIT(2),
	OPLUS_DISPLAY_TEMP_COMPENSATION_TRACE_ENABLE = BIT(3),
	OPLUS_DISPLAY_TRACE_ALL = 0xFFFF,
};

/* ---------------- oplus debug config ---------------- */
/* log level config */
extern unsigned int oplus_display_log_level;  /* Default value = OPLUS_LOG_LEVEL_DEBUG */
/* debug log config */
extern unsigned int oplus_display_log_type;   /* Default value = OPLUS_DEBUG_LOG_DISABLED */
/* debug trace config */
extern unsigned int oplus_display_trace_enable;
/* dual display id */
extern unsigned int oplus_ofp_display_id;

/* ---------------- base debug log  ---------------- */
#define OPLUS_DISP_ERR(tag, var, fmt, ...) \
	do { \
		pr_err("[ERR][%s][DISPLAY][%d][%s:%d] " pr_fmt(fmt), tag, var, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DISP_WARN(tag, var, fmt, ...) \
	do { \
		pr_warn("[WARN][%s][DISPLAY][%d][%s:%d] " pr_fmt(fmt), tag, var, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DISP_INFO(tag, var, fmt, ...) \
	do { \
		pr_info("[INFO][%s][DISPLAY][%d][%s:%d] " pr_fmt(fmt), tag, var, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DISP_INFO_ONCE(tag, var, fmt, ...) \
	do { \
		pr_info_once("[INFO_ONCE][%s][DISPLAY][%d][%s:%d] " pr_fmt(fmt), tag, var, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DISP_DEBUG(tag, var, fmt, ...) \
	do { \
		pr_debug("[DEBUG][%s][DISPLAY][%d][%s:%d] " pr_fmt(fmt), tag, var, __func__, __LINE__, ##__VA_ARGS__); \
	} while (0)

/* ---------------- dsi debug log  ---------------- */
#define OPLUS_DSI_ERR(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_ERR) \
			OPLUS_DISP_ERR("DSI", oplus_ofp_display_id, fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DSI_WARN(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_WARN) \
			OPLUS_DISP_WARN("DSI", oplus_ofp_display_id, fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DSI_INFO(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_INFO) \
			OPLUS_DISP_INFO("DSI", oplus_ofp_display_id, fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DSI_INFO_ONCE(fmt, ...) \
	do { \
		if (oplus_display_log_level >= OPLUS_LOG_LEVEL_INFO) \
			OPLUS_DISP_INFO_ONCE("DSI", oplus_ofp_display_id, fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DSI_DEBUG(fmt, ...) \
	do { \
		if ((oplus_display_log_level >= OPLUS_LOG_LEVEL_DEBUG) || (oplus_display_log_type & OPLUS_DEBUG_LOG_DSI)) \
			OPLUS_DISP_INFO("DSI", oplus_ofp_display_id, fmt, ##__VA_ARGS__); \
		else	\
			OPLUS_DISP_DEBUG("DSI", oplus_ofp_display_id, fmt, ##__VA_ARGS__); \
	} while (0)

#define OPLUS_DSI_DEBUG_DCS(fmt, ...) \
	do { \
		if ((oplus_display_log_level >= OPLUS_LOG_LEVEL_DEBUG) || (oplus_display_log_type & OPLUS_DEBUG_LOG_DCS)) \
			OPLUS_DISP_INFO("DSI", oplus_ofp_display_id, fmt, ##__VA_ARGS__); \
		else	\
			OPLUS_DISP_DEBUG("DSI", oplus_ofp_display_id, fmt, ##__VA_ARGS__); \
	} while (0)

/* ---------------- ofp debug log  ---------------- */
/* in oplus_onscreenfingerprint.h */

/* ---------------- dsi debug trace  ---------------- */
#define OPLUS_DSI_TRACE_BEGIN(name) \
	do { \
		if (oplus_display_trace_enable & OPLUS_DISPLAY_DSI_TRACE_ENABLE) \
			SDE_ATRACE_BEGIN(name); \
	} while (0)

#define OPLUS_DSI_TRACE_END(name) \
	do { \
		if (oplus_display_trace_enable & OPLUS_DISPLAY_DSI_TRACE_ENABLE) \
			SDE_ATRACE_END(name); \
	} while (0)

#define OPLUS_DSI_TRACE_INT(name, value) \
	do { \
		if (oplus_display_trace_enable & OPLUS_DISPLAY_DSI_TRACE_ENABLE) \
			SDE_ATRACE_INT(name, value); \
	} while (0)

/* debug trace */
#define OPLUS_OFP_TRACE_BEGIN(name)	\
	do {	\
		if (oplus_display_trace_enable & OPLUS_DISPLAY_OFP_TRACE_ENABLE)	\
			SDE_ATRACE_BEGIN(name);	\
	} while (0)

#define OPLUS_OFP_TRACE_END(name)	\
	do {	\
		if (oplus_display_trace_enable & OPLUS_DISPLAY_OFP_TRACE_ENABLE)	\
			SDE_ATRACE_END(name);	\
	} while (0)

#define OPLUS_OFP_TRACE_INT(name, value)	\
	do {	\
		if (oplus_display_trace_enable & OPLUS_DISPLAY_OFP_TRACE_ENABLE)	\
			SDE_ATRACE_INT(name, value);	\
	} while (0)

#endif /* _OPLUS_DEBUG_H_ */
