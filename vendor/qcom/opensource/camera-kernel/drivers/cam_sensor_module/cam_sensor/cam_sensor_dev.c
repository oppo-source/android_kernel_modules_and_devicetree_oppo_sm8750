// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "cam_sensor_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_sensor_soc.h"
#include "cam_sensor_core.h"
#include "camera_main.h"
#include "cam_compat.h"
#include "cam_mem_mgr_api.h"

static struct cam_sensor_i3c_sensor_data {
	struct cam_sensor_ctrl_t                  *s_ctrl;
	struct completion                          probe_complete;
} g_i3c_sensor_data[MAX_CAMERAS];

struct completion *cam_sensor_get_i3c_completion(uint32_t index)
{
	return &g_i3c_sensor_data[index].probe_complete;
}

static int cam_sensor_subdev_close_internal(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_sensor_ctrl_t *s_ctrl =
		v4l2_get_subdevdata(sd);

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "s_ctrl ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	cam_sensor_shutdown(s_ctrl);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));

	return 0;
}

static int cam_sensor_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	bool crm_active = cam_req_mgr_is_open();

	if (crm_active) {
		CAM_DBG(CAM_SENSOR, "CRM is ACTIVE, close should be from CRM");
		return 0;
	}

	return cam_sensor_subdev_close_internal(sd, fh);
}

static long cam_sensor_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl =
		v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_sensor_driver_cmd(s_ctrl, arg);
		if (rc) {
			if (rc == -EBADR)
				CAM_INFO(CAM_SENSOR,
					"Failed in Driver cmd: %d, it has been flushed", rc);
			else if (rc != -ENODEV)
				CAM_ERR(CAM_SENSOR,
					"Failed in Driver cmd: %d", rc);
		}
		break;
	case CAM_SD_SHUTDOWN:
		if (!cam_req_mgr_is_shutdown()) {
			CAM_ERR(CAM_CORE, "SD shouldn't come from user space");
			return 0;
		}

		rc = cam_sensor_subdev_close_internal(sd, NULL);
		break;
	default:
		CAM_ERR(CAM_SENSOR, "Invalid ioctl cmd: %d", cmd);
		rc = -ENOIOCTLCMD;
		break;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_sensor_init_subdev_do_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_SENSOR, "Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_sensor_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "cam_sensor_subdev_ioctl failed");
		break;
	default:
		CAM_ERR(CAM_SENSOR, "Invalid compat ioctl cmd_type: %d", cmd);
		rc = -ENOIOCTLCMD;
		break;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_SENSOR,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}

	return rc;
}

#endif
#ifdef OPLUS_FEATURE_CAMERA_COMMON
struct v4l2_subdev_core_ops cam_sensor_subdev_core_ops = {
#else
static struct v4l2_subdev_core_ops cam_sensor_subdev_core_ops = {
#endif
	.ioctl = cam_sensor_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_sensor_init_subdev_do_ioctl,
#endif
	.s_power = cam_sensor_power,
};
#ifdef OPLUS_FEATURE_CAMERA_COMMON
EXPORT_SYMBOL(cam_sensor_subdev_core_ops);
#endif

static struct v4l2_subdev_ops cam_sensor_subdev_ops = {
	.core = &cam_sensor_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cam_sensor_internal_ops = {
	.close = cam_sensor_subdev_close,
};

static int cam_sensor_init_subdev_params(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	s_ctrl->v4l2_dev_str.internal_ops = &cam_sensor_internal_ops;
	s_ctrl->v4l2_dev_str.ops = &cam_sensor_subdev_ops;
	strscpy(s_ctrl->device_name, CAMX_SENSOR_DEV_NAME, CAM_CTX_DEV_NAME_MAX_LENGTH);
	s_ctrl->v4l2_dev_str.name = s_ctrl->device_name;
	s_ctrl->v4l2_dev_str.sd_flags = (V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	s_ctrl->v4l2_dev_str.ent_function = CAM_SENSOR_DEVICE_TYPE;
	s_ctrl->v4l2_dev_str.token = s_ctrl;
	s_ctrl->v4l2_dev_str.close_seq_prior = CAM_SD_CLOSE_MEDIUM_LOW_PRIORITY;

	rc = cam_register_subdev(&(s_ctrl->v4l2_dev_str));
	if (rc)
		CAM_ERR(CAM_SENSOR, "Fail with cam_register_subdev rc: %d", rc);

	return rc;
}

static int cam_sensor_i3c_driver_probe(struct i3c_device *client)
{
	int32_t rc = 0;
	struct cam_sensor_ctrl_t       *s_ctrl = NULL;
	uint32_t                        index;
	struct device                  *dev;

	if (!client) {
		CAM_ERR(CAM_CSIPHY, "Invalid input args");
		return -EINVAL;
	}

	dev = &client->dev;

	CAM_DBG(CAM_SENSOR, "Probe for I3C Slave %s", dev_name(dev));

	rc = of_property_read_u32(dev->of_node, "cell-index", &index);
	if (rc) {
		CAM_ERR(CAM_UTIL, "device %s failed to read cell-index", dev_name(dev));
		return rc;
	}

	if (index >= MAX_CAMERAS) {
		CAM_ERR(CAM_SENSOR, "Invalid Cell-Index: %u for %s", index, dev_name(dev));
		return -EINVAL;
	}

	s_ctrl = g_i3c_sensor_data[index].s_ctrl;
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "S_ctrl is null. I3C Probe before platfom driver probe for %s",
			dev_name(dev));
		return -EINVAL;
	}

