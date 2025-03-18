
#ifndef _CAMERA_EXTENSION_H
#define _CAMERA_EXTENSION_H

#include <linux/miscdevice.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include "uapi/cam_extension_uapi.h"

#include <media/cam_sensor.h>
#include <media/cam_defs.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_io.h>
#include <cam_soc_util.h>


#define EXTENSION_LINE_MAX_LENGTH	256
#define MONITOR_SOC_MAX_REGULATOR	36

#define REGULATOR_STATES_NUM (PM_SUSPEND_MAX + 1)

#define CCI_MASTER           1
#define I2C_MASTER           2
#define SPI_MASTER           3
#define I3C_MASTER           4

struct regulator_voltage {
	int min_uV;
	int max_uV;
};

struct regulator {
	struct device *dev;
	struct list_head list;
	unsigned int always_on:1;
	unsigned int bypass:1;
	unsigned int device_link:1;
	int uA_load;
	unsigned int enable_count;
	unsigned int deferred_disables;
	struct regulator_voltage voltage[REGULATOR_STATES_NUM];
	const char *supply_name;
	struct device_attribute dev_attr;
	struct regulator_dev *rdev;
	struct dentry *debugfs;
};

struct debug_regulator {
	struct list_head list;
	char name[100];
	int use_count;
};

struct debug_clock {
	struct list_head list;
	char name[100];
	int use_count;
};

struct debug_state {
	struct list_head list;
	char		dev_uid[64];
	void*		dev_ptr;
	short 		seq_type[(SENSOR_SEQ_TYPE_MAX - 1) * 2];
	short 		seq_val[(SENSOR_SEQ_TYPE_MAX - 1) * 2];
	enum camera_extension_feature_type type;
	int use_count;
};

enum cam_operation_type {
	CAM_OPERATION_TYPE_SUMMARY,
	CAM_OPERATION_TYPE_STATE,
	CAM_OPERATION_TYPE_MONITOR,
	CAM_OPERATION_TYPE_REGULATOR,
	CAM_OPERATION_TYPE_CLOCK,
	CAM_OPERATION_TYPE_MAX
};

struct cam_state_monitor {
	enum cam_operation_type type;
	struct debug_regulator regulator;
	struct debug_clock clock;
	struct debug_state state;
	uint32_t enable;
	uint32_t ref_count;
	struct timespec64 timestamp;
};

struct cam_state_queue_info {
	atomic64_t                state_monitor_head;
	struct cam_state_monitor *state_monitor;
};

struct regulator_info {
	char* regulator_name;
	int   use_count;
};

struct camera_extension_data {
	struct platform_device *plat_dev;
	struct miscdevice dev;
	bool   enable_camera_extension;
	struct cam_state_queue_info state_queue;
};


#endif /* _CAMERA_EXTENSION_H */
