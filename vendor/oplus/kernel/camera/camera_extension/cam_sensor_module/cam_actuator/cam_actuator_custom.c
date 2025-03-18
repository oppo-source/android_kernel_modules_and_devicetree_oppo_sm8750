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

#include "cam_actuator_custom.h"
#include "cam_debug.h"
#include "oplus_cam_actuator.h"

//ms
#define AF_POWER_DOWN_DELAY 500

uint32_t AK7316_PARKLENS_DOWN[ACTUATOR_REGISTER_SIZE][2] = {
	{0x00, 0xC8},
	{0x00, 0xD0},
	{0x00, 0xD8},
	{0x00, 0xE0},
	{0x00, 0xE8},
	{0x00, 0xF0},
	{0x00, 0xF8},
	{0x00, 0xFE},
	{0xff, 0xff},
};

extern int cam_actuator_power_down(struct cam_actuator_ctrl_t *a_ctrl);
extern int cam_actuator_power_up(struct cam_actuator_ctrl_t *a_ctrl);
extern struct v4l2_subdev_core_ops cam_actuator_subdev_core_ops;
extern struct v4l2_subdev_internal_ops cam_actuator_internal_ops;

static struct v4l2_subdev_core_ops g_actuator_core_ops;
static struct v4l2_subdev_internal_ops g_actuator_internal_ops;

static long cam_actuator_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;
	bool cam_ext_cmd = false;
	struct cam_actuator_ctrl_t *a_ctrl =
	v4l2_get_subdevdata(sd);
	bool power_up_process = 0;
	size_t len_of_buff = 0;
	struct cam_control        *ioctl_ctrl = NULL;
	struct cam_packet         *csl_packet = NULL;
	struct cam_config_dev_cmd config;
	uintptr_t generic_pkt_ptr;
	struct cam_actuator_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_control *cam_cmd = (struct cam_control *)arg;
	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	mutex_lock(&(a_ctrl->actuator_ioctl_mutex));
	switch (cmd) {
	case VIDIOC_CAM_ACTUATOR_SHAKE_DETECT_ENABLE:
		oplus_cam_actuator_sds_enable(a_ctrl);
		cam_ext_cmd = true;
		break;
	case VIDIOC_CAM_ACTUATOR_LOCK:
		rc = oplus_cam_actuator_lock(a_ctrl);
		cam_ext_cmd = true;
		break;
	case VIDIOC_CAM_ACTUATOR_UNLOCK:
		rc = oplus_cam_actuator_unlock(a_ctrl);
		cam_ext_cmd = true;
		break;
	case CAM_SD_SHUTDOWN:
		if (!cam_req_mgr_is_shutdown()) {
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "SD shouldn't come from user space");
			break;
		}

		if ((!power_info) &&
			(power_info->power_setting == NULL) &&
			(power_info->power_down_setting == NULL))
		{
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "Using default power settings");
			oplus_cam_actuator_construct_default_power_setting(a_ctrl, power_info);
		}

		if(a_ctrl->cam_act_state >= CAM_ACTUATOR_CONFIG) {
			if (a_ctrl->is_af_parklens != 0) {
				oplus_cam_actuator_parklens_power_down(a_ctrl);
			} else {
				oplus_cam_monitor_state(a_ctrl,
						a_ctrl->v4l2_dev_str.ent_function,
						CAM_ACTUATOR_NORMAL_POWER_UP_TYPE,
						false);
			}
		}

		if(a_ctrl->camera_actuator_shake_detect_enable &&
			a_ctrl->cam_act_last_state == CAM_ACTUATOR_LOCK){
			oplus_cam_actuator_unlock(a_ctrl);
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "oplus_cam_actuator_unlock");
		}
		break;
	case VIDIOC_CAM_CONTROL:
		switch(cam_cmd->op_code) {
		case CAM_ACQUIRE_DEV: {
			if ((!power_info) &&
				(power_info->power_setting == NULL) &&
				(power_info->power_down_setting == NULL))
			{
				CAM_EXT_INFO(CAM_EXT_ACTUATOR, "Using default power settings");
				oplus_cam_actuator_construct_default_power_setting(a_ctrl, power_info);
			}
		}
			break;
		case CAM_RELEASE_DEV: {
			if ((!power_info) &&
				(power_info->power_setting == NULL) &&
				(power_info->power_down_setting == NULL)) {
				CAM_EXT_INFO(CAM_EXT_ACTUATOR, "Using default power settings");
				oplus_cam_actuator_construct_default_power_setting(a_ctrl, power_info);
			}
			if(a_ctrl->cam_act_state == CAM_ACTUATOR_CONFIG) {
				if (a_ctrl->is_af_parklens != 0) {
					oplus_cam_actuator_parklens_power_down(a_ctrl);
				} else {
					oplus_cam_monitor_state(a_ctrl,
						a_ctrl->v4l2_dev_str.ent_function,
						CAM_ACTUATOR_NORMAL_POWER_UP_TYPE,
						false);
				}
			}
		}
			break;
		case CAM_QUERY_CAP: {
			cam_actuator_init(a_ctrl);
			break;
		}
		case CAM_CONFIG_DEV: {
			if (a_ctrl->cam_act_state == CAM_ACTUATOR_ACQUIRE) {
				power_up_process = 1;
			}
		}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	mutex_unlock(&(a_ctrl->actuator_ioctl_mutex));

	if (!cam_ext_cmd)
	{
		rc = g_actuator_core_ops.ioctl(sd, cmd, arg);
	}

	// ioctl post.
	switch (cmd) {
	case VIDIOC_CAM_CONTROL: {
		switch(cam_cmd->op_code) {
		case CAM_CONFIG_DEV: {
			if (power_up_process) {
				if (a_ctrl->cam_act_state == CAM_ACTUATOR_CONFIG) {
					ioctl_ctrl = (struct cam_control *)arg;
					if (copy_from_user(&config,
						u64_to_user_ptr(ioctl_ctrl->handle),
						sizeof(config))) {
						CAM_EXT_ERR(CAM_EXT_ACTUATOR,
							"copy_from_user fail");

						break;
					}
					if (cam_mem_get_cpu_buf(config.packet_handle,
						&generic_pkt_ptr, &len_of_buff)) {
						CAM_EXT_ERR(CAM_EXT_ACTUATOR,
							"cam_mem_get_cpu_buf fail");
						break;
					}
					csl_packet = (struct cam_packet *)
						(generic_pkt_ptr + (uint32_t)config.offset);

					if ((csl_packet->header.op_code & 0xFFFFFF) ==
						CAM_ACTUATOR_PACKET_OPCODE_INIT) {
						oplus_cam_monitor_state(a_ctrl,
							a_ctrl->v4l2_dev_str.ent_function,
							CAM_ACTUATOR_NORMAL_POWER_UP_TYPE,
							true);
					}
					cam_mem_put_cpu_buf(config.packet_handle);
				}
			}
		}
			break;
		default:
			break;
		}
	}
		break;
	default:
		break;
	}

	return rc;
}
extern bool cam_req_mgr_is_open(void);

