/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_power.c
** Description : oplus display panel power control
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/power_supply.h>
#include "oplus_display_power.h"
#include "oplus_debug.h"
#include "oplus_panel.h"
#include "oplus_adfr.h"
#include "oplus_display_pwm.h"
#include "oplus_bl_ic_ktz8868.h"

#ifdef OPLUS_FEATURE_TP_BASIC
#include "oplus_display_notify_tp.h"
#endif /* OPLUS_FEATURE_TP_BASIC */

#define OPLUS_PINCTRL_NAMES_COUNT 2

PANEL_VOLTAGE_BAK panel_vol_bak[PANEL_VOLTAGE_ID_MAX] = {{0}, {0}, {2, 0, 1, 2, ""}};
u32 panel_pwr_vg_base = 0;
struct notifier_block psy_nb = {0};

int oplus_panel_parse_power_config(struct dsi_panel *panel)
{
	int rc = 0, i = 0;
	const char *name_vddi = NULL;
	const char *name_vddr = NULL;
	u32 *panel_vol = NULL;
	struct dsi_parser_utils *utils = &panel->utils;

	OPLUS_DSI_INFO("parse panel power config\n");

	if (!strcmp(panel->type, "primary")) {
		panel_vol = &panel_vol_bak[PANEL_VOLTAGE_ID_VDDI].voltage_id;
		rc = utils->read_u32_array(utils->data, "qcom,panel_voltage_vddi",
				panel_vol, PANEL_VOLTAGE_VALUE_COUNT);

		if (rc) {
			OPLUS_DSI_ERR("[%s] failed to parse panel_voltage vddi\n",
					panel->oplus_panel.vendor_name);
			goto error;
		}

		rc = utils->read_string(utils->data, "qcom,panel_voltage_vddi_name",
				&name_vddi);

		if (rc) {
			OPLUS_DSI_ERR("[%s] failed to parse vddi name\n",
					panel->oplus_panel.vendor_name);
			goto error;

		} else {
			OPLUS_DSI_INFO("[%s] surccess to parse vddi name %s\n",
					panel->oplus_panel.vendor_name, name_vddi);
			strcpy(panel_vol_bak[PANEL_VOLTAGE_ID_VDDI].pwr_name, name_vddi);
		}

		panel_vol = &panel_vol_bak[PANEL_VOLTAGE_ID_VDDR].voltage_id;
		rc = utils->read_u32_array(utils->data, "qcom,panel_voltage_vddr",
				panel_vol, PANEL_VOLTAGE_VALUE_COUNT);

		if (rc) {
			OPLUS_DSI_ERR("[%s] failed to parse panel_voltage vddr\n",
					panel->oplus_panel.vendor_name);
			goto error;
		}

		rc = utils->read_string(utils->data, "qcom,panel_voltage_vddr_name",
				&name_vddr);

		if (rc) {
			OPLUS_DSI_ERR("[%s] failed to parse vddr name\n",
					panel->oplus_panel.vendor_name);
			goto error;

		} else {
			OPLUS_DSI_INFO("[%s] surccess to parse vddr name %s\n",
					panel->oplus_panel.vendor_name, name_vddr);
			strcpy(panel_vol_bak[PANEL_VOLTAGE_ID_VDDR].pwr_name, name_vddr);
		}
		/* add for debug */
		for (i = 0; i < PANEL_VOLTAGE_ID_MAX; i++) {
			OPLUS_DSI_INFO("panel_voltage[%d] = %d,%d,%d,%d,%s\n", i,
					panel_vol_bak[i].voltage_id,
					panel_vol_bak[i].voltage_min, panel_vol_bak[i].voltage_current,
					panel_vol_bak[i].voltage_max, panel_vol_bak[i].pwr_name);
		}
	}

error:
	return rc;
}

static int oplus_panel_find_vreg_by_name(const char *name)
{
	int count = 0, i = 0;
	struct dsi_vreg *vreg = NULL;
	struct dsi_regulator_info *dsi_reg = NULL;
	struct dsi_display *display = get_main_display();

	if (!display) {
		return -ENODEV;
	}

	if (!display->panel) {
		return -EINVAL;
	}

	dsi_reg = &display->panel->power_info;
	count = dsi_reg->count;

	for (i = 0; i < count; i++) {
		vreg = &dsi_reg->vregs[i];
		OPLUS_DSI_INFO("finding: %s\n", vreg->vreg_name);

		if (!strcmp(vreg->vreg_name, name)) {
			OPLUS_DSI_INFO("find the vreg: %s\n", name);
			return i;

		} else {
			continue;
		}
	}

	OPLUS_DSI_ERR("dose not find the vreg: %s\n", name);

	return -EINVAL;
}

static u32 oplus_panel_update_current_voltage(u32 id)
{
	int vol_current = 0, pwr_id = 0;
	struct dsi_vreg *dsi_reg = NULL;
	struct dsi_regulator_info *dsi_reg_info = NULL;
	struct dsi_display *display = get_main_display();

	if (!display) {
		return -ENODEV;
	}

	if (!display->panel || !display->drm_conn) {
		return -EINVAL;
	}

	dsi_reg_info = &display->panel->power_info;
	pwr_id = oplus_panel_find_vreg_by_name(panel_vol_bak[id].pwr_name);

	if (pwr_id < 0) {
		OPLUS_DSI_ERR("can't find the pwr_id, please check the vreg name\n");
		return pwr_id;
	}

	dsi_reg = &dsi_reg_info->vregs[pwr_id];

	if (!dsi_reg) {
		return -EINVAL;
	}

	vol_current = regulator_get_voltage(dsi_reg->vreg);

	return vol_current;
}

