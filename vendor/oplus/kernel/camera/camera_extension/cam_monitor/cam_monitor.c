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

#include "tof_pdrv.h"
#include "cam_debug.h"
#include "cam_monitor.h"
#include "cam_debug.h"
#include <cam_subdev.h>
#include <cam_sensor_dev.h>
#include <cam_actuator_dev.h>
#include <cam_ois_dev.h>

#define CAM_STATE_MONITOR_MAX_ENTRIES   600

#define CAM_INC_HEAD(head, max_entries, ret) \
	div_u64_rem(atomic64_add_return(1, head),\
	max_entries, (ret))

#define DUMP_MONITOR_ARRAY

#define CHECK_SUCCESS 0
#define CHECK_FAIL    1

static struct kobject *cam_extension_kobj;

int camera_state[64];
#ifdef DUMP_MONITOR_ARRAY
static int dump_count = 0;
#endif
static bool g_is_enable_dump = false;

extern struct list_head cam_req_mgr_ordered_sd_list;

struct camera_monitor_data {
	bool   enable_camera_extension;
	struct list_head        debug_reg_list;
	struct list_head        debug_clk_list;
	struct list_head        debug_state_list;
	struct mutex            reg_list_lock;
	struct mutex            clk_list_lock;
	struct mutex            state_list_lock;
	struct cam_state_queue_info state_queue;
};

struct camera_monitor_data *g_cam_monitor =NULL;

int init_cam_monitor(struct platform_device *plat_dev)
{
	int ret = 0;
	struct camera_monitor_data *monitor_data;
	struct cam_state_queue_info *state_queue_info = NULL;

	CAM_EXT_INFO(CAM_EXT_UTIL, "begin.\n");

	/* allocate camera_extension platform data */
	monitor_data = devm_kzalloc(&plat_dev->dev, sizeof(*monitor_data), GFP_KERNEL);
	if (!monitor_data) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "Failed to alloc plat_priv.\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&monitor_data->debug_reg_list);
	INIT_LIST_HEAD(&monitor_data->debug_clk_list);
	INIT_LIST_HEAD(&monitor_data->debug_state_list);
	mutex_init(&monitor_data->reg_list_lock);
	mutex_init(&monitor_data->clk_list_lock);
	mutex_init(&monitor_data->state_list_lock);

	state_queue_info = &monitor_data->state_queue;
	state_queue_info->state_monitor =  kzalloc(
				sizeof(struct cam_state_monitor) * CAM_STATE_MONITOR_MAX_ENTRIES,
				GFP_KERNEL);
	atomic64_set(&state_queue_info->state_monitor_head, -1);

	if (!state_queue_info->state_monitor)
	{
		CAM_EXT_ERR(CAM_EXT_UTIL, "failed to alloc memory for state monitor");
		goto free_epd;
	}

	g_cam_monitor = monitor_data;


	CAM_EXT_INFO(CAM_EXT_UTIL, "done.\n");
	return 0;

free_epd:
	devm_kfree(&plat_dev->dev, monitor_data);
	return ret;

}


#define OPLUS_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute oplus_attr_##_name = __ATTR(_name, _mode, _show, _store)

static ssize_t enable_dump_show(struct kobject *kobj,
	struct kobj_attribute * attr,
	char * buf)
{

	return scnprintf(buf, PAGE_SIZE, "%d\n", g_is_enable_dump);
}

static ssize_t enable_dump_store(struct kobject *kobj,
	struct kobj_attribute * attr,
	const char * buf,
	size_t count)
{
	int debug;
	sscanf(buf, "%d", &debug);
	if (debug == 0) {
		g_is_enable_dump = false;

	} else {
		g_is_enable_dump = true;
	}
	return count;
}

static OPLUS_ATTR(enable_dump, 0644, enable_dump_show, enable_dump_store);

extern struct camera_extension_data *g_plat_priv;

static ssize_t extension_dump_state_show(
	struct kobject *kobj,
	struct kobj_attribute * attr,
	char * buf)
{
	int len = 0;
	int bufsize = PAGE_SIZE;

	len = extension_dump_item(CAM_OPERATION_TYPE_STATE, buf, bufsize);

	return len;
}

static OPLUS_ATTR(extension_dump_state, 0644, extension_dump_state_show, NULL);


static ssize_t extension_dump_clk_show(
	struct kobject *kobj,
	struct kobj_attribute * attr,
	char * buf)
{
	int len = 0;
	int bufsize = PAGE_SIZE;

	len = extension_dump_item(CAM_OPERATION_TYPE_CLOCK, buf, bufsize);

	return len;
}

static OPLUS_ATTR(extension_dump_clk, 0644, extension_dump_clk_show, NULL);


static ssize_t extension_dump_rgltr_show(
	struct kobject *kobj,
	struct kobj_attribute * attr,
	char * buf)
{
	int len = 0;
	int bufsize = PAGE_SIZE;

	len = extension_dump_item(CAM_OPERATION_TYPE_REGULATOR, buf, bufsize);

	return len;
}

static OPLUS_ATTR(extension_dump_rgltr, 0644, extension_dump_rgltr_show, NULL);


static ssize_t extension_dump_monitor_show(
	struct kobject *kobj,
	struct kobj_attribute * attr,
	char * buf)
{
	ssize_t len = 0;

	len = extension_dump_item(CAM_OPERATION_TYPE_MONITOR, buf, PAGE_SIZE);
	return len;
}

static OPLUS_ATTR(extension_dump_monitor, 0644, extension_dump_monitor_show, NULL);