static int cam_actuator_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	bool crm_active = cam_req_mgr_is_open();
	struct cam_actuator_ctrl_t *a_ctrl =
		v4l2_get_subdevdata(sd);
	struct cam_actuator_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;

	if (crm_active) {
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "CRM is ACTIVE, close should be from CRM");
		return 0;
	}

	if (!a_ctrl) {
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "a_ctrl ptr is NULL");
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	if (power_info != NULL &&
		(power_info->power_setting == NULL) &&
		(power_info->power_down_setting == NULL)) {
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "Using default power settings");
		rc = oplus_cam_actuator_construct_default_power_setting(a_ctrl, power_info);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Construct default actuator power setting failed.");
		}
	}

	if(a_ctrl->cam_act_state >= CAM_ACTUATOR_CONFIG &&
		a_ctrl->is_af_parklens != 0) {
		oplus_cam_actuator_parklens_power_down(a_ctrl);
	}

	if(a_ctrl->camera_actuator_shake_detect_enable &&
		a_ctrl->cam_act_last_state == CAM_ACTUATOR_LOCK) {
		oplus_cam_actuator_unlock(a_ctrl);
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "oplus_cam_actuator_unlock");
	}

	g_actuator_internal_ops.close(sd, fh);

	return 0;
}

void cam_actuator_register(void)
{
	CAM_EXT_INFO(CAM_EXT_ACTUATOR, "cam_actuator_register E");

	g_actuator_core_ops.ioctl = cam_actuator_subdev_core_ops.ioctl;
	cam_actuator_subdev_core_ops.ioctl = cam_actuator_ioctl;

	g_actuator_internal_ops.close = cam_actuator_internal_ops.close;
	cam_actuator_internal_ops.close = cam_actuator_close;

}
void cam_actuator_init(struct cam_actuator_ctrl_t *a_ctrl)
{
	CAM_EXT_INFO(CAM_EXT_ACTUATOR, "cam_actuator_init E");

	if (!a_ctrl) {
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "a_ctrl ptr is NULL");
		return;
	}

	oplus_cam_actuator_parse_dt(a_ctrl);

	a_ctrl->cam_act_last_state = CAM_ACTUATOR_INIT;
	if (a_ctrl->is_need_read_current) {
		a_ctrl->af_power_down_thread_state = CAM_AF_POWER_DOWN_THREAD_STOPPED;
		mutex_init(&(a_ctrl->af_power_down_mutex));
	}
	if (a_ctrl->is_update_pid)
	{
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "create af download fw thread");
		a_ctrl->actuator_update_pid_thread = kthread_run(oplus_cam_actuator_update_pid, a_ctrl, a_ctrl->device_name);
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "not need update pid");
	}

	CAM_EXT_INFO(CAM_EXT_ACTUATOR, "cam_actuator_init X");

}

