#include "fw_download_interface.h"
#include "cam_ois_custom.h"
#include "cam_debug.h"

#include "cam_sensor_io_custom.h"
#include "cam_sensor_util_custom.h"
#include "cam_trace_custom.h"

#include "SEM1217S/sem1217_fw.h"
#include "cam_monitor.h"

static struct v4l2_subdev_core_ops g_ois_ops;
static struct v4l2_subdev_internal_ops g_ois_internal_ops;

extern struct v4l2_subdev_core_ops cam_ois_subdev_core_ops;
extern struct v4l2_subdev_internal_ops cam_ois_internal_ops;

extern int cam_sensor_util_power_down(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info);
extern int cam_sensor_core_power_up(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info, struct completion *i3c_probe_status);
extern int cam_ois_power_down(struct cam_ois_ctrl_t *o_ctrl);

extern int cam_packet_util_validate_packet(struct cam_packet *packet,
	size_t remain_len);

extern struct completion *cam_ois_get_i3c_completion(uint32_t index);
extern int cam_ois_pkt_parse(struct cam_ois_ctrl_t *o_ctrl, void *arg);

int cam_ext_ois_component_bind(struct cam_ois_ctrl_t *o_ctrl)
{
	int rc = 0;
	mutex_init(&(o_ctrl->ois_read_mutex));
#ifdef ENABLE_OIS_DELAY_POWER_DOWN
	o_ctrl->ois_power_down_thread_state = CAM_OIS_POWER_DOWN_THREAD_STOPPED;
	o_ctrl->ois_power_state = CAM_OIS_POWER_OFF;
	o_ctrl->ois_power_down_thread_exit = false;
	mutex_init(&(o_ctrl->ois_power_down_mutex));
	mutex_init(&(o_ctrl->do_ioctl_ois));
	o_ctrl->ois_download_fw_done = CAM_OIS_FW_NOT_DOWNLOAD;
	o_ctrl->ois_fd_have_close_state = CAM_OIS_IS_OPEN;
#endif

	InitOISResource(o_ctrl);
	o_ctrl->is_ois_thread_running = FALSE;
	return rc;
}

