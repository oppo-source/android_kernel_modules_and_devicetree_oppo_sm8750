// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */
#ifndef _OPLUS_CAM_OIS_H_
#define _OPLUS_CAM_OIS_H_
#include "cam_ois_dev.h"

int OIS_READ_HALL_DATA_TO_UMD_Bu24721 (struct cam_ois_ctrl_t *o_ctrl,struct i2c_settings_array *i2c_settings);
int OIS_READ_HALL_DATA_TO_UMD_SEM1217S (struct cam_ois_ctrl_t *o_ctrl,struct i2c_settings_array *i2c_settings);
int OIS_READ_HALL_DATA_TO_UMD_DW9786 (struct cam_ois_ctrl_t *o_ctrl,struct i2c_settings_array *i2c_settings);
#endif /* _OPLUS_CAM_OIS_H_ */