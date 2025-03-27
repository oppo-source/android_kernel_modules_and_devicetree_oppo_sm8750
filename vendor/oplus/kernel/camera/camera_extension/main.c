/*
 * Copyright (c) 2021 ZEKU Technology(Shanghai) Corp.,Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Basecode Created :        2020/1/20 Author: zf@zeku.com
 *
 */

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
#include "include/main.h"
#include <linux/suspend.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/sysfs.h>


#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>


#include "cam_monitor.h"
#include "cam_debug.h"
#include "cam_actuator_custom.h"
#include "cam_sensor_custom.h"
#include "cam_ois_custom.h"
#include "cam_eeprom_custom.h"
#include "tof8801_driver.h"
#include "tmf8806_driver.h"

struct camera_extension_data *g_plat_priv =NULL;

static int32_t camera_extension_ioctl_generic(
	unsigned int cmd,
	void *arg)
{
	int32_t rc = 0;
	struct extension_control *cmd_data = (struct extension_control *)arg;

	switch (cmd_data->op_code) {
	case CAM_EXTENSION_OPCODE_MONITOR_CHECK: {

		rc = cam_extension_check_process(cmd_data);
		break;
	}
	case CAM_EXTENSION_OPCODE_MONITOR_DUMP: {

		rc = cam_extension_dump_process(cmd_data);
		break;
	}
	default:

		break;
	}

	return rc;
}

static int camera_extension_open(struct inode *nodp, struct file *filp)
{
	//struct miscdevice *miscdev = (struct miscdevice *)filp->private_data;
	CAM_EXT_INFO(CAM_EXT_CORE, "%s, done.\n", __func__);
	return 0;
}

static int camera_extension_close(struct inode *nodp, struct file *filp)
{
	CAM_EXT_INFO(CAM_EXT_CORE, "%s, done.\n", __func__);
	return 0;
}

