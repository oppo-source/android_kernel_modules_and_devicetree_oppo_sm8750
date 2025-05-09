
#include "sensor_interface.h"
#include "sensor_list.h"
#include "hf_sensor_type.h"
#include <linux/rtc.h>
#include "hf_manager.h"
#include <linux/slab.h>
#include <linux/string.h>
#include <sensor_comm.h>
#include "scp.h"

static struct sensor_info sensor_list[SENSOR_TYPE_SENSOR_MAX];

extern int sensor_list_get_list(struct sensor_info *list, unsigned int num);
extern int get_support_sensor_list(struct sensor_info *list);

static int oplus_send_comm_to_hub(int sensor_type, int cmd, void *data, uint8_t length)
{
    int ret = 0;
    struct sensor_comm_ctrl *ctrl = NULL;

    ctrl = kzalloc(sizeof(*ctrl) + length, GFP_KERNEL);
    ctrl->sensor_type = sensor_type;
    ctrl->command = cmd;
    ctrl->length = length;
    if (length){
        memcpy(ctrl->data, data, length);
    }
    ret = sensor_comm_ctrl_send(ctrl, sizeof(*ctrl) + ctrl->length);
    kfree(ctrl);
    return ret;
}

static int oplus_send_factory_mode_cmd_to_hub(int sensor_type, int mode, void *result)
{
    return oplus_send_comm_to_hub(sensor_type, OPLUS_ACTION_SET_FACTORY_MODE, &mode, sizeof(mode));
}

static int oplus_send_selftest_cmd_to_hub(int sensor_type, void *testresult)
{
    return oplus_send_comm_to_hub(sensor_type, OPLUS_ACTION_SELF_TEST, NULL, 0);
}

static int send_utc_time_to_hub(void)
{
    struct timespec64 tv={0};
    struct rtc_time tm;
    uint32_t utc_data[4] = {0};

    ktime_get_real_ts64(&tv);
    rtc_time64_to_tm(tv.tv_sec, &tm);

    utc_data[0] = (uint32_t)tm.tm_mday;
    utc_data[1] = (uint32_t)tm.tm_hour;
    utc_data[2] = (uint32_t)tm.tm_min;
    utc_data[3] = (uint32_t)tm.tm_sec;
    return oplus_send_comm_to_hub(0, OPLUS_ACTION_SCP_SYNC_UTC, utc_data, sizeof(utc_data));
}

static int send_lcdinfo_to_hub(struct als_info *lcd_info)
{
	DEVINFO_LOG("type:%u, brightness:%u, dc_mode:%u, pwm_turbo:%u\n",
		(uint32_t)lcd_info->senstype,
		(uint32_t)lcd_info->brightness,
		(uint32_t)lcd_info->dc_mode,
		(uint32_t)lcd_info->pwm_turbo);

	return oplus_send_comm_to_hub(lcd_info->senstype, OPLUS_ACTION_SET_LCD_INFO, lcd_info, sizeof(struct als_info));
}

#if IS_ENABLED(CONFIG_OPLUS_SENSOR_USE_SCREENSHOT_INFO)
static int send_sf_info_to_hub(struct screen_sf_info *sf_info)
{
	DEVINFO_LOG("type:%u, start_ts:%lld, end_ts:%lld, index:%d\n",
		sf_info->senstype,
		sf_info->start_ts,
		sf_info->end_ts,
		sf_info->index);

	return oplus_send_comm_to_hub(sf_info->senstype, OPLUS_ACTION_SET_SF_INFO, sf_info, sizeof(struct screen_sf_info));
}
#endif /* CONFIG_OPLUS_SENSOR_USE_SCREENSHOT_INFO */

static void init_sensorlist(void)
{
   int ret = 0;
   ret = sensor_list_get_list(sensor_list, SENSOR_TYPE_SENSOR_MAX);
   if (ret < 0) {
        get_support_sensor_list(sensor_list);
        DEVINFO_LOG("get sensor list\n");
   }
}

bool is_sensor_available(char *name)
{
    bool find = false;
    int handle = 0;

    for (handle = 0; handle < SENSOR_TYPE_SENSOR_MAX; ++handle) {
        if (name && (strstr(sensor_list[handle].name, name))) {
            find = true;
            break;
        }
    }

    return find;
}

bool is_sensor_type_available(uint8_t sensor_type)
{
    bool find = false;
    int handle = 0;

    for (handle = 0; handle < SENSOR_TYPE_SENSOR_MAX; ++handle) {
        if (sensor_list[handle].sensor_type == sensor_type) {
            find = true;
            break;
        }
    }

    return find;
}

static bool get_sensor_name(uint8_t sensor_type, char **name)
{
	bool find = false;
	int handle = 0;

	for (handle = 0; handle < SENSOR_TYPE_SENSOR_MAX; ++handle) {
		if (sensor_list[handle].sensor_type == sensor_type) {
			*name = sensor_list[handle].name;
			find = true;
			break;
		}
	}

	return find;
}

static int get_lcdinfo_brocast_type(void)
{
	int type = 0;

	if (is_sensor_type_available(SENSOR_TYPE_PWM_RGB)) {
		type = SENSOR_TYPE_PWM_RGB;
	} else if (is_sensor_type_available(SENSOR_TYPE_RGBW)) {
		type = SENSOR_TYPE_RGBW;
	} else if (is_sensor_type_available(SENSOR_TYPE_FRONT_CCT)) {
		type = SENSOR_TYPE_FRONT_CCT;
	} else {
		type = SENSOR_TYPE_RGBW;
	}
	DEVINFO_LOG("lcd_senstype %d", type);
	return type;
}

struct sensorhub_interface sensorhub_v2 = {
    .send_factory_mode = oplus_send_factory_mode_cmd_to_hub,
    .send_selft_test = oplus_send_selftest_cmd_to_hub,
    .send_reg_config = NULL,
    .send_cfg = NULL,
    .send_utc_time = send_utc_time_to_hub,
    .send_lcdinfo = send_lcdinfo_to_hub,
#if IS_ENABLED(CONFIG_OPLUS_SENSOR_USE_SCREENSHOT_INFO)
    .send_sf_info = send_sf_info_to_hub,
#endif /* CONFIG_OPLUS_SENSOR_USE_SCREENSHOT_INFO */
    .init_sensorlist = init_sensorlist,
    .is_sensor_available = is_sensor_available,
    .is_sensor_type_available = is_sensor_type_available,
    .get_lcdinfo_brocast_type = get_lcdinfo_brocast_type,
    .get_sensor_name = get_sensor_name,
};

void init_sensorhub_interface(struct sensorhub_interface **si)
{
    *si = &sensorhub_v2;
}