int oplus_display_panel_get_pwr(void *data)
{
	int ret = 0;
	struct panel_vol_get *panel_vol = data;
	u32 vol_id = ((panel_vol->panel_id & 0x0F) - 1);

	if (vol_id < 0 || vol_id >= PANEL_VOLTAGE_ID_MAX) {
		OPLUS_DSI_ERR("error id: %d\n", vol_id);
		return -EINVAL;
	}

	OPLUS_DSI_INFO("id = %d\n", vol_id);
	panel_vol->panel_min = panel_vol_bak[vol_id].voltage_min;
	panel_vol->panel_max = panel_vol_bak[vol_id].voltage_max;
	panel_vol->panel_cur = panel_vol_bak[vol_id].voltage_current;

	if (vol_id < PANEL_VOLTAGE_ID_VG_BASE &&
		vol_id >= PANEL_VOLTAGE_ID_VDDI) {
		ret = oplus_panel_update_current_voltage(vol_id);
		if (ret < 0) {
			OPLUS_DSI_ERR("update_current_voltage error = %d\n", ret);
			return ret;
		} else {
			panel_vol->panel_cur = ret;
			OPLUS_DSI_ERR("[id min cur max] = [%u32, %u32, %u32, %u32]\n",
				vol_id, panel_vol->panel_min,
				panel_vol->panel_cur, panel_vol->panel_max);
			return 0;
		}
	}

	return ret;
}

int oplus_display_panel_set_pwr(void *data)
{
	struct panel_vol_set *panel_vol = data;
	int panel_vol_value = 0, rc = 0, panel_vol_id = 0, pwr_id = 0;
	struct dsi_vreg *dsi_reg = NULL;
	struct dsi_regulator_info *dsi_reg_info = NULL;
	struct dsi_display *display = get_main_display();

	panel_vol_id = ((panel_vol->panel_id & 0x0F)-1);
	panel_vol_value = panel_vol->panel_vol;

	if (panel_vol_id < 0 || panel_vol_id >= PANEL_VOLTAGE_ID_MAX) {
		OPLUS_DSI_ERR("error id: %d\n", panel_vol_id);
		return -EINVAL;
	}

	OPLUS_DSI_INFO("id = %d, value = %d\n",
			panel_vol_id, panel_vol_value);
	if (panel_vol_value < panel_vol_bak[panel_vol_id].voltage_min ||
			panel_vol_value > panel_vol_bak[panel_vol_id].voltage_max)
		return -EINVAL;

	if (!display) {
		return -ENODEV;
	}

	if (!display->panel || !display->drm_conn) {
		return -EINVAL;
	}

	if (panel_vol_id == PANEL_VOLTAGE_ID_VG_BASE) {
		OPLUS_DSI_ERR("set the VGH_L pwr = %d \n", panel_vol_value);
		panel_pwr_vg_base = panel_vol_value;
		return rc;
	}

	dsi_reg_info = &display->panel->power_info;

	pwr_id = oplus_panel_find_vreg_by_name(panel_vol_bak[panel_vol_id].pwr_name);
	if (pwr_id < 0) {
		OPLUS_DSI_ERR("can't find the vreg name, please re-check vreg name: %s \n",
			panel_vol_bak[panel_vol_id].pwr_name);
		return pwr_id;
	}

	dsi_reg = &dsi_reg_info->vregs[pwr_id];

	rc = regulator_set_voltage(dsi_reg->vreg, panel_vol_value, panel_vol_value);

	if (rc) {
		OPLUS_DSI_ERR("Set voltage(%s) fail, rc=%d\n",
			 dsi_reg->vreg_name, rc);
		return -EINVAL;
	}

	return rc;
}

int oplus_display_panel_get_power_status(void *data) {
	uint32_t *power_status = data;
	struct dsi_display *display = oplus_display_get_current_display();

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("display is null or display->panel is null\n");
		return -EINVAL;
	}

	OPLUS_DSI_INFO("oplus_display_get_power_status = %d\n", display->panel->power_mode);
	(*power_status) = display->panel->power_mode;

	return 0;
}

int oplus_display_panel_set_power_status(void *data) {
	uint32_t *temp_save = data;

	OPLUS_DSI_INFO("oplus_display_set_power_status = %d\n", (*temp_save));

	return 0;
}

int oplus_display_panel_regulator_control(void *data) {
	uint32_t *temp_save_user = data;
	uint32_t temp_save = (*temp_save_user);
	struct dsi_display *temp_display;

	OPLUS_DSI_INFO("oplus_display_regulator_control = %d\n", temp_save);
	if(get_main_display() == NULL) {
		OPLUS_DSI_ERR("display is null\n");
		return -1;
	}
	temp_display = get_main_display();
	if(temp_save == 0) {
		dsi_pwr_enable_regulator(&temp_display->panel->power_info, false);
	} else if (temp_save == 1) {
		dsi_pwr_enable_regulator(&temp_display->panel->power_info, true);
	}

	return 0;
}

int oplus_panel_parse_gpios(struct dsi_panel *panel)
{
	struct dsi_parser_utils *utils;
	struct oplus_gpio_config* gpio_cfg;

	if (!panel) {
		OPLUS_DSI_ERR("Oplus Features config No panel device\n");
		return -ENODEV;
	}
	utils = &panel->utils;
	gpio_cfg = &panel->oplus_panel.gpio_cfg;

	gpio_cfg->panel_gpio1 = utils->get_named_gpio(utils->data, "oplus,panel-gpio1", 0);
	if (!gpio_is_valid(gpio_cfg->panel_gpio1)) {
		OPLUS_DSI_INFO("[%s] failed get panel_gpio1\n", panel->oplus_panel.vendor_name);
	}

	gpio_cfg->panel_gpio2 = utils->get_named_gpio(utils->data, "oplus,panel-gpio2", 0);
	if (!gpio_is_valid(gpio_cfg->panel_gpio2)) {
		OPLUS_DSI_INFO("[%s] failed get oplus,panel-gpio2\n", panel->oplus_panel.vendor_name);
	}

	gpio_cfg->panel_gpio3 = utils->get_named_gpio(utils->data, "oplus,panel-gpio3", 0);
	if (!gpio_is_valid(gpio_cfg->panel_gpio3)) {
		OPLUS_DSI_INFO("[%s] failed get oplus,panel-gpio3\n", panel->oplus_panel.vendor_name);
	}

	gpio_cfg->pmic_gpio = utils->get_named_gpio(utils->data, "oplus,pmic-gpio", 0);
	if (!gpio_is_valid(gpio_cfg->pmic_gpio)) {
		OPLUS_DSI_INFO("[%s] failed get oplus,pmic-gpio\n", panel->oplus_panel.vendor_name);
	} else {
		oplus_panel_register_supply_notifier();
	}

	return 0;
}

