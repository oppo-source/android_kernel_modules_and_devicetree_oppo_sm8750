#include <linux/module.h>
#include <linux/crc32.h>
#include <media/cam_sensor.h>

#include "cam_eeprom_core.h"
#include "cam_eeprom_soc.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#include "oplus_cam_eeprom.h"

void oplus_cam_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
{
	if (e_ctrl->io_master_info.master_type == CCI_MASTER) {
		if (e_ctrl->io_master_info.cci_client->sid==0x3A) {
			oplus_cam_eeprom_write(e_ctrl);
		}
	}
}

int oplus_cam_eeprom_write(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int                                rc = 0;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings = {0};
	struct cam_sensor_i2c_reg_array    i2c_reg_array = {0};
	CAM_INFO(CAM_EEPROM,"start oplus_cam_eeprom_dw9786");
	e_ctrl->io_master_info.cci_client->sid = 0x19;
	i2c_reg_settings.addr_type = 2;
	i2c_reg_settings.data_type = 2;
	i2c_reg_settings.size = 1;
	i2c_reg_array.reg_addr = 0xE000;
	i2c_reg_array.reg_data = 0;
	i2c_reg_array.delay = 1;
	i2c_reg_settings.reg_setting = &i2c_reg_array;
	rc = camera_io_dev_write(&e_ctrl->io_master_info,&i2c_reg_settings);
	if (rc) {
		CAM_ERR(CAM_EEPROM,"page enable failed rc %d", rc);
	}
	msleep(10);
	e_ctrl->io_master_info.cci_client->sid = 0x19;
	i2c_reg_settings.addr_type = 2;
	i2c_reg_settings.data_type = 2;
	i2c_reg_settings.size = 1;
	i2c_reg_array.reg_addr = 0xE000;
	i2c_reg_array.reg_data = 1;
	i2c_reg_array.delay = 1;
	i2c_reg_settings.reg_setting = &i2c_reg_array;
	rc = camera_io_dev_write(&e_ctrl->io_master_info,&i2c_reg_settings);
	if (rc) {
		CAM_ERR(CAM_EEPROM,"page enable failed rc %d", rc);
	}
	msleep(5);
	e_ctrl->io_master_info.cci_client->sid = 0x19;
	i2c_reg_settings.addr_type = 2;
	i2c_reg_settings.data_type = 2;
	i2c_reg_settings.size = 1;
	i2c_reg_array.reg_addr = 0xE2FC;
	i2c_reg_array.reg_data = 0xAC1E;
	i2c_reg_array.delay = 1;
	i2c_reg_settings.reg_setting = &i2c_reg_array;
	rc = camera_io_dev_write(&e_ctrl->io_master_info,&i2c_reg_settings);
	if (rc) {
		CAM_ERR(CAM_EEPROM,"page enable failed rc %d", rc);
	}
	msleep(1);
	e_ctrl->io_master_info.cci_client->sid = 0x19;
	i2c_reg_settings.addr_type = 2;
	i2c_reg_settings.data_type = 2;
	i2c_reg_settings.size = 1;
	i2c_reg_array.reg_addr = 0xE164;
	i2c_reg_array.reg_data = 8;
	i2c_reg_array.delay = 1;
	i2c_reg_settings.reg_setting = &i2c_reg_array;
	rc = camera_io_dev_write(&e_ctrl->io_master_info,&i2c_reg_settings);
	if (rc) {
		CAM_ERR(CAM_EEPROM,"page enable failed rc %d", rc);
	}
	msleep(1);
	e_ctrl->io_master_info.cci_client->sid = 0x19;
	i2c_reg_settings.addr_type = 2;
	i2c_reg_settings.data_type = 2;
	i2c_reg_settings.size = 1;
	i2c_reg_array.reg_addr = 0xE2FC;
	i2c_reg_array.reg_data = 0;
	i2c_reg_array.delay = 1;
	i2c_reg_settings.reg_setting = &i2c_reg_array;
	rc = camera_io_dev_write(&e_ctrl->io_master_info,&i2c_reg_settings);
	if (rc) {
		CAM_ERR(CAM_EEPROM,"page enable failed rc %d", rc);
	}
	msleep(1);
	e_ctrl->io_master_info.cci_client->sid = 0x19;
	i2c_reg_settings.addr_type = 2;
	i2c_reg_settings.data_type = 1;
	i2c_reg_settings.size = 1;
	i2c_reg_array.reg_addr = 0xE004;
	i2c_reg_array.reg_data = 1;
	i2c_reg_array.delay = 1;
	i2c_reg_settings.reg_setting = &i2c_reg_array;
	rc = camera_io_dev_write(&e_ctrl->io_master_info,&i2c_reg_settings);
	if (rc) {
		CAM_ERR(CAM_EEPROM,"page enable failed rc %d", rc);
	}
	msleep(100);
	e_ctrl->io_master_info.cci_client->sid = 0x3A;
	msleep(5);

	return rc;
}