int32_t oplus_cam_actuator_parse_dt(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t                         i,rc = 0;
	const char                      *p = NULL;
	uint32_t                        reactive_setting_data[6];
	uint32_t                        reactive_setting_size;
	const uint32_t                  default_setting_size =6;
	uint32_t                        write_setting_size;
	uint32_t                        reg_count = 0;
	uint32_t                        size;
	struct cam_hw_soc_info          *soc_info = &a_ctrl->soc_info;
	struct device_node              *of_node = NULL;
	of_node = soc_info->dev->of_node;

	CAM_EXT_INFO(CAM_EXT_ACTUATOR, "%s begin",__func__);

	rc = of_property_read_bool(of_node, "is_update_pid");
	if (rc) {
		a_ctrl->is_update_pid = true;
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read is_update_pid success, value:%d", a_ctrl->is_update_pid);
	} else {
		a_ctrl->is_update_pid = false;
		CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get is_update_pid failed rc:%d, default %d", rc, a_ctrl->is_update_pid);
	}
	rc = of_property_read_bool(of_node, "is_petrel_ak7316_update_pid");
	if (rc) {
		a_ctrl->is_petrel_ak7316_update_pid = true;
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read is_petrel_ak7316_update_pid success, value:%d", a_ctrl->is_petrel_ak7316_update_pid);
	} else {
		a_ctrl->is_petrel_ak7316_update_pid = false;
		CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get is_petrel_ak7316_update_pid failed rc:%d, default %d", rc, a_ctrl->is_petrel_ak7316_update_pid);
	}
	rc = of_property_read_string_index(of_node, "actuator,name", 0, (const char **)&p);
	if (rc) {
		CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get actuator,name failed rc:%d", rc);
	} else {
		memcpy(a_ctrl->actuator_name, p, sizeof(a_ctrl->actuator_name));
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read actuator,name success, value:%s", a_ctrl->actuator_name);
	}

	rc = of_property_read_u32(of_node, "is_af_parklens", &a_ctrl->is_af_parklens);
	if (rc)
	{
		a_ctrl->is_af_parklens = 0;
		CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get failed for is_af_parklens = %d",a_ctrl->is_af_parklens);
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read is_af_parklens success, value:%d", a_ctrl->is_af_parklens);
	}

	if (!of_property_read_bool(of_node, "reactive-ctrl-support")) {
		a_ctrl->reactive_ctrl_support = false;
		CAM_EXT_DBG(CAM_EXT_ACTUATOR, "No reactive control parameter defined");
	} else {
		reactive_setting_size = of_property_count_u32_elems(of_node, "reactive-reg-setting");

		if (reactive_setting_size != 6) {
			a_ctrl->reactive_ctrl_support = false;
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "reactive control parameter config err!");
		} else {
			rc = of_property_read_u32_array(of_node, "reactive-reg-setting",
					reactive_setting_data, reactive_setting_size);

			a_ctrl->reactive_reg_array.reg_addr  = reactive_setting_data[0];
			a_ctrl->reactive_setting.addr_type	 = reactive_setting_data[1];
			a_ctrl->reactive_reg_array.reg_data  = reactive_setting_data[2];
			a_ctrl->reactive_setting.data_type	 = reactive_setting_data[3];
			a_ctrl->reactive_reg_array.delay	 = reactive_setting_data[4];
			a_ctrl->reactive_reg_array.data_mask = reactive_setting_data[5];
			a_ctrl->reactive_setting.reg_setting = &(a_ctrl->reactive_reg_array);
			a_ctrl->reactive_setting.size		 = 1;
			a_ctrl->reactive_setting.delay		 = 0;
			a_ctrl->reactive_ctrl_support		 = true;

			CAM_EXT_INFO(CAM_EXT_ACTUATOR,
				"reactive control support %d, reactive [0x%x %d 0x%x %d %d 0x%x]",
				a_ctrl->reactive_ctrl_support,
				a_ctrl->reactive_setting.reg_setting->reg_addr,
				a_ctrl->reactive_setting.addr_type,
				a_ctrl->reactive_setting.reg_setting->reg_data,
				a_ctrl->reactive_setting.data_type,
				a_ctrl->reactive_setting.reg_setting->delay,
				a_ctrl->reactive_setting.reg_setting->data_mask);
		}
	}

	rc = of_property_read_u32(of_node, "power-size-support", &a_ctrl->power_setting_size);
	if (rc)
	{
		a_ctrl->power_setting_size = 0;
		CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get failed for power_setting_size = %d",a_ctrl->power_setting_size);
	}
	else
	{
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read power_setting_size success, value:%d", a_ctrl->power_setting_size);
	}

	rc = of_property_read_bool(of_node, "power-custom1-reg");
	if (rc) {
		a_ctrl->power_custom1_reg = true;
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read power_custom1_reg success, value:%d", a_ctrl->power_custom1_reg);
	} else {
		a_ctrl->power_custom1_reg = false;
		CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get power_custom1_reg failed rc:%d, default %d", rc, a_ctrl->power_custom1_reg);
	}

	rc = of_property_read_bool(of_node, "power-custom2-reg");
	if (rc) {
		a_ctrl->power_custom2_reg = true;
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read power_custom2_reg success, value:%d", a_ctrl->power_custom2_reg);
	} else {
		a_ctrl->power_custom2_reg = false;
		CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get power_custom2_reg failed rc:%d, default %d", rc, a_ctrl->power_custom2_reg);
	}
/*
	rc = of_property_read_u32(of_node, "actuator_function", &a_ctrl->actuator_function);
	if (rc) {
		a_ctrl->actuator_function = 0x00;
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "get actuator_function failed default %d", a_ctrl->actuator_function);
	} else {
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read actuator_function success, value:%d", a_ctrl->actuator_function);
	}
*/
	rc = of_property_read_u32(of_node, "cci_client_sid", &a_ctrl->cci_client_sid);
	if (rc) {
		a_ctrl->cci_client_sid = 0x00;
		CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get cci_client_sid failed default %d", a_ctrl->cci_client_sid);
	} else {
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read cci_client_sid success, value:%d", a_ctrl->cci_client_sid);
	}

	if (!of_property_read_bool(of_node, "sds-lock-support")) {
		a_ctrl->sds_lock_support = false;
		CAM_EXT_DBG(CAM_EXT_ACTUATOR, "No sds-lock-support control parameter defined");
	} else {
		//must config as below in dtsi:
		//						 reg_addr	addr_type	reg_data	 data_type	   delay	   data_mask
		//sds-lock-reg-setting = [ 0x02 		  1 		0x40		1		   10			 0x00];

		//length
		write_setting_size = of_property_count_u32_elems(of_node, "sds-lock-reg-setting");
		//count
		while(write_setting_size >= default_setting_size)
		{
			write_setting_size -= default_setting_size;
			reg_count++;
		}

		size = reg_count * sizeof(*a_ctrl->sds_lock_setting.reg_setting);
		a_ctrl->sds_lock_setting.reg_setting = kzalloc(size, GFP_KERNEL);
		if(!a_ctrl->sds_lock_setting.reg_setting)
		{
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "%s kzalloc memory fail", __func__);
			return -ENOMEM;
		}
		a_ctrl->sds_lock_setting.size = reg_count;


		for (i = 0; i < reg_count; i++)
		{
			of_property_read_u32_index(of_node, "sds-lock-reg-setting",
				(i*default_setting_size+0), &(a_ctrl->sds_lock_setting.reg_setting[i].reg_addr));
			of_property_read_u32_index(of_node, "sds-lock-reg-setting",
				(i*default_setting_size+2), &(a_ctrl->sds_lock_setting.reg_setting[i].reg_data));
			of_property_read_u32_index(of_node, "sds-lock-reg-setting",
				(i*default_setting_size+4), &(a_ctrl->sds_lock_setting.reg_setting[i].delay));
			of_property_read_u32_index(of_node, "sds-lock-reg-setting",
				(i*default_setting_size+5), &(a_ctrl->sds_lock_setting.reg_setting[i].data_mask));
		}

		a_ctrl->sds_lock_setting.delay = 0;
		of_property_read_u32_index(of_node, "sds-lock-reg-setting",
				1, &(a_ctrl->sds_lock_setting.addr_type));
		of_property_read_u32_index(of_node, "sds-lock-reg-setting",
				3, &(a_ctrl->sds_lock_setting.data_type));

		a_ctrl->sds_lock_support = true;
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "sds_lock_support count %d, reg [0x%x %d 0x%x %d %d 0x%x]",
				a_ctrl->sds_lock_setting.size,
				a_ctrl->sds_lock_setting.reg_setting[0].reg_addr,
				a_ctrl->sds_lock_setting.addr_type,
				a_ctrl->sds_lock_setting.reg_setting[0].reg_data,
				a_ctrl->sds_lock_setting.data_type,
				a_ctrl->sds_lock_setting.reg_setting[0].delay,
				a_ctrl->sds_lock_setting.reg_setting[0].data_mask);
	}

	rc = of_property_read_bool(of_node, "is_need_read_current");
	if (rc) {
		a_ctrl->is_need_read_current = true;
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read is_need_read_current success, value:%d", a_ctrl->is_need_read_current);

		rc = of_property_read_bool(of_node, "is_default_high_voltage");
		if (rc) {
			a_ctrl->is_default_high_voltage = true;
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read is_default_high_voltage success, value:%d", a_ctrl->is_default_high_voltage);
		} else {
			a_ctrl->is_default_high_voltage = false;
			CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get is_default_high_voltage failed rc:%d, default %d", rc, a_ctrl->is_default_high_voltage);
		}

		rc = of_property_read_u32(of_node, "pull_gpio", &a_ctrl->pull_gpio);
		if (rc) {
			a_ctrl->pull_gpio = -1;
			CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get pull_gpio failed default %d", a_ctrl->pull_gpio);
		} else {
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read pull_gpio success, value:%d", a_ctrl->pull_gpio);
		}

		rc = of_property_read_u32(of_node, "current_register", &a_ctrl->current_register);
		if (rc) {
			a_ctrl->current_register = 0;
			CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get current_register failed default %d", a_ctrl->current_register);
		} else {
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read current_register success, value:%d", a_ctrl->current_register);
		}

		rc = of_property_read_u32(of_node, "min_current", &a_ctrl->min_current);
		if (rc) {
			a_ctrl->min_current = 0;
			CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get min_current failed default %d", a_ctrl->min_current);
		} else {
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read min_current success, value:%d", a_ctrl->min_current);
		}

		rc = of_property_read_u32(of_node, "max_current", &a_ctrl->max_current);
		if (rc) {
			a_ctrl->max_current = 256;
			CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get max_current failed default %d", a_ctrl->max_current);
		} else {
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read max_current success, value:%d", a_ctrl->max_current);
		}
	} else {
		a_ctrl->is_need_read_current = false;
		CAM_EXT_WARN(CAM_EXT_ACTUATOR, "get is_need_read_current failed rc:%d, default %d", rc, a_ctrl->is_need_read_current);
	}
	return rc;
}