static ssize_t extension_debug_power_show(
	struct kobject *kobj,
	struct kobj_attribute * attr,
	char * buf)
{
	ssize_t len = 0;
	int i = 0;
	int bufsize = PAGE_SIZE;
	struct cam_subdev *csd;
	struct cam_sensor_ctrl_t *s_ctrl;

	len += scnprintf(buf + len, bufsize - len,
			"%-12s %-35s %-14s %-27s %-2s %-8s %-9s %-10s\n",
			"sensor_name", "dev_name", "rgltr_name", "rdev_name",
			"en", "vol_uV", "use_count", "open_count");

	list_for_each_entry(csd, &cam_req_mgr_ordered_sd_list, list) {
		if (csd->ent_function == CAM_SENSOR_DEVICE_TYPE) {
			s_ctrl = v4l2_get_subdevdata(&csd->sd);

			for (i = 0; i < s_ctrl->soc_info.num_rgltr; i++) {
				len += scnprintf(buf + len, bufsize - len,
					"%-12s %-35s %-14s %-27s %c  %-8d %-9d %d\n",
					s_ctrl->sensor_name,
					s_ctrl->soc_info.dev_name,
					s_ctrl->soc_info.rgltr_name[i],
					s_ctrl->soc_info.rgltr[i]->rdev->desc->name,
					(regulator_is_enabled(s_ctrl->soc_info.rgltr[i])? 'Y' : 'N'),
					regulator_get_voltage(s_ctrl->soc_info.rgltr[i]),
					s_ctrl->soc_info.rgltr[i]->rdev->use_count,
					s_ctrl->soc_info.rgltr[i]->rdev->open_count);
			}
			len += scnprintf(buf + len, bufsize - len, "\n");
		}

	}
	len += scnprintf(buf + len, bufsize - len, "=End=%zd %zd", len, bufsize - len);

	return len;
}

static ssize_t extension_debug_power_store(struct kobject *kobj,
	struct kobj_attribute * attr,
	const char * buf,
	size_t count)
{
	// const char *cp = buf;
	// int seq = 0, i = 0;
	// uint16_t power_setting_size = 0;
	// struct monitor_power_setting *power_setting;
	// char power[36];
	// int val, delay_ms;
	// struct monitor_soc_info	*soc_info = &g_plat_priv->soc_info;

	// while( (cp = strchr(cp + 1, ';'))) {
	// 	power_setting_size++;
	// }
	// power_setting_size++;

	// CAM_EXT_ERR(CAM_EXT_UTIL, "power_setting_size %d", power_setting_size);

	// power_setting = kzalloc(
	// 	sizeof(struct monitor_power_setting) * power_setting_size,
	// 	GFP_KERNEL);

	// if (!power_setting) {
	// 	CAM_EXT_ERR(CAM_EXT_UTIL, "ENOMEM!");
	// 	return -ENOMEM;
	// }

	// cp = buf;
	// for (i = 0; i < power_setting_size; i++) {
	// 	power_setting[i].valid_config = false;

	// 	if (sscanf(cp, "%s %d %d", power, &val, &delay_ms) != 3) {
	// 		CAM_EXT_ERR(CAM_EXT_UTIL, "check input: %s", cp);
	// 		break;
	// 	}
	// 	power_setting[i].valid_config	= true;
	// 	power_setting[i].delay		= delay_ms;
	// 	power_setting[i].config_val	= val;
	// 	strncpy(power_setting[i].power, power, sizeof(power_setting[i].power)-1);

	// 	if((cp = strchr(cp, ';')))
	// 		cp++;
	// }

	// for (seq = 0; seq < power_setting_size; seq++) {
	// 	if (power_setting[seq].valid_config != true) {
	// 		CAM_EXT_ERR(CAM_EXT_UTIL, "power up break %d", seq);
	// 		break;
	// 	}
	// 	for (i = 0; i < soc_info->num_rgltr; i++) {
	// 		if (!strcmp(soc_info->rgltr_name[i],
	// 			power_setting[seq].power)) {

	// 			if (!soc_info->rgltr[i]) {
	// 				CAM_EXT_ERR(CAM_EXT_CORE, "%s regulator is NULL! skip it.",
	// 						soc_info->rgltr_name[i]);
	// 				break;
	// 			}

	// 			if (power_setting[seq].config_val == 0) {
	// 				CAM_EXT_ERR(CAM_EXT_CORE, "Disable power: %s",
	// 					power_setting[seq].power);

	// 				if (regulator_disable(soc_info->rgltr[i])) {
	// 					CAM_EXT_ERR(CAM_EXT_CORE, "Disable power failed %s",
	// 						soc_info->rgltr_name[i]);
	// 				}

	// 				if (regulator_set_load(
	// 					soc_info->rgltr[i], 0)) {
	// 					CAM_EXT_ERR(CAM_EXT_CORE, "set optimum mode failed %s %d",
	// 						soc_info->rgltr_name[i],
	// 						soc_info->rgltr_op_mode[i]);
	// 				}
	// 				if (regulator_set_voltage(
	// 					soc_info->rgltr[i], 0,
	// 					soc_info->rgltr_max_volt[i])) {
	// 					CAM_EXT_ERR(CAM_EXT_CORE, "Set voltage failed %s 0 %d",
	// 						soc_info->rgltr_name[i],
	// 						soc_info->rgltr_max_volt[i]);
	// 				}

	// 			} else {
	// 				CAM_EXT_ERR(CAM_EXT_CORE, "Enable power: %s",
	// 					power_setting[seq].power);

	// 				if (power_setting[seq].config_val >
	// 					soc_info->rgltr_max_volt[i]) {
	// 					power_setting[seq].config_val = soc_info->rgltr_max_volt[i];
	// 				} else if (power_setting[seq].config_val <
	// 					soc_info->rgltr_min_volt[i]) {
	// 					power_setting[seq].config_val = soc_info->rgltr_min_volt[i];
	// 				}

	// 				if (regulator_set_voltage(
	// 					soc_info->rgltr[i],
	// 					power_setting[seq].config_val,
	// 					power_setting[seq].config_val)) {
	// 					CAM_EXT_ERR(CAM_EXT_CORE, "Set voltage failed %s %d %d",
	// 						soc_info->rgltr_name[i],
	// 						soc_info->rgltr_min_volt[i],
	// 						soc_info->rgltr_max_volt[i]);
	// 				}

	// 				if (regulator_set_load(soc_info->rgltr[i],
	// 					soc_info->rgltr_op_mode[i])) {
	// 					CAM_EXT_ERR(CAM_EXT_CORE, "set optimum mode failed %s %d",
	// 						soc_info->rgltr_name[i],
	// 						soc_info->rgltr_op_mode[i]);
	// 				}

	// 				if (regulator_enable(soc_info->rgltr[i])) {
	// 					CAM_EXT_ERR(CAM_EXT_CORE, "Enable power failed %s",
	// 						power_setting[seq].power);
	// 				}
	// 			}

	// 			if (power_setting[seq].delay > 20)
	// 				msleep(power_setting[seq].delay);
	// 			else if (power_setting[seq].delay)
	// 				usleep_range(power_setting[seq].delay * 1000,
	// 					(power_setting[seq].delay * 1000) + 1000);

	// 			break;
	// 		}
	// 	}

	// 	if (i == soc_info->num_rgltr) {
	// 		CAM_EXT_ERR(CAM_EXT_CORE, "Unknow rgltr %s", soc_info->rgltr_name[i]);
	// 	}

	// }


	// kfree(power_setting);
	return count;
}

