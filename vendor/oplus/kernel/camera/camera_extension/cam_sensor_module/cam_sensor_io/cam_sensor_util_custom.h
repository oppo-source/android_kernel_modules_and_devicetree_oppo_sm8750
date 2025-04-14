#ifndef _CAM_SENSOR_UTIL_CUSTOM_H
#define _CAM_SENSOR_UTIL_CUSTOM_H

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
#include <cam_sensor_util.h>

static int32_t mclk_up_index = -1;
static int32_t mclk_down_index = -1;
static int32_t mclk_enabled = -1;

#define CAM_SENSOR_PINCTRL_STATE_SLEEP "cam_suspend"
#define CAM_SENSOR_PINCTRL_STATE_DEFAULT "cam_default"

#define VALIDATE_VOLTAGE(min, max, config_val) ((config_val) && \
	(config_val >= min) && (config_val <= max))

int32_t cam_ext_fill_vreg_params(
	struct cam_hw_soc_info *soc_info,
	struct cam_sensor_power_setting *power_setting,
	uint16_t power_setting_size);

int cam_ext_pinctrl_init(struct msm_pinctrl_info *sensor_pctrl,
	struct device *dev);

int cam_ext_res_mgr_gpio_request(struct device *dev, uint gpio,
	unsigned long flags, const char *label);

void cam_ext_res_mgr_gpio_free_arry(const struct gpio *array, size_t num);

int cam_ext_sensor_util_request_gpio_table(struct cam_hw_soc_info *soc_info,
	int gpio_en);

int cam_ext_soc_util_regulator_enable(struct regulator *rgltr,
	const char *rgltr_name,
	uint32_t rgltr_min_volt, uint32_t rgltr_max_volt,
	uint32_t rgltr_op_mode, uint32_t rgltr_delay);

int cam_ext_soc_util_set_clk_rate(struct clk *clk, const char *clk_name,
	int64_t clk_rate);

int cam_ext_soc_util_clk_enable(struct clk *clk,
	const char *clk_name, int32_t clk_rate);

int cam_ext_soc_util_regulator_disable(struct regulator *rgltr,
	const char *rgltr_name, uint32_t rgltr_min_volt,
	uint32_t rgltr_max_volt, uint32_t rgltr_op_mode,
	uint32_t rgltr_delay_ms);

void cam_ext_res_mgr_gpio_set_value(unsigned int gpio, int value);

void cam_ext_cam_sensor_handle_reg_gpio(int seq_type,
	struct msm_camera_gpio_num_info *gpio_num_info, int val);

int cam_ext_soc_util_clk_disable(struct clk *clk, const char *clk_name);

int cam_ext_config_mclk_reg(struct cam_sensor_power_ctrl_t *ctrl,
	struct cam_hw_soc_info *soc_info, int32_t index);

void cam_ext_update_mclk_down_index(struct cam_sensor_power_ctrl_t *ctrl);

int32_t cam_ext_disable_mclk(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info);

struct cam_sensor_power_setting *cam_ext_get_power_settings(
	struct cam_sensor_power_ctrl_t *ctrl,
	enum msm_camera_power_seq_type seq_type,
	uint16_t seq_val);

int cam_ext_sensor_handle_reg_gpio(int seq_type,
	struct msm_camera_gpio_num_info *gpio_num_info, int val);

#endif /* _CAM_SENSOR_UTIL_CUSTOM_H */

