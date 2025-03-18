#ifndef _CAMERA_MONITOR_H
#define _CAMERA_MONITOR_H


#include "../include/main.h"
#include "cam_debug.h"

struct monitor_power_setting {
	char power[36];
	long config_val;
	unsigned short delay;
	bool valid_config;
};

ssize_t monitor_format_oneline(
	struct cam_state_queue_info *state_queue_info,
	int index,
	uint32_t num_entries,
	char * buf);

ssize_t extension_dump_item(
	enum cam_operation_type item,
	char* buf,
	int32_t bufsize);

void extension_dump_monitor_print(
	void);

void check_power_exception(
	struct monitor_check *r);

int32_t cam_extension_dump_process(
	struct extension_control *cmd);

int32_t cam_extension_check_process(
	struct extension_control *cmd);

int create_sysfs(void);

int init_cam_monitor(struct platform_device *plat_dev);

int set_camera_state(struct debug_state state, bool enable);

void oplus_cam_monitor_state(
	void *subdev_ctrl,
	u32 ent_function,
	enum camera_extension_feature_type type,
	bool enable);
#endif /* _CAMERA_MONITOR_H */