	dev->driver_data = s_ctrl;

	s_ctrl->io_master_info.qup_client = CAM_MEM_ZALLOC(sizeof(
		struct cam_sensor_qup_client), GFP_KERNEL);
	if (!(s_ctrl->io_master_info.qup_client)) {
		CAM_ERR(CAM_SENSOR, "Unable to allocate memory for QUP handle for %s",
			dev_name(dev));
		return -ENOMEM;
	}
	cam_sensor_utils_parse_pm_ctrl_flag(s_ctrl->of_node, &(s_ctrl->io_master_info));

	CAM_INFO(CAM_SENSOR,
		"master: %d (1-CCI, 2-I2C, 3-SPI, 4-I3C) pm_ctrl_client_enable: %d",
		s_ctrl->io_master_info.master_type,
		s_ctrl->io_master_info.qup_client->pm_ctrl_client_enable);

	s_ctrl->io_master_info.qup_client->i3c_client = client;
	s_ctrl->io_master_info.qup_client->i3c_wait_for_hotjoin = false;

	complete_all(&g_i3c_sensor_data[index].probe_complete);

	CAM_DBG(CAM_SENSOR, "I3C Probe Finished for %s", dev_name(dev));
	return rc;
}


#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
static void cam_i3c_driver_remove(struct i3c_device *client)
{
	int32_t                        rc = 0;
	uint32_t                       index;
	struct cam_sensor_ctrl_t       *s_ctrl = NULL;
	struct device                  *dev;

	if (!client) {
		CAM_ERR(CAM_SENSOR, "I3C Driver Remove: Invalid input args");
		return;
	}

	dev = &client->dev;

	CAM_DBG(CAM_SENSOR, "driver remove for I3C Slave %s", dev_name(dev));

	rc = of_property_read_u32(dev->of_node, "cell-index", &index);
	if (rc) {
		CAM_ERR(CAM_UTIL, "device %s failed to read cell-index", dev_name(dev));
		return;
	}

	if (index >= MAX_CAMERAS) {
		CAM_ERR(CAM_SENSOR, "Invalid Cell-Index: %u for %s", index, dev_name(dev));
		return;
	}

	s_ctrl = g_i3c_sensor_data[index].s_ctrl;
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "S_ctrl is null. I3C Probe before platfom driver probe for %s",
			dev_name(dev));
		return;
	}

	CAM_DBG(CAM_SENSOR, "I3C remove invoked for %s",
		(client ? dev_name(&client->dev) : "none"));
	CAM_MEM_FREE(s_ctrl->io_master_info.qup_client);
	s_ctrl->io_master_info.qup_client = NULL;
}
#else
static int cam_i3c_driver_remove(struct i3c_device *client)
{
	int32_t                        rc = 0;
	uint32_t                       index;
	struct cam_sensor_ctrl_t       *s_ctrl = NULL;
	struct device                  *dev;

	if (!client) {
		CAM_ERR(CAM_SENSOR, "I3C Driver Remove: Invalid input args");
		return -EINVAL;
	}

	dev = &client->dev;

	CAM_DBG(CAM_SENSOR, "driver remove for I3C Slave %s", dev_name(dev));

	rc = of_property_read_u32(dev->of_node, "cell-index", &index);
	if (rc) {
		CAM_ERR(CAM_UTIL, "device %s failed to read cell-index", dev_name(dev));
		return -EINVAL;
	}

	if (index >= MAX_CAMERAS) {
		CAM_ERR(CAM_SENSOR, "Invalid Cell-Index: %u for %s", index, dev_name(dev));
		return -EINVAL;
	}

	s_ctrl = g_i3c_sensor_data[index].s_ctrl;
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "S_ctrl is null. I3C Probe before platfom driver probe for %s",
			dev_name(dev));
		return -EINVAL;
	}

	CAM_DBG(CAM_SENSOR, "I3C remove invoked for %s",
		(client ? dev_name(&client->dev) : "none"));
	CAM_MEM_FREE(s_ctrl->io_master_info.qup_client);
	s_ctrl->io_master_info.qup_client = NULL;
	return 0;
}
#endif

