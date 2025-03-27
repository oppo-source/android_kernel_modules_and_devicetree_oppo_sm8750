#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/host.h>
#include <linux/delay.h>
#include <linux/thermal.h>
#include <linux/time.h>
#include <linux/timekeeping.h>
#include <linux/suspend.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/sysfs.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>

#include "cam_sensor_io_custom.h"
#include "cam_sensor_util_custom.h"
#include "cam_debug.h"

int32_t cam_ext_sensor_cci_i2c_util(struct cam_sensor_cci_client *cci_client,
	uint16_t cci_cmd)
{
	int32_t rc = 0;
	struct cam_cci_ctrl cci_ctrl = {0};

	if (!cci_client)
		return -EINVAL;

	cci_ctrl.cmd = cci_cmd;
	cci_ctrl.cci_info = cci_client;
	rc = v4l2_subdev_call(cci_client->cci_subdev,
		core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
	if (rc < 0) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Failed rc = %d", __func__, rc);
		return rc;
	}
	return cci_ctrl.status;
}

int32_t cam_ext_cci_i2c_read(struct cam_sensor_cci_client *cci_client,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	bool is_probing)
{
	int32_t rc = -EINVAL;
	unsigned char buf[CAMERA_SENSOR_I2C_TYPE_DWORD];
	struct cam_cci_ctrl cci_ctrl;

	if (addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX
		|| data_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| data_type >= CAMERA_SENSOR_I2C_TYPE_MAX)
		return rc;

	cci_ctrl.is_probing = is_probing;
	cci_ctrl.cmd = MSM_CCI_I2C_READ;
	cci_ctrl.cci_info = cci_client;
	cci_ctrl.cfg.cci_i2c_read_cfg.addr = addr;
	cci_ctrl.cfg.cci_i2c_read_cfg.addr_type = addr_type;
	cci_ctrl.cfg.cci_i2c_read_cfg.data_type = data_type;
	cci_ctrl.cfg.cci_i2c_read_cfg.data = buf;
	cci_ctrl.cfg.cci_i2c_read_cfg.num_byte = data_type;
	rc = v4l2_subdev_call(cci_client->cci_subdev,
		core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
	if (rc < 0) {
		if (is_probing)
			CAM_EXT_INFO(CAM_EXT_SENSOR, "rc = %d", rc);
		else
			CAM_EXT_ERR(CAM_EXT_SENSOR, "rc = %d", rc);
		return rc;
	}

	rc = cci_ctrl.status;
	if (data_type == CAMERA_SENSOR_I2C_TYPE_BYTE)
		*data = buf[0];
	else if (data_type == CAMERA_SENSOR_I2C_TYPE_WORD)
		*data = buf[0] << 8 | buf[1];
	else if (data_type == CAMERA_SENSOR_I2C_TYPE_3B)
		*data = buf[0] << 16 | buf[1] << 8 | buf[2];
	else
		*data = buf[0] << 24 | buf[1] << 16 |
			buf[2] << 8 | buf[3];

	return rc;
}

int32_t cam_ext_cci_i2c_compare(struct cam_sensor_cci_client *client,
	uint32_t addr, uint16_t data, uint16_t data_mask,
	enum camera_sensor_i2c_type data_type,
	enum camera_sensor_i2c_type addr_type)
{
	int32_t rc;
	uint32_t reg_data = 0;

	rc = cam_ext_cci_i2c_read(client, addr, &reg_data,
		addr_type, data_type,false);
	if (rc < 0)
		return rc;

	reg_data = reg_data & 0xFFFF;
	if (data == (reg_data & ~data_mask))
		return I2C_COMPARE_MATCH;
	else {
		CAM_EXT_WARN(CAM_EXT_SENSOR, "mismatch: reg_data 0x%x: data: 0x%x, data_mask: 0x%x",
			reg_data, data, data_mask);
		return I2C_COMPARE_MISMATCH;
	}
}

int32_t cam_ext_cci_i2c_poll(struct cam_sensor_cci_client *client,
	uint32_t addr, uint16_t data, uint16_t data_mask,
	enum camera_sensor_i2c_type data_type,
	enum camera_sensor_i2c_type addr_type,
	uint32_t delay_ms)
{
	int32_t rc = -EINVAL;
	int32_t i = 0;

	CAM_EXT_INFO(CAM_EXT_SENSOR, "addr: 0x%x data: 0x%x dt: %d",
		addr, data, data_type);

	if (delay_ms > MAX_POLL_DELAY_MS) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "invalid delay = %d max_delay = %d",
			delay_ms, MAX_POLL_DELAY_MS);
		return -EINVAL;
	}
	for (i = 0; i < delay_ms; i++) {
		rc = cam_ext_cci_i2c_compare(client,
			addr, data, data_mask, data_type, addr_type);
		if (!rc)
			return rc;

		usleep_range(1000, 1010);
	}

	/* If rc is 1 then read is successful but poll is failure */
	if (rc == 1)
		CAM_EXT_ERR(CAM_EXT_SENSOR, "poll failed rc=%d(non-fatal)",	rc);

	if (rc < 0)
		CAM_EXT_ERR(CAM_EXT_SENSOR, "poll failed rc=%d", rc);

	return rc;
}