static long camera_extension_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	int type = _IOC_TYPE(cmd);
	int nr = _IOC_NR(cmd);
	unsigned int param_size = _IOC_SIZE(cmd);
	struct extension_control cmd_data;

	CAM_EXT_INFO(CAM_EXT_CORE, "%s, cmd = %d, type = %d, nr = %d, param size = %d \n",
		__func__, cmd, type, nr, param_size);

	if (type != CAM_EXTENSION_IOC_MAGIC)
	{
		CAM_EXT_ERR(CAM_EXT_CORE, "%s, ioctl magic not match, type:0x%x, CAM_EXTENSION_IOC_MAGIC: 0x%x",
			__func__, type, CAM_EXTENSION_IOC_MAGIC);
		return -EINVAL;
	}

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_EXT_ERR(CAM_EXT_CORE, "%s, Failed to copy from user_ptr=%pK size=%zu",
			__func__, (void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	/* send cmd to camera_extension */
	switch (nr) {
	case IOC_GENERIC:
		if (param_size != sizeof(struct extension_control))
		{
			CAM_EXT_ERR(CAM_EXT_CORE, "%s, ioctl param size not match, _IOC_SIZE:0x%x, data size: 0x%x",
				__func__, param_size, sizeof(struct extension_control));
			return -EINVAL;
		}
		ret = camera_extension_ioctl_generic(cmd, &cmd_data);

		break;
	default:
		CAM_EXT_ERR(CAM_EXT_CORE, "%s Don't support ioctl number: %d", __func__, nr);
		break;
	}

	CAM_EXT_INFO(CAM_EXT_CORE, "%s, done.\n", __func__);
	return ret;
}


static const struct file_operations camera_extension_ops = {
	.owner = THIS_MODULE,
	.open = camera_extension_open,
	.release = camera_extension_close,
	.unlocked_ioctl = camera_extension_ioctl,
	.compat_ioctl = camera_extension_ioctl,
};

static int camera_extension_parse_dts(struct camera_extension_data *plat_priv)
{
	int ret = 0;
	struct device *dev = &(plat_priv->plat_dev->dev);
	struct device_node *np = dev->of_node;

	CAM_EXT_INFO(CAM_EXT_CORE, "%s, begin.\n", __func__);
	if (of_property_read_bool(np, "enable_camera_extension")) {
		plat_priv->enable_camera_extension = true;
		CAM_EXT_INFO(CAM_EXT_CORE, "%s, enable_camera_extension:%d .\n", __func__, plat_priv->enable_camera_extension);
	}

	CAM_EXT_INFO(CAM_EXT_CORE, "%s, done.\n", __func__);

	return ret;
}

static int camera_extension_probe(struct platform_device *plat_dev)
{
	int ret = 0;
	struct camera_extension_data *plat_priv;
	struct cam_state_queue_info *state_queue_info = NULL;

	CAM_EXT_INFO(CAM_EXT_CORE, "%s, begin.\n", __func__);

	/* allocate camera_extension platform data */
	plat_priv = devm_kzalloc(&plat_dev->dev, sizeof(*plat_priv), GFP_KERNEL);
	if (!plat_priv) {
		CAM_EXT_ERR(CAM_EXT_CORE, "%s, Failed to alloc plat_priv.\n", __func__);
		return -ENOMEM;
	}

	plat_priv->plat_dev = plat_dev;

	platform_set_drvdata(plat_dev, plat_priv);


	CAM_EXT_INFO(CAM_EXT_CORE, "%s, camera_extension platform device name is %s.\n", __func__, plat_dev->name);

	/* create misc dev */
	plat_priv->dev.minor = MISC_DYNAMIC_MINOR;
	plat_priv->dev.name = "camera_extension_monitor";
	plat_priv->dev.fops = &camera_extension_ops;
	plat_priv->dev.mode = S_IWUSR|S_IRUGO;
	ret = misc_register(&plat_priv->dev);

	camera_extension_parse_dts(plat_priv);

	state_queue_info = &plat_priv->state_queue;
	state_queue_info->state_monitor =  kzalloc(
				sizeof(struct cam_state_monitor) * CAM_STATE_MONITOR_MAX_ENTRIES,
				GFP_KERNEL);
	atomic64_set(&state_queue_info->state_monitor_head, -1);

	if (!state_queue_info->state_monitor)
	{
		CAM_EXT_ERR(CAM_EXT_CORE, "%s, failed to alloc memory for state monitor", __func__);
		goto free_epd;
	}

	g_plat_priv = plat_priv;

	init_cam_monitor(plat_dev);

	create_sysfs();

	cam_ext_debugfs_init();

	CAM_EXT_INFO(CAM_EXT_CORE, "%s, done.\n", __func__);
	return 0;


free_epd:
	devm_kfree(&plat_dev->dev, plat_priv);
	return ret;
}

static int camera_extension_remove(struct platform_device *plat_dev)
{
	struct camera_extension_data *plat_priv = platform_get_drvdata(plat_dev);

	misc_deregister(&plat_priv->dev);

	return 0;
}


static const struct of_device_id camera_extension_of_match_table[] = {
	{.compatible = "oplus,camera_extension", },
	{ },
};
MODULE_DEVICE_TABLE(of, camera_extension_of_match_table);

#ifdef CONFIG_PM_SLEEP
static int camera_extension_suspend(struct device *dev)
{
	struct camera_extension_data *ced = dev_get_drvdata(dev);
	struct monitor_check	check_result;

	CAM_EXT_INFO(CAM_EXT_CORE, "%s, E. enable_camera_extension:%d \n", __func__, ced->enable_camera_extension);

	// if(true == g_is_enable_dump)
	// {
	// 	extension_dump_monitor_print();
	// }

	check_power_exception(&check_result);
	CAM_EXT_INFO(CAM_EXT_CORE, "%s, camera extension monitor check result: feature_inuse %d clk %d rgltr %d\n",
		__func__,
		check_result.camera_feature_inuse_mask,
		check_result.count_inuse_clock,
		check_result.count_inuse_regulator);

	CAM_EXT_INFO(CAM_EXT_CORE, "%s, X.\n", __func__);


	return 0;
}

static int camera_extension_resume(struct device *dev)
{
	CAM_EXT_INFO(CAM_EXT_CORE, "%s, resume.\n", __func__);
	return 0;
}
#endif


static const struct dev_pm_ops camera_extension_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(camera_extension_suspend, camera_extension_resume)
};

static struct platform_driver camera_extension_platform_driver = {
	.probe  = camera_extension_probe,
	.remove = camera_extension_remove,
	.driver = {
		.name = "camera_extension",
		.of_match_table = camera_extension_of_match_table,
		.pm = &camera_extension_pm_ops,
	},
};

/*
 * Module init function
 */
static int __init camera_extension_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&camera_extension_platform_driver);

	cam_actuator_register();
	cam_sensor_register();
	cam_ois_register();
	cam_eeprom_register();
	cam_tmf8806_driver_init();
	cam_tof8801_driver_init();
	CAM_EXT_INFO(CAM_EXT_CORE, "%s, done.\n", __func__);

	/*
	 * A non 0 return means init_module failed; module can't be loaded.
	 */
	return ret;
}

/*
 * Module exit function
 */
static void __exit camera_extension_exit(void)
{
	platform_driver_unregister(&camera_extension_platform_driver);
	cam_tmf8806_driver_exit();
	cam_tof8801_driver_exit();
	CAM_EXT_INFO(CAM_EXT_CORE, "%s, done.\n", __func__);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Camera Extension Linux Kernel module");

module_init(camera_extension_init);
module_exit(camera_extension_exit);