void cam_actuator_poll_setting_update(struct cam_actuator_ctrl_t *a_ctrl) {

	struct i2c_settings_list *i2c_list = NULL;

	a_ctrl->is_actuator_ready = true;
	memset(&(a_ctrl->poll_register), 0, sizeof(struct cam_sensor_i2c_reg_array));
	list_for_each_entry(i2c_list,
		&(a_ctrl->i2c_data.init_settings.list_head), list)
	{
		if (i2c_list->op_code == CAM_SENSOR_I2C_POLL)
		{
			a_ctrl->poll_register.reg_addr = i2c_list->i2c_settings.reg_setting[0].reg_addr;
			a_ctrl->poll_register.reg_data = i2c_list->i2c_settings.reg_setting[0].reg_data;
			a_ctrl->poll_register.data_mask = i2c_list->i2c_settings.reg_setting[0].data_mask;
			a_ctrl->poll_register.delay = 100; //i2c_list->i2c_settings.reg_setting[0].delay; // The max delay should be 100
			a_ctrl->addr_type = i2c_list->i2c_settings.addr_type;
			a_ctrl->data_type = i2c_list->i2c_settings.data_type;
		}
	}
}

void cam_actuator_poll_setting_apply(struct cam_actuator_ctrl_t *a_ctrl) {
	int ret = 0;
	if (!a_ctrl->is_actuator_ready)
	{
		if (a_ctrl->poll_register.reg_addr || a_ctrl->poll_register.reg_data)
		{
			ret = cam_ext_io_dev_poll(
				&(a_ctrl->io_master_info),
				a_ctrl->poll_register.reg_addr,
				a_ctrl->poll_register.reg_data,
				a_ctrl->poll_register.data_mask,
				a_ctrl->addr_type,
				a_ctrl->data_type,
				a_ctrl->poll_register.delay);
			if (ret < 0)
			{
					CAM_EXT_ERR(CAM_EXT_ACTUATOR, "i2c poll apply setting Fail: %d, is_actuator_ready %d", ret, a_ctrl->is_actuator_ready);
			}
			else
			{
					CAM_EXT_DBG(CAM_EXT_ACTUATOR, "is_actuator_ready %d, ret %d", a_ctrl->is_actuator_ready, ret);
			}
			a_ctrl->is_actuator_ready = true; //Just poll one time
		}
	}
}

