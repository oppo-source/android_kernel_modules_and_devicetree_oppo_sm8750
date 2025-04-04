// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022-2023 Oplus. All rights reserved.
 */

static const char * const strategy_soc[];
static const char * const strategy_temp[];

struct batt_sys_curves *g_svooc_curves_metadata[] = {
	svooc_curves_soc0_2_50,
	svooc_curves_soc0_2_50_mid,
	svooc_curves_soc0_2_50_high,
	svooc_curves_soc50_2_75,
	svooc_curves_soc75_2_85,
	svooc_curves_soc85_2_90
};

struct batt_sys_curves *g_vooc_curves_metadata[] = {
	vooc_curves_soc0_2_50,
	vooc_curves_soc0_2_50_mid,
	vooc_curves_soc0_2_50_high,
	vooc_curves_soc50_2_75,
	vooc_curves_soc75_2_85,
	vooc_curves_soc85_2_90
};

#define NAME_BUF_MAX 256
static int oplus_get_strategy_data(
	struct oplus_param_head *param_head, const char *name, struct batt_sys_curves *metadata[])
{
	struct oplus_cfg_data_head *data_head;
	ssize_t data_len;
	const int range_data_size = sizeof(struct batt_sys_curve);
	char *tmp_buf;
	char name_buf[NAME_BUF_MAX];
	int curv_num[BATT_SOC_90_TO_100][BATT_SYS_CURVE_MAX] = { 0 };
	bool skip[BATT_SOC_90_TO_100][BATT_SYS_CURVE_MAX] = { 0 };
	int i, j;
	int rc = 0;
	int m;
	struct batt_sys_curve *tmp;

	tmp_buf = kzalloc(BATT_SOC_90_TO_100 * BATT_SYS_CURVE_MAX * BATT_SYS_ROW_MAX * range_data_size, GFP_KERNEL);
	if (!tmp_buf) {
		chg_err("fastchg strategy alloc data buf error\n");
		return -ENOMEM;
	}

	for (i = 0; i < BATT_SOC_90_TO_100; i++) {
		for (j = 0; j < BATT_SYS_CURVE_MAX; j++) {
			memset(name_buf, 0, NAME_BUF_MAX);
			snprintf(name_buf, NAME_BUF_MAX, "%s:%s:%s", name, strategy_soc[i], strategy_temp[j]);
			data_head = oplus_cfg_find_param_by_name(param_head, name_buf);
			if (data_head == NULL) {
				rc = -ENODATA;
				chg_info("%s not found\n", name_buf);
				skip[i][j] = true;
				continue;
			}
			data_len = oplus_cfg_get_data_size(data_head);
			if (data_len < 0 || data_len % range_data_size) {
				chg_err("%s data len error, data_len=%ld\n", name_buf, data_len);
				rc = -EINVAL;
				goto err;
			}
			rc = oplus_cfg_get_data(
				data_head,
				tmp_buf + (BATT_SYS_CURVE_MAX * i + j) * BATT_SYS_ROW_MAX * range_data_size,
				data_len);
			if (rc < 0) {
				chg_err("get %s data buf error, rc=%d\n",
					name_buf, rc);
				goto err;
			}
			curv_num[i][j] = data_len / range_data_size;
		}
	}

	for (i = 0; i < BATT_SOC_90_TO_100; i++) {
		for (j = 0; j < BATT_SYS_CURVE_MAX; j++) {
			if (skip[i][j])
				continue;
			memcpy(metadata[i][j].batt_sys_curve,
			       tmp_buf + (BATT_SYS_CURVE_MAX * i + j) * BATT_SYS_ROW_MAX * range_data_size,
			       BATT_SYS_ROW_MAX * range_data_size);
			metadata[i][j].sys_curv_num = curv_num[i][j];

			tmp = metadata[i][j].batt_sys_curve;
			for (m = 0; m < curv_num[i][j]; m++)
				chg_info("[TEST]:%s:%s:%s[%d]: %u %u %d %u %u\n",
					 name,
					 strategy_soc[i],
					 strategy_temp[j], m,
					 tmp[m].target_vbat,
					 tmp[m].target_ibus,
					 tmp[m].exit,
					 tmp[m].target_time,
					 tmp[m].chg_time);
		}
	}

err:
	kfree(tmp_buf);
	return rc;
}

static void oplus_voocphy_update_lcf_curves_strategy_config(
	struct oplus_param_head *param_head, struct oplus_voocphy_manager *chip)
{
	struct oplus_chg_strategy *strategy;

	strategy = oplus_chg_strategy_alloc_by_param_head("low_curr_full_strategy", "svooc_lcf_strategy", param_head);
	if (IS_ERR_OR_NULL(strategy)) {
		chg_err("alloc svooc_lcf_strategy error, rc=%ld", PTR_ERR(strategy));
	} else {
		(void)oplus_chg_strategy_release(chip->svooc_lcf_strategy[0]);
		chip->svooc_lcf_strategy[0] = strategy;
		chg_info("[TEST]: update svooc_lcf_strategy startegy\n");
	}
}

static int oplus_voocphy_update_config(void *data, struct oplus_param_head *param_head)
{
	struct oplus_voocphy_manager *chip;

	if (data == NULL) {
		chg_err("data is NULL\n");
		return -EINVAL;
	}
	if (param_head == NULL) {
		chg_err("param_head is NULL\n");
		return -EINVAL;
	}
	chip = (struct oplus_voocphy_manager *)data;

	(void)oplus_get_strategy_data(param_head, "vooc_charge_strategy", g_vooc_curves_metadata);
	(void)oplus_get_strategy_data(param_head, "svooc_charge_strategy", g_svooc_curves_metadata);
	oplus_voocphy_update_lcf_curves_strategy_config(param_head, chip);

	return 0;
}

static int oplus_voocphy_reg_debug_config(struct oplus_voocphy_manager *chip)
{
	int rc;

	chip->debug_cfg.type = OPLUS_CHG_VOOCPHY_PARAM;
	chip->debug_cfg.update = oplus_voocphy_update_config;
	chip->debug_cfg.priv_data = chip;
	rc = oplus_cfg_register(&chip->debug_cfg);
	if (rc < 0)
		chg_err("spec cfg register error, rc=%d\n", rc);

	return rc;
}

__maybe_unused static void oplus_voocphy_unreg_debug_config(struct oplus_voocphy_manager *chip)
{
	oplus_cfg_unregister(&chip->debug_cfg);
}