int oplus_panel_gpio_request(struct dsi_panel *panel)
{
	int rc = 0;
	struct oplus_gpio_config *gpio_cfg;
	if (!panel) {
		OPLUS_DSI_ERR("Oplus Features config No panel device\n");
		return -ENODEV;
	}

	gpio_cfg = &panel->oplus_panel.gpio_cfg;

	if (gpio_is_valid(gpio_cfg->panel_gpio1)) {
		rc = gpio_request(gpio_cfg->panel_gpio1, "panel_gpio1");
		if (rc) {
			OPLUS_DSI_ERR("request for panel_gpio1 failed, rc=%d\n", rc);
			if (gpio_is_valid(gpio_cfg->panel_gpio1))
				gpio_free(gpio_cfg->panel_gpio1);
		}
	}
	if (gpio_is_valid(gpio_cfg->panel_gpio2)) {
		rc = gpio_request(gpio_cfg->panel_gpio2, "panel_gpio2");
		if (rc) {
			OPLUS_DSI_ERR("request for panel_gpio2 failed, rc=%d\n", rc);
			if (gpio_is_valid(gpio_cfg->panel_gpio2))
				gpio_free(gpio_cfg->panel_gpio2);
		}
	}
	if (gpio_is_valid(gpio_cfg->panel_gpio3)) {
		rc = gpio_request(gpio_cfg->panel_gpio3, "panel_gpio3");
		if (rc) {
			OPLUS_DSI_ERR("request for panel_gpio3 failed, rc=%d\n", rc);
			if (gpio_is_valid(gpio_cfg->panel_gpio3))
				gpio_free(gpio_cfg->panel_gpio3);
		}
	}
	if (gpio_is_valid(gpio_cfg->pmic_gpio)) {
		rc = gpio_request(gpio_cfg->pmic_gpio, "pmic_gpio");
		if (rc) {
			OPLUS_DSI_ERR("request for pmic_gpio failed, rc=%d\n", rc);
			if (gpio_is_valid(gpio_cfg->pmic_gpio))
				gpio_free(gpio_cfg->pmic_gpio);
		}
	}

	return rc;
}

int oplus_panel_gpio_release(struct dsi_panel *panel)
{
	struct oplus_gpio_config *gpio_cfg;
	if (!panel) {
		OPLUS_DSI_ERR("Oplus Features config No panel device\n");
		return -ENODEV;
	}

	gpio_cfg = &panel->oplus_panel.gpio_cfg;

	if (gpio_is_valid(gpio_cfg->panel_gpio1))
		gpio_free(gpio_cfg->panel_gpio1);
	if (gpio_is_valid(gpio_cfg->panel_gpio2))
		gpio_free(gpio_cfg->panel_gpio2);
	if (gpio_is_valid(gpio_cfg->panel_gpio3))
		gpio_free(gpio_cfg->panel_gpio3);
	if (gpio_is_valid(gpio_cfg->pmic_gpio))
		gpio_free(gpio_cfg->pmic_gpio);

	return 0;
}

int oplus_panel_set_pinctrl_state(struct dsi_panel *panel, bool enable)
{
	int rc = 0;
	struct pinctrl_state *state;

	if (panel->host_config.ext_bridge_mode)
		return 0;

	if (!panel->pinctrl.pinctrl)
		return 0;

	/* oplus panel pinctrl */
	if (panel->oplus_panel.pinctrl_enabled) {
		if (enable)
			state = panel->oplus_panel.pinctrl_info.oplus_panel_active;
		else
			state = panel->oplus_panel.pinctrl_info.oplus_panel_suspend;

		rc = pinctrl_select_state(panel->pinctrl.pinctrl, state);
		if (rc)
			OPLUS_DSI_ERR("[%s] failed to set oplus pin state, rc=%d\n",
					panel->oplus_panel.vendor_name, rc);
	}

	return rc;
}

int oplus_panel_pinctrl_init(struct dsi_panel *panel)
{
	int rc = 0, count = 0;
	const char *pinctrl_name;

	if (panel->host_config.ext_bridge_mode)
		return 0;

	panel->pinctrl.pinctrl = devm_pinctrl_get(panel->parent);
	if (IS_ERR_OR_NULL(panel->pinctrl.pinctrl)) {
		rc = PTR_ERR(panel->pinctrl.pinctrl);
		OPLUS_DSI_ERR("failed to get pinctrl, rc=%d\n", rc);
		goto error;
	}

	/* oplus panel pinctrl */
	count = of_property_count_strings(panel->panel_of_node,
			"oplus,dsi-pinctrl-names");
	if (OPLUS_PINCTRL_NAMES_COUNT == count) {
		of_property_read_string_index(panel->panel_of_node,
				"oplus,dsi-pinctrl-names", 0, &pinctrl_name);
		panel->oplus_panel.pinctrl_info.oplus_panel_active =
				pinctrl_lookup_state(panel->pinctrl.pinctrl, pinctrl_name);
		if (IS_ERR_OR_NULL(panel->oplus_panel.pinctrl_info.oplus_panel_active)) {
			rc = PTR_ERR(panel->oplus_panel.pinctrl_info.oplus_panel_active);
			OPLUS_DSI_ERR("[%s] failed to get pinctrl: %s, rc=%d\n",
					panel->oplus_panel.vendor_name, pinctrl_name, rc);
			goto error;
		}

		of_property_read_string_index(panel->panel_of_node,
				"oplus,dsi-pinctrl-names", 1, &pinctrl_name);
		panel->oplus_panel.pinctrl_info.oplus_panel_suspend =
				pinctrl_lookup_state(panel->pinctrl.pinctrl, pinctrl_name);
		if (IS_ERR_OR_NULL(panel->oplus_panel.pinctrl_info.oplus_panel_suspend)) {
			rc = PTR_ERR(panel->oplus_panel.pinctrl_info.oplus_panel_suspend);
			OPLUS_DSI_ERR("[%s] failed to get pinctrl: %s, rc=%d\n",
					panel->oplus_panel.vendor_name, pinctrl_name, rc);
			goto error;
		}

		panel->oplus_panel.pinctrl_enabled = true;
		OPLUS_DSI_INFO("[%s] successfully init oplus panel pinctrl, rc=%d\n",
				panel->oplus_panel.vendor_name, rc);
	} else if (count >= 0) {
		OPLUS_DSI_ERR("[%s] invalid oplus,dsi-pinctrl-names, count=%d\n",
				panel->oplus_panel.vendor_name, count);
	}

error:
	return rc;
}

