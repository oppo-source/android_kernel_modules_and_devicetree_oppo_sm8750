/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _OPLUS_CAM_DEBUG_UTIL_H_
#define _OPLUS_CAM_DEBUG_UTIL_H_

#include <dt-bindings/msm-camera.h>
#include <linux/platform_device.h>

extern unsigned long long oplus_debug_mdl;
extern unsigned int oplus_debug_type;
extern unsigned int oplus_debug_priority;
extern unsigned int oplus_debug_drv;
extern unsigned int oplus_debug_bypass_drivers;

#define CAM_EXT_IS_NULL_TO_STR(ptr) ((ptr) ? "Non-NULL" : "NULL")

#define CAM_EXT_LOG_BUF_LEN                  512
#define BYPASS_VALUE       0xDEADBEEF
#define DEFAULT_CLK_VALUE  19200000

/* Module IDs used for debug logging */
enum cam_ext_debug_module_id {
	CAM_EXT_CORE,                /* bit 1 */
	CAM_EXT_SENSOR,              /* bit 5 */
	CAM_EXT_FLASH,               /* bit 12 */
	CAM_EXT_ACTUATOR,            /* bit 13 */
	CAM_EXT_CCI,                 /* bit 14 */
	CAM_EXT_EEPROM,              /* bit 16 */
	CAM_EXT_OIS,                 /* bit 20 */
	CAM_EXT_TOF,                 /* bit 37 */
	CAM_EXT_UTIL,
	CAM_EXT_DBG_MOD_MAX
};

/* Log level types */
enum cam_ext_debug_log_level {
	CAM_EXT_TYPE_TRACE,
	CAM_EXT_TYPE_ERR,
	CAM_EXT_TYPE_WARN,
	CAM_EXT_TYPE_INFO,
	CAM_EXT_TYPE_DBG,
	CAM_EXT_TYPE_MAX,
};

/*
 * enum cam_ext_debug_priority - Priority of debug log (0 = Lowest)
 */
enum cam_ext_debug_priority {
	CAM_EXT_DBG_PRIORITY_0,
	CAM_EXT_DBG_PRIORITY_1,
	CAM_EXT_DBG_PRIORITY_2,
};

static const char *cam_ext_debug_mod_name[CAM_EXT_DBG_MOD_MAX] = {
	[CAM_EXT_CORE]        = "CAM-EXT-CORE",
	[CAM_EXT_SENSOR]      = "CAM-EXT-SENSOR",
	[CAM_EXT_FLASH]       = "CAM-EXT-FLASH",
	[CAM_EXT_ACTUATOR]    = "CAM-EXT-ACTUATOR",
	[CAM_EXT_CCI]         = "CAM-EXT-CCI",
	[CAM_EXT_EEPROM]      = "CAM-EXT-EEPROM",
	[CAM_EXT_OIS]         = "CAM-EXT-OIS",
	[CAM_EXT_TOF]         = "CAM-EXT-TOF",
	[CAM_EXT_UTIL]        = "CAM-EXT-UTIL",
};

#define ___CAM_EXT_DBG_MOD_NAME(module_id)                                      \
__builtin_choose_expr(((module_id) == CAM_EXT_CORE), "CAM-EXT-CORE",                \
__builtin_choose_expr(((module_id) == CAM_EXT_SENSOR), "CAM-EXT-SENSOR",            \
__builtin_choose_expr(((module_id) == CAM_EXT_FLASH), "CAM-EXT-FLASH",              \
__builtin_choose_expr(((module_id) == CAM_EXT_ACTUATOR), "CAM-EXT-ACTUATOR",        \
__builtin_choose_expr(((module_id) == CAM_EXT_CCI), "CAM-EXT-CAM-CCI",                  \
__builtin_choose_expr(((module_id) == CAM_EXT_EEPROM), "CAM-EXT-EEPROM",            \
__builtin_choose_expr(((module_id) == CAM_EXT_OIS), "CAM-EXT-OIS",                  \
__builtin_choose_expr(((module_id) == CAM_EXT_TOF), "CAM-EXT-TOF",                  \
__builtin_choose_expr(((module_id) == CAM_EXT_UTIL), "CAM-EXT-UTIL",                  \
"CAMERA"))))))))))))))))))))))))))))))))))))))

