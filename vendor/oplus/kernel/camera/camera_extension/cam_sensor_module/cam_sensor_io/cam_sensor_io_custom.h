#ifndef _CAM_SENSOR_IO_CUSTOM_H
#define _CAM_SENSOR_IO_CUSTOM_H

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

#include "../../include/main.h"

#define I2C_POLL_TIME_MS 5
#define MAX_POLL_DELAY_MS 100

#define I2C_COMPARE_MATCH 0
#define I2C_COMPARE_MISMATCH 1
#define MAX_CCI 3

extern struct v4l2_subdev *cam_cci_get_subdev(int cci_dev_index);
static struct v4l2_subdev *g_ext_cci_subdev[MAX_CCI] = { 0 };

int32_t cam_ext_sensor_cci_i2c_util(struct cam_sensor_cci_client *cci_client,
	uint16_t cci_cmd);

int32_t cam_ext_cci_i2c_poll(struct cam_sensor_cci_client *client,
	uint32_t addr, uint16_t data, uint16_t data_mask,
	enum camera_sensor_i2c_type data_type,
	enum camera_sensor_i2c_type addr_type,
	uint32_t delay_ms);

int32_t cam_ext_cci_i2c_read(struct cam_sensor_cci_client *cci_client,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	bool is_probing);

int32_t cam_ext_cci_i2c_compare(struct cam_sensor_cci_client *client,
	uint32_t addr, uint16_t data, uint16_t data_mask,
	enum camera_sensor_i2c_type data_type,
	enum camera_sensor_i2c_type addr_type);

int32_t cam_ext_cci_i2c_poll(struct cam_sensor_cci_client *client,
	uint32_t addr, uint16_t data, uint16_t data_mask,
	enum camera_sensor_i2c_type data_type,
	enum camera_sensor_i2c_type addr_type,
	uint32_t delay_ms);

int32_t cam_ext_io_dev_poll(struct camera_io_master *io_master_info,
	uint32_t addr, uint16_t data, uint32_t data_mask,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	uint32_t delay_ms);

int32_t cam_ext_io_dev_read(struct cam_sensor_cci_client *client,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	bool is_probing);

int32_t cam_ext_cci_i2c_write_table_cmd(
		struct cam_sensor_cci_client *client,
		struct cam_sensor_i2c_reg_setting *write_setting,
		enum cam_cci_cmd_type cmd);

int32_t cam_ext_cci_i2c_write_table(
		struct cam_sensor_cci_client *client,
		struct cam_sensor_i2c_reg_setting *write_setting);

int32_t cam_ext_cci_i2c_write_continuous_table(
		struct cam_sensor_cci_client *client,
		struct cam_sensor_i2c_reg_setting *write_setting,
		u8 cam_sensor_i2c_write_flag);

int32_t cam_ext_io_dev_write(struct cam_sensor_cci_client *client,
		struct cam_sensor_i2c_reg_setting *write_setting);

void cam_ext_cci_set_subdev(void);

int32_t cam_ext_io_init(struct cam_sensor_cci_client *client);

int32_t cam_ext_io_release(struct cam_sensor_cci_client *client);


#define VIDIOC_MSM_CCI_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 23, struct cam_cci_ctrl)

#endif /* _CAM_SENSOR_IO_CUSTOM_H */