static int cam_sensor_i2c_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int32_t                   rc = 0;
	int                       i = 0;
	struct i2c_client        *client = NULL;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	struct cam_hw_soc_info   *soc_info = NULL;
	struct timespec64         ts_start, ts_end;
	long                      microsec = 0;
	struct device_node       *np = NULL;
	const char               *drv_name;

	CAM_GET_TIMESTAMP(ts_start);
	client = container_of(dev, struct i2c_client, dev);
	if (client == NULL) {
		CAM_ERR(CAM_SENSOR, "Invalid Args client: %pK",
			client);
		return -EINVAL;
	}

	/* Create sensor control structure */
	s_ctrl = CAM_MEM_ZALLOC(sizeof(*s_ctrl), GFP_KERNEL);
	if (!s_ctrl)
		return -ENOMEM;

	s_ctrl->io_master_info.qup_client = CAM_MEM_ZALLOC(sizeof(
		struct cam_sensor_qup_client), GFP_KERNEL);
	if (!(s_ctrl->io_master_info.qup_client)) {
		rc = -ENOMEM;
		goto free_s_ctrl;
	}

	i2c_set_clientdata(client, s_ctrl);

	s_ctrl->io_master_info.qup_client->i2c_client = client;
	soc_info = &s_ctrl->soc_info;
	soc_info->dev = &client->dev;
	soc_info->dev_name = client->name;

	/* Initialize sensor device type */
	s_ctrl->of_node = client->dev.of_node;
	s_ctrl->io_master_info.master_type = I2C_MASTER;
	s_ctrl->is_probe_succeed = 0;
	s_ctrl->last_flush_req = 0;

	np = of_node_get(client->dev.of_node);
	drv_name = of_node_full_name(np);

	rc = cam_sensor_parse_dt(s_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "cam_sensor_parse_dt rc %d", rc);
		goto free_qup;
	}

	rc = cam_sensor_init_subdev_params(s_ctrl);
	if (rc)
		goto free_qup;

	s_ctrl->i2c_data.per_frame =
		CAM_MEM_ZALLOC(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (s_ctrl->i2c_data.per_frame == NULL) {
		rc = -ENOMEM;
		goto unreg_subdev;
	}

	s_ctrl->i2c_data.frame_skip =
		CAM_MEM_ZALLOC(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (s_ctrl->i2c_data.frame_skip == NULL) {
		rc = -ENOMEM;
		goto free_perframe;
	}

	s_ctrl->i2c_data.deferred_frame_update =
		CAM_MEM_ZALLOC(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (s_ctrl->i2c_data.deferred_frame_update == NULL) {
		rc = -ENOMEM;
		goto free_frame_skip;
	}

	s_ctrl->i2c_data.bubble_update =
		CAM_MEM_ZALLOC(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (s_ctrl->i2c_data.bubble_update == NULL) {
		rc = -ENOMEM;
		goto free_deferred_frame_update;
	}

	INIT_LIST_HEAD(&(s_ctrl->i2c_data.init_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.config_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.streamon_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.streamoff_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.reg_bank_unlock_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.reg_bank_lock_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.read_settings.list_head));
#ifdef OPLUS_FEATURE_CAMERA_COMMON
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.resolution_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.lsc_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.qsc_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.awbotp_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.spc_settings.list_head));
	mutex_init(&(s_ctrl->sensor_power_state_mutex));
	mutex_init(&(s_ctrl->sensor_initsetting_mutex));
	s_ctrl->sensor_power_state = CAM_SENSOR_POWER_OFF;
	s_ctrl->sensor_initsetting_state = CAM_SENSOR_SETTING_WRITE_INVALID;
	s_ctrl->sensor_qsc_setting.qscsetting_state = CAM_SENSOR_SETTING_WRITE_INVALID;
#endif

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.deferred_frame_update[i].list_head));
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.per_frame[i].list_head));
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.frame_skip[i].list_head));
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.bubble_update[i].list_head));
	}

	cam_sensor_module_add_i2c_device((void *) s_ctrl, CAM_SENSOR_DEVICE);

	s_ctrl->bridge_intf.device_hdl = -1;
	s_ctrl->bridge_intf.link_hdl = -1;
	s_ctrl->bridge_intf.ops.get_dev_info = cam_sensor_publish_dev_info;
	s_ctrl->bridge_intf.ops.link_setup = cam_sensor_establish_link;
	s_ctrl->bridge_intf.ops.apply_req = cam_sensor_apply_request;
	s_ctrl->bridge_intf.ops.notify_frame_skip =
		cam_sensor_notify_frame_skip;
	s_ctrl->bridge_intf.ops.flush_req = cam_sensor_flush_request;
	s_ctrl->bridge_intf.ops.process_evt = cam_sensor_process_evt;

	s_ctrl->sensordata->power_info.dev = soc_info->dev;
	CAM_GET_TIMESTAMP(ts_end);
	CAM_GET_TIMESTAMP_DIFF_IN_MICRO(ts_start, ts_end, microsec);
	cam_record_bind_latency(drv_name, microsec);
	of_node_put(np);

	return rc;