int oplus_panel_parse_power_sequence_config(struct dsi_panel *panel)
{
	struct dsi_parser_utils *utils = NULL;
	int temp = 0;
	int ret_of_power = 0;
	int ret;
	int rc = 0;
	int power_count = 0;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid panel\n");
		return -EINVAL;
	}

	utils = &panel->utils;
	if (!utils) {
		OPLUS_DSI_ERR("Invalid utils\n");
		return -EINVAL;
	}

	rc = utils->read_u32(utils->data,
				"oplus,panel-reset-position", &panel->oplus_panel.panel_reset_position);
	if (rc) {
		OPLUS_DSI_INFO("oplus,panel-reset-position is not config, set default value 0x00\n");
		panel->oplus_panel.panel_reset_position = PANEL_RESET_POSITION0;
	} else {
		if (panel->oplus_panel.panel_reset_position < PANEL_RESET_POSITION0 ||
				panel->oplus_panel.panel_reset_position > PANEL_RESET_POSITION2) {
			OPLUS_DSI_INFO("oplus,panel-reset-position = %d, but it only three values: 0x00, 0x01, 0x02, set default value 0x00\n",
				panel->oplus_panel.panel_reset_position);
			panel->oplus_panel.panel_reset_position = PANEL_RESET_POSITION0;
		}
	}

	const char *power_on_array[PANEL_POWER_SUPPLY_MAX * PANEL_POWER_SUPPLY_MESSAGE_MAX] = {0};
	ret_of_power = of_property_read_string_array(utils->data, "oplus,panel-power-on-sequence",
								power_on_array, PANEL_POWER_SUPPLY_MAX * PANEL_POWER_SUPPLY_MESSAGE_MAX);
	if (ret_of_power < 0) {
		OPLUS_DSI_ERR("failed to get the count of oplus,panel-power-on-sequence\n");
	} else if (ret_of_power > PANEL_POWER_SUPPLY_MAX*PANEL_POWER_SUPPLY_MESSAGE_MAX) {
		OPLUS_DSI_ERR("oplus,panel-power-on-sequence-string count not exceed\n");
		return -EINVAL;
	} else {
		power_count = (ret_of_power - 1) / 2;
		OPLUS_DSI_INFO("panel power on count = %d\n", power_count);
		panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_ORDER] = power_count;
		ret = kstrtoint(power_on_array[0], 10, &temp);
		if (ret) {
			OPLUS_DSI_ERR("%s cannot be converted to int", power_on_array[0]);
			return -EINVAL;
		}
		panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_POST_MS] = temp;
		OPLUS_DSI_INFO("the number of power on = %d\n",
						panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_ORDER]);
		OPLUS_DSI_INFO("pre delay time power on = %d\n",
						panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_POST_MS]);
		for (int i = 1; i < ret_of_power; i++) {
			if (i % 2 != 0) {
				if (!strcmp(power_on_array[i], "vddio")) {
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_VDDIO][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_on_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_on_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_VDDIO][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power on vddio post time = %d\n",
									panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_VDDIO][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else if (!strcmp(power_on_array[i], "vci")) {
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_VCI][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_on_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_on_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_VCI][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power on vci post time = %d\n",
									panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_VCI][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else if (!strcmp(power_on_array[i], "vdd")) {
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_VDD][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_on_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_on_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_VDD][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power on vdd post time = %d\n",
									panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_VDD][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else if (!strcmp(power_on_array[i], "gpio1")) {
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_GPIO1][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_on_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_on_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_GPIO1][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power on gpio1 post time = %d\n",
									panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_GPIO1][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else if (!strcmp(power_on_array[i], "gpio2")) {
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_GPIO2][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_on_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_on_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_GPIO2][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power on gpio2 post time = %d\n",
									panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_GPIO2][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else if (!strcmp(power_on_array[i], "gpio3")) {
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_GPIO3][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_on_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_on_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_GPIO3][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power on gpio3 post time = %d\n",
									panel->oplus_panel.power_on_sequence[PANEL_POWER_SUPPLY_GPIO3][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else {
					if(i%2 != 0)
						OPLUS_DSI_INFO("please check the power name, the %s is wrong, \
										the correct name is vddr, vci, vddio, gpio1, gpio2, gpio3\n", power_on_array[i]);
				}
			}
		}
		OPLUS_DSI_INFO("oplus,panel-power-on-sequence parsing success\n");
	}

	ret_of_power = 0;
	const char *power_off_array[PANEL_POWER_SUPPLY_MAX * PANEL_POWER_SUPPLY_MESSAGE_MAX] = {0};
	ret_of_power = of_property_read_string_array(utils->data, "oplus,panel-power-off-sequence",
												power_off_array, PANEL_POWER_SUPPLY_MAX * PANEL_POWER_SUPPLY_MESSAGE_MAX);
	if (ret_of_power < 0) {
		OPLUS_DSI_ERR("failed to get the count of oplus,panel-power-off-sequence\n");
	}
	else if (ret_of_power > PANEL_POWER_SUPPLY_MAX * PANEL_POWER_SUPPLY_MESSAGE_MAX) {
		OPLUS_DSI_ERR("oplus,panel-power-off-sequence count not exceed\n");
	} else {
		power_count = (ret_of_power - 1) / 2;
		OPLUS_DSI_INFO("panel power off node number = %d\n", power_count);
		panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_ORDER] = power_count;
		ret = kstrtoint(power_off_array[0], 10, &temp);
		if (ret) {
			OPLUS_DSI_ERR("%s cannot be converted to int", power_off_array[0]);
			return -EINVAL;
		}
		panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_POST_MS] = temp;

		OPLUS_DSI_INFO("the number of power off = %d\n",
						panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_ORDER]);
		OPLUS_DSI_INFO("pre delay time power off = %d\n",
						panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_POST_MS]);
		for(int i = 1; i < ret_of_power; i++) {
			if (i % 2 != 0) {
				if (!strcmp(power_off_array[i], "vddio")) {
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_VDDIO][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_off_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_off_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_VDDIO][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power off vddio post time = %d\n",
									panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_VDDIO][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else if (!strcmp(power_off_array[i], "vci")) {
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_VCI][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_off_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_off_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_VCI][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power off vci post time = %d\n",
									panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_VCI][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else if (!strcmp(power_off_array[i], "vdd")) {
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_VDD][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_off_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_off_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_VDD][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power off vdd post time = %d\n",
									panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_VDD][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else if (!strcmp(power_off_array[i], "gpio1")) {
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_GPIO1][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_off_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_off_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_GPIO1][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power off gpio1 post time = %d\n",
									panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_GPIO1][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else if (!strcmp(power_off_array[i], "gpio2")) {
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_GPIO2][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_off_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_off_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_GPIO2][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power off gpio2 post time = %d\n",
									panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_GPIO2][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else if (!strcmp(power_off_array[i], "gpio3")) {
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_GPIO3][PANEL_POWER_SUPPLY_ORDER] = (i+1)/2;
					ret = kstrtoint(power_off_array[i+1], 10, &temp);
					if (ret) {
						OPLUS_DSI_ERR("%s cannot be converted to int", power_off_array[i+1]);
						return -EINVAL;
					}
					panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_GPIO3][PANEL_POWER_SUPPLY_POST_MS] = temp;
					OPLUS_DSI_INFO("power off gpio3 post time = %d\n",
									panel->oplus_panel.power_off_sequence[PANEL_POWER_SUPPLY_GPIO3][PANEL_POWER_SUPPLY_POST_MS]);
					i+=1;
				} else {
					if(i%2 != 0)
						OPLUS_DSI_INFO("please check the power name, the %s is wrong, \
										the correct name is vddr, vci, vddio, gpio1, gpio2, gpio3\n", power_off_array[i]);
				}
			}
		}
		OPLUS_DSI_INFO("oplus,panel-power-off-sequence parsing success\n");
	}
	return 0;
}

int oplus_panel_enable_gpio(struct dsi_panel *panel, const char *gipo_name, bool enable)
{
	int rc = 0;
	struct oplus_gpio_config *gpio_cfg;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid panel\n");
		return -EINVAL;
	}

	gpio_cfg = &panel->oplus_panel.gpio_cfg;

	if(!strcmp(gipo_name, "panel_gpio1")
			&& gpio_is_valid(gpio_cfg->panel_gpio1)) {
		if (enable) {
			rc = gpio_direction_output(gpio_cfg->panel_gpio1, 1);
			if (rc)
				OPLUS_DSI_ERR("unable to set dir for panel_gpio1 rc=%d\n", rc);
			gpio_set_value(gpio_cfg->panel_gpio1, 1);
		} else {
			gpio_set_value(gpio_cfg->panel_gpio1, 0);
		}
	} else if (!strcmp(gipo_name, "panel_gpio2")
			&& gpio_is_valid(gpio_cfg->panel_gpio2)) {
		if (enable) {
			rc = gpio_direction_output(gpio_cfg->panel_gpio2, 1);
			if (rc)
				OPLUS_DSI_ERR("unable to set dir for panel_gpio2 rc=%d\n", rc);
			gpio_set_value(gpio_cfg->panel_gpio2, 1);
		} else {
			gpio_set_value(gpio_cfg->panel_gpio2, 0);
		}
	} else if (!strcmp(gipo_name, "panel_gpio3")
			&& gpio_is_valid(gpio_cfg->panel_gpio3)) {
		if (enable) {
			rc = gpio_direction_output(gpio_cfg->panel_gpio3, 1);
			if (rc)
				OPLUS_DSI_ERR("unable to set dir for panel_gpio3 rc=%d\n", rc);
			gpio_set_value(gpio_cfg->panel_gpio3, 1);
		} else {
			gpio_set_value(gpio_cfg->panel_gpio3, 0);
		}
	} else if (!strcmp(gipo_name, "pmic_gpio")
			&& gpio_is_valid(gpio_cfg->pmic_gpio)) {
		if (enable) {
			rc = gpio_direction_output(gpio_cfg->pmic_gpio, 1);
			if (rc)
				OPLUS_DSI_ERR("unable to set dir for pmic_gpio rc=%d\n", rc);
			gpio_set_value(gpio_cfg->pmic_gpio, 1);
		} else {
			gpio_set_value(gpio_cfg->pmic_gpio, 0);
		}
	} else {
		OPLUS_DSI_ERR("Invalid gpio name or gpio_is_invalid\n");
	}

	return rc;
}

int oplus_panel_enable_vregs(struct dsi_regulator_info *regs, const char *vreg_name, bool enable)
{
	int rc = 0;
	int i = 0;
	int num_of_v = 0;
	struct dsi_vreg *vreg = NULL;

	if (!regs) {
		OPLUS_DSI_ERR("Invalid dsi_regulator_info\n");
		return -EINVAL;
	}

	if (enable) {
		for (i = 0; i < regs->count; i++) {
			vreg = &regs->vregs[i];
			if (!strcmp(vreg->vreg_name, vreg_name)) {
				rc = regulator_set_load(vreg->vreg,
					vreg->enable_load);
				if (rc < 0) {
					OPLUS_DSI_ERR("Setting optimum mode failed for %s\n",
							vreg->vreg_name);
					return rc;
				}

				num_of_v = regulator_count_voltages(vreg->vreg);
				if (num_of_v > 0) {
					rc = regulator_set_voltage(vreg->vreg,
								   vreg->min_voltage,
								   vreg->max_voltage);
					if (rc) {
						OPLUS_DSI_ERR("Set voltage(%s) fail, rc=%d\n",
								vreg->vreg_name, rc);
						return rc;
					}
				}

				rc = regulator_enable(vreg->vreg);
				if (rc) {
					OPLUS_DSI_ERR("enable failed for %s, rc=%d\n",
							vreg->vreg_name, rc);
					return rc;
				}
				break;
			}
		}
	} else {
		for (i = 0; i < regs->count; i++) {
			vreg = &regs->vregs[i];
			if (!strcmp(vreg->vreg_name, vreg_name)) {
				(void)regulator_disable(regs->vregs[i].vreg);

				(void)regulator_set_load(regs->vregs[i].vreg,
							regs->vregs[i].disable_load);

				num_of_v = regulator_count_voltages(vreg->vreg);
				if (num_of_v > 0)
					(void)regulator_set_voltage(regs->vregs[i].vreg,
							regs->vregs[i].off_min_voltage,
							regs->vregs[i].max_voltage);
				break;
			}
		}
	}

	return 0;
}

int oplus_panel_enable_regulator(struct dsi_panel *panel, const char *vreg_name, bool enable)
{
	int rc = 0;
	struct dsi_regulator_info *regs = NULL;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid panel\n");
		return -EINVAL;
	}

	regs = &panel->power_info;
	if (!regs) {
		OPLUS_DSI_ERR("Invalid dsi_regulator_info\n");
		return -EINVAL;
	}

	if (regs->count == 0) {
		OPLUS_DSI_DEBUG("No valid regulators to enable\n");
		return 0;
	}

	if (enable) {
		rc = oplus_panel_enable_vregs(regs, vreg_name, true);
		if (rc) {
			DSI_ERR("failed to enable %s regulators\n", vreg_name);
		}
	} else {
		rc = oplus_panel_enable_vregs(regs, vreg_name, false);
		if (rc)
			OPLUS_DSI_ERR("failed to disable %s regulators\n", vreg_name);
	}

	return rc;
}

int oplus_panel_power_supply_enable(struct dsi_panel *panel)
{
	int rc = 0;
	int count = 0;
	unsigned int sleep_ms = 0;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid panel\n");
		return -EINVAL;
	}
	sleep_ms = panel->oplus_panel.power_on_sequence
			[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_POST_MS];
	usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
	for(count = 1; count <= panel->oplus_panel.power_on_sequence
			[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_ORDER]; count++) {
		if (count == panel->oplus_panel.power_on_sequence
				[PANEL_POWER_SUPPLY_VDDIO][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_regulator(panel, "vddio", true);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to enable vddio regulator, rc=%d\n",
						panel->name, rc);
				return rc;
			}
			sleep_ms = panel->oplus_panel.power_on_sequence
					[PANEL_POWER_SUPPLY_VDDIO][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("enable vddio\n");
		} else if (count == panel->oplus_panel.power_on_sequence
				[PANEL_POWER_SUPPLY_VCI][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_regulator(panel, "vci", true);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to enable vci regulator, rc=%d\n",
						panel->name, rc);
				return rc;
			}
			sleep_ms = panel->oplus_panel.power_on_sequence
					[PANEL_POWER_SUPPLY_VCI][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("enable vci\n");
		} else if (count == panel->oplus_panel.power_on_sequence
				[PANEL_POWER_SUPPLY_VDD][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_regulator(panel, "vdd", true);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to enable vdd regulator, rc=%d\n",
						panel->name, rc);
				return rc;
			}
			sleep_ms = panel->oplus_panel.power_on_sequence
					[PANEL_POWER_SUPPLY_VDD][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("enable vdd\n");
		} else if (count == panel->oplus_panel.power_on_sequence
				[PANEL_POWER_SUPPLY_GPIO1][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_gpio(panel, "panel_gpio1", true);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to enable panel_gpio1, rc=%d\n",
						panel->name, rc);
				return rc;
			}
			sleep_ms = panel->oplus_panel.power_on_sequence
					[PANEL_POWER_SUPPLY_GPIO1][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("enable panel_gpio1\n");
		} else if (count == panel->oplus_panel.power_on_sequence
				[PANEL_POWER_SUPPLY_GPIO2][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_gpio(panel, "panel_gpio2", true);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to enable panel_gpio2, rc=%d\n",
						panel->name, rc);
			}
			sleep_ms = panel->oplus_panel.power_on_sequence
					[PANEL_POWER_SUPPLY_GPIO2][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("enable panel_gpio2\n");
		} else if (count == panel->oplus_panel.power_on_sequence
				[PANEL_POWER_SUPPLY_GPIO3][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_gpio(panel, "panel_gpio3", true);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to enable panel_gpio3, rc=%d\n",
						panel->name, rc);
			}
			sleep_ms = panel->oplus_panel.power_on_sequence
					[PANEL_POWER_SUPPLY_GPIO3][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("enable panel_gpio3\n");
		}
	}

	return rc;
}

int oplus_panel_power_supply_disable(struct dsi_panel *panel)
{
	int rc = 0;
	int count;
	unsigned int sleep_ms = 0;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid panel\n");
		return -EINVAL;
	}
	sleep_ms = panel->oplus_panel.power_off_sequence
			[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_POST_MS];
	usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
	for(count = 1; count <= panel->oplus_panel.power_off_sequence
			[PANEL_POWER_SUPPLY_MESSAGE][PANEL_POWER_SUPPLY_ORDER]; count++) {
		if (count == panel->oplus_panel.power_off_sequence
				[PANEL_POWER_SUPPLY_VDDIO][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_regulator(panel, "vddio", false);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to disable vddio regulator, rc=%d\n",
						panel->name, rc);
				return rc;
			}
			sleep_ms = panel->oplus_panel.power_off_sequence
					[PANEL_POWER_SUPPLY_VDDIO][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("disable vddio\n");
		} else if (count == panel->oplus_panel.power_off_sequence
				[PANEL_POWER_SUPPLY_VCI][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_regulator(panel, "vci", false);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to disable vci regulator, rc=%d\n",
						panel->name, rc);
				return rc;
			}
			sleep_ms = panel->oplus_panel.power_off_sequence
					[PANEL_POWER_SUPPLY_VCI][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("disable vci\n");
		} else if (count == panel->oplus_panel.power_off_sequence
				[PANEL_POWER_SUPPLY_VDD][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_regulator(panel, "vdd", false);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to disable vdd regulator, rc=%d\n",
						panel->name, rc);
				return rc;
			}
			sleep_ms = panel->oplus_panel.power_off_sequence
					[PANEL_POWER_SUPPLY_VDD][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("disable vdd\n");
		} else if (count == panel->oplus_panel.power_off_sequence
				[PANEL_POWER_SUPPLY_GPIO1][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_gpio(panel, "panel_gpio1", false);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to disable panel_gpio1, rc=%d\n",
						panel->name, rc);
				return rc;
			}
			sleep_ms = panel->oplus_panel.power_off_sequence
					[PANEL_POWER_SUPPLY_GPIO1][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("disable panel_gpio1\n");
		} else if (count == panel->oplus_panel.power_off_sequence
				[PANEL_POWER_SUPPLY_GPIO2][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_gpio(panel, "panel_gpio2", false);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to disable panel_gpio2, rc=%d\n",
						panel->name, rc);
			}
			sleep_ms = panel->oplus_panel.power_off_sequence
					[PANEL_POWER_SUPPLY_GPIO2][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("disable panel_gpio2\n");
		} else if (count == panel->oplus_panel.power_off_sequence
				[PANEL_POWER_SUPPLY_GPIO3][PANEL_POWER_SUPPLY_ORDER]) {
			rc = oplus_panel_enable_gpio(panel, "panel_gpio3", false);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed to disable panel_gpio3, rc=%d\n",
						panel->name, rc);
			}
			sleep_ms = panel->oplus_panel.power_off_sequence
					[PANEL_POWER_SUPPLY_GPIO3][PANEL_POWER_SUPPLY_POST_MS];
			usleep_range(sleep_ms*1000, (sleep_ms*1000)+100);
			OPLUS_DSI_DEBUG("disable panel_gpio3\n");
		}
	}

	return rc;
}