int actuator_power_down_thread(void *arg)
{
	int rc = 0;
	int i;
	uint32_t read_val = 0;
	struct cam_actuator_ctrl_t *a_ctrl = (struct cam_actuator_ctrl_t *)arg;
	struct cam_actuator_soc_private *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;
	msleep(5);
	if (!a_ctrl) {
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "failed: a_ctrl %pK", a_ctrl);
		return -EINVAL;
	}

	soc_private = (struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;

	if (!soc_private) {
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "failed:soc_private %pK", soc_private);
		return -EINVAL;
	}
	else{
		power_info  = &soc_private->power_info;
		if (!power_info){
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "failed: power_info %pK", power_info);
			return -EINVAL;
		}
	}

	//down(&a_ctrl->actuator_sem);

	if (a_ctrl->is_need_read_current) {
		mutex_lock(&(a_ctrl->af_power_down_mutex));
		a_ctrl->af_power_down_thread_state = CAM_AF_POWER_DOWN_THREAD_RUNNING;
		mutex_unlock(&(a_ctrl->af_power_down_mutex));
	}

	oplus_cam_monitor_state(a_ctrl,
		a_ctrl->v4l2_dev_str.ent_function,
		CAM_ACTUATOR_NORMAL_POWER_UP_TYPE,
		false);
	oplus_cam_monitor_state(a_ctrl,
		a_ctrl->v4l2_dev_str.ent_function,
		CAM_ACTUATOR_DELAY_POWER_DOWN_TYPE,
		true);

	CAM_EXT_INFO(CAM_EXT_ACTUATOR, "actuator_power_down_thread start ");
	cam_ext_io_dev_read(a_ctrl->io_master_info.cci_client, AK7316_DAC_ADDR, &read_val,
			CAMERA_SENSOR_I2C_TYPE_BYTE, CAMERA_SENSOR_I2C_TYPE_WORD,false);

	for (i = 0; i < ACTUATOR_REGISTER_SIZE; i++) {
		if ((AK7316_PARKLENS_DOWN[i][0] != 0xff) && (AK7316_PARKLENS_DOWN[i][1] != 0xff)) {
			if(read_val <= (AK7316_PARKLENS_DOWN[i][1] << 8)) {
				CAM_EXT_INFO(CAM_EXT_ACTUATOR, "read_val: 0x%x, Ak7316_PARKLENS_DOWN: 0x%x", read_val, AK7316_PARKLENS_DOWN[i][1]);
				break;
			}
		} else {
			break;
		}
	}

	for (; i < ACTUATOR_REGISTER_SIZE; i++) {
		if ( (AK7316_PARKLENS_DOWN[i][0] != 0xff) && (AK7316_PARKLENS_DOWN[i][1] != 0xff) ) {
			rc = oplus_cam_actuator_ram_write(a_ctrl, (uint32_t)AK7316_PARKLENS_DOWN[i][0], (uint32_t)AK7316_PARKLENS_DOWN[i][1]);
			if(rc < 0) {
				CAM_EXT_ERR(CAM_EXT_ACTUATOR, "write failed ret : %d", rc);
				goto free_power_settings;
			} else {
				CAM_EXT_INFO(CAM_EXT_ACTUATOR, "write register 0x%x: 0x%x sucess", AK7316_PARKLENS_DOWN[i][0], AK7316_PARKLENS_DOWN[i][1]);
			}
			msleep(5);
		}
		else {
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "set parklens success ");
			break;
		}
		mutex_lock(&(a_ctrl->af_power_down_mutex));
		if (a_ctrl->is_need_read_current && a_ctrl->af_power_down_thread_state == CAM_AF_POWER_DOWN_THREAD_STOPPED) {
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "actuator:%s has start power up, not need parklens", a_ctrl->actuator_name);
			mutex_unlock(&(a_ctrl->af_power_down_mutex));
			break;
		}
		mutex_unlock(&(a_ctrl->af_power_down_mutex));
	}
	if (a_ctrl->is_need_read_current) {
		for (i = 0; i < AF_POWER_DOWN_DELAY/20; i++) {
			mutex_lock(&(a_ctrl->af_power_down_mutex));
			if (a_ctrl->af_power_down_thread_state == CAM_AF_POWER_DOWN_THREAD_STOPPED) {
				CAM_EXT_INFO(CAM_EXT_ACTUATOR, "actuator:%s has start power up, not need power down delat", a_ctrl->actuator_name);
				mutex_unlock(&(a_ctrl->af_power_down_mutex));
				break;
			}
			mutex_unlock(&(a_ctrl->af_power_down_mutex));
			msleep(20);
		}
	}

free_power_settings:
	if (a_ctrl->is_need_read_current) {
		mutex_lock(&(a_ctrl->af_power_down_mutex));
		if (a_ctrl->af_power_down_thread_state == CAM_AF_POWER_DOWN_THREAD_STOPPED)
		{
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "Actuator:%s has start power up, not need power down", a_ctrl->actuator_name);
			mutex_unlock(&(a_ctrl->af_power_down_mutex));
			oplus_cam_monitor_state(a_ctrl,
				a_ctrl->v4l2_dev_str.ent_function,
				CAM_ACTUATOR_DELAY_POWER_DOWN_TYPE,
				false);
			return rc;
		}
	}
	oplus_cam_monitor_state(a_ctrl,
		a_ctrl->v4l2_dev_str.ent_function,
		CAM_ACTUATOR_DELAY_POWER_DOWN_TYPE,
		false);
	rc = cam_actuator_power_down(a_ctrl);

	if (rc < 0) {
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Actuator Power down failed");
	} else {
		kfree(power_info->power_setting);
		kfree(power_info->power_down_setting);
		power_info->power_setting = NULL;
		power_info->power_down_setting = NULL;
		power_info->power_setting_size = 0;
		power_info->power_down_setting_size = 0;
	}
	CAM_EXT_INFO(CAM_EXT_ACTUATOR, "actuator_power_down_thread exit");
	if (a_ctrl->is_need_read_current) {
		a_ctrl->af_power_down_thread_state = CAM_AF_POWER_DOWN_THREAD_STOPPED;
		mutex_unlock(&(a_ctrl->af_power_down_mutex));
	}
//	up(&a_ctrl->actuator_sem);
	return rc;
}

void oplus_cam_actuator_parklens_power_down(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	struct cam_actuator_soc_private  *soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info =
		&soc_private->power_info;

	a_ctrl->actuator_parklens_thread = kthread_run(actuator_power_down_thread, a_ctrl, "actuator_power_down_thread");

	if (IS_ERR(a_ctrl->actuator_parklens_thread)) {
		//down(&a_ctrl->actuator_sem);
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "create actuator_power_down_thread failed");
		rc = cam_actuator_power_down(a_ctrl);
		oplus_cam_monitor_state(a_ctrl,
			a_ctrl->v4l2_dev_str.ent_function,
			CAM_ACTUATOR_NORMAL_POWER_UP_TYPE,
			false);

		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Actuator Power down failed");
		} else {
			kfree(power_info->power_setting);
			kfree(power_info->power_down_setting);
			power_info->power_setting = NULL;
			power_info->power_down_setting = NULL;
			power_info->power_setting_size = 0;
			power_info->power_down_setting_size = 0;
		}
		//up(&a_ctrl->actuator_sem);
	}
}