free_deferred_frame_update:
	CAM_MEM_FREE(s_ctrl->i2c_data.deferred_frame_update);
free_frame_skip:
	CAM_MEM_FREE(s_ctrl->i2c_data.frame_skip);
free_perframe:
	CAM_MEM_FREE(s_ctrl->i2c_data.per_frame);
unreg_subdev:
	cam_unregister_subdev(&(s_ctrl->v4l2_dev_str));
free_qup:
	CAM_MEM_FREE(s_ctrl->io_master_info.qup_client);
free_s_ctrl:
	CAM_MEM_FREE(s_ctrl);
	return rc;
}

static void cam_sensor_i2c_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct i2c_client         *client = NULL;
	struct cam_sensor_ctrl_t  *s_ctrl = NULL;

	client = container_of(dev, struct i2c_client, dev);
	if (!client) {
		CAM_ERR(CAM_SENSOR,
			"Failed to get i2c client");
		return;
	}

	s_ctrl = i2c_get_clientdata(client);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "sensor device is NULL");
		return;
	}

	CAM_DBG(CAM_SENSOR, "i2c remove invoked");
	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	cam_sensor_shutdown(s_ctrl);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	cam_unregister_subdev(&(s_ctrl->v4l2_dev_str));

	CAM_MEM_FREE(s_ctrl->i2c_data.deferred_frame_update);
	CAM_MEM_FREE(s_ctrl->i2c_data.per_frame);
	CAM_MEM_FREE(s_ctrl->i2c_data.frame_skip);
	CAM_MEM_FREE(s_ctrl->i2c_data.bubble_update);
	v4l2_set_subdevdata(&(s_ctrl->v4l2_dev_str.sd), NULL);
	CAM_MEM_FREE(s_ctrl->io_master_info.qup_client);
	CAM_MEM_FREE(s_ctrl);
}

const static struct component_ops cam_sensor_i2c_component_ops = {
	.bind = cam_sensor_i2c_component_bind,
	.unbind = cam_sensor_i2c_component_unbind,
};