static OPLUS_ATTR(extension_debug_power, 0644, extension_debug_power_show, extension_debug_power_store);

static struct attribute *camera_extension_common_attrs[] = {
	&oplus_attr_enable_dump.attr,
	&oplus_attr_extension_dump_state.attr,
	&oplus_attr_extension_dump_clk.attr,
	&oplus_attr_extension_dump_rgltr.attr,
	&oplus_attr_extension_dump_monitor.attr,
	&oplus_attr_extension_debug_power.attr,
	NULL,
};


static const struct attribute_group camera_extension_common_group = {
	.attrs = camera_extension_common_attrs,
};


static const struct attribute_group *camera_extension_groups[] = {
	&camera_extension_common_group,
	NULL,
};

int create_sysfs(void)
{
	int ret = 0;

	cam_extension_kobj = kobject_create_and_add("camera_extension_control", kernel_kobj);

	ret = sysfs_create_groups(cam_extension_kobj, camera_extension_groups);
	if (ret) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "failed to create sysfs for state monitor");
		sysfs_remove_groups(cam_extension_kobj, camera_extension_groups);
	}

	return ret;
}

const char* camera_extension_feature_type_to_string(
	enum camera_extension_feature_type type)
{
	switch (type) {
		case CAM_EXTENSION_FEATURE_TYPE_INVALID:
			return "INVALID";
		case CAM_ACTUATOR_NORMAL_POWER_UP_TYPE:
			return "A_PU_NM";
		case CAM_ACTUATOR_SDS_POWER_UP_TYPE:
			return "A_PU_SDS";
		case CAM_ACTUATOR_UPDATE_PID_POWER_UP_TYPE:
			return "A_PU_FW";
		case CAM_ACTUATOR_DELAY_POWER_DOWN_TYPE:
			return "A_PD_DL";
		case CAM_SENSOR_ADVANCE_POWER_UP_TYPE:
			return "S_PU_AD";
		case CAM_SENSOR_NORMAL_POWER_UP_TYPE:
			return "S_PU_NM";
		case CAM_OIS_NORMAL_POWER_UP_TYPE:
			return "O_PU_NM";
		case CAM_OIS_UPDATE_FW_POWER_UP_TYPE:
			return "O_PU_FW";
		case CAM_OIS_DELAY_POWER_DOWN_TYPE:
			return "O_PD_DL";
		case CAM_OIS_PUSH_CENTER_POWER_UP_TYPE:
			return "O_PU_CT";
		case CAM_TOF_NORMAL_POWER_UP_TYPE:
			return "T_UP_NM";
		case CAM_DEV_EXCEPTION_POWER_DOWN_TYPE:
			return "PD_EXP";
		default:
			return "Unknown camera extension feature type";
	}
}

ssize_t monitor_format_oneline(
	struct cam_state_queue_info *state_queue_info,
	int index,
	uint32_t num_entries,
	char * buf)
{
	char format_str[EXTENSION_LINE_MAX_LENGTH];
	char str_power_up[72];
	char str_power_dn[72];
	struct tm ts;
	int len = 0, i = 0, len_power_info = 0;

	time64_to_tm(state_queue_info->state_monitor[index].timestamp.tv_sec, 0, &ts);

	switch (state_queue_info->state_monitor[index].type) {
	case CAM_OPERATION_TYPE_REGULATOR:
		len = scnprintf(format_str, sizeof(format_str),
			" [%d/%d] regulator:%s use_count:%d at %d-%d %d:%d:%d.%ld\n",
			index, num_entries,
			state_queue_info->state_monitor[index].regulator.name,
			state_queue_info->state_monitor[index].regulator.use_count,
			ts.tm_mon + 1, ts.tm_mday, ts.tm_hour,
			ts.tm_min, ts.tm_sec,
			state_queue_info->state_monitor[index].timestamp.tv_nsec / 1000);
		break;
	case CAM_OPERATION_TYPE_CLOCK:
		len = scnprintf(format_str, sizeof(format_str),
			" [%d/%d] clock:%s use_count:%d at %d-%d %d:%d:%d.%ld\n",
			index, num_entries,
			state_queue_info->state_monitor[index].clock.name,
			state_queue_info->state_monitor[index].clock.use_count,
			ts.tm_mon + 1, ts.tm_mday, ts.tm_hour,
			ts.tm_min, ts.tm_sec,
			state_queue_info->state_monitor[index].timestamp.tv_nsec / 1000);
		break;
	case CAM_OPERATION_TYPE_STATE:
		len_power_info = 0;
		for (i = 0; i < (SENSOR_SEQ_TYPE_MAX - 1); i++) {
			if (state_queue_info->state_monitor[index].state.seq_type[i] == -1
			 && state_queue_info->state_monitor[index].state.seq_val[i] == -1)
				break;

			len_power_info += scnprintf(str_power_up + len_power_info, sizeof(str_power_up) -len_power_info, "%d-%d ",
				state_queue_info->state_monitor[index].state.seq_type[i],
				state_queue_info->state_monitor[index].state.seq_val[i]);
		}
		len_power_info = 0;
		for (i = SENSOR_SEQ_TYPE_MAX - 1; i < (SENSOR_SEQ_TYPE_MAX - 1) * 2; i++) {
			if (state_queue_info->state_monitor[index].state.seq_type[i] == -1
			 && state_queue_info->state_monitor[index].state.seq_val[i] == -1)
				break;

			len_power_info += scnprintf(str_power_dn + len_power_info, sizeof(str_power_dn) -len_power_info, "%d-%d ",
				state_queue_info->state_monitor[index].state.seq_type[i],
				state_queue_info->state_monitor[index].state.seq_val[i]);
		}
		len = scnprintf(format_str, sizeof(format_str),
				" %d/%d|%p %s %s uc:%d pu[%s] pd[%s] %d-%d %d:%d:%d.%ld\n",
				index, num_entries,
				state_queue_info->state_monitor[index].state.dev_ptr,
				state_queue_info->state_monitor[index].state.dev_uid,
				camera_extension_feature_type_to_string(state_queue_info->state_monitor[index].state.type),
				state_queue_info->state_monitor[index].state.use_count,
				str_power_up,
				str_power_dn,
				ts.tm_mon + 1, ts.tm_mday, ts.tm_hour,
				ts.tm_min, ts.tm_sec,
				state_queue_info->state_monitor[index].timestamp.tv_nsec / 1000);

		break;
	default:
		len = scnprintf(format_str, sizeof(format_str),
			" get invalid type:%d\n",
			state_queue_info->state_monitor[index].type);
		break;
	}

	if (buf) {
		return scnprintf(buf, sizeof(format_str), format_str);
	} else {
		CAM_EXT_INFO(CAM_EXT_UTIL, "%s", format_str);
		return 0;
	}
}

