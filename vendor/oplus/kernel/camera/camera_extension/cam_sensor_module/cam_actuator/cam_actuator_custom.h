#ifndef _CAM_ACTUATOR_CUSTOM_H
#define _CAM_ACTUATOR_CUSTOM_H

#include <linux/miscdevice.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>

#include <media/cam_sensor.h>
#include <media/cam_defs.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_io.h>
#include <cam_soc_util.h>
#include <cam_actuator_dev.h>
#include <oplus_cam_actuator.h>

#include "../../include/main.h"
#include "../cam_sensor_io/cam_sensor_io_custom.h"
#include "../cam_sensor_io/cam_sensor_util_custom.h"
#include "../../cam_monitor/cam_monitor.h"


#define VIDIOC_CAM_ACTUATOR_LOCK 0x9003
#define VIDIOC_CAM_ACTUATOR_UNLOCK 0x9004
#define VIDIOC_CAM_ACTUATOR_SHAKE_DETECT_ENABLE 0x9005

#define ACTUATOR_REGISTER_SIZE 10
#define AK7316_DAC_ADDR 0x84

#define SEM1217S_POWER_SIZE 3
#define DW9786_POWER_SIZE 3

typedef struct {
    uint8_t id_reg;
    uint8_t expected_ic_id;
    uint8_t expected_pid_ver;
    uint8_t pid_ver_reg;
    uint8_t write_control_reg;
    uint8_t write_control_data;
    uint8_t check_addr;
    uint8_t check_bit;
} actuator_control_info_t;

int32_t oplus_cam_actuator_parse_dt(struct cam_actuator_ctrl_t *a_ctrl);

void cam_actuator_poll_setting_update(struct cam_actuator_ctrl_t *a_ctrl);
void cam_actuator_poll_setting_apply(struct cam_actuator_ctrl_t *a_ctrl);
void oplus_cam_actuator_parklens_power_down(struct cam_actuator_ctrl_t *a_ctrl);
int actuator_power_down_thread(void *arg);
int oplus_cam_actuator_reactive_setting_apply(struct cam_actuator_ctrl_t *a_ctrl);

int oplus_cam_actuator_ram_write(struct cam_actuator_ctrl_t *a_ctrl, uint32_t addr, uint32_t data);
int oplus_cam_actuator_ram_write_word(struct cam_actuator_ctrl_t *a_ctrl, uint32_t addr, uint32_t data);
int oplus_cam_actuator_ram_read(struct cam_actuator_ctrl_t *a_ctrl, uint32_t addr, uint32_t* data);
int oplus_cam_actuator_ram_read_word(struct cam_actuator_ctrl_t *a_ctrl, uint32_t addr, uint32_t* data);
int oplus_cam_actuator_update_pid_oem(struct cam_actuator_ctrl_t *a_ctrl);
int oplus_cam_actuator_update_pid(void *arg);
void oplus_cam_actuator_sds_enable(struct cam_actuator_ctrl_t *a_ctrl);
int32_t oplus_cam_actuator_lock(struct cam_actuator_ctrl_t *a_ctrl);
int32_t oplus_cam_actuator_unlock(struct cam_actuator_ctrl_t *a_ctrl);
int oplus_cam_actuator_ram_write_extend(struct cam_actuator_ctrl_t *a_ctrl,
	uint32_t addr, uint32_t data,unsigned short mdelay,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);
int oplus_cam_actuator_ram_read_extend(struct cam_actuator_ctrl_t *a_ctrl,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);


void cam_actuator_init(struct cam_actuator_ctrl_t *a_ctrl);
void cam_actuator_register(void);


#endif /* _CAM_ACTUATOR_CUSTOM_H */

