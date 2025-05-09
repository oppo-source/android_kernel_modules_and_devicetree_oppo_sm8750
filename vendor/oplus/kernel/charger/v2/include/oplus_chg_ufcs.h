// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023-2023 Oplus. All rights reserved.
 */

#ifndef __OPLUS_CHG_UFCS_H__
#define __OPLUS_CHG_UFCS_H__

#include <oplus_mms.h>

enum ufcs_topic_item {
	UFCS_ITEM_ONLINE,
	UFCS_ITEM_CHARGING,
	UFCS_ITEM_ADAPTER_ID,
	UFCS_ITEM_OPLUS_ADAPTER,
	UFCS_ITEM_BCC_MAX_CURR,
	UFCS_ITEM_BCC_MIN_CURR,
	UFCS_ITEM_BCC_EXIT_CURR,
	UFCS_ITEM_BCC_TEMP_RANGE,
	UFCS_ITEM_SLOW_CHG_BATT_LIMIT,
	UFCS_ITEM_UFCS_VID,
};

enum ufcs_fastchg_type {
	UFCS_FASTCHG_TYPE_UNKOWN,
	UFCS_FASTCHG_TYPE_THIRD = 0x8211,
	UFCS_FASTCHG_TYPE_V1 = 0x4211,
	UFCS_FASTCHG_TYPE_V2,
	UFCS_FASTCHG_TYPE_V3,
	UFCS_FASTCHG_TYPE_OTHER,
};

enum ufcs_power_type {
	UFCS_POWER_TYPE_UNKOWN = 18,
	UFCS_POWER_TYPE_V0 = 44,
	UFCS_POWER_TYPE_V1 = 45,
	UFCS_POWER_TYPE_V2 = 55,
	UFCS_POWER_TYPE_V3 = 67,
	UFCS_POWER_TYPE_V4 = 80,
	UFCS_POWER_TYPE_V5 = 88,
	UFCS_POWER_TYPE_V6 = 100,
	UFCS_POWER_TYPE_V7 = 120,
	UFCS_POWER_TYPE_V8 = 150,
	UFCS_POWER_TYPE_V9 = 240,
	UFCS_POWER_TYPE_MAX = 999,
};

enum ufcs_curr_table_type {
	UFCS_CURR_BIDIRECT_TABLE = 0,
	UFCS_CURR_CP_TABLE = 1,
};

enum ufcs_user_err_type {
	UFCS_ERR_IBUS_LIMIT = 1,
	UFCS_ERR_CP_ENABLE,
	UFCS_ERR_R_COOLDOWN,
	UFCS_ERR_BATT_BTB_COOLDOWN,
	UFCS_ERR_IBAT_OVER,
	UFCS_ERR_BTB_OVER,
	UFCS_ERR_MOS_OVER,
	UFCS_ERR_USBTEMP_OVER,
	UFCS_ERR_TFG_OVER,
	UFCS_ERR_VBAT_DIFF,
	UFCS_ERR_STARTUP_FAIL,
	UFCS_ERR_CIRCUIT_SWITCH,
	UFCS_ERR_ANTHEN_ERR,
	UFCS_ERR_PDO_ERR,
	UFCS_ERR_IMP,
	UFCS_ERR_MAX,
};

int oplus_ufcs_current_to_level(struct oplus_mms *topic, int ibus_curr);
enum fastchg_protocol_type oplus_ufcs_adapter_id_to_protocol_type(u32 id);
int oplus_ufcs_get_ufcs_power(struct oplus_mms *topic);
int oplus_ufcs_get_curve_ibus(struct oplus_mms *mms);
int oplus_ufcs_level_to_current(struct oplus_mms *mms, int cool_down);
#endif /* __OPLUS_CHG_UFCS_H__ */