#if KERNEL_VERSION(6, 2, 0) <= LINUX_VERSION_CODE
static int cam_sensor_i2c_driver_probe(struct i2c_client *client)
{
	int rc = 0;

	if (client == NULL) {
		CAM_ERR(CAM_SENSOR, "Invalid Args client: %pK",
			client);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CAM_ERR(CAM_SENSOR, "%s :: i2c_check_functionality failed",
			client->name);
		return -EFAULT;
	}

	CAM_DBG(CAM_SENSOR, "Adding sensor component");
	rc = component_add(&client->dev, &cam_sensor_i2c_component_ops);
	if (rc)
		CAM_ERR(CAM_SENSOR, "failed to add component rc: %d", rc);

	return rc;
}
#else
static int cam_sensor_i2c_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;

	if (client == NULL || id == NULL) {
		CAM_ERR(CAM_SENSOR, "Invalid Args client: %pK id: %pK",
			client, id);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CAM_ERR(CAM_SENSOR, "%s :: i2c_check_functionality failed",
			client->name);
		return -EFAULT;
	}

	CAM_DBG(CAM_SENSOR, "Adding sensor component");
	rc = component_add(&client->dev, &cam_sensor_i2c_component_ops);
	if (rc)
		CAM_ERR(CAM_SENSOR, "failed to add component rc: %d", rc);

	return rc;
}
#endif

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
void cam_sensor_i2c_driver_remove(struct i2c_client *client)
{
	component_del(&client->dev, &cam_sensor_i2c_component_ops);
}

#else
static int cam_sensor_i2c_driver_remove(struct i2c_client *client)
{
	component_del(&client->dev, &cam_sensor_i2c_component_ops);

	return 0;
}
#endif

static int cam_sensor_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int32_t rc = 0, i = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	bool i3c_i2c_target;
	struct platform_device *pdev = to_platform_device(dev);
	struct timespec64 ts_start, ts_end;
	long microsec = 0;

	CAM_GET_TIMESTAMP(ts_start);
	i3c_i2c_target = of_property_read_bool(pdev->dev.of_node, "i3c-i2c-target");
	if (i3c_i2c_target)
		return 0;

	/* Create sensor control structure */
	s_ctrl = devm_kzalloc(&pdev->dev,
		sizeof(struct cam_sensor_ctrl_t), GFP_KERNEL);
	if (!s_ctrl)
		return -ENOMEM;

	soc_info = &s_ctrl->soc_info;
	soc_info->pdev = pdev;
	soc_info->dev = &pdev->dev;
	soc_info->dev_name = pdev->name;

	/* Initialize sensor device type */
	s_ctrl->of_node = pdev->dev.of_node;
	s_ctrl->is_probe_succeed = 0;
	s_ctrl->last_flush_req = 0;

	/*fill in platform device*/
	s_ctrl->pdev = pdev;

	s_ctrl->io_master_info.master_type = CCI_MASTER;

	rc = cam_sensor_parse_dt(s_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "failed: cam_sensor_parse_dt rc %d", rc);
		goto free_s_ctrl;
	}

	CAM_DBG(CAM_SENSOR, "Master Type: %u", s_ctrl->io_master_info.master_type);

	/* Fill platform device id*/
	pdev->id = soc_info->index;

	rc = cam_sensor_init_subdev_params(s_ctrl);
	if (rc)
		goto free_s_ctrl;

	s_ctrl->i2c_data.per_frame =
		CAM_MEM_ZALLOC(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (s_ctrl->i2c_data.per_frame == NULL) {
		rc = -ENOMEM;
		goto unreg_subdev;
	}

	s_ctrl->i2c_data.frame_skip =
		CAM_MEM_ZALLOC(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (s_ctrl->i2c_data.frame_skip == NULL) {
		rc = -ENOMEM;
		goto free_perframe;
	}

	s_ctrl->i2c_data.deferred_frame_update =
		CAM_MEM_ZALLOC(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (s_ctrl->i2c_data.deferred_frame_update == NULL) {
		rc = -ENOMEM;
		goto free_frame_skip;
	}

	s_ctrl->i2c_data.bubble_update =
		CAM_MEM_ZALLOC(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (s_ctrl->i2c_data.bubble_update == NULL) {
		rc = -ENOMEM;
		goto free_deferred_frame_update;
	}

	INIT_LIST_HEAD(&(s_ctrl->i2c_data.init_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.config_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.streamon_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.streamoff_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.reg_bank_unlock_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.reg_bank_lock_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.read_settings.list_head));

#ifdef OPLUS_FEATURE_CAMERA_COMMON
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.resolution_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.lsc_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.qsc_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.awbotp_settings.list_head));
	INIT_LIST_HEAD(&(s_ctrl->i2c_data.spc_settings.list_head));
	mutex_init(&(s_ctrl->sensor_power_state_mutex));
	mutex_init(&(s_ctrl->sensor_initsetting_mutex));
	s_ctrl->sensor_power_state = CAM_SENSOR_POWER_OFF;
	s_ctrl->sensor_initsetting_state = CAM_SENSOR_SETTING_WRITE_INVALID;
#endif

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.deferred_frame_update[i].list_head));
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.per_frame[i].list_head));
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.frame_skip[i].list_head));
		INIT_LIST_HEAD(&(s_ctrl->i2c_data.bubble_update[i].list_head));
	}

	cam_sensor_module_add_i2c_device((void *) s_ctrl, CAM_SENSOR_DEVICE);

	s_ctrl->bridge_intf.device_hdl = -1;
	s_ctrl->bridge_intf.link_hdl = -1;
	s_ctrl->bridge_intf.ops.get_dev_info = cam_sensor_publish_dev_info;
	s_ctrl->bridge_intf.ops.link_setup = cam_sensor_establish_link;
	s_ctrl->bridge_intf.ops.apply_req = cam_sensor_apply_request;
	s_ctrl->bridge_intf.ops.notify_frame_skip =
		cam_sensor_notify_frame_skip;
	s_ctrl->bridge_intf.ops.flush_req = cam_sensor_flush_request;
	s_ctrl->bridge_intf.ops.process_evt = cam_sensor_process_evt;

	s_ctrl->sensordata->power_info.dev = &pdev->dev;
	platform_set_drvdata(pdev, s_ctrl);
	s_ctrl->sensor_state = CAM_SENSOR_INIT;
	CAM_DBG(CAM_SENSOR, "Component bound successfully for %s", pdev->name);

	g_i3c_sensor_data[soc_info->index].s_ctrl = s_ctrl;
	init_completion(&g_i3c_sensor_data[soc_info->index].probe_complete);
	CAM_GET_TIMESTAMP(ts_end);
	CAM_GET_TIMESTAMP_DIFF_IN_MICRO(ts_start, ts_end, microsec);
	cam_record_bind_latency(pdev->name, microsec);

	return rc;

