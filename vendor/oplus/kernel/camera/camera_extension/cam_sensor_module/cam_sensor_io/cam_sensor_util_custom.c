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

int32_t cam_ext_fill_vreg_params(
	struct cam_hw_soc_info *soc_info,
	struct cam_sensor_power_setting *power_setting,
	uint16_t power_setting_size)
{
	int32_t rc = 0, j = 0, i = 0;
	int num_vreg;

	/* Validate input parameters */
	if (!soc_info || !power_setting || (power_setting_size <= 0)) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s %d failed: soc_info %p power_setting %p",
			__func__, __LINE__, soc_info, power_setting);
		return -EINVAL;
	}

	num_vreg = soc_info->num_rgltr;

	if ((num_vreg <= 0) || (num_vreg > CAM_SOC_MAX_REGULATOR)) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s %d failed: num_vreg %d", __func__, __LINE__, num_vreg);
		return -EINVAL;
	}

	for (i = 0; i < power_setting_size; i++) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s [%d] seq_type: %d", __func__, i, power_setting[i].seq_type);

		if (power_setting[i].seq_type < SENSOR_MCLK ||
			power_setting[i].seq_type >= SENSOR_SEQ_TYPE_MAX) {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "%s %d failed: Invalid Seq type: %d", __func__, __LINE__,
				power_setting[i].seq_type);
			return -EINVAL;
		}

		switch (power_setting[i].seq_type) {
		case SENSOR_VDIG:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j], "cam_vdig")) {
					CAM_EXT_INFO(CAM_EXT_SENSOR, "%s i: %d j: %d cam_vdig", __func__, i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_VIO:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j], "cam_vio")) {
					CAM_EXT_INFO(CAM_EXT_SENSOR, "%s i: %d j: %d cam_vio", __func__, i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_VANA:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j], "cam_vana")) {
					CAM_EXT_INFO(CAM_EXT_SENSOR, "%s i: %d j: %d cam_vana", __func__, i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_VAF:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j], "cam_vaf")) {
					CAM_EXT_INFO(CAM_EXT_SENSOR, "%s i: %d j: %d cam_vaf", __func__, i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}

					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_CUSTOM_REG1:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j], "cam_v_custom1")) {
					CAM_EXT_INFO(CAM_EXT_SENSOR, "%s i: %d j: %d cam_vcustom1", __func__, i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;
		case SENSOR_CUSTOM_REG2:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j], "cam_v_custom2")) {
					CAM_EXT_INFO(CAM_EXT_SENSOR, "%s i: %d j: %d cam_vcustom2", __func__, i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;
		default:
			break;
		}
	}

	return rc;
}

int cam_ext_pinctrl_init(struct msm_pinctrl_info *sensor_pctrl,
	struct device *dev)
{
	if (!sensor_pctrl || !dev) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s invalid params, sensor_pctrl %p, dev %p",
			__func__, sensor_pctrl, dev);
		return -EINVAL;
	}
	sensor_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(sensor_pctrl->pinctrl)) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s Getting pinctrl handle failed", __func__);
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_active =
		pinctrl_lookup_state(sensor_pctrl->pinctrl,
				CAM_SENSOR_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_active)) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Failed to get the active state pinctrl handle", __func__);
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_suspend
		= pinctrl_lookup_state(sensor_pctrl->pinctrl,
				CAM_SENSOR_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_suspend)) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Failed to get the suspend state pinctrl handle", __func__);
		return -EINVAL;
	}
	return 0;
}

int cam_ext_res_mgr_gpio_request(struct device *dev, uint gpio,
	unsigned long flags, const char *label)
{
	int rc = 0;

	rc = gpio_request_one(gpio, flags, label);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s gpio %d:%s request fails", __func__,
			gpio, label);
		return rc;
	}
	return rc;
}

void cam_ext_res_mgr_gpio_free_arry(const struct gpio *array, size_t num)
{
	while (num--) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s free array[%d].gpio", __func__, num);
		gpio_free(array[num].gpio);
	}
}