ssize_t extension_dump_item(
	enum cam_operation_type item,
	char* buf,
	int32_t bufsize)
{
	int32_t len = 0, i = 0;
	struct tm ts;
	struct timespec64 timespec;
	struct debug_state     *dstate;
	struct debug_clock     *dclk;
	struct debug_regulator *dreg;
	int64_t state_head     = 0;
	int32_t num_entries    = 0;
	int32_t oldest_entry   = 0;
	struct cam_state_queue_info *state_queue_info = NULL;
	struct monitor_check	check_result;

	switch (item) {
	case CAM_OPERATION_TYPE_SUMMARY:
		ktime_get_clocktai_ts64(&timespec);
		time64_to_tm(timespec.tv_sec, 0, &ts);

		check_power_exception(&check_result);
		len += scnprintf(buf + len, bufsize - len,
			"== Camera Extension Dump %d-%d %d:%d:%d.%ld ==\n"
			"CheckResult: feature_inuse %d clk_inuse %d rgltr_inuse %d rgltr_enabled %d\n",
			ts.tm_mon + 1,
			ts.tm_mday,
			ts.tm_hour,
			ts.tm_min,
			ts.tm_sec,
			timespec.tv_nsec / 1000,
			check_result.camera_feature_inuse_mask,
			check_result.count_inuse_clock,
			check_result.count_inuse_regulator,
			check_result.count_enabled_regulator);

		break;
	case CAM_OPERATION_TYPE_STATE:
		len += scnprintf(buf + len, bufsize - len, "Dump State\n");
		mutex_lock(&g_cam_monitor->state_list_lock);
		list_for_each_entry(dstate, &g_cam_monitor->debug_state_list, list) {
			len += scnprintf(buf + len, bufsize - len,
				"  dev_uid:%s, type:%s, use_count:%d\n",
				dstate->dev_uid,
				camera_extension_feature_type_to_string(dstate->type),
				dstate->use_count);
		}
		mutex_unlock(&g_cam_monitor->state_list_lock);

		break;
	case CAM_OPERATION_TYPE_CLOCK:
		len += scnprintf(buf + len, bufsize - len, "Dump Clock\n");
		mutex_lock(&g_cam_monitor->clk_list_lock);
		list_for_each_entry(dclk, &g_cam_monitor->debug_clk_list, list) {
			len += scnprintf(buf + len, bufsize - len,
				"  clk:%s use_count:%d\n",
				dclk->name, dclk->use_count);
		}
		mutex_unlock(&g_cam_monitor->clk_list_lock);
		break;
	case CAM_OPERATION_TYPE_REGULATOR:
		len += scnprintf(buf + len, bufsize - len, "Dump Regulator\n");
		mutex_lock(&g_cam_monitor->reg_list_lock);
		list_for_each_entry(dreg, &g_cam_monitor->debug_reg_list, list) {
			len += scnprintf(buf + len, bufsize - len,
				"  regulator:%s use_count:%d\n",
				dreg->name, dreg->use_count);
		}
		mutex_unlock(&g_cam_monitor->reg_list_lock);
		break;
	case CAM_OPERATION_TYPE_MONITOR:
		state_queue_info = &g_cam_monitor->state_queue;
		if (!state_queue_info->state_monitor)
		{
			if (bufsize > len)
				len += scnprintf(buf + len, bufsize - len, "Monitor is null");
			return len;
		}
		state_head = atomic64_read(&state_queue_info->state_monitor_head);
		if (state_head == -1)
		{
			if (bufsize > len)
				len += scnprintf(buf + len, bufsize - len, "Invalid monitor");
			return len;
		}
		if (bufsize > len)
			len += scnprintf(buf + len, bufsize - len, "Dump Monitor\n");

		if (state_head < CAM_STATE_MONITOR_MAX_ENTRIES)
		{
			num_entries = state_head + 1;
			oldest_entry = 0;
		}
		else
		{
			num_entries = CAM_STATE_MONITOR_MAX_ENTRIES;
			div_u64_rem(state_head + 1,
				CAM_STATE_MONITOR_MAX_ENTRIES, &oldest_entry);

			for (i = oldest_entry - 1 ; i >= 0; i--)
			{
				if (bufsize - len > EXTENSION_LINE_MAX_LENGTH)
				len += monitor_format_oneline(state_queue_info,
							i,
							num_entries,
							buf + len);
			}
		}

		for (i = num_entries - 1; i >= oldest_entry; i--)
		{
			if (bufsize - len > EXTENSION_LINE_MAX_LENGTH)
			len += monitor_format_oneline(state_queue_info,
							i,
							num_entries,
							buf + len);
		}
		break;
	default:
		break;

	}

	return len;
}

