#ifndef _OPLUS_CAM_ACTUATOR_H
#define _OPLUS_CAM_ACTUATOR_H
#include "cam_actuator_dev.h"

#define DEFULT_POWER_SIZE 2
int32_t oplus_cam_actuator_ignore_init_error(struct cam_actuator_ctrl_t *a_ctrl, int32_t rc);
int32_t oplus_cam_actuator_construct_default_power_setting(struct cam_actuator_ctrl_t *a_ctrl, struct cam_sensor_power_ctrl_t *power_info);
int32_t oplus_cam_actuator_power_up(struct cam_actuator_ctrl_t *a_ctrl, struct cam_sensor_power_ctrl_t *power_info);
int oplus_cam_actuator_read_current(void *arg);
#endif /* _OPLUS_CAM_EEPROM_H */
