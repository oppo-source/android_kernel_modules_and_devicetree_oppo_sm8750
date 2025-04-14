/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _OPLUS_CAM_SENSOR_H_
#define _OPLUS_CAM_SENSOR_H_
#include "cam_sensor_dev.h"

enum cam_oem_sensor_type {
	CAM_SENSOR_TYPE_STREAM_OFF = 0,
	CAM_SENSOR_TYPE_IMX615,
	CAM_SENSOR_TYPE_IMX615_FAB2,
	CAM_SENSOR_TYPE_IMX766,
	CAM_SENSOR_TYPE_IMX709,
	CAM_SENSOR_TYPE_IMX709_AON_IRQ,
	CAM_SENSOR_TYPE_IMX709_ANO_HE_CLR,
	CAM_SENSOR_TYPE_IMX581,
	CAM_SENSOR_TYPE_IMX989,
	CAM_SENSOR_TYPE_IMX890,
	CAM_SENSOR_TYPE_IMX888,
	CAM_SENSOR_TYPE_IMXOV64B0,
	CAM_SENSOR_TYPE_IMX858,
	CAM_SENSOR_TYPE_IMX966,
	CAM_SENSOR_TYPE_IMX06A,
	CAM_SENSOR_TYPE_IMX355,
	CAM_SENSOR_TYPE_IMX882,
	CAM_SENSOR_TYPE_MAX
};

int cam_sensor_read_qsc(struct cam_sensor_ctrl_t *s_ctrl);

int cam_sensor_get_dpc_data(struct cam_sensor_ctrl_t *s_ctrl);

int oplus_cam_sensor_apply_settings(struct cam_sensor_ctrl_t *s_ctrl);

int oplus_sensor_sony_get_dpc_data(struct cam_sensor_ctrl_t *s_ctrl);

void oplus_sensor_ov_bypass_framedrop(struct cam_sensor_ctrl_t *s_ctrl,enum cam_sensor_packet_opcodes opcode,struct i2c_settings_list *i2c_list);

void oplus_sensor_sony_bypass_vsync(struct cam_sensor_ctrl_t *s_ctrl,struct i2c_settings_list *i2c_list);

void oplus_sensor_sony_get_vsync_data(struct device_node *of_node,struct sensor_vsync_info *vsync_info);
#endif /* _OPLUS_CAM_SENSOR_H_ */