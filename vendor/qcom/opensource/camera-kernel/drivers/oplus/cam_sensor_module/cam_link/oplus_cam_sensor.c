// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include "cam_sensor_util.h"
#include "cam_sensor_dev.h"
#include "cam_sensor_soc.h"
#include "cam_sensor_core.h"
#include "oplus_cam_sensor.h"

#define OVSENSOR_FRAMEDROP_ADDR 0x15
#define OVSENSOR_FRAMEDROP_ENABLE 0x10
#define OVSENSOR_FRAMEDROP_DISABLE 0x00

struct sony_dfct_tbl_t sony_dfct_tbl;

int cam_sensor_read_qsc(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc=0;

	s_ctrl->sensor_qsc_setting.read_qsc_success = false;
	if(s_ctrl->sensor_qsc_setting.enable_qsc_write_in_advance)
	{
		uint8_t      *data;
		uint16_t     temp_slave_id = 0;
		int          i=0;

		data =  kzalloc(sizeof(uint8_t)*s_ctrl->sensor_qsc_setting.qsc_data_size, GFP_KERNEL);
		if(!data)
		{
			CAM_ERR(CAM_SENSOR,"kzalloc data failed");
			s_ctrl->sensor_qsc_setting.read_qsc_success = false;
			return rc;
		}
		temp_slave_id = s_ctrl->io_master_info.cci_client->sid;
		s_ctrl->io_master_info.cci_client->sid = (s_ctrl->sensor_qsc_setting.eeprom_slave_addr >> 1);
		rc = camera_io_dev_read_seq(&s_ctrl->io_master_info,
					s_ctrl->sensor_qsc_setting.qsc_reg_addr,data,
					CAMERA_SENSOR_I2C_TYPE_WORD,
					CAMERA_SENSOR_I2C_TYPE_BYTE,
					s_ctrl->sensor_qsc_setting.qsc_data_size);
		s_ctrl->io_master_info.cci_client->sid = temp_slave_id;
		if(rc)
		{
			CAM_ERR(CAM_SENSOR,"read qsc data failed");
			s_ctrl->sensor_qsc_setting.read_qsc_success = false;
			kfree(data);
			return rc;
		}

		if(s_ctrl->sensor_qsc_setting.qsc_setting.reg_setting == NULL)
		{
			s_ctrl->sensor_qsc_setting.qsc_setting.reg_setting = kzalloc(sizeof(struct cam_sensor_i2c_reg_array)*s_ctrl->sensor_qsc_setting.qsc_data_size, GFP_KERNEL);
			if (!s_ctrl->sensor_qsc_setting.qsc_setting.reg_setting)
			{
				CAM_ERR(CAM_SENSOR,"allocate qsc data failed");
				s_ctrl->sensor_qsc_setting.read_qsc_success = false;
				kfree(data);
				return rc;
			}
		}

		for(i = 0;i < s_ctrl->sensor_qsc_setting.qsc_data_size;i++)
		{
			s_ctrl->sensor_qsc_setting.qsc_setting.reg_setting[i].reg_addr = s_ctrl->sensor_qsc_setting.write_qsc_addr;
			s_ctrl->sensor_qsc_setting.qsc_setting.reg_setting[i].reg_data = *(data+i);
			s_ctrl->sensor_qsc_setting.qsc_setting.reg_setting[i].data_mask= 0x00;
			s_ctrl->sensor_qsc_setting.qsc_setting.reg_setting[i].delay    = 0x00;
			CAM_DBG(CAM_SENSOR,"read qsc data 0x%x i=%d",s_ctrl->sensor_qsc_setting.qsc_setting.reg_setting[i].reg_data,i);
		}

		s_ctrl->sensor_qsc_setting.qsc_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		s_ctrl->sensor_qsc_setting.qsc_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		s_ctrl->sensor_qsc_setting.qsc_setting.size = s_ctrl->sensor_qsc_setting.qsc_data_size;
		s_ctrl->sensor_qsc_setting.qsc_setting.delay = 1;

		s_ctrl->sensor_qsc_setting.read_qsc_success = true;
		kfree(data);
	}

	return 0;
}

#ifdef OPLUS_FEATURE_CAMERA_COMMON
EXPORT_SYMBOL(cam_sensor_read_qsc);
#endif

