/*
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include "oplus_display_interface.h"
#include "oplus_bl_ic_ktz8868.h"


#define KTZ8868_I2C_M_NAME  			"ktz8868-master"
#define KTZ8868_I2C_S_NAME  			"ktz8868-salve"
#define KTZ8868_HW_GPIO_NAME			"ktz8868_hw_en_gpio"
#define KTZ8868_BAIS_ENP_GPIO_NAME		"ktz8868_bais_enp_gpio"
#define KTZ8868_BAIS_ENN_GPIO_NAME		"ktz8868_bais_enn_gpio"

#define KTZ8868_IC_BL_LEVEL_MAX			(2047)
/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/
static struct i2c_client *g_i2c_m_client = NULL;
static struct i2c_client *g_i2c_s_client = NULL;
static int ktz8868_hw_en_gpio_num = -1;
static int ktz8868_bais_enp_gpio_num = -1;
static int ktz8868_bais_enn_gpio_num = -1;
static DEFINE_MUTEX(read_lock);

/*****************************************************************************
 * Extern Area
 *****************************************************************************/

static int ktz8868_ic_write_byte_single(struct i2c_client *i2c_client, unsigned char addr, unsigned char value)
{
	int ret = 0;
	unsigned char write_data[2] = {0};

	if (NULL == i2c_client) {
		pr_err("[LCD][BL] i2c_client is null!!\n");
		return -EINVAL;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(i2c_client, write_data, 2);

	if (ret < 0)
	pr_err("[LCD][BL] i2c write data fail !!\n");

	return ret;
}

static int ktz8868_ic_write_byte_dual(unsigned char addr, unsigned char value)
{
	int ret = 0;
	unsigned char write_data[2] = {0};

	if ((NULL == g_i2c_m_client)||(NULL == g_i2c_s_client)) {
		pr_err("[LCD][BL] i2c_client is null!!\n");
		return -EINVAL;
	}

	write_data[0] = addr;
	write_data[1] = value;

	ret = i2c_master_send(g_i2c_m_client, write_data, 2);
	if (ret < 0) {
		pr_err("[LCD][BL] i2c write data fail %s !!\n", dev_name(&g_i2c_m_client->dev));
	}

	ret = i2c_master_send(g_i2c_s_client, write_data, 2);
	if (ret < 0) {
		pr_err("[LCD][BL] i2c write data fail %s !!\n", dev_name(&g_i2c_s_client->dev));
	}

	return ret;
}

int ktz8868_ic_read_byte_dual(struct i2c_client *i2c_client_m, unsigned char i2c_client_m_addr, unsigned char *i2c_client_m_buf,
		struct i2c_client *i2c_client_s, unsigned char i2c_client_s_addr, unsigned char *i2c_client_s_buf)
{
	int res = 0;

	mutex_lock(&read_lock);

	if (i2c_client_m) {
		res = i2c_master_send(i2c_client_m, &i2c_client_m_addr, 0x1);
		if (res <= 0) {
			mutex_unlock(&read_lock);
			pr_err("[LCD][BL]read reg send res = %d %s\n", res, dev_name(&i2c_client_m->dev));
			return res;
		}
		res = i2c_master_recv(i2c_client_m, i2c_client_m_buf, 0x1);
		if (res <= 0) {
			mutex_unlock(&read_lock);
			pr_err("[LCD][BL]read reg recv res = %d %s\n", res, dev_name(&i2c_client_m->dev));
			return res;
		}
	} else {
		pr_err("[LCD][BL] i2c_client_m is NULL\n");
	}

	if (i2c_client_s) {
		res = i2c_master_send(i2c_client_s, &i2c_client_s_addr, 0x1);
		if (res <= 0) {
			mutex_unlock(&read_lock);
			pr_err("[LCD][BL]read reg send res = %d %s\n", res, dev_name(&i2c_client_s->dev));
			return res;
		}
		res = i2c_master_recv(i2c_client_s, i2c_client_s_buf, 0x1);
		if (res <= 0) {
			mutex_unlock(&read_lock);
			pr_err("[LCD][BL]read reg recv res = %d %s\n", res, dev_name(&i2c_client_s->dev));
			return res;
		}
	} else {
		pr_err("[LCD][BL] i2c_client_s is NULL\n");
	}

	mutex_unlock(&read_lock);

	return res;
}

static int bl_ic_ktz8868_enable(bool enable)
{
	static bool bl_ic_ktz8868_enabled = false;

	if (enable) {
		if (!bl_ic_ktz8868_enabled) {
			/* config i2c0 and i2c3 */
			/* BL_CFG1；OVP=34.0V，线性调光，PWM Disabled */
			ktz8868_ic_write_byte_dual(0x02, 0XB3); //set ovp 31.5v pwm enable
			/* Current ramp 256ms pwm_hyst 10lsb */
			ktz8868_ic_write_byte_dual(0x03, 0XEB); //set default LED CURRENT RAMP and PWM_HYST
			/* BL_OPTION2；电感4.7uH，BL_CURRENT_LIMIT 2.5A */
			ktz8868_ic_write_byte_dual(0x11, 0x76); //4.7uH and 2.0A current limit
			/* Backlight Full-scale LED Current 30.0mA/CH */
			pr_err("[LCD]bl_ic_ktz8868 set LED Current 30.0mA/CH\n");
			ktz8868_ic_write_byte_dual(0x15, 0xF8); //4.7uH and 2.0A current limit
			ktz8868_ic_write_byte_dual(0x08, 0x77);

			bl_ic_ktz8868_enabled = true;
			pr_err("[LCD]bl_ic_ktz8868 enable\n");
		}
	} else {
		/* BL disabled and Current sink 1/2/3/4 /5 enabled；*/
		ktz8868_ic_write_byte_dual(0x08, 0x00);

		bl_ic_ktz8868_enabled = false;
		pr_err("[LCD]bl_ic_ktz8868 disable\n");
	}
	return 0;
}

int bl_ic_ktz8868_set_brightness(int bl_lvl)//for set bringhtness
{
	unsigned int mapping_value = 0;
	static bool ktz8868_set_bl_flag = false;

	if (bl_lvl < 0) {
		pr_err("[LCD]%d %s set backlight invalid value=%d, not to set\n", __LINE__, __func__, bl_lvl);
		return 0;
	}

	if (bl_lvl > KTZ8868_IC_BL_LEVEL_MAX) {
		mapping_value = backlight_map[KTZ8868_IC_BL_LEVEL_MAX];
	} else {
		mapping_value = backlight_map[bl_lvl];
	}
	OPLUS_DSI_DEBUG("[LCD]%s:set backlight lvl= %d, mapping value = %d\n", __func__, bl_lvl, mapping_value);

	if (bl_lvl > 0) {
		if(ktz8868_set_bl_flag == false) {
			bl_ic_ktz8868_enable(true); /* BL enabled and Current sink 1/2/3/4/5 enabled */
		}
		ktz8868_ic_write_byte_dual(0x04, mapping_value & 0x07); /* lsb */
		ktz8868_ic_write_byte_dual(0x05, (mapping_value >> 3) & 0xFF); /* msb */
		if (ktz8868_set_bl_flag == false) {
			mdelay(15);
			ktz8868_ic_write_byte_dual(0x01, 0x01);
			ktz8868_set_bl_flag = true;
		}
	}

	if (bl_lvl == 0) {
		ktz8868_ic_write_byte_dual(0x01, 0x00); /* disable backlight */
		mdelay(9);
		ktz8868_ic_write_byte_dual(0x04, 0x00); /* lsb */
		ktz8868_ic_write_byte_dual(0x05, 0x00); /* msb */
		if(ktz8868_set_bl_flag == true) {
			bl_ic_ktz8868_enable(false); /* BL disabled and Current sink 1/2/3/4/5 disabled */
			ktz8868_set_bl_flag = false;
		}
	}
	return 0;
}

int bl_ic_ktz8868_set_lcd_bias_by_gpio(bool enable)
{
	int  rc = 0;;

	if (enable) {
		pr_err("[LCD]%s:enable lcd_enable_bias by gpio\n", __func__);
		mdelay(1);
		/* enable bl bais enp */
		if (gpio_is_valid(ktz8868_bais_enp_gpio_num)) {
			rc = gpio_direction_output(ktz8868_bais_enp_gpio_num, true);
			if (rc < 0) {
				pr_err("[LCD]unable to set bl_bais_enp to high rc=%d\n", rc);
				gpio_free(ktz8868_bais_enp_gpio_num);
			}
		}
		mdelay(8);
		/* enable bl bais enn */
		if (gpio_is_valid(ktz8868_bais_enn_gpio_num)) {
			rc = gpio_direction_output(ktz8868_bais_enn_gpio_num, true);
			if (rc < 0) {
				pr_err("[LCD]unable to set bl_bais_enn to high rc=%d\n", rc);
				gpio_free(ktz8868_bais_enn_gpio_num);
			}
		}
		mdelay(10);
	} else {
		pr_err("[LCD]%s:disable lcd_enable_bias by gpio\n", __func__);
		mdelay(2);
		/* disable bl bais enn */
		if (gpio_is_valid(ktz8868_bais_enn_gpio_num)) {
			rc = gpio_direction_output(ktz8868_bais_enn_gpio_num, false);
			if (rc < 0) {
				pr_err("[LCD]unable to set bl_bais_enn to low rc=%d\n", rc);
				gpio_free(ktz8868_bais_enn_gpio_num);
			}
		}
		mdelay(8);
		/* disable bl bais enp */
		if (gpio_is_valid(ktz8868_bais_enp_gpio_num)) {
			rc = gpio_direction_output(ktz8868_bais_enp_gpio_num, false);
			if (rc < 0) {
				pr_err("[LCD]unable to  set bl_bais_enp to low rc=%d\n", rc);
				gpio_free(ktz8868_bais_enp_gpio_num);
			}
		}
		mdelay(10);
	}

	return 0;
}

int bl_ic_ktz8868_set_lcd_bias_by_reg(struct dsi_panel *panel, bool enable)
{
	if (enable) {
		pr_err("[LCD] enable lcd_enable_bias by reg\n");
		/* only config i2c0*/
		ktz8868_ic_write_byte_single(g_i2c_m_client, 0x0C, 0x28);/* LCD_BOOST_CFG */
		ktz8868_ic_write_byte_single(g_i2c_m_client, 0x0D, 0x1E);/* OUTP_CFG，OUTP = 6.0V */
		ktz8868_ic_write_byte_single(g_i2c_m_client, 0x0E, 0x1E);/* OUTN_CFG，OUTN = -6.0V */
		ktz8868_ic_write_byte_single(g_i2c_m_client, 0x09, 0x9E);/* enable OUTP */
	} else {
		pr_err("[LCD] disable lcd_enable_bias by reg\n");
		ktz8868_ic_write_byte_single(g_i2c_m_client, 0x09, 0x9C);/* Disable OUTN */
		mdelay(5);
		ktz8868_ic_write_byte_single(g_i2c_m_client, 0x09, 0x98);/* Disable OUTP */
	}
	return 0;
}

int bl_ic_ktz8868_hw_en(bool enable)
{
	int ret;
	u8 value  = enable ? 1 : 0;

	if(gpio_is_valid(ktz8868_hw_en_gpio_num)) {
		ret = gpio_direction_output(ktz8868_hw_en_gpio_num, value);
		if(ret){
			pr_err("[LCD]failed to set %s gpio %d, ret = %d\n", KTZ8868_HW_GPIO_NAME, value, ret);
			return ret;
		} else {
			pr_err("[LCD]%s:set KTZ8868_HW_EN enable=%d succ\n", __func__, enable);
		}
		if (value) {
			mdelay(1);
			ktz8868_ic_write_byte_single(g_i2c_m_client, 0x0C, 0x28);/* LCD_BOOST_CFG */
			ktz8868_ic_write_byte_single(g_i2c_m_client, 0x0D, 0x1E);/* OUTP_CFG，OUTP = 6.0V */
			ktz8868_ic_write_byte_single(g_i2c_m_client, 0x0E, 0x1E);/* OUTN_CFG，OUTN = -6.0V */
			ktz8868_ic_write_byte_single(g_i2c_m_client, 0x09, 0x99);/* enable OUTP */
		}
	} else {
		pr_err("[LCD]get KTZ8868_HW_EN gpio(%d) is not vaild\n", ktz8868_hw_en_gpio_num);
	}

	return 0;
}

static int ktz8868_i2c_master_probe(struct i2c_client *client)
{
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_warn(&client->dev, "[LCD]xx adapter does not support I2C\n");
		return -EIO;
	}

	g_i2c_m_client = client;
	if (client->dev.of_node) {
		ktz8868_hw_en_gpio_num = of_get_named_gpio(client->dev.of_node, KTZ8868_HW_GPIO_NAME, 0);
		if (ktz8868_hw_en_gpio_num < 0) {
			dev_err(&client->dev,"[LCD]failed to get %s\n", KTZ8868_HW_GPIO_NAME);
			return -EINVAL;
		} else {
			pr_err("[LCD]%s:get %s num=%d SUCC!\n", __func__, KTZ8868_HW_GPIO_NAME, ktz8868_hw_en_gpio_num);
			//turn on hw_en
			if(bl_ic_ktz8868_hw_en(true)) {
				pr_err("[LCD]failed to turn on hwen!\n");
			}
		}

		ktz8868_bais_enp_gpio_num = of_get_named_gpio(client->dev.of_node, KTZ8868_BAIS_ENP_GPIO_NAME, 0);
		if (ktz8868_bais_enp_gpio_num < 0) {
			dev_err(&client->dev,"[LCD]failed to get %s\n", KTZ8868_BAIS_ENP_GPIO_NAME);
		} else {
			pr_err("[LCD]%s:get %s num=%d SUCC!\n", __func__, KTZ8868_BAIS_ENP_GPIO_NAME, ktz8868_bais_enp_gpio_num);
		}
		ktz8868_bais_enn_gpio_num = of_get_named_gpio(client->dev.of_node, KTZ8868_BAIS_ENN_GPIO_NAME, 0);
		if (ktz8868_bais_enn_gpio_num < 0) {
			dev_err(&client->dev,"failed to get %s\n", KTZ8868_BAIS_ENN_GPIO_NAME);
		} else {
			pr_err("[LCD]%s:get %s num=%d SUCC!\n", __func__, KTZ8868_BAIS_ENN_GPIO_NAME, ktz8868_bais_enn_gpio_num);
		}
	}
	pr_err("[LCD]%s:get g_i2c_m_client SUCC!\n", __func__);

	return 0;
}

