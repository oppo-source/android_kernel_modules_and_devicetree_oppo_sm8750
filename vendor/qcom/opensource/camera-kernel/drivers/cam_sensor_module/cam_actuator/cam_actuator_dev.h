/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */


#ifndef _CAM_ACTUATOR_DEV_H_
#define _CAM_ACTUATOR_DEV_H_

#include <cam_sensor_io.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <cam_cci_dev.h>
#include <cam_sensor_cmn_header.h>
#include <cam_subdev.h>
#include "cam_sensor_util.h"
#include "cam_soc_util.h"
#include "cam_debug_util.h"
#include "cam_context.h"

#define NUM_MASTERS 2
#define NUM_QUEUES 2

#define ACTUATOR_DRIVER_I2C                    "cam-i2c-actuator"
#define CAMX_ACTUATOR_DEV_NAME                 "cam-actuator-driver"
#define ACTUATOR_DRIVER_I3C                    "i3c_camera_actuator"


#define MSM_ACTUATOR_MAX_VREGS (10)
#define ACTUATOR_MAX_POLL_COUNT 10

#ifdef OPLUS_FEATURE_CAMERA_COMMON
enum cam_af_power_down_thread_state {
	CAM_AF_POWER_DOWN_THREAD_RUNNING,
	CAM_AF_POWER_DOWN_THREAD_STOPPED,
};
#endif

enum cam_actuator_apply_state_t {
	ACT_APPLY_SETTINGS_NOW,
	ACT_APPLY_SETTINGS_LATER,
};

enum cam_actuator_state {
	CAM_ACTUATOR_INIT,
	CAM_ACTUATOR_ACQUIRE,
	CAM_ACTUATOR_CONFIG,
	CAM_ACTUATOR_START,
#ifdef OPLUS_FEATURE_CAMERA_COMMON
	CAM_ACTUATOR_LOCK,
#endif
};

/**
 * struct cam_actuator_i2c_info_t - I2C info
 * @slave_addr      :   slave address
 * @i2c_freq_mode   :   i2c frequency mode
 */
struct cam_actuator_i2c_info_t {
	uint16_t slave_addr;
	uint8_t i2c_freq_mode;
};

struct cam_actuator_soc_private {
	struct cam_actuator_i2c_info_t i2c_info;
	struct cam_sensor_power_ctrl_t power_info;
};

/**
 * struct actuator_intf_params
 * @device_hdl: Device Handle
 * @session_hdl: Session Handle
 * @ops: KMD operations
 * @crm_cb: Callback API pointers
 */
struct actuator_intf_params {
	int32_t device_hdl;
	int32_t session_hdl;
	int32_t link_hdl;
	struct cam_req_mgr_kmd_ops ops;
	struct cam_req_mgr_crm_cb *crm_cb;
};

/**
 * struct cam_actuator_ctrl_t
 * @device_name: Device name
 * @i2c_driver: I2C device info
 * @pdev: Platform device
 * @cci_i2c_master: I2C structure
 * @io_master_info: Information about the communication master
 * @actuator_mutex: Actuator mutex
 * @is_i3c_device : A Flag to indicate whether this actuator is I3C device
 * @act_apply_state: Actuator settings aRegulator config
 * @id: Cell Index
 * @res_apply_state: Actuator settings apply state
 * @cam_act_state:   Actuator state
 * @gconf: GPIO config
 * @pinctrl_info: Pinctrl information
 * @v4l2_dev_str: V4L2 device structure
 * @i2c_data: I2C register settings structure
 * @act_info: Sensor query cap structure
 * @of_node: Node ptr
 * @last_flush_req: Last request to flush
 */
struct cam_actuator_ctrl_t {
	char device_name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	struct i2c_driver *i2c_driver;
	enum cci_i2c_master_t cci_i2c_master;
	enum cci_device_num cci_num;
	struct camera_io_master io_master_info;
	struct cam_hw_soc_info soc_info;
	struct mutex actuator_mutex;
	bool is_i3c_device;
	uint32_t id;
	enum cam_actuator_apply_state_t setting_apply_state;
	enum cam_actuator_state cam_act_state;
	uint8_t cam_pinctrl_status;
	struct cam_subdev v4l2_dev_str;
	struct i2c_data_settings i2c_data;
	struct cam_actuator_query_cap act_info;
	struct actuator_intf_params bridge_intf;
	uint32_t last_flush_req;
#ifdef OPLUS_FEATURE_CAMERA_COMMON
	bool is_actuator_ready;
	struct cam_sensor_i2c_reg_array poll_register;
	enum camera_sensor_i2c_type addr_type;
	enum camera_sensor_i2c_type data_type;
	bool camera_actuator_shake_detect_enable;
	enum cam_actuator_state cam_act_last_state;
	struct mutex actuator_ioctl_mutex;
	struct task_struct *actuator_parklens_thread;
	uint32_t is_af_parklens;
	bool is_update_pid;
	bool is_petrel_ak7316_update_pid;
	struct task_struct *actuator_update_pid_thread;
	struct semaphore actuator_sem;
	bool reactive_ctrl_support;
	struct cam_sensor_i2c_reg_array reactive_reg_array;
	struct cam_sensor_i2c_reg_setting reactive_setting;
	char actuator_name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	uint32_t power_setting_size;
	bool power_custom1_reg;
	bool power_custom2_reg;
	bool power_delay_support;
	uint32_t actuator_function;
	uint32_t cci_client_sid;
	bool sds_lock_support;
	struct cam_sensor_i2c_reg_setting sds_lock_setting;
	uint32_t is_af_ignore_init_error;
	uint32_t min_current;
	uint32_t max_current;
	uint32_t current_register;
	uint32_t pull_gpio;
	bool is_need_read_current;
	bool is_default_high_voltage;
	struct task_struct *actuator_read_current_thread;
	struct mutex af_power_down_mutex;
	enum cam_af_power_down_thread_state af_power_down_thread_state;
#endif

};

/**
 * @brief : API to register Actuator hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_actuator_driver_init(void);

/**
 * @brief : API to remove Actuator Hw from platform framework.
 */
void cam_actuator_driver_exit(void);
#endif /* _CAM_ACTUATOR_DEV_H_ */