int cam_sensor_get_dpc_data(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t chipid = 0;
	struct cam_camera_slave_info *slave_info;

	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!slave_info) {
		CAM_ERR(CAM_SENSOR, "get_dpc_data failed: %pK",
			 slave_info);
		return -EINVAL;
	}

	if (s_ctrl->io_master_info.master_type == I2C_MASTER) {
		if (s_ctrl->probe_sensor_slave_addr != 0) {
			s_ctrl->io_master_info.qup_client->i2c_client->addr =
				s_ctrl->probe_sensor_slave_addr;
		}
	} else if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
		if (s_ctrl->probe_sensor_slave_addr != 0) {
			s_ctrl->io_master_info.cci_client->sid =
				s_ctrl->probe_sensor_slave_addr >> 1;
		}
	}

	rc = camera_io_dev_read(
		&(s_ctrl->io_master_info),
		slave_info->sensor_id_reg_addr,
		&chipid, s_ctrl->sensor_probe_addr_type,
		s_ctrl->sensor_probe_data_type, true);

	if (chipid == s_ctrl->swremosaic_sensor_id)
	{
		CAM_INFO(CAM_SENSOR, "sensor 0x%x need to get dpc data", chipid);
		rc = oplus_sensor_sony_get_dpc_data(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "0x%x get dpc data failed",
				chipid);
			return rc;
		}
	}
	else
	{
		CAM_WARN(CAM_SENSOR, "0x%x no need to get dpc data, swremosaic_sensor_id: 0x%x",
				chipid,
				s_ctrl->swremosaic_sensor_id);
	}

	return rc;
}

#ifdef OPLUS_FEATURE_CAMERA_COMMON
EXPORT_SYMBOL(cam_sensor_get_dpc_data);
#endif

bool oplus_cam_sensor_bypass_qsc(struct cam_sensor_ctrl_t *s_ctrl)
{
	bool is_need_bypass = false;
	if(s_ctrl->sensor_qsc_setting.enable_qsc_write_in_advance
		&& s_ctrl->sensor_qsc_setting.read_qsc_success)
	{
		if(s_ctrl->sensor_qsc_setting.qscsetting_state == CAM_SENSOR_SETTING_WRITE_INVALID)
		{
			CAM_INFO(CAM_SENSOR, "qsc setting write failed before ,need write again");
			return is_need_bypass;
		}
		else
		{
			CAM_INFO(CAM_SENSOR, "qsc setting have write  before , no need write again");
			is_need_bypass = true;
			return is_need_bypass;
		}
	}
	else
	{
		CAM_INFO(CAM_SENSOR, "no need compare,don't bypass");
		return is_need_bypass;
	}
}