static void ktz8868_i2c_master_remove(struct i2c_client *client)
{
	i2c_unregister_device(client);
	g_i2c_m_client = NULL;
	pr_err("[LCD]%s: dev_name:%s unregister\n", __func__, dev_name(&client->dev));
}

static int ktz8868_i2c_salve_probe(struct i2c_client *client)
{
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_warn(&client->dev, "[LCD]xx adapter does not support I2C\n");
		return -EIO;
	}

	g_i2c_s_client = client;
	pr_err("[LCD]%s:get g_i2c_s_client SUCC!\n", __func__);

	return 0;
}


static void ktz8868_i2c_salve_remove(struct i2c_client *client)
{
	i2c_unregister_device(client);
	g_i2c_s_client = NULL;
	pr_err("[LCD]%s: dev_name:%s unregister\n", __func__, dev_name(&client->dev));
}


/************************************************************
Attention:
Althouh i2c_bus do not use .id_table to match, but it must be defined,
otherwise the probe function will not be executed!
************************************************************/
static const struct of_device_id ktz8868_m_i2c_of_match[] = {
	{ .compatible = "ktz8868-i2c-master", },
	{},
};

static const struct i2c_device_id ktz8868_m_i2c_id_table[] = {
	{KTZ8868_I2C_M_NAME, 0},
	{},
};