int oplus_panel_power_on(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid panel\n");
		return -EINVAL;
	}

	OPLUS_DSI_INFO("oplus_panel_power_on");

	oplus_pwm_set_power_on(panel);

	rc = dsi_panel_set_pinctrl_state(panel, true);
	if (rc) {
		OPLUS_DSI_ERR("[%s] failed to set pinctrl, rc=%d\n", panel->name, rc);
		goto error_disable_pinctrl;
	}

#ifdef OPLUS_FEATURE_TP_BASIC
	if (oplus_display_notify_tp_ops.tp_panel_power_on_supply) {
		if (oplus_display_notify_tp_ops.tp_panel_power_on_supply(panel)) {
			if(panel->power_info.refcount == 0) {
				rc = oplus_panel_power_supply_enable(panel);
				if (rc) {
					OPLUS_DSI_ERR("[%s] failed set power supply enable, rc=%d\n", panel->name, rc);
					goto error_disable_supply;
				}
			}
			panel->power_info.refcount++;

			if(panel->oplus_panel.bl_ic_ktz8868_used) {
				rc = oplus_bl_ic_ktz8868_power_on(panel);
				if (rc) {
					OPLUS_DSI_ERR("[%s] failed to set ktz8868 on!, rc=%d\n", panel->name, rc);
					goto error_disable_pinctrl;
				}
			}
		}
	}