void update_state_monitor_array(
	enum cam_operation_type type,
	struct debug_state* state)
{
	int iterator;
	struct cam_state_queue_info *state_queue_info = NULL;

	state_queue_info = &g_cam_monitor->state_queue;

	if (!state_queue_info->state_monitor) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "state_monitor is NULL");
		return;
	}

	CAM_INC_HEAD(&state_queue_info->state_monitor_head,
		CAM_STATE_MONITOR_MAX_ENTRIES, &iterator);

	state_queue_info->state_monitor[iterator].type		= type;
	state_queue_info->state_monitor[iterator].state.type	= state->type;
	state_queue_info->state_monitor[iterator].state.dev_ptr = state->dev_ptr;
	strcpy(state_queue_info->state_monitor[iterator].state.dev_uid,
		state->dev_uid);
	memcpy(state_queue_info->state_monitor[iterator].state.seq_type,
		state->seq_type, sizeof(state->seq_type));
	memcpy(state_queue_info->state_monitor[iterator].state.seq_val,
		state->seq_val, sizeof(state->seq_val));
	state_queue_info->state_monitor[iterator].state.use_count = state->use_count;

	ktime_get_clocktai_ts64(&state_queue_info->state_monitor[iterator].timestamp);
}

void update_regulator_monitor_array(
	enum cam_operation_type type, struct debug_regulator* regulator)
{
	int iterator;
	struct cam_state_queue_info *state_queue_info = NULL;


	state_queue_info = &g_cam_monitor->state_queue;

	if (!state_queue_info->state_monitor)
	{
		CAM_EXT_ERR(CAM_EXT_UTIL, "state_monitor is NULL");
		return;
	}

	CAM_INC_HEAD(&state_queue_info->state_monitor_head,
		CAM_STATE_MONITOR_MAX_ENTRIES, &iterator);

	state_queue_info->state_monitor[iterator].type = type;
	strcpy(state_queue_info->state_monitor[iterator].regulator.name, regulator->name);
	state_queue_info->state_monitor[iterator].regulator.use_count = regulator->use_count;
	ktime_get_clocktai_ts64(&state_queue_info->state_monitor[iterator].timestamp);
}

void update_clock_monitor_array(
	enum cam_operation_type type, struct debug_clock* clock)
{
	int iterator;
	struct cam_state_queue_info *state_queue_info = NULL;


	state_queue_info = &g_cam_monitor->state_queue;

	if (!state_queue_info->state_monitor)
	{
		CAM_EXT_ERR(CAM_EXT_UTIL, "state_monitor is NULL");
		return;
	}

	CAM_INC_HEAD(&state_queue_info->state_monitor_head,
		CAM_STATE_MONITOR_MAX_ENTRIES, &iterator);

	state_queue_info->state_monitor[iterator].type = type;
	strcpy(state_queue_info->state_monitor[iterator].clock.name, clock->name);
	state_queue_info->state_monitor[iterator].clock.use_count = clock->use_count;
	ktime_get_clocktai_ts64(&state_queue_info->state_monitor[iterator].timestamp);
}

void extension_dump_monitor_print(void)
{
	int64_t state_head 	= 0;
	int32_t num_entries 	= 0;
	int32_t oldest_entry 	= 0;
	int32_t i = 0;
	struct tm ts;
	struct timespec64 timespec;
	struct cam_state_queue_info *state_queue_info = NULL;


	state_queue_info = &g_cam_monitor->state_queue;

	if (!state_queue_info->state_monitor) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "state monitor is null");
		return;
	}

	state_head = atomic64_read(&state_queue_info->state_monitor_head);

	ktime_get_clocktai_ts64(&timespec);
	time64_to_tm(timespec.tv_sec, 0, &ts);
	CAM_EXT_INFO(CAM_EXT_UTIL, "Dumping state information for preceding CCI cmd time: %d-%d %d:%d:%d.%lld",
		ts.tm_mon + 1, ts.tm_mday, ts.tm_hour,
		ts.tm_min, ts.tm_sec, timespec.tv_nsec / 1000);

	if (state_head == -1) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "has no valid state monitor");
		return;
	} else if (state_head < CAM_STATE_MONITOR_MAX_ENTRIES) {
		num_entries = state_head + 1;
		oldest_entry = 0;
	} else {
		num_entries = CAM_STATE_MONITOR_MAX_ENTRIES;
		div_u64_rem(state_head + 1,
			CAM_STATE_MONITOR_MAX_ENTRIES, &oldest_entry);
		for (i = oldest_entry - 1 ; i >= 0; i--) {
			monitor_format_oneline(state_queue_info,
						i,
						num_entries,
						NULL);
		}
	}

	for (i = num_entries - 1; i >= oldest_entry; i--) {
		monitor_format_oneline(state_queue_info,
					i,
					num_entries,
					NULL);
	}
	CAM_EXT_INFO(CAM_EXT_UTIL, "Dumping state information end.");
}