int cam_ext_ois_driver_soc_init(struct cam_ois_ctrl_t *o_ctrl)
{
	const char                     *p = NULL;
	int                            ret = 0;
	int                            id;
	struct cam_hw_soc_info         *soc_info = &o_ctrl->soc_info;
	struct device_node             *of_node = NULL;
	int                            rc = 0;

	if (!soc_info->dev) {
		CAM_EXT_ERR(CAM_EXT_OIS, "soc_info is not initialized");
		return -EINVAL;
	}

	of_node = soc_info->dev->of_node;
	if (!of_node) {
		CAM_EXT_ERR(CAM_EXT_OIS, "dev.of_node NULL");
		return -EINVAL;
	}

	ret = of_property_read_u32(of_node, "ois_gyro,position", &id);
	if (ret) {
		o_ctrl->ois_gyro_position = 1;
		CAM_EXT_INFO(CAM_EXT_OIS, "get ois_gyro,position failed rc:%d, set default value to %d", ret, o_ctrl->ois_gyro_position);
	} else {
		o_ctrl->ois_gyro_position = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read ois_gyro,position success, value:%d", o_ctrl->ois_gyro_position);
	}

	ret = of_property_read_u32(of_node, "ois,type", &id);
	if (ret) {
		o_ctrl->ois_type = CAM_OIS_MASTER;
		CAM_EXT_INFO(CAM_EXT_OIS, "get ois,type failed rc:%d, default %d", ret, o_ctrl->ois_type);
	} else {
		o_ctrl->ois_type = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read ois,type success, value:%d", o_ctrl->ois_type);
	}

	ret = of_property_read_u32(of_node, "ois_gyro,type", &id);
	if (ret) {
		o_ctrl->ois_gyro_vendor = 0x02;
		CAM_EXT_INFO(CAM_EXT_OIS, "get ois_gyro,type failed rc:%d, default %d", ret, o_ctrl->ois_gyro_vendor);
	} else {
		o_ctrl->ois_gyro_vendor = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read ois_gyro,type success, value:%d", o_ctrl->ois_gyro_vendor);
	}

	ret = of_property_read_string_index(of_node, "ois,name", 0, (const char **)&p);
	if (ret) {
		CAM_EXT_INFO(CAM_EXT_OIS, "get ois,name failed rc:%d, set default value to %s", ret, o_ctrl->ois_name);
	} else {
		memcpy(o_ctrl->ois_name, p, sizeof(o_ctrl->ois_name));
		CAM_EXT_INFO(CAM_EXT_OIS, "read ois,name success, value:%s", o_ctrl->ois_name);
	}

	ret = of_property_read_u32(of_node, "ois_module,vendor", &id);
	if (ret) {
		o_ctrl->ois_module_vendor = 0x01;
		CAM_EXT_INFO(CAM_EXT_OIS, "get ois_module,vendor failed rc:%d, default %d", ret, o_ctrl->ois_module_vendor);
	} else {
		o_ctrl->ois_module_vendor = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read ois_module,vendor success, value:%d", o_ctrl->ois_module_vendor);
	}

	ret = of_property_read_u32(of_node, "ois_actuator,vednor", &id);
	if (ret) {
		o_ctrl->ois_actuator_vendor = 0x01;
		CAM_EXT_INFO(CAM_EXT_OIS, "get ois_actuator,vednor failed rc:%d, default %d", ret, o_ctrl->ois_actuator_vendor);
	} else {
		o_ctrl->ois_actuator_vendor = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read ois_actuator,vednor success, value:%d", o_ctrl->ois_actuator_vendor);
	}

	ret = of_property_read_u32(of_node, "ois,fw", &id);
	if (ret) {
		o_ctrl->ois_fw_flag = 0x01;
		CAM_EXT_INFO(CAM_EXT_OIS, "get ois,fw failed rc:%d, default %d", ret, o_ctrl->ois_fw_flag);
	} else {
		o_ctrl->ois_fw_flag = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read ois,fw success, value:%d", o_ctrl->ois_fw_flag);
	}

	ret = of_property_read_u32(of_node, "change_cci", &id);
	if (ret) {
		o_ctrl->ois_change_cci = 0x00;
		CAM_EXT_INFO(CAM_EXT_OIS, "get change_cci failed rc:%d, default %d", ret, o_ctrl->ois_change_cci);
	} else {
		o_ctrl->ois_change_cci = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read change_cci success, value:%d", o_ctrl->ois_change_cci);
	}

	ret = of_property_read_u32(of_node, "ois_eis_function", &id);
	if (ret) {
		o_ctrl->ois_eis_function = 0x00;
		CAM_EXT_INFO(CAM_EXT_OIS, "get ois_eis_function failed rc:%d, default %d", ret, o_ctrl->ois_eis_function);
	} else {
		o_ctrl->ois_eis_function = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read ois_eis_function success, value:%d", o_ctrl->ois_eis_function);
	}

	ret = of_property_read_u32(of_node, "download,fw", &id);
	if (ret) {
	    o_ctrl->cam_ois_download_fw_in_advance = 0;
		CAM_EXT_INFO(CAM_EXT_OIS, "get download,fw failed rc:%d, default %d", ret, o_ctrl->cam_ois_download_fw_in_advance);
	} else {
	    o_ctrl->cam_ois_download_fw_in_advance = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read download,fw success, value:%d", o_ctrl->cam_ois_download_fw_in_advance);
	}

	ret = of_property_read_u32(of_node, "ois_switch_spi_mode", &id);
	if (ret) {
		o_ctrl->ois_switch_spi_mode = 0;
		CAM_EXT_INFO(CAM_EXT_OIS, "get ois_switch_spi_mode failed rc:%d, default %d", ret, o_ctrl->ois_switch_spi_mode);
	} else {
		o_ctrl->ois_switch_spi_mode = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read ois_switch_spi_mode success, value:%d", o_ctrl->ois_switch_spi_mode);
	}

	ret = of_property_read_u32(of_node, "actuator_ois_eeprom_merge", &id);
	if (ret) {
		o_ctrl->actuator_ois_eeprom_merge_flag = 0;
		CAM_EXT_INFO(CAM_EXT_OIS, "get actuator_ois_eeprom_merge_flag failed rc:%d, default %d", ret, o_ctrl->actuator_ois_eeprom_merge_flag);
	} else {
		o_ctrl->actuator_ois_eeprom_merge_flag = (uint8_t)id;
		CAM_EXT_INFO(CAM_EXT_OIS, "read actuator_ois_eeprom_merge_flag success, value:%d", o_ctrl->actuator_ois_eeprom_merge_flag);

		/*o_ctrl->actuator_ois_eeprom_merge_mutex = &actuator_ois_eeprom_shared_mutex;
		if (!actuator_ois_eeprom_shared_mutex_init_flag) {
			mutex_init(o_ctrl->actuator_ois_eeprom_merge_mutex);
			actuator_ois_eeprom_shared_mutex_init_flag = true;
		}*/
	}
	return rc;
}