#else /* OPLUS_FEATURE_TP_BASIC */
	if(panel->power_info.refcount == 0) {
		rc = oplus_panel_power_supply_enable(panel);
		if (rc) {
			OPLUS_DSI_ERR("[%s] failed set power supply enable, rc=%d\n", panel->name, rc);
			goto error_disable_supply;
		}
	}
	panel->power_info.refcount++;

	if(panel->oplus_panel.bl_ic_ktz8868_used) {
		rc = oplus_bl_ic_ktz8868_power_on(panel);
		if (rc) {
			OPLUS_DSI_ERR("[%s] failed to set ktz8868 on!, rc=%d\n", panel->name, rc);
			goto error_disable_pinctrl;
		}
	}
#endif /* OPLUS_FEATURE_TP_BASIC */

	if (panel->oplus_panel.panel_reset_position == PANEL_RESET_POSITION2) {
		return 0;
	} else {
		rc = dsi_panel_reset(panel);
		if (rc) {
			DSI_ERR("[%s] failed to reset panel, rc=%d\n", panel->name, rc);
			goto error_disable_gpio;
		}
	}

	goto exit;

error_disable_gpio:
	if (gpio_is_valid(panel->reset_config.disp_en_gpio))
		gpio_set_value(panel->reset_config.disp_en_gpio, 0);

	if (gpio_is_valid(panel->bl_config.en_gpio))
		gpio_set_value(panel->bl_config.en_gpio, 0);