int oplus_cam_sensor_apply_settings(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	if (s_ctrl->i2c_data.qsc_settings.is_settings_valid &&
		(s_ctrl->i2c_data.qsc_settings.request_id == 0)) {
		if (s_ctrl->sensor_initsetting_state == CAM_SENSOR_SETTING_WRITE_SUCCESS) {
			if(!oplus_cam_sensor_bypass_qsc(s_ctrl))
			{
				trace_int("KMD_QSC1", 1);
				rc = cam_sensor_apply_settings(s_ctrl, 0, CAM_SENSOR_PACKET_OPCODE_SENSOR_QSC);
				trace_int("KMD_QSC1", 0);
				if (rc) {
					CAM_INFO(CAM_SENSOR, "%s:retry apply qsc settings %d", s_ctrl->sensor_name, rc);
					trace_int("KMD_QSC2", 1);
					rc = cam_sensor_apply_settings(s_ctrl, 0, CAM_SENSOR_PACKET_OPCODE_SENSOR_QSC);
					trace_int("KMD_QSC2", 0);
					if (rc) {
						CAM_INFO(CAM_SENSOR, "%s: Failed apply qsc settings %d",
							s_ctrl->sensor_name, rc);
						delete_request(&s_ctrl->i2c_data.qsc_settings);
						return rc;
					}
				}
				rc = delete_request(&s_ctrl->i2c_data.qsc_settings);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR, "%s: Fail in deleting the qsc settings",
						s_ctrl->sensor_name);
					return rc;
				}
			}
			else
			{
				rc = delete_request(&s_ctrl->i2c_data.qsc_settings);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR, "%s: Fail in deleting the qsc settings",
						s_ctrl->sensor_name);
					return rc;
				}
				CAM_INFO(CAM_SENSOR, "%s: cam_sensor_bypass_qsc", s_ctrl->sensor_name);
			}
		}
		else
			CAM_ERR(CAM_SENSOR, "%s: init setting not readly",s_ctrl->sensor_name);
	}
	if (s_ctrl->i2c_data.spc_settings.is_settings_valid &&
		(s_ctrl->i2c_data.spc_settings.request_id == 0)) {
		if (s_ctrl->sensor_initsetting_state == CAM_SENSOR_SETTING_WRITE_SUCCESS) {
			trace_int("KMD_SPC1", 1);
			rc = cam_sensor_apply_settings(s_ctrl, 0, CAM_SENSOR_PACKET_OPCODE_SENSOR_SPC);
			trace_int("KMD_SPC1", 0);
			if (rc) {
				CAM_WARN(CAM_SENSOR, "%s:retry apply spc settings %d", s_ctrl->sensor_name, rc);
				trace_int("KMD_SPC2", 1);
				rc = cam_sensor_apply_settings(s_ctrl, 0, CAM_SENSOR_PACKET_OPCODE_SENSOR_SPC);
				trace_int("KMD_SPC2", 0);
				if (rc) {
					CAM_WARN(CAM_SENSOR, "%s: Failed apply spc settings %d",
						s_ctrl->sensor_name, rc);
					delete_request(&s_ctrl->i2c_data.spc_settings);
					return rc;
				}
			}
			rc = delete_request(&s_ctrl->i2c_data.spc_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,"%s: Fail in deleting the spc settings",
					s_ctrl->sensor_name);
				return rc;
			}
		}
		else
			CAM_ERR(CAM_SENSOR,"%s: init setting not readly",s_ctrl->sensor_name);
	}
	if (s_ctrl->i2c_data.awbotp_settings.is_settings_valid &&
		(s_ctrl->i2c_data.awbotp_settings.request_id == 0)) {
		if (s_ctrl->sensor_initsetting_state == CAM_SENSOR_SETTING_WRITE_SUCCESS) {
			trace_int("KMD_AWB1", 1);
			rc = cam_sensor_apply_settings(s_ctrl, 0, CAM_SENSOR_PACKET_OPCODE_SENSOR_AWBOTP);
			trace_int("KMD_AWB1", 0);
			if (rc) {
				CAM_INFO(CAM_SENSOR, "%s:retry apply awb settings %d", s_ctrl->sensor_name, rc);
				trace_int("KMD_AWB2", 1);
				rc = cam_sensor_apply_settings(s_ctrl, 0, CAM_SENSOR_PACKET_OPCODE_SENSOR_AWBOTP);
				trace_int("KMD_AWB2", 0);
				if (rc) {
					CAM_INFO(CAM_SENSOR, "%s: Failed apply awb settings %d",
						s_ctrl->sensor_name, rc);
					delete_request(&s_ctrl->i2c_data.awbotp_settings);
					return rc;
				}
			}
			rc = delete_request(&s_ctrl->i2c_data.awbotp_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "%s: Fail in deleting the awb settings",
					s_ctrl->sensor_name);
				return rc;
			}
		}
		else
			CAM_ERR(CAM_SENSOR, "%s: init setting not readly",s_ctrl->sensor_name);
	}
	if (s_ctrl->i2c_data.lsc_settings.is_settings_valid &&
		(s_ctrl->i2c_data.lsc_settings.request_id == 0)) {
		if (s_ctrl->sensor_initsetting_state == CAM_SENSOR_SETTING_WRITE_SUCCESS) {
			trace_int("KMD_LSC1", 1);
			rc = cam_sensor_apply_settings(s_ctrl, 0, CAM_SENSOR_PACKET_OPCODE_SENSOR_LSC);
			trace_int("KMD_LSC1", 0);
			if (rc) {
				CAM_INFO(CAM_SENSOR, "%s:retry apply lsc settings %d", s_ctrl->sensor_name, rc);
				trace_int("KMD_LSC2", 1);
				rc = cam_sensor_apply_settings(s_ctrl, 0, CAM_SENSOR_PACKET_OPCODE_SENSOR_LSC);
				trace_int("KMD_LSC2", 0);
				if (rc) {
					CAM_INFO(CAM_SENSOR, "%s: Failed apply lsc settings %d",
						s_ctrl->sensor_name, rc);
					delete_request(&s_ctrl->i2c_data.lsc_settings);
					return rc;
				}
			}
			rc = delete_request(&s_ctrl->i2c_data.lsc_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "%s: Fail in deleting the lsc settings",
					s_ctrl->sensor_name);
				return rc;
			}
		}
		else
			CAM_ERR(CAM_SENSOR, "%s: init setting not readly",s_ctrl->sensor_name);
	}
	if (s_ctrl->i2c_data.resolution_settings.is_settings_valid &&
		(s_ctrl->i2c_data.resolution_settings.request_id == 0)) {
		if (s_ctrl->sensor_initsetting_state == CAM_SENSOR_SETTING_WRITE_SUCCESS) {
			trace_int("KMD_RES1", 1);
			rc = cam_sensor_apply_settings(s_ctrl, 0, CAM_SENSOR_PACKET_OPCODE_SENSOR_RESOLUTION);
			trace_int("KMD_RES1", 0);
			if (rc) {
				CAM_INFO(CAM_SENSOR, "%s:retry apply res settings %d", s_ctrl->sensor_name, rc);
				trace_int("KMD_RES2", 1);
				rc = cam_sensor_apply_settings(s_ctrl, 0, CAM_SENSOR_PACKET_OPCODE_SENSOR_RESOLUTION);
				trace_int("KMD_RES2", 0);
				if (rc) {
					CAM_INFO(CAM_SENSOR, "%s: Failed apply res settings %d",
						s_ctrl->sensor_name, rc);
					delete_request(&s_ctrl->i2c_data.resolution_settings);
					return rc;
				}
			}
			rc = delete_request(&s_ctrl->i2c_data.resolution_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "%s: Fail in deleting the res settings",
					s_ctrl->sensor_name);
				return rc;
			}
		}
		else
			CAM_ERR(CAM_SENSOR, "%s: init setting not readly",s_ctrl->sensor_name);
	}
	return rc;
}