int cam_ext_ois_power_up(struct cam_ois_ctrl_t *o_ctrl)
{
	int                                     rc = 0;
	struct cam_hw_soc_info                 *soc_info = &o_ctrl->soc_info;
	struct cam_ois_soc_private             *soc_private;
	struct cam_sensor_power_ctrl_t         *power_info;
	struct completion                      *i3c_probe_completion = NULL;

	CAM_EXT_INFO(CAM_EXT_OIS, "cam_ois_power_up");
	soc_private = (struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	if ((power_info->power_setting == NULL) &&
		(power_info->power_down_setting == NULL)) {
		CAM_EXT_INFO(CAM_EXT_OIS,
			"Using default power settings");
		if(strstr(o_ctrl->ois_name, "bu24721")){
			cam_ext_ois_construct_default_power_setting_bu24721(power_info);
			CAM_EXT_DBG(CAM_EXT_OIS,"Using bu24271 power settings");
		}else if (strstr(o_ctrl->ois_name, "sem1217s")) {
			rc = cam_ext_ois_construct_default_power_setting_1217s(power_info);
			CAM_EXT_INFO(CAM_EXT_OIS,"Using 1217 power settings");
		}else if (strstr(o_ctrl->ois_name, "dw9786")) {
			rc = cam_ext_ois_construct_default_power_setting_dw9786(power_info);
			CAM_EXT_INFO(CAM_EXT_OIS,"Using dw9786 power settings rc = %d",rc);
		} else{
			rc = cam_ext_ois_construct_default_power_setting(power_info);
		}

		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_OIS,
				"Construct default ois power setting failed.");
			return rc;
		}
	}

	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_OIS,
			"failed to fill vreg params for power up rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_OIS,
			"failed to fill vreg params for power down rc:%d", rc);
		return rc;
	}

	power_info->dev = soc_info->dev;

	if (o_ctrl->io_master_info.master_type == I3C_MASTER)
		i3c_probe_completion = cam_ois_get_i3c_completion(o_ctrl->soc_info.index);

	rc = cam_sensor_core_power_up(power_info, soc_info, i3c_probe_completion);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_OIS, "failed in ois power up rc %d", rc);
		return rc;
	}

	rc = camera_io_init(&o_ctrl->io_master_info);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_OIS, "cci_init failed: rc: %d", rc);
		goto cci_failure;
	}

	return rc;
cci_failure:
	if (cam_sensor_util_power_down(power_info, soc_info))
		CAM_EXT_ERR(CAM_EXT_OIS, "Power Down failed");

	return rc;
}


/**
 * cam_ext_ois_power_down - power down OIS device
 * @o_ctrl:     ctrl structure
 *
 * Returns success or failure
 */
int cam_ext_ois_power_down(struct cam_ois_ctrl_t *o_ctrl)
{
	int32_t                         rc = 0;

	CAM_EXT_INFO(CAM_EXT_OIS, "cam_ois_power_down");

	if (!o_ctrl) {
		CAM_EXT_ERR(CAM_EXT_OIS, "failed: o_ctrl %pK", o_ctrl);
		return -EINVAL;
	}
	DeinitOIS(o_ctrl);
	rc = cam_ois_power_down(o_ctrl);

	return rc;
}