int oplus_cam_actuator_ram_write(struct cam_actuator_ctrl_t *a_ctrl, uint32_t addr, uint32_t data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 5,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
		.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
		.delay = 0x00,
	};

	if (a_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++) {
		rc = cam_ext_io_dev_write(a_ctrl->io_master_info.cci_client, &i2c_write);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "actuator write 0x%x failed, retry:%d", addr, i+1);
		}
		else
		{
			CAM_EXT_DBG(CAM_EXT_ACTUATOR, "actuator write success 0x%x, data:=0x%x", addr, data);
			return rc;
		}
	}
	return rc;
}

int oplus_cam_actuator_ram_write_word(struct cam_actuator_ctrl_t *a_ctrl, uint32_t addr, uint32_t data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 5,
		.data_mask = 0x00,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.data_type = CAMERA_SENSOR_I2C_TYPE_WORD,
		.delay = 0x00,
	};

	if (a_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++) {
		rc = cam_ext_io_dev_write(a_ctrl->io_master_info.cci_client, &i2c_write);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "actuator write 0x%x failed, retry:%d", addr, i+1);
		}
		else
		{
			CAM_EXT_DBG(CAM_EXT_ACTUATOR, "actuator write success 0x%x, data:=0x%x", addr, data);
			return rc;
		}
	}
	return rc;
}

int oplus_cam_actuator_ram_read_word(struct cam_actuator_ctrl_t *a_ctrl, uint32_t addr, uint32_t* data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	if (a_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++)
	{
		rc = cam_ext_io_dev_read(a_ctrl->io_master_info.cci_client, (uint32_t)addr, (uint32_t *)data,
								CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD,false);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "read 0x%x failed, retry:%d", addr, i+1);
		}
		else
		{
			return rc;
		}
	}
	return rc;
};


int oplus_cam_actuator_ram_read(struct cam_actuator_ctrl_t *a_ctrl, uint32_t addr, uint32_t* data)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	if (a_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	for(i = 0; i < retry; i++)
	{
		rc = cam_ext_io_dev_read(a_ctrl->io_master_info.cci_client, (uint32_t)addr, (uint32_t *)data,
								CAMERA_SENSOR_I2C_TYPE_BYTE, CAMERA_SENSOR_I2C_TYPE_BYTE,false);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "read 0x%x failed, retry:%d", addr, i+1);
		}
		else
		{
			return rc;
		}
	}
	return rc;
}

int oplus_cam_apply_dtsetting_util(
	struct cam_actuator_ctrl_t *a_ctrl,
	const char *propname,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	int32_t rc = 0, i = 0;
	const int DT_UNIT		= 4;
	struct device_node *of_node	= NULL;
	uint32_t *dt_setting		= NULL;
	uint32_t setting_size = 0, reg_read = 0;
	struct cam_sensor_i2c_reg_array     reg_setting_one;
	struct cam_sensor_i2c_reg_setting   sensor_setting;

	if (!a_ctrl || !a_ctrl->soc_info.dev) {
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "a_ctrl is null");
		return -EINVAL;
	}

	of_node = a_ctrl->soc_info.dev->of_node;

	setting_size = of_property_count_u32_elems(of_node,
			propname);
	if (setting_size == 0 || setting_size % DT_UNIT != 0) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "Check %s", propname);
		return -EINVAL;
	}
	dt_setting = vmalloc(sizeof(uint32_t) * setting_size);
	if (dt_setting == NULL) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "vmalloc fail for dt_setting.");
		return -EINVAL;
	}
	rc  = of_property_read_u32_array(of_node, propname,
			dt_setting, setting_size);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "get pid_version_verify fail.");
		goto free_dt_setting;
	}

	for (i = 0; i < (setting_size / DT_UNIT); i++) {
		if (dt_setting[i*DT_UNIT] == 'W') {
			reg_setting_one.reg_addr  = dt_setting[i*DT_UNIT+1];
			reg_setting_one.reg_data  = dt_setting[i*DT_UNIT+2];
			reg_setting_one.delay     = 0;
			reg_setting_one.data_mask = 0XFFFF;
			sensor_setting.addr_type  = addr_type;
			sensor_setting.data_type  = data_type;
			sensor_setting.delay      = dt_setting[i*DT_UNIT+3];
			sensor_setting.size       = 1;
			sensor_setting.reg_setting = &reg_setting_one;
			rc = camera_io_dev_write(
				&(a_ctrl->io_master_info),
				&sensor_setting);
			if (rc) {
				CAM_EXT_ERR(CAM_EXT_SENSOR,
					"0x%02X write 0x%02X 0x%02X failed.",
					a_ctrl->cci_client_sid,
					reg_setting_one.reg_addr,
					reg_setting_one.reg_data);
				goto free_dt_setting;
			}
		} else if (dt_setting[i*DT_UNIT] == 'R') {
			rc = camera_io_dev_read(
				&(a_ctrl->io_master_info),
				dt_setting[i*DT_UNIT+1],
				&reg_read,
				addr_type,
				data_type,
				false);
			if (rc) {
				CAM_EXT_ERR(CAM_EXT_SENSOR,
					"%x read %x fail, rc = %d",
					a_ctrl->cci_client_sid,
					dt_setting[i*DT_UNIT+1],
					rc);
				goto free_dt_setting;
			} else if ((reg_read & dt_setting[i*DT_UNIT+3])
				!= dt_setting[i*DT_UNIT+2]) {
				CAM_EXT_ERR(CAM_EXT_SENSOR,
					"0x%02X read 0x%02X 0x%02X expected data 0x%02X mask 0x%02X, rc = %d",
					a_ctrl->cci_client_sid,
					dt_setting[i*DT_UNIT+1],
					reg_read,
					dt_setting[i*DT_UNIT+2],
					dt_setting[i*DT_UNIT+3],
					rc);
				goto free_dt_setting;
			}
		} else {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "Check %s", propname);
			goto free_dt_setting;
		}
	}

	if (dt_setting)
		vfree(dt_setting);
	dt_setting = NULL;
	return rc;

free_dt_setting:
	if (dt_setting)
		vfree(dt_setting);
	dt_setting = NULL;
	return -EINVAL;
}