int cam_ext_sensor_util_request_gpio_table(struct cam_hw_soc_info *soc_info,
	int gpio_en)
{
	int rc = 0, i = 0;
	u8 size = 0;
	struct cam_soc_gpio_data *gpio_conf = NULL;
	struct gpio *gpio_tbl = NULL;

	if (!soc_info) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s invalid params", __func__);
		return -EINVAL;
	}
	gpio_conf = soc_info->gpio_data;

	if (!gpio_conf) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s No GPIO data", __func__);
		return 0;
	}

	if (gpio_conf->cam_gpio_common_tbl_size <= 0) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s No GPIO entry", __func__);
		return -EINVAL;
	}

	gpio_tbl = gpio_conf->cam_gpio_req_tbl;
	size = gpio_conf->cam_gpio_req_tbl_size;

	if (!gpio_tbl || !size) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s invalid gpio_tbl %p / size %d", __func__,
			gpio_tbl, size);
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s i: %d, gpio %d dir %ld", __func__, i,
			gpio_tbl[i].gpio, gpio_tbl[i].flags);
	}

	if (gpio_en) {
		for (i = 0; i < size; i++) {
			rc = cam_ext_res_mgr_gpio_request(soc_info->dev,
					gpio_tbl[i].gpio,
					gpio_tbl[i].flags, gpio_tbl[i].label);
			if (rc) {
				CAM_EXT_ERR(CAM_EXT_SENSOR, "%s gpio %d:%s request fails", __func__,
					gpio_tbl[i].gpio, gpio_tbl[i].label);
			}
		}
	} else {
		cam_ext_res_mgr_gpio_free_arry(gpio_tbl, size);
	}
	return rc;
}

//extern int set_camera_regulator(struct regulator *rgltr, bool enable);
//extern int set_camera_clk(struct clk *clk,bool enable);

int cam_ext_soc_util_regulator_enable(struct regulator *rgltr,
	const char *rgltr_name,
	uint32_t rgltr_min_volt, uint32_t rgltr_max_volt,
	uint32_t rgltr_op_mode, uint32_t rgltr_delay)
{
	int32_t rc = 0;

	if (!rgltr) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Invalid NULL parameter", __func__);
		return -EINVAL;
	}

	if (regulator_count_voltages(rgltr) > 0) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s voltage min=%d, max=%d", __func__,
			rgltr_min_volt, rgltr_max_volt);

		rc = regulator_set_voltage(
			rgltr, rgltr_min_volt, rgltr_max_volt);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "%s %s set voltage failed", __func__, rgltr_name);
			return rc;
		}

		rc = regulator_set_load(rgltr, rgltr_op_mode);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "%s %s set optimum mode failed", __func__,
				rgltr_name);
			return rc;
		}
	}

	rc = regulator_enable(rgltr);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s %s regulator_enable failed", __func__,
				rgltr_name);
		return rc;
	}

//	rc = set_camera_regulator(rgltr, true);

	if (rgltr_delay > 20)
		msleep(rgltr_delay);
	else if (rgltr_delay)
		usleep_range(rgltr_delay * 1000,
			(rgltr_delay * 1000) + 1000);

	return rc;
}

int cam_ext_soc_util_set_clk_rate(struct clk *clk, const char *clk_name,
	int64_t clk_rate)
{
	int rc = 0;
	long clk_rate_round;

	if (!clk || !clk_name) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s invalid params, clk %p, clk_name %p",
			__func__, clk, clk_name);
		return -EINVAL;
	}

	CAM_EXT_INFO(CAM_EXT_SENSOR, "%s set %s, rate %lld", __func__, clk_name, clk_rate);
	if (clk_rate > 0) {
		clk_rate_round = clk_round_rate(clk, clk_rate);
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s new_rate %ld", __func__, clk_rate_round);
		if (clk_rate_round < 0) {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "%s round failed for clock %s rc = %ld", __func__,
				clk_name, clk_rate_round);
			return clk_rate_round;
		}
		rc = clk_set_rate(clk, clk_rate_round);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "%s set_rate failed on %s", __func__, clk_name);
			return rc;
		}
	} else if (clk_rate == INIT_RATE) {
		clk_rate_round = clk_get_rate(clk);
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s init new_rate %ld", __func__, clk_rate_round);
		if (clk_rate_round == 0) {
			clk_rate_round = clk_round_rate(clk, 0);
			if (clk_rate_round <= 0) {
				CAM_EXT_ERR(CAM_EXT_SENSOR, "%s round rate failed on %s",__func__, clk_name);
				return clk_rate_round;
			}
		}
		rc = clk_set_rate(clk, clk_rate_round);
		if (rc) {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "%s set_rate failed on %s", __func__, clk_name);
			return rc;
		}
	}

	return rc;
}