#ifdef ENABLE_OIS_DELAY_POWER_DOWN
int cam_ext_ois_power_down_thread(void *arg)
{
	int rc = 0;
	int i;
	struct cam_ois_ctrl_t *o_ctrl = (struct cam_ois_ctrl_t *)arg;
	struct cam_ois_soc_private *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;

	if (!o_ctrl) {
		CAM_EXT_ERR(CAM_EXT_OIS, "failed: o_ctrl %pK", o_ctrl);
		return -EINVAL;
	}

	soc_private = (struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	if (!soc_private) {
		CAM_EXT_ERR(CAM_EXT_OIS, "failed:soc_private %pK", soc_private);
		return -EINVAL;
	}
	else{
		power_info  = &soc_private->power_info;
		if (!power_info){
			CAM_EXT_ERR(CAM_EXT_OIS, "failed: power_info %pK", o_ctrl, power_info);
			return -EINVAL;
		}
	}

	oplus_cam_monitor_state(o_ctrl,
				o_ctrl->v4l2_dev_str.ent_function,
				CAM_OIS_NORMAL_POWER_UP_TYPE,
				false);
	oplus_cam_monitor_state(o_ctrl,
				o_ctrl->v4l2_dev_str.ent_function,
				CAM_OIS_DELAY_POWER_DOWN_TYPE,
				true);
	mutex_lock(&(o_ctrl->ois_power_down_mutex));
	o_ctrl->ois_power_down_thread_state = CAM_OIS_POWER_DOWN_THREAD_RUNNING;
	mutex_unlock(&(o_ctrl->ois_power_down_mutex));

	for (i = 0; i < (OIS_POWER_DOWN_DELAY/50); i++) {
		if (o_ctrl->ois_power_state == CAM_OIS_POWER_OFF) {
			CAM_EXT_WARN(CAM_EXT_OIS, "ois type=%d has powered down", o_ctrl->ois_type);
			break;
		}
		if(!IsOISReady(o_ctrl)){
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d, is not ready!", o_ctrl->ois_type);
			break;
		}
		msleep(50);// sleep 50ms every time, and sleep OIS_POWER_DOWN_DELAY/50 times.

		mutex_lock(&(o_ctrl->ois_power_down_mutex));
		if (o_ctrl->ois_power_down_thread_exit) {
			mutex_unlock(&(o_ctrl->ois_power_down_mutex));
			break;
		}
		mutex_unlock(&(o_ctrl->ois_power_down_mutex));
	}

	mutex_lock(&(o_ctrl->ois_power_down_mutex));
	if ((!o_ctrl->ois_power_down_thread_exit) && (o_ctrl->ois_power_state == CAM_OIS_POWER_ON)) {
		rc = cam_ext_ois_power_down(o_ctrl);
		oplus_cam_monitor_state(o_ctrl,
					o_ctrl->v4l2_dev_str.ent_function,
					CAM_OIS_DELAY_POWER_DOWN_TYPE,
					false);
		if (!rc){
			kfree(power_info->power_setting);
			kfree(power_info->power_down_setting);
			power_info->power_setting = NULL;
			power_info->power_down_setting = NULL;
			power_info->power_down_setting_size = 0;
			power_info->power_setting_size = 0;
			CAM_EXT_INFO(CAM_EXT_OIS, "ois type=%d,cam_ext_ois_power_down successfully",o_ctrl->ois_type);
		} else {
			CAM_EXT_ERR(CAM_EXT_OIS, "ois type=%d,cam_ext_ois_power_down failed",o_ctrl->ois_type);
		}
		o_ctrl->ois_power_state = CAM_OIS_POWER_OFF;
		if(o_ctrl->cam_ois_download_fw_in_advance)
		{
			mutex_lock(&(o_ctrl->do_ioctl_ois));
			o_ctrl->ois_downloadfw_thread = NULL;
			o_ctrl->ois_download_fw_done = CAM_OIS_FW_NOT_DOWNLOAD;
			o_ctrl->ois_fd_have_close_state = CAM_OIS_IS_CLOSE;
			mutex_unlock(&(o_ctrl->do_ioctl_ois));
			CAM_EXT_INFO(CAM_EXT_OIS, "ois type=%d,cam_ext_ois_power_down,so reset state",o_ctrl->ois_type);
		}
	} else {
		CAM_EXT_INFO(CAM_EXT_OIS, "ois type=%d,No need to do power down, ois_power_down_thread_exit %d, ois_power_state %d",
			o_ctrl->ois_type, o_ctrl->ois_power_down_thread_exit, o_ctrl->ois_power_state);
	}
	o_ctrl->ois_power_down_thread_state = CAM_OIS_POWER_DOWN_THREAD_STOPPED;
	mutex_unlock(&(o_ctrl->ois_power_down_mutex));

	return rc;
}
#endif

void cam_ext_ois_shutdown(struct cam_ois_ctrl_t *o_ctrl)
{
	if (o_ctrl->cam_ois_state >= CAM_OIS_CONFIG) {
		DeinitOIS(o_ctrl);
		oplus_cam_monitor_state(o_ctrl,
				o_ctrl->v4l2_dev_str.ent_function,
				CAM_OIS_NORMAL_POWER_UP_TYPE,
				false);
	}
	cam_ois_shutdown(o_ctrl);
}

/**
 * cam_ois_pkt_parse - Parse csl packet
 * @o_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
static int cam_ext_ois_pkt_parse(struct cam_ois_ctrl_t *o_ctrl, void *arg)
{
	int32_t                         rc = 0;
	struct cam_control             *ioctl_ctrl = NULL;
	struct cam_config_dev_cmd       dev_config;
	uintptr_t                       generic_pkt_addr;
	size_t                          pkt_len;
	size_t                          remain_len = 0;
	struct cam_packet              *csl_packet = NULL;

	int count=0;
	int enable=0;

	ioctl_ctrl = (struct cam_control *)arg;
	if (copy_from_user(&dev_config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(dev_config)))
		return -EFAULT;
	rc = cam_mem_get_cpu_buf(dev_config.packet_handle,
		&generic_pkt_addr, &pkt_len);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_OIS,
			"error in converting command Handle Error: %d", rc);
		return rc;
	}

	remain_len = pkt_len;
	if ((sizeof(struct cam_packet) > pkt_len) ||
		((size_t)dev_config.offset >= pkt_len -
		sizeof(struct cam_packet))) {
		CAM_EXT_ERR(CAM_EXT_OIS,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), pkt_len);
		return -EINVAL;
	}

	remain_len -= (size_t)dev_config.offset;
	csl_packet = (struct cam_packet *)
		(generic_pkt_addr + (uint32_t)dev_config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid packet params");
		return -EINVAL;
	}
	switch (csl_packet->header.op_code & 0xFFFFFF) {
		case CAM_OIS_PACKET_OPCODE_INIT:
			cam_mem_put_cpu_buf(dev_config.packet_handle);
			rc = cam_ois_pkt_parse(o_ctrl, arg);
			if (o_ctrl->cam_ois_state == CAM_OIS_CONFIG) {
				oplus_cam_monitor_state(o_ctrl,
					o_ctrl->v4l2_dev_str.ent_function,
					CAM_OIS_NORMAL_POWER_UP_TYPE,
					true);
			}
			if (rc < 0) {
				CAM_EXT_ERR(CAM_EXT_OIS, "CAM_OIS_PACKET_OPCODE_INIT failed");
			}
			else {
				if(strstr(o_ctrl->ois_name,"bu24721")){
					Enable_gyro_gain(o_ctrl);
				}
			}
			return rc;
		case CAM_OIS_PACKET_OPCODE_OIS_CONTROL:
			if (o_ctrl->cam_ois_state < CAM_OIS_CONFIG) {
				rc = -EINVAL;
				CAM_EXT_WARN(CAM_EXT_OIS,
					"Not in right state to control OIS: %d",
					o_ctrl->cam_ois_state);
				cam_mem_put_cpu_buf(dev_config.packet_handle);
				return rc;
			}

			if (!IsOISReady(o_ctrl)) {
				CAM_EXT_ERR(CAM_EXT_OIS, "OIS is not ready, apply setting may fail");
				for(count=0;count<o_ctrl->soc_info.num_rgltr;count++){
					enable=regulator_is_enabled(regulator_get(o_ctrl->soc_info.dev,o_ctrl->soc_info.rgltr_name[count]));
					CAM_EXT_ERR(CAM_EXT_OIS, "regulator enable=%d,name[%d]=%s",enable,count,o_ctrl->soc_info.rgltr_name[count]);
				}
			}

			goto qcom_ois_pkt_parse;
		default:
			goto qcom_ois_pkt_parse;
	}
	cam_mem_put_cpu_buf(dev_config.packet_handle);

	if (!rc)
		return rc;

qcom_ois_pkt_parse:
	cam_mem_put_cpu_buf(dev_config.packet_handle);
	rc = cam_ois_pkt_parse(o_ctrl, arg);
	return rc;
}

int cam_ext_ois_driver_cmd(struct cam_ois_ctrl_t *o_ctrl, void *arg)
{
	int                              rc = 0;
	struct cam_control              *cmd = (struct cam_control *)arg;
	struct cam_ois_soc_private      *soc_private = NULL;
	struct cam_sensor_power_ctrl_t  *power_info = NULL;
	if (!o_ctrl || !cmd) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid arguments");
		return -EINVAL;
	}

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_EXT_ERR(CAM_EXT_OIS, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	mutex_lock(&(o_ctrl->ois_mutex));
	switch (cmd->op_code) {
		case CAM_STORE_DUALOIS_GYRO_GAIN: {
			int m_result = 1;
			int enable = (int)cmd->reserved;

			if(enable) {
				m_result = StoreOisGyroGian(o_ctrl);
				CAM_EXT_ERR(CAM_EXT_OIS, "start CAM_STORE_DUALOIS_GYRO_GAIN m_result:%d", m_result);
			}else {
				CAM_EXT_ERR(CAM_EXT_OIS, "no need CAM_STORE_DUALOIS_GYRO_GAIN");
			}

			m_result = copy_to_user((void __user *) cmd->handle, &m_result, sizeof(m_result));
			if (m_result != 0) {
				CAM_EXT_ERR(CAM_EXT_OIS, "Failed set power status:%d !!!", enable);
				rc = -EFAULT;
				goto release_mutex;
			}
			break;
		}
		case CAM_WRITE_DUALOIS_GYRO_GAIN: {
			if(strstr(o_ctrl->ois_name, "bu24721") || strstr(o_ctrl->ois_name, "sem1217s") || strstr(o_ctrl->ois_name, "dw9786")) {
				OIS_GYROGAIN current_gyro_gain;
				if (copy_from_user(&current_gyro_gain, (void __user *)cmd->handle,
					sizeof(struct ois_gyrogain_t))) {
					CAM_EXT_ERR(CAM_EXT_OIS,
						"Fail in copy oem control infomation form user data");
					rc = -1;
					goto release_mutex;
				}else{
					WriteOisGyroGian(o_ctrl,&current_gyro_gain);
				}
			}
			break;
		}

		case CAM_FIRMWARE_CALI_GYRO_OFFSET: {
			uint32_t gyro_offset = 0;
			if(strstr(o_ctrl->ois_name, "bu24721")) {
				DoBU24721GyroOffset(o_ctrl, &gyro_offset);
			} else if (strstr(o_ctrl->ois_name, "sem1217s")) {
				DoSEM1217SGyroOffset(o_ctrl, &gyro_offset);
			}else if(strstr(o_ctrl->ois_name, "dw9786")){
				DoDW9786GyroOffset(o_ctrl, &gyro_offset);
			}
			CAM_EXT_ERR(CAM_EXT_OIS, "[GyroOffsetCaliByFirmware] gyro_offset: 0x%x !!!", gyro_offset);
			if (copy_to_user((void __user *) cmd->handle, &gyro_offset,
				sizeof(gyro_offset))) {
				CAM_EXT_ERR(CAM_EXT_OIS, "Failed Copy to User");
				rc = -1;
				goto release_mutex;
			}
			break;
		}
		case CAM_CONFIG_DEV: {
			rc = cam_ext_ois_pkt_parse(o_ctrl, arg);
			if (rc) {
				CAM_EXT_ERR(CAM_EXT_OIS, "Failed in ois pkt Parsing");
				goto release_mutex;
			}
			break;
		}
		case CAM_RELEASE_DEV: {
			if (o_ctrl->cam_ois_state == CAM_OIS_CONFIG) {
				DeinitOIS(o_ctrl);
				oplus_cam_monitor_state(o_ctrl,
					o_ctrl->v4l2_dev_str.ent_function,
					CAM_OIS_NORMAL_POWER_UP_TYPE,
					false);
			}
			mutex_unlock(&(o_ctrl->ois_mutex));
			rc = cam_ois_driver_cmd(o_ctrl, arg);
			return rc;
		}
		default:
			mutex_unlock(&(o_ctrl->ois_mutex));
			rc = cam_ois_driver_cmd(o_ctrl, arg);
			return rc;
	}
release_mutex:
	mutex_unlock(&(o_ctrl->ois_mutex));
	return rc;
}

static int cam_ext_ois_subdev_close_internal(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_ois_ctrl_t *o_ctrl =
		v4l2_get_subdevdata(sd);

	if (!o_ctrl) {
		CAM_EXT_ERR(CAM_EXT_OIS, "o_ctrl ptr is NULL");
			return -EINVAL;
	}

	mutex_lock(&(o_ctrl->ois_mutex));
	cam_ext_ois_shutdown(o_ctrl);
	mutex_unlock(&(o_ctrl->ois_mutex));

	return 0;
}

static long cam_ext_ois_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int                       rc     = 0;
	struct cam_ois_ctrl_t *o_ctrl = v4l2_get_subdevdata(sd);
	if (!o_ctrl) {
		CAM_EXT_ERR(CAM_EXT_OIS, "o_ctrl ptr is NULL");
			return -EINVAL;
	}
	if (o_ctrl->cam_ois_init_completed == false) {
		cam_ext_ois_component_bind(o_ctrl);
		cam_ext_ois_driver_soc_init(o_ctrl);
		o_ctrl->cam_ois_init_completed = true;
	}

	switch (cmd) {
		case VIDIOC_CAM_CONTROL:
			rc = cam_ext_ois_driver_cmd(o_ctrl, arg);
			if (rc)
				CAM_EXT_ERR(CAM_EXT_OIS,
					"Failed with driver cmd: %d", rc);
			break;
		case CAM_SD_SHUTDOWN:
			if (!cam_req_mgr_is_shutdown()) {
				CAM_EXT_ERR(CAM_EXT_OIS, "SD shouldn't come from user space");
				return 0;
			}
			rc = cam_ext_ois_subdev_close_internal(sd, NULL);
			break;
		case VIDIOC_CAM_SENSOR_STATR:
			rc = cam_ois_download_start(o_ctrl);
			break;
		default:
			rc = g_ois_ops.ioctl(sd, cmd, arg);
			break;
	}

	return rc;
}

static int cam_ext_ois_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	bool crm_active = cam_req_mgr_is_open();

	if (crm_active) {
		CAM_EXT_DBG(CAM_EXT_OIS, "CRM is ACTIVE, close should be from CRM");
		return 0;
	}

	return cam_ext_ois_subdev_close_internal(sd, fh);
}

void cam_ois_register(void)
{
	g_ois_internal_ops.close = cam_ois_internal_ops.close;
	cam_ois_internal_ops.close = cam_ext_ois_subdev_close;
	g_ois_ops.ioctl = cam_ois_subdev_core_ops.ioctl;
	cam_ois_subdev_core_ops.ioctl = cam_ext_ois_subdev_ioctl;
}