int oplus_cam_actuator_update_pid_oem(struct cam_actuator_ctrl_t *a_ctrl)
{
	struct device_node *of_node = NULL;
	struct cam_hw_soc_info *soc_info = &a_ctrl->soc_info;
	int32_t rc = 0, i = 0, retrycnt = 5;
	enum camera_sensor_i2c_type addr_type = 0;
	enum camera_sensor_i2c_type data_type = 0;

	if (a_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "oplus_cam_actuator_update_pid Invalid Args");
		return -EINVAL;
	}

	a_ctrl->io_master_info.cci_client->cci_i2c_master = a_ctrl->cci_i2c_master;
	a_ctrl->io_master_info.cci_client->i2c_freq_mode = I2C_FAST_PLUS_MODE;
	a_ctrl->io_master_info.cci_client->sid = (a_ctrl->cci_client_sid >> 1);
	a_ctrl->io_master_info.cci_client->retries = 0;
	a_ctrl->io_master_info.cci_client->id_map = 0;

	rc = cam_actuator_power_up(a_ctrl);
	if (rc < 0) {
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Failed for Actuator Power up failed: %d", rc);
		return rc;
	}

	oplus_cam_monitor_state(a_ctrl,
		a_ctrl->v4l2_dev_str.ent_function,
		CAM_ACTUATOR_UPDATE_PID_POWER_UP_TYPE,
		true);

	msleep(10);

	of_node = soc_info->dev->of_node;

	rc = of_property_read_u32(of_node, "pid_settings_addr_type", &i);
	addr_type = (enum camera_sensor_i2c_type)i;
	rc |= of_property_read_u32(of_node, "pid_settings_data_type", &i);
	data_type = (enum camera_sensor_i2c_type)i;
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_SENSOR,
			"Please check pid_settings_addr_type and pid_settings_data_type config.");
		goto power_down;
	}

	CAM_EXT_INFO(CAM_EXT_ACTUATOR, "0x%02X PID version verify...", a_ctrl->cci_client_sid);
	rc = oplus_cam_apply_dtsetting_util(a_ctrl,
		"pid_version_verify",
		addr_type, data_type);
	// Petrel ak3716 update only when parameters are the same as pid_version_verify.
	if (a_ctrl->is_petrel_ak7316_update_pid)
	{
		if(rc){
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "0x%02X Petrel AK7316 PID version is not R7, no need to update PID.",
				a_ctrl->cci_client_sid);
			goto power_down;
		}
	}
	else
	{
		if (!rc){
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "0x%02X PID version verify succ, no need to update PID.",
				a_ctrl->cci_client_sid);
			goto power_down;
		}
	}

	CAM_EXT_INFO(CAM_EXT_ACTUATOR, "0x%02X PID version verify fail, update...",
		a_ctrl->cci_client_sid);
	rc = oplus_cam_apply_dtsetting_util(a_ctrl,
		"pid_update_setting",
		addr_type, data_type);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "0x%02X Write PID setting failed.",
			a_ctrl->cci_client_sid);
		goto power_down;
	}

	do {
		rc = oplus_cam_apply_dtsetting_util(a_ctrl,
			"pid_post_verify",
			addr_type, data_type);
		if (rc) {
			oplus_cam_apply_dtsetting_util(a_ctrl,
				"pid_update_setting",
				addr_type, data_type);
			retrycnt--;
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "0x%02X  PID register double-check error, retry left %d times",
				a_ctrl->cci_client_sid,retrycnt);
		} else {
			CAM_EXT_INFO(CAM_EXT_ACTUATOR, "0x%02X PID register double-check Succ.",
				a_ctrl->cci_client_sid);
		}
	}while(rc && retrycnt > 0);

power_down:
	rc = cam_actuator_power_down(a_ctrl);
	oplus_cam_monitor_state(a_ctrl,
			a_ctrl->v4l2_dev_str.ent_function,
			CAM_ACTUATOR_UPDATE_PID_POWER_UP_TYPE,
			false);
	return rc;
}

int oplus_cam_actuator_update_pid(void *arg)
{
	int rc = 0;
	struct cam_actuator_ctrl_t *a_ctrl = (struct cam_actuator_ctrl_t *)arg;

	if (a_ctrl == NULL) {
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "oplus_cam_actuator_update_pid Invalid Args");
		return -EINVAL;
	}
	rc = oplus_cam_actuator_update_pid_oem(a_ctrl);

	return rc;
}

void oplus_cam_actuator_sds_enable(struct cam_actuator_ctrl_t *a_ctrl)
{
	mutex_lock(&(a_ctrl->actuator_mutex));
	a_ctrl->camera_actuator_shake_detect_enable = true;
	CAM_EXT_INFO(CAM_EXT_ACTUATOR, "SDS Actuator:%s enbale", a_ctrl->actuator_name);
	mutex_unlock(&(a_ctrl->actuator_mutex));
}