error_disable_supply:
	(void) oplus_panel_power_supply_disable(panel);

error_disable_pinctrl:
	(void)dsi_panel_set_pinctrl_state(panel, false);

exit:
	return rc;
}

int oplus_panel_power_off(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid panel\n");
		return -EINVAL;
	}

	OPLUS_DSI_INFO("oplus_panel_power_off");

#ifdef OPLUS_FEATURE_TP_BASIC
	if (oplus_display_notify_tp_ops.tp_panel_power_off_supply) {
		if (oplus_display_notify_tp_ops.tp_panel_power_off_supply(panel)) {
			if (panel->power_info.refcount == 0) {
					OPLUS_DSI_ERR("Unbalanced regulator off:%s\n",
							panel->power_info.vregs->vreg_name);
			} else {
				panel->power_info.refcount--;
				if (panel->power_info.refcount == 0) {
					rc = oplus_panel_power_supply_disable(panel);
					if (rc) {
						OPLUS_DSI_ERR("[%s] failed set power supply disable, rc=%d\n", panel->name, rc);
					}
				}
			}
		}
	}
#else /* OPLUS_FEATURE_TP_BASIC */
	if (panel->power_info.refcount == 0) {
			OPLUS_DSI_ERR("Unbalanced regulator off:%s\n",
					panel->power_info.vregs->vreg_name);
	} else {
		panel->power_info.refcount--;
		if (panel->power_info.refcount == 0) {
			rc = oplus_panel_power_supply_disable(panel);
			if (rc) {
				OPLUS_DSI_ERR("[%s] failed set power supply disable, rc=%d\n", panel->name, rc);
			}
		}
	}