int cam_ext_soc_util_clk_enable(struct clk *clk,
	const char *clk_name, int32_t clk_rate)
{
	int rc = 0;

	if (!clk || !clk_name) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s invalid params, clk %p, clk_name %p",
			__func__, clk, clk_name);
		return -EINVAL;
	}

	rc = cam_ext_soc_util_set_clk_rate(clk, clk_name, clk_rate);
	if (rc)
		return rc;

	rc = clk_prepare_enable(clk);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s enable failed for %s: rc(%d)", __func__, clk_name, rc);
		return rc;
	}

//	set_camera_clk(clk,true);

	return rc;
}

int cam_ext_soc_util_regulator_disable(struct regulator *rgltr,
	const char *rgltr_name, uint32_t rgltr_min_volt,
	uint32_t rgltr_max_volt, uint32_t rgltr_op_mode,
	uint32_t rgltr_delay_ms)
{
	int32_t rc = 0;

	if (!rgltr) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Invalid NULL parameter", __func__);
		return -EINVAL;
	}

	rc = regulator_disable(rgltr);
	if (rc) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s %s regulator disable failed", __func__, rgltr_name);
		return rc;
	}

//	rc = set_camera_regulator(rgltr, false);

	if (rgltr_delay_ms > 20)
		msleep(rgltr_delay_ms);
	else if (rgltr_delay_ms)
		usleep_range(rgltr_delay_ms * 1000,
			(rgltr_delay_ms * 1000) + 1000);

	if (regulator_count_voltages(rgltr) > 0) {
		regulator_set_load(rgltr, 0);
		regulator_set_voltage(rgltr, 0, rgltr_max_volt);
	}

	return rc;
}

void cam_ext_res_mgr_gpio_set_value(unsigned int gpio, int value)
{
	gpio_set_value_cansleep(gpio, value);
}

void cam_ext_cam_sensor_handle_reg_gpio(int seq_type,
	struct msm_camera_gpio_num_info *gpio_num_info, int val)
{
	int gpio_offset = -1;

	if (!gpio_num_info) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s Input Parameters are not proper", __func__);
		return;
	}

	CAM_EXT_INFO(CAM_EXT_SENSOR, "%s Seq type: %d, config: %d", __func__, seq_type, val);

	gpio_offset = seq_type;

	if (gpio_num_info->valid[gpio_offset] == 1) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s VALID GPIO offset: %d, seqtype: %d", __func__,
			 gpio_offset, seq_type);
		cam_ext_res_mgr_gpio_set_value(gpio_num_info->gpio_num[gpio_offset], val);
	}
}

int cam_ext_soc_util_clk_disable(struct clk *clk, const char *clk_name)
{
	if (!clk || !clk_name) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s invalid params, clk %p, clk_name %p",
			__func__, clk, clk_name);
		return -EINVAL;
	}

	CAM_EXT_INFO(CAM_EXT_SENSOR, "%s disable %s", __func__, clk_name);
	clk_disable_unprepare(clk);

//	set_camera_clk(clk,false);

	return 0;
}

void cam_ext_update_mclk_down_index(struct cam_sensor_power_ctrl_t *ctrl)
{
	int32_t index = 0;
	struct cam_sensor_power_setting *pd = NULL;

	if (!ctrl) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s power ctrl is null", __func__);
		return;
	}

	for (index = 0; index < ctrl->power_down_setting_size; index++) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s power_down_index %d", __func__, index);
		pd = &ctrl->power_down_setting[index];
		if (!pd) {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Invalid power down settings for index %d", __func__, index);
			return;
		}

		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s power down seq_type %d", __func__, pd->seq_type);
		if (pd->seq_type == SENSOR_MCLK) {
			mclk_down_index = index;
			CAM_EXT_INFO(CAM_EXT_SENSOR, "%s mclk_down_index: %d", __func__, mclk_down_index);
			break;
		}
	}
}