void check_power_exception(struct monitor_check *r)
{
	struct debug_state *dstate;
	struct debug_clock *dclk;
	struct debug_regulator *dreg;
	struct cam_subdev  *csd;
	struct cam_sensor_ctrl_t *s_ctrl;
	int i = 0;

	if (NULL == r)
		return;

	r->camera_feature_inuse_mask 	= 0;
	r->count_inuse_regulator 	= 0;
	r->count_inuse_clock		= 0;
	r->count_enabled_regulator	= 0;
	r->check_pass 			= true;

	mutex_lock(&g_cam_monitor->state_list_lock);
	list_for_each_entry(dstate, &g_cam_monitor->debug_state_list, list) {
		if (dstate->use_count) {
			r->camera_feature_inuse_mask |= (1 << dstate->type);
			CAM_EXT_INFO(CAM_EXT_UTIL, "Inuse mask:0x%x dev_uid:%s, type:%d, use_count:%d",
					r->camera_feature_inuse_mask,
					dstate->dev_uid,
					dstate->type,
					dstate->use_count);
		}
	}
	mutex_unlock(&g_cam_monitor->state_list_lock);

	mutex_lock(&g_cam_monitor->clk_list_lock);
	list_for_each_entry(dclk, &g_cam_monitor->debug_clk_list, list) {
		if(dclk->use_count) {
			r->count_inuse_clock ++;
			CAM_EXT_INFO(CAM_EXT_UTIL, "Exception clk:%s use_count:%d",
				dclk->name, dclk->use_count);
		}
	}
	mutex_unlock(&g_cam_monitor->clk_list_lock);

	mutex_lock(&g_cam_monitor->reg_list_lock);
	list_for_each_entry(dreg, &g_cam_monitor->debug_reg_list, list) {
		if(dreg->use_count) {
			r->count_inuse_regulator ++;
			CAM_EXT_INFO(CAM_EXT_UTIL, "Exception regulator:%s use_count:%d",
				dreg->name, dreg->use_count);
		}
	}
	mutex_unlock(&g_cam_monitor->reg_list_lock);

	list_for_each_entry(csd, &cam_req_mgr_ordered_sd_list, list) {
		if (csd->ent_function == CAM_SENSOR_DEVICE_TYPE) {
			s_ctrl = v4l2_get_subdevdata(&csd->sd);

			for (i = 0; i < s_ctrl->soc_info.num_rgltr; i++) {
				if (regulator_is_enabled(s_ctrl->soc_info.rgltr[i]) &&
					(!strstr(s_ctrl->soc_info.rgltr[i]->rdev->desc->name,
						"regulator-dummy") &&
					 !strstr(s_ctrl->soc_info.rgltr[i]->rdev->desc->name,
						"cam_cc_titan_top_gdsc"))) {
					r->count_enabled_regulator ++;
					CAM_EXT_ERR(CAM_EXT_UTIL,
						"Enabled regulator name:%s_%s: %s use_cont %d",
						s_ctrl->sensor_name,
						s_ctrl->soc_info.rgltr_name[i],
						s_ctrl->soc_info.rgltr[i]->rdev->desc->name,
						s_ctrl->soc_info.rgltr[i]->rdev->use_count);
				}
			}
		}
	}

	if (0 == r->camera_feature_inuse_mask 	&&
	    // (r->count_inuse_clock > 0		||
	    (r->count_inuse_regulator > 0	||
	    r->count_enabled_regulator > 0)) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "Check fail: count enabled regulator %d",
				r->count_enabled_regulator);
		r->check_pass = false;
		extension_dump_monitor_print();
	}
}

int set_camera_state(struct debug_state state, bool enable)
{
	struct debug_state *dstate;

	if(enable) {
		mutex_lock(&g_cam_monitor->state_list_lock);
		list_for_each_entry(dstate, &g_cam_monitor->debug_state_list, list) {
			if (!strcmp(dstate->dev_uid, state.dev_uid)) {
				dstate->use_count++;
				dstate->type    = state.type;
				dstate->dev_ptr = state.dev_ptr;
				memcpy(dstate->seq_type, state.seq_type, sizeof(dstate->seq_type));
				memcpy(dstate->seq_val, state.seq_val, sizeof(dstate->seq_val));
				state.use_count = dstate->use_count;
				CAM_EXT_INFO(CAM_EXT_UTIL, "Inc use_count %s type:%d, use_count:%d",
					state.dev_uid,
					state.type,
					dstate->use_count);
				mutex_unlock(&g_cam_monitor->state_list_lock);
				goto update_state_monitor;
			}
		}

		dstate = kzalloc(sizeof(*dstate), GFP_KERNEL);
		if (!dstate) {
			mutex_unlock(&g_cam_monitor->state_list_lock);
			CAM_EXT_INFO(CAM_EXT_UTIL, "add %s to debug_state_list fail! type:%d",
					state.dev_uid,
					state.type);
			return 0;
		}

		strcpy(dstate->dev_uid , state.dev_uid);
		dstate->type    = state.type;
		dstate->dev_ptr = state.dev_ptr;
		memcpy(dstate->seq_type, state.seq_type, sizeof(dstate->seq_type));
		memcpy(dstate->seq_val, state.seq_val, sizeof(dstate->seq_val));
		dstate->use_count = 1;
		state.use_count = dstate->use_count;

		list_add(&dstate->list, &g_cam_monitor->debug_state_list);
		mutex_unlock(&g_cam_monitor->state_list_lock);

		CAM_EXT_INFO(CAM_EXT_UTIL, "add %s to debug_state_list, type:%d, use_count:%d",
			state.dev_uid,
			state.type,
			dstate->use_count);

	} else {
		mutex_lock(&g_cam_monitor->state_list_lock);
		list_for_each_entry(dstate, &g_cam_monitor->debug_state_list, list) {
			if (!strcmp(dstate->dev_uid, state.dev_uid)) {
				dstate->use_count--;
				dstate->type    = state.type;
				dstate->dev_ptr = state.dev_ptr;
				memcpy(dstate->seq_type, state.seq_type, sizeof(dstate->seq_type));
				memcpy(dstate->seq_val, state.seq_val, sizeof(dstate->seq_val));
				state.use_count = dstate->use_count;
				CAM_EXT_INFO(CAM_EXT_UTIL, "Dec use_count %s type:%d, use_count:%d",
					state.dev_uid,
					state.type,
					dstate->use_count);
			}
		}
		mutex_unlock(&g_cam_monitor->state_list_lock);
	}

update_state_monitor:
	update_state_monitor_array(CAM_OPERATION_TYPE_STATE, &state);

#ifdef DUMP_MONITOR_ARRAY
	if(true == g_is_enable_dump && ((dump_count++ % 10) == 1))
	{
		extension_dump_monitor_print();
	}
#endif

	return 0;

}