free_deferred_frame_update:
	CAM_MEM_FREE(s_ctrl->i2c_data.deferred_frame_update);
free_frame_skip:
	CAM_MEM_FREE(s_ctrl->i2c_data.frame_skip);
free_perframe:
	CAM_MEM_FREE(s_ctrl->i2c_data.per_frame);
unreg_subdev:
	cam_unregister_subdev(&(s_ctrl->v4l2_dev_str));
free_s_ctrl:
	devm_kfree(&pdev->dev, s_ctrl);
	return rc;
}

static void cam_sensor_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int                        i;
	struct cam_sensor_ctrl_t  *s_ctrl;
	struct cam_hw_soc_info    *soc_info;
	bool                       i3c_i2c_target;
	struct platform_device *pdev = to_platform_device(dev);

	i3c_i2c_target = of_property_read_bool(pdev->dev.of_node, "i3c-i2c-target");
	if (i3c_i2c_target)
		return;

	s_ctrl = platform_get_drvdata(pdev);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "sensor device is NULL");
		return;
	}

	CAM_DBG(CAM_SENSOR, "Component unbind called for: %s", pdev->name);
	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	cam_sensor_shutdown(s_ctrl);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	cam_unregister_subdev(&(s_ctrl->v4l2_dev_str));
	soc_info = &s_ctrl->soc_info;
	for (i = 0; i < soc_info->num_clk; i++) {
		if (!soc_info->clk[i]) {
			CAM_DBG(CAM_SENSOR, "%s handle is NULL skip put",
				soc_info->clk_name[i]);
			continue;
		}
		devm_clk_put(soc_info->dev, soc_info->clk[i]);
	}

	CAM_MEM_FREE(s_ctrl->i2c_data.deferred_frame_update);
	CAM_MEM_FREE(s_ctrl->i2c_data.per_frame);
	CAM_MEM_FREE(s_ctrl->i2c_data.frame_skip);
	CAM_MEM_FREE(s_ctrl->i2c_data.bubble_update);
	platform_set_drvdata(pdev, NULL);
	v4l2_set_subdevdata(&(s_ctrl->v4l2_dev_str.sd), NULL);
	devm_kfree(&pdev->dev, s_ctrl);
}

const static struct component_ops cam_sensor_component_ops = {
	.bind = cam_sensor_component_bind,
	.unbind = cam_sensor_component_unbind,
};

static int cam_sensor_platform_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_sensor_component_ops);
	return 0;
}

static const struct of_device_id cam_sensor_driver_dt_match[] = {
	{.compatible = "qcom,cam-sensor"},
	{}
};
MODULE_DEVICE_TABLE(of, cam_sensor_driver_dt_match);

