#ifndef _CAM_OIS_CUSTOM_H_
#define _CAM_OIS_CUSTOM_H_

#include "cam_ois_dev.h"

#define SAMPLE_COUNT_IN_DRIVER        100
#define SAMPLE_COUNT_IN_OIS           34
#define GET_HALL_DATA_VERSION_DEFUALT         0
#define GET_HALL_DATA_VERSION_V2              1
#define GET_HALL_DATA_VERSION_V3              2
struct cam_ois_hall_data_in_ois_aligned {
	uint16_t hall_data_cnt;
	uint32_t hall_data;
};

struct cam_ois_hall_data_in_driver {
	uint32_t high_dword;
	uint32_t low_dword;
	uint32_t hall_data;
};

#define SAMPLE_SIZE_IN_OIS            6
#define SAMPLE_SIZE_IN_OIS_ALIGNED    (sizeof(struct cam_ois_hall_data_in_ois_aligned))
#define SAMPLE_SIZE_IN_DRIVER         (sizeof(struct cam_ois_hall_data_in_driver))
#define CLOCK_TICKCOUNT_MS            19200
#define OIS_MAGIC_NUMBER              0x7777
#define OIS_MAX_COUNTER               36
#define SAMPLE_COUNT_IN_NCS_DATA      32
#define SAMPLE_COUNT_IN_NCS_DATA_TELE124  28

#define SAMPLE_SIZE_IN_OIS_NCS        144

//extern struct mutex actuator_ois_eeprom_shared_mutex;
//extern bool actuator_ois_eeprom_shared_mutex_init_flag;

int cam_ext_ois_driver_soc_init(struct cam_ois_ctrl_t *o_ctrl);

int cam_ext_ois_driver_cmd(struct cam_ois_ctrl_t *o_ctrl, void *arg);

int cam_ext_ois_power_down(struct cam_ois_ctrl_t *o_ctrl);
int cam_ext_ois_power_up(struct cam_ois_ctrl_t *o_ctrl);
int cam_ext_ois_power_down_thread(void *arg);
void cam_ext_ois_shutdown(struct cam_ois_ctrl_t *o_ctrl);

void cam_ois_register(void);
int cam_ext_sensor_i2c_read_data(struct cam_ois_ctrl_t *o_ctrl, struct i2c_settings_array *i2c_read_settings);

int cam_ext_ois_component_bind(struct cam_ois_ctrl_t *o_ctrl);

extern int32_t cam_sensor_util_get_current_qtimer_ns(uint64_t *qtime_ns);
extern int32_t camera_io_init(struct camera_io_master *io_master_info);
extern int32_t camera_io_release(struct camera_io_master *io_master_info);

#endif
/* _CAM_OIS_CUSTOM_H_ */