int32_t cam_ext_io_dev_poll(struct camera_io_master *io_master_info,
	uint32_t addr, uint16_t data, uint32_t data_mask,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	uint32_t delay_ms)
{
	int16_t mask = data_mask & 0xFF;

	if (!io_master_info) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	switch (io_master_info->master_type) {
	case CCI_MASTER:
		return cam_ext_cci_i2c_poll(io_master_info->cci_client,
			addr, data, mask, data_type, addr_type, delay_ms);
	default:
		CAM_EXT_ERR(CAM_EXT_SENSOR, "Invalid Master Type: %d", io_master_info->master_type);
	}

	return -EINVAL;
}

int32_t cam_ext_io_dev_read(struct cam_sensor_cci_client *client,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	bool is_probing)
{
	if (!client) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Invalid Args", __func__);
		return -EINVAL;
	}

	return cam_ext_cci_i2c_read(client,
		addr, data, addr_type, data_type, is_probing);
}

int32_t cam_ext_cci_i2c_write_table_cmd(
	struct cam_sensor_cci_client *client,
	struct cam_sensor_i2c_reg_setting *write_setting,
	enum cam_cci_cmd_type cmd)
{
	int32_t rc = -EINVAL;
	struct cam_cci_ctrl cci_ctrl = {0};

	if (!client || !write_setting)
		return rc;

	if (write_setting->addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| write_setting->addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX
		|| write_setting->data_type <= CAMERA_SENSOR_I2C_TYPE_INVALID
		|| write_setting->data_type >= CAMERA_SENSOR_I2C_TYPE_MAX)
		return rc;

	cci_ctrl.cmd = cmd;
	cci_ctrl.cci_info = client;
	cci_ctrl.cfg.cci_i2c_write_cfg.reg_setting =
		write_setting->reg_setting;
	cci_ctrl.cfg.cci_i2c_write_cfg.data_type = write_setting->data_type;
	cci_ctrl.cfg.cci_i2c_write_cfg.addr_type = write_setting->addr_type;
	cci_ctrl.cfg.cci_i2c_write_cfg.size = write_setting->size;
	rc = v4l2_subdev_call(client->cci_subdev,
		core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
	CAM_EXT_INFO(CAM_EXT_SENSOR, "%s v4l2_subdev_call rc=%d", __func__, rc);
	if (rc < 0) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Failed rc = %d", __func__, rc);
		return rc;
	}
	rc = cci_ctrl.status;
	if (write_setting->delay > 20)
		msleep(write_setting->delay);
	else if (write_setting->delay)
		usleep_range(write_setting->delay * 1000, (write_setting->delay
			* 1000) + 1000);

	return rc;
}

int32_t cam_ext_cci_i2c_write_table(
	struct cam_sensor_cci_client *client,
	struct cam_sensor_i2c_reg_setting *write_setting)
{
	return cam_ext_cci_i2c_write_table_cmd(client, write_setting,
		MSM_CCI_I2C_WRITE);
}

int32_t cam_ext_cci_i2c_write_continuous_table(
	struct cam_sensor_cci_client *client,
	struct cam_sensor_i2c_reg_setting *write_setting,
	u8 cam_sensor_i2c_write_flag)
{
	int32_t rc = 0;

	if (cam_sensor_i2c_write_flag == 1)
		rc = cam_ext_cci_i2c_write_table_cmd(client, write_setting,
			MSM_CCI_I2C_WRITE_BURST);
	else if (cam_sensor_i2c_write_flag == 0)
		rc = cam_ext_cci_i2c_write_table_cmd(client, write_setting,
			MSM_CCI_I2C_WRITE_SEQ);

	return rc;
}

int32_t cam_ext_io_dev_write(struct cam_sensor_cci_client *client,
	struct cam_sensor_i2c_reg_setting *write_setting)
{
	if (!write_setting || !client) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Input parameters not valid ws: %p ioinfo: %p", __func__,
			write_setting, client);
		return -EINVAL;
	}

	if (!write_setting->reg_setting) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Invalid Register Settings", __func__);
		return -EINVAL;
	}

	return cam_ext_cci_i2c_write_table(client, write_setting);
}

void cam_ext_cci_set_subdev(void)
{
	for(int i = 0;i < MAX_CCI; i++)
	{
		g_ext_cci_subdev[i] = cam_cci_get_subdev(i);
	}
}


int32_t cam_ext_io_init(struct cam_sensor_cci_client *client)
{
	if (!client) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Invalid Args", __func__);
		return -EINVAL;
	}

	CAM_EXT_INFO(CAM_EXT_SENSOR, "%s client->cci_device: %d",
		__func__, client->cci_device);

	cam_ext_cci_set_subdev();

	if ((client->cci_device < MAX_CCI) && (g_ext_cci_subdev[client->cci_device] != NULL))
	{
		client->cci_subdev = g_ext_cci_subdev[client->cci_device];
	}
	else
	{
		CAM_EXT_ERR(CAM_EXT_SENSOR, "CCI subdev not available at Index: %u, MAX_CCI : %u",
			client->cci_device , MAX_CCI);
		client->cci_subdev = NULL;
	}

	CAM_EXT_INFO(CAM_EXT_SENSOR, "%s cci_subdev: %p", __func__, client->cci_subdev);

	return cam_ext_sensor_cci_i2c_util(client, MSM_CCI_INIT);
}

int32_t cam_ext_io_release(struct cam_sensor_cci_client *client)
{
	if (!client) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Invalid Args", __func__);
		return -EINVAL;
	}

	return cam_ext_sensor_cci_i2c_util(client, MSM_CCI_RELEASE);
}