#define CAM_EXT_DBG_MOD_NAME(module_id) \
((module_id < CAM_EXT_DBG_MOD_MAX) ? cam_ext_debug_mod_name[module_id] : "CAMERA")

#define __CAM_EXT_DBG_MOD_NAME(module_id) \
__builtin_choose_expr(__builtin_constant_p((module_id)), ___CAM_EXT_DBG_MOD_NAME(module_id), \
	CAM_EXT_DBG_MOD_NAME(module_id))

static const char *cam_ext_debug_tag_name[CAM_EXT_TYPE_MAX] = {
	[CAM_EXT_TYPE_TRACE] = "CAM_EXT_TRACE",
	[CAM_EXT_TYPE_ERR]   = "CAM_EXT_ERR",
	[CAM_EXT_TYPE_WARN]  = "CAM_EXT_WARN",
	[CAM_EXT_TYPE_INFO]  = "CAM_EXT_INFO",
	[CAM_EXT_TYPE_DBG]   = "CAM_EXT_DBG",
};

#define ___CAM_EXT_LOG_TAG_NAME(tag)                     \
({                                                  \
	static_assert(tag < CAM_EXT_TYPE_MAX);          \
	cam_ext_debug_tag_name[tag];                    \
})

#define CAM_EXT_LOG_TAG_NAME(tag) ((tag < CAM_EXT_TYPE_MAX) ? cam_ext_debug_tag_name[tag] : "CAM_EXT_LOG")

#define __CAM_EXT_LOG_TAG_NAME(tag) \
__builtin_choose_expr(__builtin_constant_p((tag)), ___CAM_EXT_LOG_TAG_NAME(tag), \
	CAM_EXT_LOG_TAG_NAME(tag))

enum cam_ext_log_print_type {
	CAM_EXT_PRINT_LOG   = 0x1,
	CAM_EXT_PRINT_TRACE = 0x2,
	CAM_EXT_PRINT_BOTH  = 0x3,
};

#define __CAM_EXT_LOG_FMT KERN_INFO "%s: %s: %s: %d: %s "

/**
 * cam_ext_print_log() - function to print logs (internal use only, use macros instead)
 *
 * @type:      Corresponds to enum cam_ext_log_print_type, selects if logs are printed in log buffer,
 *        trace buffers or both
 * @module_id: Module calling the log macro
 * @tag:       Tag for log level
 * @func:      Function string
 * @line:      Line number
 * @fmt:       Formatting string
 */

void cam_ext_print_log(int type, int module, int tag, const char *func,
	int line, const char *fmt, ...);