int oplus_sensor_sony_get_dpc_data(struct cam_sensor_ctrl_t *s_ctrl)
{
	int i = 0, j = 0;
	int rc = 0;
	int check_reg_val, dfct_data_h, dfct_data_l;
	int dfct_data = 0;
	int fd_dfct_num = 0, sg_dfct_num = 0;
	int retry_cnt = 5;
	int data_h = 0, data_v = 0;
	int fd_dfct_addr = FD_DFCT_ADDR;
	int sg_dfct_addr = SG_DFCT_ADDR;

	CAM_INFO(CAM_SENSOR, "oplus_sensor_sony_get_dpc_data enter");
	if (s_ctrl == NULL) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		return -EINVAL;
	}

	memset(&sony_dfct_tbl, 0, sizeof(struct sony_dfct_tbl_t));

	for (i = 0; i < retry_cnt; i++) {
		check_reg_val = 0;
		rc = camera_io_dev_read(&(s_ctrl->io_master_info),
			FD_DFCT_NUM_ADDR, &check_reg_val,
			CAMERA_SENSOR_I2C_TYPE_WORD,
			CAMERA_SENSOR_I2C_TYPE_BYTE,
			FALSE);

		if (0 == rc) {
			fd_dfct_num = check_reg_val & 0x07;
			if (fd_dfct_num > FD_DFCT_MAX_NUM)
				fd_dfct_num = FD_DFCT_MAX_NUM;
			break;
		}
	}

	for (i = 0; i < retry_cnt; i++) {
		check_reg_val = 0;
		rc = camera_io_dev_read(&(s_ctrl->io_master_info),
			SG_DFCT_NUM_ADDR, &check_reg_val,
			CAMERA_SENSOR_I2C_TYPE_WORD,
			CAMERA_SENSOR_I2C_TYPE_WORD,
			FALSE);

		if (0 == rc) {
			sg_dfct_num = check_reg_val & 0x01FF;
			if (sg_dfct_num > SG_DFCT_MAX_NUM)
				sg_dfct_num = SG_DFCT_MAX_NUM;
			break;
		}
	}

	CAM_INFO(CAM_SENSOR, " fd_dfct_num = %d, sg_dfct_num = %d", fd_dfct_num, sg_dfct_num);
	sony_dfct_tbl.fd_dfct_num = fd_dfct_num;
	sony_dfct_tbl.sg_dfct_num = sg_dfct_num;

	if (fd_dfct_num > 0) {
		for (j = 0; j < fd_dfct_num; j++) {
			dfct_data = 0;
			for (i = 0; i < retry_cnt; i++) {
				dfct_data_h = 0;
				rc = camera_io_dev_read(&(s_ctrl->io_master_info),
					fd_dfct_addr, &dfct_data_h,
					CAMERA_SENSOR_I2C_TYPE_WORD,
					CAMERA_SENSOR_I2C_TYPE_WORD,
					FALSE);
				if (0 == rc) {
					break;
				}
			}
			for (i = 0; i < retry_cnt; i++) {
				dfct_data_l = 0;
				rc = camera_io_dev_read(&(s_ctrl->io_master_info),
					fd_dfct_addr+2, &dfct_data_l,
					CAMERA_SENSOR_I2C_TYPE_WORD,
					CAMERA_SENSOR_I2C_TYPE_WORD,
					FALSE);
				if (0 == rc) {
					break;
				}
			}
			CAM_DBG(CAM_SENSOR, " dfct_data_h = 0x%x, dfct_data_l = 0x%x", dfct_data_h, dfct_data_l);
			dfct_data = (dfct_data_h << 16) | dfct_data_l;
			data_h = 0;
			data_v = 0;
			data_h = (dfct_data & (H_DATA_MASK >> j%8)) >> (19 - j%8); //19 = 32 -13;
			data_v = (dfct_data & (V_DATA_MASK >> j%8)) >> (7 - j%8);  // 7 = 32 -13 -12;
			CAM_DBG(CAM_SENSOR, "j = %d, H = %d, V = %d", j, data_h, data_v);
			sony_dfct_tbl.fd_dfct_addr[j] = ((data_h & 0x1FFF) << V_ADDR_SHIFT) | (data_v & 0x0FFF);
			CAM_DBG(CAM_SENSOR, "fd_dfct_data[%d] = 0x%08x", j, sony_dfct_tbl.fd_dfct_addr[j]);
			fd_dfct_addr = fd_dfct_addr + 3 + ((j+1)%8 == 0);
		}
	}
	if (sg_dfct_num > 0) {
		for (j = 0; j < sg_dfct_num; j++) {
			dfct_data = 0;
			for (i = 0; i < retry_cnt; i++) {
				dfct_data_h = 0;
				rc = camera_io_dev_read(&(s_ctrl->io_master_info),
					sg_dfct_addr, &dfct_data_h,
					CAMERA_SENSOR_I2C_TYPE_WORD,
					CAMERA_SENSOR_I2C_TYPE_WORD,
					FALSE);
				if (0 == rc) {
					break;
				}
			}
			for (i = 0; i < retry_cnt; i++) {
				dfct_data_l = 0;
				rc = camera_io_dev_read(&(s_ctrl->io_master_info),
					sg_dfct_addr+2, &dfct_data_l,
					CAMERA_SENSOR_I2C_TYPE_WORD,
					CAMERA_SENSOR_I2C_TYPE_WORD,
					FALSE);
				if (0 == rc) {
					break;
				}
			}
			CAM_DBG(CAM_SENSOR, " dfct_data_h = 0x%x, dfct_data_l = 0x%x", dfct_data_h, dfct_data_l);
			dfct_data = (dfct_data_h << 16) | dfct_data_l;
			data_h = 0;
			data_v = 0;
			data_h = (dfct_data & (H_DATA_MASK >> j%8)) >> (19 - j%8); //19 = 32 -13;
			data_v = (dfct_data & (V_DATA_MASK >> j%8)) >> (7 - j%8);  // 7 = 32 -13 -12;
			CAM_DBG(CAM_SENSOR, "j = %d, H = %d, V = %d", j, data_h, data_v);
			sony_dfct_tbl.sg_dfct_addr[j] = ((data_h & 0x1FFF) << V_ADDR_SHIFT) | (data_v & 0x0FFF);
			CAM_DBG(CAM_SENSOR, "sg_dfct_data[%d] = 0x%08x", j, sony_dfct_tbl.sg_dfct_addr[j]);
			sg_dfct_addr = sg_dfct_addr + 3 + ((j+1)%8 == 0);
		}
	}

	CAM_INFO(CAM_SENSOR, "exit");
	return rc;
}

void oplus_sensor_ov_bypass_framedrop(struct cam_sensor_ctrl_t *s_ctrl,enum cam_sensor_packet_opcodes opcode,struct i2c_settings_list *i2c_list)
{
	if((opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON) && (s_ctrl->is_need_framedrop == 1))
	{
		for (int i = 0; i < i2c_list->i2c_settings.size; i++)
		{
			if(i2c_list->i2c_settings.reg_setting[i].reg_addr == OVSENSOR_FRAMEDROP_ADDR)
			{
				if(s_ctrl->streamon_num == 1)
				{
					CAM_INFO(CAM_SENSOR,"petrelwide need bypass frame drop");
					i2c_list->i2c_settings.reg_setting[i].reg_data = OVSENSOR_FRAMEDROP_DISABLE;
				}
				else
				{
					i2c_list->i2c_settings.reg_setting[i].reg_data = OVSENSOR_FRAMEDROP_ENABLE;
				}
			}
		}
	}
}
#ifdef OPLUS_FEATURE_CAMERA_COMMON
EXPORT_SYMBOL(oplus_sensor_sony_get_dpc_data);
#endif