int32_t oplus_cam_actuator_lock(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = -1;
	//uint32_t data = 0;
	mutex_lock(&(a_ctrl->actuator_mutex));
	if (a_ctrl->camera_actuator_shake_detect_enable && a_ctrl->cam_act_last_state == CAM_ACTUATOR_INIT)
	{
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "SDS Actuator:%s lock start", a_ctrl->actuator_name);
		a_ctrl->io_master_info.cci_client->cci_i2c_master = a_ctrl->cci_i2c_master;
		a_ctrl->io_master_info.cci_client->i2c_freq_mode = I2C_FAST_PLUS_MODE;

		if (a_ctrl->cci_client_sid > 0 && a_ctrl->sds_lock_support)
		{
			a_ctrl->io_master_info.cci_client->sid = (a_ctrl->cci_client_sid >> 1);
		}
		else
		{
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "No actuator sensor match for: %s", a_ctrl->actuator_name);
			mutex_unlock(&(a_ctrl->actuator_mutex));
			return rc;
		}
		a_ctrl->io_master_info.cci_client->retries = 0;
		a_ctrl->io_master_info.cci_client->id_map = 0;

		rc = cam_actuator_power_up(a_ctrl);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Failed for Actuator Power up failed: %d", rc);
			mutex_unlock(&(a_ctrl->actuator_mutex));
			return rc;
		}

		oplus_cam_monitor_state(a_ctrl,
					a_ctrl->v4l2_dev_str.ent_function,
					CAM_ACTUATOR_SDS_POWER_UP_TYPE,
					true);

		if (a_ctrl->sds_lock_setting.size >0 && a_ctrl->sds_lock_support)
		{
			int i;
			for(i=0; i<a_ctrl->sds_lock_setting.size; i++)
			{
				rc = oplus_cam_actuator_ram_write_extend(a_ctrl,(uint32_t)a_ctrl->sds_lock_setting.reg_setting[i].reg_addr,
											(uint32_t)a_ctrl->sds_lock_setting.reg_setting[i].reg_data,
											a_ctrl->sds_lock_setting.reg_setting[i].delay,
											a_ctrl->sds_lock_setting.addr_type,
											a_ctrl->sds_lock_setting.data_type);
				CAM_EXT_DBG(CAM_EXT_ACTUATOR, "SDS set reg: 0x%x, data: 0x%x, rc = %d",a_ctrl->sds_lock_setting.reg_setting[i].reg_addr,
													a_ctrl->sds_lock_setting.reg_setting[i].reg_data, rc);
			}
		}
		else
		{
			CAM_EXT_DBG(CAM_EXT_ACTUATOR, "not support %s rc = %d",a_ctrl->actuator_name, rc);
		}

		if (rc < 0)
		{
			int rc_power_down = cam_actuator_power_down(a_ctrl);
			oplus_cam_monitor_state(a_ctrl,
				a_ctrl->v4l2_dev_str.ent_function,
				CAM_ACTUATOR_SDS_POWER_UP_TYPE,
				false);
			if (rc_power_down < 0)
			{
				CAM_EXT_ERR(CAM_EXT_ACTUATOR, "SDS cam_actuator_power_down fail, rc_power_down = %d", rc_power_down);
			}
		}
		else
		{
			a_ctrl->cam_act_last_state = CAM_ACTUATOR_LOCK;
			//CAM_EXT_ERR(CAM_EXT_ACTUATOR, "SDS CAM_ACTUATOR_LOCK a_ctrl->cam_act_last_state %d", a_ctrl->cam_act_last_state);
		}
	}
	else
	{
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "do not support SDS(shake detect service)");
	}
	mutex_unlock(&(a_ctrl->actuator_mutex));
	return rc;
}

int32_t oplus_cam_actuator_unlock(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;

	struct cam_actuator_soc_private *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;

	if (!a_ctrl) {
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "failed: a_ctrl %pK", a_ctrl);
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	if (!power_info) {
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "failed: power_info %pK", power_info);
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	if (a_ctrl->camera_actuator_shake_detect_enable && a_ctrl->cam_act_last_state == CAM_ACTUATOR_LOCK)
	{
		CAM_EXT_INFO(CAM_EXT_ACTUATOR, "SDS Actuator:%s unlock start", a_ctrl->actuator_name);
		rc = cam_actuator_power_down(a_ctrl);
		oplus_cam_monitor_state(a_ctrl,
				a_ctrl->v4l2_dev_str.ent_function,
				CAM_ACTUATOR_SDS_POWER_UP_TYPE,
				false);
		if (rc < 0) {
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Actuator Power down failed");
		} else {
			a_ctrl->cam_act_last_state = CAM_ACTUATOR_INIT;
			kfree(power_info->power_setting);
			kfree(power_info->power_down_setting);
			power_info->power_setting = NULL;
			power_info->power_down_setting = NULL;
			power_info->power_setting_size = 0;
			power_info->power_down_setting_size = 0;
		}
	}
	else
	{
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "do not support SDS(shake detect service)");
	}

	mutex_unlock(&(a_ctrl->actuator_mutex));

	return rc;
}

int oplus_cam_actuator_ram_write_extend(struct cam_actuator_ctrl_t *a_ctrl,
			uint32_t addr, uint32_t data,unsigned short mdelay,
			enum camera_sensor_i2c_type addr_type,
			enum camera_sensor_i2c_type data_type)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	struct cam_sensor_i2c_reg_array i2c_write_setting = {
		.reg_addr = addr,
		.reg_data = data,
		.delay = 0,
		.data_mask = 0,
	};
	struct cam_sensor_i2c_reg_setting i2c_write = {
		.reg_setting = &i2c_write_setting,
		.size = 1,
		.addr_type = addr_type,
		.data_type = data_type,
		.delay = mdelay,
	};

	if (a_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	if ((addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID) || (addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX)
		|| (data_type <= CAMERA_SENSOR_I2C_TYPE_INVALID) || (data_type >= CAMERA_SENSOR_I2C_TYPE_MAX))
	{
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "addr_type: %d, data_type: %d, is not invalid", addr_type, data_type);
		return -EINVAL;
	}

	for(i = 0; i < retry; i++) {
		rc = cam_ext_io_dev_write(a_ctrl->io_master_info.cci_client, &i2c_write);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "actuator write 0x%x failed, retry:%d", addr, i+1);
		}
		else
		{
			CAM_EXT_DBG(CAM_EXT_ACTUATOR, "actuator write success 0x%x, data:=0x%x", addr, data);
			return rc;
		}
	}
	return rc;
}

int oplus_cam_actuator_ram_read_extend(struct cam_actuator_ctrl_t *a_ctrl,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	int32_t rc = 0;
	int retry = 3;
	int i;

	if (a_ctrl == NULL)
	{
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	if ((addr_type <= CAMERA_SENSOR_I2C_TYPE_INVALID) || (addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX)
		|| (data_type <= CAMERA_SENSOR_I2C_TYPE_INVALID) || (data_type >= CAMERA_SENSOR_I2C_TYPE_MAX))
	{
		CAM_EXT_ERR(CAM_EXT_ACTUATOR, "addr_type: %d, data_type: %d, is not invalid", addr_type, data_type);
		return -EINVAL;
	}

	for(i = 0; i < retry; i++)
	{
		rc = cam_ext_io_dev_read(a_ctrl->io_master_info.cci_client, (uint32_t)addr, (uint32_t *)data,
			addr_type, data_type,false);
		if (rc < 0)
		{
			CAM_EXT_ERR(CAM_EXT_ACTUATOR, "read 0x%x failed, retry:%d", addr, i+1);
		}
		else
		{
			return rc;
		}
	}
	return rc;
}