int32_t cam_ext_disable_mclk(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info)
{
	int32_t i = 0;
	int32_t ret = 0;

	if (mclk_enabled != 1) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s MCLK already disabled", __func__);
		return 0;
	}

	if (!ctrl || !soc_info) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s power ctrl or soc_info is null", __func__);
		return -1;
	}

	if (mclk_down_index < 0) {
		cam_ext_update_mclk_down_index(ctrl);
		if (mclk_down_index < 0) {
			CAM_EXT_ERR(CAM_EXT_SENSOR, "%s mclk_down_index = %d", __func__, mclk_down_index);
			return -1;
		}
	}

	for (i = soc_info->num_clk - 1; i >= 0; i--) {
		cam_ext_soc_util_clk_disable(soc_info->clk[i],
			soc_info->clk_name[i]);
	}
	ret = cam_ext_config_mclk_reg(ctrl, soc_info, mclk_down_index);
	if (ret < 0) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s config clk reg failed rc: %d", __func__, ret);
	} else
		mclk_enabled = 0;

	return ret;
}

int cam_ext_config_mclk_reg(struct cam_sensor_power_ctrl_t *ctrl,
	struct cam_hw_soc_info *soc_info, int32_t index)
{
	int32_t num_vreg = 0, j = 0, rc = 0, idx = 0;
	struct cam_sensor_power_setting *ps = NULL;
	struct cam_sensor_power_setting *pd = NULL;

	num_vreg = soc_info->num_rgltr;

	pd = &ctrl->power_down_setting[index];

	for (j = 0; j < num_vreg; j++) {
		if (!strcmp(soc_info->rgltr_name[j], "cam_clk")) {
			ps = NULL;
			for (idx = 0; idx < ctrl->power_setting_size; idx++) {
				if (ctrl->power_setting[idx].seq_type ==
					pd->seq_type) {
					ps = &ctrl->power_setting[idx];
					break;
				}
			}

			if (ps != NULL) {
				CAM_EXT_ERR(CAM_EXT_SENSOR, "%s Disable MCLK Regulator",__func__);
				rc = cam_ext_soc_util_regulator_disable(
					soc_info->rgltr[j],
					soc_info->rgltr_name[j],
					soc_info->rgltr_min_volt[j],
					soc_info->rgltr_max_volt[j],
					soc_info->rgltr_op_mode[j],
					soc_info->rgltr_delay[j]);

				if (rc) {
					CAM_EXT_ERR(CAM_EXT_SENSOR, "%s MCLK REG DISALBE FAILED: %d",__func__,
						rc);
					return rc;
				}

				ps->data[0] =
					soc_info->rgltr[j];
			}
		}
	}

	return rc;
}

struct cam_sensor_power_setting *cam_ext_get_power_settings(
	struct cam_sensor_power_ctrl_t *ctrl,
	enum msm_camera_power_seq_type seq_type,
	uint16_t seq_val)
{
	struct cam_sensor_power_setting *power_setting = NULL, *ps = NULL;
	int idx = 0;

	if (!ctrl) {
		CAM_EXT_ERR(CAM_EXT_SENSOR, "%s invalid params", __func__);
		return NULL;
	}

	for (idx = 0; idx < ctrl->power_setting_size; idx++) {
		power_setting = &ctrl->power_setting[idx];
		if (power_setting->seq_type == seq_type &&
			power_setting->seq_val == seq_val) {
			ps = power_setting;
			return ps;
		}
	}

	return ps;
}

int cam_ext_sensor_handle_reg_gpio(int seq_type,
	struct msm_camera_gpio_num_info *gpio_num_info, int val)
{
	int gpio_offset = -1;

	if (!gpio_num_info) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s: Input Parameters are not proper", __func__);
		return 0;
	}

	CAM_EXT_INFO(CAM_EXT_SENSOR, "%s: Seq type: %d, config: %d", __func__, seq_type, val);

	gpio_offset = seq_type;

	if (gpio_num_info->valid[gpio_offset] == 1) {
		CAM_EXT_INFO(CAM_EXT_SENSOR, "%s: VALID GPIO offset: %d, seqtype: %d", __func__,
			 gpio_offset, seq_type);
		cam_ext_res_mgr_gpio_set_value(gpio_num_info->gpio_num[gpio_offset], val);
	}

	return 0;
}