int set_camera_clk(struct clk *clk,bool enable)
{
	struct debug_clock *dclk;

	if(enable)
	{
		mutex_lock(&g_cam_monitor->clk_list_lock);
		list_for_each_entry(dclk, &g_cam_monitor->debug_clk_list, list)
		{
			if(!strcmp(__clk_get_name(clk),dclk->name)){
				dclk->use_count++;
				CAM_EXT_INFO(CAM_EXT_UTIL, "enable clk:%s use_count:%d", dclk->name, dclk->use_count);
				mutex_unlock(&g_cam_monitor->clk_list_lock);
				goto update_clock_monitor;
			}
		}

		dclk = kzalloc(sizeof(*dclk), GFP_KERNEL);
		if (!dclk) {
			mutex_unlock(&g_cam_monitor->clk_list_lock);
			return 0;
		}

		strcpy(dclk->name, __clk_get_name(clk));
		dclk->use_count ++;
		list_add(&dclk->list, &g_cam_monitor->debug_clk_list);
		CAM_EXT_INFO(CAM_EXT_UTIL, "first time enable clk:%s use_count:%d", dclk->name, dclk->use_count);
		mutex_unlock(&g_cam_monitor->clk_list_lock);

	}
	else
	{
		mutex_lock(&g_cam_monitor->clk_list_lock);
		list_for_each_entry(dclk, &g_cam_monitor->debug_clk_list, list)
		{
			if(!strcmp(__clk_get_name(clk),dclk->name))
			{
				dclk->use_count--;
				CAM_EXT_INFO(CAM_EXT_UTIL, "disable clk:%s use_count:%d", dclk->name, dclk->use_count);
			}
		}
		mutex_unlock(&g_cam_monitor->clk_list_lock);

	}

update_clock_monitor:
	//update_clock_monitor_array(CAM_OPERATION_TYPE_CLOCK, dclk);

	return 0;

}

static const char *rdev_name(struct regulator_dev *rdev)
{
	if (rdev->constraints && rdev->constraints->name)
		return rdev->constraints->name;
	else if (rdev->desc->name)
		return rdev->desc->name;
	else
		return "";
}

int set_camera_regulator(struct regulator *rgltr, bool enable)
{
	struct debug_regulator *dreg;

	if(enable)
	{
		mutex_lock(&g_cam_monitor->reg_list_lock);
		list_for_each_entry(dreg, &g_cam_monitor->debug_reg_list, list)
		{
			if(!strcmp(rdev_name(rgltr->rdev),dreg->name)){
				dreg->use_count++;
				CAM_EXT_INFO(CAM_EXT_UTIL, "enable:%s use_count:%d", dreg->name, dreg->use_count);
				mutex_unlock(&g_cam_monitor->reg_list_lock);
				goto update_regulator_monitor;

			}
		}

		dreg = kzalloc(sizeof(*dreg), GFP_KERNEL);
		if (!dreg) {
			mutex_unlock(&g_cam_monitor->reg_list_lock);
			return 0;
		}

		strcpy(dreg->name, rdev_name(rgltr->rdev));
		dreg->use_count ++;
		list_add(&dreg->list, &g_cam_monitor->debug_reg_list);
		CAM_EXT_INFO(CAM_EXT_UTIL, "first time enable:%s use_count:%d", dreg->name, dreg->use_count);
		mutex_unlock(&g_cam_monitor->reg_list_lock);

	}
	else
	{
		mutex_lock(&g_cam_monitor->reg_list_lock);
		list_for_each_entry(dreg, &g_cam_monitor->debug_reg_list, list)
		{
			if(!strcmp(rdev_name(rgltr->rdev),dreg->name))
			{
				dreg->use_count--;
				CAM_EXT_INFO(CAM_EXT_UTIL, "disable:%s use_count:%d", dreg->name, dreg->use_count);
			}
		}
		mutex_unlock(&g_cam_monitor->reg_list_lock);
	}

update_regulator_monitor:
	//update_regulator_monitor_array(CAM_OPERATION_TYPE_REGULATOR, dreg);

	return 0;
}

int32_t cam_extension_dump_process(
	struct extension_control *cmd)
{
	int32_t rc = 0, i = 0;
	ssize_t len = 0;
	char* dump_buf = NULL;
	struct cam_subdev  *csd;
	struct cam_sensor_ctrl_t *s_ctrl;