#define __CAM_EXT_LOG(type, tag, module_id, fmt, args...)                               \
({                                                                                  \
	cam_ext_print_log(type,                                      \
		module_id, tag, __func__,   \
		__LINE__,  fmt, ##args);                                                  \
})

#define CAM_EXT_LOG(tag, module_id, fmt, args...) \
__CAM_EXT_LOG(CAM_EXT_PRINT_BOTH, tag, module_id, fmt, ##args)

#define CAM_EXT_LOG_RL_CUSTOM(type, module_id, interval, burst, fmt, args...)                \
({                                                                                       \
	static DEFINE_RATELIMIT_STATE(_rs, (interval * HZ), burst);                      \
	__CAM_EXT_LOG(__ratelimit(&_rs) ? CAM_EXT_PRINT_BOTH : CAM_EXT_PRINT_TRACE,                  \
		type, module_id, fmt, ##args);                                           \
})

#define CAM_EXT_LOG_RL(type, module_id, fmt, args...)                                        \
CAM_EXT_LOG_RL_CUSTOM(type, module_id, DEFAULT_RATELIMIT_INTERVAL, DEFAULT_RATELIMIT_BURST,  \
fmt, ##args)

#define __CAM_EXT_DBG(module_id, priority, fmt, args...)                                              \
({                                                                                                \
	if (unlikely((oplus_debug_mdl & BIT_ULL(module_id)) && (priority >= oplus_debug_priority))) {         \
		CAM_EXT_LOG(CAM_EXT_TYPE_DBG, module_id, fmt, ##args);                                    \
	}                                                                                         \
})

/**
 * CAM_EXT_ERR / CAM_EXT_WARN / CAM_EXT_INFO / CAM_EXT_TRACE
 *
 * @brief: Macros to print logs at respective level error/warn/info/trace. All
 * logs except CAM_EXT_TRACE are printed in both log and trace buffers.
 *
 * @__module: Respective enum cam_ext_debug_module_id
 * @fmt:      Format string
 * @args:     Arguments to match with format
 */
#define CAM_EXT_ERR(__module, fmt, args...)  CAM_EXT_LOG(CAM_EXT_TYPE_ERR, __module, fmt, ##args)
#define CAM_EXT_WARN(__module, fmt, args...) CAM_EXT_LOG(CAM_EXT_TYPE_WARN, __module, fmt, ##args)
#define CAM_EXT_INFO(__module, fmt, args...) CAM_EXT_LOG(CAM_EXT_TYPE_INFO, __module, fmt, ##args)
#define CAM_EXT_TRACE(__module, fmt, args...) \
__CAM_EXT_LOG(CAM_EXT_PRINT_TRACE, CAM_EXT_TYPE_TRACE, __module, fmt, ##args)

/**
 * CAM_EXT_ERR_RATE_LIMIT / CAM_EXT_WARN_RATE_LIMIT / CAM_EXT_INFO_RATE_LIMIT
 *
 * @brief: Rate limited version of logs used to reduce log spew.
 *
 * @__module: Respective enum cam_ext_debug_module_id
 * @fmt:      Format string
 * @args:     Arguments to match with format
 */
#define CAM_EXT_ERR_RATE_LIMIT(__module, fmt, args...)  CAM_EXT_LOG_RL(CAM_EXT_TYPE_ERR, __module, fmt, ##args)
#define CAM_EXT_WARN_RATE_LIMIT(__module, fmt, args...) CAM_EXT_LOG_RL(CAM_EXT_TYPE_WARN, __module, fmt, ##args)
#define CAM_EXT_INFO_RATE_LIMIT(__module, fmt, args...) CAM_EXT_LOG_RL(CAM_EXT_TYPE_INFO, __module, fmt, ##args)

/**
 * CAM_EXT_ERR_RATE_LIMIT_CUSTOM / CAM_EXT_WARN_RATE_LIMITT_CUSTOM/ CAM_EXT_INFO_RATE_LIMITT_CUSTOM
 *
 * @brief: Rate limited version of logs used to reduce log spew that can have
 * customized burst rate
 *
 * @__module: Respective enum cam_ext_debug_module_id
 * @interval: Sliding window interval in which to count logs
 * @burst:    Maximum number of logs in the specified interval
 * @fmt:      Format string
 * @args:     Arguments to match with format
 */
#define CAM_EXT_ERR_RATE_LIMIT_CUSTOM(__module, interval, burst, fmt, args...)  \
	CAM_EXT_LOG_RL_CUSTOM(CAM_EXT_TYPE_ERR, __module, interval, burst, fmt, ##args)

#define CAM_EXT_WARN_RATE_LIMIT_CUSTOM(__module, interval, burst, fmt, args...) \
	CAM_EXT_LOG_RL_CUSTOM(CAM_EXT_TYPE_WARN, __module, interval, burst, fmt, ##args)

#define CAM_EXT_INFO_RATE_LIMIT_CUSTOM(__module, interval, burst, fmt, args...) \
	CAM_EXT_LOG_RL_CUSTOM(CAM_EXT_TYPE_INFO, __module, interval, burst, fmt, ##args)

/*
 * CAM_EXT_DBG
 * @brief    :  This Macro will print debug logs when enabled using GROUP and
 *              if its priority is greater than the priority parameter
 *
 * @__module :  Respective module id which is been calling this Macro
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_EXT_DBG(__module, fmt, args...)     __CAM_EXT_DBG(__module, CAM_EXT_DBG_PRIORITY_0, fmt, ##args)
#define CAM_EXT_DBG_PR1(__module, fmt, args...) __CAM_EXT_DBG(__module, CAM_EXT_DBG_PRIORITY_1, fmt, ##args)
#define CAM_EXT_DBG_PR2(__module, fmt, args...) __CAM_EXT_DBG(__module, CAM_EXT_DBG_PRIORITY_2, fmt, ##args)

/**
 * cam_ext_print_to_buffer
 * @brief:         Function to print to camera logs to a buffer. Don't use directly. Use macros
 *                 provided below.
 *
 * @buf:           Buffer to print into
 * @buf_size:      Total size of the buffer
 * @len:           Pointer to variable used to keep track of the length
 * @tag:           Log level tag to be prefixed
 * @module_id:     Module id tag to be prefixed
 * @fmt:           Formatted string which needs to be print in log
 * @args:          Arguments which needs to be print in log
 */
void cam_ext_print_to_buffer(char *buf, const size_t buf_size, size_t *len, unsigned int tag,
	unsigned long long module_id, const char *fmt, ...);

/**
 * CAM_EXT_[ERR/WARN/INFO]_BUF
 * @brief:         Macro to print a new line into log buffer.
 *
 * @module_id:     Module id tag to be prefixed
 * @buf:           Buffer to print into
 * @buf_size:      Total size of the buffer
 * @len:           Pointer to the variable used to keep track of the length
 * @fmt:           Formatted string which needs to be print in log
 * @args:          Arguments which needs to be print in log
 */
#define CAM_EXT_ERR_BUF(module_id, buf, buf_size, len, fmt, args...)                                   \
	cam_ext_print_to_buffer(buf, buf_size, len, CAM_EXT_TYPE_ERR, module_id, fmt, ##args)
#define CAM_EXT_WARN_BUF(module_id, buf, buf_size, len, fmt, args...)                                  \
	cam_ext_print_to_buffer(buf, buf_size, len, CAM_EXT_TYPE_WARN, module_id, fmt, ##args)
#define CAM_EXT_INFO_BUF(module_id, buf, buf_size, len, fmt, args...)                                  \
	cam_ext_print_to_buffer(buf, buf_size, len, CAM_EXT_TYPE_INFO, module_id, fmt, ##args)

#define CAM_EXT_BOOL_TO_YESNO(val) ((val) ? "Y" : "N")

/**
 * cam_ext_debugfs_init()
 *
 * @brief: create camera debugfs root folder
 */
void cam_ext_debugfs_init(void);

/**
 * cam_ext_debugfs_deinit()
 *
 * @brief: remove camera debugfs root folder
 */
void cam_ext_debugfs_deinit(void);

/**
 * cam_ext_debugfs_create_subdir()
 *
 * @brief:  create a directory within the camera debugfs root folder
 *
 * @name:   name of the directory
 * @subdir: pointer to the newly created directory entry
 *
 * @return: 0 on success, negative on failure
 */
int cam_ext_debugfs_create_subdir(const char *name, struct dentry **subdir);

/**
 * cam_ext_debugfs_lookup_subdir()
 *
 * @brief:  lookup a directory within the camera debugfs root folder
 *
 * @name:   name of the directory
 * @subdir: pointer to the successfully found directory entry
 *
 * @return: 0 on success, negative on failure
 */
int cam_ext_debugfs_lookup_subdir(const char *name, struct dentry **subdir);

/**
 * cam_ext_debugfs_available()
 *
 * @brief:  Check if debugfs is enabled for camera. Use this function before creating any
 *          debugfs entries.
 *
 * @return: true if enabled, false otherwise
 */
static inline bool cam_ext_debugfs_available(void)
{
	#if defined(CONFIG_DEBUG_FS)
		return true;
	#else
		return false;
	#endif
}

#endif /* _OPLUS_CAM_DEBUG_UTIL_H_ */
