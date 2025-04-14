#ifndef _CAM_SENSOR_CUSTOM_H
#define _CAM_SENSOR_CUSTOM_H

#include <linux/miscdevice.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>

#include <media/cam_sensor.h>
#include <media/cam_defs.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_io.h>
#include <cam_soc_util.h>
#include <cam_sensor_dev.h>
#include <oplus_cam_sensor.h>

#include "../../include/main.h"
#include "cam_monitor.h"

struct cam_sensor_i2c_reg_setting_array {
	struct cam_sensor_i2c_reg_array reg_setting[CAM_OEM_INITSETTINGS_SIZE_MAX];
	unsigned short size;
	enum camera_sensor_i2c_type addr_type;
	enum camera_sensor_i2c_type data_type;
	unsigned short delay;
};

struct cam_sensor_settings1 {
	struct cam_sensor_i2c_reg_setting_array sensor_setting_array[CAM_SENSOR_TYPE_MAX];
};

struct cam_sensor_settings {
	struct cam_sensor_i2c_reg_setting_array streamoff;
	struct cam_sensor_i2c_reg_setting_array imx615_setting;
	struct cam_sensor_i2c_reg_setting_array imx615_setting_fab2;
	struct cam_sensor_i2c_reg_setting_array imx766_setting;
	struct cam_sensor_i2c_reg_setting_array imx709_setting;
	struct cam_sensor_i2c_reg_setting_array imx709_aon_irq_setting;
	struct cam_sensor_i2c_reg_setting_array imx709_aon_irq_he_clr_setting;
	struct cam_sensor_i2c_reg_setting_array imx581_setting;
	struct cam_sensor_i2c_reg_setting_array imx989_setting;
	struct cam_sensor_i2c_reg_setting_array imx890_setting;
	struct cam_sensor_i2c_reg_setting_array imx888_setting;
	struct cam_sensor_i2c_reg_setting_array ov64b40_setting;
	struct cam_sensor_i2c_reg_setting_array imx858_setting;
	struct cam_sensor_i2c_reg_setting_array imx966_setting;
	struct cam_sensor_i2c_reg_setting_array imx06A_setting;
	struct cam_sensor_i2c_reg_setting_array imx06A_setting_MP;
	struct cam_sensor_i2c_reg_setting_array imx355_setting;
	struct cam_sensor_i2c_reg_setting_array imx882_setting;
	struct cam_sensor_i2c_reg_setting_array imx809_setting;
};

int32_t cam_ext_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl, void *arg);

int cam_ext_sensor_config_settings(struct cam_sensor_ctrl_t *s_ctrl);

int cam_ext_ftm_apply_setting(
	struct cam_sensor_ctrl_t *s_ctrl,
	bool apply_stream_off);

int cam_ext_ftm_power_down(struct cam_sensor_ctrl_t *s_ctrl);
int cam_ext_ftm_power_up(struct cam_sensor_ctrl_t *s_ctrl);
int cam_ext_sensor_power_up(struct cam_sensor_ctrl_t *s_ctrl);
int cam_ext_sensor_power_down(struct cam_sensor_ctrl_t *s_ctrl);
int cam_ext_sensor_stop(struct cam_sensor_ctrl_t *s_ctrl);
int cam_ext_sensor_start(struct cam_sensor_ctrl_t *s_ctrl, void *arg);

bool cam_ext_ftm_if_do(void);

int cam_ext_sensor_match_id(struct cam_sensor_ctrl_t *s_ctrl);
int32_t cam_ext_sensor_update_id_info(struct cam_cmd_probe_v2 *probe_info,
    struct cam_sensor_ctrl_t *s_ctrl);
int cam_ext_shift_sensor_mode(struct cam_sensor_ctrl_t *s_ctrl);

int cam_ext_sensor_write_continuous(struct cam_sensor_ctrl_t *s_ctrl);

void cam_sensor_register(void);

int cam_ext_write_reg(struct cam_sensor_ctrl_t *s_ctrl,
	uint32_t addr, enum camera_sensor_i2c_type addr_type,
	uint32_t data, enum camera_sensor_i2c_type data_type);

int cam_ext_read_reg(struct cam_sensor_ctrl_t *s_ctrl,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	bool is_probing);

int cam_get_sensor_reg_otp(struct cam_sensor_ctrl_t *s_ctrl);

#endif /* _CAM_SENSOR_CUSTOM_H_ */