	dump_buf = vmalloc(cmd->size);
	if (!dump_buf) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "No memory %d", cmd->size);
		return -ENOMEM;
	}

	for (i = 0; i < CAM_OPERATION_TYPE_MAX; i++)
	{
		if (cmd->size > len)
		len += extension_dump_item(i, dump_buf + len, cmd->size - len);
	}

	len += scnprintf(dump_buf + len, cmd->size - len,
			"%-12s %-35s %-14s %-27s %-2s %-8s %-9s %-10s\n",
			"sensor_name", "dev_name", "rgltr_name", "rdev_name",
			"en", "vol_uV", "use_count", "open_count");

	list_for_each_entry(csd, &cam_req_mgr_ordered_sd_list, list) {
		if (csd->ent_function == CAM_SENSOR_DEVICE_TYPE) {
			s_ctrl = v4l2_get_subdevdata(&csd->sd);

			for (i = 0; i < s_ctrl->soc_info.num_rgltr; i++) {
				len += scnprintf(dump_buf + len, cmd->size - len,
					"%-12s %-35s %-14s %-27s %c  %-8d %-9d %d\n",
					s_ctrl->sensor_name,
					s_ctrl->soc_info.dev_name,
					s_ctrl->soc_info.rgltr_name[i],
					s_ctrl->soc_info.rgltr[i]->rdev->desc->name,
					(regulator_is_enabled(s_ctrl->soc_info.rgltr[i])? 'Y' : 'N'),
					regulator_get_voltage(s_ctrl->soc_info.rgltr[i]),
					s_ctrl->soc_info.rgltr[i]->rdev->use_count,
					s_ctrl->soc_info.rgltr[i]->rdev->open_count);
			}
			len += scnprintf(dump_buf + len, cmd->size - len, "\n");
		}
	}

	if (cmd->size > len)
	len += scnprintf(dump_buf + len, cmd->size - len,
			"== Camera Extension Dump End ==");

	if (copy_to_user(u64_to_user_ptr(cmd->handle),
		dump_buf,
		len)) {
		CAM_EXT_ERR(CAM_EXT_UTIL, "Failed Copy to User");
		rc =  -EFAULT;
		goto free_buf;
	}

	CAM_EXT_INFO(CAM_EXT_UTIL, "succ.");

free_buf:
	vfree(dump_buf);
	dump_buf = NULL;

	return rc;
}

int32_t cam_extension_check_process(
	struct extension_control *cmd)
{
	int32_t rc = 0;
	struct monitor_check check_result;

	if (cmd->size != sizeof(check_result)) {
		return -EFAULT;
	}

	check_power_exception(&check_result);
	if (copy_to_user(u64_to_user_ptr(cmd->handle),
		&check_result,
		sizeof(check_result))) {
			CAM_EXT_ERR(CAM_EXT_UTIL, "Failed Copy to User");
			rc =  -EFAULT;
	}

	return rc;
}

void oplus_cam_monitor_state(
	void *subdev_ctrl,
	u32 ent_function,
	enum camera_extension_feature_type type,
	bool enable)
{
	int i;
	struct debug_state state;
	struct cam_sensor_power_ctrl_t *power_info = NULL;
	struct cam_actuator_soc_private *a_soc_private;
	struct cam_ois_soc_private	*o_soc_private;
	struct cam_actuator_ctrl_t	*a_ctrl;
	struct cam_ois_ctrl_t		*o_ctrl;
	struct cam_sensor_ctrl_t	*s_ctrl;
	struct cam_tof_ctrl_t 		*tof_ctrl;

	switch (ent_function) {
	case CAM_ACTUATOR_DEVICE_TYPE: {
		a_ctrl = (struct cam_actuator_ctrl_t *)subdev_ctrl;
		if (!a_ctrl)
			return;
		strcpy(state.dev_uid, a_ctrl->soc_info.dev_name);
		state.dev_ptr = a_ctrl;
		a_soc_private =
			(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
		if (!a_soc_private) {
			CAM_EXT_ERR(CAM_EXT_UTIL, "a_soc_private is null!");
			// return;
		} else {
			power_info = &a_soc_private->power_info;
		}
	}
		break;
	case CAM_OIS_DEVICE_TYPE: {
		o_ctrl = (struct cam_ois_ctrl_t *)subdev_ctrl;
		if (!o_ctrl)
			return;
		strcpy(state.dev_uid, o_ctrl->soc_info.dev_name);
		state.dev_ptr = o_ctrl;
		o_soc_private =
			(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
		if (!o_soc_private) {
			CAM_EXT_ERR(CAM_EXT_UTIL, "o_soc_private is null!");
			// return;
		} else {
			power_info = &o_soc_private->power_info;
		}
	}
		break;
	case CAM_SENSOR_DEVICE_TYPE: {
		s_ctrl = (struct cam_sensor_ctrl_t *)subdev_ctrl;
		if (!s_ctrl)
			return;
		strcpy(state.dev_uid, s_ctrl->soc_info.dev_name);
		state.dev_ptr = s_ctrl;
		if (s_ctrl->sensordata) {
			power_info = &s_ctrl->sensordata->power_info;
		}
	}
		break;
	case CAM_CUSTOM_DEVICE_TYPE: {
		tof_ctrl = (struct cam_tof_ctrl_t *)subdev_ctrl;
		if (!tof_ctrl)
			return;
		strcpy(state.dev_uid, tof_ctrl->soc_info.dev_name);
		state.dev_ptr = tof_ctrl;
		if (tof_ctrl) {
			power_info = &tof_ctrl->power_info;
		}
	}
		break;
	default:
		return;
		break;
	}

	state.type = type;

	// power_up_info                          //power_down_info
	// [-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1]
	memset(state.seq_type, -1, sizeof(state.seq_type));
	memset(state.seq_val, -1, sizeof(state.seq_val));

	if (power_info != NULL &&
		power_info->power_setting_size < SENSOR_SEQ_TYPE_MAX &&
		power_info->power_setting != NULL) {
		for (i = 0; i < power_info->power_setting_size; i++) {

			state.seq_type[i] = power_info->power_setting[i].seq_type;
			state.seq_val[i]  = power_info->power_setting[i].seq_val;
		}
	} else {
		CAM_EXT_ERR(CAM_EXT_UTIL, "power_setting is null!");
	}

	if (power_info != NULL &&
		power_info->power_down_setting_size < SENSOR_SEQ_TYPE_MAX &&
		power_info->power_down_setting != NULL) {
		for (i = 0; i < power_info->power_down_setting_size; i++) {

			state.seq_type[i + (SENSOR_SEQ_TYPE_MAX - 1)] =
									power_info->power_down_setting[i].seq_type;
			state.seq_val[i + (SENSOR_SEQ_TYPE_MAX - 1)]  =
										power_info->power_down_setting[i].seq_val;
		}
	} else {
		CAM_EXT_ERR(CAM_EXT_UTIL, "power_down_setting is null!");
	}

	set_camera_state(state, enable);
}
