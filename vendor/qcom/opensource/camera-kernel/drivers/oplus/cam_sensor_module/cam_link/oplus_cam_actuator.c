#include <linux/module.h>
#include <linux/crc32.h>
#include <media/cam_sensor.h>

#include "cam_actuator_core.h"
#include "cam_actuator_soc.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#include "oplus_cam_actuator.h"

int32_t oplus_cam_actuator_ignore_init_error(struct cam_actuator_ctrl_t *a_ctrl, int32_t rc)
{
	if(a_ctrl->is_af_ignore_init_error != 0)
	{
		return 0;
	} else {
		return rc;
	}
}

int32_t oplus_cam_actuator_construct_default_power_setting(
	struct cam_actuator_ctrl_t *a_ctrl,
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;
	int pwr_up_count = 0;
	int pwr_down_count = 0;

	if (a_ctrl->power_setting_size > 0)
	{
		power_info->power_setting_size = a_ctrl->power_setting_size;
		power_info->power_setting =
			kzalloc(sizeof(struct cam_sensor_power_setting) * a_ctrl->power_setting_size,
				GFP_KERNEL);
	}
	else
	{
		power_info->power_setting_size = DEFULT_POWER_SIZE;
		power_info->power_setting =
			kzalloc(sizeof(struct cam_sensor_power_setting) * DEFULT_POWER_SIZE,
				GFP_KERNEL);
	}
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_setting[pwr_up_count].seq_type = SENSOR_VIO;
	power_info->power_setting[pwr_up_count].seq_val = CAM_VIO;
	power_info->power_setting[pwr_up_count].config_val = 1;
	power_info->power_setting[pwr_up_count].delay = 2;
	pwr_up_count++;

	if(true == a_ctrl->power_custom1_reg)
	{
		power_info->power_setting[pwr_up_count].seq_type = SENSOR_CUSTOM_REG1;
		power_info->power_setting[pwr_up_count].seq_val = CAM_V_CUSTOM1;
		power_info->power_setting[pwr_up_count].config_val = 1;
		power_info->power_setting[pwr_up_count].delay = 8;
		pwr_up_count++;
	}

	power_info->power_setting[pwr_up_count].seq_type = SENSOR_VAF;
	power_info->power_setting[pwr_up_count].seq_val = CAM_VAF;
	power_info->power_setting[pwr_up_count].config_val = 1;
	power_info->power_setting[pwr_up_count].delay = 10;
	pwr_up_count++;

	if (a_ctrl->power_setting_size)
	{
		power_info->power_down_setting_size = a_ctrl->power_setting_size;
		power_info->power_down_setting =
			kzalloc(sizeof(struct cam_sensor_power_setting) * a_ctrl->power_setting_size,
				GFP_KERNEL);
	}
	else
	{
		power_info->power_down_setting_size = DEFULT_POWER_SIZE;
		power_info->power_down_setting =
			kzalloc(sizeof(struct cam_sensor_power_setting) * DEFULT_POWER_SIZE,
				GFP_KERNEL);
	}
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	if(true == a_ctrl->power_custom1_reg)
	{
		power_info->power_down_setting[pwr_down_count].seq_type = SENSOR_CUSTOM_REG1;
		power_info->power_down_setting[pwr_down_count].seq_val = CAM_V_CUSTOM1;
		power_info->power_down_setting[pwr_down_count].config_val = 1;
		power_info->power_down_setting[pwr_down_count].delay = 0;
		pwr_down_count++;
	}

	power_info->power_down_setting[pwr_down_count].seq_type = SENSOR_VAF;
	power_info->power_down_setting[pwr_down_count].seq_val = CAM_VAF;
	power_info->power_down_setting[pwr_down_count].config_val = 0;
	power_info->power_down_setting[pwr_down_count].delay = 1;
	pwr_down_count++;

	power_info->power_down_setting[pwr_down_count].seq_type = SENSOR_VIO;
	power_info->power_down_setting[pwr_down_count].seq_val = CAM_VIO;
	power_info->power_down_setting[pwr_down_count].config_val = 0;
	power_info->power_down_setting[pwr_down_count].delay = 0;
	pwr_down_count++;

	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	power_info->power_setting = NULL;
	power_info->power_setting_size = 0;
	return rc;
}
#ifdef OPLUS_FEATURE_CAMERA_COMMON
EXPORT_SYMBOL(oplus_cam_actuator_construct_default_power_setting);
#endif

int32_t oplus_cam_actuator_power_up(
	struct cam_actuator_ctrl_t *a_ctrl,
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;
	CAM_INFO(CAM_ACTUATOR,
		"power_up after power_setting freed when power down, Using default power settings");
	rc = oplus_cam_actuator_construct_default_power_setting(a_ctrl, power_info);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR,
			"Construct default actuator power setting failed.");
		return rc;
	}

	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		&a_ctrl->soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed to fill vreg params for power up rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		&a_ctrl->soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed to fill vreg params power down rc:%d", rc);
		return rc;
	}

	return rc;
}

int oplus_cam_actuator_read_current(void *arg)
{
	int rc = 0;
	struct cam_actuator_ctrl_t *a_ctrl = (struct cam_actuator_ctrl_t *)arg;
	uint32_t register_value = 0;
	int af_current = 0;
	while (!kthread_should_stop()) {
		if (gpio_get_value_cansleep(a_ctrl->pull_gpio + GPIO_DYNAMIC_BASE) == GPIOF_INIT_LOW)
		{
			rc = camera_io_dev_read(
				&(a_ctrl->io_master_info),
				a_ctrl->current_register,
				&register_value,
				CAMERA_SENSOR_I2C_TYPE_BYTE,
				CAMERA_SENSOR_I2C_TYPE_WORD, true);
			af_current = (uint32_t)(register_value >> 7);
			if (af_current < a_ctrl->min_current || af_current > a_ctrl->max_current)
			{
				gpio_set_value_cansleep(a_ctrl->pull_gpio + GPIO_DYNAMIC_BASE, GPIOF_INIT_HIGH);
				CAM_INFO(CAM_ACTUATOR, "min_current: %d max_current:%d, current:%d, need pull gpio:%d", a_ctrl->min_current, a_ctrl->max_current, af_current, a_ctrl->pull_gpio);
			}
		}
		usleep_range(500, 500);
	}
	CAM_ERR(CAM_ACTUATOR, "actuator: %s oplus_cam_actuator_read_current exit", a_ctrl->actuator_name);
	return rc;
}
#ifdef OPLUS_FEATURE_CAMERA_COMMON
EXPORT_SYMBOL(oplus_cam_actuator_read_current);
#endif