static struct i2c_driver ktz8868_i2c_m_driver = {
	.probe = ktz8868_i2c_master_probe,
	.remove = ktz8868_i2c_master_remove,
	.id_table = ktz8868_m_i2c_id_table,
	.driver = {
		.owner = THIS_MODULE,
		.name = KTZ8868_I2C_M_NAME,
		.of_match_table = ktz8868_m_i2c_of_match,
    },
};

static const struct of_device_id ktz8868_s_i2c_of_match[] = {
	{ .compatible = "ktz8868-i2c-salve", },
	{},
};

static const struct i2c_device_id ktz8868_s_i2c_id_table[] = {
	{KTZ8868_I2C_S_NAME, 0},
	{},
};

static struct i2c_driver ktz8868_i2c_s_driver = {
	.probe = ktz8868_i2c_salve_probe,
	.remove = ktz8868_i2c_salve_remove,
	.id_table = ktz8868_s_i2c_id_table,
	.driver = {
		.owner = THIS_MODULE,
		.name = KTZ8868_I2C_S_NAME,
		.of_match_table = ktz8868_s_i2c_of_match,
    },
};

int __init bl_ic_ktz8868_init(void)
{
	pr_err("[LCD][BL]bl_ic_ktz8868_init +++\n");

	g_i2c_s_client = NULL;
	g_i2c_m_client = NULL;
	if (i2c_add_driver(&ktz8868_i2c_s_driver)) {
		pr_err("[LCD][BL]Failed to register ktz8868_i2c_s_driver!\n");
		return -EINVAL;
	}

	if (i2c_add_driver(&ktz8868_i2c_m_driver)) {
		pr_err("[LCD][BL]Failed to register ktz8868_i2c_m_driver!\n");
		i2c_del_driver(&ktz8868_i2c_s_driver);
		g_i2c_s_client = NULL;
		return -EINVAL;
	}

	pr_err("[LCD][BL]bl_ic_ktz8868_init ---\n");
	return 0;
}

void __exit bl_ic_ktz8868_exit(void)
{
	i2c_del_driver(&ktz8868_i2c_m_driver);
	i2c_del_driver(&ktz8868_i2c_s_driver);
	g_i2c_s_client = NULL;
	g_i2c_m_client = NULL;
}