#endif /* OPLUS_FEATURE_TP_BASIC */

	rc = dsi_panel_set_pinctrl_state(panel, false);
	if (rc) {
		OPLUS_DSI_ERR("[%s] failed set pinctrl state, rc=%d\n", panel->name, rc);
	}

	return rc;
}

int oplus_panel_prepare(struct dsi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid panel\n");
		return -EINVAL;
	}

	if (panel->oplus_panel.panel_reset_position == PANEL_RESET_POSITION1) {
		rc = oplus_panel_power_on(panel);
		if (rc) {
			OPLUS_DSI_ERR("[%s] panel power on failed, rc=%d\n",
					panel->name, rc);
		}
	} else if (panel->oplus_panel.panel_reset_position == PANEL_RESET_POSITION2) {
		rc = dsi_panel_reset(panel);
		if (rc) {
			OPLUS_DSI_ERR("[%s] panel reset failed, rc=%d\n",
					panel->name, rc);
		}
#ifdef OPLUS_FEATURE_TP_BASIC
		/* notify tp load fw after lcd reset */
		if (oplus_display_notify_tp_ops.tp_panel_power_on_load_fw) {
			oplus_display_notify_tp_ops.tp_panel_power_on_load_fw(panel);
		}
#endif /* OPLUS_FEATURE_TP_BASIC */
	}

	return rc;
}

bool oplus_panel_pre_prepare(struct dsi_panel *panel)
{
	if (panel->oplus_panel.need_trigger_event) {
		oplus_panel_event_data_notifier_trigger(panel, DRM_PANEL_EVENT_UNBLANK, 0, true);
	}

	panel->oplus_panel.power_mode_early = SDE_MODE_DPMS_ON;

	if (panel->oplus_panel.panel_reset_position == PANEL_RESET_POSITION1) {
		return true;
	} else {
		return false;
	}
}

int oplus_panel_charger_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct dsi_display *display;
	struct dsi_panel *panel;
	struct power_supply *psy = v;
	union power_supply_propval val;
	static union power_supply_propval last_val;
	int ret;

	display = get_main_display();
	if (!display) {
		OPLUS_DSI_ERR("main display is NULL\n");
		return NOTIFY_DONE;
	}

	panel = display->panel;
	if (!panel) {
		OPLUS_DSI_ERR("main panel is NULL\n");
		return NOTIFY_DONE;
	}

	if (strcmp(psy->desc->name, "battery") == 0) {
		ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_STATUS , &val);
		if (ret) {
			OPLUS_DSI_ERR("failed to get supply status, ret=%d \n", ret);
		} else {
			OPLUS_DSI_DEBUG("supply status is %d \n", val.intval);
			if ((val.intval != POWER_SUPPLY_STATUS_UNKNOWN) && (val.intval != last_val.intval)) {
				switch (val.intval) {
				case POWER_SUPPLY_STATUS_CHARGING:
				case POWER_SUPPLY_STATUS_DISCHARGING:
					OPLUS_DSI_INFO("supply status is %d, enable pmic gpio\n", val.intval);
					ret = oplus_panel_enable_gpio(panel, "pmic_gpio", true);
					if (ret) {
						OPLUS_DSI_ERR("[%s] failed to enable pmic_gpio, ret=%d\n",
								panel->name, ret);
					}
					break;
				case POWER_SUPPLY_STATUS_NOT_CHARGING:
				case POWER_SUPPLY_STATUS_FULL:
					OPLUS_DSI_INFO("supply status is %d, disable pmic gpio\n", val.intval);
					ret = oplus_panel_enable_gpio(panel, "pmic_gpio", false);
					if (ret) {
						OPLUS_DSI_ERR("[%s] failed to disable pmic_gpio, ret=%d\n",
								panel->name, ret);
					}
					break;
				default:
					break;
				}

				last_val = val;
			}
		}
	}

	return NOTIFY_DONE;
}

void oplus_panel_register_supply_notifier(void)
{
	psy_nb.notifier_call = oplus_panel_charger_psy_event;
	power_supply_reg_notifier(&psy_nb);
	OPLUS_DSI_INFO("successfully register supply notifier\n");

	return;
}

int oplus_bl_ic_ktz8868_power_on(struct dsi_panel *panel)
{
	int rc = 0;

	/* add for ktz8866 poweron */
	rc = bl_ic_ktz8868_hw_en(true);
	if (rc) {
		DSI_ERR("[%s] failed to bl_ic_ktz8866_hw_en, rc=%d\n",
			panel->name, rc);
		return rc;
	}

	rc = bl_ic_ktz8868_set_lcd_bias_by_gpio(true);
	if (rc) {
		DSI_ERR("[%s] failed to lcd_set_bias, rc=%d\n",
			panel->name, rc);
		return rc;
	}
	usleep_range(10*1000, (10*1000)+100);
	return rc;
}

void oplus_bl_ic_ktz8868_power_off(struct dsi_panel *panel)
{
	int rc = 0;

	rc = bl_ic_ktz8868_set_lcd_bias_by_gpio(false);
	if (rc) {
		DSI_ERR("[%s] failed to lcd_set_bias, rc=%d\n",
			panel->name, rc);
	}

	/* add for ktz8866 poweroff */
	rc = bl_ic_ktz8868_hw_en(false);
	if (rc) {
		DSI_ERR("[%s] failed to bl_ic_ktz8866_hw_en, rc=%d\n",
			panel->name, rc);
	}
}