static int32_t cam_sensor_driver_platform_probe(
	struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_SENSOR, "Adding Sensor component for %s", pdev->name);
	rc = component_add(&pdev->dev, &cam_sensor_component_ops);
	if (rc)
		CAM_ERR(CAM_SENSOR, "failed to add component rc: %d", rc);

	return rc;
}

struct platform_driver cam_sensor_platform_driver = {
	.probe = cam_sensor_driver_platform_probe,
	.driver = {
		.name = "qcom,camera",
		.owner = THIS_MODULE,
		.of_match_table = cam_sensor_driver_dt_match,
		.suppress_bind_attrs = true,
	},
	.remove = cam_sensor_platform_remove,
};

static const struct of_device_id cam_sensor_i2c_driver_dt_match[] = {
	{.compatible = "qcom,cam-i2c-sensor"},
	{}
};
MODULE_DEVICE_TABLE(of, cam_sensor_i2c_driver_dt_match);

static const struct i2c_device_id i2c_id[] = {
	{SENSOR_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

struct i2c_driver cam_sensor_i2c_driver = {
	.id_table = i2c_id,
	.probe = cam_sensor_i2c_driver_probe,
	.remove = cam_sensor_i2c_driver_remove,
	.driver = {
		.name = SENSOR_DRIVER_I2C,
		.owner = THIS_MODULE,
		.of_match_table = cam_sensor_i2c_driver_dt_match,
		.suppress_bind_attrs = true,
	},
};

static struct i3c_device_id sensor_i3c_id[MAX_I3C_DEVICE_ID_ENTRIES + 1];

static struct i3c_driver cam_sensor_i3c_driver = {
	.id_table = sensor_i3c_id,
	.probe = cam_sensor_i3c_driver_probe,
	.remove = cam_i3c_driver_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_DRIVER_I3C,
		.of_match_table = cam_sensor_driver_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_sensor_driver_init(void)
{
	int rc;
	struct device_node                      *dev;
	int num_entries = 0;

	rc = platform_driver_register(&cam_sensor_platform_driver);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "platform_driver_register Failed: rc = %d", rc);
		return rc;
	}

	rc = i2c_add_driver(&cam_sensor_i2c_driver);
	if (rc) {
		CAM_ERR(CAM_SENSOR, "i2c_add_driver failed rc = %d", rc);
		goto i2c_register_err;
	}

	memset(sensor_i3c_id, 0, sizeof(struct i3c_device_id) * (MAX_I3C_DEVICE_ID_ENTRIES + 1));

	dev = of_find_node_by_path(I3C_SENSOR_DEV_ID_DT_PATH);
	if (!dev) {
		CAM_DBG(CAM_SENSOR, "Couldnt Find the i3c-id-table dev node");
		return 0;
	}

	rc = cam_sensor_count_elems_i3c_device_id(dev, &num_entries,
		"i3c-sensor-id-table");
	if (rc)
		return 0;

	rc = cam_sensor_fill_i3c_device_id(dev, num_entries,
		"i3c-sensor-id-table", sensor_i3c_id);
	if (rc)
		goto i3c_register_err;

	rc = i3c_driver_register_with_owner(&cam_sensor_i3c_driver, THIS_MODULE);
	if (rc) {
		CAM_ERR(CAM_SENSOR, "i3c_driver registration failed, rc: %d", rc);
		goto i3c_register_err;
	}

	cam_sensor_module_debug_register();

	return 0;

i3c_register_err:
	i2c_del_driver(&cam_sensor_i2c_driver);
i2c_register_err:
	platform_driver_unregister(&cam_sensor_platform_driver);

	return rc;
}

void cam_sensor_driver_exit(void)
{
	struct device_node *dev;

	platform_driver_unregister(&cam_sensor_platform_driver);
	i2c_del_driver(&cam_sensor_i2c_driver);

	dev = of_find_node_by_path(I3C_SENSOR_DEV_ID_DT_PATH);
	if (!dev) {
		CAM_DBG(CAM_ACTUATOR, "Couldnt Find the i3c-id-table dev node");
		return;
	}

	i3c_driver_unregister(&cam_sensor_i3c_driver);

	cam_sensor_module_debug_deregister();
}

MODULE_DESCRIPTION("cam_sensor_driver");
MODULE_LICENSE("GPL v2");
