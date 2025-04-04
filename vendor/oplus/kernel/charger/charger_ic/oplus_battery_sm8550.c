// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 Oplus. All rights reserved.
 */

#define pr_fmt(fmt)	"BATTERY_CHG: %s: " fmt, __func__

#ifdef OPLUS_FEATURE_CHG_BASIC
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/sched/clock.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/iio/consumer.h>
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
#include <soc/oplus/system/oplus_project.h>
#endif
#include <linux/remoteproc/qcom_rproc.h>
#include <linux/rtc.h>
#include <linux/device.h>
#include "oplus_battery_sm8550.h"
#include "../oplus_charger.h"
#include "../oplus_gauge.h"
#include "../oplus_vooc.h"
#include "../oplus_short.h"
#include "../oplus_wireless.h"
#include "../charger_ic/oplus_short_ic.h"
#include "../oplus_adapter.h"
#include "../oplus_configfs.h"
#include "../gauge_ic/oplus_bq27541.h"
#include "oplus_da9313.h"
#include "op_charge.h"
#include "../wireless_ic/oplus_nu1619.h"
#include "../oplus_debug_info.h"
#include "oplus_mp2650.h"
#include "../oplus_chg_ops_manager.h"
#include "../oplus_chg_module.h"
#include "../oplus_chg_comm.h"
#include "../op_wlchg_v2/oplus_chg_wls.h"
#include "../voocphy/oplus_adsp_voocphy.h"
#include "../oplus_pps.h"
#include "../oplus_ufcs.h"
#include "oplus_quirks.h"
#include "../voocphy/oplus_voocphy.h"
#include "../op_wlchg_v2/hal/wls_chg_intf.h"
#include "../chargepump_ic/oplus_pps_cp.h"
#include "../chargepump_ic/oplus_sc8571.h"
#include "../oplus_pps_ops_manager.h"
#include "../oplus_chg_track.h"
#include "../voocphy/oplus_sc8517.h"
#include "../v1/bob_ic/oplus_tps6128xd.h"
#ifdef CONFIG_DISABLE_OPLUS_FUNCTION
bool __attribute__((weak)) qpnp_is_power_off_charging(void)
{
	return false;
}
#endif

#define OPLUS_USBTEMP_HIGH_CURR 1
#define OPLUS_USBTEMP_LOW_CURR 0
#define OPLUS_USBTEMP_CURR_CHANGE_TEMP 3
#define OPLUS_USBTEMP_CHANGE_RANGE_TIME 30

#define OPLUS_PLUGIN_CNT_DETECT_INTERVAL round_jiffies_relative(msecs_to_jiffies(5000))
#define OPLUS_PD_TYPE_CHECK_INTERVAL round_jiffies_relative(msecs_to_jiffies(1500))
#define OPLUS_HVDCP_DISABLE_INTERVAL round_jiffies_relative(msecs_to_jiffies(15000))
#define OPLUS_HVDCP_DETECT_TO_DETACH_TIME 3500
#define CPU_CLOCK_TIME_MS	1000000
#define OEM_MISC_CTL_DATA_PAIR(cmd, enable) ((enable ? 0x3 : 0x1) << cmd)
#define FLASH_SCREEN_CTRL_OTA		0X01
#define FLASH_SCREEN_CTRL_DTSI	0X02
#define LCM_CHECK_COUNT  5
#define CHG_OPS_LEN 64
#define SWITCH_TO_NORMAL_DELAY_MS 500

struct oplus_chg_chip *g_oplus_chip = NULL;
static struct task_struct *oplus_usbtemp_kthread;
struct wakeup_source *usbtemp_wakelock;
static bool probe_done;
#ifdef WLS_QI_DEBUG
static int wls_dbg_icl_ma = 0;
static int wls_dbg_fcc_ma = 0;
#endif

static int usbtemp_dbg_tempr = 0;
module_param(usbtemp_dbg_tempr, int, 0644);
MODULE_PARM_DESC(usbtemp_dbg_tempr, "debug usbtemp temp r");

static int usbtemp_dbg_templ = 0;
module_param(usbtemp_dbg_templ, int, 0644);
MODULE_PARM_DESC(usbtemp_dbg_templ, "debug usbtemp temp l");

static int usbtemp_dbg_curr_status = -1;
module_param(usbtemp_dbg_curr_status, int, 0644);
MODULE_PARM_DESC(usbtemp_dbg_curr_status, "debug usbtemp current status");

static int plugin_cnt_test = 0;
module_param(plugin_cnt_test, int, 0644);
MODULE_PARM_DESC(plugin_cnt_test, "add for test plugin/out ap cnt(rev) and adsp cnt(snd) consistency");

static int usbin_status_test = 0;
module_param(usbin_status_test, int, 0644);
MODULE_PARM_DESC(usbin_status_test, "usbin status test");
#if IS_MODULE(CONFIG_OPLUS_FEATURE_OPLUSBOOT)
#define MAX_CMDLINE_PARAM_LEN 1024
extern char bootmode[];
static bool is_charger_boot_mode(void)
{
	char charger_mode[MAX_CMDLINE_PARAM_LEN + 1];

	if (strlen(bootmode) <= MAX_CMDLINE_PARAM_LEN) {
		strcpy(charger_mode, bootmode);
		charger_mode[strlen(bootmode)] = '\0';
		if (!strncmp(charger_mode, "charger", strlen("charger")))
			return true;
	}
	return false;
}
#else
static bool is_charger_boot_mode(void)
{
	return false;
}
#endif

static int oplus_get_ibus_current(void);
int qpnp_get_prop_charger_voltage_now(void);
bool oplus_ccdetect_check_is_gpio(struct oplus_chg_chip *chip);
int oplus_adsp_voocphy_enable(bool enable);
int oplus_adsp_voocphy_reset_again(void);
static void oplus_adsp_voocphy_status_func(struct work_struct *work);
int oplus_get_otg_online_status(void);
int oplus_set_otg_switch_status(bool enable);
static int oplus_otg_ap_enable(bool enable);
int oplus_chg_get_charger_subtype(void);
static int oplus_get_vchg_trig_status(void);
static bool oplus_vchg_trig_is_support(void);
void oplus_wake_up_usbtemp_thread(void);
bool oplus_chg_is_usb_present(void);
static int oplus_usbtemp_adc_gpio_dt(struct oplus_chg_chip *chip);
static int oplus_subboard_temp_gpio_init(struct oplus_chg_chip *chip);
static int oplus_btb_temp_gpio_init(struct oplus_chg_chip *chip);
static int oplus_mos_btb_gpio_init(struct oplus_chg_chip *chip);
static int oplus_ufcs_mos_switch_gpio_init(struct oplus_chg_chip *chip);
int oplus_get_usb_status(void);
int oplus_adsp_voocphy_get_enable(void);
static void oplus_usbtemp_recover_func(struct oplus_chg_chip *chip);
int oplus_tbatt_power_off_task_init(struct oplus_chg_chip *chip);
static int smbchg_usb_suspend_enable(void);
static void oplus_set_otg_boost_en_val(int value);
static void oplus_set_otg_ovp_en_val(int value);
/*extern void oplus_dwc3_config_usbphy_pfunc(bool (*pfunc)(void));*/
extern void oplus_usb_set_none_role(void);
static int fg_bq27541_get_average_current(void);
static int oplus_ap_init_adsp_gague(void);
bool oplus_get_pps_type(void);
static void oplus_otg_status_check_work(struct work_struct *work);
extern int oplus_chg_get_curr_time_ms(unsigned long *time_ms);
static int oplus_chg_track_upload_icl_err_info(
	struct battery_chg_dev *bcdev, int err_type);
static bool oplus_chg_wls_support_bcc(struct oplus_chg_chip *chip);
static int oplus_chg_track_upload_adsp_err_info(
	struct battery_chg_dev *bcdev, int err_type);
int oplus_get_otg_online_status_with_cid_scheme(void);

/*extern void oplus_usb_set_none_role(void);*/
#if defined(OPLUS_FEATURE_POWERINFO_FTM) && defined(CONFIG_OPLUS_POWERINFO_FTM)
extern bool ext_boot_with_console(void);
#endif
static int smbchg_get_charge_enable(void);
#endif /*OPLUS_FEATURE_CHG_BASIC*/

#ifdef OPLUS_FEATURE_CHG_BASIC
/*for p922x compile*/
void __attribute__((weak)) oplus_set_wrx_otg_value(void)
{
	return;
}
int __attribute__((weak)) oplus_get_idt_en_val(void)
{
	return -1;
}
int __attribute__((weak)) oplus_get_wrx_en_val(void)
{
	return -1;
}
int __attribute__((weak)) oplus_get_wrx_otg_val(void)
{
	return 0;
}
void __attribute__((weak)) oplus_wireless_set_otg_en_val(void)
{
	return;
}
void __attribute__((weak)) oplus_dcin_irq_enable(void)
{
	return;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static struct timespec current_kernel_time(void)
{
	struct timespec ts;

	getnstimeofday(&ts);
	return ts;
}
#endif

__maybe_unused static bool is_wls_ocm_available(struct oplus_chg_chip *chip)
{
	if (!chip->wls_ocm)
		chip->wls_ocm = oplus_chg_mod_get_by_name("wireless");
	return !!chip->wls_ocm;
}

__maybe_unused static bool is_comm_ocm_available(struct oplus_chg_chip *chip)
{
	if (!chip->comm_ocm)
		chip->comm_ocm = oplus_chg_mod_get_by_name("common");
	return !!chip->comm_ocm;
}

#endif /*OPLUS_FEATURE_CHG_BASIC*/

static const int battery_prop_map[BATT_PROP_MAX] = {
	[BATT_STATUS]		= POWER_SUPPLY_PROP_STATUS,
	[BATT_HEALTH]		= POWER_SUPPLY_PROP_HEALTH,
	[BATT_PRESENT]		= POWER_SUPPLY_PROP_PRESENT,
	[BATT_CHG_TYPE]		= POWER_SUPPLY_PROP_CHARGE_TYPE,
	[BATT_CAPACITY]		= POWER_SUPPLY_PROP_CAPACITY,
	[BATT_VOLT_OCV]		= POWER_SUPPLY_PROP_VOLTAGE_OCV,
	[BATT_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[BATT_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[BATT_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[BATT_CHG_CTRL_LIM]	= POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	[BATT_CHG_CTRL_LIM_MAX]	= POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	[BATT_TEMP]		= POWER_SUPPLY_PROP_TEMP,
	[BATT_TECHNOLOGY]	= POWER_SUPPLY_PROP_TECHNOLOGY,
	[BATT_CHG_COUNTER]	= POWER_SUPPLY_PROP_CHARGE_COUNTER,
	[BATT_CYCLE_COUNT]	= POWER_SUPPLY_PROP_CYCLE_COUNT,
	[BATT_CHG_FULL_DESIGN]	= POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	[BATT_CHG_FULL]		= POWER_SUPPLY_PROP_CHARGE_FULL,
	[BATT_MODEL_NAME]	= POWER_SUPPLY_PROP_MODEL_NAME,
	[BATT_TTF_AVG]		= POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	[BATT_TTE_AVG]		= POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	[BATT_POWER_NOW]	= POWER_SUPPLY_PROP_POWER_NOW,
	[BATT_POWER_AVG]	= POWER_SUPPLY_PROP_POWER_AVG,
};

static const int usb_prop_map[USB_PROP_MAX] = {
	[USB_ONLINE]		= POWER_SUPPLY_PROP_ONLINE,
	[USB_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[USB_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[USB_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[USB_CURR_MAX]		= POWER_SUPPLY_PROP_CURRENT_MAX,
	[USB_INPUT_CURR_LIMIT]	= POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	[USB_ADAP_TYPE]		= POWER_SUPPLY_PROP_USB_TYPE,
	[USB_TEMP]		= POWER_SUPPLY_PROP_TEMP,
};

static const int wls_prop_map[WLS_PROP_MAX] = {
	[WLS_ONLINE]		= POWER_SUPPLY_PROP_ONLINE,
	[WLS_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[WLS_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[WLS_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[WLS_CURR_MAX]		= POWER_SUPPLY_PROP_CURRENT_MAX,
	[WLS_INPUT_CURR_LIMIT]	= POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	[WLS_CONN_TEMP]		= POWER_SUPPLY_PROP_TEMP,
};

/* Standard usb_type definitions similar to power_supply_sysfs.c */
static const char * const power_supply_usb_type_text[] = {
	"Unknown", "SDP", "DCP", "CDP", "ACA", "C",
	"PD", "PD_DRP", "PD_PPS", "BrickID"
};

/* Custom usb_type definitions */
static const char * const qc_power_supply_usb_type_text[] = {
	"HVDCP", "HVDCP_3", "HVDCP_3P5"
};

/* wireless_type definitions */
static const char * const qc_power_supply_wls_type_text[] = {
	"Unknown", "BPP", "EPP", "HPP"
};

static RAW_NOTIFIER_HEAD(hboost_notifier);

int register_hboost_event_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&hboost_notifier, nb);
}
EXPORT_SYMBOL(register_hboost_event_notifier);

int unregister_hboost_event_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&hboost_notifier, nb);
}
EXPORT_SYMBOL(unregister_hboost_event_notifier);

#ifdef OPLUS_FEATURE_CHG_BASIC
static bool is_ext_chg_ops(void)
{
	return (strncmp(oplus_chg_ops_name_get(), "plat-pmic", 64));
}

static bool is_ext_mp2650_chg_ops(void)
{
	return (strncmp(oplus_chg_ops_name_get(), "ext-mp2650", CHG_OPS_LEN) == 0);
}

static int oem_battery_chg_write(struct battery_chg_dev *bcdev, void *data,
	int len)
{
	int rc;

	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_err("glink state is down\n");
		return -ENOTCONN;
	}

	mutex_lock(&bcdev->read_buffer_lock);
	reinit_completion(&bcdev->oem_read_ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->oem_read_ack,
			msecs_to_jiffies(OEM_READ_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			oplus_chg_track_upload_adsp_err_info(
				bcdev, TRACK_ADSP_ERR_OEM_GLINK_ABNORMAL);
			mutex_unlock(&bcdev->read_buffer_lock);
			return -ETIMEDOUT;
		}

		rc = 0;
	}
	/*pr_err("oem_battery_chg_write end\n");*/
	mutex_unlock(&bcdev->read_buffer_lock);

	return rc;
}

static int oem_read_buffer(struct battery_chg_dev *bcdev)
{
	struct oem_read_buffer_req_msg req_msg = { { 0 } };

	req_msg.data_size = sizeof(bcdev->read_buffer_dump.data_buffer);
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = OEM_OPCODE_READ_BUFFER;

	/*pr_err("oem_read_buffer\n");*/

	return oem_battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static void handle_oem_read_buffer(struct battery_chg_dev *bcdev,
	struct oem_read_buffer_resp_msg *resp_msg, size_t len)
{
	u32 buf_len;

	/*pr_err("correct length received: %zu expected: %u\n", len,
		sizeof(bcdev->read_buffer_dump));*/

	if (len > sizeof(bcdev->read_buffer_dump)) {
		pr_err("Incorrect length received: %zu expected: %u\n", len,
		sizeof(bcdev->read_buffer_dump));
		return;
	}

	buf_len = resp_msg->data_size;
	if (buf_len > sizeof(bcdev->read_buffer_dump.data_buffer)) {
		pr_err("Incorrect buffer length: %u\n", buf_len);
		return;
	}

	/*pr_err("buf length: %u\n", buf_len);*/
	if (buf_len == 0) {
		pr_err("Incorrect buffer length: %u\n", buf_len);
		return;
	}
	memcpy(bcdev->read_buffer_dump.data_buffer, resp_msg->data_buffer, buf_len);
	/*
	chg_err("%s : ----temp[%d], current[%d], vol[%d], soc[%d], rm[%d], chg_cyc[%d], fcc[%d], cc[%d], soh[%d], \
		suspend[%d], oplus_UsbCommCapable[%d], oplus_pd_svooc[%d], typec_mode[%d], vol_min[%d]", __func__,
		bcdev->read_buffer_dump.data_buffer[0], bcdev->read_buffer_dump.data_buffer[1], bcdev->read_buffer_dump.data_buffer[2],
		bcdev->read_buffer_dump.data_buffer[3], bcdev->read_buffer_dump.data_buffer[4], bcdev->read_buffer_dump.data_buffer[5],
		bcdev->read_buffer_dump.data_buffer[6], bcdev->read_buffer_dump.data_buffer[7], bcdev->read_buffer_dump.data_buffer[8],
		bcdev->read_buffer_dump.data_buffer[9], bcdev->read_buffer_dump.data_buffer[10], bcdev->read_buffer_dump.data_buffer[11],
		bcdev->read_buffer_dump.data_buffer[12], bcdev->read_buffer_dump.data_buffer[13]);
	*/
	bcdev->gauge_data_initialized = true;
	complete(&bcdev->oem_read_ack);
}

static int oem_pmic_buffer_write(struct battery_chg_dev *bcdev, void *data,
	int len)
{
	int rc;

	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_err("glink state is down\n");
		return -ENOTCONN;
	}

	mutex_lock(&bcdev->read_pmic_buffer_lock);
	reinit_completion(&bcdev->read_pmic_buffer_ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->read_pmic_buffer_ack,
			msecs_to_jiffies(OEM_READ_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			oplus_chg_track_upload_adsp_err_info(
				bcdev, TRACK_ADSP_ERR_OEM_GLINK_ABNORMAL);
			mutex_unlock(&bcdev->read_pmic_buffer_lock);
			return -ETIMEDOUT;
		}

		rc = 0;
	}
	mutex_unlock(&bcdev->read_pmic_buffer_lock);

	return rc;
}

static int oem_read_pmic_buffer(struct battery_chg_dev *bcdev)
{
	struct oem_read_buffer_req_msg req_msg = { { 0 } };

	req_msg.data_size = sizeof(bcdev->read_pmic_buffer_dump.data_buffer);
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = OEM_OPCODE_WDOG_READ_BUFFER;

	return oem_pmic_buffer_write(bcdev, &req_msg, sizeof(req_msg));
}


static void handle_oem_read_pmic_buffer(struct battery_chg_dev *bcdev,
        struct oem_read_buffer_resp_msg *resp_msg, size_t len)
{
	u32 buf_len;

	if (len > sizeof(bcdev->read_pmic_buffer_dump)) {
		pr_err("Incorrect length received: %zu expected: %u\n", len,
			sizeof(bcdev->read_pmic_buffer_dump));
		goto out;
	}

	buf_len = resp_msg->data_size;
	if (buf_len > sizeof(bcdev->read_pmic_buffer_dump.data_buffer)) {
		pr_err("Incorrect buffer length: %u\n", buf_len);
		goto out;
	}

	if (buf_len == 0) {
		pr_err("Incorrect buffer length: %u\n", buf_len);
		goto out;
	}
	memcpy(bcdev->read_pmic_buffer_dump.data_buffer, resp_msg->data_buffer, buf_len);

out:
	complete(&bcdev->read_pmic_buffer_ack);
}

static int wls_bcc_get_max_curr(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	union oplus_chg_mod_propval pval = { 0 };
	int max_curr = 0;
	int rc;

	if (!chip) {
		chg_err("g_oplus_chip is NULL\n");
		return 0;
	}

	if (!is_wls_ocm_available(chip))
		return 0;

	rc = oplus_chg_mod_get_property(chip->wls_ocm,
					OPLUS_CHG_PROP_WLS_BCC_MAX, &pval);
	if (rc < 0)
		chg_err("get max curr failed\n");
	else
		max_curr = pval.intval;

	chg_err("wls bcc max=%d", max_curr);

	return max_curr;
}

static int wls_bcc_get_min_curr(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	union oplus_chg_mod_propval pval = { 0 };
	int min_curr = 0;
	int rc;

	if (!chip) {
		chg_err("g_oplus_chip is NULL\n");
		return 0;
	}

	if (!is_wls_ocm_available(chip))
		return 0;

	rc = oplus_chg_mod_get_property(chip->wls_ocm,
					OPLUS_CHG_PROP_WLS_BCC_MIN, &pval);
	if (rc < 0)
		chg_err("get min curr failed\n");
	else
		min_curr = pval.intval;

	chg_err("wls bcc min=%d", min_curr);

	return min_curr;
}

static int wls_bcc_get_stop_curr(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	union oplus_chg_mod_propval pval = { 0 };
	int stop_curr = 0;
	int rc;

	if (!chip) {
		chg_err("g_oplus_chip is NULL\n");
		return 0;
	}

	if (!is_wls_ocm_available(chip))
		return 0;

	rc = oplus_chg_mod_get_property(chip->wls_ocm,
					OPLUS_CHG_PROP_WLS_BCC_STOP, &pval);
	if (rc < 0)
		chg_err("get stop curr failed\n");
	else
		stop_curr = pval.intval;

	chg_err("wls bcc stop=%d", stop_curr);

	return stop_curr;
}

static int bcc_battery_chg_write(struct battery_chg_dev *bcdev, void *data,
	int len)
{
	int rc;

	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_err("glink state is down\n");
		return -ENOTCONN;
	}

	mutex_lock(&bcdev->bcc_read_buffer_lock);
	reinit_completion(&bcdev->bcc_read_ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->bcc_read_ack,
			msecs_to_jiffies(OEM_READ_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			oplus_chg_track_upload_adsp_err_info(
				bcdev, TRACK_ADSP_ERR_BCC_GLINK_ABNORMAL);
			mutex_unlock(&bcdev->bcc_read_buffer_lock);
			return -ETIMEDOUT;
		}

		rc = 0;
	}
	pr_err("bcc_battery_chg_write end\n");
	mutex_unlock(&bcdev->bcc_read_buffer_lock);

	return rc;
}

static int bcc_read_buffer(struct battery_chg_dev *bcdev)
{
	struct oem_read_buffer_req_msg req_msg = { { 0 } };

	req_msg.data_size = sizeof(bcdev->bcc_read_buffer_dump.data_buffer);
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = BCC_OPCODE_READ_BUFFER;

	pr_err("bcc_read_buffer\n");

	return bcc_battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static void handle_bcc_read_buffer(struct battery_chg_dev *bcdev,
	struct oem_read_buffer_resp_msg *resp_msg, size_t len)
{
	u32 buf_len;

	/*pr_err("correct length received: %zu expected: %u\n", len,
		sizeof(bcdev->read_buffer_dump));*/

	if (g_oplus_chip == NULL) {
		chg_err("%s g_oplus_chip is null\n", __func__);
		return;
	}

	if (len > sizeof(bcdev->bcc_read_buffer_dump)) {
		pr_err("Incorrect length received: %zu expected: %u\n", len,
		sizeof(bcdev->bcc_read_buffer_dump));
		complete(&bcdev->bcc_read_ack);
		return;
	}

	buf_len = resp_msg->data_size;
	if (buf_len > sizeof(bcdev->bcc_read_buffer_dump.data_buffer)) {
		pr_err("Incorrect buffer length: %u\n", buf_len);
		complete(&bcdev->bcc_read_ack);
		return;
	}

	/*pr_err("buf length: %u\n", buf_len);*/
	if (buf_len == 0) {
		pr_err("Incorrect buffer length: %u\n", buf_len);
		complete(&bcdev->bcc_read_ack);
		return;
	}
	memcpy(bcdev->bcc_read_buffer_dump.data_buffer, resp_msg->data_buffer, buf_len);

	if ((oplus_vooc_get_fastchg_ing() &&
	    oplus_vooc_get_fast_chg_type() != CHARGER_SUBTYPE_FASTCHG_VOOC) ||
	    oplus_pps_get_pps_fastchg_started())
		bcdev->bcc_read_buffer_dump.data_buffer[15] = 1;
	else
		bcdev->bcc_read_buffer_dump.data_buffer[15] = 0;

	if (oplus_chg_wls_support_bcc(g_oplus_chip) &&
		oplus_chg_is_wls_present() && oplus_chg_is_wls_fastchg_started()) {
		bcdev->bcc_read_buffer_dump.data_buffer[15] = 1;
		bcdev->bcc_read_buffer_dump.data_buffer[9] = wls_bcc_get_max_curr();
		bcdev->bcc_read_buffer_dump.data_buffer[10] = wls_bcc_get_min_curr();
		bcdev->bcc_read_buffer_dump.data_buffer[14] = wls_bcc_get_stop_curr();
	}

	if (oplus_pps_get_pps_fastchg_started()) {
		if (oplus_pps_bcc_get_temp_range()) {
			bcdev->bcc_read_buffer_dump.data_buffer[9] = oplus_pps_get_bcc_max_curr();
			bcdev->bcc_read_buffer_dump.data_buffer[10] = oplus_pps_get_bcc_min_curr();
			bcdev->bcc_read_buffer_dump.data_buffer[14] = oplus_pps_get_bcc_exit_curr();
		} else {
			bcdev->bcc_read_buffer_dump.data_buffer[9] = 0;
			bcdev->bcc_read_buffer_dump.data_buffer[10] = 0;
			bcdev->bcc_read_buffer_dump.data_buffer[14] = 0;
			bcdev->bcc_read_buffer_dump.data_buffer[15] = 0;
		}
	}
	if (bcdev->bcc_read_buffer_dump.data_buffer[9] == 0) {
		bcdev->bcc_read_buffer_dump.data_buffer[15] = 0;
	}

	bcdev->bcc_read_buffer_dump.data_buffer[8] = DIV_ROUND_CLOSEST((int)bcdev->bcc_read_buffer_dump.data_buffer[8], 1000);
	bcdev->bcc_read_buffer_dump.data_buffer[16] = oplus_chg_get_bcc_curr_done_status();

	bcdev->bcc_read_buffer_dump.data_buffer[18] = DOUBLE_SERIES_WOUND_CELLS;

	chg_err("%s : ----dod0_1[%d], dod0_2[%d], dod0_passed_q[%d], qmax_1[%d], qmax_2[%d], qmax_passed_q[%d] \
		voltage_cell1[%d], temperature[%d], batt_current[%d], max_current[%d], min_current[%d], voltage_cell2[%d], \
		soc_ext_1[%d], soc_ext_2[%d], atl_last_geat_current[%d], charging_flag[%d], bcc_curr_done[%d], guage[%d], batt_type[%d]", __func__,
		bcdev->bcc_read_buffer_dump.data_buffer[0], bcdev->bcc_read_buffer_dump.data_buffer[1], bcdev->bcc_read_buffer_dump.data_buffer[2],
		bcdev->bcc_read_buffer_dump.data_buffer[3], bcdev->bcc_read_buffer_dump.data_buffer[4], bcdev->bcc_read_buffer_dump.data_buffer[5],
		bcdev->bcc_read_buffer_dump.data_buffer[6], bcdev->bcc_read_buffer_dump.data_buffer[7], bcdev->bcc_read_buffer_dump.data_buffer[8],
		bcdev->bcc_read_buffer_dump.data_buffer[9], bcdev->bcc_read_buffer_dump.data_buffer[10], bcdev->bcc_read_buffer_dump.data_buffer[11],
		bcdev->bcc_read_buffer_dump.data_buffer[12], bcdev->bcc_read_buffer_dump.data_buffer[13], bcdev->bcc_read_buffer_dump.data_buffer[14],
		bcdev->bcc_read_buffer_dump.data_buffer[15], bcdev->bcc_read_buffer_dump.data_buffer[16], bcdev->bcc_read_buffer_dump.data_buffer[17],
		bcdev->bcc_read_buffer_dump.data_buffer[18]);
	complete(&bcdev->bcc_read_ack);
}

static int adsp_track_battery_chg_write(
	struct battery_chg_dev *bcdev, void *data, int len)
{
	int rc;

	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_err("glink state is down\n");
		return -ENOTCONN;
	}

	mutex_lock(&bcdev->adsp_track_read_buffer_lock);
	reinit_completion(&bcdev->adsp_track_read_ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->adsp_track_read_ack,
			msecs_to_jiffies(TRACK_READ_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			mutex_unlock(&bcdev->adsp_track_read_buffer_lock);
			return -ETIMEDOUT;
		}

		rc = 0;
	}
	oplus_chg_track_handle_adsp_info(
		bcdev->adsp_track_read_buffer.data_buffer,
		bcdev->adsp_track_read_buffer.data_size);
	mutex_unlock(&bcdev->adsp_track_read_buffer_lock);
	pr_info("success\n");

	return rc;
}

static int adsp_track_read_buffer(struct battery_chg_dev *bcdev)
{
	struct adsp_track_read_req_msg req_msg = { { 0 } };

	req_msg.data_size = sizeof(bcdev->adsp_track_read_buffer.data_buffer);
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = TRACK_OPCODE_READ_BUFFER;

	pr_info("start\n");

	return adsp_track_battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static void handle_adsp_track_read_buffer(
	struct battery_chg_dev *bcdev,
	struct adsp_track_read_resp_msg *resp_msg, size_t len)
{
	u32 buf_len;

	if (len > sizeof(bcdev->adsp_track_read_buffer)) {
		pr_err("Incorrect length received: %zu expected: %lu\n", len,
		sizeof(bcdev->adsp_track_read_buffer));
		return;
	}

	buf_len = resp_msg->data_size;
	if (buf_len > sizeof(bcdev->adsp_track_read_buffer.data_buffer) ||!buf_len) {
		pr_err("Incorrect buffer length: %u\n", buf_len);
		return;
	}

	bcdev->adsp_track_read_buffer.data_size = buf_len;
	memcpy(bcdev->adsp_track_read_buffer.data_buffer,
		resp_msg->data_buffer, buf_len);

	complete(&bcdev->adsp_track_read_ack);
}

static void adsp_track_notification_handler(struct work_struct *work)
{
	struct battery_chg_dev *bcdev =
		container_of(work, struct battery_chg_dev, adsp_track_notify_work.work);

	if (!bcdev) {
		pr_err("bcdev null, return\n");
		return;
	}
	adsp_track_read_buffer(bcdev);
}

#ifdef OPLUS_FEATURE_CHG_BASIC
static int pps_battery_chg_write(struct battery_chg_dev *bcdev, void *data,
	int len)
{
	int rc;

	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_err("glink state is down\n");
		return -ENOTCONN;
	}

	mutex_lock(&bcdev->pps_read_buffer_lock);
	reinit_completion(&bcdev->pps_read_ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->pps_read_ack,
			msecs_to_jiffies(OEM_READ_WAIT_TIME_MS));
		if (!rc) {
			chg_err("Error, timed out sending message\n");
			oplus_chg_track_upload_adsp_err_info(
				bcdev, TRACK_ADSP_ERR_PPS_GLINK_ABNORMAL);
			mutex_unlock(&bcdev->pps_read_buffer_lock);
			return -ETIMEDOUT;
		}

		rc = 0;
	}
	chg_err("pps_battery_chg_write end\n");
	mutex_unlock(&bcdev->pps_read_buffer_lock);

	return rc;
}

static int pps_read_buffer(struct battery_chg_dev *bcdev)
{
	struct oem_read_buffer_req_msg req_msg = { { 0 } };

	req_msg.data_size = sizeof(bcdev->pps_read_buffer_dump.data_buffer);
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = PPS_OPCODE_READ_BUFFER;

	chg_err("pps_read_buffer\n");

	return pps_battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static void handle_pps_read_buffer(struct battery_chg_dev *bcdev,
	struct oem_read_buffer_resp_msg *resp_msg, size_t len)
{
	u32 buf_len;

	if (len > sizeof(bcdev->pps_read_buffer_dump)) {
		chg_err("Incorrect length received: %zu expected: %u\n", len,
		sizeof(bcdev->pps_read_buffer_dump));
		return;
	}

	buf_len = resp_msg->data_size;
	if (buf_len > sizeof(bcdev->pps_read_buffer_dump.data_buffer)) {
		chg_err("Incorrect buffer length: %u\n", buf_len);
		return;
	}

	if (buf_len == 0) {
		chg_err("Incorrect buffer length: %u\n", buf_len);
		return;
	}
	memcpy(bcdev->pps_read_buffer_dump.data_buffer, resp_msg->data_buffer, buf_len);
	complete(&bcdev->pps_read_ack);
}
#endif /*OPLUS_FEATURE_CHG_BASIC*/

void oplus_get_props_from_adsp_by_buffer(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("!!!chip null, oplus_get_batt_argv_buffer\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	oem_read_buffer(bcdev);
}

#define BCC_SET_DEBUG_PARMS 1
#define BCC_PARMS_COUNT 19
#define BCC_PAGE_SIZE 256
#define BCC_N_DEBUG 0
#define BCC_Y_DEBUG 1
static int bcc_debug_mode  = BCC_N_DEBUG;
static char bcc_debug_buf[BCC_PAGE_SIZE] = {0};
static int oplus_get_bcc_parameters_from_adsp(char *buf)
{
	int ret = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	u8 tmpbuf[PAGE_SIZE] = {0};
	int len = 0;
	int i = 0;
	int idx = 0;

	if (!chip) {
		chg_err("!!!chip null, oplus_get_batt_argv_buffer\n");
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	ret = bcc_read_buffer(bcdev);

	for (i = 0; i < BCC_PARMS_COUNT - 1; i++) {
		len = snprintf(tmpbuf, BCC_PAGE_SIZE - idx,
						"%d,", bcdev->bcc_read_buffer_dump.data_buffer[i]);
		memcpy(&buf[idx], tmpbuf, len);
		idx += len;
	}
	len = snprintf(tmpbuf, BCC_PAGE_SIZE - idx,
						"%d", bcdev->bcc_read_buffer_dump.data_buffer[i]);
	memcpy(&buf[idx], tmpbuf, len);
#ifdef BCC_SET_DEBUG_PARMS
	if (bcc_debug_mode & BCC_Y_DEBUG) {
		memcpy(&buf[0], bcc_debug_buf, BCC_PAGE_SIZE);
		chg_err("%s bcc_debug_buf:%s\n", __func__, bcc_debug_buf);
		return ret;
	}
#endif
	chg_err("%s buf:%s\n", __func__, buf);
	return ret;
}

#define BCC_DEBUG_PARAM_SIZE 8
static int oplus_set_bcc_debug_parameters(const char *buf)
{
	int ret = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
#ifdef BCC_SET_DEBUG_PARMS
	char temp_buf[10] = {0};
#endif
	if (!chip) {
		chg_err("!!!chip null, oplus_get_batt_argv_buffer\n");
		return -1;
	}

#ifdef BCC_SET_DEBUG_PARMS
	if (strlen(buf) <= BCC_PAGE_SIZE) {
		if (strncpy(temp_buf, buf, 7)) {
			chg_err("%s temp_buf:%s\n", __func__, temp_buf);
		}
		if (!strncmp(temp_buf, "Y_DEBUG", 7)) {
			bcc_debug_mode = BCC_Y_DEBUG;
			chg_err("%s BCC_Y_DEBUG:%d\n",
				__func__, bcc_debug_mode);
		} else {
			bcc_debug_mode = BCC_N_DEBUG;
			chg_err("%s BCC_N_DEBUG:%d\n",
				__func__, bcc_debug_mode);
		}
		strncpy(bcc_debug_buf, buf + BCC_DEBUG_PARAM_SIZE, BCC_PAGE_SIZE);
		chg_err("%s bcc_debug_buf:%s, temp_buf\n",
			__func__, bcc_debug_buf, temp_buf);
		return ret;
	}
#endif

	chg_err("%s buf:%s\n", __func__, buf);
	return ret;
}

void oplus_get_pps_parameters_from_adsp(void)

{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	int imax = 0, vmax = 0;
	struct oplus_pps_chip *pps_chip;

	if (!chip) {
		chg_err("!!!chip null, oplus_get_batt_argv_buffer\n");
		return;
	}

	pps_chip = oplus_pps_get_pps_chip();
	bcdev = chip->pmic_spmi.bcdev_chip;
	pps_read_buffer(bcdev);
	imax = bcdev->pps_read_buffer_dump.data_buffer[0];
	vmax = bcdev->pps_read_buffer_dump.data_buffer[1];

	if ((imax > OPLUS_PPS_IMIN_V3) && (vmax >= OPLUS_EXTEND_VMIN)) {
		oplus_pps_set_power(OPLUS_PPS_POWER_V3, imax, vmax);
	} else if ((imax > OPLUS_EXTEND_IMIN) && (vmax >= OPLUS_EXTEND_VMIN)) {
		oplus_pps_set_power(OPLUS_PPS_POWER_V2, imax, vmax);
	} else if ((imax > OPLUS_PPS_IMIN_V1) && (vmax >= OPLUS_EXTEND_VMIN)) {
		oplus_pps_set_power(OPLUS_PPS_POWER_V1, imax, vmax);
		oplus_pps_track_upload_err_info(pps_chip, TRACK_PPS_ERR_POWER_V1, imax);
	} else {
		oplus_pps_track_upload_err_info(pps_chip, TRACK_PPS_ERR_POWER_V0, imax);
		oplus_pps_set_power(OPLUS_PPS_POWER_CLR, 0, 0);
	}
	chg_err("oplus_get_pps_parameters_from_adsp imax = %d, vmax = %d\n", imax, vmax);
}
#endif

static int battery_chg_fw_write(struct battery_chg_dev *bcdev, void *data,
				int len)
{
	int rc;

	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_debug("glink state is down\n");
		return -ENOTCONN;
	}

	reinit_completion(&bcdev->fw_buf_ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->fw_buf_ack,
					msecs_to_jiffies(WLS_FW_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			oplus_chg_track_upload_adsp_err_info(
				bcdev, TRACK_ADSP_ERR_FW_GLINK_ABNORMAL);
			return -ETIMEDOUT;
		}

		rc = 0;
	}

	return rc;
}
#define  TRANSFER_TIMOUT_LIMIT  10

static int battery_chg_write(struct battery_chg_dev *bcdev, void *data,
				int len)
{
	int rc;

	/*
	 * When the subsystem goes down, it's better to return the last
	 * known values until it comes back up. Hence, return 0 so that
	 * pmic_glink_write() is not attempted until pmic glink is up.
	 */
	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_debug("glink state is down\n");
		return 0;
	}

	if (bcdev->debug_battery_detected && bcdev->block_tx)
		return 0;

	mutex_lock(&bcdev->rw_lock);
	reinit_completion(&bcdev->ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->ack,
					msecs_to_jiffies(BC_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			if (g_oplus_chip)
				g_oplus_chip->transfer_timeout_count++;
			oplus_chg_track_upload_adsp_err_info(
				bcdev, TRACK_ADSP_ERR_GLINK_ABNORMAL);
			mutex_unlock(&bcdev->rw_lock);
			return -ETIMEDOUT;
		}

		rc = 0;
	}
	mutex_unlock(&bcdev->rw_lock);
	if (g_oplus_chip)
		g_oplus_chip->transfer_timeout_count = 0;
	if (bcdev->debug_pmic_glink_err) {
		oplus_chg_track_upload_adsp_err_info(
			bcdev, bcdev->debug_pmic_glink_err);
		bcdev->debug_pmic_glink_err = 0;
	}

	return rc;
}

static int write_property_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id, u32 val)
{
	struct battery_charger_req_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.battery_id = 0;
	req_msg.value = val;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;

	pr_debug("psy: %s prop_id: %u val: %u\n", pst->psy->desc->name,
		req_msg.property_id, val);

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static int read_property_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id)
{
	struct battery_charger_req_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.battery_id = 0;
	req_msg.value = 0;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_get;

	pr_debug("psy: %s prop_id: %u\n", pst->psy->desc->name,
		req_msg.property_id);

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static int get_property_id(struct psy_state *pst,
			enum power_supply_property prop)
{
	u32 i;

	for (i = 0; i < pst->prop_count; i++)
		if (pst->map[i] == prop)
			return i;

	pr_err("No property id for property %d in psy %s\n", prop,
		pst->psy->desc->name);

	return -ENOENT;
}

static void battery_chg_notify_enable(struct battery_chg_dev *bcdev)
{
	struct battery_charger_set_notify_msg req_msg = { { 0 } };
	int rc;

	/* Send request to enable notification */
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_NOTIFY;
	req_msg.hdr.opcode = BC_SET_NOTIFY_REQ;

	rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
	if (rc < 0)
		pr_err("Failed to enable notification rc=%d\n", rc);
}

#ifdef OPLUS_FEATURE_CHG_BASIC
static void oplus_ccdetect_happened_to_adsp(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_CCDETECT_HAPPENED, 1);
	if (rc < 0) {
		chg_err("[OPLUS_CHG][%s]: write ccdetect plugout fail, rc[%d]\n", __func__, rc);
		return;
	} else {
		chg_err("[OPLUS_CHG][%s]:write ccdetect plugout success, rc[%d]\n", __func__, rc);
	}
}

bool oplus_ccdetect_check_is_gpio(struct oplus_chg_chip *chip);
static int get_otg_scheme(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = chip->pmic_spmi.bcdev_chip;
	int otg_scheme = bcdev->otg_scheme;

	otg_scheme = OTG_SCHEME_CID;
	return otg_scheme;
}

void oplus_ccdetect_enable(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_TYPEC_MODE);
	if (rc < 0) {
		chg_err("[OPLUS_CHG][%s]: 111 Couldn't read 0x2b44 rc=%d\n", __func__, rc);
		return;
	} else {
		chg_err("[OPLUS_CHG][%s]:111 reg0x2b44[0x%x], bit[2:0]=0(DRP)\n", __func__, pst->prop[USB_TYPEC_MODE]);
	}

	/* set DRP mode */
	rc = write_property_id(bcdev, pst, USB_TYPEC_MODE, TYPEC_PORT_ROLE_DRP);
	if (rc < 0) {
		chg_err("[OPLUS_CHG][%s]: Couldn't clear 0x2b44[0] rc=%d\n", __func__, rc);
	}

	rc = read_property_id(bcdev, pst, USB_TYPEC_MODE);
	if (rc < 0) {
		chg_err("[OPLUS_CHG][%s]: 111 Couldn't read 0x2b44 rc=%d\n", __func__, rc);
		return;
	} else {
		chg_err("[OPLUS_CHG][%s]:111 reg0x2b44[0x%x], bit[2:0]=0(DRP)\n", __func__, pst->prop[USB_TYPEC_MODE]);
	}
}

void oplus_typec_disable(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	/* set disable typec mode */
	rc = write_property_id(bcdev, pst, USB_TYPEC_MODE, TYPEC_PORT_ROLE_DISABLE);
	if (rc < 0) {
		chg_err("[OPLUS_CHG][%s]: Couldn't write 0x2b44[3] rc=%d\n", __func__, rc);
	}
}

void oplus_ccdetect_disable(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	if (oplus_ccdetect_check_is_gpio(chip) != true) {
		chg_err("ccdetect check is gpio fail, return!\n");
		return;
	}

	/* set UFP mode */
	rc = write_property_id(bcdev, pst, USB_TYPEC_MODE, TYPEC_PORT_ROLE_SNK);
	if (rc < 0) {
		chg_err("[OPLUS_CHG][%s]: Couldn't clear 0x2b44[0] rc=%d\n", __func__, rc);
	}

	rc = read_property_id(bcdev, pst, USB_TYPEC_MODE);
	if (rc < 0) {
		chg_err("[OPLUS_CHG][%s]: 111 Couldn't read 0x2b44 rc=%d\n", __func__, rc);
		return;
	} else {
		chg_err("[OPLUS_CHG][%s]:111 reg0x2b44[0x%x], bit[2:0]=0(UFP)\n", __func__, pst->prop[USB_TYPEC_MODE]);
	}
}

#define CCDETECT_DELAY_MS	50
irqreturn_t oplus_ccdetect_change_handler(int irq, void *data)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	cancel_delayed_work_sync(&bcdev->ccdetect_work);

	chg_err("[OPLUS_CHG][%s]: !!!!handle!\n", __func__);
	schedule_delayed_work(&bcdev->ccdetect_work,
			msecs_to_jiffies(CCDETECT_DELAY_MS));
	return IRQ_HANDLED;
}

int oplus_ccdetect_gpio_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	bcdev->oplus_custom_gpio.ccdetect_pinctrl = devm_pinctrl_get(chip->dev);

	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.ccdetect_pinctrl)) {
		chg_err("get ccdetect ccdetect_pinctrl fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.ccdetect_active = pinctrl_lookup_state(
		bcdev->oplus_custom_gpio.ccdetect_pinctrl, "ccdetect_active");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.ccdetect_active)) {
		chg_err("get ccdetect_active fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.ccdetect_sleep = pinctrl_lookup_state(
		bcdev->oplus_custom_gpio.ccdetect_pinctrl, "ccdetect_sleep");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.ccdetect_sleep)) {
		chg_err("get ccdetect_sleep fail\n");
		return -EINVAL;
	}

	if (bcdev->oplus_custom_gpio.ccdetect_gpio > 0) {
		gpio_direction_input(bcdev->oplus_custom_gpio.ccdetect_gpio);
	}

	pinctrl_select_state(bcdev->oplus_custom_gpio.ccdetect_pinctrl,
		bcdev->oplus_custom_gpio.ccdetect_active);

	return 0;
}

bool oplus_ccdetect_check_is_gpio(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	int boot_mode = get_boot_mode();
#endif

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip ready!\n", __func__);
		return false;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	/* HW engineer requirement */
	if (boot_mode == MSM_BOOT_MODE__RF || boot_mode == MSM_BOOT_MODE__WLAN
			|| boot_mode == MSM_BOOT_MODE__FACTORY) {
		chg_err("boot_mode: %d, return!\n", boot_mode);
		return false;
	}
#endif

	if (gpio_is_valid(bcdev->oplus_custom_gpio.ccdetect_gpio)) {
		chg_err("ccdetect gpio is valid!\n");
		return true;
	}

	return false;
}

void oplus_ccdetect_irq_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	bcdev->ccdetect_irq = gpio_to_irq(bcdev->oplus_custom_gpio.ccdetect_gpio);
	chg_err("[OPLUS_CHG][%s]: bcdev->ccdetect_irq[%d]!\n", __func__, bcdev->ccdetect_irq);
}
static void oplus_ccdetect_irq_register(struct oplus_chg_chip *chip)
{
	int ret = 0;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	ret = devm_request_threaded_irq(chip->dev, bcdev->ccdetect_irq,
			NULL, oplus_ccdetect_change_handler, IRQF_TRIGGER_FALLING
			| IRQF_TRIGGER_RISING | IRQF_ONESHOT, "ccdetect-change", chip);
	if (ret < 0) {
		chg_err("Unable to request ccdetect-change irq: %d\n", ret);
	}
	chg_err("%s: !!!!! irq register\n", __FUNCTION__);

	ret = enable_irq_wake(bcdev->ccdetect_irq);
	if (ret != 0) {
		chg_err("enable_irq_wake: ccdetect_irq failed %d\n", ret);
	}
}

static void oplus_hvdcp_disable_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, hvdcp_disable_work.work);

	if (oplus_chg_is_usb_present() == false) {
		chg_err("set bcdev->hvdcp_disable false\n");
		bcdev->hvdcp_disable = false;
	}
}

static void oplus_pd_type_check_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, pd_type_check_work.work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc < 0) {
		chg_err("Failed to read USB_ADAP_TYPE rc=%d\n", rc);
		return;
	}

	if (bcdev->pd_svooc) {
		chg_err("pd_svooc return\n");
		return;
	}

	if (pst->prop[USB_ADAP_TYPE] == POWER_SUPPLY_USB_TYPE_PD
	    || pst->prop[USB_ADAP_TYPE] == POWER_SUPPLY_USB_TYPE_PD_DRP) {
		chg_err("USB_ADAP_TYPE=%d,update work\n", pst->prop[USB_ADAP_TYPE]);
		oplus_chg_wake_update_work();
	} else {
		chg_err("USB_ADAP_TYPE=%d\n", pst->prop[USB_ADAP_TYPE]);
	}
}

static void oplus_unsuspend_usb_work(struct work_struct *work)
{
	if (g_oplus_chip) {
		if (g_oplus_chip && g_oplus_chip->chg_ops->charger_unsuspend) {
			g_oplus_chip->chg_ops->charger_unsuspend();
		}
	}
}

static void oplus_adsp_voocphy_status_func(struct work_struct *work)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct psy_state *pst_batt = NULL;
	int rc = 0;
	int intval = 0;

	if (!chip) {
		chg_err("!!!chip null, oplus_adsp_voocphy_status_func\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	pst_batt = &bcdev->psy_list[PSY_TYPE_BATTERY];
	rc = read_property_id(bcdev, pst, USB_VOOCPHY_STATUS);
	if (rc < 0) {
		chg_err("!!![adsp_voocphy] read adsp voocphy status fail\n");
		return;
	}
	intval = pst->prop[USB_VOOCPHY_STATUS];

	if ((intval & 0xFF) == ADSP_VPHY_FAST_NOTIFY_ERR_COMMU
		|| (intval & 0xFF) == ADSP_VPHY_FAST_NOTIFY_COMMU_TIME_OUT
		|| (intval & 0xFF) == ADSP_VPHY_FAST_NOTIFY_COMMU_CLK_ERR) {
		/*unplug svooc but usb_in_status (in oplus_plugin_irq_work) was 1 sometimes*/
		schedule_delayed_work(&bcdev->adsp_voocphy_enable_check_work, round_jiffies_relative(msecs_to_jiffies(5000)));
		if (bcdev->usb_in_status != false && bcdev->usb_ocm)
			oplus_chg_global_event(bcdev->usb_ocm, OPLUS_CHG_EVENT_OFFLINE);
		schedule_delayed_work(&bcdev->plugin_irq_work, 0);
		schedule_delayed_work(&bcdev->recheck_input_current_work, msecs_to_jiffies(3000));
	}
	if ((intval & 0xFF) == ADSP_VPHY_FAST_NOTIFY_BATT_TEMP_OVER) {
		/*fast charge warm switch to normal charge,input current limmit to 500mA,rerun ICL setting*/
			schedule_delayed_work(&bcdev->recheck_input_current_work, msecs_to_jiffies(3000));
	}

	oplus_adsp_voocphy_handle_status(pst_batt->psy, intval);
}

static void oplus_otg_init_status_func(struct work_struct *work)
{
	int count = 20;
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, otg_vbus_enable_work.work);
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!bcdev || !chip) {
		pr_err("bcdev is null or chip is null, return\n");
		return;
	}

	if (bcdev->otg_boost_src == OTG_BOOST_SOURCE_EXTERNAL) {
		while (count--) {
			if (is_wls_ocm_available(chip))
				break;

			msleep(500);
		}
	}

	chg_err("!!!!oplus_otg_init_status_func, count[%d]\n", count);
	oplus_otg_ap_enable(true);
}

static void otg_notification_handler(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, otg_vbus_enable_work.work);
	struct oplus_chg_chip *chip = g_oplus_chip;
	bool enable = false;
	int (*otg_func_ptr)(void) = NULL;

	if (!bcdev || !chip) {
		pr_err("bcdev is null or chip is null, return\n");
		return;
	}

	if (bcdev->otg_boost_src == OTG_BOOST_SOURCE_EXTERNAL) {
		if (bcdev->otg_online) {
			if (bcdev->usb_ocm) {
				oplus_chg_global_event(bcdev->usb_ocm, OPLUS_CHG_EVENT_OTG_ENABLE);
				/*for HW spec,need 100ms delay*/
				msleep(100);
			}
			oplus_set_otg_ovp_en_val(1);
			oplus_set_otg_boost_en_val(1);
		} else {
			oplus_set_otg_boost_en_val(0);
			oplus_set_otg_ovp_en_val(0);
			if (bcdev->usb_ocm) {
				/*for HW spec,need 100ms delay*/
				msleep(100);
				oplus_chg_global_event(bcdev->usb_ocm, OPLUS_CHG_EVENT_OTG_DISABLE);
			}
		}
	} else if (bcdev->otg_boost_src == OTG_BOOST_SOURCE_PMIC) {
		enable = (bcdev->otg_prohibited ? false : bcdev->otg_online);
		oplus_voocphy_set_chg_auto_mode(enable);
		otg_func_ptr = (enable ? chip->chg_ops->otg_enable : chip->chg_ops->otg_disable);
		if (otg_func_ptr != NULL) {
			otg_func_ptr();
		}
	}
}

static void oplus_vbus_enable_adc_work(struct work_struct *work)
{
	struct oplus_chg_chip *chip = g_oplus_chip;


	if (!chip || !chip->chg_ops) {
		pr_err("chip or chip->chg_ops NULL\n");
		return;
	}

	oplus_chg_disable_charge();
	oplus_chg_suspend_charger();
}
static void oplus_turn_off_power_when_adsp_crash(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("!!!chip null\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	bcdev->is_chargepd_ready = false;
	bcdev->pd_svooc = false;

	if (bcdev->otg_online == true) {
		bcdev->otg_online = false;
		oplus_wpc_set_booster_en_val(0);
		oplus_set_otg_ovp_en_val(0);
		if (bcdev->usb_ocm) {
			oplus_chg_global_event(bcdev->usb_ocm, OPLUS_CHG_EVENT_OTG_DISABLE);
		}
	}
}

bool oplus_is_pd_svooc(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("!!!chip null\n");
		return false;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	if (!bcdev) {
		chg_err("!!!bcdev null\n");
		return false;
	}

	chg_err("!!!:%s, pd_svooc[%d]\n", __func__, bcdev->pd_svooc);
	return bcdev->pd_svooc;
}
EXPORT_SYMBOL(oplus_is_pd_svooc);

static void oplus_adsp_crash_recover_work(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("!!!chip null\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	schedule_delayed_work(&bcdev->adsp_crash_recover_work, round_jiffies_relative(msecs_to_jiffies(1500)));
}
#define OTG_RECOVERY_DELAY_TIME  round_jiffies_relative(msecs_to_jiffies(2000))
static void oplus_adsp_crash_recover_func(struct work_struct *work)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("!!!chip null\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	if (chip->voocphy_support == ADSP_VOOCPHY) {
		oplus_ap_init_adsp_gague();
		oplus_adsp_voocphy_reset_status_when_crash_recover();
	}
	chip->charger_type  = POWER_SUPPLY_TYPE_UNKNOWN;
	if (chip->voocphy_support == ADSP_VOOCPHY) {
		oplus_adsp_voocphy_enable(true);
	}
	schedule_delayed_work(&bcdev->otg_init_work, OTG_RECOVERY_DELAY_TIME);
	oplus_chg_wake_update_work();
	schedule_delayed_work(&bcdev->adsp_voocphy_enable_check_work, round_jiffies_relative(msecs_to_jiffies(0)));
	schedule_delayed_work(&bcdev->check_charger_out_work, round_jiffies_relative(msecs_to_jiffies(3000)));
}
static int smbchg_lcm_en(bool en);

enum lcm_en_status {
	LCM_EN_DEAFULT = 1,
	LCM_EN_ENABLE,
	LCM_EN_DISABLE,
};

#define LCM_CHARGER_VOL_THR_MV	2500
#define LCM_FREQUENCY_INTERVAL	5000
void lcm_frequency_ctrl(void)
{
	static int lcm_en_flag = LCM_EN_DEAFULT;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	static int  check_count = 0;

	if (!chip) {
		chg_err("!!!chip null\n");
		return;
	}
	check_count++;
	if (check_count > LCM_CHECK_COUNT) {
		lcm_en_flag = LCM_EN_DEAFULT;
		check_count = 0;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	if ((oplus_chg_get_charger_voltage() > LCM_CHARGER_VOL_THR_MV) || (oplus_chg_is_wls_present())) {
		if (chip->sw_full || (oplus_pps_get_ffc_started() == true) ||
		    chip->hw_full_by_sw || !smbchg_get_charge_enable()) {
			if (lcm_en_flag != LCM_EN_ENABLE) {
				lcm_en_flag = LCM_EN_ENABLE;
				smbchg_lcm_en(true);
				pr_info("lcm_en_flag:%d\n", lcm_en_flag);
			}
		} else {
			if (lcm_en_flag != LCM_EN_DISABLE) {
				lcm_en_flag = LCM_EN_DISABLE;
				smbchg_lcm_en(false);
				pr_info(" lcm_en_flag:%d\n", lcm_en_flag);
			}
		}

		mod_delayed_work(system_highpri_wq, &bcdev->ctrl_lcm_frequency,
						 LCM_FREQUENCY_INTERVAL);
	} else {
			if (lcm_en_flag != LCM_EN_ENABLE) {
				lcm_en_flag = LCM_EN_ENABLE;
				smbchg_lcm_en(true);
				pr_info(" lcm_en_flag:%d\n", lcm_en_flag);
			}
	}
}

static void oplus_chg_ctrl_lcm_work(struct work_struct *work)
{
	lcm_frequency_ctrl();
}

#define CHARGER_VOL_THR_MV	2000
static void oplus_check_charger_out_func(struct work_struct *work)
{
	int chg_vol = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst_batt = NULL;

	if (!chip) {
		chg_err("!!!chip null\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst_batt = &bcdev->psy_list[PSY_TYPE_BATTERY];

	chg_vol = oplus_chg_get_charger_voltage();
	if (chg_vol >= 0 && chg_vol < CHARGER_VOL_THR_MV) {
		if (oplus_voocphy_get_bidirect_cp_support() && oplus_vooc_get_fastchg_started())
			oplus_voocphy_chg_out_check_event_handle(true);
		oplus_adsp_voocphy_clear_status();
		oplus_chg_clear_abnormal_adapter_var();
		if (pst_batt->psy)
			power_supply_changed(pst_batt->psy);
		chg_err("charger out, chg_vol:%d\n", chg_vol);
	}
}

static void oplus_adsp_voocphy_enable_check_func(struct work_struct *work)
{
	int rc = 0;
	int voocphy_enable = 0;
	static int first_enalbe = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("!!!chip null\n");
		return;
	}

	if (chip->voocphy_support != ADSP_VOOCPHY) {
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;

	if (first_enalbe == 0) {
		first_enalbe++;
		oplus_chg_wake_update_work();
		schedule_delayed_work(&bcdev->adsp_voocphy_enable_check_work, round_jiffies_relative(msecs_to_jiffies(5000)));
		return;
	}
	chip->first_enabled_adspvoocphy = true;

	if (chip->mmi_chg == 0 || chip->charger_exist == false ||
	    chip->charger_type != POWER_SUPPLY_TYPE_USB_DCP || bcdev->qc_enable_status == 1 ||
	    oplus_get_quirks_plug_status(QUIRKS_STOP_ADSP_VOOCPHY)) {
		/*chg_err("is_mmi_chg no_charger_exist no_dcp_type\n");*/
		schedule_delayed_work(&bcdev->adsp_voocphy_enable_check_work, round_jiffies_relative(msecs_to_jiffies(5000)));
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	voocphy_enable = oplus_adsp_voocphy_get_enable();
	if (voocphy_enable == 0) {
		chg_err("!!!need enable voocphy again\n");
		rc = oplus_adsp_voocphy_enable(true);
		schedule_delayed_work(&bcdev->adsp_voocphy_enable_check_work, round_jiffies_relative(msecs_to_jiffies(500)));
	} else {
		/*chg_err("!!!enable voocphy ok\n");*/
		schedule_delayed_work(&bcdev->adsp_voocphy_enable_check_work, round_jiffies_relative(msecs_to_jiffies(5000)));
	}
}

#endif /*OPLUS_FEATURE_CHG_BASIC*/

#ifdef OPLUS_FEATURE_CHG_BASIC
static void oplus_wait_wired_charge_on_work(struct work_struct *work)
{
	chg_err("[OPLUS_CHG][%s]<~WPC~> wait_wired_charge_on\n", __func__);
/*#if 0
	oplus_wpc_set_wrx_en_value(0);
	oplus_wpc_set_wls_pg_value(1);
	msleep(100);
	oplus_wpc_set_booster_en_val(1);
	oplus_wpc_set_ext2_wireless_otg_en_val(1);
	msleep(100);
	oplus_wpc_set_tx_start();
	return;
#endif*/
}

/*#if 0
static void oplus_switch_to_wired_charge(struct battery_chg_dev *bcdev)
{
	oplus_wpc_dis_wireless_chg(1);
	if (oplus_wpc_get_wireless_charge_start() == true) {
		//oplus_wpc_dis_wireless_chg(1);
		oplus_wpc_set_vbat_en_val(0);
		msleep(100);
		oplus_wpc_set_wrx_en_value(0);
		oplus_wpc_set_wls_pg_value(1);
	}

	if (oplus_wpc_get_otg_charging()) {
		//oplus_wpc_dis_wireless_chg(1);
		mp2650_wireless_set_mps_otg_en_val(0);
		oplus_wpc_set_wrx_otg_en_value(0);

		cancel_delayed_work_sync(&bcdev->wait_wired_charge_on);
		schedule_delayed_work(&bcdev->wait_wired_charge_on, msecs_to_jiffies(100));
	}
}
#endif*/

static void oplus_wait_wired_charge_off_work(struct work_struct *work)
{
/*#if 0
	chg_err("[OPLUS_CHG][%s]<~WPC~> wait_wired_charge_off\n", __func__);
	oplus_wpc_dis_wireless_chg(0);
	oplus_wpc_set_rtx_function_prepare();
	oplus_wpc_set_rtx_function(true);
	return;
#endif*/
}

/*#if 0
static void oplus_switch_from_wired_charge(struct battery_chg_dev *bcdev)
{
	if (oplus_wpc_get_otg_charging()) {
		oplus_wpc_set_booster_en_val(0);
		oplus_wpc_set_ext2_wireless_otg_en_val(0);
		oplus_wpc_set_wls_pg_value(0);
		cancel_delayed_work_sync(&bcdev->wait_wired_charge_off);
		schedule_delayed_work(&bcdev->wait_wired_charge_off, msecs_to_jiffies(100));
	} else {
		if (oplus_wpc_get_fw_updating() == false)
			oplus_wpc_dis_wireless_chg(0);
	}
}
#endif*/

bool oplus_get_wired_otg_online(void)
{
/*#if 0
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return false;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (bcdev) {
		if (bcdev->wls_fw_update == true)
			return false;
		return bcdev->otg_online;
	}
#endif*/
	return false;
}

bool oplus_get_wired_chg_present(void)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return false;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;

	return bcdev->usb_in_status;
}
#endif /*OPLUS_FEATURE_CHG_BASIC*/

static void battery_chg_state_cb(void *priv, enum pmic_glink_state state)
{
	struct battery_chg_dev *bcdev = priv;

	pr_debug("state: %d\n", state);

	atomic_set(&bcdev->state, state);
	if (state == PMIC_GLINK_STATE_UP)
		schedule_work(&bcdev->subsys_up_work);
}

/**
 * qti_battery_charger_get_prop() - Gets the property being requested
 *
 * @name: Power supply name
 * @prop_id: Property id to be read
 * @val: Pointer to value that needs to be updated
 *
 * Return: 0 if success, negative on error.
 */
int qti_battery_charger_get_prop(const char *name,
				enum battery_charger_prop prop_id, int *val)
{
	struct power_supply *psy;
	struct battery_chg_dev *bcdev;
	struct psy_state *pst;
	int rc = 0;

	if (prop_id >= BATTERY_CHARGER_PROP_MAX)
		return -EINVAL;

	if (strcmp(name, "battery") && strcmp(name, "usb") &&
	    strcmp(name, "wireless"))
		return -EINVAL;

	psy = power_supply_get_by_name(name);
	if (!psy)
		return -ENODEV;

	bcdev = power_supply_get_drvdata(psy);
	if (!bcdev)
		return -ENODEV;

	power_supply_put(psy);

	switch (prop_id) {
	case BATTERY_RESISTANCE:
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
		rc = read_property_id(bcdev, pst, BATT_RESISTANCE);
		if (!rc)
			*val = pst->prop[BATT_RESISTANCE];
		break;
	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL(qti_battery_charger_get_prop);

static bool validate_message(struct battery_charger_resp_msg *resp_msg,
				size_t len)
{
	if (len != sizeof(*resp_msg)) {
		pr_err("Incorrect response length %zu for opcode %#x\n", len,
			resp_msg->hdr.opcode);
		return false;
	}

	if (resp_msg->ret_code) {
		pr_err("Error in response for opcode %#x prop_id %u, rc=%d\n",
			resp_msg->hdr.opcode, resp_msg->property_id,
			(int)resp_msg->ret_code);
		return false;
	}

	return true;
}

static bool current_message_check(struct battery_charger_resp_msg *resp_msg,
				size_t len)
{
	if (len != sizeof(*resp_msg)) {
		pr_err("Incorrect response length %zu for opcode %#x\n", len,
			resp_msg->hdr.opcode);
		return false;
	}

	if (resp_msg->ret_code == BATTMNGR_EFAILED &&
	    resp_msg->property_id == BATT_CURR_NOW) {
		return true;
	}

	return false;
}

#define MODEL_DEBUG_BOARD	"Debug_Board"
static void handle_message(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct battery_charger_resp_msg *resp_msg = data;
	struct battery_model_resp_msg *model_resp_msg = data;
	struct wireless_fw_check_resp *fw_check_msg;
	struct wireless_fw_push_buf_resp *fw_resp_msg;
	struct wireless_fw_update_status *fw_update_msg;
	struct wireless_fw_get_version_resp *fw_ver_msg;
	struct psy_state *pst;
	bool ack_set = false;

	switch (resp_msg->hdr.opcode) {
	case BC_BATTERY_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

		/* Handle model response uniquely as it's a string */
		if (pst->model && len == sizeof(*model_resp_msg)) {
			memcpy(pst->model, model_resp_msg->model, MAX_STR_LEN);
			ack_set = true;
			bcdev->debug_battery_detected = !strcmp(pst->model,
					MODEL_DEBUG_BOARD);
			break;
		}

		/* Other response should be of same type as they've u32 value */
		if (validate_message(resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
			break;
		}

		if (current_message_check(resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			ack_set = true;
		}

		break;
	case BC_USB_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		if (validate_message(resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_WLS_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_WLS];
		if (validate_message(resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_BATTERY_STATUS_SET:
	case BC_USB_STATUS_SET:
	case BC_WLS_STATUS_SET:
		if (validate_message(data, len))
			ack_set = true;

		break;
	case BC_SET_NOTIFY_REQ:
	case BC_SHUTDOWN_NOTIFY:
		/* Always ACK response for notify request */
		ack_set = true;
		break;
	case BC_WLS_FW_CHECK_UPDATE:
		if (len == sizeof(*fw_check_msg)) {
			fw_check_msg = data;
			if (fw_check_msg->ret_code == 1)
				bcdev->wls_fw_update_reqd = true;
			ack_set = true;
		} else {
			pr_err("Incorrect response length %zu for wls_fw_check_update\n",
				len);
		}
		break;
	case BC_WLS_FW_PUSH_BUF_RESP:
		if (len == sizeof(*fw_resp_msg)) {
			fw_resp_msg = data;
			if (fw_resp_msg->fw_update_status == 1)
				complete(&bcdev->fw_buf_ack);
		} else {
			pr_err("Incorrect response length %zu for wls_fw_push_buf_resp\n",
				len);
		}
		break;
	case BC_WLS_FW_UPDATE_STATUS_RESP:
		if (len == sizeof(*fw_update_msg)) {
			fw_update_msg = data;
			if (fw_update_msg->fw_update_done == 1)
				complete(&bcdev->fw_update_ack);
			else
				pr_err("Wireless FW update not done %d\n",
					(int)fw_update_msg->fw_update_done);
		} else {
			pr_err("Incorrect response length %zu for wls_fw_update_status_resp\n",
				len);
		}
		break;
	case BC_WLS_FW_GET_VERSION:
		if (len == sizeof(*fw_ver_msg)) {
			fw_ver_msg = data;
			bcdev->wls_fw_version = fw_ver_msg->fw_version;
			ack_set = true;
		} else {
			pr_err("Incorrect response length %zu for wls_fw_get_version\n",
				len);
		}
		break;
	default:
		pr_err("Unknown opcode: %u\n", resp_msg->hdr.opcode);
		break;
	}

	if (ack_set)
		complete(&bcdev->ack);
}

static struct power_supply_desc usb_psy_desc;

static void battery_chg_update_usb_type_work(struct work_struct *work)
{
#ifdef OPLUS_FEATURE_CHG_BASIC
	int chg_type;
	int sub_chg_type;
	static int last_usb_adap_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
#endif
#ifndef OPLUS_FEATURE_CHG_BASIC
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, usb_type_work);
#else
	struct delayed_work *dwork = to_delayed_work(work);
	struct battery_chg_dev *bcdev = container_of(dwork,
					struct battery_chg_dev, usb_type_work);
#endif
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc < 0) {
		pr_err("Failed to read USB_ADAP_TYPE rc=%d\n", rc);
		return;
	}

	/* Reset usb_icl_ua whenever USB adapter type changes */
	if (pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_SDP &&
	    pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_PD)
		bcdev->usb_icl_ua = 0;

	pr_debug("usb_adap_type: %u\n", pst->prop[USB_ADAP_TYPE]);

#ifdef OPLUS_FEATURE_CHG_BASIC
	chg_type = opchg_get_charger_type();
	sub_chg_type = oplus_chg_get_charger_subtype();
	bcdev->real_chg_type = chg_type | (sub_chg_type << 8);
#endif

	switch (pst->prop[USB_ADAP_TYPE]) {
	case POWER_SUPPLY_USB_TYPE_SDP:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	case POWER_SUPPLY_USB_TYPE_DCP:
	case POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID:
	case QTI_POWER_SUPPLY_USB_TYPE_HVDCP:
	case QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3:
	case QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case POWER_SUPPLY_USB_TYPE_CDP:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case POWER_SUPPLY_USB_TYPE_ACA:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_ACA;
		break;
	case POWER_SUPPLY_USB_TYPE_C:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_TYPE_C;
		break;
	case POWER_SUPPLY_USB_TYPE_PD:
	case POWER_SUPPLY_USB_TYPE_PD_DRP:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;
		if (!bcdev->pd_type_checked) {
			bcdev->pd_type_checked = true;
			chg_err("start check pd type\n");
			schedule_delayed_work(&bcdev->pd_type_check_work, OPLUS_PD_TYPE_CHECK_INTERVAL);
		}
		break;
	case POWER_SUPPLY_USB_TYPE_PD_PPS:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;
		break;
#ifdef OPLUS_FEATURE_CHG_BASIC
	case POWER_SUPPLY_USB_TYPE_PD_SDP:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
#endif
	default:
#ifdef OPLUS_FEATURE_CHG_BASIC
		rc = read_property_id(bcdev, pst, USB_ONLINE);
		if (rc < 0) {
			pr_err("Failed to read USB_ONLINE rc=%d\n", rc);
			return;
		}
		if (pst->prop[USB_ONLINE] == 0) {
			/*pr_err("lizhijie USB_ONLINE 00000\n");*/
			if (!(((oplus_chg_get_voocphy_support() == ADSP_VOOCPHY ||
				oplus_chg_get_voocphy_support() == AP_SINGLE_CP_VOOCPHY ||
				oplus_chg_get_voocphy_support() == AP_DUAL_CP_VOOCPHY) &&
				g_oplus_chip && g_oplus_chip->mmi_fastchg == 0) ||
				oplus_quirks_keep_connect_status() || bcdev->usb_in_status))
				usb_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		}
#else
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
#endif
		break;
	}
#ifdef OPLUS_FEATURE_CHG_BASIC
	if (g_oplus_chip) {
		if (usb_psy_desc.type != POWER_SUPPLY_TYPE_UNKNOWN) {
			if (chg_type == POWER_SUPPLY_TYPE_USB_DCP
				&& pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_DCP
				&& bcdev->hvdcp_disable == false) {
				usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			} else if (chg_type == POWER_SUPPLY_TYPE_USB
				&& pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_SDP) {
				usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
			} else if (chg_type == POWER_SUPPLY_TYPE_USB_CDP
				&& pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_CDP) {
				usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			}
		}

		if (usb_psy_desc.type == POWER_SUPPLY_TYPE_UNKNOWN &&
		    (oplus_chg_get_voocphy_support() != ADSP_VOOCPHY)) {
			chg_err("!!! usb_psy_desc.type: [%d]to_warm[%d]dummy[%d]to_normal[%d]started[%d]\n",
				usb_psy_desc.type,
				oplus_vooc_get_fastchg_to_warm(),
				oplus_vooc_get_fastchg_dummy_started(),
				oplus_vooc_get_fastchg_to_normal(),
				oplus_vooc_get_fastchg_started());
			if (oplus_vooc_get_fastchg_to_warm() == true ||
			    oplus_vooc_get_fastchg_dummy_started() == true ||
			    oplus_vooc_get_fastchg_to_normal() == true ||
			    oplus_vooc_get_fastchg_started() == true) {
				usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
				chg_err("!!! usb_psy_desc.type: [%d]\n", usb_psy_desc.type);
			}
		}

		if (!((last_usb_adap_type == POWER_SUPPLY_USB_TYPE_PD_PPS
		    || last_usb_adap_type == POWER_SUPPLY_USB_TYPE_PD
		    || last_usb_adap_type == POWER_SUPPLY_USB_TYPE_PD_DRP)
		    && pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_PD_PPS
		    && pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_PD
		    && pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_PD_DRP)
		    && pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_UNKNOWN
		    && last_usb_adap_type != pst->prop[USB_ADAP_TYPE]) {
			chg_err("oplus_chg_wake_update_work\n");
			power_supply_changed(g_oplus_chip->batt_psy);
			oplus_chg_wake_update_work();
		}
		last_usb_adap_type = pst->prop[USB_ADAP_TYPE];
	}
#endif
}

static void handle_notification(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
#ifdef OPLUS_FEATURE_CHG_BASIC
	int chg_type;
	int sub_chg_type;
#endif
	struct battery_charger_notify_msg *notify_msg = data;
	struct psy_state *pst = NULL;
	u32 hboost_vmax_mv, notification;

	if (len != sizeof(*notify_msg)) {
		pr_err("Incorrect response length %zu\n", len);
		return;
	}

	notification = notify_msg->notification;
	pr_debug("notification: %#x\n", notification);
	if ((notification & 0xffff) == BC_HBOOST_VMAX_CLAMP_NOTIFY) {
		hboost_vmax_mv = (notification >> 16) & 0xffff;
		raw_notifier_call_chain(&hboost_notifier, VMAX_CLAMP, &hboost_vmax_mv);
		pr_debug("hBoost is clamped at %u mV\n", hboost_vmax_mv);
		return;
	}

	switch (notification) {
	case BC_BATTERY_STATUS_GET:
	case BC_GENERIC_NOTIFY:
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
		break;
	case BC_USB_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_USB];
#ifndef OPLUS_FEATURE_CHG_BASIC
		schedule_work(&bcdev->usb_type_work);
#else
		schedule_delayed_work(&bcdev->usb_type_work, 0);
#endif
		break;
	case BC_WLS_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_WLS];
		break;
#ifdef OPLUS_FEATURE_CHG_BASIC
	case BC_PD_SVOOC:
		bcdev->pd_svooc = true;
		g_oplus_chip->pd_svooc = true;
		chg_err("!!!:%s, set pd_svooc[%d]\n", __func__, bcdev->pd_svooc);
		break;
	case BC_ABNORMAL_PD_SVOOC_ADAPTER:
		printk(KERN_ERR "!!!:%s, is_abnormal_adapter\n", __func__);
		g_oplus_chip->is_abnormal_adapter = true;
		break;
	case BC_VOOC_STATUS_GET:
		schedule_delayed_work(&bcdev->adsp_voocphy_status_work, 0);
		break;
	case BC_OTG_ENABLE:
		chg_err("!!!!!enable otg\n");
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		bcdev->otg_online = true;
		bcdev->pd_svooc = false;
		schedule_delayed_work(&bcdev->otg_vbus_enable_work, 0);
		break;
	case BC_OTG_DISABLE:
		chg_err("!!!!!disable otg\n");
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		bcdev->otg_online = false;
		schedule_delayed_work(&bcdev->otg_vbus_enable_work, 0);
		break;
	case BC_ADSP_NOTIFY_TRACK:
		pr_info("!!!!!adsp track notify\n");
		schedule_delayed_work(&bcdev->adsp_track_notify_work, 0);
		break;
	case BC_VOOC_VBUS_ADC_ENABLE:
		chg_err("!!!!!vooc_vbus_adc_enable\n");
		bcdev->adsp_voocphy_err_check = true;
		oplus_adsp_voocphy_set_fastchg_start(true);
		cancel_delayed_work_sync(&bcdev->adsp_voocphy_err_work);
		schedule_delayed_work(&bcdev->adsp_voocphy_err_work, msecs_to_jiffies(8500));
		if (is_ext_chg_ops()) {
			oplus_chg_disable_charge();
			oplus_chg_suspend_charger();/*excute in glink loop for real time*/
		} else {
			schedule_delayed_work(&bcdev->vbus_adc_enable_work, 0);/*excute in work to avoid glink dead loop*/
		}
		break;
	case BC_CID_DETECT:
		chg_err("!!!!!cid detect || no detect\n");
		schedule_delayed_work(&bcdev->cid_status_change_work, 0);
		break;
	case BC_QC_DETECT:
		chg_type = opchg_get_charger_type();
		sub_chg_type = oplus_chg_get_charger_subtype();
		bcdev->real_chg_type = chg_type | (sub_chg_type << 8);
		bcdev->hvdcp_detect_ok = true;
		break;
	case BC_TYPEC_STATE_CHANGE:
		chg_err("!!!!!typec_state_change_work\n");
		schedule_delayed_work(&bcdev->typec_state_change_work, 0);
		break;
	case BC_PLUGIN_IRQ:
		chg_err("!!!!!oplus_plugin_irq_work\n");
		bcdev->adspvoocphy_plugin_cnt++;
		if (!plugin_cnt_test)
			schedule_delayed_work(&bcdev->plugin_irq_work, 0);
		break;
	case BC_APSD_DONE:
		chg_err("!!!!!oplus_apsd_done_work\n");
		schedule_delayed_work(&bcdev->apsd_done_work, 0);
		break;
	case BC_CHG_STATUS_GET:
		schedule_delayed_work(&bcdev->chg_status_send_work, 0);
		break;
	case BC_ADSP_NOTIFY_AP_SUSPEND_CHG:
		chg_err("!!!!!oplus_apsd_notify_ap_suspend_chg\n");
		oplus_chg_set_adsp_notify_ap_suspend();
		break;
	case BC_PD_SOFT_RESET:
		chg_err("!!!!!PD hard reset happend\n");
		/*oplus_pps_hard_reset_notify();*/
		break;
	case BC_CHG_STATUS_SET:
		schedule_delayed_work(&bcdev->unsuspend_usb_work, 0);
		break;
	case BC_ADSP_NOTIFY_AP_CP_BYPASS_INIT:
		printk(KERN_ERR "!!!!!BC_ADSP_NOTIFY_AP_CP_BYPASS_INIT\n");
		if (g_oplus_chip && g_oplus_chip->vooc_project == DUAL_BATT_150W)
			oplus_cp_cfg_mode_init(PPS_BYPASS_MODE);
		if (g_oplus_chip && (oplus_pps_get_support_type() == PPS_SUPPORT_2CP ||
		    oplus_pps_get_support_type() == PPS_SUPPORT_3CP))
			oplus_pps_cp_mode_init(PPS_BYPASS_MODE);
		break;
	case BC_ADSP_NOTIFY_AP_CP_MOS_ENABLE:
		printk(KERN_ERR "!!!!!BC_ADSP_NOTIFY_AP_CP_MOS_ENABLE\n");
		if (g_oplus_chip && g_oplus_chip->vooc_project == DUAL_BATT_150W) {
			oplus_cp_master_cp_enable(1);
			oplus_cp_slave_cp_enable(1);
		}
		if (g_oplus_chip && (oplus_pps_get_support_type() == PPS_SUPPORT_2CP ||
		    oplus_pps_get_support_type() == PPS_SUPPORT_3CP))
			oplus_pps_set_svooc_mos_enable(true);

		break;
	case BC_ADSP_NOTIFY_AP_CP_MOS_DISABLE:
		printk(KERN_ERR "!!!!!BC_ADSP_NOTIFY_AP_CP_MOS_DISABLE\n");
		if (g_oplus_chip && g_oplus_chip->vooc_project == DUAL_BATT_150W) {
			oplus_cp_master_cp_enable(0);
			oplus_cp_slave_cp_enable(0);
		}
		if (g_oplus_chip && (oplus_pps_get_support_type() == PPS_SUPPORT_2CP ||
		    oplus_pps_get_support_type() == PPS_SUPPORT_3CP))
			oplus_pps_set_pps_mos_enable(false);

		break;
	case BC_PPS_OPLUS:
		printk(KERN_ERR "!!!!!BC_PPS_OPLUS\n");
		oplus_chg_wake_update_work();
		break;
#endif
	default:
		break;
	}

	if (pst && pst->psy) {
		/*
		 * For charger mode, keep the device awake at least for 50 ms
		 * so that device won't enter suspend when a non-SDP charger
		 * is removed. This would allow the userspace process like
		 * "charger" to be able to read power supply uevents to take
		 * appropriate actions (e.g. shutting down when the charger is
		 * unplugged).
		 */
#ifndef OPLUS_FEATURE_CHG_BASIC
		power_supply_changed(pst->psy);
#endif
		pm_wakeup_dev_event(bcdev->dev, 50, true);
	}
}

static int battery_chg_callback(void *priv, void *data, size_t len)
{
	struct pmic_glink_hdr *hdr = data;
	struct battery_chg_dev *bcdev = priv;

	pr_debug("owner: %u type: %u opcode: %#x len: %zu\n", hdr->owner,
		hdr->type, hdr->opcode, len);

	if (!bcdev->initialized) {
		pr_debug("Driver initialization failed: Dropping glink callback message: state %d\n",
			 bcdev->state);
		return 0;
	}

	if (hdr->opcode == BC_NOTIFY_IND)
		handle_notification(bcdev, data, len);
#ifdef OPLUS_FEATURE_CHG_BASIC
	else if (hdr->opcode == OEM_OPCODE_READ_BUFFER)
		handle_oem_read_buffer(bcdev, data, len);
	else if (hdr->opcode == BCC_OPCODE_READ_BUFFER)
		handle_bcc_read_buffer(bcdev, data, len);
	else if (hdr->opcode == TRACK_OPCODE_READ_BUFFER)
		handle_adsp_track_read_buffer(bcdev, data, len);
	else if (hdr->opcode == OEM_OPCODE_WDOG_READ_BUFFER)
		handle_oem_read_pmic_buffer(bcdev, data, len);
#endif
#ifdef OPLUS_FEATURE_CHG_BASIC
	else if (hdr->opcode == PPS_OPCODE_READ_BUFFER)
		handle_pps_read_buffer(bcdev, data, len);
#endif
	else
		handle_message(bcdev, data, len);

	return 0;
}

#ifdef OPLUS_FEATURE_CHG_BASIC
#define WLS_SK_DELAY_UNLOCK_TIME 100
static void oplus_chg_wls_status_keep_clean_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct battery_chg_dev *bcdev =
		container_of(dwork, struct battery_chg_dev, status_keep_clean_work);
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (chip == NULL) {
		chg_err("oplus_chg_chip is NULL\n");
	} else {
		if (chip->wls_status_keep == WLS_SK_BY_HAL) {
			chip->wls_status_keep = WLS_SK_WAIT_TIMEOUT;
			schedule_delayed_work(&bcdev->status_keep_clean_work, msecs_to_jiffies(5000));
			return;
		}

		chip->wls_status_keep = WLS_SK_NULL;
		power_supply_changed(bcdev->psy_list[PSY_TYPE_BATTERY].psy);
		schedule_delayed_work(&bcdev->status_keep_delay_unlock_work,
			msecs_to_jiffies(WLS_SK_DELAY_UNLOCK_TIME));
	}
}

static void oplus_chg_wls_status_keep_delay_unlock_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct battery_chg_dev *bcdev =
		container_of(dwork, struct battery_chg_dev, status_keep_delay_unlock_work);

	if (bcdev->status_wake_lock_on) {
		chg_err("release status_wake_lock\n");
		__pm_relax(bcdev->status_wake_lock);
		bcdev->status_wake_lock_on = false;
	}
}

static bool oplus_chg_wls_is_present(struct oplus_chg_chip *chip)
{
	union oplus_chg_mod_propval temp_val = {0};
	bool present = false;
	int rc;

	if (!is_wls_ocm_available(chip))
		return false;

	rc = oplus_chg_mod_get_property(chip->wls_ocm,
					OPLUS_CHG_PROP_ONLINE_KEEP, &temp_val);
	if (rc < 0)
		present = false;
	else
		present = temp_val.intval;
	rc = oplus_chg_mod_get_property(chip->wls_ocm, OPLUS_CHG_PROP_PRESENT,
					&temp_val);
	if (rc >= 0)
		present = present || (!!temp_val.intval);

	return present;
}

static bool oplus_chg_wls_support_bcc(struct oplus_chg_chip *chip)
{
	union oplus_chg_mod_propval temp_val = {0};
	bool support = false;
	int rc;

	if (!is_wls_ocm_available(chip))
		return false;

	rc = oplus_chg_mod_get_property(chip->wls_ocm,
					OPLUS_CHG_PROP_BCC_SUPPORT, &temp_val);
	if (rc < 0)
		support = false;
	else
		support = temp_val.intval;

	return support;
}
#endif

#define KEEP_CLEAN_INTERVAL	2000
static int wls_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
#ifndef OPLUS_FEATURE_CHG_BASIC
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_WLS];
	int prop_id, rc;

	pval->intval = -ENODATA;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	pval->intval = pst->prop[prop_id];
#else /*OPLUS_FEATURE_CHG_BASIC*/
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct oplus_chg_chip *chip = g_oplus_chip;
	static bool pre_wls_online;
	static bool pre_wls_online_lcm = 0;
	int rc = 0;

	if (!chip || !is_wls_ocm_available(chip)) {
		return -ENODEV;
	}
	if (bcdev == NULL)
		return -ENODEV;

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		pval->intval = oplus_chg_wls_is_present(chip);
/*#ifdef OPLUS_FEATURE_CHG_BASIC*/
		if (pre_wls_online_lcm != pval->intval) {
			if (bcdev && bcdev->ctrl_lcm_frequency.work.func) {
				mod_delayed_work(system_highpri_wq, &bcdev->ctrl_lcm_frequency, 50);
			}
		}
		pre_wls_online_lcm = pval->intval;
/*#endif*/
		if (oplus_get_wired_chg_present() == true) {
			pval->intval = 0;
			pre_wls_online = pval->intval;
			chip->wls_status_keep = WLS_SK_NULL;
			break;
		}
		if (chip->wls_status_keep != WLS_SK_NULL) {
			pval->intval = 1;
		} else {
			if (pre_wls_online && pval->intval == 0) {
				if (!bcdev->status_wake_lock_on) {
					chg_err("acquire status_wake_lock\n");
					__pm_stay_awake(bcdev->status_wake_lock);
					bcdev->status_wake_lock_on = true;
				}
				pre_wls_online = pval->intval;
				chip->wls_status_keep = WLS_SK_BY_KERNEL;
				pval->intval = 1;
				schedule_delayed_work(&bcdev->status_keep_clean_work, msecs_to_jiffies(KEEP_CLEAN_INTERVAL));
			} else {
				pre_wls_online = pval->intval;
				if (bcdev->status_wake_lock_on) {
					cancel_delayed_work_sync(&bcdev->status_keep_clean_work);
					schedule_delayed_work(&bcdev->status_keep_clean_work, 0);
				}
			}
		}

		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = oplus_chg_mod_get_property(chip->wls_ocm,
			OPLUS_CHG_PROP_VOLTAGE_NOW,
			(union oplus_chg_mod_propval *)pval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = oplus_chg_mod_get_property(chip->wls_ocm,
			OPLUS_CHG_PROP_VOLTAGE_MAX,
			(union oplus_chg_mod_propval *)pval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = oplus_chg_mod_get_property(chip->wls_ocm,
			OPLUS_CHG_PROP_CURRENT_NOW,
			(union oplus_chg_mod_propval *)pval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		rc = oplus_chg_mod_get_property(chip->wls_ocm,
			OPLUS_CHG_PROP_CURRENT_MAX,
			(union oplus_chg_mod_propval *)pval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		pval->intval = oplus_get_report_batt_temp() - chip->offset_temp;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = oplus_chg_mod_get_property(chip->wls_ocm,
			OPLUS_CHG_PROP_PRESENT,
			(union oplus_chg_mod_propval *)pval);
		break;
	default:
		pr_err("get prop %d is not supported\n", prop);
		return -EINVAL;
	}
	if (rc < 0) {
		pr_err("Couldn't get prop %d rc = %d\n", prop, rc);
		return -ENODATA;
	}
#endif /*OPLUS_FEATURE_CHG_BASIC*/

	return 0;
}

static int wls_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	return 0;
}

static int wls_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	return 0;
}

static enum power_supply_property wls_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_TEMP,
#ifdef OPLUS_FEATURE_CHG_BASIC
	POWER_SUPPLY_PROP_PRESENT,
#endif
};

static const struct power_supply_desc wls_psy_desc = {
	.name			= "wireless",
	.type			= POWER_SUPPLY_TYPE_WIRELESS,
	.properties		= wls_props,
	.num_properties		= ARRAY_SIZE(wls_props),
	.get_property		= wls_psy_get_prop,
	.set_property		= wls_psy_set_prop,
	.property_is_writeable	= wls_psy_prop_is_writeable,
};

static const char *get_wls_type_name(u32 wls_type)
{
	if (wls_type >= ARRAY_SIZE(qc_power_supply_wls_type_text))
		return "Unknown";

	return qc_power_supply_wls_type_text[wls_type];
}

static const char *get_usb_type_name(u32 usb_type)
{
	u32 i;

	if (usb_type >= QTI_POWER_SUPPLY_USB_TYPE_HVDCP &&
	    usb_type <= QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5) {
		for (i = 0; i < ARRAY_SIZE(qc_power_supply_usb_type_text);
		     i++) {
			if (i == (usb_type - QTI_POWER_SUPPLY_USB_TYPE_HVDCP))
				return qc_power_supply_usb_type_text[i];
		}
		return "Unknown";
	}

	for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
		if (i == usb_type)
			return power_supply_usb_type_text[i];
	}

	return "Unknown";
}

#ifndef OPLUS_FEATURE_CHG_BASIC
static int usb_psy_set_icl(struct battery_chg_dev *bcdev, u32 prop_id, int val)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	u32 temp;
	int rc;

	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc < 0) {
		pr_err("Failed to read prop USB_ADAP_TYPE, rc=%d\n", rc);
		return rc;
	}

	/* Allow this only for SDP or USB_PD and not for other charger types */
	if (pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_SDP &&
	    pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_PD)
		return -EINVAL;

	/*
	 * Input current limit (ICL) can be set by different clients. E.g. USB
	 * driver can request for a current of 500/900 mA depending on the
	 * port type. Also, clients like EUD driver can pass 0 or -22 to
	 * suspend or unsuspend the input for its use case.
	 */

	temp = val;
	if (val < 0)
		temp = UINT_MAX;

	rc = write_property_id(bcdev, pst, prop_id, temp);
	if (rc < 0) {
		pr_err("Failed to set ICL (%u uA) rc=%d\n", temp, rc);
	} else {
		pr_debug("Set ICL to %u\n", temp);
		bcdev->usb_icl_ua = temp;
	}

	return rc;
}
#endif /*OPLUS_FEATURE_CHG_BASIC*/

#ifdef OPLUS_FEATURE_CHG_BASIC
void oplus_adsp_voocphy_set_current_level(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	int cool_down = 0;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	cool_down = oplus_chg_get_cool_down_status();
	rc = write_property_id(bcdev, pst, BATT_SET_COOL_DOWN, cool_down);
	if (rc) {
		chg_err("set current level fail, rc=%d\n", rc);
		return;
	}

	chg_debug("ap set current level[%d] to adsp voocphy\n", cool_down);
}

void oplus_adsp_voocphy_set_match_temp(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	int match_temp = 0;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	match_temp = oplus_chg_match_temp_for_chging();
	rc = write_property_id(bcdev, pst, BATT_SET_MATCH_TEMP, match_temp);
	if (rc) {
		chg_err("set match temp fail, rc=%d\n", rc);
		return;
	}

	/*chg_debug("ap set match temp[%d] to voocphy\n", match_temp);*/
}

int oplus_set_bcc_curr_to_voocphy(int bcc_curr)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	rc = write_property_id(bcdev, pst, BATT_SET_BCC_CURRENT, bcc_curr);
	if (rc) {
		chg_err("set bcc current fail, rc=%d\n", rc);
		return rc;
	}

	chg_debug("ap set bcc current[%d] to voocphy\n", bcc_curr);
	return rc;
}

static int oplus_usb_psy_get_plugin_cnt(u32 *plug_cnt)
{
	int rc = 0;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_PLUGIN_CNT);
	if (rc) {
		chg_err("read adsp usb_plugin_cnt fail, rc=%d\n", rc);
		return -EINVAL;
	}
	*plug_cnt = pst->prop[USB_PLUGIN_CNT];
	return rc;
}

static int oplus_usb_psy_get_plugin_to_align_plugin_irq(void)
{
	int rc = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_PLUGIN_CNT, (int)1);
	if (rc) {
		chg_err("set align plugin/out PLUGIN_IRQ is fail\n");
		return -EINVAL;
	}
	return rc;
}

static int oplus_get_abnormal_adapter_disconnect_cnt(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip || !chip->pmic_spmi.bcdev_chip) {
		chg_err("chip or bcdev is NULL!\n");
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	return bcdev->abnormal_adapter_disconnect_cnt;
}

static int oplus_chg_get_adsp_abnormal_adapter_disconnect_cnt(void)
{
	int rc = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_GET_ABNORMAL_ADAPTER_DISCONNECT_CNT);
	if (rc) {
		chg_err("read adsp vooc_disconnect_counts fail, rc=%d\n", rc);
		return -EINVAL;
	}
	return pst->prop[USB_GET_ABNORMAL_ADAPTER_DISCONNECT_CNT];
}

static int oplus_chg_get_adsp_pre_is_abnormal_adapter(void)
{
	int rc = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_GET_PRE_IS_ABNORMAL_ADAPTER);
	if (rc) {
		chg_err("read adsp pre_is_abnormal_adapter fail, rc=%d\n", rc);
		return -EINVAL;
	}
	return pst->prop[USB_GET_PRE_IS_ABNORMAL_ADAPTER];
}

#endif /*OPLUS_FEATURE_CHG_BASIC*/

#ifdef OPLUS_FEATURE_CHG_BASIC
#define VBUS_VOTAGE_3000MV	3000
#define BATT_TEMP_70C		700
#endif
#define CHARGER_PRESENT_VOLT_MV	3500
static int usb_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = NULL;
	int prop_id, rc;
#ifdef OPLUS_FEATURE_CHG_BASIC
	static int pre_usb_in_status = 0;
#endif

	if (bcdev == NULL)
		return -ENODEV;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	pval->intval = -ENODATA;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

#ifdef OPLUS_FEATURE_CHG_BASIC
	if (USB_ONLINE == prop_id) {
		pval->intval = bcdev->usb_in_status;
		if (pre_usb_in_status != bcdev->usb_in_status) {
			chg_err("get usb online[%d]\n", pval->intval);
			if (bcdev && bcdev->ctrl_lcm_frequency.work.func) {
				mod_delayed_work(system_highpri_wq, &bcdev->ctrl_lcm_frequency, 50);
			}
			if ((oplus_get_wired_chg_present() == false)
				&& (g_oplus_chip->charger_volt < CHARGER_PRESENT_VOLT_MV)) {
					bcdev->pd_svooc = false; /*remove svooc flag*/
			}
		}

		if (oplus_chg_get_voocphy_support() != ADSP_VOOCPHY) {
			if (pval->intval == 2 ||
			    (oplus_vooc_get_fastchg_started() == true) ||
			    (oplus_vooc_get_fastchg_to_warm() == true) ||
			    (oplus_vooc_get_fastchg_dummy_started() == true) ||
			    (oplus_vooc_get_fastchg_to_normal() == true) ||
			    (g_oplus_chip && g_oplus_chip->charger_volt > VBUS_VOTAGE_3000MV &&
			    g_oplus_chip->tbatt_temp > BATT_TEMP_70C)) {
				chg_err("fastchg on, hold usb online state: \n");
				pval->intval = 1;
			} else if (pval->intval == 0 && g_oplus_chip && g_oplus_chip->mmi_fastchg == 0) {
				chg_err("mmi_fastchg=0, hold usb online state\n");
				pval->intval = 1;
			} else if (oplus_quirks_keep_connect_status()) {
				pval->intval = 1;
				chg_err("oplus_chg_get_voocphy_support get usb online[%d]\n", pval->intval);
			} else if ((usb_psy_desc.type != POWER_SUPPLY_TYPE_UNKNOWN) &&
			    (g_oplus_chip && g_oplus_chip->charger_volt < VBUS_VOTAGE_3000MV)) {
				schedule_delayed_work(&bcdev->usb_type_work, 0);
				chg_err("fastchg not, clear usb type: \n");
			}
		} else {
			if (pval->intval == 0 && g_oplus_chip && g_oplus_chip->mmi_fastchg == 0) {
				chg_err("mmi_fastchg=0, hold usb online state\n");
				pval->intval = 1;
			}
		}

		pre_usb_in_status = bcdev->usb_in_status;
		if (bcdev->usb_in_status == 1 && oplus_chg_wls_is_present(g_oplus_chip) == true) {
			pval->intval = 0;
			chg_err("wls get usb online[%d]\n", pval->intval);
		}
		if (oplus_quirks_keep_connect_status()) {
			pval->intval = 1;
			chg_err("oplus_quirks_keep_connect_status get usb online[%d]\n", pval->intval);
		}
		return 0;
	}
#endif

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	pval->intval = pst->prop[prop_id];
	if (prop == POWER_SUPPLY_PROP_TEMP)
		pval->intval = DIV_ROUND_CLOSEST((int)pval->intval, 10);

	if (prop == POWER_SUPPLY_PROP_VOLTAGE_NOW) {
		if (is_ext_mp2650_chg_ops() &&
		    g_oplus_chip->chg_ops->get_charger_volt) {
			pval->intval = g_oplus_chip->chg_ops->get_charger_volt() * 1000;
		} else {
			if (pval->intval < 0)
				pval->intval = 0;
		}
	}
	return 0;
}

#ifdef OPLUS_FEATURE_CHG_BASIC
int oplus_get_typec_cc_orientation(void)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;
	int rc = 0;
	int typec_cc_orientation = 0;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return 0;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_TYPEC_CC_ORIENTATION);
	if (rc < 0) {
		chg_err("!!![OPLUS_CHG] read typec_cc_orientation fail\n");
		return 0;
	}
	typec_cc_orientation = pst->prop[USB_TYPEC_CC_ORIENTATION];
	chg_err("!!![OPLUS_CHG] typec_cc_orientation=%d\n", typec_cc_orientation);

	return typec_cc_orientation;
}

int oplus_get_otg_switch_status(void)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;
	int rc = 0;
	int otg_switch_status = 0;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return 0;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	if (get_otg_scheme(chip) != OTG_SCHEME_CCDETECT_GPIO) {
		rc = read_property_id(bcdev, pst, USB_OTG_SWITCH);
		if (rc < 0) {
			chg_err("!!![OPLUS_CHG] read otg_switch_status fail\n");
			return 0;
		}
		otg_switch_status = pst->prop[USB_OTG_SWITCH];
		chip->otg_switch = otg_switch_status;
	}

	chg_err("!!![OPLUS_CHG] otg_switch_status=%d\n", chip->otg_switch);
	return chip->otg_switch;
}

int oplus_get_fast_chg_type(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	union oplus_chg_mod_propval pval;
	int fast_chg_type = 0;
	int rc;

	fast_chg_type = oplus_vooc_get_fast_chg_type();
	if (fast_chg_type == 0) {
		fast_chg_type = oplus_chg_get_charger_subtype();
	}

	if (!chip) {
		chg_err("chip is NULL!\n");
		return fast_chg_type;
	}
	if (is_wls_ocm_available(chip) && (fast_chg_type == 0) && (oplus_get_wired_chg_present() == false)) {
		rc = oplus_chg_mod_get_property(chip->wls_ocm, OPLUS_CHG_PROP_WLS_TYPE, &pval);
		if (rc == 0) {
			if ((pval.intval == OPLUS_CHG_WLS_VOOC)
					|| (pval.intval == OPLUS_CHG_WLS_SVOOC)
					|| (pval.intval == OPLUS_CHG_WLS_PD_65W)) {
				rc = oplus_chg_mod_get_property(chip->wls_ocm, OPLUS_CHG_PROP_ADAPTER_TYPE, &pval);
				if (rc == 0)
					fast_chg_type = pval.intval;
				if (fast_chg_type == WLS_ADAPTER_TYPE_PD_65W)
					fast_chg_type = WLS_ADAPTER_TYPE_SVOOC;
			}
		}
	}

	return fast_chg_type;
}
#endif

static int usb_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = NULL;
	int prop_id, rc = 0;

	if (bcdev == NULL)
		return -ENODEV;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
#ifndef OPLUS_FEATURE_CHG_BASIC
		rc = usb_psy_set_icl(bcdev, prop_id, pval->intval);
#endif
		break;
	default:
		break;
	}

	return rc;
}

static int usb_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	default:
		break;
	}

	return 0;
}

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_usb_type usb_psy_supported_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_ACA,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_PD_PPS,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,
};

static struct power_supply_desc usb_psy_desc = {
	.name			= "usb",
#ifdef OPLUS_FEATURE_CHG_BASIC
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
#else
	.type			= POWER_SUPPLY_TYPE_USB,
#endif
	.properties		= usb_props,
	.num_properties		= ARRAY_SIZE(usb_props),
	.get_property		= usb_psy_get_prop,
	.set_property		= usb_psy_set_prop,
	.usb_types		= usb_psy_supported_types,
	.num_usb_types		= ARRAY_SIZE(usb_psy_supported_types),
	.property_is_writeable	= usb_psy_prop_is_writeable,
};

static int __battery_psy_set_charge_current(struct battery_chg_dev *bcdev,
					u32 fcc_ua)
{
	int rc;

	if (bcdev->restrict_chg_en) {
		fcc_ua = min_t(u32, fcc_ua, bcdev->restrict_fcc_ua);
		fcc_ua = min_t(u32, fcc_ua, bcdev->thermal_fcc_ua);
	}

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_BATTERY],
				BATT_CHG_CTRL_LIM, fcc_ua);
	if (rc < 0)
		pr_err("Failed to set FCC %u, rc=%d\n", fcc_ua, rc);
	else
		pr_debug("Set FCC to %u uA\n", fcc_ua);

	return rc;
}

static int battery_psy_set_charge_current(struct battery_chg_dev *bcdev,
					int val)
{
	int rc;
	u32 fcc_ua, prev_fcc_ua;

	if (!bcdev->num_thermal_levels)
		return 0;

	if (bcdev->num_thermal_levels < 0) {
		pr_err("Incorrect num_thermal_levels\n");
		return -EINVAL;
	}

	if (val < 0 || val > bcdev->num_thermal_levels)
		return -EINVAL;

	fcc_ua = bcdev->thermal_levels[val];
	prev_fcc_ua = bcdev->thermal_fcc_ua;
	bcdev->thermal_fcc_ua = fcc_ua;

	rc = __battery_psy_set_charge_current(bcdev, fcc_ua);
	if (!rc)
		bcdev->curr_thermal_level = val;
	else
		bcdev->thermal_fcc_ua = prev_fcc_ua;

	return rc;
}

#define TIME_TO_FULL_AVG	1800
#define TIME_TO_EMPTY_AVG	3600
#define CHARGER_VOL_5V_MV	5000
#define CHARGER_VOL_10V_MV	10000
#define DEFAULT_CHARGE_VOL_MIN_TIME	12000
#ifdef OPLUS_FEATURE_CHG_BASIC
#define PARALLEL_SWITCH_HIGH_TEMP	690
#endif
static int battery_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = NULL;
#ifndef OPLUS_FEATURE_CHG_BASIC
	int prop_id, rc;
#else
	union oplus_chg_mod_propval temp_val = {0, };
	int batt_health = POWER_SUPPLY_HEALTH_UNKNOWN;
	bool wls_online = false;
	static int pre_batt_status;
	int rc = 0;
	unsigned long cur_chg_time = 0;
#endif

#ifdef OPLUS_FEATURE_CHG_BASIC
	struct oplus_chg_chip *chip = g_oplus_chip;
#endif

#ifdef OPLUS_FEATURE_CHG_BASIC
	if (!chip) {
		chg_err("chip is NULL\n");
		return -EINVAL;
	}
#endif
	if (bcdev == NULL)
		return -ENODEV;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	pval->intval = -ENODATA;

#ifndef OPLUS_FEATURE_CHG_BASIC
	/*
	 * The prop id of TIME_TO_FULL_NOW and TIME_TO_FULL_AVG is same.
	 * So, map the prop id of TIME_TO_FULL_AVG for TIME_TO_FULL_NOW.
	 */
	if (prop == POWER_SUPPLY_PROP_TIME_TO_FULL_NOW)
		prop = POWER_SUPPLY_PROP_TIME_TO_FULL_AVG;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		pval->strval = pst->model;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		pval->intval = DIV_ROUND_CLOSEST(pst->prop[prop_id], 100);
		if (IS_ENABLED(CONFIG_QTI_PMIC_GLINK_CLIENT_DEBUG) &&
		   (bcdev->fake_soc >= 0 && bcdev->fake_soc <= 100))
			pval->intval = bcdev->fake_soc;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		pval->intval = DIV_ROUND_CLOSEST((int)pst->prop[prop_id], 10);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		pval->intval = bcdev->curr_thermal_level;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pval->intval = bcdev->num_thermal_levels;
		break;
	default:
		pval->intval = pst->prop[prop_id];
		break;
	}
#else
	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		if (chip->wls_status_keep) {
			pval->intval = pre_batt_status;
		} else {
			if (oplus_chg_show_vooc_logo_ornot() == 1) {
				if (oplus_vooc_get_fastchg_started() == true ||
					oplus_vooc_get_fastchg_to_normal() == true ||
					oplus_vooc_get_fastchg_to_warm() == true ||
					oplus_vooc_get_fastchg_dummy_started() == true) {
					if (chip->prop_status != POWER_SUPPLY_STATUS_FULL &&
					    chip->mmi_chg) {
						pval->intval = POWER_SUPPLY_STATUS_CHARGING;
					} else {
						pval->intval = chip->prop_status;
					}
				} else {
					if ((oplus_is_vooc_project() == DUAL_BATT_150W ||
					    oplus_is_vooc_project() == DUAL_BATT_240W) &&
					    oplus_quirks_keep_connect_status() && chip->mmi_chg)
						pval->intval = chip->keep_prop_status;
					else
						pval->intval = chip->prop_status;
				}

				if (chip->tbatt_status == BATTERY_STATUS__HIGH_TEMP ||
					chip->tbatt_status == BATTERY_STATUS__LOW_TEMP ||
					chip->tbatt_status == BATTERY_STATUS__REMOVED)
					pval->intval = chip->prop_status;
			} else if (!chip->authenticate) {
				pval->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			} else {
				pval->intval = chip->prop_status;
			}
			if (oplus_chg_wls_is_present(chip))
				pre_batt_status = pval->intval;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (is_wls_ocm_available(chip)) {
			rc = oplus_chg_mod_get_property(chip->wls_ocm, OPLUS_CHG_PROP_PRESENT, &temp_val);
			if (rc == 0)
				wls_online = !!temp_val.intval;
			else
				rc = 0;
		}
		if (is_comm_ocm_available(chip) && wls_online)
			batt_health = oplus_chg_comm_get_batt_health(chip->comm_ocm);
		if ((batt_health == POWER_SUPPLY_HEALTH_UNKNOWN) ||
		    (batt_health == POWER_SUPPLY_HEALTH_GOOD)) {
			pval->intval = oplus_chg_get_prop_batt_health(chip);
		} else {
			pval->intval = batt_health;
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		pval->intval = chip->batt_exist;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		pval->intval = chip->charger_type;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if(chip->vooc_show_ui_soc_decimal == true && chip->decimal_control) {
			pval->intval = (chip->ui_soc_integer + chip->ui_soc_decimal)/1000;
		} else {
			pval->intval = chip->ui_soc;
		}
		if(pval->intval > 100) {
			pval->intval = 100;
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		pval->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		if (chip && (chip->ui_soc == 0)) {
			pval->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
			chg_err("bat pro POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL, should shutdown!!!\n");
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		pval->intval = chip->limits.charger_hv_thr;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		pval->intval = chip->batt_volt * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pval->intval = chip->limits.temp_normal_vfloat_mv;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (chip->voocphy_support == ADSP_VOOCPHY || chip->read_by_reg == 1) {
			pval->intval = oplus_gauge_get_batt_current();
		} else {
			pval->intval = oplus_gauge_get_prev_batt_current();
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		pval->intval = bcdev->curr_thermal_level;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pval->intval = bcdev->num_thermal_levels;
		break;
	case POWER_SUPPLY_PROP_TEMP:
                if (oplus_switching_support_parallel_chg()) {
			if ((chip->tbatt_temp > PARALLEL_SWITCH_HIGH_TEMP) ||
			    (chip->sub_batt_temperature > PARALLEL_SWITCH_HIGH_TEMP)) {
				pval->intval = chip->tbatt_temp > chip->sub_batt_temperature ? chip->tbatt_temp : chip->sub_batt_temperature;
				pval->intval = pval->intval - chip->offset_temp;
			} else {
				pval->intval = chip->tbatt_temp - chip->offset_temp;
			}
		} else {
			pval->intval = oplus_get_report_batt_temp() - chip->offset_temp;
		}
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		pval->intval = chip->vooc_project;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		pval->intval = chip->batt_rm * 1000;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		pval->intval = chip->charger_cycle;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		pval->intval = chip->batt_capacity_mah * 1000;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		pval->strval = pst->model;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		if (chip->time_to_full > 0 && chip->ui_soc == 100)
			pval->intval = 0;
		else
			pval->intval = chip->time_to_full;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		pval->intval = TIME_TO_EMPTY_AVG;
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
	case POWER_SUPPLY_PROP_POWER_AVG:
		pval->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		pval->intval = chip->charger_volt;
		if (oplus_vooc_get_fastchg_started() == true && (chip->vbatt_num == 2)
				&& oplus_vooc_get_fast_chg_type() != CHARGER_SUBTYPE_FASTCHG_VOOC) {
			pval->intval = CHARGER_VOL_10V_MV;
		}

		if (qpnp_is_power_off_charging() == true
			&& (oplus_chg_get_curr_time_ms(&cur_chg_time) < DEFAULT_CHARGE_VOL_MIN_TIME)
			&& oplus_vooc_get_fastchg_started() != true) {
			pval->intval = CHARGER_VOL_5V_MV;
			chg_err("qpnp_is_power_off_charging power off charge here  5000");
		}
		rc = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		pval->intval = chip->batt_volt_min * 1000;
		rc = 0;
		break;
	default:
		pval->intval = 0;
		rc = 0;
		break;
	}
#endif

	return rc;
}

static int battery_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);

	if (bcdev == NULL)
		return -ENODEV;
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		return battery_psy_set_charge_current(bcdev, pval->intval);
#ifdef OPLUS_FEATURE_CHG_BASIC
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (g_oplus_chip && g_oplus_chip->smart_charging_screenoff) {
			oplus_smart_charge_by_shell_temp(g_oplus_chip, pval->intval);
			break;
		} else {
			return  -EINVAL;
		}
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		if (g_oplus_chip) {
			g_oplus_chip->time_to_full =
				(pval->intval & TTF_VALUE_MASK) > 0 ? (pval->intval & TTF_VALUE_MASK) : 0;
			if (pval->intval & TTF_UPDATE_UEVENT_BIT)
				power_supply_changed(g_oplus_chip->batt_psy);
		}
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int battery_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		return 1;
#ifdef OPLUS_FEATURE_CHG_BASIC
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (g_oplus_chip && g_oplus_chip->smart_charging_screenoff) {
			return 1;
		} else {
			return 0;
		}
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		return 1;
#endif
	default:
		break;
	}

	return 0;
}

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
#ifdef OPLUS_FEATURE_CHG_BASIC
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
#endif
};

static const struct power_supply_desc batt_psy_desc = {
	.name			= "battery",
	.type			= POWER_SUPPLY_TYPE_BATTERY,
	.properties		= battery_props,
	.num_properties		= ARRAY_SIZE(battery_props),
	.get_property		= battery_psy_get_prop,
	.set_property		= battery_psy_set_prop,
	.property_is_writeable	= battery_psy_prop_is_writeable,
};

static int battery_chg_init_psy(struct battery_chg_dev *bcdev)
{
	struct power_supply_config psy_cfg = {};
	int rc;

	psy_cfg.drv_data = bcdev;
	psy_cfg.of_node = bcdev->dev->of_node;
	bcdev->psy_list[PSY_TYPE_BATTERY].psy =
		devm_power_supply_register(bcdev->dev, &batt_psy_desc,
						&psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_BATTERY].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_BATTERY].psy);
		pr_err("Failed to register battery power supply, rc=%d\n", rc);
		return rc;
	}

#ifdef OPLUS_FEATURE_CHG_BASIC
	bcdev->psy_list[PSY_TYPE_WLS].psy =
		devm_power_supply_register(bcdev->dev, &wls_psy_desc, &psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy);
		pr_err("Failed to register wireless power supply, rc=%d\n", rc);
		return rc;
	}
#endif

	bcdev->psy_list[PSY_TYPE_USB].psy =
		devm_power_supply_register(bcdev->dev, &usb_psy_desc, &psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_USB].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_USB].psy);
		pr_err("Failed to register USB power supply, rc=%d\n", rc);
		return rc;
	}

#ifndef OPLUS_FEATURE_CHG_BASIC
	bcdev->psy_list[PSY_TYPE_WLS].psy =
		devm_power_supply_register(bcdev->dev, &wls_psy_desc, &psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy);
		pr_err("Failed to register wireless power supply, rc=%d\n", rc);
		return rc;
	}
#endif

	return 0;
}

static void battery_chg_subsys_up_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, subsys_up_work);
	int rc;

	battery_chg_notify_enable(bcdev);

	/*
	 * Give some time after enabling notification so that USB adapter type
	 * information can be obtained properly which is essential for setting
	 * USB ICL.
	 */
	msleep(200);

	if (bcdev->last_fcc_ua) {
		rc = __battery_psy_set_charge_current(bcdev,
				bcdev->last_fcc_ua);
		if (rc < 0)
			pr_err("Failed to set FCC (%u uA), rc=%d\n",
				bcdev->last_fcc_ua, rc);
	}

#ifndef OPLUS_FEATURE_CHG_BASIC
	if (bcdev->usb_icl_ua) {
		rc = usb_psy_set_icl(bcdev, USB_INPUT_CURR_LIMIT,
				bcdev->usb_icl_ua);
		if (rc < 0)
			pr_err("Failed to set ICL(%u uA), rc=%d\n",
				bcdev->usb_icl_ua, rc);
	}
#endif
}

static int wireless_fw_send_firmware(struct battery_chg_dev *bcdev,
					const struct firmware *fw)
{
	struct wireless_fw_push_buf_req msg = {};
	const u8 *ptr;
	u32 i, num_chunks, partial_chunk_size;
	int rc;

	num_chunks = fw->size / WLS_FW_BUF_SIZE;
	partial_chunk_size = fw->size % WLS_FW_BUF_SIZE;

	if (!num_chunks)
		return -EINVAL;

	pr_debug("Updating FW...\n");

	ptr = fw->data;
	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_WLS_FW_PUSH_BUF_REQ;

	for (i = 0; i < num_chunks; i++, ptr += WLS_FW_BUF_SIZE) {
		msg.fw_chunk_id = i + 1;
		memcpy(msg.buf, ptr, WLS_FW_BUF_SIZE);

		pr_debug("sending FW chunk %u\n", i + 1);
		rc = battery_chg_fw_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			return rc;
	}

	if (partial_chunk_size) {
		msg.fw_chunk_id = i + 1;
		memset(msg.buf, 0, WLS_FW_BUF_SIZE);
		memcpy(msg.buf, ptr, partial_chunk_size);

		pr_debug("sending partial FW chunk %u\n", i + 1);
		rc = battery_chg_fw_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int wireless_fw_check_for_update(struct battery_chg_dev *bcdev,
					u32 version, size_t size)
{
	struct wireless_fw_check_req req_msg = {};

	bcdev->wls_fw_update_reqd = false;

	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = BC_WLS_FW_CHECK_UPDATE;
	req_msg.fw_version = version;
	req_msg.fw_size = size;
	req_msg.fw_crc = bcdev->wls_fw_crc;

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

#define IDT_FW_MAJOR_VER_OFFSET		0x94
#define IDT_FW_MINOR_VER_OFFSET		0x96
static int wireless_fw_update(struct battery_chg_dev *bcdev, bool force)
{
	const struct firmware *fw;
	struct psy_state *pst;
	u32 version;
	u16 maj_ver, min_ver;
	int rc;

	pm_stay_awake(bcdev->dev);

	/*
	 * Check for USB presence. If nothing is connected, check whether
	 * battery SOC is at least 50% before allowing FW update.
	 */
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_ONLINE);
	if (rc < 0)
		goto out;

	if (!pst->prop[USB_ONLINE]) {
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
		rc = read_property_id(bcdev, pst, BATT_CAPACITY);
		if (rc < 0)
			goto out;

		if ((pst->prop[BATT_CAPACITY] / 100) < 50) {
			pr_err("Battery SOC should be at least 50%% or connect charger\n");
			rc = -EINVAL;
			goto out;
		}
	}

	rc = firmware_request_nowarn(&fw, bcdev->wls_fw_name, bcdev->dev);
	if (rc) {
		pr_err("Couldn't get firmware rc=%d\n", rc);
		goto out;
	}

	if (!fw || !fw->data || !fw->size) {
		pr_err("Invalid firmware\n");
		rc = -EINVAL;
		goto release_fw;
	}

	if (fw->size < SZ_16K) {
		pr_err("Invalid firmware size %zu\n", fw->size);
		rc = -EINVAL;
		goto release_fw;
	}

	maj_ver = le16_to_cpu(*(__le16 *)(fw->data + IDT_FW_MAJOR_VER_OFFSET));
	min_ver = le16_to_cpu(*(__le16 *)(fw->data + IDT_FW_MINOR_VER_OFFSET));
	version = maj_ver << 16 | min_ver;

	if (force)
		version = UINT_MAX;

	pr_debug("FW size: %zu version: %#x\n", fw->size, version);

	rc = wireless_fw_check_for_update(bcdev, version, fw->size);
	if (rc < 0) {
		pr_err("Wireless FW update not needed, rc=%d\n", rc);
		goto release_fw;
	}

	if (!bcdev->wls_fw_update_reqd) {
		pr_warn("Wireless FW update not required\n");
		goto release_fw;
	}

	/* Wait for IDT to be setup by charger firmware */
	msleep(WLS_FW_PREPARE_TIME_MS);

	reinit_completion(&bcdev->fw_update_ack);
	rc = wireless_fw_send_firmware(bcdev, fw);
	if (rc < 0) {
		pr_err("Failed to send FW chunk, rc=%d\n", rc);
		goto release_fw;
	}

	rc = wait_for_completion_timeout(&bcdev->fw_update_ack,
				msecs_to_jiffies(WLS_FW_UPDATE_TIME_MS));
	if (!rc) {
		pr_err("Error, timed out updating firmware\n");
		rc = -ETIMEDOUT;
		goto release_fw;
	} else {
		rc = 0;
	}

	pr_info("Wireless FW update done\n");

release_fw:
	bcdev->wls_fw_crc = 0;
	release_firmware(fw);
out:
	pm_relax(bcdev->dev);

	return rc;
}

static ssize_t wireless_fw_crc_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	u16 val;

	if (kstrtou16(buf, 0, &val) || !val)
		return -EINVAL;

	bcdev->wls_fw_crc = val;

	return count;
}
static CLASS_ATTR_WO(wireless_fw_crc);

static ssize_t wireless_fw_version_show(struct class *c,
					struct class_attribute *attr,
					char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct wireless_fw_get_version_req req_msg = {};
	int rc;

	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = BC_WLS_FW_GET_VERSION;

	rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
	if (rc < 0) {
		pr_err("Failed to get FW version rc=%d\n", rc);
		return rc;
	}

	return scnprintf(buf, PAGE_SIZE, "%#x\n", bcdev->wls_fw_version);
}
static CLASS_ATTR_RO(wireless_fw_version);

static ssize_t wireless_fw_force_update_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	bool val;
	int rc;

	if (kstrtobool(buf, &val) || !val)
		return -EINVAL;

	rc = wireless_fw_update(bcdev, true);
	if (rc < 0)
		return rc;

	return count;
}
static CLASS_ATTR_WO(wireless_fw_force_update);

static ssize_t wireless_fw_update_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	bool val;
	int rc;

	if (kstrtobool(buf, &val) || !val)
		return -EINVAL;

	rc = wireless_fw_update(bcdev, false);
	if (rc < 0)
		return rc;

	return count;
}
static CLASS_ATTR_WO(wireless_fw_update);

static ssize_t wireless_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_WLS];
	int rc;

	rc = read_property_id(bcdev, pst, WLS_ADAP_TYPE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			get_wls_type_name(pst->prop[WLS_ADAP_TYPE]));
}
static CLASS_ATTR_RO(wireless_type);

static ssize_t usb_typec_compliant_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_TYPEC_COMPLIANT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			(int)pst->prop[USB_TYPEC_COMPLIANT]);
}
static CLASS_ATTR_RO(usb_typec_compliant);

static ssize_t usb_real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_REAL_TYPE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			get_usb_type_name(pst->prop[USB_REAL_TYPE]));
}
static CLASS_ATTR_RO(usb_real_type);

static ssize_t restrict_cur_store(struct class *c, struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	u32 fcc_ua, prev_fcc_ua;

	if (kstrtou32(buf, 0, &fcc_ua) || fcc_ua > bcdev->thermal_fcc_ua)
		return -EINVAL;

	prev_fcc_ua = bcdev->restrict_fcc_ua;
	bcdev->restrict_fcc_ua = fcc_ua;
	if (bcdev->restrict_chg_en) {
		rc = __battery_psy_set_charge_current(bcdev, fcc_ua);
		if (rc < 0) {
			bcdev->restrict_fcc_ua = prev_fcc_ua;
			return rc;
		}
	}

	return count;
}

static ssize_t restrict_cur_show(struct class *c, struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%u\n", bcdev->restrict_fcc_ua);
}
static CLASS_ATTR_RW(restrict_cur);

static ssize_t restrict_chg_store(struct class *c, struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	bcdev->restrict_chg_en = val;
	rc = __battery_psy_set_charge_current(bcdev, bcdev->restrict_chg_en ?
			bcdev->restrict_fcc_ua : bcdev->thermal_fcc_ua);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t restrict_chg_show(struct class *c, struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->restrict_chg_en);
}
static CLASS_ATTR_RW(restrict_chg);

static ssize_t fake_soc_store(struct class *c, struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	bcdev->fake_soc = val;
	pr_debug("Set fake soc to %d\n", val);

	if (IS_ENABLED(CONFIG_QTI_PMIC_GLINK_CLIENT_DEBUG) && pst->psy)
		power_supply_changed(pst->psy);

	return count;
}

static ssize_t fake_soc_show(struct class *c, struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->fake_soc);
}
static CLASS_ATTR_RW(fake_soc);

static ssize_t wireless_boost_en_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_WLS],
				WLS_BOOST_EN, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t wireless_boost_en_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_WLS];
	int rc;

	rc = read_property_id(bcdev, pst, WLS_BOOST_EN);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[WLS_BOOST_EN]);
}
static CLASS_ATTR_RW(wireless_boost_en);

static ssize_t moisture_detection_en_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_USB],
				USB_MOISTURE_DET_EN, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t moisture_detection_en_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_MOISTURE_DET_EN);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			pst->prop[USB_MOISTURE_DET_EN]);
}
static CLASS_ATTR_RW(moisture_detection_en);

static ssize_t moisture_detection_status_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_MOISTURE_DET_STS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			pst->prop[USB_MOISTURE_DET_STS]);
}
static CLASS_ATTR_RO(moisture_detection_status);

static ssize_t resistance_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc;

	rc = read_property_id(bcdev, pst, BATT_RESISTANCE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[BATT_RESISTANCE]);
}
static CLASS_ATTR_RO(resistance);

static ssize_t soh_show(struct class *c, struct class_attribute *attr,
			char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc;

	rc = read_property_id(bcdev, pst, BATT_SOH);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[BATT_SOH]);
}
static CLASS_ATTR_RO(soh);

static ssize_t ship_mode_en_store(struct class *c, struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	if (kstrtobool(buf, &bcdev->ship_mode_en))
		return -EINVAL;

	return count;
}

static ssize_t ship_mode_en_show(struct class *c, struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->ship_mode_en);
}
static CLASS_ATTR_RW(ship_mode_en);

static struct attribute *battery_class_attrs[] = {
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_moisture_detection_status.attr,
	&class_attr_moisture_detection_en.attr,
	&class_attr_wireless_boost_en.attr,
	&class_attr_fake_soc.attr,
	&class_attr_wireless_fw_update.attr,
	&class_attr_wireless_fw_force_update.attr,
	&class_attr_wireless_fw_version.attr,
	&class_attr_wireless_fw_crc.attr,
	&class_attr_wireless_type.attr,
	&class_attr_ship_mode_en.attr,
	&class_attr_restrict_chg.attr,
	&class_attr_restrict_cur.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_usb_typec_compliant.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class);

#ifdef CONFIG_DEBUG_FS
static void battery_chg_add_debugfs(struct battery_chg_dev *bcdev)
{
	int rc;
	struct dentry *dir;

	dir = debugfs_create_dir("battery_charger", NULL);
	if (IS_ERR(dir)) {
		rc = PTR_ERR(dir);
		pr_err("Failed to create charger debugfs directory, rc=%d\n",
			rc);
		return;
	}

	bcdev->debugfs_dir = dir;
	debugfs_create_bool("block_tx", 0600, dir, &bcdev->block_tx);
}
#else
static void battery_chg_add_debugfs(struct battery_chg_dev *bcdev) { }
#endif

#ifdef OPLUS_FEATURE_CHG_BASIC
static int oplus_otg_boost_en_gpio_init(struct battery_chg_dev *bcdev)
{
	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.otg_boost_en_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_boost_en_pinctrl)) {
		chg_err("get otg_boost_en_pinctrl fail\n");
		return -EINVAL;
	}
	bcdev->oplus_custom_gpio.otg_boost_en_active =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.otg_boost_en_pinctrl, "otg_booster_en_active");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_boost_en_active)) {
		chg_err("get otg_boost_en_active\n");
		return -EINVAL;
	}
	bcdev->oplus_custom_gpio.otg_boost_en_sleep =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.otg_boost_en_pinctrl, "otg_booster_en_sleep");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_boost_en_sleep)) {
		chg_err("get otg_booster_en_sleep\n");
		return -EINVAL;
	}

	pinctrl_select_state(bcdev->oplus_custom_gpio.otg_boost_en_pinctrl,
		bcdev->oplus_custom_gpio.otg_boost_en_sleep);

	return 0;
}

static int oplus_otg_ovp_en_gpio_init(struct battery_chg_dev *bcdev)
{
	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.otg_ovp_en_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_ovp_en_pinctrl)) {
		chg_err("get otg_ovp_en_pinctrl fail\n");
		return -EINVAL;
	}
	bcdev->oplus_custom_gpio.otg_ovp_en_active =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.otg_ovp_en_pinctrl, "otg_ovp_en_active");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_ovp_en_active)) {
		chg_err("get otg_ovp_en_active\n");
		return -EINVAL;
	}
	bcdev->oplus_custom_gpio.otg_ovp_en_sleep =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.otg_ovp_en_pinctrl, "otg_ovp_en_sleep");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_ovp_en_sleep)) {
		chg_err("get otg_ovp_en_sleep\n");
		return -EINVAL;
	}

	pinctrl_select_state(bcdev->oplus_custom_gpio.otg_ovp_en_pinctrl,
		bcdev->oplus_custom_gpio.otg_ovp_en_sleep);

	return 0;
}

static void oplus_set_otg_boost_en_val(int value)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (bcdev->oplus_custom_gpio.otg_boost_en_gpio <= 0) {
		chg_err("otg_boost_en_gpio not exist, return\n");
		return;
	}

	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_boost_en_pinctrl)
		|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_boost_en_active)
		|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_boost_en_sleep)) {
		chg_err("otg_boost_en pinctrl null, return\n");
		return;
	}

	if (value) {
		gpio_direction_output(bcdev->oplus_custom_gpio.otg_boost_en_gpio , 1);
		pinctrl_select_state(bcdev->oplus_custom_gpio.otg_boost_en_pinctrl,
				bcdev->oplus_custom_gpio.otg_boost_en_active);
	} else {
		gpio_direction_output(bcdev->oplus_custom_gpio.otg_boost_en_gpio, 0);
		pinctrl_select_state(bcdev->oplus_custom_gpio.otg_boost_en_pinctrl,
				bcdev->oplus_custom_gpio.otg_boost_en_sleep);
	}

	chg_err("<~OTG~>set value:%d, gpio_val:%d\n", value,
		gpio_get_value(bcdev->oplus_custom_gpio.otg_boost_en_gpio));
}

static void oplus_set_otg_ovp_en_val(int value)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (bcdev->oplus_custom_gpio.otg_ovp_en_gpio <= 0) {
		chg_err("otg_ovp_en_gpio not exist, return\n");
		return;
	}

	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_ovp_en_pinctrl)
		|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_ovp_en_active)
		|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.otg_ovp_en_sleep)) {
		chg_err("otg_ovp_en pinctrl null, return\n");
		return;
	}

	if (value) {
		gpio_direction_output(bcdev->oplus_custom_gpio.otg_ovp_en_gpio , 1);
		pinctrl_select_state(bcdev->oplus_custom_gpio.otg_ovp_en_pinctrl,
				bcdev->oplus_custom_gpio.otg_ovp_en_active);
	} else {
		gpio_direction_output(bcdev->oplus_custom_gpio.otg_ovp_en_gpio, 0);
		pinctrl_select_state(bcdev->oplus_custom_gpio.otg_ovp_en_pinctrl,
				bcdev->oplus_custom_gpio.otg_ovp_en_sleep);
	}

	chg_err("<~OTG~>set value:%d, gpio_val:%d\n", value,
		gpio_get_value(bcdev->oplus_custom_gpio.otg_ovp_en_gpio));
}

static bool is_support_tx_boost(struct battery_chg_dev *bcdev)
{
	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return false;
	}

	if (gpio_is_valid(bcdev->oplus_custom_gpio.tx_boost_en_gpio))
		return true;
	return false;
}

static int oplus_tx_boost_en_gpio_init(struct battery_chg_dev *bcdev)
{
	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.tx_boost_en_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_boost_en_pinctrl)) {
		chg_err("get tx_boost_en_pinctrl fail\n");
		return -EINVAL;
	}
	bcdev->oplus_custom_gpio.tx_boost_en_active =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.tx_boost_en_pinctrl, "tx_boost_en_active");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_boost_en_active)) {
		chg_err("get tx_boost_en_active fail\n");
		return -EINVAL;
	}
	bcdev->oplus_custom_gpio.tx_boost_en_sleep =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.tx_boost_en_pinctrl, "tx_boost_en_sleep");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_boost_en_sleep)) {
		chg_err("get tx_boost_en_sleep fail\n");
		return -EINVAL;
	}

	gpio_direction_output(bcdev->oplus_custom_gpio.tx_boost_en_gpio, 0);
	pinctrl_select_state(bcdev->oplus_custom_gpio.tx_boost_en_pinctrl,
		bcdev->oplus_custom_gpio.tx_boost_en_sleep);

	return 0;
}

/*static void oplus_set_tx_boost_en_val(int value)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (bcdev->oplus_custom_gpio.tx_boost_en_gpio <= 0) {
		chg_err("tx_boost_en_gpio not exist, return\n");
		return;
	}

	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_boost_en_pinctrl)
		|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_boost_en_active)
		|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_boost_en_sleep)) {
		chg_err("otg_boost_en pinctrl null, return\n");
		return;
	}

	if (value) {
		gpio_direction_output(bcdev->oplus_custom_gpio.tx_boost_en_gpio , 1);
		pinctrl_select_state(bcdev->oplus_custom_gpio.tx_boost_en_pinctrl,
				bcdev->oplus_custom_gpio.tx_boost_en_active);
	} else {
		gpio_direction_output(bcdev->oplus_custom_gpio.tx_boost_en_gpio, 0);
		pinctrl_select_state(bcdev->oplus_custom_gpio.tx_boost_en_pinctrl,
				bcdev->oplus_custom_gpio.tx_boost_en_sleep);
	}

	chg_err("set value:%d, gpio_val:%d\n", value,
		gpio_get_value(bcdev->oplus_custom_gpio.tx_boost_en_gpio));
}*/

static int oplus_tx_ovp_en_gpio_init(struct battery_chg_dev *bcdev)
{
	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.tx_ovp_en_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_ovp_en_pinctrl)) {
		chg_err("get tx_ovp_en_pinctrl fail\n");
		return -EINVAL;
	}
	bcdev->oplus_custom_gpio.tx_ovp_en_active =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.tx_ovp_en_pinctrl, "tx_ovp_en_active");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_ovp_en_active)) {
		chg_err("get tx_ovp_en_active fail\n");
		return -EINVAL;
	}
	bcdev->oplus_custom_gpio.tx_ovp_en_sleep =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.tx_ovp_en_pinctrl, "tx_ovp_en_sleep");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_ovp_en_sleep)) {
		chg_err("get tx_ovp_en_sleep fail\n");
		return -EINVAL;
	}

	gpio_direction_output(bcdev->oplus_custom_gpio.tx_ovp_en_gpio, 0);
	pinctrl_select_state(bcdev->oplus_custom_gpio.tx_ovp_en_pinctrl,
		bcdev->oplus_custom_gpio.tx_ovp_en_sleep);

	return 0;
}

static void oplus_set_tx_ovp_en_val(int value)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (bcdev->oplus_custom_gpio.tx_ovp_en_gpio <= 0) {
		chg_err("tx_ovp_en_gpio not exist, return\n");
		return;
	}

	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_ovp_en_pinctrl)
		|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_ovp_en_active)
		|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.tx_ovp_en_sleep)) {
		chg_err("otg_ovp_en pinctrl null, return\n");
		return;
	}

	if (value) {
		gpio_direction_output(bcdev->oplus_custom_gpio.tx_ovp_en_gpio , 1);
		pinctrl_select_state(bcdev->oplus_custom_gpio.tx_ovp_en_pinctrl,
				bcdev->oplus_custom_gpio.tx_ovp_en_active);
	} else {
		gpio_direction_output(bcdev->oplus_custom_gpio.tx_ovp_en_gpio, 0);
		pinctrl_select_state(bcdev->oplus_custom_gpio.tx_ovp_en_pinctrl,
				bcdev->oplus_custom_gpio.tx_ovp_en_sleep);
	}

	chg_err("set value:%d, gpio_val:%d\n", value,
		gpio_get_value(bcdev->oplus_custom_gpio.tx_ovp_en_gpio));
}

static int oplus_wrx_ovp_off_gpio_init(struct battery_chg_dev *bcdev)
{
	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.wrx_ovp_off_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_ovp_off_pinctrl)) {
		chg_err("get wrx_ovp_off_pinctrl fail\n");
		return -EINVAL;
	}
	bcdev->oplus_custom_gpio.wrx_ovp_off_active =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.wrx_ovp_off_pinctrl, "wrx_ovp_off_active");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_ovp_off_active)) {
		chg_err("get tx_ovp_en_active fail\n");
		return -EINVAL;
	}
	bcdev->oplus_custom_gpio.wrx_ovp_off_sleep =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.wrx_ovp_off_pinctrl, "wrx_ovp_off_sleep");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_ovp_off_sleep)) {
		chg_err("get wrx_ovp_off_sleep fail\n");
		return -EINVAL;
	}

	gpio_direction_output(bcdev->oplus_custom_gpio.wrx_ovp_off_gpio, 0);
	pinctrl_select_state(bcdev->oplus_custom_gpio.wrx_ovp_off_pinctrl,
		bcdev->oplus_custom_gpio.tx_ovp_en_sleep);

	return 0;
}

/*static void oplus_set_wrx_ovp_off_val(int value)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (bcdev->oplus_custom_gpio.wrx_ovp_off_gpio <= 0) {
		chg_err("wrx_ovp_off_gpio not exist, return\n");
		return;
	}

	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_ovp_off_pinctrl)
		|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_ovp_off_active)
		|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_ovp_off_sleep)) {
		chg_err("wrx_ovp_off pinctrl null, return\n");
		return;
	}

	if (value) {
		gpio_direction_output(bcdev->oplus_custom_gpio.wrx_ovp_off_gpio , 1);
		pinctrl_select_state(bcdev->oplus_custom_gpio.wrx_ovp_off_pinctrl,
				bcdev->oplus_custom_gpio.wrx_ovp_off_active);
	} else {
		gpio_direction_output(bcdev->oplus_custom_gpio.wrx_ovp_off_gpio, 0);
		pinctrl_select_state(bcdev->oplus_custom_gpio.wrx_ovp_off_pinctrl,
				bcdev->oplus_custom_gpio.wrx_ovp_off_sleep);
	}

	chg_err("set value:%d, gpio_val:%d\n", value,
		gpio_get_value(bcdev->oplus_custom_gpio.wrx_ovp_off_gpio));
}*/

static int oplus_wrx_otg_en_gpio_init(struct battery_chg_dev *bcdev)
{
	if (!bcdev) {
		chg_err("bcdev not ready!\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.wrx_otg_en_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_otg_en_pinctrl)) {
		chg_err("get wrx_otg_en_pinctrl fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.wrx_otg_en_active =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.wrx_otg_en_pinctrl, "wrx_otg_en_active");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_otg_en_active)) {
		chg_err("get wrx_otg_en_active fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.wrx_otg_en_sleep =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.wrx_otg_en_pinctrl, "wrx_otg_en_sleep");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_otg_en_sleep)) {
		chg_err("get wrx_otg_en_sleep fail\n");
		return -EINVAL;
	}

	gpio_direction_output(bcdev->oplus_custom_gpio.wrx_otg_en_gpio, 0);
	pinctrl_select_state(bcdev->oplus_custom_gpio.wrx_otg_en_pinctrl,
		bcdev->oplus_custom_gpio.wrx_otg_en_sleep);

	return 0;
}

void oplus_set_wrx_otg_en_val(int value)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (bcdev->oplus_custom_gpio.wrx_otg_en_gpio <= 0) {
		chg_err("wrx_otg_en_gpio not exist, return\n");
		return;
	}

	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_otg_en_pinctrl) ||
	   IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_otg_en_active) ||
	   IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.wrx_otg_en_sleep)) {
		chg_err("wrx_otg_en pinctrl null, return\n");
		return;
	}

	if (value) {
		gpio_direction_output(bcdev->oplus_custom_gpio.wrx_otg_en_gpio , 1);
		pinctrl_select_state(bcdev->oplus_custom_gpio.wrx_otg_en_pinctrl,
			bcdev->oplus_custom_gpio.wrx_otg_en_active);
	} else {
		gpio_direction_output(bcdev->oplus_custom_gpio.wrx_otg_en_gpio, 0);
		pinctrl_select_state(bcdev->oplus_custom_gpio.wrx_otg_en_pinctrl,
			bcdev->oplus_custom_gpio.wrx_otg_en_sleep);
	}

	chg_err("set value:%d, gpio_val:%d\n", value,
		gpio_get_value(bcdev->oplus_custom_gpio.wrx_otg_en_gpio));
}

static int oplus_chg_2uart_pinctrl_init(struct oplus_chg_chip *chip)
{
	struct pinctrl			*chg_2uart_pinctrl;
	struct pinctrl_state	*chg_2uart_active;
	struct pinctrl_state	*chg_2uart_sleep;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: smb5_chg not ready!\n", __func__);
		return -EINVAL;
	}

	chg_2uart_pinctrl = devm_pinctrl_get(chip->dev);

	if (IS_ERR_OR_NULL(chg_2uart_pinctrl)) {
		chg_err("get 2uart chg_2uart_pinctrl fail\n");
		return -EINVAL;
	}

	chg_2uart_active = pinctrl_lookup_state(chg_2uart_pinctrl, "chg_qupv3_se3_2uart_active");
	if (IS_ERR_OR_NULL(chg_2uart_active)) {
		chg_err("get chg_qupv3_se3_2uart_active fail\n");
		return -EINVAL;
	}

	chg_2uart_sleep = pinctrl_lookup_state(chg_2uart_pinctrl, "chg_qupv3_se3_2uart_sleep");
	if (IS_ERR_OR_NULL(chg_2uart_sleep)) {
		chg_err("get chg_qupv3_se3_2uart_sleep fail\n");
		return -EINVAL;
	}

#if defined(OPLUS_FEATURE_POWERINFO_FTM) && defined(CONFIG_OPLUS_POWERINFO_FTM)
	if (!ext_boot_with_console()) {
		chg_err("set chg_qupv3_se3_2uart_sleep\n");
		pinctrl_select_state(chg_2uart_pinctrl, chg_2uart_sleep);
	}
#endif

	return 0;
}
#endif

#ifdef OPLUS_FEATURE_CHG_BASIC
static bool oplus_vchg_trig_is_support(void)
{
/*#if 0
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return false;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	if (bcdev->oplus_custom_gpio.vchg_trig_gpio <= 0)
		return false;

	if (get_PCB_Version() >= EVT1)
		return true;
#endif*/
	return false;
}

static int oplus_vchg_trig_gpio_init(struct battery_chg_dev *bcdev)
{
	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.vchg_trig_pinctrl = devm_pinctrl_get(bcdev->dev);

	bcdev->oplus_custom_gpio.vchg_trig_default =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.vchg_trig_pinctrl, "vchg_trig_default");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.vchg_trig_default)) {
		chg_err("get vchg_trig_default\n");
		return -EINVAL;
	}

	if (bcdev->oplus_custom_gpio.vchg_trig_gpio > 0) {
		gpio_direction_input(bcdev->oplus_custom_gpio.vchg_trig_gpio);
	}
	pinctrl_select_state(bcdev->oplus_custom_gpio.vchg_trig_pinctrl,
		bcdev->oplus_custom_gpio.vchg_trig_default);

	chg_err("get vchg_trig_default level[%d]\n", gpio_get_value(bcdev->oplus_custom_gpio.vchg_trig_gpio));
	return 0;
}

static int oplus_get_vchg_trig_gpio_val(void)
{
	int level = 1;
	static int pre_level = 1;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (bcdev->oplus_custom_gpio.vchg_trig_gpio <= 0) {
		chg_err("vchg_trig_gpio not exist, return\n");
		return -1;
	}

	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.vchg_trig_pinctrl)
			|| IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.vchg_trig_default)) {
		chg_err("pinctrl null, return\n");
		return -1;
	}

	level = gpio_get_value(bcdev->oplus_custom_gpio.vchg_trig_gpio);
	if (pre_level ^ level) {
		pre_level = level;
		chg_err("!!!!! vchg_trig gpio level[%d], wired[%d]\n", level, !level);
	}
	return level;
}

static int vchg_trig_status = -1;
static int oplus_get_vchg_trig_status(void)
{
	if (vchg_trig_status == -1) {
		vchg_trig_status = !!oplus_get_vchg_trig_gpio_val();
	}
	return vchg_trig_status;
}

static void oplus_vchg_trig_work(struct work_struct *work)
{
/*#if 0
	int level;
	static bool pre_otg = false;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	level = oplus_get_vchg_trig_gpio_val();
	vchg_trig_status = !!level;
	if (level == 0) {
		if (bcdev->otg_online == true) {
			pre_otg = true;
			return;
		}
		if (chip->wireless_support)
			oplus_switch_to_wired_charge(bcdev);
	} else {
		if (pre_otg == true) {
			pre_otg = false;
			return;
		}
		if (chip->wireless_support
			&& oplus_voocphy_get_fastchg_to_warm() == false
			&& oplus_voocphy_get_fastchg_to_normal() == false)
			oplus_switch_from_wired_charge(bcdev);
	}

	if (oplus_voocphy_get_fastchg_to_warm() == false
		&& oplus_voocphy_get_fastchg_to_normal() == false) {
		oplus_chg_wake_update_work();
	}
#endif*/
}

static void oplus_vchg_trig_irq_init(struct battery_chg_dev *bcdev)
{
	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return;
	}

	bcdev->vchg_trig_irq = gpio_to_irq(bcdev->oplus_custom_gpio.vchg_trig_gpio);
	chg_err("[OPLUS_CHG][%s]: vchg_trig_irq[%d]!\n", __func__, bcdev->vchg_trig_irq);
}

#define VCHG_TRIG_DELAY_MS	50
irqreturn_t oplus_vchg_trig_change_handler(int irq, void *data)
{
	struct battery_chg_dev *bcdev = data;

	cancel_delayed_work_sync(&bcdev->vchg_trig_work);
	chg_err("[OPLUS_CHG][%s]: scheduling vchg_trig work!\n", __func__);
	schedule_delayed_work(&bcdev->vchg_trig_work, msecs_to_jiffies(VCHG_TRIG_DELAY_MS));

	return IRQ_HANDLED;
}

static void oplus_vchg_trig_irq_register(struct battery_chg_dev *bcdev)
{
	int ret = 0;

	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return;
	}

	ret = devm_request_threaded_irq(bcdev->dev, bcdev->vchg_trig_irq,
			NULL, oplus_vchg_trig_change_handler, IRQF_TRIGGER_FALLING
			| IRQF_TRIGGER_RISING | IRQF_ONESHOT, "vchg_trig_change", bcdev);
	if (ret < 0) {
		chg_err("Unable to request vchg_trig_change irq: %d\n", ret);
	}
	chg_err("%s: !!!!! irq register\n", __FUNCTION__);

	ret = enable_irq_wake(bcdev->vchg_trig_irq);
	if (ret != 0) {
		chg_err("enable_irq_wake: vchg_trig_irq failed %d\n", ret);
	}
}
#endif /*OPLUS_FEATURE_CHG_BASIC*/

#ifdef OPLUS_FEATURE_CHG_BASIC
static void smbchg_enter_shipmode_pmic(struct oplus_chg_chip *chip)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	rc = write_property_id(bcdev, pst, BATT_SET_SHIP_MODE, 1);
	if (rc) {
		chg_err("set ship mode fail, rc=%d\n", rc);
		return;
	}
	chg_debug("power off after 15s\n");
}

static int oplus_ship_gpio_init(struct oplus_chg_chip *chip)
{
	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: smb2_chg not ready!\n", __func__);
		return -EINVAL;
	}

	chip->normalchg_gpio.pinctrl = devm_pinctrl_get(chip->dev);

	chip->normalchg_gpio.ship_active =
			pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "ship_active");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.ship_active)) {
		chg_err("get ship_active fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.ship_sleep =
			pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "ship_sleep");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.ship_sleep)) {
		chg_err("get ship_sleep fail\n");
		return -EINVAL;
	}

	pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.ship_sleep);

	return 0;
}

static bool oplus_ship_check_is_gpio(struct oplus_chg_chip *chip)
{
	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: smb2_chg not ready!\n", __func__);
		return false;
	}

	if (gpio_is_valid(chip->normalchg_gpio.ship_gpio))
		return true;

	return false;
}

#define PWM_COUNT	5
static void smbchg_enter_shipmode(struct oplus_chg_chip *chip)
{
	int i = 0;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (oplus_ship_check_is_gpio(chip) == true) {
		chg_debug("select gpio control\n");

		mutex_lock(&chip->pmic_spmi.bcdev_chip->oplus_custom_gpio.pinctrl_mutex);
		if (!IS_ERR_OR_NULL(chip->normalchg_gpio.ship_sleep)) {
			pinctrl_select_state(chip->normalchg_gpio.pinctrl,
				chip->normalchg_gpio.ship_sleep);
		}
		for (i = 0; i < PWM_COUNT; i++) {
			/*gpio_direction_output(chip->normalchg_gpio.ship_gpio, 1);*/
			pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.ship_active);
			mdelay(3);
			/*gpio_direction_output(chip->normalchg_gpio.ship_gpio, 0);*/
			pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.ship_sleep);
			mdelay(3);
		}

		mutex_unlock(&chip->pmic_spmi.bcdev_chip->oplus_custom_gpio.pinctrl_mutex);
		chg_debug("power off after 15s\n");
	} else {
		smbchg_enter_shipmode_pmic(chip);
	}
}

static int oplus_mcu_en_gpio_init(struct oplus_chg_chip *chip)
{
	int level;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip)
		return -EINVAL;

	bcdev = chip->pmic_spmi.bcdev_chip;
	if (bcdev->oplus_custom_gpio.mcu_en_gpio <= 0) {
		chg_err("don't need init mcu_en_gpio\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.mcu_en_pinctrl = devm_pinctrl_get(bcdev->dev);
	bcdev->oplus_custom_gpio.mcu_en_active =
			pinctrl_lookup_state(bcdev->oplus_custom_gpio.mcu_en_pinctrl, "mcu_en_active");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.mcu_en_active)) {
		chg_err("get mcu_en_active fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.mcu_en_sleep =
			pinctrl_lookup_state(bcdev->oplus_custom_gpio.mcu_en_pinctrl, "mcu_en_sleep");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.mcu_en_sleep)) {
		chg_err("get mcu_en_sleep fail\n");
		return -EINVAL;
	}

	mutex_lock(&chip->pmic_spmi.bcdev_chip->oplus_custom_gpio.pinctrl_mutex);
	pinctrl_select_state(bcdev->oplus_custom_gpio.mcu_en_pinctrl, bcdev->oplus_custom_gpio.mcu_en_sleep);
	usleep_range(1000, 1000);
	pinctrl_select_state(bcdev->oplus_custom_gpio.mcu_en_pinctrl, bcdev->oplus_custom_gpio.mcu_en_active);
	usleep_range(20000, 20000);
	pinctrl_select_state(bcdev->oplus_custom_gpio.mcu_en_pinctrl, bcdev->oplus_custom_gpio.mcu_en_sleep);
	level = gpio_get_value(bcdev->oplus_custom_gpio.mcu_en_gpio);
	chg_err("mcu_en_gpio level=%d\n", level);
	mutex_unlock(&chip->pmic_spmi.bcdev_chip->oplus_custom_gpio.pinctrl_mutex);

	return 0;
}

static void oplus_chg_mcu_en_init_work(struct work_struct *work)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	int rc;

	if (!chip)
		return;

	rc = oplus_mcu_en_gpio_init(chip);
	if (rc < 0)
		chg_err("failed to init mcu_en gpio,rc=%d\n", rc);
}

static int smbchg_chargerid_switch_gpio_init(struct oplus_chg_chip *chip)
{
	chip->normalchg_gpio.pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.pinctrl)) {
		chg_err("get normalchg_gpio.pinctrl fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.chargerid_switch_active =
			pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "chargerid_switch_active");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_active)) {
		chg_err("get chargerid_switch_active fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.chargerid_switch_sleep =
			pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "chargerid_switch_sleep");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_sleep)) {
		chg_err("get chargerid_switch_sleep fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.chargerid_switch_default =
			pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "chargerid_switch_default");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.chargerid_switch_default)) {
		chg_err("get chargerid_switch_default fail\n");
		return -EINVAL;
	}

	if (chip->normalchg_gpio.chargerid_switch_gpio > 0) {
		gpio_direction_output(chip->normalchg_gpio.chargerid_switch_gpio, 0);
	}
	mutex_lock(&chip->pmic_spmi.bcdev_chip->oplus_custom_gpio.pinctrl_mutex);
	pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.chargerid_switch_default);
	mutex_unlock(&chip->pmic_spmi.bcdev_chip->oplus_custom_gpio.pinctrl_mutex);

	return 0;
}

static int oplus_chg_parse_custom_dt(struct oplus_chg_chip *chip)
{
	int rc = 0;
	struct device_node *node = NULL;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	node = bcdev->dev->of_node;

	chip->normalchg_gpio.chargerid_switch_gpio =
			of_get_named_gpio(node, "qcom,chargerid_switch-gpio", 0);
	if (chip->normalchg_gpio.chargerid_switch_gpio <= 0) {
		chg_err("Couldn't read chargerid_switch-gpio rc = %d, chargerid_switch_gpio:%d\n",
			rc, chip->normalchg_gpio.chargerid_switch_gpio);
	} else {
		if (gpio_is_valid(chip->normalchg_gpio.chargerid_switch_gpio)) {
			rc = gpio_request(chip->normalchg_gpio.chargerid_switch_gpio, "charging-switch1-gpio");
			if (rc) {
				chg_err("unable to request chargerid_switch_gpio:%d\n", chip->normalchg_gpio.chargerid_switch_gpio);
			} else {
				smbchg_chargerid_switch_gpio_init(chip);
			}
		}
		chg_err("chargerid_switch_gpio:%d\n", chip->normalchg_gpio.chargerid_switch_gpio);
	}


	chip->normalchg_gpio.ship_gpio =
			of_get_named_gpio(node, "qcom,ship-gpio", 0);
	if (chip->normalchg_gpio.ship_gpio <= 0) {
		chg_err("Couldn't read qcom,ship-gpio rc = %d, qcom,ship-gpio:%d\n",
				rc, chip->normalchg_gpio.ship_gpio);
	} else {
		if (oplus_ship_check_is_gpio(chip) == true) {
			rc = gpio_request(chip->normalchg_gpio.ship_gpio, "ship-gpio");
			if (rc) {
				chg_err("unable to request ship-gpio:%d\n", chip->normalchg_gpio.ship_gpio);
			} else {
				oplus_ship_gpio_init(chip);
				if (rc)
					chg_err("unable to init ship-gpio:%d\n", chip->normalchg_gpio.ship_gpio);
				else
					chg_err("init ship-gpio level[%d]\n", gpio_get_value(chip->normalchg_gpio.ship_gpio));
			}
		}
		chg_err("ship-gpio:%d\n", chip->normalchg_gpio.ship_gpio);
	}

	bcdev->oplus_custom_gpio.mcu_en_gpio =
			of_get_named_gpio(node, "qcom,mcu-en-gpio", 0);
	if (bcdev->oplus_custom_gpio.mcu_en_gpio <= 0) {
		chg_err("Couldn't read qcom,mcu-en-gpio rc = %d, qcom,mcu-en-gpio:%d\n",
				rc, bcdev->oplus_custom_gpio.mcu_en_gpio);
	} else {
		rc = gpio_request(bcdev->oplus_custom_gpio.mcu_en_gpio, "mcu-en-gpio");
		if (rc)
			chg_err("unable to request mcu-en-gpio:%d\n", bcdev->oplus_custom_gpio.mcu_en_gpio);
		else
			chg_err("init mcu-en-gpio level[%d]\n", gpio_get_value(bcdev->oplus_custom_gpio.mcu_en_gpio));
	}

	bcdev->oplus_custom_gpio.vchg_trig_gpio =
		of_get_named_gpio(node, "qcom,vchg_trig-gpio", 0);
	if (bcdev->oplus_custom_gpio.vchg_trig_gpio <= 0) {
		chg_err("Couldn't read qcom,vchg_trig-gpio rc = %d, vchg_trig-gpio:%d\n",
					rc, bcdev->oplus_custom_gpio.vchg_trig_gpio);
	} else {
		if (oplus_vchg_trig_is_support() == true) {
			rc = gpio_request(bcdev->oplus_custom_gpio.vchg_trig_gpio, "vchg_trig-gpio");
			if (rc) {
				chg_err("unable to vchg_trig-gpio:%d\n",
							bcdev->oplus_custom_gpio.vchg_trig_gpio);
			} else {
				rc = oplus_vchg_trig_gpio_init(bcdev);
				if (rc)
					chg_err("unable to init vchg_trig-gpio:%d\n",
							bcdev->oplus_custom_gpio.vchg_trig_gpio);
				else
					oplus_vchg_trig_irq_init(bcdev);
			}
		}
		chg_err("vchg_trig-gpio:%d\n", bcdev->oplus_custom_gpio.vchg_trig_gpio);
	}

	bcdev->oplus_custom_gpio.ccdetect_gpio = of_get_named_gpio(node, "qcom,ccdetect-gpio", 0);
	if (bcdev->oplus_custom_gpio.ccdetect_gpio <= 0) {
		chg_err("Couldn't read qcom,ccdetect-gpio rc=%d, qcom,ccdetect-gpio:%d\n",
		rc, bcdev->oplus_custom_gpio.ccdetect_gpio);
	} else {
		if (oplus_ccdetect_check_is_gpio(chip) == true) {
			rc = gpio_request(bcdev->oplus_custom_gpio.ccdetect_gpio, "ccdetect-gpio");
			if (rc) {
				chg_err("unable to request ccdetect-gpio:%d\n", bcdev->oplus_custom_gpio.ccdetect_gpio);
			} else {
				rc = oplus_ccdetect_gpio_init(chip);
				if (rc)
					chg_err("unable to init ccdetect-gpio:%d\n", bcdev->oplus_custom_gpio.ccdetect_gpio);
				else
					oplus_ccdetect_irq_init(chip);
			}
		}
		chg_err("ccdetect-gpio:%d\n", bcdev->oplus_custom_gpio.ccdetect_gpio);
	}

	bcdev->oplus_custom_gpio.otg_boost_en_gpio =
			of_get_named_gpio(node, "qcom,otg-booster-en-gpio", 0);
	if (bcdev->oplus_custom_gpio.otg_boost_en_gpio <= 0) {
		chg_err("Couldn't read qcom,otg_booster-en-gpio rc = %d, qcom,otg-booster-en-gpio:%d\n",
				rc, bcdev->oplus_custom_gpio.otg_boost_en_gpio);
	} else {
		if (gpio_is_valid(bcdev->oplus_custom_gpio.otg_boost_en_gpio) == true) {
			rc = gpio_request(bcdev->oplus_custom_gpio.otg_boost_en_gpio, "otg-boost-en-gpio");
			if (rc) {
				chg_err("unable to request otg-boost-en-gpio:%d\n", bcdev->oplus_custom_gpio.otg_boost_en_gpio);
			} else {
				rc = oplus_otg_boost_en_gpio_init(bcdev);
				if (rc)
					chg_err("unable to init otg-boost-en-gpio:%d\n", bcdev->oplus_custom_gpio.otg_boost_en_gpio);
				else
					chg_err("init otg-boost-en-gpio level[%d]\n", gpio_get_value(bcdev->oplus_custom_gpio.otg_boost_en_gpio));
			}
		}
		chg_err("otg-boost-en-gpio:%d\n", bcdev->oplus_custom_gpio.otg_boost_en_gpio);
	}

	bcdev->oplus_custom_gpio.otg_ovp_en_gpio =
			of_get_named_gpio(node, "qcom,otg-ovp-en-gpio", 0);
	if (bcdev->oplus_custom_gpio.otg_ovp_en_gpio <= 0) {
		chg_err("Couldn't read qcom,otg-ovp-en-gpio rc = %d, qcom,otg-ovp-en-gpio:%d\n",
				rc, bcdev->oplus_custom_gpio.otg_ovp_en_gpio);
	} else {
		if (gpio_is_valid(bcdev->oplus_custom_gpio.otg_ovp_en_gpio) == true) {
			rc = gpio_request(bcdev->oplus_custom_gpio.otg_ovp_en_gpio, "otg-ovp-en-gpio");
			if (rc) {
				chg_err("unable to request otg-ovp-en-gpio:%d\n", bcdev->oplus_custom_gpio.otg_ovp_en_gpio);
			} else {
				rc = oplus_otg_ovp_en_gpio_init(bcdev);
				if (rc)
					chg_err("unable to init otg-ovp-en-gpio:%d\n", bcdev->oplus_custom_gpio.otg_ovp_en_gpio);
				else
					chg_err("init otg-ovp-en-gpio level[%d]\n", gpio_get_value(bcdev->oplus_custom_gpio.otg_ovp_en_gpio));
			}
		}
		chg_err("otg-ovp-en-gpio:%d\n", bcdev->oplus_custom_gpio.otg_ovp_en_gpio);
	}

	bcdev->oplus_custom_gpio.tx_boost_en_gpio = of_get_named_gpio(node, "qcom,tx_boost_en-gpio", 0);
	if (bcdev->oplus_custom_gpio.tx_boost_en_gpio <= 0) {
		chg_err("Couldn't read qcom,tx_boost_en-gpio, rc = %d\n", rc);
	} else {
		if (gpio_is_valid(bcdev->oplus_custom_gpio.tx_boost_en_gpio)) {
			rc = gpio_request(bcdev->oplus_custom_gpio.tx_boost_en_gpio, "tx_boost_en-gpio");
			if (rc) {
				chg_err("unable to tx_boost_en-gpio:%d\n", bcdev->oplus_custom_gpio.tx_boost_en_gpio);
			} else {
				rc = oplus_tx_boost_en_gpio_init(bcdev);
				if (rc)
					chg_err("unable to init tx_boost_en-gpio:%d\n", bcdev->oplus_custom_gpio.tx_boost_en_gpio);
				else
					chg_err("init tx_boost_en_gpio level[%d]\n", gpio_get_value(bcdev->oplus_custom_gpio.tx_boost_en_gpio));
			}
		}
		chg_err("tx_boost_en-gpio:%d\n", bcdev->oplus_custom_gpio.tx_boost_en_gpio);
	}

	bcdev->oplus_custom_gpio.tx_ovp_en_gpio = of_get_named_gpio(node, "qcom,tx_ovp_en-gpio", 0);
	if (bcdev->oplus_custom_gpio.tx_ovp_en_gpio <= 0) {
		chg_err("Couldn't read qcom,tx_ovp_en-gpio, rc = %d\n", rc);
	} else {
		if (gpio_is_valid(bcdev->oplus_custom_gpio.tx_ovp_en_gpio)) {
			rc = gpio_request(bcdev->oplus_custom_gpio.tx_ovp_en_gpio, "tx_ovp_en-gpio");
			if (rc) {
				chg_err("unable to tx_ovp_en-gpio:%d\n", bcdev->oplus_custom_gpio.tx_ovp_en_gpio);
			} else {
				rc = oplus_tx_ovp_en_gpio_init(bcdev);
				if (rc)
					chg_err("unable to init tx_ovp_en-gpio:%d\n", bcdev->oplus_custom_gpio.tx_ovp_en_gpio);
				else
					chg_err("init tx_ovp_en_gpio level[%d]\n", gpio_get_value(bcdev->oplus_custom_gpio.tx_ovp_en_gpio));
			}
		}
		chg_err("tx_ovp_en-gpio:%d\n", bcdev->oplus_custom_gpio.tx_ovp_en_gpio);
	}

	bcdev->oplus_custom_gpio.wrx_ovp_off_gpio = of_get_named_gpio(node, "qcom,wrx_ovp_off-gpio", 0);
	if (bcdev->oplus_custom_gpio.wrx_ovp_off_gpio <= 0) {
		chg_err("Couldn't read qcom,wrx_ovp_off-gpio, rc = %d\n", rc);
	} else {
		if (gpio_is_valid(bcdev->oplus_custom_gpio.wrx_ovp_off_gpio)) {
			rc = gpio_request(bcdev->oplus_custom_gpio.wrx_ovp_off_gpio, "wrx_ovp_off-gpio");
			if (rc) {
				chg_err("unable to wrx_ovp_off-gpio:%d\n", bcdev->oplus_custom_gpio.wrx_ovp_off_gpio);
			} else {
				rc = oplus_wrx_ovp_off_gpio_init(bcdev);
				if (rc)
					chg_err("unable to init wrx_ovp_off-gpio:%d\n", bcdev->oplus_custom_gpio.wrx_ovp_off_gpio);
				else
					chg_err("init wrx_ovp_off_gpio level[%d]\n", gpio_get_value(bcdev->oplus_custom_gpio.wrx_ovp_off_gpio));
			}
		}
		chg_err("wrx_ovp_off-gpio:%d\n", bcdev->oplus_custom_gpio.wrx_ovp_off_gpio);
	}

	bcdev->oplus_custom_gpio.wrx_otg_en_gpio = of_get_named_gpio(node, "qcom,wrx_otg_en-gpio", 0);
	if (bcdev->oplus_custom_gpio.wrx_otg_en_gpio <= 0) {
		chg_err("Couldn't read qcom,wrx_otg_en-gpio, rc = %d\n", rc);
	} else {
		if (gpio_is_valid(bcdev->oplus_custom_gpio.wrx_otg_en_gpio)) {
			rc = gpio_request(bcdev->oplus_custom_gpio.wrx_otg_en_gpio, "wrx_otg_en-gpio");
			if (rc) {
				chg_err("unable to wrx_otg_en-gpio:%d\n", bcdev->oplus_custom_gpio.wrx_otg_en_gpio);
			} else {
				rc = oplus_wrx_otg_en_gpio_init(bcdev);
				if (rc)
					chg_err("unable to init wrx_otg_en-gpio:%d\n", bcdev->oplus_custom_gpio.wrx_otg_en_gpio);
				else
					chg_err("init wrx_otg_en_gpio level[%d]\n", gpio_get_value(bcdev->oplus_custom_gpio.wrx_otg_en_gpio));
			}
		}
		chg_err("wrx_otg_en-gpio:%d\n", bcdev->oplus_custom_gpio.wrx_otg_en_gpio);
	}

	rc = of_property_read_u32(node, "qcom,otg_scheme",
			&bcdev->otg_scheme);
	if (rc) {
		bcdev->otg_scheme = OTG_SCHEME_UNDEFINE;
	}

	rc = of_property_read_u32(node, "qcom,otg_boost_src",
			&bcdev->otg_boost_src);
	if (rc) {
		bcdev->otg_boost_src = OTG_BOOST_SOURCE_EXTERNAL;
	}

	rc = of_property_read_u32(node, "qcom,usbtemp_thread_100w_support",
			&bcdev->usbtemp_thread_100w_support);
	if (rc) {
		bcdev->usbtemp_thread_100w_support = false;
	}
	chg_err("usbtemp_thread_100w_support:%d\n", bcdev->usbtemp_thread_100w_support);

	rc = of_property_read_u32(node, "qcom,otg_curr_limit_max",
			&bcdev->otg_curr_limit_max);
	if (rc) {
		bcdev->otg_curr_limit_max = USB_OTG_CURR_LIMIT_MAX;
	}

        rc = of_property_read_u32(node, "qcom,otg_curr_limit_high",
                        &bcdev->otg_curr_limit_high);
        if (rc) {
                bcdev->otg_curr_limit_high = USB_OTG_CURR_LIMIT_HIGH;
        }

        rc = of_property_read_u32(node, "qcom,otg_real_soc_min",
                        &bcdev->otg_real_soc_min);
        if (rc) {
                bcdev->otg_real_soc_min = USB_OTG_REAL_SOC_MIN;
        }

	bcdev->subboard_adjust_support = of_property_read_bool(node, "oplus,support_subboard_adjust");
	chg_err("subboard_adjust_support:%d\n", bcdev->subboard_adjust_support);
	return 0;
}
#endif /*OPLUS_FEATURE_CHG_BASIC*/

static int battery_chg_parse_dt(struct battery_chg_dev *bcdev)
{
	struct device_node *node = bcdev->dev->of_node;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int i, rc, len;
	u32 prev, val;

#ifdef OPLUS_FEATURE_CHG_BASIC
	bcdev->otg_online = false;
	bcdev->pd_svooc = false;
#endif
#ifdef OPLUS_FEATURE_CHG_BASIC
	oplus_usbtemp_adc_gpio_dt(g_oplus_chip);
	oplus_subboard_temp_gpio_init(g_oplus_chip);
	oplus_btb_temp_gpio_init(g_oplus_chip);
	oplus_mos_btb_gpio_init(g_oplus_chip);
	oplus_ufcs_mos_switch_gpio_init(g_oplus_chip);
#endif
	of_property_read_string(node, "qcom,wireless-fw-name",
				&bcdev->wls_fw_name);

	rc = of_property_count_elems_of_size(node, "qcom,thermal-mitigation",
						sizeof(u32));
	if (rc <= 0)
		return 0;

	len = rc;

	rc = read_property_id(bcdev, pst, BATT_CHG_CTRL_LIM_MAX);
	if (rc < 0) {
		pr_err("Failed to read prop BATT_CHG_CTRL_LIM_MAX, rc=%d\n",
			rc);
		return rc;
	}

	prev = pst->prop[BATT_CHG_CTRL_LIM_MAX];

	for (i = 0; i < len; i++) {
		rc = of_property_read_u32_index(node, "qcom,thermal-mitigation",
						i, &val);
		if (rc < 0)
			return rc;

		if (val > prev) {
			pr_err("Thermal levels should be in descending order\n");
			bcdev->num_thermal_levels = -EINVAL;
			return 0;
		}

		prev = val;
	}

	bcdev->thermal_levels = devm_kcalloc(bcdev->dev, len + 1,
					sizeof(*bcdev->thermal_levels),
					GFP_KERNEL);
	if (!bcdev->thermal_levels)
		return -ENOMEM;

	/*
	 * Element 0 is for normal charging current. Elements from index 1
	 * onwards is for thermal mitigation charging currents.
	 */

	bcdev->thermal_levels[0] = pst->prop[BATT_CHG_CTRL_LIM_MAX];

	rc = of_property_read_u32_array(node, "qcom,thermal-mitigation",
					&bcdev->thermal_levels[1], len);
	if (rc < 0) {
		pr_err("Error in reading qcom,thermal-mitigation, rc=%d\n", rc);
		return rc;
	}

	bcdev->num_thermal_levels = len;
	bcdev->thermal_fcc_ua = pst->prop[BATT_CHG_CTRL_LIM_MAX];

	return 0;
}

static int battery_chg_ship_mode(struct notifier_block *nb, unsigned long code,
		void *unused)
{
	struct battery_charger_notify_msg msg_notify = { { 0 } };
	struct battery_charger_ship_mode_req_msg msg = { { 0 } };
	struct battery_chg_dev *bcdev = container_of(nb, struct battery_chg_dev,
						     reboot_notifier);
	int rc;

	msg_notify.hdr.owner = MSG_OWNER_BC;
	msg_notify.hdr.type = MSG_TYPE_NOTIFY;
	msg_notify.hdr.opcode = BC_SHUTDOWN_NOTIFY;

	rc = battery_chg_write(bcdev, &msg_notify, sizeof(msg_notify));
	if (rc < 0)
		pr_err("Failed to send shutdown notification rc=%d\n", rc);

	if (!bcdev->ship_mode_en)
		return NOTIFY_DONE;

	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_SHIP_MODE_REQ_SET;
	msg.ship_mode_type = SHIP_MODE_PMIC;

	if (code == SYS_POWER_OFF) {
		rc = battery_chg_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			pr_emerg("Failed to write ship mode: %d\n", rc);
	}

	return NOTIFY_DONE;
}

/**********************************************************************
 * battery charge ops *
 **********************************************************************/
#ifdef OPLUS_FEATURE_CHG_BASIC

#define USBTEMP_TRIGGER_CONDITION_1	1
#define USBTEMP_TRIGGER_CONDITION_2	2
#define USBTEMP_TRIGGER_CONDITION_COOL_DOWN	3
#define USBTEMP_TRIGGER_CONDITION_COOL_DOWN_RECOVERY 4
static int oplus_chg_track_upload_usbtemp_info(
	struct oplus_chg_chip *chip, int condition,
	int last_usb_temp_l, int last_usb_temp_r, int batt_current)
{
	int index = 0;

	mutex_lock(&chip->track_upload_lock);
	memset(chip->usbtemp_load_trigger.crux_info,
		0, sizeof(chip->usbtemp_load_trigger.crux_info));
	oplus_chg_track_obtain_power_info(chip->chg_power_info, sizeof(chip->chg_power_info));
	if (condition == USBTEMP_TRIGGER_CONDITION_1) {
		index += snprintf(&(chip->usbtemp_load_trigger.crux_info[index]),
				OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
				"$$reason@@%s", "first_condition");
		index += snprintf(&(chip->usbtemp_load_trigger.crux_info[index]),
				OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
				"$$batt_temp@@%d$$usb_temp_l@@%d"
				"$$usb_temp_r@@%d",
				chip->tbatt_temp, chip->usb_temp_l,
				chip->usb_temp_r);
	} else if (condition == USBTEMP_TRIGGER_CONDITION_2) {
		index += snprintf(&(chip->usbtemp_load_trigger.crux_info[index]),
				OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
				"$$reason@@%s", "second_condition");
		index += snprintf(&(chip->usbtemp_load_trigger.crux_info[index]),
				OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
				"$$batt_temp@@%d$$usb_temp_l@@%d"
				"$$last_usb_temp_l@@%d"
				"$$usb_temp_r@@%d$$last_usb_temp_r@@%d",
				chip->tbatt_temp, chip->usb_temp_l, last_usb_temp_l,
				chip->usb_temp_r, last_usb_temp_r);
	} else if (condition == USBTEMP_TRIGGER_CONDITION_COOL_DOWN) {
		index += snprintf(&(chip->usbtemp_load_trigger.crux_info[index]),
				OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
				"$$reason@@%s", "cool_down_condition");
		index += snprintf(&(chip->usbtemp_load_trigger.crux_info[index]),
				OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
				"$$batt_temp@@%d$$usb_temp_l@@%d"
				"$$usb_temp_r@@%d$$batt_current@@%d",
				chip->tbatt_temp, chip->usb_temp_l,
				chip->usb_temp_r, batt_current);
	} else if (condition == USBTEMP_TRIGGER_CONDITION_COOL_DOWN_RECOVERY) {
		index += snprintf(&(chip->usbtemp_load_trigger.crux_info[index]),
				OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
				"$$reason@@%s", "cool_down_recovery_condition");
		index += snprintf(&(chip->usbtemp_load_trigger.crux_info[index]),
				OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
				"$$batt_temp@@%d$$usb_temp_l@@%d"
				"$$usb_temp_r@@%d$$batt_current@@%d",
				chip->tbatt_temp, chip->usb_temp_l,
				chip->usb_temp_r, batt_current);
	} else {
		chg_err("!!!condition err\n");
		mutex_unlock(&chip->track_upload_lock);
		return -1;
	}

	index += snprintf(&(chip->usbtemp_load_trigger.crux_info[index]),
			OPLUS_CHG_TRACK_CURX_INFO_LEN - index, "%s", chip->chg_power_info);

	schedule_delayed_work(&chip->usbtemp_load_trigger_work, 0);
	pr_info("%s\n", chip->usbtemp_load_trigger.crux_info);
	mutex_unlock(&chip->track_upload_lock);

	return 0;
}

static int oplus_subboard_temp_iio_init(struct oplus_chg_chip *chip)
{
	int rc;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip not ready!\n");
		return false;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	rc = of_property_match_string(bcdev->dev->of_node, "io-channel-names", "subboard_temp_adc");
	if (rc >= 0) {
		bcdev->iio.subboard_temp_chan = iio_channel_get(bcdev->dev, "subboard_temp_adc");
		if (IS_ERR(bcdev->iio.subboard_temp_chan)) {
			rc = PTR_ERR(bcdev->iio.subboard_temp_chan);
			if (rc != -EPROBE_DEFER)
				chg_err("subboard_temp_chan get error, %d\n", rc);
			bcdev->iio.subboard_temp_chan = NULL;
			return rc;
		}
		chg_err("bcdev->iio.subboard_temp_chan OK\n");
	} else
		chg_err("can't find subboard_temp_adc node\n");

	return rc;
}

#define SUBBOARD_TEMP_DEFAULT_VALUE	250
#define SUBBOARD_HIGH_TEMP 690
#define SUBBOARD_TEMP_ADJUST_TEMP_LOW 150
#define SUBBOARD_TEMP_ADJUST_TEMP_HIGH 480
#define SUBBOARD_TEMP_ADJUST_IBAT_LEVEL0 7500
#define SUBBOARD_TEMP_ADJUST_IBAT_LEVEL1 8600
#define SUBBOARD_TEMP_ADJUST_IBAT_LEVEL2 10500
#define SUBBOARD_TEMP_ADJUST_IBAT_LEVEL3 12500
#define SUBBOARD_TEMP_ADJUST_TMAX_LEVEL0 10
#define SUBBOARD_TEMP_ADJUST_TMAX_LEVEL1 20
#define SUBBOARD_TEMP_ADJUST_TMAX_LEVEL2 40
#define SUBBOARD_TEMP_ADJUST_TMAX_LEVEL3 50

static int oplus_subboard_temp_adjust(int subboard_temp)
{
	static int cb_count = 0;
	static bool cb_flag = false;
	static int adjust_max = 0;
	int icharging = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (chip == NULL) {
		return 0;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	subboard_temp = subboard_temp / 100;

	if (!bcdev->subboard_adjust_support)
		return subboard_temp;
	icharging = -oplus_gauge_get_batt_current();

	if ((subboard_temp >= SUBBOARD_TEMP_ADJUST_TEMP_LOW) && (subboard_temp < SUBBOARD_TEMP_ADJUST_TEMP_HIGH) &&
	    (icharging >= SUBBOARD_TEMP_ADJUST_IBAT_LEVEL0)) {
		if (icharging >= SUBBOARD_TEMP_ADJUST_IBAT_LEVEL3) {
			adjust_max = SUBBOARD_TEMP_ADJUST_TMAX_LEVEL3;
		} else if (icharging >= SUBBOARD_TEMP_ADJUST_IBAT_LEVEL2) {
			adjust_max = SUBBOARD_TEMP_ADJUST_TMAX_LEVEL2;
		} else if (icharging >= SUBBOARD_TEMP_ADJUST_IBAT_LEVEL1) {
			adjust_max = SUBBOARD_TEMP_ADJUST_TMAX_LEVEL1;
		} else {
			adjust_max = SUBBOARD_TEMP_ADJUST_TMAX_LEVEL0;
		}

		cb_flag = true;
		if (cb_count > adjust_max)
			cb_count--;
		else if (cb_count < adjust_max)
			cb_count++;
		else
			chg_err("[%d, %d]\n", subboard_temp, cb_count);
	} else if (cb_flag && cb_count > 0) {
		cb_count--;
	} else {
		cb_count = 0;
		cb_flag = false;
	}

	subboard_temp -= cb_count;
	return subboard_temp;
}

static int oplus_get_subboard_temp(void)
{
	int rc;
	int subboard_temp = SUBBOARD_TEMP_DEFAULT_VALUE;
	static int subboard_temp_pre = SUBBOARD_TEMP_DEFAULT_VALUE;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip not ready!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (IS_ERR_OR_NULL(bcdev->iio.subboard_temp_chan)) {
		chg_err("bcdev->iio.subboard_temp_chan is NULL !\n");
		subboard_temp = subboard_temp_pre;
		goto exit;
	}

	rc = iio_read_channel_processed(bcdev->iio.subboard_temp_chan, &subboard_temp);
	if (rc < 0) {
		chg_err("iio_read_channel_processed get error\n");
		subboard_temp = subboard_temp_pre;
		goto exit;
	}

	/* millidegreeC to degreeC*10 for charging */
	subboard_temp = oplus_subboard_temp_adjust(subboard_temp);
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	if ((get_eng_version() == HIGH_TEMP_AGING) || (get_eng_version() == PTCRB)) {
		chg_err("CONFIG_HIGH_TEMP_VERSION enable here, disable high subboard temp shutdown\n");
		if (subboard_temp > SUBBOARD_HIGH_TEMP)
			subboard_temp = SUBBOARD_HIGH_TEMP;
	}
#endif
exit:
	return subboard_temp;
}

static int oplus_subboard_temp_gpio_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip not ready!\n");
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err("bcdev not ready!\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.subboard_temp_gpio_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.subboard_temp_gpio_pinctrl)) {
		chg_err("get subboard_temp_gpio_pinctrl fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.subboard_temp_gpio_default =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.subboard_temp_gpio_pinctrl, "subboard_temp_gpio_default");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.subboard_temp_gpio_default)) {
		chg_err("set subboard_temp_gpio_default error\n");
		return -EINVAL;
	}

	pinctrl_select_state(bcdev->oplus_custom_gpio.subboard_temp_gpio_pinctrl,
		bcdev->oplus_custom_gpio.subboard_temp_gpio_default);

	return 0;
}

static int oplus_batbtb0_gpio_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.batt0_btb_gpio_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.batt0_btb_gpio_pinctrl)) {
		chg_err("get batt0_btb_gpio_pinctrl fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.batt0_btb_gpio_default =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.batt0_btb_gpio_pinctrl, "batt0_btb_temp_gpio_default");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.batt0_btb_gpio_default)) {
		chg_err("get batt0_btb_temp_gpio_default error\n");
		return -EINVAL;
	}

	pinctrl_select_state(bcdev->oplus_custom_gpio.batt0_btb_gpio_pinctrl,
		bcdev->oplus_custom_gpio.batt0_btb_gpio_default);
	chg_err("[OPLUS_CHG][%s]: batt0_btb_temp_gpio_default !\n", __func__);

	return 0;
}

static int oplus_batbtb1_gpio_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.batt1_btb_gpio_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.batt1_btb_gpio_pinctrl)) {
		chg_err("get batt0_btb_gpio_pinctrl fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.batt1_btb_gpio_default =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.batt1_btb_gpio_pinctrl, "batt1_btb_temp_gpio_default");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.batt1_btb_gpio_default)) {
		chg_err("get batt1_btb_gpio_default error\n");
		return -EINVAL;
	}

	pinctrl_select_state(bcdev->oplus_custom_gpio.batt1_btb_gpio_pinctrl,
		bcdev->oplus_custom_gpio.batt1_btb_gpio_default);
	chg_err("[OPLUS_CHG][%s]: batt1_btb_temp_gpio_default !\n", __func__);

	return 0;
}

static int oplus_mos0btb_gpio_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip not ready!\n");
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err(": bcdev not ready!\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.mos0_btb_gpio_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.mos0_btb_gpio_pinctrl)) {
		chg_err("get mos0_btb_gpio_pinctrl fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.mos0_btb_gpio_default =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.mos0_btb_gpio_pinctrl, "mos0_btb_temp_gpio_default");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.mos0_btb_gpio_default)) {
		chg_err("get mos0_btb_temp_gpio_default error\n");
		return -EINVAL;
	}

	pinctrl_select_state(bcdev->oplus_custom_gpio.mos0_btb_gpio_pinctrl,
			     bcdev->oplus_custom_gpio.mos0_btb_gpio_default);
	chg_err(": mos0_btb_temp_gpio_default !\n");

	return 0;
}

static int oplus_mos1btb_gpio_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err(": chip not ready!\n");
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err(": bcdev not ready!\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.mos1_btb_gpio_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.mos1_btb_gpio_pinctrl)) {
		chg_err("get mos1_btb_gpio_pinctrl fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.mos1_btb_gpio_default =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.mos1_btb_gpio_pinctrl, "mos1_btb_temp_gpio_default");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.mos1_btb_gpio_default)) {
		chg_err("get mos1_btb_gpio_default error\n");
		return -EINVAL;
	}

	pinctrl_select_state(bcdev->oplus_custom_gpio.mos1_btb_gpio_pinctrl,
		bcdev->oplus_custom_gpio.mos1_btb_gpio_default);
	chg_err(": mos1_btb_temp_gpio_default !\n");

	return 0;
}

static int oplus_mos_btb_gpio_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip not ready!\n");
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err("bcdev not ready!\n");
		return -EINVAL;
	}

	oplus_mos0btb_gpio_init(chip);
	oplus_mos1btb_gpio_init(chip);

	return 0;
}

static int oplus_btb_temp_gpio_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip not ready!\n");
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err("bcdev not ready!\n");
		return -EINVAL;
	}

	oplus_batbtb0_gpio_init(chip);
	oplus_batbtb1_gpio_init(chip);

	return 0;
}

static int oplus_usbtemp_iio_init(struct oplus_chg_chip *chip)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip not ready!\n");
		return false;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	rc = of_property_match_string(bcdev->dev->of_node, "io-channel-names", "usb_temp_adc");
	if (rc >= 0) {
		bcdev->iio.usbtemp_v_chan = iio_channel_get(bcdev->dev, "usb_temp_adc");
		if (IS_ERR(bcdev->iio.usbtemp_v_chan)) {
			rc = PTR_ERR(bcdev->iio.usbtemp_v_chan);
			if (rc != -EPROBE_DEFER)
				chg_err("usb_temp_adc get error, %d\n", rc);
			bcdev->iio.usbtemp_v_chan = NULL;
			return rc;
		}
		chg_err("bcdev->iio.usb_temp_adc OK\n");
	} else
		chg_err("can't find usb_temp_adc node\n");

	rc = of_property_match_string(bcdev->dev->of_node, "io-channel-names", "usb_supplementary_temp_adc");
	if (rc >= 0) {
		bcdev->iio.usbtemp_sup_v_chan = iio_channel_get(bcdev->dev,
					"usb_supplementary_temp_adc");
		if (IS_ERR(bcdev->iio.usbtemp_sup_v_chan)) {
			rc = PTR_ERR(bcdev->iio.usbtemp_sup_v_chan);
			if (rc != -EPROBE_DEFER)
				chg_err("usb_supplementary_temp_adc get error, %d\n", rc);
			bcdev->iio.usbtemp_sup_v_chan = NULL;
			return rc;
		}
		chg_err("bcdev->iio.usb_supplementary_temp_adc OK\n");
	} else
		chg_err("can't find usb_supplementary_temp_adc node\n");

	return rc;
}

#define BTB_TEMP_DEFAULT	25
int oplus_chg_get_battery_btb_temp_cal(void)
{
	int rc;
	int temp = BTB_TEMP_DEFAULT;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	int batt0_con_btb_temp = BTB_TEMP_DEFAULT;
	int batt1_con_btb_temp = BTB_TEMP_DEFAULT;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return temp;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!IS_ERR_OR_NULL(bcdev->iio.batt0_con_btb_chan)) {
		rc = iio_read_channel_processed(bcdev->iio.batt0_con_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			 batt0_con_btb_temp = temp / 1000;
		}
	} else {
		chg_err("[OPLUS_CHG]: batt0_con_btb_chan is NULL !\n");
	}

	if (!IS_ERR_OR_NULL(bcdev->iio.batt1_con_btb_chan)) {
		rc = iio_read_channel_processed(bcdev->iio.batt1_con_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			batt1_con_btb_temp = temp / 1000;
		}
	} else {
		chg_err("[OPLUS_CHG] batt1_con_btb_chan is NULL !\n");
	}

	return batt0_con_btb_temp > batt1_con_btb_temp ? batt0_con_btb_temp : batt1_con_btb_temp;
}

int oplus_chg_get_battery_btb_tdiff(void)
{
	int rc;
	int temp = BTB_TEMP_DEFAULT;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	int batt0_con_btb_temp = BTB_TEMP_DEFAULT;
	int batt1_con_btb_temp = BTB_TEMP_DEFAULT;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return temp;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!IS_ERR_OR_NULL(bcdev->iio.batt0_con_btb_chan)) {
		rc = iio_read_channel_processed(bcdev->iio.batt0_con_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			 batt0_con_btb_temp = temp / 1000;
		}
	} else {
		chg_err("[OPLUS_CHG]: batt0_con_btb_chan is NULL !\n");
	}

	if (!IS_ERR_OR_NULL(bcdev->iio.batt1_con_btb_chan)) {
		rc = iio_read_channel_processed(bcdev->iio.batt1_con_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			batt1_con_btb_temp = temp / 1000;
		}
	} else {
		chg_err("[OPLUS_CHG] batt1_con_btb_chan is NULL !\n");
	}

	chg_err("batt_con_btb_temp %d %d\n", batt0_con_btb_temp, batt1_con_btb_temp);

	return abs(batt0_con_btb_temp - batt1_con_btb_temp);
}

int oplus_chg_get_usb_btb_temp_cal(void)
{
	int rc;
	int temp = 25;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		printk(KERN_ERR "[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return temp;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (IS_ERR_OR_NULL(bcdev->iio.usbcon_btb_chan)) {
		printk(KERN_ERR "[OPLUS_CHG][%s]: bcdev->iio.usbcon_btb_chan  is  NULL !\n", __func__);
		return temp;
	}

	rc = iio_read_channel_processed(bcdev->iio.usbcon_btb_chan, &temp);
	if (rc < 0) {
		chg_err("[OPLUS_CHG][%s]: iio_read_channel_processed  get error\n", __func__);
		return temp;
	}

	return temp / 1000;
}

static int oplus_btbtemp_iio_init(struct oplus_chg_chip *chip)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	int temp = BTB_TEMP_DEFAULT;
	int batt0_con_btb_temp = BTB_TEMP_DEFAULT;

	if (!chip) {
		chg_err("chip not ready!\n");
		return false;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err("bcdev not ready!\n");
		return -EINVAL;
	}

	rc = of_property_match_string(bcdev->dev->of_node, "io-channel-names", "batt0_con_therm_adc");
	if (rc >= 0) {
		bcdev->iio.batt0_con_btb_chan = iio_channel_get(bcdev->dev, "batt0_con_therm_adc");
		if (IS_ERR(bcdev->iio.batt0_con_btb_chan)) {
			rc = PTR_ERR(bcdev->iio.batt0_con_btb_chan);
			if (rc != -EPROBE_DEFER) {
				dev_err(bcdev->dev, "batt0_con_btb_chan  get error, %ld\n", rc);
				bcdev->iio.batt0_con_btb_chan = NULL;
				return rc;
			}
			chg_err(" err bcdev->iio.batt0_con_btb_chan, rc = %ld\n", rc);
		}
		chg_err(" bcdev->iio.batt0_con_btb_chan\n");
	}
	if (!(IS_ERR_OR_NULL(bcdev->iio.batt0_con_btb_chan))) {
		rc = iio_read_channel_processed(bcdev->iio.batt0_con_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			 batt0_con_btb_temp = temp / 1000;
		}
		chg_err(" batt0_con_btb_chan is ok ! %d, %d\n", temp, batt0_con_btb_temp);
	} else {
		chg_err("batt0_con_btb_chan is NULL !\n");
	}

	rc = of_property_match_string(bcdev->dev->of_node, "io-channel-names", "batt1_con_therm_adc");
	if (rc >= 0) {
		bcdev->iio.batt1_con_btb_chan = iio_channel_get(bcdev->dev, "batt1_con_therm_adc");
		if (IS_ERR(bcdev->iio.batt1_con_btb_chan)) {
			rc = PTR_ERR(bcdev->iio.batt1_con_btb_chan);
			if (rc != -EPROBE_DEFER) {
				dev_err(bcdev->dev, "batt1_con_btb_chan  get error, %ld\n", rc);
				bcdev->iio.batt1_con_btb_chan = NULL;
				return rc;
			}
		}
		pr_err("[OPLUS_CHG] test bcdev->iio.batt1_con_btb_chan\n");
	}
	if (!(IS_ERR_OR_NULL(bcdev->iio.batt1_con_btb_chan))) {
		rc = iio_read_channel_processed(bcdev->iio.batt1_con_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			 batt0_con_btb_temp = temp / 1000;
		}
		chg_err(" batt1_con_btb_chan is ok ! %d, %d\n", temp, batt0_con_btb_temp);
	} else {
		chg_err("batt1_con_btb_chan is NULL !\n");
	}

	rc = of_property_match_string(bcdev->dev->of_node, "io-channel-names", "conn_therm");
	if (rc >= 0) {
		bcdev->iio.usbcon_btb_chan = iio_channel_get(bcdev->dev, "conn_therm");
		if (IS_ERR(bcdev->iio.usbcon_btb_chan)) {
			rc = PTR_ERR(bcdev->iio.usbcon_btb_chan);
			if (rc != -EPROBE_DEFER) {
				dev_err(bcdev->dev, "usbcon_btb_chan  get error, %ld\n", rc);
				bcdev->iio.usbcon_btb_chan = NULL;
				return rc;
			}
		}
		pr_err("[OPLUS_CHG] test bcdev->iio.usbcon_btb_chan\n");
	}

	if (!(IS_ERR_OR_NULL(bcdev->iio.usbcon_btb_chan))) {
		rc = iio_read_channel_processed(bcdev->iio.usbcon_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			 batt0_con_btb_temp = temp / 1000;
		}
		chg_err(" usbcon_btb_chan is ok ! %d, %d\n", temp, batt0_con_btb_temp);
	} else {
		chg_err("usbcon_btb_chan is NULL !\n");
	}

	return rc;
}


int oplus_chg_get_mos_btb_temp_cal(void)
{
	int rc;
	int temp = BTB_TEMP_DEFAULT;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	int mos0_con_btb_temp = BTB_TEMP_DEFAULT;
	int mos1_con_btb_temp = BTB_TEMP_DEFAULT;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return temp;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!IS_ERR_OR_NULL(bcdev->iio.batt0_con_btb_chan)) {
		rc = iio_read_channel_processed(bcdev->iio.batt0_con_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			 mos0_con_btb_temp = temp / 1000;
		}
	} else {
		chg_err("[OPLUS_CHG]: mos0_con_btb_chan is NULL !\n");
	}

	if (!IS_ERR_OR_NULL(bcdev->iio.mos1_con_btb_chan)) {
		rc = iio_read_channel_processed(bcdev->iio.mos1_con_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			mos1_con_btb_temp = temp / 1000;
		}
	} else {
		chg_err("[OPLUS_CHG] mos1_con_btb_chan is NULL !\n");
	}

	return mos0_con_btb_temp > mos1_con_btb_temp ? mos0_con_btb_temp : mos1_con_btb_temp;
}

static int oplus_mosbtb_iio_init(struct oplus_chg_chip *chip)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	int temp = BTB_TEMP_DEFAULT;
	int mos0_con_btb_temp = BTB_TEMP_DEFAULT;
	int mos1_con_btb_temp = BTB_TEMP_DEFAULT;

	if (!chip) {
		chg_err("chip not ready!\n");
		return false;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err("bcdev not ready!\n");
		return -EINVAL;
	}

	rc = of_property_match_string(bcdev->dev->of_node, "io-channel-names", "mos0_con_therm_adc");
	if (rc >= 0) {
		bcdev->iio.mos0_con_btb_chan = iio_channel_get(bcdev->dev, "mos0_con_therm_adc");
		if (IS_ERR(bcdev->iio.mos0_con_btb_chan)) {
			rc = PTR_ERR(bcdev->iio.mos0_con_btb_chan);
			if (rc != -EPROBE_DEFER) {
				dev_err(bcdev->dev, "mos0_con_btb_chan  get error, %ld\n", rc);
				bcdev->iio.mos0_con_btb_chan = NULL;
			}
			chg_err(" err bcdev->iio.mos0_con_btb_chan, rc = %ld\n", rc);
		}
		chg_err(" bcdev->iio.mos0_con_btb_chan\n");
	}

	rc = of_property_match_string(bcdev->dev->of_node, "io-channel-names", "mos1_con_therm_adc");
	if (rc >= 0) {
		bcdev->iio.mos1_con_btb_chan = iio_channel_get(bcdev->dev, "mos1_con_therm_adc");
		if (IS_ERR(bcdev->iio.mos1_con_btb_chan)) {
			rc = PTR_ERR(bcdev->iio.mos1_con_btb_chan);
			if (rc != -EPROBE_DEFER) {
				dev_err(bcdev->dev, "mos1_con_btb_chan  get error, %ld\n", rc);
				bcdev->iio.mos1_con_btb_chan = NULL;
				return rc;
			}
		}
		chg_err(" bcdev->iio.mos1_con_btb_chan\n");
	}

	if (!(IS_ERR_OR_NULL(bcdev->iio.mos0_con_btb_chan))) {
		rc = iio_read_channel_processed(bcdev->iio.mos0_con_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			 mos0_con_btb_temp = temp / 1000;
		}
		chg_err(" mos0_con_btb_chan is ok ! %d, %d\n", temp, mos0_con_btb_temp);
	} else {
		chg_err("mos0_con_btb_chan is NULL !\n");
	}
	if (!(IS_ERR_OR_NULL(bcdev->iio.mos1_con_btb_chan))) {
		rc = iio_read_channel_processed(bcdev->iio.mos1_con_btb_chan, &temp);
		if (rc < 0) {
			chg_err("[OPLUS_CHG] iio_read_channel_processed get error\n");
		} else {
			mos1_con_btb_temp = temp / 1000;
		}
			chg_err("mos1_con_btb_chan is ok ! %d, %d\n", temp, mos1_con_btb_temp);
	} else {
		chg_err(" mos1_con_btb_chan is NULL !\n");
	}

	return rc;
}


static bool oplus_usbtemp_check_is_gpio(struct oplus_chg_chip *chip)
{
	if (!chip) {
		chg_err("chip not ready!\n");
		return false;
	}

	if (gpio_is_valid(chip->normalchg_gpio.dischg_gpio))
		return true;

	return false;
}

static bool oplus_usbtemp_check_is_support(void)
{
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	if (get_eng_version() == AGING) {
		chg_err("AGING mode, disable usbtemp\n");
		return false;
	}
#endif

	if (oplus_usbtemp_check_is_gpio(g_oplus_chip) == true)
		return true;

	chg_err("dischg return false\n");

	return false;
}

#define USBTEMP_DEFAULT_VOLT_VALUE_MV 950
void oplus_get_usbtemp_volt(struct oplus_chg_chip *chip)
{
	int rc = 0;
	int usbtemp_volt = 0;
	struct battery_chg_dev *bcdev = NULL;
	static int usbtemp_volt_l_pre = USBTEMP_DEFAULT_VOLT_VALUE_MV;
	static int usbtemp_volt_r_pre = USBTEMP_DEFAULT_VOLT_VALUE_MV;

	if (!chip) {
		chg_err("chip not ready!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (IS_ERR_OR_NULL(bcdev->iio.usbtemp_v_chan)) {
		chg_err("bcdev->iio.usbtemp_v_chan is NULL !\n");
		chip->usbtemp_volt_l = usbtemp_volt_l_pre;
		goto usbtemp_next;
	}

	rc = iio_read_channel_processed(bcdev->iio.usbtemp_v_chan, &usbtemp_volt);
	if (rc < 0) {
		chg_err("iio_read_channel_processed get error\n");
		chip->usbtemp_volt_l = usbtemp_volt_l_pre;
		goto usbtemp_next;
	}

	usbtemp_volt = 18 * usbtemp_volt / 10000;
	if (usbtemp_volt > USBTEMP_DEFAULT_VOLT_VALUE_MV) {
		usbtemp_volt = USBTEMP_DEFAULT_VOLT_VALUE_MV;
	}

	chip->usbtemp_volt_l = usbtemp_volt;
	usbtemp_volt_l_pre = usbtemp_volt;
usbtemp_next:
	usbtemp_volt = 0;
	if (IS_ERR_OR_NULL(bcdev->iio.usbtemp_sup_v_chan)) {
		chg_err("chg->iio.usbtemp_sup_v_chan is NULL !\n");
		chip->usbtemp_volt_r = usbtemp_volt_r_pre;
		return;
	}

	rc = iio_read_channel_processed(bcdev->iio.usbtemp_sup_v_chan, &usbtemp_volt);
	if (rc < 0) {
		chg_err("iio_read_channel_processed get error\n");
		chip->usbtemp_volt_r = usbtemp_volt_r_pre;
		return;
	}

	usbtemp_volt = 18 * usbtemp_volt / 10000;
	if (usbtemp_volt > USBTEMP_DEFAULT_VOLT_VALUE_MV) {
		usbtemp_volt = USBTEMP_DEFAULT_VOLT_VALUE_MV;
	}

	chip->usbtemp_volt_r = usbtemp_volt;
	usbtemp_volt_r_pre = usbtemp_volt;

	/*chg_err("usbtemp_volt_l:%d, usbtemp_volt_r:%d\n",chip->usbtemp_volt_l, chip->usbtemp_volt_r);*/
}

int oplus_get_usbtemp_volt_l(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (chip == NULL) {
		return 0;
	}

	return chip->usbtemp_volt_l;
}

int oplus_get_usbtemp_volt_r(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (chip == NULL) {
		return 0;
	}

	return chip->usbtemp_volt_r;
}

static int oplus_dischg_gpio_init(struct oplus_chg_chip *chip)
{
	if (!chip) {
		chg_err("chip NULL\n");
		return EINVAL;
	}

	chip->normalchg_gpio.pinctrl = devm_pinctrl_get(chip->dev);

	if (IS_ERR_OR_NULL(chip->normalchg_gpio.pinctrl)) {
		chg_err("get dischg_pinctrl fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.dischg_enable = pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "dischg_enable");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_enable)) {
		chg_err("get dischg_enable fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.dischg_disable = pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "dischg_disable");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_disable)) {
		chg_err("get dischg_disable fail\n");
		return -EINVAL;
	}

	pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.dischg_disable);

	return 0;
}

void oplus_set_usb_status(int status)
{
	if (g_oplus_chip)
		g_oplus_chip->usb_status = g_oplus_chip->usb_status | status;
}

void oplus_clear_usb_status(int status)
{
	if (g_oplus_chip)
		g_oplus_chip->usb_status = g_oplus_chip->usb_status & (~status);
}

int oplus_get_usb_status(void)
{
	if (g_oplus_chip)
		return g_oplus_chip->usb_status;
	return 0;
}

#define USB_20C 20
#define USB_40C	40
#define USB_30C 30
#define USB_50C	50
#define USB_55C	55
#define USB_57C	57
#define USB_100C 100

#define USB_50C_VOLT 467
#define USB_55C_VOLT 400
#define USB_57C_VOLT 376
#define USB_100C_VOLT 100
#define VBUS_VOLT_THRESHOLD	400

#define VBUS_MONITOR_INTERVAL	3000

#define MIN_MONITOR_INTERVAL	50
#define MAX_MONITOR_INTERVAL	50
#define RETRY_CNT_DELAY         5
#define HIGH_TEMP_SHORT_CHECK_TIMEOUT 1000
static void get_usb_temp(struct oplus_chg_chip *chg)
{
	int i = 0;

	for (i = ARRAY_SIZE(con_volt_855) - 1; i >= 0; i--) {
		if (con_volt_855[i] >= chg->usbtemp_volt_l)
			break;
		else if (i == 0)
			break;
	}

	if (usbtemp_dbg_templ != 0)
		chg->usb_temp_l = usbtemp_dbg_templ;
	else
		chg->usb_temp_l = con_temp_855[i];

	for (i = ARRAY_SIZE(con_volt_855) - 1; i >= 0; i--) {
		if (con_volt_855[i] >= chg->usbtemp_volt_r)
			break;
		else if (i == 0)
			break;
	}

	if (usbtemp_dbg_tempr != 0)
		chg->usb_temp_r = usbtemp_dbg_tempr;
	else
		chg->usb_temp_r = con_temp_855[i];
	if (oplus_is_pps_charging() && (oplus_pps_get_support_type() == PPS_SUPPORT_3CP)) {
		if(chg->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR)
			chg->usb_temp_r = chg->usb_temp_r - chg->usbtemp_cool_down_recoup_high;
		else
			chg->usb_temp_r = chg->usb_temp_r - chg->usbtemp_cool_down_recoup_low;
	}
}

static void oplus_set_usbtemp_wakelock(bool value)
{
	if(value) {
		__pm_stay_awake(usbtemp_wakelock);
	} else {
		__pm_relax(usbtemp_wakelock);
	}
}

static void oplus_usbtemp_recover_func(struct oplus_chg_chip *chip)
{
	int level;
	int count_time = 0;
	struct battery_chg_dev *bcdev = NULL;
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (gpio_is_valid(chip->normalchg_gpio.dischg_gpio)) {
		level = gpio_get_value(chip->normalchg_gpio.dischg_gpio);
	} else {
		return;
	}

	if (level == 1) {
		oplus_set_usbtemp_wakelock(true);
		do {
			oplus_get_usbtemp_volt(chip);
			get_usb_temp(chip);
			msleep(2000);
			count_time++;
		} while (!(((chip->usb_temp_r < USB_55C || chip->usb_temp_r == USB_100C)
			&& (chip->usb_temp_l < USB_55C ||  chip->usb_temp_l == USB_100C)) || count_time == 30));
		oplus_set_usbtemp_wakelock(false);
		if (count_time == 30) {
			chg_err("[OPLUS_USBTEMP] temp still high");
		} else {
			chip->dischg_flag = false;
			chg_err("dischg disable...[%d]\n", chip->usbtemp_volt);
			oplus_clear_usb_status(USB_TEMP_HIGH);

			mutex_lock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));
			pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.dischg_disable);
			mutex_unlock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));

			chg_err("[OPLUS_USBTEMP] usbtemp recover");
		}
	}
	return;
}

static void oplus_usbtemp_recover_work(struct work_struct *work)
{
	oplus_usbtemp_recover_func(g_oplus_chip);
}

static int g_tbatt_temp = 0;

#define USBTEMP_BATTTEMP_GAP_HIGH 19
#define USBTEMP_BATTTEMP_CURRENT_GAP_HIGH 17
#define USBTEMP_MAX_TEMP_THR_HIGH 65
#define USBTEMP_MAX_TEMP_DIFF_HIGH 9
#define USBTEMP_BATTTEMP_RECOVER_GAP_HIGH 11

#define USBTEMP_BATTTEMP_GAP_DEFAULT 12
#define USBTEMP_BATTTEMP_CURRENT_GAP_DEFAULT 12
#define USBTEMP_MAX_TEMP_THR_DEFAULT 57
#define USBTEMP_MAX_TEMP_DIFF_DEFAULT 7
#define USBTEMP_BATTTEMP_RECOVER_GAP_DEFAULT 6

static void oplus_usbtemp_recover_tbatt_temp(struct oplus_chg_chip *chip) {
	int chg_type = 0;
	int batt_temp = 0;
	int sub_batt_temp = 0;

	if (oplus_pps_get_support_type() == PPS_SUPPORT_2CP ||
	    oplus_pps_get_support_type() == PPS_SUPPORT_3CP ||
	    chip->vooc_project == DUAL_BATT_100W) {
		g_tbatt_temp = oplus_gauge_get_batt_temperature();
	} else if (oplus_switching_support_parallel_chg()) {
		batt_temp = oplus_gauge_get_batt_temperature();
		sub_batt_temp = oplus_gauge_get_sub_batt_temperature();
		g_tbatt_temp = batt_temp > sub_batt_temp ? batt_temp : sub_batt_temp;
	} else {
		g_tbatt_temp = chip->tbatt_temp;
	}
	chg_type = oplus_vooc_get_fast_chg_type();
	/*chg_err("g_tbatt_temp:%d, tbatt_temp:%d, chg_type:%d\n", g_tbatt_temp, chip->tbatt_temp, chg_type);*/

	if ((chg_type == ADAPTER_ID_20W_0X13 || chg_type == ADAPTER_ID_20W_0X34 ||
		chg_type == ADAPTER_ID_20W_0X45) && chip->usbtemp_change_gap) {/*20W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH;
	} else if ((chg_type == ADAPTER_ID_30W_0X19 || chg_type == ADAPTER_ID_30W_0X29 ||
		chg_type == ADAPTER_ID_30W_0X41 || chg_type == ADAPTER_ID_30W_0X42 ||
		chg_type == ADAPTER_ID_30W_0X43 || chg_type == ADAPTER_ID_30W_0X44 ||
		chg_type == ADAPTER_ID_30W_0X46) && chip->usbtemp_change_gap) {/*30W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH;
	} else if ((chg_type == ADAPTER_ID_33W_0X49 || chg_type == ADAPTER_ID_33W_0X4A ||
		chg_type == ADAPTER_ID_33W_0X61) && chip->usbtemp_change_gap) {/*33W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH;
	} else if ((chg_type == ADAPTER_ID_50W_0X11 || chg_type == ADAPTER_ID_50W_0X12 ||
		chg_type == ADAPTER_ID_50W_0X21 || chg_type == ADAPTER_ID_50W_0X31 ||
		chg_type == ADAPTER_ID_50W_0X33 || chg_type == ADAPTER_ID_50W_0X62) &&
		chip->usbtemp_change_gap) {/*50W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH;
	} else if ((chg_type == ADAPTER_ID_65W_0X14 || chg_type == ADAPTER_ID_65W_0X35 ||
		chg_type == ADAPTER_ID_65W_0X63 || chg_type == ADAPTER_ID_65W_0X66 ||
		chg_type == ADAPTER_ID_65W_0X6E) && chip->usbtemp_change_gap) {/*65W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH;
	} else if ((chg_type == ADAPTER_ID_66W_0X36 || chg_type == ADAPTER_ID_66W_0X64) &&
		chip->usbtemp_change_gap) {/*66W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH;
	} else if ((chg_type == ADAPTER_ID_67W_0X6C || chg_type == ADAPTER_ID_67W_0X6D) &&
		chip->usbtemp_change_gap) {/*67W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH;
	} else if ((chg_type == ADAPTER_ID_80W_0X4B || chg_type == ADAPTER_ID_80W_0X4C ||
		chg_type == ADAPTER_ID_80W_0X4D || chg_type == ADAPTER_ID_80W_0X4E ||
		chg_type == ADAPTER_ID_80W_0X65) && chip->usbtemp_change_gap) {/*80W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH;
	} else if ((chg_type == ADAPTER_ID_100W_0X69 || chg_type == ADAPTER_ID_100W_0X6A ||
		chg_type == ADAPTER_ID_120W_0X32 || chg_type == ADAPTER_ID_120W_0X6B) &&
		chip->usbtemp_change_gap) {/*100W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH;
	} else {
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH;
	}
}

#define USBTEMP_BATTTEMP_GAP_HIGH_100W 24
#define USBTEMP_BATTTEMP_CURRENT_GAP_HIGH_100W 20
#define USBTEMP_MAX_TEMP_THR_HIGH_100W 69
#define USBTEMP_MAX_TEMP_DIFF_HIGH_100W 12
#define USBTEMP_BATTTEMP_RECOVER_GAP_HIGH_100W 15
#define USBTEMP_BATTTEMP_CURRENT_THR_HIGH_100W 65
#define USBTEMP_BATTTEMP_RECOVER_THR_HIGH_100W 60

#define USBTEMP_BATTTEMP_CURRENT_THR_DEFAULT_100W 54
#define USBTEMP_BATTTEMP_RECOVER_THR_DEFAULT_100W 48
int usbtemp_batttemp_current_thr;
int usbtemp_batttemp_recover_thr;

static void oplus_usbtemp_recover_tbatt_temp_100w(struct oplus_chg_chip *chip) {
	int chg_type = 0;
	if (oplus_pps_get_support_type() == PPS_SUPPORT_2CP ||
	    oplus_pps_get_support_type() == PPS_SUPPORT_3CP ||
	    chip->vooc_project == DUAL_BATT_100W) {
		g_tbatt_temp = oplus_gauge_get_batt_temperature();
	} else {
		g_tbatt_temp = chip->tbatt_temp;
	}
	chg_type = oplus_vooc_get_fast_chg_type();
	/*chg_err("g_tbatt_temp:%d, tbatt_temp:%d, chg_type:%d\n", g_tbatt_temp, chip->tbatt_temp, chg_type);*/

	if ((chg_type == ADAPTER_ID_20W_0X13 || chg_type == ADAPTER_ID_20W_0X34 ||
		chg_type == ADAPTER_ID_20W_0X45) && chip->usbtemp_change_gap) {/*20W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_DEFAULT;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_DEFAULT;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_DEFAULT;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_DEFAULT;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_DEFAULT;
		usbtemp_batttemp_current_thr = USBTEMP_BATTTEMP_CURRENT_THR_DEFAULT_100W;
		usbtemp_batttemp_recover_thr = USBTEMP_BATTTEMP_RECOVER_THR_DEFAULT_100W;
	} else if ((chg_type == ADAPTER_ID_30W_0X19 || chg_type == ADAPTER_ID_30W_0X29 ||
		chg_type == ADAPTER_ID_30W_0X41 || chg_type == ADAPTER_ID_30W_0X42 ||
		chg_type == ADAPTER_ID_30W_0X43 || chg_type == ADAPTER_ID_30W_0X44 ||
		chg_type == ADAPTER_ID_30W_0X46) && chip->usbtemp_change_gap) {/*30W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_DEFAULT;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_DEFAULT;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_DEFAULT;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_DEFAULT;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_DEFAULT;
		usbtemp_batttemp_current_thr = USBTEMP_BATTTEMP_CURRENT_THR_DEFAULT_100W;
		usbtemp_batttemp_recover_thr = USBTEMP_BATTTEMP_RECOVER_THR_DEFAULT_100W;
	} else if ((chg_type == ADAPTER_ID_33W_0X49 || chg_type == ADAPTER_ID_33W_0X4A ||
		chg_type == ADAPTER_ID_33W_0X61) && chip->usbtemp_change_gap) {/*33W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_DEFAULT;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_DEFAULT;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_DEFAULT;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_DEFAULT;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_DEFAULT;
		usbtemp_batttemp_current_thr = USBTEMP_BATTTEMP_CURRENT_THR_DEFAULT_100W;
		usbtemp_batttemp_recover_thr = USBTEMP_BATTTEMP_RECOVER_THR_DEFAULT_100W;
	} else if ((chg_type == ADAPTER_ID_50W_0X11 || chg_type == ADAPTER_ID_50W_0X12 ||
		chg_type == ADAPTER_ID_50W_0X21 || chg_type == ADAPTER_ID_50W_0X31 ||
		chg_type == ADAPTER_ID_50W_0X33 || chg_type == ADAPTER_ID_50W_0X62) &&
		chip->usbtemp_change_gap) {/*50W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_DEFAULT;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_DEFAULT;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_DEFAULT;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_DEFAULT;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_DEFAULT;
		usbtemp_batttemp_current_thr = USBTEMP_BATTTEMP_CURRENT_THR_DEFAULT_100W;
		usbtemp_batttemp_recover_thr = USBTEMP_BATTTEMP_RECOVER_THR_DEFAULT_100W;
	} else if ((chg_type == ADAPTER_ID_65W_0X14 || chg_type == ADAPTER_ID_65W_0X35 ||
		chg_type == ADAPTER_ID_65W_0X63 || chg_type == ADAPTER_ID_65W_0X66 ||
		chg_type == ADAPTER_ID_65W_0X6E) && chip->usbtemp_change_gap) {/*65W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_DEFAULT;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_DEFAULT;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_DEFAULT;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_DEFAULT;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_DEFAULT;
		usbtemp_batttemp_current_thr = USBTEMP_BATTTEMP_CURRENT_THR_DEFAULT_100W;
		usbtemp_batttemp_recover_thr = USBTEMP_BATTTEMP_RECOVER_THR_DEFAULT_100W;
	} else if ((chg_type == ADAPTER_ID_66W_0X36 || chg_type == ADAPTER_ID_66W_0X64) &&
		chip->usbtemp_change_gap) {/*66W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_DEFAULT;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_DEFAULT;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_DEFAULT;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_DEFAULT;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_DEFAULT;
		usbtemp_batttemp_current_thr = USBTEMP_BATTTEMP_CURRENT_THR_DEFAULT_100W;
		usbtemp_batttemp_recover_thr = USBTEMP_BATTTEMP_RECOVER_THR_DEFAULT_100W;
	} else if ((chg_type == ADAPTER_ID_67W_0X6C || chg_type == ADAPTER_ID_67W_0X6D) &&
		chip->usbtemp_change_gap) {/*67W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_DEFAULT;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_DEFAULT;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_DEFAULT;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_DEFAULT;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_DEFAULT;
		usbtemp_batttemp_current_thr = USBTEMP_BATTTEMP_CURRENT_THR_DEFAULT_100W;
		usbtemp_batttemp_recover_thr = USBTEMP_BATTTEMP_RECOVER_THR_DEFAULT_100W;
	} else if ((chg_type == ADAPTER_ID_80W_0X4B || chg_type == ADAPTER_ID_80W_0X4C ||
		chg_type == ADAPTER_ID_80W_0X4D || chg_type == ADAPTER_ID_80W_0X4E ||
		chg_type == ADAPTER_ID_80W_0X65) && chip->usbtemp_change_gap) {/*80W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH_100W;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH_100W;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH_100W;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH_100W;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH_100W;
		usbtemp_batttemp_current_thr = USBTEMP_BATTTEMP_CURRENT_THR_HIGH_100W;
		usbtemp_batttemp_recover_thr = USBTEMP_BATTTEMP_RECOVER_THR_HIGH_100W;
	} else if ((chg_type == ADAPTER_ID_100W_0X69 || chg_type == ADAPTER_ID_100W_0X6A ||
		chg_type == ADAPTER_ID_120W_0X32 || chg_type == ADAPTER_ID_120W_0X6B) &&
		chip->usbtemp_change_gap) {/*100W*/
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH_100W;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH_100W;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH_100W;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH_100W;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH_100W;
		usbtemp_batttemp_current_thr = USBTEMP_BATTTEMP_CURRENT_THR_HIGH_100W;
		usbtemp_batttemp_recover_thr = USBTEMP_BATTTEMP_RECOVER_THR_HIGH_100W;
	} else {
		chip->usbtemp_batttemp_gap = USBTEMP_BATTTEMP_GAP_HIGH_100W;
		chip->usbtemp_batttemp_current_gap = USBTEMP_BATTTEMP_CURRENT_GAP_HIGH_100W;
		chip->usbtemp_max_temp_thr = USBTEMP_MAX_TEMP_THR_HIGH_100W;
		chip->usbtemp_max_temp_diff = USBTEMP_MAX_TEMP_DIFF_HIGH_100W;
		chip->usbtemp_batttemp_recover_gap = USBTEMP_BATTTEMP_RECOVER_GAP_HIGH_100W;
		usbtemp_batttemp_current_thr = USBTEMP_BATTTEMP_CURRENT_THR_HIGH_100W;
		usbtemp_batttemp_recover_thr = USBTEMP_BATTTEMP_RECOVER_THR_HIGH_100W;
	}
}

#define CID_STATUS_DELAY_MS 55
static void oplus_typec_state_change_work(struct work_struct *work)
{
	int level = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	int cid_status = 0;
	struct psy_state *pst = NULL;
	int rc = 0;

	if (chip == NULL) {
		pr_err("[OPLUS_CCDETECT] chip null, return\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;

	if(oplus_ccdetect_check_is_gpio(chip) == true) {
		level = gpio_get_value(bcdev->oplus_custom_gpio.ccdetect_gpio);
		chg_err("%s: !!! level[%d]\n", __func__, level);
		if (level == 1 && oplus_get_otg_switch_status() == false)
			oplus_ccdetect_disable();
	} else {
		msleep(CID_STATUS_DELAY_MS);
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		if(pst == NULL) {
			chg_err("cid status is NULL\n");
			return;
		}
		rc = read_property_id(bcdev, pst, USB_CID_STATUS);
		if(rc) {
			chg_err("read cid_status fail, rc=%d\n", rc);
			return;
		} else {
			cid_status = pst->prop[USB_CID_STATUS];
		}

		if(cid_status == 0)
			oplus_chg_clear_abnormal_adapter_var();
		chg_err("!!!cid_status[%d]\n", cid_status);
	}
	/*oplus_pps_stop();*/
}

static void oplus_ccdetect_work(struct work_struct *work)
{
	int level = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (chip == NULL) {
		pr_err("[OPLUS_CCDETECT] chip null, return\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	level = gpio_get_value(bcdev->oplus_custom_gpio.ccdetect_gpio);

	chg_err("%s: !!!level[%d]\n", __func__, level);
	if (level != 1) {
		oplus_ccdetect_enable();
		if (g_oplus_chip->usb_status == USB_TEMP_HIGH) {
			schedule_delayed_work(&bcdev->usbtemp_recover_work, 0);
		}
	} else {
		chip->usbtemp_check = false;
		if(g_oplus_chip->usb_status == USB_TEMP_HIGH) {
			schedule_delayed_work(&bcdev->usbtemp_recover_work, 0);
		}
		if (oplus_get_otg_switch_status() == false) {
			oplus_ccdetect_disable();
		}
	}
	oplus_ccdetect_happened_to_adsp();
}

static void oplus_cid_status_change_work(struct work_struct *work)
{
	int rc = 0;
	int cid_status = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;

	if (chip == NULL) {
		pr_err("[OPLUS_CCDETECT] chip null, return\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_CID_STATUS);
	if (rc < 0) {
		chg_err("!!!%s, read cid_status fail\n", __func__);
		return;
	}

	cid_status = pst->prop[USB_CID_STATUS];
	bcdev->cid_status = cid_status;
	chg_err("%s: !!!cid_status[%d]\n", __func__, cid_status);

	if (cid_status == 0) {
		chip->usbtemp_check = false;
		oplus_chg_clear_abnormal_adapter_var();
	}

	if (g_oplus_chip->usb_status == USB_TEMP_HIGH) {
		schedule_delayed_work(&bcdev->usbtemp_recover_work, 0);
	}
}

static void oplus_chg_plugin_cnt_check_work(struct work_struct *work)
{
	int rc = -1;
	u32 plugin_cnt = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	rc = oplus_usb_psy_get_plugin_cnt(&plugin_cnt);
	if (rc < 0) {
		chg_err("get plugin cnt is fail\n");
		goto out;
	}

	if (plugin_cnt_test) {
		plugin_cnt = plugin_cnt_test;
		chg_err("plugin_cnt_test adsp cnt:[%d]\n", plugin_cnt_test);
		plugin_cnt_test = 0;
	}
	if (plugin_cnt != bcdev->adspvoocphy_plugin_cnt) {
		if (bcdev->adspvoocphy_plugin_cnt < plugin_cnt) {
			pr_info("kernel ap cnt < adsp update plugin cnt, [%d < %d]\n",
				 bcdev->adspvoocphy_plugin_cnt, plugin_cnt);
			bcdev->adspvoocphy_plugin_cnt = plugin_cnt - 1;
			oplus_usb_psy_get_plugin_to_align_plugin_irq();
		} else if (bcdev->adspvoocphy_plugin_cnt > plugin_cnt) {
			pr_info("kernel ap cnt > adsp update plugin cnt, count err!!! [%d > %d]\n",
				 bcdev->adspvoocphy_plugin_cnt, plugin_cnt);
			bcdev->adspvoocphy_plugin_cnt = plugin_cnt;
		}
	}
out:
	schedule_delayed_work(&bcdev->adspvoocphy_plugin_cnt_check_work,
			      OPLUS_PLUGIN_CNT_DETECT_INTERVAL);
}

static int oplus_usbtemp_dischg_action(struct oplus_chg_chip *chip)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;

	if (chip == NULL) {
		pr_err("[OPLUS_CCDETECT] chip null, return\n");
		return rc;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	if ((get_eng_version() != HIGH_TEMP_AGING)
			&& (get_eng_version() != PTCRB)) {
#endif
		oplus_set_usb_status(USB_TEMP_HIGH);

		if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) {
			rc = write_property_id(bcdev, pst, USB_VOOCPHY_ENABLE, false);
			if (rc < 0) {
				printk(KERN_ERR "!!![OPLUS_USBTEMP] write utemp high action fail\n");
				return rc;
			}
		} else {
			if (oplus_vooc_get_fastchg_started() == true) {
				oplus_vooc_turn_off_fastchg();
				if (oplus_pps_get_support_type() == PPS_SUPPORT_2CP ||
				    oplus_pps_get_support_type() == PPS_SUPPORT_3CP)
					oplus_pps_set_pps_mos_enable(false);
			}
		}
		if (oplus_is_pps_charging()) {
			chg_err("oplus_pps_stop_usb_temp\n");
			oplus_pps_stop_usb_temp();
		}
		if (oplus_is_ufcs_charging()) {
			chg_err("oplus_ufcs_stop_usb_temp\n");
			oplus_ufcs_stop_usb_temp();
		}
		usleep_range(10000, 10000);
		chip->chg_ops->charger_suspend();
		usleep_range(10000, 10000);

		rc = write_property_id(bcdev, pst, USB_TYPEC_MODE, TYPEC_PORT_ROLE_DISABLE);
		if (rc < 0) {
			chg_err("!!![OPLUS_USBTEMP] write usb typec sinkonly fail\n");
			return rc;
		}
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	}
#endif

	mutex_lock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	if ((get_eng_version() == HIGH_TEMP_AGING) || (get_eng_version() == PTCRB)) {
		chg_err(" CONFIG_HIGH_TEMP_VERSION enable here,do not set vbus down \n");
		rc = pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.dischg_disable);
	} else {
#endif
		pr_err("[oplus_usbtemp_dischg_action]: set vbus down");
		rc = pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.dischg_enable);
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	}
#endif
	mutex_unlock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));

	return 0;
}

#define RETRY_COUNT		3
static void oplus_update_usbtemp_current_status(struct oplus_chg_chip *chip)
{
	static int limit_cur_cnt_r = 0;
	static int limit_cur_cnt_l = 0;
	static int recover_cur_cnt_r = 0;
	static int recover_cur_cnt_l = 0;

	if (!chip) {
		return;
	}

	if((chip->usb_temp_l < USB_30C || chip->usb_temp_l > USB_100C)
		&& (chip->usb_temp_r < USB_30C || chip->usb_temp_r > USB_100C)) {
		chip->smart_charge_user = SMART_CHARGE_USER_OTHER;
		chip->usbtemp_cool_down = 0;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
		return;
	}

	if ((chip->usb_temp_r  - g_tbatt_temp/10) >= chip->usbtemp_batttemp_current_gap) {
		limit_cur_cnt_r++;
		if (limit_cur_cnt_r >= RETRY_COUNT) {
			limit_cur_cnt_r = RETRY_COUNT;
		}
		recover_cur_cnt_r = 0;
	} else if ((chip->usb_temp_r  - g_tbatt_temp/10) <= chip->usbtemp_batttemp_recover_gap)  {
		recover_cur_cnt_r++;
		if (recover_cur_cnt_r >= RETRY_COUNT) {
			recover_cur_cnt_r = RETRY_COUNT;
		}
		limit_cur_cnt_r = 0;
	}

	if ((chip->usb_temp_l  - g_tbatt_temp/10) >= chip->usbtemp_batttemp_current_gap) {
		limit_cur_cnt_l++;
		if (limit_cur_cnt_l >= RETRY_COUNT) {
			limit_cur_cnt_l = RETRY_COUNT;
		}
		recover_cur_cnt_l = 0;
	} else if ((chip->usb_temp_l  - g_tbatt_temp/10) <= chip->usbtemp_batttemp_recover_gap)  {
		recover_cur_cnt_l++;
		if (recover_cur_cnt_l >= RETRY_COUNT) {
			recover_cur_cnt_l = RETRY_COUNT;
		}
		limit_cur_cnt_l = 0;
	}

	if ((RETRY_COUNT <= limit_cur_cnt_r || RETRY_COUNT <= limit_cur_cnt_l)
			&& (chip->smart_charge_user == SMART_CHARGE_USER_OTHER)) {
		chg_err("use usbtemp cooldown g_tbatt_temp:%d, usb_temp_l:%d, usb_temp_r:%d, usbtemp_batttemp_current_gap:%d\n",
			g_tbatt_temp, chip->usb_temp_l, chip->usb_temp_r, chip->usbtemp_batttemp_current_gap);
		chip->smart_charge_user = SMART_CHARGE_USER_USBTEMP;
		chip->cool_down_done = true;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
	} else if ((RETRY_COUNT <= recover_cur_cnt_r &&  RETRY_COUNT <= recover_cur_cnt_l)
			&& (chip->smart_charge_user == SMART_CHARGE_USER_USBTEMP)) {
		chip->smart_charge_user = SMART_CHARGE_USER_OTHER;
		chip->usbtemp_cool_down = 0;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
	}

	return;
}
static void oplus_update_usbtemp_current_status_100w(struct oplus_chg_chip *chip)
{
	static int limit_cur_cnt_r = 0;
	static int limit_cur_cnt_l = 0;
	static int recover_cur_cnt_r = 0;
	static int recover_cur_cnt_l = 0;

	if (!chip) {
		return;
	}

	if((chip->usb_temp_l < USB_30C || chip->usb_temp_l > USB_100C)
		&& (chip->usb_temp_r < USB_30C || chip->usb_temp_r > USB_100C)) {
		chip->smart_charge_user = SMART_CHARGE_USER_OTHER;
		chip->usbtemp_cool_down = 0;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
		return;
	}

	if (((chip->usb_temp_r  - g_tbatt_temp/10) >= chip->usbtemp_batttemp_current_gap) || (chip->usb_temp_r >= usbtemp_batttemp_current_thr)) {
		limit_cur_cnt_r++;
		if (limit_cur_cnt_r >= RETRY_COUNT) {
			limit_cur_cnt_r = RETRY_COUNT;
		}
		recover_cur_cnt_r = 0;
	} else if (((chip->usb_temp_r  - g_tbatt_temp/10) <= chip->usbtemp_batttemp_recover_gap) && (chip->usb_temp_r <= usbtemp_batttemp_recover_thr))  {
		recover_cur_cnt_r++;
		if (recover_cur_cnt_r >= RETRY_COUNT) {
			recover_cur_cnt_r = RETRY_COUNT;
		}
		limit_cur_cnt_r = 0;
	}

	if (((chip->usb_temp_l  - g_tbatt_temp/10) >= chip->usbtemp_batttemp_current_gap) || (chip->usb_temp_l >= usbtemp_batttemp_current_thr)) {
		limit_cur_cnt_l++;
		if (limit_cur_cnt_l >= RETRY_COUNT) {
			limit_cur_cnt_l = RETRY_COUNT;
		}
		recover_cur_cnt_l = 0;
	} else if (((chip->usb_temp_l  - g_tbatt_temp/10) <= chip->usbtemp_batttemp_recover_gap) && (chip->usb_temp_l <= usbtemp_batttemp_recover_thr)) {
		recover_cur_cnt_l++;
		if (recover_cur_cnt_l >= RETRY_COUNT) {
			recover_cur_cnt_l = RETRY_COUNT;
		}
		limit_cur_cnt_l = 0;
	}

	if ((RETRY_COUNT <= limit_cur_cnt_r || RETRY_COUNT <= limit_cur_cnt_l)
			&& (chip->smart_charge_user == SMART_CHARGE_USER_OTHER)) {
		chg_err("use usbtemp cooldown g_tbatt_temp:%d, usb_temp_l:%d, usb_temp_r:%d, usbtemp_batttemp_current_gap:%d\n",
			g_tbatt_temp, chip->usb_temp_l, chip->usb_temp_r, chip->usbtemp_batttemp_current_gap);
		chip->smart_charge_user = SMART_CHARGE_USER_USBTEMP;
		chip->cool_down_done = true;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
	} else if ((RETRY_COUNT <= recover_cur_cnt_r &&  RETRY_COUNT <= recover_cur_cnt_l)
			&& (chip->smart_charge_user == SMART_CHARGE_USER_USBTEMP)) {
		chip->smart_charge_user = SMART_CHARGE_USER_OTHER;
		chip->usbtemp_cool_down = 0;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
	}

	return;
}
#define BATT_VOL_DEFAULT_MV	5000
static int get_battery_mvolts_for_usbtemp_monitor(struct oplus_chg_chip *chip)
{
	if (!is_ext_chg_ops())
		return BATT_VOL_DEFAULT_MV;

	if (chip->chg_ops && chip->chg_ops->get_charger_volt)
		return chip->chg_ops->get_charger_volt();

	return BATT_VOL_DEFAULT_MV;
}

static int oplus_usbtemp_monitor_main(void *data)
{
	int delay = 0;
	int vbus_volt = 0;
	static int count = 0;
	static int total_count = 0;
	static int last_usb_temp_l = 25;
	static int current_temp_l = 25;
	static int last_usb_temp_r = 25;
	static int current_temp_r = 25;
	int retry_cnt = 3, i = 0;
	int count_r = 1, count_l = 1;
	bool condition1 = false;
	bool condition2 = false;
	int condition;
	int batt_current = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	static int log_count = 0;

	pr_err("[oplus_usbtemp_monitor_main]:run first!");

	while (!kthread_should_stop()) {
		wait_event_interruptible(chip->oplus_usbtemp_wq, chip->usbtemp_check == true);
		if(chip->dischg_flag == true) {
			goto dischg;
		}
		oplus_get_usbtemp_volt(chip);
		get_usb_temp(chip);
		if ((chip->usb_temp_l < USB_50C) && (chip->usb_temp_r < USB_50C)) {/*get vbus when usbtemp < 50C*/
			vbus_volt = get_battery_mvolts_for_usbtemp_monitor(chip);
		} else {
			vbus_volt = 0;
		}
		if ((chip->usb_temp_l < USB_40C) && (chip->usb_temp_r < USB_40C)) {
			delay = MAX_MONITOR_INTERVAL;
			total_count = 10;
		} else {
			delay = MIN_MONITOR_INTERVAL;
			total_count = 30;
		}

		if(chip->pmic_spmi.bcdev_chip->usbtemp_thread_100w_support) {
			oplus_usbtemp_recover_tbatt_temp_100w(chip);
			oplus_update_usbtemp_current_status_100w(chip);
		} else {
			oplus_usbtemp_recover_tbatt_temp(chip);
			oplus_update_usbtemp_current_status(chip);
		}
		if ((chip->usbtemp_volt_l < USB_50C) && (chip->usbtemp_volt_r < USB_50C) && (vbus_volt < VBUS_VOLT_THRESHOLD))
			delay = VBUS_MONITOR_INTERVAL;

		/*condition1  :the temp is higher than 57*/
		if (g_tbatt_temp/10 <= USB_50C &&(((chip->usb_temp_l >= chip->usbtemp_max_temp_thr) && (chip->usb_temp_l < USB_100C))
			|| ((chip->usb_temp_r >= chip->usbtemp_max_temp_thr) && (chip->usb_temp_r < USB_100C)))) {
			pr_err("in loop 1");
			for (i = 1; i < retry_cnt; i++) {
				mdelay(RETRY_CNT_DELAY);
				oplus_get_usbtemp_volt(chip);
				get_usb_temp(chip);
				if (chip->usb_temp_r >= chip->usbtemp_max_temp_thr && chip->usb_temp_r < USB_100C)
					count_r++;
				if (chip->usb_temp_l >= chip->usbtemp_max_temp_thr && chip->usb_temp_l < USB_100C)
					count_l++;
				pr_err("countl : %d", count_l);
			}
			if (count_r >= retry_cnt || count_l >= retry_cnt) {
				if (!IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_enable)) {
					chip->dischg_flag = true;
					condition1 = true;
					chg_err("dischg enable1...[%d, %d, %d]\n", chip->usb_temp_l, chip->usb_temp_r, g_tbatt_temp);
				}
			}
			count_r = 1;
			count_l = 1;
			count = 0;
			last_usb_temp_r = chip->usb_temp_r;
			last_usb_temp_l = chip->usb_temp_l;
		}
		if (g_tbatt_temp/10 > USB_50C && (((chip->usb_temp_l >= g_tbatt_temp/10 + chip->usbtemp_max_temp_diff) && (chip->usb_temp_l < USB_100C))
			|| ((chip->usb_temp_r >= g_tbatt_temp/10 + chip->usbtemp_max_temp_diff) && (chip->usb_temp_r < USB_100C)))) {
			pr_err("in loop 1");
			for (i = 1; i <= retry_cnt; i++) {
				mdelay(RETRY_CNT_DELAY);
				oplus_get_usbtemp_volt(chip);
				get_usb_temp(chip);
				if ((chip->usb_temp_r >= g_tbatt_temp/10 + chip->usbtemp_max_temp_diff) && chip->usb_temp_r < USB_100C)
					count_r++;
				if ((chip->usb_temp_l >= g_tbatt_temp/10 + chip->usbtemp_max_temp_diff) && chip->usb_temp_l < USB_100C)
					count_l++;
				pr_err("countl : %d", count_l);
			}
			if (count_r >= retry_cnt || count_l >= retry_cnt) {
				if (!IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_enable)) {
					chip->dischg_flag = true;
					condition1 = true;
					chg_err("dischg enable2...[%d, %d, %d]\n", chip->usb_temp_l, chip->usb_temp_r, g_tbatt_temp);
				}
			}
			count_r = 1;
			count_l = 1;
			count = 0;
			last_usb_temp_r = chip->usb_temp_r;
			last_usb_temp_l = chip->usb_temp_l;
		}
		if(condition1 == true) {
			pr_err("jump_to_dischg");
			goto dischg;
		}

		/*condition2  :the temp uprising to fast*/
		if ((((chip->usb_temp_l - g_tbatt_temp/10) > chip->usbtemp_batttemp_gap) && (chip->usb_temp_l < USB_100C))
				|| (((chip->usb_temp_r - g_tbatt_temp/10) > chip->usbtemp_batttemp_gap) && (chip->usb_temp_r < USB_100C))) {
			if (count == 0) {
				last_usb_temp_r = chip->usb_temp_r;
				last_usb_temp_l = chip->usb_temp_l;
			} else {
				current_temp_r = chip->usb_temp_r;
				current_temp_l = chip->usb_temp_l;
			}
			if (((current_temp_l - last_usb_temp_l) >= 3) || (current_temp_r - last_usb_temp_r) >= 3) {
				for (i = 1; i <= retry_cnt; i++) {
					mdelay(RETRY_CNT_DELAY);
					oplus_get_usbtemp_volt(chip);
					get_usb_temp(chip);
					if ((chip->usb_temp_r - last_usb_temp_r) >= 3 && chip->usb_temp_r < USB_100C)
						count_r++;
					if ((chip->usb_temp_l - last_usb_temp_l) >= 3 && chip->usb_temp_l < USB_100C)
						count_l++;
					pr_err("countl : %d,countr : %d", count_l, count_r);
				}
				current_temp_l = chip->usb_temp_l;
				current_temp_r = chip->usb_temp_r;
				if ((count_l >= retry_cnt &&  chip->usb_temp_l > USB_30C && chip->usb_temp_l < USB_100C)
						|| (count_r >= retry_cnt &&  chip->usb_temp_r > USB_30C  && chip->usb_temp_r < USB_100C))  {
					if (!IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_enable)) {
						chip->dischg_flag = true;
						chg_err("dischg enable3...,current_temp_l=%d,last_usb_temp_l=%d,current_temp_r=%d,last_usb_temp_r =%d, g_tbatt_temp = %d\n",
								current_temp_l, last_usb_temp_l, current_temp_r, last_usb_temp_r, g_tbatt_temp);
						condition2 = true;
					}
				}
				count_r = 1;
				count_l = 1;
			}
			count++;
			if (count > total_count)
				count = 0;
		} else {
			count = 0;
			last_usb_temp_r = chip->usb_temp_r;
			last_usb_temp_l = chip->usb_temp_l;
		}
	/*judge whether to go the action*/
	dischg:
		if ((chip->usb_temp_l < USB_30C || chip->usb_temp_l > USB_100C)
				&& (chip->usb_temp_r < USB_30C || chip->usb_temp_r > USB_100C)) {
			condition1 = false;
			condition2 = false;
			chip->dischg_flag = false;
		}
		if((condition1== true || condition2 == true) && chip->dischg_flag == true) {
			condition = (condition1== true ?
				USBTEMP_TRIGGER_CONDITION_1 :
				USBTEMP_TRIGGER_CONDITION_2);
			oplus_chg_track_upload_usbtemp_info(chip,
				condition, last_usb_temp_l, last_usb_temp_r,
				batt_current);
			oplus_usbtemp_dischg_action(chip);
			condition1 = false;
			condition2 = false;
		} else if (chip->debug_force_usbtemp_trigger) {
			oplus_chg_track_upload_usbtemp_info(
				chip, chip->debug_force_usbtemp_trigger,
				last_usb_temp_l, last_usb_temp_r, batt_current);
			chip->debug_force_usbtemp_trigger = 0;
		}
		msleep(delay);
		log_count++;
		if (log_count == 40) {
			chg_err("==================usbtemp_volt_l[%d], usb_temp_l[%d], usbtemp_volt_r[%d], usb_temp_r[%d]\n",
					chip->usbtemp_volt_l, chip->usb_temp_l, chip->usbtemp_volt_r, chip->usb_temp_r);
			log_count = 0;
		}
	}

	return 0;
}

bool oplus_usbtemp_l_trigger_current_status(void)
{
	if (!g_oplus_chip)
		return false;

	if (g_oplus_chip->usb_temp_l < USB_30C || g_oplus_chip->usb_temp_l > USB_100C) {
		return false;
	}

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if ((g_oplus_chip->usb_temp_l >= g_oplus_chip->usbtemp_cool_down_ntc_low) ||
			(g_oplus_chip->usb_temp_l - g_tbatt_temp / 10) >=
				g_oplus_chip->usbtemp_cool_down_gap_low)
			return true;
		return false;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if ((g_oplus_chip->usb_temp_l >= g_oplus_chip->usbtemp_cool_down_ntc_high) ||
			(g_oplus_chip->usb_temp_l - g_tbatt_temp / 10) >=
				g_oplus_chip->usbtemp_cool_down_gap_high)
			return true;
		return false;
	} else {
		return false;
	}
}

bool oplus_usbtemp_l_recovery_current_status(void)
{
	if (!g_oplus_chip)
		return false;

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if ((g_oplus_chip->usb_temp_l <= g_oplus_chip->usbtemp_cool_down_recover_ntc_low) &&
			(g_oplus_chip->usb_temp_l - g_tbatt_temp / 10) <=
				g_oplus_chip->usbtemp_cool_down_recover_gap_low)
			return true;
		return false;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if ((g_oplus_chip->usb_temp_l <= g_oplus_chip->usbtemp_cool_down_recover_ntc_high) &&
			(g_oplus_chip->usb_temp_l - g_tbatt_temp / 10) <=
				g_oplus_chip->usbtemp_cool_down_recover_gap_high)
			return true;
		return false;
	} else {
		return false;
	}
}

bool oplus_usbtemp_r_trigger_current_status(void)
{
	if (!g_oplus_chip)
		return false;

	if (g_oplus_chip->usb_temp_r < USB_30C || g_oplus_chip->usb_temp_r > USB_100C) {
		return false;
	}

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if ((g_oplus_chip->usb_temp_r >= g_oplus_chip->usbtemp_cool_down_ntc_low) ||
			(g_oplus_chip->usb_temp_r - g_tbatt_temp / 10) >=
				g_oplus_chip->usbtemp_cool_down_gap_low)
			return true;
		return false;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if ((g_oplus_chip->usb_temp_r >= g_oplus_chip->usbtemp_cool_down_ntc_high) ||
			(g_oplus_chip->usb_temp_r - g_tbatt_temp / 10) >=
				g_oplus_chip->usbtemp_cool_down_gap_high)
			return true;
		return false;
	} else {
		return false;
	}
}

bool oplus_usbtemp_r_recovery_current_status(void)
{
	if (!g_oplus_chip)
		return false;

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if ((g_oplus_chip->usb_temp_r <= g_oplus_chip->usbtemp_cool_down_recover_ntc_low) &&
			(g_oplus_chip->usb_temp_r - g_tbatt_temp / 10) <=
				g_oplus_chip->usbtemp_cool_down_recover_gap_low)
			return true;
		return false;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if ((g_oplus_chip->usb_temp_r <= g_oplus_chip->usbtemp_cool_down_recover_ntc_high) &&
			(g_oplus_chip->usb_temp_r - g_tbatt_temp / 10) <=
				g_oplus_chip->usbtemp_cool_down_recover_gap_high)
			return true;
		return false;
	} else {
		return false;
	}
}

#define RETRY_COUNT				3
#define RETRY_COUNT_UFCS		3
static void oplus_update_usbtemp_current_status_new_method(struct oplus_chg_chip *chip)
{
	static int limit_cur_cnt_r = 0;
	static int limit_cur_cnt_l = 0;
	static int recover_cur_cnt_r = 0;
	static int recover_cur_cnt_l = 0;
	int condition, batt_current;
	int last_usb_temp_l = 25;
	int last_usb_temp_r = 25;

	if (!chip) {
		return;
	}

	batt_current = chip->usbtemp_batt_current;

	if((chip->usb_temp_l < USB_30C || chip->usb_temp_l > USB_100C)
		&& (chip->usb_temp_r < USB_30C || chip->usb_temp_r > USB_100C)) {
		chip->smart_charge_user = SMART_CHARGE_USER_OTHER;
		chip->usbtemp_cool_down = 0;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
		return;
	}

	if (oplus_usbtemp_r_trigger_current_status()) {
		limit_cur_cnt_r++;
		if (limit_cur_cnt_r >= RETRY_COUNT) {
			limit_cur_cnt_r = RETRY_COUNT;
		}
		recover_cur_cnt_r = 0;
	} else if (oplus_usbtemp_r_recovery_current_status())  {
		recover_cur_cnt_r++;
		if (recover_cur_cnt_r >= RETRY_COUNT) {
			recover_cur_cnt_r = RETRY_COUNT;
		}
		limit_cur_cnt_r = 0;
	}

	if (oplus_usbtemp_l_trigger_current_status()) {
		limit_cur_cnt_l++;
		if (limit_cur_cnt_l >= RETRY_COUNT) {
			limit_cur_cnt_l = RETRY_COUNT;
		}
		recover_cur_cnt_l = 0;
	} else if (oplus_usbtemp_l_recovery_current_status())  {
		recover_cur_cnt_l++;
		if (recover_cur_cnt_l >= RETRY_COUNT) {
			recover_cur_cnt_l = RETRY_COUNT;
		}
		limit_cur_cnt_l = 0;
	}

	if ((RETRY_COUNT <= limit_cur_cnt_r || RETRY_COUNT <= limit_cur_cnt_l)
			&& (chip->smart_charge_user == SMART_CHARGE_USER_OTHER)) {
		chg_err("use usbtemp cooldown g_tbatt_temp:%d, usb_temp_l:%d, usb_temp_r:%d, usbtemp_batttemp_current_gap:%d\n",
			g_tbatt_temp, chip->usb_temp_l, chip->usb_temp_r, chip->usbtemp_batttemp_current_gap);
		chip->smart_charge_user = SMART_CHARGE_USER_USBTEMP;
		chip->cool_down_done = true;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
		condition = USBTEMP_TRIGGER_CONDITION_COOL_DOWN;
		oplus_chg_track_upload_usbtemp_info(chip,
				condition, last_usb_temp_l, last_usb_temp_r, batt_current);
	} else if ((RETRY_COUNT <= recover_cur_cnt_r &&  RETRY_COUNT <= recover_cur_cnt_l)
			&& (chip->smart_charge_user == SMART_CHARGE_USER_USBTEMP)) {
		chip->smart_charge_user = SMART_CHARGE_USER_OTHER;
		chip->usbtemp_cool_down = 0;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
		condition = USBTEMP_TRIGGER_CONDITION_COOL_DOWN_RECOVERY;
		oplus_chg_track_upload_usbtemp_info(chip,
				condition, last_usb_temp_l, last_usb_temp_r, batt_current);
	}

	return;
}

int oplus_usbtemp_l_trigger_current_status_ufcs(void)
{
	if (!g_oplus_chip)
		return SMART_CHARGE_USER_OTHER;

	if (g_oplus_chip->usb_temp_l < USB_30C || g_oplus_chip->usb_temp_l > USB_100C) {
		return SMART_CHARGE_USER_OTHER;
	}

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if ((g_oplus_chip->usb_temp_l >= g_oplus_chip->usbtemp_cool_down_ntc_low) ||
			(g_oplus_chip->usb_temp_l - g_tbatt_temp / 10) >=
				g_oplus_chip->usbtemp_cool_down_gap_low)
			return SMART_CHARGE_USER_USBTEMP;
		return SMART_CHARGE_USER_OTHER;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if ((g_oplus_chip->usb_temp_l - g_tbatt_temp / 10) >= g_oplus_chip->usbtemp_cool_down_gap_high) {
			return SMART_CHARGE_USER_USBTEMP;
		} else if ((g_oplus_chip->usb_temp_l >= g_oplus_chip->usbtemp_cool_down_ntc_high)) {
			return SMART_CHARGE_USER_USBTEMP;
		} else if ((g_oplus_chip->usb_temp_l >= g_oplus_chip->usbtemp_cool_down_ntc_high1)) {
			return SMART_CHARGE_USER_USBTEMP1;
		} else if ((g_oplus_chip->usb_temp_l >= g_oplus_chip->usbtemp_cool_down_ntc_high2)) {
			return SMART_CHARGE_USER_USBTEMP2;
		} else if ((g_oplus_chip->usb_temp_l >= g_oplus_chip->usbtemp_cool_down_ntc_high3)) {
			return SMART_CHARGE_USER_USBTEMP3;
		} else {
			return SMART_CHARGE_USER_OTHER;
		}
	} else {
		return SMART_CHARGE_USER_OTHER;
	}
}

bool oplus_usbtemp_l_recovery_current_status_ufcs(void)
{
	if (!g_oplus_chip)
		return false;

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if ((g_oplus_chip->usb_temp_l <= g_oplus_chip->usbtemp_cool_down_recover_ntc_low) &&
			(g_oplus_chip->usb_temp_l - g_tbatt_temp / 10) <=
				g_oplus_chip->usbtemp_cool_down_recover_gap_low)
			return true;
		return false;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if (((g_oplus_chip->usb_temp_l - g_tbatt_temp / 10) <= g_oplus_chip->usbtemp_cool_down_recover_gap_high) &&
			(g_oplus_chip->usb_temp_l <= (g_oplus_chip->usbtemp_cool_down_ntc_high3 - SMART_CHARGE_RECOVER_DELTA)))
			return true;
		return false;
	} else {
		return false;
	}
}

int oplus_usbtemp_r_trigger_current_status_ufcs(void)
{
	if (!g_oplus_chip)
		return SMART_CHARGE_USER_OTHER;

	if (g_oplus_chip->usb_temp_r < USB_30C || g_oplus_chip->usb_temp_r > USB_100C) {
		return SMART_CHARGE_USER_OTHER;
	}

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if ((g_oplus_chip->usb_temp_r >= g_oplus_chip->usbtemp_cool_down_ntc_low) ||
			(g_oplus_chip->usb_temp_r - g_tbatt_temp / 10) >=
				g_oplus_chip->usbtemp_cool_down_gap_low)
			return SMART_CHARGE_USER_USBTEMP;
		return SMART_CHARGE_USER_OTHER;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if ((g_oplus_chip->usb_temp_r - g_tbatt_temp / 10) >= g_oplus_chip->usbtemp_cool_down_gap_high) {
			return SMART_CHARGE_USER_USBTEMP;
		} else if ((g_oplus_chip->usb_temp_r >= g_oplus_chip->usbtemp_cool_down_ntc_high)) {
			return SMART_CHARGE_USER_USBTEMP;
		} else if ((g_oplus_chip->usb_temp_r >= g_oplus_chip->usbtemp_cool_down_ntc_high1)) {
			return SMART_CHARGE_USER_USBTEMP1;
		} else if ((g_oplus_chip->usb_temp_r >= g_oplus_chip->usbtemp_cool_down_ntc_high2)) {
			return SMART_CHARGE_USER_USBTEMP2;
		} else if ((g_oplus_chip->usb_temp_r >= g_oplus_chip->usbtemp_cool_down_ntc_high3)) {
			return SMART_CHARGE_USER_USBTEMP3;
		} else {
			return SMART_CHARGE_USER_OTHER;
		}
	} else {
		return SMART_CHARGE_USER_OTHER;
	}
}

bool oplus_usbtemp_r_recovery_current_status_ufcs(void)
{
	if (!g_oplus_chip)
		return false;

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if ((g_oplus_chip->usb_temp_r <= g_oplus_chip->usbtemp_cool_down_recover_ntc_low) &&
		    (g_oplus_chip->usb_temp_r - g_tbatt_temp / 10) <= g_oplus_chip->usbtemp_cool_down_recover_gap_low)
			return true;
		return false;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if ((g_oplus_chip->usb_temp_r <=
		     (g_oplus_chip->usbtemp_cool_down_ntc_high3 - SMART_CHARGE_RECOVER_DELTA)) &&
		    ((g_oplus_chip->usb_temp_r - g_tbatt_temp / 10) <=
		     g_oplus_chip->usbtemp_cool_down_recover_gap_high))
			return true;
		return false;
	} else {
		return false;
	}
}

static void oplus_update_usbtemp_current_status_ufcs(struct oplus_chg_chip *chip)
{
	static int limit_cur_cnt_r = 0;
	static int limit_cur_cnt_l = 0;
	static int recover_cur_cnt_r = 0;
	static int recover_cur_cnt_l = 0;
	int condition, batt_current;
	int last_usb_temp_l = 25;
	int last_usb_temp_r = 25;
	int trigger_l = SMART_CHARGE_USER_OTHER, trigger_r = SMART_CHARGE_USER_OTHER,
	    trigger_result = SMART_CHARGE_USER_OTHER;
	int recovery_l = SMART_CHARGE_USER_OTHER, recovery_r = SMART_CHARGE_USER_OTHER;

	if (!chip) {
		return;
	}

	batt_current = chip->usbtemp_batt_current;

	if ((chip->usb_temp_l < USB_30C || chip->usb_temp_l > USB_100C) &&
	    (chip->usb_temp_r < USB_30C || chip->usb_temp_r > USB_100C)) {
		chip->smart_charge_user = SMART_CHARGE_USER_OTHER;
		chip->usbtemp_cool_down = 0;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
		return;
	}
	trigger_l = oplus_usbtemp_l_trigger_current_status_ufcs();
	trigger_r = oplus_usbtemp_r_trigger_current_status_ufcs();
	recovery_l = oplus_usbtemp_l_recovery_current_status_ufcs();
	recovery_r = oplus_usbtemp_r_recovery_current_status_ufcs();
	if (!trigger_l || !trigger_r)
		trigger_result = max(trigger_l, trigger_r);
	else
		trigger_result = min(trigger_l, trigger_r);

	if (trigger_r && (chip->smart_charge_user == SMART_CHARGE_USER_OTHER || trigger_r < chip->smart_charge_user)) {
		limit_cur_cnt_r++;
		if (limit_cur_cnt_r >= RETRY_COUNT_UFCS) {
			limit_cur_cnt_r = RETRY_COUNT_UFCS;
		}
		recover_cur_cnt_r = 0;
	} else if (recovery_r) {
		recover_cur_cnt_r++;
		if (recover_cur_cnt_r >= RETRY_COUNT_UFCS) {
			recover_cur_cnt_r = RETRY_COUNT_UFCS;
		}
		limit_cur_cnt_r = 0;
	}

	if (trigger_l && (chip->smart_charge_user == SMART_CHARGE_USER_OTHER || trigger_l < chip->smart_charge_user)) {
		limit_cur_cnt_l++;
		if (limit_cur_cnt_l >= RETRY_COUNT_UFCS) {
			limit_cur_cnt_l = RETRY_COUNT_UFCS;
		}
		recover_cur_cnt_l = 0;
	} else if (recovery_l) {
		recover_cur_cnt_l++;
		if (recover_cur_cnt_l >= RETRY_COUNT_UFCS) {
			recover_cur_cnt_l = RETRY_COUNT_UFCS;
		}
		limit_cur_cnt_l = 0;
	}

	if ((RETRY_COUNT_UFCS <= limit_cur_cnt_r || RETRY_COUNT_UFCS <= limit_cur_cnt_l) &&
	    (chip->smart_charge_user != trigger_result)) {
		chg_err("use usbtemp cooldown g_tbatt_temp:%d, usb_temp_l:%d, "
			"usb_temp_r:%d, usbtemp_batttemp_current_gap:%d\n",
			g_tbatt_temp, chip->usb_temp_l, chip->usb_temp_r, chip->usbtemp_batttemp_current_gap);
		chip->smart_charge_user = trigger_result;
		chip->cool_down_done = true;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
		condition = USBTEMP_TRIGGER_CONDITION_COOL_DOWN;
		oplus_chg_track_upload_usbtemp_info(chip, condition, last_usb_temp_l, last_usb_temp_r, batt_current);
	} else if ((RETRY_COUNT_UFCS <= recover_cur_cnt_r && RETRY_COUNT_UFCS <= recover_cur_cnt_l) &&
		   (chip->smart_charge_user != SMART_CHARGE_USER_OTHER)) {
		chip->smart_charge_user = SMART_CHARGE_USER_OTHER;
		chip->usbtemp_cool_down = 0;
		limit_cur_cnt_r = 0;
		recover_cur_cnt_r = 0;
		limit_cur_cnt_l = 0;
		recover_cur_cnt_l = 0;
		condition = USBTEMP_TRIGGER_CONDITION_COOL_DOWN_RECOVERY;
		oplus_chg_track_upload_usbtemp_info(chip, condition, last_usb_temp_l, last_usb_temp_r, batt_current);
	}

	return;
}

bool oplus_usbtemp_condition_temp_high(void)
{
	if (!g_oplus_chip)
		return false;

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if (g_tbatt_temp / 10 <= g_oplus_chip->usbtemp_batt_temp_low &&
			(((g_oplus_chip->usb_temp_l >= g_oplus_chip->usbtemp_ntc_temp_low)
					&& (g_oplus_chip->usb_temp_l < USB_100C))
			|| ((g_oplus_chip->usb_temp_r >= g_oplus_chip->usbtemp_ntc_temp_low)
					&& (g_oplus_chip->usb_temp_r < USB_100C))))
			return true;
		return false;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if (g_tbatt_temp / 10 <= g_oplus_chip->usbtemp_batt_temp_high &&
			(((g_oplus_chip->usb_temp_l >= g_oplus_chip->usbtemp_ntc_temp_high)
					&& (g_oplus_chip->usb_temp_l < USB_100C))
			|| ((g_oplus_chip->usb_temp_r >= g_oplus_chip->usbtemp_ntc_temp_high)
					&& (g_oplus_chip->usb_temp_r < USB_100C))))
			return true;
		return false;
	} else {
		return false;
	}
}

bool oplus_usbtemp_temp_rise_fast_with_batt_temp(void)
{
	if (!g_oplus_chip)
		return false;

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if (g_tbatt_temp / 10 > g_oplus_chip->usbtemp_batt_temp_low &&
				(((g_oplus_chip->usb_temp_l >= g_tbatt_temp / 10 +
					g_oplus_chip->usbtemp_temp_gap_low_with_batt_temp)
					&& (g_oplus_chip->usb_temp_l < USB_100C))
				|| ((g_oplus_chip->usb_temp_r >= g_tbatt_temp / 10 +
					g_oplus_chip->usbtemp_temp_gap_low_with_batt_temp)
					&& (g_oplus_chip->usb_temp_r < USB_100C))))
			return true;
		return false;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if (g_tbatt_temp/10 > g_oplus_chip->usbtemp_batt_temp_high &&
				(((g_oplus_chip->usb_temp_l >= g_tbatt_temp / 10 +
					g_oplus_chip->usbtemp_temp_gap_high_with_batt_temp)
						&& (g_oplus_chip->usb_temp_l < USB_100C))
				|| ((g_oplus_chip->usb_temp_r >= g_tbatt_temp / 10 +
					g_oplus_chip->usbtemp_temp_gap_high_with_batt_temp)
						&& (g_oplus_chip->usb_temp_r < USB_100C))))
			return true;
		return false;
	} else {
		return false;
	}
}

bool oplus_usbtemp_temp_rise_fast_without_batt_temp(void)
{
	if (!g_oplus_chip)
		return false;

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if ((((g_oplus_chip->usb_temp_l - g_tbatt_temp / 10) >=
				g_oplus_chip->usbtemp_temp_gap_low_without_batt_temp)
				&& (g_oplus_chip->usb_temp_l < USB_100C)) ||
			(((g_oplus_chip->usb_temp_r - g_tbatt_temp / 10) >=
				g_oplus_chip->usbtemp_temp_gap_low_without_batt_temp)
				&& (g_oplus_chip->usb_temp_r < USB_100C)))
			return true;
		return false;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if ((((g_oplus_chip->usb_temp_l - g_tbatt_temp / 10) >=
				g_oplus_chip->usbtemp_temp_gap_high_without_batt_temp)
				&& (g_oplus_chip->usb_temp_l < USB_100C)) ||
			(((g_oplus_chip->usb_temp_r - g_tbatt_temp / 10) >=
				g_oplus_chip->usbtemp_temp_gap_high_without_batt_temp)
				&& (g_oplus_chip->usb_temp_r < USB_100C)))
			return true;
		return false;
	} else {
		return false;
	}
}

bool oplus_usbtemp_judge_temp_gap(int current_temp, int last_temp)
{
	if (!g_oplus_chip)
		return false;

	if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
		if ((current_temp - last_temp) >= g_oplus_chip->usbtemp_rise_fast_temp_low)
			return true;
		return false;
	} else if (g_oplus_chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
		if ((current_temp - last_temp) >= g_oplus_chip->usbtemp_rise_fast_temp_high)
			return true;
		return false;
	} else {
		return false;
	}
}

bool oplus_usbtemp_change_curr_range(struct oplus_chg_chip *chip, int retry_cnt,
					int usbtemp_first_time_in_curr_range, bool curr_range_change)
{
	static int last_curr_change_usb_temp_l = 25;
	static int current_curr_change_temp_l = 25;
	static int last_curr_change_usb_temp_r = 25;
	static int current_curr_change_temp_r = 25;
	int count_curr_r = 1, count_curr_l = 1;
	int i = 0;

	if (!chip)
		return false;

	chip->usbtemp_curr_status = OPLUS_USBTEMP_HIGH_CURR;
	if (usbtemp_first_time_in_curr_range == false) {
		last_curr_change_usb_temp_r = chip->usb_temp_r;
		last_curr_change_usb_temp_l = chip->usb_temp_l;
	} else {
		current_curr_change_temp_r = chip->usb_temp_r;
		current_curr_change_temp_l = chip->usb_temp_l;
	}
	if (((current_curr_change_temp_l - last_curr_change_usb_temp_l) >= OPLUS_USBTEMP_CURR_CHANGE_TEMP)
			|| (current_curr_change_temp_r - last_curr_change_usb_temp_r) >= OPLUS_USBTEMP_CURR_CHANGE_TEMP) {
		for (i = 1; i <= retry_cnt; i++) {
			mdelay(RETRY_CNT_DELAY);
			oplus_get_usbtemp_volt(chip);
			get_usb_temp(chip);
			if ((chip->usb_temp_r - last_curr_change_usb_temp_r) >= OPLUS_USBTEMP_CURR_CHANGE_TEMP
					&& chip->usb_temp_r < USB_100C)
				count_curr_r++;
			if ((chip->usb_temp_l - last_curr_change_usb_temp_l) >= OPLUS_USBTEMP_CURR_CHANGE_TEMP
					&& chip->usb_temp_l < USB_100C)
				count_curr_l++;
			pr_err("countl : %d,countr : %d", count_curr_l, count_curr_r);
		}
		current_curr_change_temp_l = chip->usb_temp_l;
		current_curr_change_temp_r = chip->usb_temp_r;

		if ((count_curr_l >= retry_cnt &&  chip->usb_temp_l > USB_30C && chip->usb_temp_l < USB_100C)
				|| (count_curr_r >= retry_cnt &&  chip->usb_temp_r > USB_30C  && chip->usb_temp_r < USB_100C)) {
			chg_err("change curr range...,current_temp_l=%d,last_usb_temp_l=%d,current_temp_r=%d,last_usb_temp_r =%d, chip->tbatt_temp = %d\n",
					current_curr_change_temp_l,
					last_curr_change_usb_temp_l,
					current_curr_change_temp_r,
					last_curr_change_usb_temp_r,
					chip->tbatt_temp);
			count_curr_r = 1;
			count_curr_l = 1;
			return true;
		}
	}
	return false;
}

bool oplus_usbtemp_trigger_for_high_temp(struct oplus_chg_chip *chip, int retry_cnt,
					int count_r, int count_l)
{
	int i = 0;

	if (!chip)
		return false;

	if (oplus_usbtemp_condition_temp_high()) {
		pr_err("in usbtemp higher than 57 or 69!\n");
		for (i = 1; i < retry_cnt; i++) {
			mdelay(RETRY_CNT_DELAY);
			oplus_get_usbtemp_volt(chip);
			get_usb_temp(chip);
			if (chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
				if (chip->usb_temp_r >= chip->usbtemp_ntc_temp_low && chip->usb_temp_r < USB_100C)
					count_r++;
				if (chip->usb_temp_l >= chip->usbtemp_ntc_temp_low && chip->usb_temp_l < USB_100C)
					count_l++;
			} else if (chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
				if (chip->usb_temp_r >= chip->usbtemp_ntc_temp_high && chip->usb_temp_r < USB_100C)
					count_r++;
				if (chip->usb_temp_l >= chip->usbtemp_ntc_temp_high && chip->usb_temp_l < USB_100C)
				count_l++;
			}
			pr_err("countl : %d countr : %d", count_l, count_r);
		}
	}
	if (count_r >= retry_cnt || count_l >= retry_cnt) {
		return true;
	}

	return false;
}

bool oplus_usbtemp_trigger_for_rise_fast_temp(struct oplus_chg_chip *chip, int retry_cnt,
					int count_r, int count_l)
{
	int i = 0;

	if (!chip)
		return false;

	if (oplus_usbtemp_temp_rise_fast_with_batt_temp()) {
		pr_err("in usbtemp rise fast with usbtemp!\n");
		for (i = 1; i <= retry_cnt; i++) {
			mdelay(RETRY_CNT_DELAY);
			oplus_get_usbtemp_volt(chip);
			get_usb_temp(chip);
			if (chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
				if ((chip->usb_temp_r >= g_tbatt_temp/10 + chip->usbtemp_temp_gap_low_with_batt_temp)
						&& chip->usb_temp_r < USB_100C)
					count_r++;
				if ((chip->usb_temp_l >= g_tbatt_temp/10 + chip->usbtemp_temp_gap_low_with_batt_temp)
						&& chip->usb_temp_l < USB_100C)
					count_l++;
			} else if (chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
				if ((chip->usb_temp_r >= g_tbatt_temp/10 + chip->usbtemp_temp_gap_high_with_batt_temp)
						&& chip->usb_temp_r < USB_100C)
					count_r++;
				if ((chip->usb_temp_l >= g_tbatt_temp/10 + chip->usbtemp_temp_gap_high_with_batt_temp)
						&& chip->usb_temp_l < USB_100C)
					count_l++;
			}
			pr_err("countl : %d countr : %d", count_l, count_r);
		}
		if (count_r >= retry_cnt || count_l >= retry_cnt) {
			return true;
		}
	}

	return false;
}

bool oplus_usbtemp_trigger_for_rise_fast_without_temp(struct oplus_chg_chip *chip, int retry_cnt,
					int count_r, int count_l, int total_count)
{
	static int count = 0;
	static int last_usb_temp_l = 25;
	static int current_temp_l = 25;
	static int last_usb_temp_r = 25;
	static int current_temp_r = 25;
	int i = 0;

	if (!chip)
		return false;

	if (oplus_usbtemp_temp_rise_fast_without_batt_temp()) {
		if (count == 0) {
			last_usb_temp_r = chip->usb_temp_r;
			last_usb_temp_l = chip->usb_temp_l;
			current_temp_r = chip->usb_temp_r;
			current_temp_l = chip->usb_temp_l;
		} else {
			current_temp_r = chip->usb_temp_r;
			current_temp_l = chip->usb_temp_l;
		}
		if (oplus_usbtemp_judge_temp_gap(current_temp_l, last_usb_temp_l)
				|| oplus_usbtemp_judge_temp_gap(current_temp_r, last_usb_temp_r)) {
			for (i = 1; i <= retry_cnt; i++) {
				mdelay(RETRY_CNT_DELAY);
				oplus_get_usbtemp_volt(chip);
				get_usb_temp(chip);
				current_temp_l = chip->usb_temp_l;
				current_temp_r = chip->usb_temp_r;
				if (oplus_usbtemp_judge_temp_gap(current_temp_r, last_usb_temp_r)
						&& chip->usb_temp_r < USB_100C)
					count_r++;
				if (oplus_usbtemp_judge_temp_gap(current_temp_l, last_usb_temp_l)
						&& chip->usb_temp_l < USB_100C)
					count_l++;
				pr_err("countl : %d,countr : %d", count_l, count_r);
			}
			current_temp_l = chip->usb_temp_l;
			current_temp_r = chip->usb_temp_r;
			if ((count_l >= retry_cnt &&  chip->usb_temp_l > USB_30C && chip->usb_temp_l < USB_100C)
					|| (count_r >= retry_cnt &&  chip->usb_temp_r > USB_30C  && chip->usb_temp_r < USB_100C))  {
					return true;
			}
			count_r = 1;
			count_l = 1;
		}
		count++;
		if (count > total_count)
			count = 0;
	} else {
		count = 0;
		last_usb_temp_r = chip->usb_temp_r;
		last_usb_temp_l = chip->usb_temp_l;
	}
	return false;
}

#define OPCHG_LOW_USBTEMP_RETRY_COUNT 10
#define OPLUS_CHG_CURRENT_READ_COUNT 15
static int oplus_usbtemp_monitor_main_new_method(void *data)
{
	int delay = 0;
	int vbus_volt = 0;
	static int count = 0;
	static int last_usb_temp_l = 25;
	static int last_usb_temp_r = 25;
	static int total_count = 0;
	int retry_cnt = 3;
	int count_r = 1, count_l = 1;
	bool condition1 = false;
	bool condition2 = false;
	int condition;
	struct oplus_chg_chip *chip = g_oplus_chip;
	static int log_count = 0;
	static bool curr_range_change = false;
	int batt_current = 0;
	struct timespec curr_range_change_first_time;
	struct timespec curr_range_change_last_time;
	bool usbtemp_first_time_in_curr_range = false;
	static int current_read_count = 0;

	struct timespec pre_hi_current_time;
	struct timespec now_time;

	/* add for variables init */
	curr_range_change_first_time.tv_sec = 0;
	curr_range_change_last_time.tv_sec = 0;
	pre_hi_current_time.tv_sec = 0;
	now_time.tv_sec = 0;

	pr_err("[oplus_usbtemp_monitor_main_new_method]:run first!");

	while (!kthread_should_stop()) {
		wait_event_interruptible(chip->oplus_usbtemp_wq_new_method, chip->usbtemp_check == true);
		if(chip->dischg_flag == true) {
			goto dischg;
		}
		oplus_get_usbtemp_volt(chip);
		get_usb_temp(chip);
		if ((chip->usb_temp_l < USB_50C) && (chip->usb_temp_r < USB_50C)) {/*get vbus when usbtemp < 50C*/
			vbus_volt = get_battery_mvolts_for_usbtemp_monitor(chip);
		} else {
			vbus_volt = 0;
		}
		if ((chip->usb_temp_l < USB_40C) && (chip->usb_temp_r < USB_40C)) {
			delay = MAX_MONITOR_INTERVAL;
			total_count = 10;
		} else {
			delay = MIN_MONITOR_INTERVAL;
			total_count = 30;
		}

		current_read_count = current_read_count + 1;
		if (current_read_count == OPLUS_CHG_CURRENT_READ_COUNT) {
			if (oplus_switching_support_parallel_chg()) {
				chip->usbtemp_batt_current = -(oplus_gauge_get_batt_current() + oplus_gauge_get_sub_batt_current());
			} else {
				if (oplus_vooc_get_allow_reading()) {
					chip->usbtemp_batt_current = -oplus_gauge_get_batt_current();
				} else {
					chip->usbtemp_batt_current = -oplus_gauge_get_prev_batt_current();
				}
			}
			current_read_count = 0;
		}

		oplus_usbtemp_recover_tbatt_temp(chip);
		if (oplus_is_ufcs_charging())
			oplus_update_usbtemp_current_status_ufcs(chip);
		else
			oplus_update_usbtemp_current_status_new_method(chip);

		batt_current = chip->usbtemp_batt_current;

		if ((chip->usbtemp_volt_l < USB_50C) && (chip->usbtemp_volt_r < USB_50C) && (vbus_volt < VBUS_VOLT_THRESHOLD))
			delay = VBUS_MONITOR_INTERVAL;

		if (usbtemp_dbg_curr_status < OPLUS_USBTEMP_LOW_CURR
					|| usbtemp_dbg_curr_status > OPLUS_USBTEMP_HIGH_CURR) {
			if (chip->usbtemp_batt_current > 5000) {
				chip->usbtemp_curr_status = OPLUS_USBTEMP_HIGH_CURR;
			} else if (chip->usbtemp_batt_current > 0 && chip->usbtemp_batt_current <= 5000) {
				chip->usbtemp_curr_status = OPLUS_USBTEMP_LOW_CURR;
			}
		} else if (usbtemp_dbg_curr_status == OPLUS_USBTEMP_LOW_CURR
					|| usbtemp_dbg_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
			chip->usbtemp_curr_status = usbtemp_dbg_curr_status;
		}

		if (curr_range_change == false && chip->usbtemp_batt_current < 5000
				&& chip->usbtemp_pre_batt_current >= 5000) {
			curr_range_change = true;
			curr_range_change_first_time = current_kernel_time();
		} else if (curr_range_change == false && chip->usbtemp_batt_current < 5000
				&& chip->usbtemp_change_across_unplug) {
			chip->usbtemp_change_across_unplug = false;
			now_time = current_kernel_time();
			if (now_time.tv_sec - pre_hi_current_time.tv_sec < OPLUS_USBTEMP_CHANGE_RANGE_TIME) {
				curr_range_change = true;
				curr_range_change_first_time = pre_hi_current_time;
			}
			chg_err("reconnected when hi_current, need keep %d seconds", now_time.tv_sec - pre_hi_current_time.tv_sec);
		} else if (curr_range_change == true && chip->usbtemp_batt_current >= 5000
				&& chip->usbtemp_pre_batt_current < 5000) {
			curr_range_change = false;
		}

		if (curr_range_change == true && chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
			if (oplus_usbtemp_change_curr_range(chip, retry_cnt,
						usbtemp_first_time_in_curr_range, curr_range_change))  {
				chip->usbtemp_curr_status = OPLUS_USBTEMP_LOW_CURR;
				curr_range_change = false;
			}
			if (usbtemp_first_time_in_curr_range == false) {
				usbtemp_first_time_in_curr_range = true;
			}
			curr_range_change_last_time = current_kernel_time();
			if (curr_range_change_last_time.tv_sec - curr_range_change_first_time.tv_sec >=
						OPLUS_USBTEMP_CHANGE_RANGE_TIME) {
				chip->usbtemp_curr_status = OPLUS_USBTEMP_LOW_CURR;
				curr_range_change = false;
			}
		} else {
			usbtemp_first_time_in_curr_range = false;
		}

		if ((chip->usb_temp_l < USB_40C) && (chip->usb_temp_r < USB_40C)) {
			total_count = OPCHG_LOW_USBTEMP_RETRY_COUNT;
		} else if (chip->usbtemp_curr_status == OPLUS_USBTEMP_LOW_CURR) {
			total_count = chip->usbtemp_rise_fast_temp_count_low;
		} else if (chip->usbtemp_curr_status == OPLUS_USBTEMP_HIGH_CURR) {
			total_count = chip->usbtemp_rise_fast_temp_count_high;
		}

		/*condition1  :the temp is higher than 57*/
		if (oplus_usbtemp_trigger_for_high_temp(chip, retry_cnt, count_r, count_l)) {
			if (!IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_enable)) {
					chip->dischg_flag = true;
				condition1 = true;
				chg_err("dischg enable1...[%d, %d, %d]\n", chip->usb_temp_l, chip->usb_temp_r, g_tbatt_temp);
			}
			count_r = 1;
			count_l = 1;
			count = 0;
			last_usb_temp_r = chip->usb_temp_r;
			last_usb_temp_l = chip->usb_temp_l;
		}

		if (oplus_usbtemp_trigger_for_rise_fast_temp(chip, retry_cnt, count_r, count_l)) {
			if (!IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_enable) ||
					chip->usbtemp_dischg_by_pmic) {
				chip->dischg_flag = true;
				condition1 = true;
				chg_err("dischg enable2...[%d, %d]\n", chip->usb_temp_l, chip->usb_temp_r);
			}
			count_r = 1;
			count_l = 1;
			count = 0;
			last_usb_temp_r = chip->usb_temp_r;
			last_usb_temp_l = chip->usb_temp_l;
		}
		if(condition1 == true) {
			pr_err("jump_to_dischg");
			goto dischg;
		}

		/*condition2  :the temp uprising to fast*/
		if (oplus_usbtemp_trigger_for_rise_fast_without_temp(chip, retry_cnt, count_r, count_l, total_count))  {
			if (!IS_ERR_OR_NULL(chip->normalchg_gpio.dischg_enable) ||
					chip->usbtemp_dischg_by_pmic) {
				chip->dischg_flag = true;
				condition2 = true;
			}
		}
	/*judge whether to go the action*/
	dischg:
		if ((chip->usb_temp_l < USB_30C || chip->usb_temp_l > USB_100C)
				&& (chip->usb_temp_r < USB_30C || chip->usb_temp_r > USB_100C)) {
			condition1 = false;
			condition2 = false;
			chip->dischg_flag = false;
		}
		if((condition1== true || condition2 == true) && chip->dischg_flag == true) {
			condition = (condition1== true ?
				USBTEMP_TRIGGER_CONDITION_1 :
				USBTEMP_TRIGGER_CONDITION_2);
			oplus_chg_track_upload_usbtemp_info(chip,
				condition, last_usb_temp_l, last_usb_temp_r, batt_current);
			oplus_usbtemp_dischg_action(chip);
			condition1 = false;
			condition2 = false;
		} else if (chip->debug_force_usbtemp_trigger) {
			oplus_chg_track_upload_usbtemp_info(
				chip, chip->debug_force_usbtemp_trigger,
				last_usb_temp_l, last_usb_temp_r, batt_current);
			chip->debug_force_usbtemp_trigger = 0;
		}
		msleep(delay);
		log_count++;
		chip->usbtemp_pre_batt_current = batt_current;
		if (chip->usbtemp_pre_batt_current > 5000) {
			pre_hi_current_time = current_kernel_time();
		}
		if (log_count == 40) {
			chg_err("==================usbtemp_volt_l[%d], usb_temp_l[%d], usbtemp_volt_r[%d], usb_temp_r[%d]\n",
					chip->usbtemp_volt_l, chip->usb_temp_l, chip->usbtemp_volt_r, chip->usb_temp_r);
			chg_err("usbtemp current status = %d [%d, %d, %d, %d]\n", chip->usbtemp_curr_status, usbtemp_dbg_curr_status,
				curr_range_change, chip->usbtemp_batt_current, chip->usbtemp_pre_batt_current);
			log_count = 0;
		}
	}

	return 0;
}

static void oplus_usbtemp_thread_init(void)
{
	if (g_oplus_chip->support_usbtemp_protect_v2)
		oplus_usbtemp_kthread =
				kthread_run(oplus_usbtemp_monitor_main_new_method, 0, "usbtemp_kthread");
	else
		oplus_usbtemp_kthread =
				kthread_run(oplus_usbtemp_monitor_main, 0, "usbtemp_kthread");
	if (IS_ERR(oplus_usbtemp_kthread)) {
		chg_err("failed to cread oplus_usbtemp_kthread\n");
	}
}

void oplus_wake_up_usbtemp_thread(void)
{
	if (g_oplus_chip == NULL) {
		chg_err("%s g_oplus_chip is null\n", __func__);
		return;
	}

	if (oplus_usbtemp_check_is_support() == true) {
		if (g_oplus_chip->support_usbtemp_protect_v2)
			wake_up_interruptible(&g_oplus_chip->oplus_usbtemp_wq_new_method);
		else
			wake_up_interruptible(&g_oplus_chip->oplus_usbtemp_wq);
	}
}

static int oplus_usbtemp_l_gpio_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.usbtemp_l_gpio_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.usbtemp_l_gpio_pinctrl)) {
		chg_err("get usbtemp_l_gpio_pinctrl fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.usbtemp_l_gpio_default =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.usbtemp_l_gpio_pinctrl, "usbtemp_l_gpio_default");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.usbtemp_l_gpio_default)) {
		chg_err("set usbtemp_l_gpio_default error\n");
		return -EINVAL;
	}

	pinctrl_select_state(bcdev->oplus_custom_gpio.usbtemp_l_gpio_pinctrl,
		bcdev->oplus_custom_gpio.usbtemp_l_gpio_default);

	return 0;
}

static int oplus_usbtemp_r_gpio_init(struct oplus_chg_chip *chip)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return -EINVAL;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (!bcdev) {
		chg_err("[OPLUS_CHG][%s]: bcdev not ready!\n", __func__);
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.usbtemp_r_gpio_pinctrl = devm_pinctrl_get(bcdev->dev);
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.usbtemp_r_gpio_pinctrl)) {
		chg_err("get usbtemp_r_gpio_pinctrl fail\n");
		return -EINVAL;
	}

	bcdev->oplus_custom_gpio.usbtemp_r_gpio_default =
		pinctrl_lookup_state(bcdev->oplus_custom_gpio.usbtemp_r_gpio_pinctrl, "usbtemp_r_gpio_default");
	if (IS_ERR_OR_NULL(bcdev->oplus_custom_gpio.usbtemp_r_gpio_default)) {
		chg_err("set usbtemp_r_gpio_default error\n");
		return -EINVAL;
	}

	pinctrl_select_state(bcdev->oplus_custom_gpio.usbtemp_r_gpio_pinctrl,
		bcdev->oplus_custom_gpio.usbtemp_r_gpio_default);

	return 0;
}

static int oplus_usbtemp_adc_gpio_dt(struct oplus_chg_chip *chip)
{
	int rc = 0;
	struct device_node *node = NULL;

	if (!chip) {
		pr_err("chip is null\n");
		return -EINVAL;
	}

	node = chip->dev->of_node;
	if (!node) {
		pr_err("device tree node missing\n");
			return -EINVAL;
	}

	chip->normalchg_gpio.dischg_gpio = of_get_named_gpio(node, "qcom,dischg-gpio", 0);
	if (chip->normalchg_gpio.dischg_gpio <= 0) {
		chg_err("Couldn't read qcom,dischg-gpio rc=%d, qcom,dischg-gpio:%d\n",
				rc, chip->normalchg_gpio.dischg_gpio);
	} else {
		if (oplus_usbtemp_check_is_support() == true) {
			if (gpio_is_valid(chip->normalchg_gpio.dischg_gpio)) {
				rc = gpio_request(chip->normalchg_gpio.dischg_gpio, "dischg-gpio");
				if (rc) {
					chg_err("unable to request dischg-gpio:%d\n", chip->normalchg_gpio.dischg_gpio);
				} else {
					oplus_dischg_gpio_init(chip);
				}
			}
		}
		chg_err("dischg-gpio:%d\n", chip->normalchg_gpio.dischg_gpio);
	}

	oplus_usbtemp_l_gpio_init(chip);
	oplus_usbtemp_r_gpio_init(chip);

	return rc;
}

static int oplus_mos0_switch_pinctrl_init(struct oplus_chg_chip *chip)
{
	if (!chip) {
		chg_err("chip NULL\n");
		return EINVAL;
	}

	chip->normalchg_gpio.pinctrl = devm_pinctrl_get(chip->dev);

	if (IS_ERR_OR_NULL(chip->normalchg_gpio.pinctrl)) {
		chg_err("get dischg_pinctrl fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.mos0_switch_enable =
		pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "mos0_switch_enable");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.mos0_switch_enable)) {
		chg_err("get mos0_switch_enable fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.mos0_switch_disable =
		pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "mos0_switch_disable");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.mos0_switch_disable)) {
		chg_err("get mos0_switch_disable fail\n");
		return -EINVAL;
	}

	pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.mos0_switch_enable);

	return 0;
}

static int oplus_mos1_switch_pinctrl_init(struct oplus_chg_chip *chip)
{
	if (!chip) {
		chg_err("chip NULL\n");
		return EINVAL;
	}

	chip->normalchg_gpio.pinctrl = devm_pinctrl_get(chip->dev);

	if (IS_ERR_OR_NULL(chip->normalchg_gpio.pinctrl)) {
		chg_err("get dischg_pinctrl fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.mos1_switch_enable =
		pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "mos1_switch_enable");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.mos1_switch_enable)) {
		chg_err("get mos1_switch_enable fail\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.mos1_switch_disable =
		pinctrl_lookup_state(chip->normalchg_gpio.pinctrl, "mos1_switch_disable");
	if (IS_ERR_OR_NULL(chip->normalchg_gpio.mos1_switch_disable)) {
		chg_err("get mos1_switch_disable fail\n");
		return -EINVAL;
	}

	pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.mos1_switch_enable);

	return 0;
}

static int oplus_ufcs_mos0_switch_gpio_dt(struct oplus_chg_chip *chip)
{
	int rc = 0;
	struct device_node *node = NULL;

	if (!chip) {
		pr_err("chip is null\n");
		return -EINVAL;
	}

	node = chip->dev->of_node;
	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.mos0_switch_gpio = of_get_named_gpio(node, "oplus,mos0-switch-gpio", 0);
	if (chip->normalchg_gpio.mos0_switch_gpio <= 0) {
		chg_err("Couldn't read mos0_switch_gpio rc=%d, mos0_switch_gpio:%d\n", rc,
			chip->normalchg_gpio.mos0_switch_gpio);
	} else {
		if (gpio_is_valid(chip->normalchg_gpio.mos0_switch_gpio)) {
			rc = gpio_request(chip->normalchg_gpio.mos0_switch_gpio, "mos0-switch-gpio");
			if (rc) {
				chg_err("unable to request mos0-switch-gpio:%d\n",
					chip->normalchg_gpio.mos0_switch_gpio);
			} else {
				oplus_mos0_switch_pinctrl_init(chip);
			}
		}
	}
	chg_err("mos0-switch:%d\n", chip->normalchg_gpio.mos0_switch_gpio);

	return rc;
}

static int oplus_ufcs_mos1_switch_gpio_dt(struct oplus_chg_chip *chip)
{
	int rc = 0;
	struct device_node *node = NULL;

	if (!chip) {
		pr_err("chip is null\n");
		return -EINVAL;
	}

	node = chip->dev->of_node;
	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	chip->normalchg_gpio.mos1_switch_gpio = of_get_named_gpio(node, "oplus,mos1-switch-gpio", 0);
	if (chip->normalchg_gpio.mos1_switch_gpio <= 0) {
		chg_err("Couldn't read mos1_switch_gpio rc=%d, mos1_switch_gpio:%d\n", rc,
			chip->normalchg_gpio.mos1_switch_gpio);
	} else {
		if (gpio_is_valid(chip->normalchg_gpio.mos1_switch_gpio)) {
			rc = gpio_request(chip->normalchg_gpio.mos1_switch_gpio, "mos1-switch-gpio");
			if (rc) {
				chg_err("unable to request mos1-switch-gpio:%d\n",
					chip->normalchg_gpio.mos1_switch_gpio);
			} else {
				oplus_mos1_switch_pinctrl_init(chip);
			}
		}
	}
	chg_err("mos1-switch:%d\n", chip->normalchg_gpio.mos1_switch_gpio);

	return rc;
}

static bool oplus_ufcs_mos0_switch_check_is_gpio(struct oplus_chg_chip *chip)
{
	if (!chip)
		return false;

	if (gpio_is_valid(chip->normalchg_gpio.mos0_switch_gpio))
		return true;

	return false;
}

static bool oplus_ufcs_mos1_switch_check_is_gpio(struct oplus_chg_chip *chip)
{
	if (!chip)
		return false;

	if (gpio_is_valid(chip->normalchg_gpio.mos1_switch_gpio))
		return true;

	return false;
}

bool oplus_ufcs_get_mos0_switch(void)
{
	bool mos0_status = true;
	struct battery_chg_dev *bcdev = NULL;

	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip)
		return true;

	if (!oplus_ufcs_mos0_switch_check_is_gpio(chip)) {
		return true;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	mutex_lock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));
	mos0_status = gpio_get_value(chip->normalchg_gpio.mos0_switch_gpio);
	mutex_unlock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));

	return mos0_status;
}

bool oplus_ufcs_set_mos0_switch(bool enable)
{
	struct battery_chg_dev *bcdev = NULL;

	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip)
		return false;

	if (!oplus_ufcs_mos0_switch_check_is_gpio(chip)) {
		return false;
	}

	if ((IS_ERR_OR_NULL(chip->normalchg_gpio.mos1_switch_enable) ||
	     (IS_ERR_OR_NULL(chip->normalchg_gpio.mos1_switch_enable)))) {
		return false;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;

	mutex_lock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));
	if (enable)
		pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.mos0_switch_enable);
	else
		pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.mos0_switch_disable);
	mutex_unlock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));


	return true;
}

bool oplus_ufcs_get_mos1_switch(void)
{
	bool mos1_status = true;
	struct battery_chg_dev *bcdev = NULL;

	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip)
		return true;

	if (!oplus_ufcs_mos1_switch_check_is_gpio(chip)) {
		return true;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	mutex_lock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));
	mos1_status = gpio_get_value(chip->normalchg_gpio.mos1_switch_gpio);
	mutex_unlock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));

	return mos1_status;
}

bool oplus_ufcs_set_mos1_switch(bool enable)
{
	struct battery_chg_dev *bcdev = NULL;

	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip)
		return false;

	if (!oplus_ufcs_mos1_switch_check_is_gpio(chip)) {
		return false;
	}
	if ((IS_ERR_OR_NULL(chip->normalchg_gpio.mos1_switch_enable) ||
	     (IS_ERR_OR_NULL(chip->normalchg_gpio.mos1_switch_enable)))) {
		return false;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	mutex_lock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));
	if (enable)
		pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.mos1_switch_enable);
	else
		pinctrl_select_state(chip->normalchg_gpio.pinctrl, chip->normalchg_gpio.mos1_switch_disable);
	mutex_unlock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));

	return true;
}

static int oplus_ufcs_mos_switch_gpio_init(struct oplus_chg_chip *chip)
{
	int rc = 0;

	if (!chip)
		return EINVAL;

	rc = oplus_ufcs_mos0_switch_gpio_dt(chip);
	rc = oplus_ufcs_mos1_switch_gpio_dt(chip);

	oplus_ufcs_get_mos0_switch();
	oplus_ufcs_get_mos1_switch();

	return rc;
}

#define DUMP_LOG_CNT_30S 3
#define DUMP_MAX_BYTE 0x27
static void dump_regs(void)
{
	static int dump_count = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	const int extra_num = 16;
	static u32 data_buffer[MAX_OEM_PROPERTY_DATA_SIZE];
	static int buf_num = 0;

	if(!chip) {
		chg_err("g_oplus_chip is not ready\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	if(!bcdev) {
		chg_err("battery_chg_dev *bcdev is not ready\n");
		return;
	}
	if(!chip->charger_exist) {
		return;
	}
	if(dump_count == DUMP_LOG_CNT_30S) {
		dump_count = 0;

	if(chip->fg_info_package_read_support) {
		buf_num = sizeof(bcdev->read_buffer_dump.data_buffer);
		memcpy(data_buffer, bcdev->read_buffer_dump.data_buffer, buf_num);
	} else {
		buf_num = sizeof(bcdev->read_pmic_buffer_dump.data_buffer);
		memcpy(data_buffer, bcdev->read_pmic_buffer_dump.data_buffer, buf_num);
	}

	chg_err("sm8550_st_dump: [chg_en=%d, suspend=%d, pd_svooc=%d, subtype=0x%02x],"
		"[oplus_UsbCommCapable=%d, oplus_pd_svooc=%d, typec_mode=%d, cid_status=0x%02x, usb_in_status=%d],"
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x, 0x%4x=0x%02x], "
		"[0x%4x=0x%02x], \n",
		smbchg_get_charge_enable(), data_buffer[9], data_buffer[11], oplus_chg_get_charger_subtype(),
		data_buffer[10], data_buffer[11], data_buffer[12], bcdev->cid_status, bcdev->usb_in_status,
		data_buffer[extra_num - 1], data_buffer[extra_num], data_buffer[extra_num + 1],
		data_buffer[extra_num + 2], data_buffer[extra_num + 3], data_buffer[extra_num + 4],
		data_buffer[extra_num + 5], data_buffer[extra_num + 6], data_buffer[extra_num + 7],
		data_buffer[extra_num + 8], data_buffer[extra_num + 9], data_buffer[extra_num + 10],
		data_buffer[extra_num + 11], data_buffer[extra_num + 12], data_buffer[extra_num + 13],
		data_buffer[extra_num + 14], data_buffer[extra_num + 15], data_buffer[extra_num + 16],
		data_buffer[extra_num + 17], data_buffer[extra_num + 18], data_buffer[extra_num + 19],
		data_buffer[extra_num + 20], data_buffer[extra_num + 21], data_buffer[extra_num + 22],
		data_buffer[extra_num + 23], data_buffer[extra_num + 24], data_buffer[extra_num + 25],
		data_buffer[extra_num + 26], data_buffer[extra_num + 27], data_buffer[extra_num + 28],
		data_buffer[extra_num + 29], data_buffer[extra_num + 30], data_buffer[extra_num + 31],
		data_buffer[extra_num + 32], data_buffer[extra_num + 33], data_buffer[extra_num + 34],
		data_buffer[extra_num + 35], data_buffer[extra_num + 36], data_buffer[extra_num + 37],
		data_buffer[extra_num + 38], data_buffer[extra_num + 39], data_buffer[extra_num + 40],
		data_buffer[extra_num + 41], data_buffer[extra_num + 42], data_buffer[extra_num + 43],
		data_buffer[extra_num + 44], data_buffer[extra_num + 45], data_buffer[extra_num + 46],
		data_buffer[extra_num + 47], data_buffer[extra_num + 48], data_buffer[extra_num + 49],
		data_buffer[extra_num + 50], data_buffer[extra_num + 51], data_buffer[extra_num + 52],
		data_buffer[extra_num + 53], data_buffer[extra_num + 54], data_buffer[extra_num + 55],
		data_buffer[extra_num + 56], data_buffer[extra_num + 57], data_buffer[extra_num + 58],
		data_buffer[extra_num + 59], data_buffer[extra_num + 60], data_buffer[extra_num + 61],
		data_buffer[extra_num + 62], data_buffer[extra_num + 63], data_buffer[extra_num + 64],
		data_buffer[extra_num + 65], data_buffer[extra_num + 66], data_buffer[extra_num + 67],
		data_buffer[extra_num + 68], data_buffer[extra_num + 69], data_buffer[extra_num + 70],
		data_buffer[extra_num + 71], data_buffer[extra_num + 72], data_buffer[extra_num + 73],
		data_buffer[extra_num + 74], data_buffer[extra_num + 75], data_buffer[extra_num + 76],
		data_buffer[extra_num + 77], data_buffer[extra_num + 78], data_buffer[extra_num + 79],
		data_buffer[extra_num + 80], data_buffer[extra_num + 81], data_buffer[extra_num + 82],
		data_buffer[extra_num + 83], data_buffer[extra_num + 84], data_buffer[extra_num + 85],
		data_buffer[extra_num + 86], data_buffer[extra_num + 87], data_buffer[extra_num + 88],
		data_buffer[extra_num + 89], data_buffer[extra_num + 90], data_buffer[extra_num + 91],
		data_buffer[extra_num + 92], data_buffer[extra_num + 93], data_buffer[extra_num + 94],
		data_buffer[extra_num + 95], data_buffer[extra_num + 96], data_buffer[extra_num + 97],
		data_buffer[extra_num + 98], data_buffer[extra_num + 99], data_buffer[extra_num + 100],
		data_buffer[extra_num + 101], data_buffer[extra_num + 102], data_buffer[extra_num + 103],
		data_buffer[extra_num + 104]);
	}
	dump_count++;
	return;
}

static int smbchg_kick_wdt(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;
	int rc;

	if (!chip) {
		chg_err("!!!chip null, oplus_get_batt_argv_buffer\n");
		return -ENOTCONN;
	}

	if (chip->fg_info_package_read_support ||
	    !smbchg_get_charge_enable())
		return 0;

	bcdev = chip->pmic_spmi.bcdev_chip;
	rc = oem_read_pmic_buffer(bcdev);

	return rc;
}

static int smbchg_set_fastchg_current_raw(int current_ma)
{
	int rc = 0;
	int prop_id = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT);
	rc = write_property_id(bcdev, pst, prop_id, current_ma * 1000);
	if (rc)
		chg_err("set fcc to %d mA fail, rc=%d\n", current_ma, rc);
	else
		chg_err("set fcc to %d mA\n", current_ma);

	return rc;
}

static int smbchg_set_wls_boost_en(bool enable)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_WLS];

	if (enable) {
		rc = write_property_id(bcdev, pst, WLS_BOOST_EN, 1);
	} else {
		rc = write_property_id(bcdev, pst, WLS_BOOST_EN, 0);
	}
	if (rc) {
		chg_err("set fcc to %d mA fail, rc=%d\n", enable, rc);
	} else {
		chg_err("set fcc to %d mA\n", enable);
	}
	return rc;
}

bool qpnp_get_prop_vbus_collapse_status(void)
{
	int rc = 0;
	bool collapse_status = false;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_VBUS_COLLAPSE_STATUS);
	if (rc < 0) {
		chg_err("read usb vbus_collapse_status fail, rc=%d\n", rc);
		return false;
	}
	collapse_status = pst->prop[USB_VBUS_COLLAPSE_STATUS];
	chg_err("read usb vbus_collapse_status[%d]\n",
			collapse_status);
	return collapse_status;
}

static int oplus_chg_set_input_current_with_no_aicl(int current_ma)
{
	int rc = 0;
	int prop_id = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);
	rc = write_property_id(bcdev, pst, prop_id, current_ma * 1000);
	if (rc)
		chg_err("set icl to %d mA fail, rc=%d\n", current_ma, rc);
	else
		chg_err("set icl to %d mA\n", current_ma);

	return rc;
}

static int oplus_chg_set_wls_input_current(int current_ma)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_WLS];

	rc = write_property_id(bcdev, pst, WLS_INPUT_CURR_LIMIT, current_ma * 1000);
	if (rc)
		chg_err("set wls input current to %d mA fail, rc=%d\n", current_ma, rc);
	else
		chg_err("set wls input current to %d mA\n", current_ma);

	return rc;
}

static void smbchg_set_aicl_point(int vol)
{
	/*do nothing*/
}

#define AICL_POINT_VOL_9V 7600
static int usb_icl[] = {
	300, 500, 800, 1000, 1200, 1500, 1750, 2000, 3000,
};

static int oplus_chg_set_input_current(int current_ma)
{
	int rc = 0, i = 0;
	int chg_vol = 0;
	int aicl_point = 0;
	int prop_id = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}

	if (chip->mmi_chg == 0) {
		chg_debug("mmi_chg, return\n");
		return rc;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);

	chg_debug("usb input max current limit=%d setting %02x\n", current_ma, i);

	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (chg_vol > AICL_POINT_VOL_9V) {
		aicl_point = AICL_POINT_VOL_9V;
	} else {
		if (chip->batt_volt > 4100) {
			aicl_point = 4550;
		} else {
			aicl_point = 4500;
		}
	}

	if (current_ma < 500) {
		i = 0;
		goto aicl_end;
	}

	i = 1; /* 500 */
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	usleep_range(50000, 51000);
	if (qpnp_get_prop_vbus_collapse_status() == true) {
		chg_debug("use 500 here\n");
		goto aicl_boost_back;
	}
	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (chg_vol < aicl_point) {
		chg_debug("use 500 here\n");
		goto aicl_end;
	} else if (current_ma < 800)
		goto aicl_end;

	i = 2; /* 800 */
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	usleep_range(50000, 51000);
	if (qpnp_get_prop_vbus_collapse_status() == true) {
		i = i - 1;
		goto aicl_boost_back;
	}
	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (chg_vol < aicl_point) {
		i = i - 1;
		goto aicl_pre_step;
	}

	i = 3; /* 1000 */
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	usleep_range(50000, 51000);
	if (qpnp_get_prop_vbus_collapse_status() == true) {
		i = i - 1;
		goto aicl_boost_back;
	}
	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (chg_vol < aicl_point) {
		i = i - 1;
		goto aicl_pre_step;
	} else if (current_ma < 1200)
		goto aicl_end;

	i = 4; /* 1200 */
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	usleep_range(50000, 51000);
	if (qpnp_get_prop_vbus_collapse_status() == true) {
		i = i - 2;
		goto aicl_boost_back;
	}
	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (chg_vol < aicl_point) {
		i = i - 2;
		goto aicl_pre_step;
	} else if (current_ma < 1500)
		goto aicl_end;

	i = 5; /* 1500 */
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	usleep_range(50000, 51000);
	if (qpnp_get_prop_vbus_collapse_status() == true) {
		i = i - 2;
		goto aicl_boost_back;
	}
	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (chg_vol < aicl_point) {
		i = i - 2; /*We DO NOT use 1.2A here*/
		goto aicl_pre_step;
	} else if (current_ma < 2000)
		goto aicl_end;

	i = 6; /* 1750 */
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	usleep_range(50000, 51000);
	if (qpnp_get_prop_vbus_collapse_status() == true) {
		i = i - 2;
		goto aicl_boost_back;
	}
	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (chg_vol < aicl_point) {
		i = i - 2; /*1.2*/
		goto aicl_pre_step;
	}

	i = 7; /* 2000 */
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	usleep_range(50000, 51000);
	if (qpnp_get_prop_vbus_collapse_status() == true) {
		i = i - 2;
		goto aicl_boost_back;
	}
	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (chg_vol < aicl_point) {
		i =  i - 2; /*1.5*/
		goto aicl_pre_step;
	} else if (current_ma < 3000)
		goto aicl_end;

	i = 8; /* 3000 */
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	usleep_range(50000, 51000);
	if (qpnp_get_prop_vbus_collapse_status() == true) {
		i = i - 1;
		goto aicl_boost_back;
	}
	chg_vol = qpnp_get_prop_charger_voltage_now();
	if (chg_vol < aicl_point) {
		i = i - 1;
		goto aicl_pre_step;
	} else if (current_ma >= 3000)
		goto aicl_end;

aicl_pre_step:
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	if ((chip->charger_type == POWER_SUPPLY_TYPE_USB_DCP &&
	    current_ma >= OPLUS_CHG_TRACK_ICL_MONITOR_THD_MA &&
	    usb_icl[i] < OPLUS_CHG_TRACK_ICL_MONITOR_THD_MA) ||
	    bcdev->debug_force_icl_err)
		oplus_chg_track_upload_icl_err_info(
			bcdev, TRACK_PMIC_ERR_ICL_VBUS_LOW_POINT);
	chg_debug("usb input max current limit aicl chg_vol=%d j[%d]=%d sw_aicl_point:%d aicl_pre_step\n", chg_vol, i, usb_icl[i], aicl_point);
	goto aicl_return;
aicl_end:
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	if ((chip->charger_type == POWER_SUPPLY_TYPE_USB_DCP &&
	    current_ma >= OPLUS_CHG_TRACK_ICL_MONITOR_THD_MA &&
	    usb_icl[i] < OPLUS_CHG_TRACK_ICL_MONITOR_THD_MA) ||
	    bcdev->debug_force_icl_err)
		oplus_chg_track_upload_icl_err_info(
			bcdev, TRACK_PMIC_ERR_ICL_VBUS_LOW_POINT);
	chg_debug("usb input max current limit aicl chg_vol=%d j[%d]=%d sw_aicl_point:%d aicl_end\n", chg_vol, i, usb_icl[i], aicl_point);
	goto aicl_return;
aicl_boost_back:
	rc = write_property_id(bcdev, pst, prop_id, usb_icl[i] * 1000);
	if (rc) {
		chg_err("set icl to %d mA fail, rc=%d\n", usb_icl[i], rc);
	} else {
		chg_err("set icl to %d mA\n", usb_icl[i]);
	}
	if ((chip->charger_type == POWER_SUPPLY_TYPE_USB_DCP &&
	    current_ma >= OPLUS_CHG_TRACK_ICL_MONITOR_THD_MA &&
	    usb_icl[i] < OPLUS_CHG_TRACK_ICL_MONITOR_THD_MA) ||
	    bcdev->debug_force_icl_err)
		oplus_chg_track_upload_icl_err_info(
			bcdev, TRACK_PMIC_ERR_ICL_VBUS_COLLAPSE);
	chg_debug("usb input max current limit aicl chg_vol=%d j[%d]=%d sw_aicl_point:%d aicl_boost_back\n", chg_vol, i, usb_icl[i], aicl_point);
	goto aicl_return;
aicl_return:
	return rc;
}

static int smbchg_float_voltage_set(int vfloat_mv)
{
	int rc = 0;
	int prop_id = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}

	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY
		&& (oplus_voocphy_get_fastchg_ing() == true
		||oplus_voocphy_get_fastchg_start() == true)
		&& !(oplus_vooc_get_fast_chg_type() == CHARGER_SUBTYPE_FASTCHG_VOOC && chip->chg_ctrl_by_vooc)) {
		chg_err("fastchg ing, do not set fv\n");
		return rc;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_VOLTAGE_MAX);
	rc = write_property_id(bcdev, pst, prop_id, vfloat_mv);
	if (rc)
		chg_err("set fv to %d mV fail, rc=%d\n", vfloat_mv, rc);
	else
		chg_err("set fv to %d mV\n", vfloat_mv);

	return rc;
}

static int smbchg_term_current_set(int term_current)
{
	int rc = 0;
/*#if 0
	u8 val_raw = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (term_current < 0 || term_current > 750)
		term_current = 150;

	val_raw = term_current / 50;
	rc = smblib_masked_write(&chip->pmic_spmi.smb5_chip->chg, TCCC_CHARGE_CURRENT_TERMINATION_CFG_REG,
			TCCC_CHARGE_CURRENT_TERMINATION_SETTING_MASK, val_raw);
	if (rc < 0)
		chg_err("Couldn't write TCCC_CHARGE_CURRENT_TERMINATION_CFG_REG rc=%d\n", rc);
#endif*/
	return rc;
}

static int oplus_ap_init_adsp_gague(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	rc = write_property_id(bcdev, pst, BATT_ADSP_GAUGE_INIT, 1);
	if (rc)
		chg_err("init adsp gague fail, rc=%d\n", rc);
	else
		chg_err("init adsp gague sucess.");

	return rc;
}

static int smbchg_charging_enable(void)
{
	int rc = 0;
	int is_rf_ftm_mode;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	is_rf_ftm_mode = oplus_is_rf_ftm_mode();
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	mutex_lock(&bcdev->chg_en_lock);
	if (is_rf_ftm_mode) {
		chg_err("is_rf_ftm_mode, force disable charger");
		rc = write_property_id(bcdev, pst, BATT_CHG_EN, 0);
		if (rc) {
			chg_err("set disable charging fail, rc=%d\n", rc);
		} else {
			bcdev->chg_en = false;
			chg_err("set disable charging sucess.\n");
		}
	} else {
		rc = write_property_id(bcdev, pst, BATT_CHG_EN, 1);
		if (rc) {
			chg_err("set enable charging fail, rc=%d\n", rc);
		} else {
			bcdev->chg_en = true;
			chg_err("set enable charging sucess.");
		}
	}
	mutex_unlock(&bcdev->chg_en_lock);

	return rc;
}

int oplus_get_charger_cycle(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	u32 cycle_count = 0;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) {
		cycle_count = bcdev->read_buffer_dump.data_buffer[5];
		return cycle_count;
	}

	rc = read_property_id(bcdev, pst, BATT_CYCLE_COUNT);
	if (rc) {
		chg_err("set charger_cycle fail, rc=%d\n", rc);
		return rc;
	}

	chg_err("get charger_cycle[%d]\n", pst->prop[BATT_CYCLE_COUNT]);
	cycle_count = pst->prop[BATT_CYCLE_COUNT];

	return cycle_count;
}

int oplus_adsp_voocphy_get_enable()
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return 0;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_VOOCPHY_ENABLE);
	if (rc) {
		chg_err("get adsp voocphy enable fail, rc=%d\n", rc);
		return 0;
	} else {
		chg_err("get adsp voocphy enable success, rc=%d, result = %d\n", rc, pst->prop[USB_VOOCPHY_ENABLE]);
	}

	return pst->prop[USB_VOOCPHY_ENABLE];
}

int oplus_adsp_voocphy_enable(bool enable)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_VOOCPHY_ENABLE, enable);
	if (rc) {
		chg_err("set enable adsp voocphy fail, enable = %d, rc=%d\n", enable, rc);
	} else {
		chg_err("set enable adsp voocphy success, enable = %d, rc=%d\n", enable, rc);
	}

	return rc;
}

static void oplus_adsp_force_svooc_work(struct work_struct *work)
{
	int rc = 0;
	int enable = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	enable = bcdev->force_svooc;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_PPS_FORCE_SVOOC, enable);
	if (rc)
		chg_err("oplus_adsp_force_svooc fail, rc=%d, enable:%d\n", rc, enable);
	else
		chg_err("oplus_adsp_force_svooc success, rc=%d, enable:%d\n", rc, enable);

	return;
}

int oplus_adsp_force_svooc(bool enable)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	bcdev->force_svooc = enable;
	schedule_delayed_work(&bcdev->apsd_force_svooc_work, 0);
	return 0;
}

static void oplus_chg_status_send_adsp_work(struct work_struct *work)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	bool chg_enable = false, chg_suspend = true, chg_status = false;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	chg_enable = g_oplus_chip->chg_ops->get_charging_enable();
	chg_suspend = g_oplus_chip->chg_ops->charger_suspend_check();
	if (!oplus_chg_get_adsp_notify_ap_suspend()
		&& chg_enable && !chg_suspend) {
		chg_status = true;
	}

	rc = write_property_id(bcdev, pst, BATT_SEND_CHG_STATUS, chg_status);
	if (rc) {
		chg_err("send chg status fail, rc=%d\n", rc);
	} else {
		chg_err("send chg status success, rc=%d, chg_status=%d\n", rc, chg_status);
	}
}

int oplus_pps_voocphy_enable(bool enable)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_PPS_VOOCPHY_ENABLE, enable);
	if (rc) {
		chg_err("oplus_pps_voocphy_enable fail, rc=%d\n", rc);
	} else {
		chg_err("oplus_pps_voocphy_enable success, rc=%d\n", rc);
	}

	return rc;
}

static void oplus_suspend_check_work(struct work_struct *work)
{
/*#if 0
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	oplus_chg_unsuspend_plat_pmic(chip);
	rc = write_property_id(bcdev, pst, USB_SUSPEND_PMIC, true);
	if (rc) {
		chg_err("set usb_suspend_pmic fail, rc=%d\n", rc);
	} else {
		chg_err("set usb_suspend_pmic success, rc=%d\n", rc);
	}
#endif*/
}

void oplus_adsp_voocphy_cancle_err_check(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("!!!chip null, oplus_adsp_voocphy_cancle_err_check\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	if (bcdev->adsp_voocphy_err_check == true) {
		cancel_delayed_work_sync(&bcdev->adsp_voocphy_err_work);
	}
	bcdev->adsp_voocphy_err_check = false;
}

static void oplus_adsp_voocphy_err_work(struct work_struct *work)
{
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev = NULL;

	if (!chip) {
		chg_err("!!!chip null, oplus_adsp_voocphy_err_work\n");
		return;
	}

	chg_err("!!!call\n");
	bcdev = chip->pmic_spmi.bcdev_chip;
	if (oplus_voocphy_get_fastchg_ing() == false && bcdev->adsp_voocphy_err_check) {
		chg_err("!!!happend\n");
		bcdev->adsp_voocphy_err_check = false;
		oplus_chg_suspend_charger();
		usleep_range(1000000, 1000010);
		if (chip->mmi_chg) {
			oplus_chg_unsuspend_charger();
			chip->chg_ops->charging_enable();
			oplus_adsp_voocphy_reset_again();
		}
	}
}

int oplus_adsp_voocphy_reset_again(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_VOOCPHY_RESET_AGAIN, true);
	if (rc) {
		chg_err("set adsp voocphy reset again fail, rc=%d\n", rc);
	} else {
		chg_err("set adsp voocphy reset again success, rc=%d\n", rc);
	}

	return rc;
}

static int oplus_get_usbin_status(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_IN_STATUS);
	if (rc) {
		bcdev->usb_in_status = false;
		chg_err("read usb_in_status fail, rc=%d\n", rc);
		return -1;
	}
	bcdev->usb_in_status = pst->prop[USB_IN_STATUS];
	chg_err("!!!usb_online[%d]\n", bcdev->usb_in_status);

	return rc;
}

#define  VOLTAGE_2000MV  2000
#define  COUNT_SIX      6
#define  COUNT_THR      3
#define  COUNT_TEN      10
#define  CHECK_CURRENT_LOW       300
#define  CHECK_CURRENT_HIGH      900
#define  VBUS_VOLT_LOW      6000



static void oplus_recheck_input_current_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	int chg_vol = 0;
	int ibus_curr = 0;
	int fast_chg_type = 0;
	static int count = 0;
	static int err_cnt = 0;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	if (!bcdev) {
		chg_err("bcdev is NULL!\n");
		return;
	}
	chg_err("reset input current count:%d\n", count);
	chg_vol = oplus_chg_get_charger_voltage();
	if (!oplus_voocphy_get_fastchg_start()
		&& !oplus_voocphy_get_fastchg_ing()) {
		ibus_curr = oplus_get_ibus_current();
	}
	if (chg_vol > VOLTAGE_2000MV) {
		count++;

		if (count > COUNT_THR && (ibus_curr > CHECK_CURRENT_LOW) && (ibus_curr < CHECK_CURRENT_HIGH)) {
			err_cnt++;
		} else {
			err_cnt = 0;
		}

		if (count > COUNT_TEN) {
			fast_chg_type = oplus_voocphy_get_fast_chg_type();
			chg_err("reset input current err_cnt: %d,chg_vol:%d,fastchg_start:%d,fastchg_ing:%d,ibus_curr:%d,fast_chg_type:%d\n",
				err_cnt, chg_vol, oplus_voocphy_get_fastchg_start(), oplus_voocphy_get_fastchg_ing(), ibus_curr, fast_chg_type);
			if (chip->charger_type != POWER_SUPPLY_TYPE_USB_DCP) {
				chg_err("reset input current chip->charger_type: %d\n", chip->charger_type);
				count = 0;
				return;
			}
			if (err_cnt > COUNT_THR) {
				chg_err("reset icl setting!\n");
				oplus_chg_input_current_recheck_work();
			}
			if (oplus_voocphy_get_fastchg_start()
						&& oplus_voocphy_get_fastchg_ing()
						&& fast_chg_type != CHARGER_SUBTYPE_FASTCHG_VOOC) {
				chg_vol = oplus_chg_get_charger_voltage();

				chg_err("reset voocphy setting!,chg_vol:%d\n", chg_vol);
				if (chg_vol < VBUS_VOLT_LOW) {
					oplus_adsp_voocphy_clear_status();
					oplus_chg_suspend_charger();
					msleep(1500);
					oplus_chg_unsuspend_charger();
					oplus_chg_set_charger_type_unknown();
					oplus_chg_wake_update_work();
				}
			}
			count = 0;
		} else {
			schedule_delayed_work(&bcdev->recheck_input_current_work, msecs_to_jiffies(2000));
		}
	} else {
		count = 0;
	}
}

int oplus_chg_wired_get_break_sub_crux_info(char *crux_info)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	pr_info("real_chg_type:%d\n", bcdev->real_chg_type);
	return bcdev->real_chg_type;
}

int oplus_abnormal_adapter_disconnect_keep(void)
{
	int ret_val = 0;
	if ((oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) &&
		(oplus_chg_get_fast_chg_type() == ADAPTER_ID_65W_0X14) &&
		oplus_is_pd_svooc() && !oplus_voocphy_get_fastchg_start())
			ret_val = 1;
	return ret_val;
}

#define MAX_VBUS_CHECK_COUNTS			4
#define VOLTAGE_800MV				800
#define GET_INFO_FROMADS_MAXIMIT		3

static void oplus_plugin_irq_work(struct work_struct *work)
{
	int rc = 0;
	int cid_status = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	static bool usb_pre_plugin_status = false, usb_plugin_status = false;
	union oplus_chg_mod_propval temp_val = {0, };
	bool status_usb_in = false;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	bcdev->plugin_already_run = true;

	rc = read_property_id(bcdev, pst, USB_IN_STATUS);
	if (rc) {
		bcdev->usb_in_status = false;
		chg_err("read usb_in_status fail, rc=%d\n", rc);
		return;
	}
	if (pst->prop[USB_IN_STATUS] > 0) {
		status_usb_in = true;
	} else {
		status_usb_in = false;
		bcdev->pd_svooc = false;
	}
	usb_plugin_status = pst->prop[USB_IN_STATUS] & 0xff;

	if (chip->support_check_usbin_status &&
	    usbin_status_test > 0) {
		status_usb_in = true;
		bcdev->usb_in_status = 1;
		usb_plugin_status = 1;
		chg_err("usbin_status_test %d\n", usbin_status_test);
	}

	cid_status = oplus_get_otg_online_status_with_cid_scheme();

	chg_err("!!!prop[%d], usb_online[%d] abnormal_adapter_info [%d %d] cid_status[%d]\n",
		pst->prop[USB_IN_STATUS], bcdev->usb_in_status,
		chip->is_abnormal_adapter, chip->support_abnormal_adapter, cid_status);
	if ((oplus_is_vooc_project() == DUAL_BATT_150W || oplus_is_vooc_project() == DUAL_BATT_240W) &&
		!status_usb_in && cid_status) {
		msleep(CID_STATUS_DELAY_MS);
		schedule_delayed_work(&bcdev->usb_type_work, msecs_to_jiffies(10));
	}
	bcdev->usb_in_status = status_usb_in;
	oplus_quirks_notify_plugin(bcdev->usb_in_status);
	oplus_adsp_voocphy_set_fastchg_start(false);

	if (chip->support_abnormal_adapter) {
		oplus_chg_check_break(usb_plugin_status);
	}

	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) {
		if (oplus_chg_get_adsp_pre_is_abnormal_adapter()) {
			bcdev->abnormal_adapter_disconnect_cnt =
				oplus_chg_get_adsp_abnormal_adapter_disconnect_cnt();
			chg_err("adspvoocphy abnormal_adapter_info [%d]",
				bcdev->abnormal_adapter_disconnect_cnt);
		}
	}

	oplus_chg_track_check_wired_charging_break(usb_plugin_status);

	if (oplus_voocphy_get_bidirect_cp_support()) {
		if (!bcdev->usb_in_status) {
			if (oplus_vooc_get_fastchg_started() == true) {
				chg_err("!!!Air charging happen,need check charger out\n");
				schedule_delayed_work(&bcdev->check_charger_out_work,
							round_jiffies_relative(msecs_to_jiffies(1500)));
			}
		}
	}

	bcdev->real_chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
/*#ifdef OPLUS_FEATURE_CHG_BASIC*/
	if (bcdev && bcdev->ctrl_lcm_frequency.work.func) {
		mod_delayed_work(system_highpri_wq, &bcdev->ctrl_lcm_frequency, 50);
	}
/*#endif*/
	if (bcdev->usb_ocm) {
		if (bcdev->usb_in_status == 1) {
			if (g_oplus_chip && g_oplus_chip->charger_type == POWER_SUPPLY_TYPE_WIRELESS)
				g_oplus_chip->charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
			oplus_chg_global_event(bcdev->usb_ocm, OPLUS_CHG_EVENT_ONLINE);
		} else {
			if ((oplus_get_wired_chg_present() == false) &&
			    (g_oplus_chip->charger_volt < CHARGER_PRESENT_VOLT_MV)) {
				bcdev->pd_svooc = false; /*remove svooc flag*/
			}
			if (is_charger_boot_mode() &&
			    g_oplus_chip->prop_status == POWER_SUPPLY_STATUS_CHARGING) {
				chg_err("clear prop_status and type\n");
				g_oplus_chip->prop_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
				g_oplus_chip->charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
			}
			oplus_chg_global_event(bcdev->usb_ocm, OPLUS_CHG_EVENT_OFFLINE);
		}
	}

	chg_err("!!!usb_pre_plugin_status[%d], usb_plugin_status[%d] bcdev->usb_in_status[%d]\n", usb_pre_plugin_status,
		usb_plugin_status, bcdev->usb_in_status);
	if (usb_pre_plugin_status != usb_plugin_status || !usb_plugin_status) {
		if (usb_plugin_status)
			oplus_chg_set_charger_type_unknown();
		oplus_chg_wake_update_work();
	}
	if (is_wls_ocm_available(chip) && usb_pre_plugin_status != usb_plugin_status) {
		(void)oplus_chg_mod_get_property(chip->wls_ocm,
			OPLUS_CHG_PROP_TRX_ONLINE, &temp_val);
		if (!!temp_val.intval) {
			temp_val.intval = false;
			oplus_chg_mod_set_property(chip->wls_ocm,
				OPLUS_CHG_PROP_TRX_ONLINE, &temp_val);
			/*for HW spec,need 100ms delay*/
			msleep(100);
			temp_val.intval = true;
			oplus_chg_mod_set_property(chip->wls_ocm,
				OPLUS_CHG_PROP_TRX_ONLINE, &temp_val);
		}
	}
	usb_pre_plugin_status = usb_plugin_status;

	if (bcdev->usb_in_status  == 1) {
		if (g_oplus_chip && g_oplus_chip->usbtemp_wq_init_finished) {
			g_oplus_chip->usbtemp_check = true;
			oplus_wake_up_usbtemp_thread();
		}
	#ifdef OPLUS_FEATURE_CHG_BASIC
		oplus_pps_set_power(OPLUS_PPS_POWER_CLR, 0, 0);
		oplus_pps_hardware_init();
		/*oplus_pps_cp_reset();*/
	#endif
	} else {
		if (g_oplus_chip) {
			g_oplus_chip->usbtemp_check = false;
			oplus_ufcs_set_third_id(false);
			if (oplus_chg_get_voocphy_support() != ADSP_VOOCPHY) {
				if (oplus_vooc_get_fastchg_started() == true &&
				    oplus_vooc_get_fastchg_dummy_started() == false &&
				    oplus_vooc_get_fastchg_to_normal() == false &&
				    oplus_vooc_get_fastchg_to_warm() == false) {      /*plug out by normal*/
					printk(KERN_ERR "[%s]: plug out normal\n", __func__);
					smbchg_set_chargerid_switch_val(0);
					oplus_ufcs_variables_reset(0);
					chip->chargerid_volt = 0;
					chip->chargerid_volt_got = false;
					chip->charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
					oplus_chg_wake_update_work();
				} else if (oplus_vooc_get_fastchg_started() == false) {
					printk(KERN_ERR "[%s]: plug out fastchg_to_normal/warm/dummy or not vooc\n", __func__);
					cancel_delayed_work_sync(&chip->update_work);
					oplus_vooc_reset_fastchg_after_usbout();
					smbchg_set_chargerid_switch_val(0);
					chip->chargerid_volt = 0;
					chip->chargerid_volt_got = false;
					chip->charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
					oplus_chg_wake_update_work();
				}
			}
		}
#ifdef OPLUS_FEATURE_CHG_BASIC
		/*oplus_pps_cp_reset();*/
		oplus_pps_set_power(OPLUS_PPS_POWER_CLR, 0, 0);
#endif

		bcdev->pd_svooc = false;
		oplus_pps_stop_disconnect();
		oplus_pps_variables_reset(true);
		bcdev->hvdcp_detach_time = cpu_clock(smp_processor_id()) / CPU_CLOCK_TIME_MS;
		chg_err("!!! %s: the hvdcp_detach_time:%lu, detect time %lu \n", __func__, bcdev->hvdcp_detach_time, bcdev->hvdcp_detect_time);
		if (bcdev->hvdcp_detach_time - bcdev->hvdcp_detect_time <= OPLUS_HVDCP_DETECT_TO_DETACH_TIME) {
			bcdev->hvdcp_disable = true;
			schedule_delayed_work(&bcdev->hvdcp_disable_work, OPLUS_HVDCP_DISABLE_INTERVAL);
		} else {
			bcdev->hvdcp_detect_ok = false;
			bcdev->hvdcp_detect_time = 0;
			bcdev->hvdcp_disable = false;
		}
		bcdev->adsp_voocphy_err_check = false;
		bcdev->pd_type_checked = false;
		cancel_delayed_work_sync(&bcdev->pd_type_check_work);
		cancel_delayed_work_sync(&bcdev->adsp_voocphy_err_work);
		if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY && bcdev->qc_enable_status != 0) {
			bcdev->qc_enable_status = 0;
			oplus_adsp_voocphy_enable(true);
		}
	}
	chg_err("!!!pd_svooc[%d]\n", bcdev->pd_svooc);
}

static void oplus_apsd_done_work(struct work_struct *work)
{
	int rc = 0;
	static int adap_type = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("!!!chip null, oplus_apsd_done_work\n");
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc) {
		adap_type = 0;
		chg_err("read usb_ADAP fail, rc=%d\n", rc);
		return;
	}
	adap_type = pst->prop[USB_ADAP_TYPE];
	chg_err("!!! usb adap type: [%d]\n", adap_type);
	oplus_chg_wake_update_work();
}

int oplus_set_otg_switch_status_default(bool enable)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_OTG_SWITCH, enable);
	if (rc) {
		chg_err("oplus_set_otg_switch_status fail, rc=%d\n", rc);
	} else {
		chg_err("oplus_set_otg_switch_status, rc=%d\n", rc);
	}

	return rc;
}

int oplus_set_otg_switch_status(bool enable)
{
	int rc = 0;
	int level = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	/*boot-up with newman OTG connected, android will set persist.sys.oplus.otg_support, so...*/
	if (get_otg_scheme(chip) == OTG_SCHEME_CCDETECT_GPIO) {
		level = gpio_get_value(bcdev->oplus_custom_gpio.ccdetect_gpio);
		if (level != 1) {
			chg_err("[OPLUS_CHG][%s]: gpio[%s], should set, return\n", __func__, level ? "H" : "L");
			return rc;
		}
	} else {
		rc = oplus_set_otg_switch_status_default(enable);
		return rc;
	}

	chip->otg_switch = !!enable;
	if (enable) {
		oplus_ccdetect_enable();
	} else {
		oplus_ccdetect_disable();
	}
	chg_err("[OPLUS_CHG][%s]: otg_switch=%d, otg_online=%d\n",
		__func__, chip->otg_switch, chip->otg_online);

	return rc;
}

#define DISCONNECT						0
#define STANDARD_TYPEC_DEV_CONNECT	BIT(0)
#define OTG_DEV_CONNECT				BIT(1)
int oplus_get_otg_online_status_with_cid_scheme(void)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;
	int rc = 0;
	int cid_status = 0;
	int online = 0;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return 0;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_CID_STATUS);
	if (rc < 0) {
		chg_err("!!!read cid_status fail\n");
		return 0;
	}
	cid_status = pst->prop[USB_CID_STATUS];
	bcdev->cid_status = cid_status;

	online = (cid_status == 1) ? STANDARD_TYPEC_DEV_CONNECT : DISCONNECT;

	return online;
}

static int oplus_get_otg_online_with_switch_scheme(void)
{
	int online = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;
	int rc = 0;

	if (!chip) {
		chg_err("chip is NULL\n");
		return 0;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_TYPEC_MODE);
	if (rc < 0) {
		chg_err("!!!%s: read typec_mode fail\n", __func__);
		return 0;
	}
	online = (pst->prop[USB_TYPEC_MODE] == 1) ? 1 : 0;

	return online;
}

int oplus_get_otg_online_status(void)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;
	int rc = 0;
	int cid_status = 0;
	int online = 0;
	int typec_otg = 0;
	static int pre_cid_status = 1;
	static int pre_level = 1;
	static int pre_typec_otg = 0;
	int level = 0, otg_scheme;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return 0;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	otg_scheme = get_otg_scheme(chip);

	if (otg_scheme == OTG_SCHEME_CID
		&& bcdev->cid_status == 0
		&& chip->otg_switch == false) {
		chip->otg_online = 0;
		return 0;
	}

	if (otg_scheme == OTG_SCHEME_CCDETECT_GPIO) {
		level = gpio_get_value(bcdev->oplus_custom_gpio.ccdetect_gpio);
		if (level != gpio_get_value(bcdev->oplus_custom_gpio.ccdetect_gpio)) {
			chg_err("[OPLUS_CHG][%s]: ccdetect_gpio is unstable, try again...\n", __func__);
			usleep_range(5000, 5100);
			level = gpio_get_value(bcdev->oplus_custom_gpio.ccdetect_gpio);
		}
		online = (level == 1) ? DISCONNECT : STANDARD_TYPEC_DEV_CONNECT;
	} else if (otg_scheme == OTG_SCHEME_CID) {
		online = oplus_get_otg_online_status_with_cid_scheme();
	} else {
		online = oplus_get_otg_online_with_switch_scheme();
	}

	rc = read_property_id(bcdev, pst, USB_TYPEC_MODE);
	if (rc < 0) {
		chg_err("!!!%s: read typec_mode fail\n", __func__);
		return 0;
	}
	typec_otg = pst->prop[USB_TYPEC_MODE];

	online = online | ((typec_otg == 1) ? OTG_DEV_CONNECT : DISCONNECT);

	if ((pre_cid_status ^ cid_status) || (pre_typec_otg ^ typec_otg) || (pre_level ^ level)) {
		pre_cid_status = cid_status;
		pre_typec_otg = typec_otg;
		pre_level = level;
		chg_err("[OPLUS_CHG][%s]: level[%d], cid_status[%d], typec_otg[%d], otg_online[%d]\n",
				__func__, level, cid_status, typec_otg, online);
	}
	chip->otg_online = typec_otg;

	return online;
}

static int oplus_otg_ap_enable(bool enable)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_OTG_AP_ENABLE, enable);
	if (rc) {
		chg_err("oplus_otg_ap_enable fail, rc=%d\n", rc);
	} else {
		chg_err("oplus_otg_ap_enable, rc=%d\n", rc);
	}
	oplus_get_otg_online_status_with_cid_scheme();
	if (bcdev->cid_status != 0) {
		chg_err("Oplus_otg_ap_enable,flag bcdev->cid_status != 0\n");
		oplus_ccdetect_enable();
	}

	return rc;
}

int sm8550_get_ccdetect_online(void)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	int online = 0;
	int level = 0, otg_scheme;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return 0;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;

	otg_scheme = get_otg_scheme(chip);


	if (otg_scheme == OTG_SCHEME_CCDETECT_GPIO) {
		level = gpio_get_value(bcdev->oplus_custom_gpio.ccdetect_gpio);
		if (level != gpio_get_value(bcdev->oplus_custom_gpio.ccdetect_gpio)) {
			printk(KERN_ERR "[OPLUS_CHG][%s]: ccdetect_gpio is unstable, try again...\n", __func__);
			usleep_range(5000, 5100);
			level = gpio_get_value(bcdev->oplus_custom_gpio.ccdetect_gpio);
		}
		online = (level == 1) ? DISCONNECT : STANDARD_TYPEC_DEV_CONNECT;
	} else if (otg_scheme == OTG_SCHEME_CID) {
		online = oplus_get_otg_online_status_with_cid_scheme();
	} else {
		online = oplus_get_otg_online_with_switch_scheme();
	}
	return online;
}

static int smbchg_charging_disable(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	mutex_lock(&bcdev->chg_en_lock);
	rc = write_property_id(bcdev, pst, BATT_CHG_EN, 0);
	if (rc) {
		chg_err("set disable charging fail, rc=%d\n", rc);
	} else {
		bcdev->chg_en = false;
		chg_err("set disable charging sucess.\n");
	}
	mutex_unlock(&bcdev->chg_en_lock);

	return rc;
}

static int smbchg_get_charge_enable(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	mutex_lock(&bcdev->chg_en_lock);
	rc = bcdev->chg_en;
	mutex_unlock(&bcdev->chg_en_lock);

	return rc;
}

static int smbchg_usb_suspend_enable(void)
{
	int rc = 0;
	int prop_id = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);
	rc = write_property_id(bcdev, pst, prop_id, 0);
	if (rc)
		chg_err("set suspend fail, rc=%d\n", rc);
	else
		chg_err("set chg suspend\n");

	return rc;
}

static int smbchg_usb_suspend_disable(void)
{
	int rc = 0;
	int prop_id = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);
	rc = write_property_id(bcdev, pst, prop_id, 0xFFFFFFFF);
	if (rc)
		chg_err("set unsuspend to fail, rc=%d\n", rc);
	else
		chg_err("set chg unsuspend\n");

	return rc;
}

static bool smbchg_usb_check_suspend_charger(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_SUSPEND_PMIC);
	if (rc)
		chg_err("read chg suspend status fail, rc=%d\n", rc);
	else
		chg_err("read chg suspend status[%d]\n", pst->prop[USB_SUSPEND_PMIC]);

	return pst->prop[USB_SUSPEND_PMIC];
}

static int oplus_chg_hw_init(void)
{
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	int boot_mode = get_boot_mode();

	if (boot_mode != MSM_BOOT_MODE__RF && boot_mode != MSM_BOOT_MODE__WLAN) {
		smbchg_usb_suspend_disable();
	} else {
		smbchg_usb_suspend_enable();
	}
	chg_err("boot_mode:%d\n", boot_mode);
#else
	smbchg_usb_suspend_disable();
#endif
	oplus_chg_set_input_current_with_no_aicl(500);
	smbchg_charging_enable();

	return 0;
}

static int smbchg_set_rechg_vol(int rechg_vol)
{
	return 0;
}

static int smbchg_reset_charger(void)
{
	return 0;
}

static int smbchg_read_full(void)
{
/*#if 0
	int rc = 0;
	u8 stat = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!oplus_chg_is_usb_present())
		return 0;

	rc = smblib_read(&chip->pmic_spmi.smb5_chip->chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		chg_err("Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n", rc);
		return 0;
	}
	stat = stat & BATTERY_CHARGER_STATUS_MASK;

	if (stat == TERMINATE_CHARGE || stat == INHIBIT_CHARGE)
		return 1;
#endif*/
	return 0;
}

static int smbchg_otg_enable(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	chg_err("smbchg_otg_enable start!\n");
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = write_property_id(bcdev, pst, USB_OTG_VBUS_REGULATOR_ENABLE, 1);
	if (rc) {
		chg_err("smbchg_otg_enable fail, rc=%d\n", rc);
		return rc;
	}
	schedule_delayed_work(&bcdev->otg_status_check_work, 0);
	chg_err("smbchg_otg_enable sucess!\n");
	return rc;
}

static int smbchg_otg_disable(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	chg_err("smbchg_otg_disable start!\n");
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = write_property_id(bcdev, pst, USB_OTG_VBUS_REGULATOR_ENABLE, 0);
	if (rc) {
		chg_err("smbchg_otg_disable fail, rc=%d\n", rc);
		return rc;
	}
	chg_err("smbchg_otg_disable sucess!\n");
	return rc;
}

static int oplus_set_chging_term_disable(void)
{
	return 0;
}

static bool qcom_check_charger_resume(void)
{
	return true;
}

int smbchg_get_chargerid_volt(void)
{
	return 0;
}

void smbchg_set_chargerid_switch_val(int value)
{
	struct battery_chg_dev *bcdev = NULL;

	if (!g_oplus_chip) {
		chg_err("fail to init oplus_chip\n");
		return;
	}
	bcdev = g_oplus_chip->pmic_spmi.bcdev_chip;

	if (g_oplus_chip->normalchg_gpio.chargerid_switch_gpio < 0) {
		chg_err("miss chargerid_switch_gpio\n");
		return;
	}

	if (IS_ERR_OR_NULL(g_oplus_chip->normalchg_gpio.pinctrl) ||
	    IS_ERR_OR_NULL(g_oplus_chip->normalchg_gpio.chargerid_switch_active) ||
	    IS_ERR_OR_NULL(g_oplus_chip->normalchg_gpio.chargerid_switch_sleep)) {
		chg_err("pinctrl or active or sleep null!\n");
		return;
	}

	if (oplus_vooc_get_adapter_update_real_status() == ADAPTER_FW_NEED_UPDATE
		|| oplus_vooc_get_btb_temp_over() == true) {
		chg_err("adapter update or btb_temp_over, return\n");
		return;
	}

	mutex_lock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));
	if (value) {
		gpio_direction_output(g_oplus_chip->normalchg_gpio.chargerid_switch_gpio, 1);
		pinctrl_select_state(g_oplus_chip->normalchg_gpio.pinctrl,
				g_oplus_chip->normalchg_gpio.chargerid_switch_active);
	} else {
		gpio_direction_output(g_oplus_chip->normalchg_gpio.chargerid_switch_gpio, 0);
		pinctrl_select_state(g_oplus_chip->normalchg_gpio.pinctrl,
				g_oplus_chip->normalchg_gpio.chargerid_switch_sleep);
	}
	mutex_unlock(&(bcdev->oplus_custom_gpio.pinctrl_mutex));

	chg_err("set usb_switch_1 = %d, result = %d\n", value, smbchg_get_chargerid_switch_val());
	return;
}

int smbchg_get_chargerid_switch_val(void)
{
	if (!g_oplus_chip) {
		chg_err("fail to init oplus_chip\n");
		return 0;
	}
	if (g_oplus_chip->normalchg_gpio.chargerid_switch_gpio < 0) {
		chg_err("miss chargerid_switch_gpio\n");
		return -1;
	}
	return gpio_get_value(g_oplus_chip->normalchg_gpio.chargerid_switch_gpio);
}

static bool smbchg_need_to_check_ibatt(void)
{
	return false;
}

static int smbchg_get_chg_current_step(void)
{
	return 25;
}

int opchg_get_charger_type(void)
{
	int rc = 0;
	int prop_id = 0;
	static int charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return POWER_SUPPLY_TYPE_UNKNOWN;
	}

	if (!chip->charger_exist) {
		return POWER_SUPPLY_TYPE_UNKNOWN;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_USB_TYPE);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read usb charger_type fail, rc=%d\n", rc);
		goto get_type_done;
	}
	switch (pst->prop[prop_id]) {
	case POWER_SUPPLY_USB_TYPE_UNKNOWN:
		charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case POWER_SUPPLY_USB_TYPE_SDP:
		charger_type = POWER_SUPPLY_TYPE_USB;
		break;
	case POWER_SUPPLY_USB_TYPE_CDP:
		charger_type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case POWER_SUPPLY_USB_TYPE_DCP:
		charger_type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case POWER_SUPPLY_USB_TYPE_PD_SDP:
		charger_type = POWER_SUPPLY_TYPE_USB_PD_SDP;
		break;
	default:
		charger_type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	}

get_type_done:
	if (bcdev->pd_svooc == true) {
		charger_type = POWER_SUPPLY_TYPE_USB_DCP;
	}

	if (chip && chip->wireless_support &&
			(oplus_wpc_get_wireless_charge_start() == true || oplus_chg_is_wls_present()))
		charger_type = POWER_SUPPLY_TYPE_WIRELESS;
	return charger_type;
}

int qpnp_get_prop_charger_voltage_now(void)
{
	int rc = 0;
	int prop_id = 0;
	static int vbus_volt = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	union oplus_chg_mod_propval pval = {0};

	if (!chip) {
		return 0;
	}

	if (oplus_chg_is_wls_present()) {
		rc = oplus_chg_mod_get_property(chip->wls_ocm, OPLUS_CHG_PROP_VOLTAGE_NOW, &pval);
		if (rc >= 0) {
			return pval.intval;
		}
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	if (!chip->charger_exist) {
		return 0;
	}

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_VOLTAGE_NOW);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read usb vbus_volt fail, rc=%d\n", rc);
		return vbus_volt;
	}
	vbus_volt = pst->prop[prop_id] / 1000;

	return vbus_volt;
}

static int oplus_get_ibus_current(void)
{
	int rc = 0;
	int prop_id = 0;
	static int ibus = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_CURRENT_NOW);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read battery curr fail, rc=%d\n", rc);
		return ibus;
	}
	ibus = DIV_ROUND_CLOSEST((int)pst->prop[prop_id], 1000);

	return ibus;
}

int oplus_adsp_batt_curve_current(void)
{
	int rc;
	static int batt_current = 0;
	struct battery_chg_dev *bcdev;
	struct psy_state *pst;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -ENODEV;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_GET_BATT_CURR);
	if (rc < 0) {
		chg_err("read battery curr fail, rc=%d\n", rc);
		return batt_current * 100;
	}
	batt_current = DIV_ROUND_CLOSEST((int)pst->prop[USB_GET_BATT_CURR], 1000);
	chg_debug("get batt_curr = %d \n", batt_current);
	return batt_current * 100;
}

int oplus_adsp_voocphy_get_fast_chg_type(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	int fast_chg_type = 0;

	if (!chip) {
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_VOOC_FAST_CHG_TYPE);
	if (rc < 0) {
		chg_err("read vooc_fast_chg_type fail, rc=%d\n", rc);
		return 0;
	}
	fast_chg_type = (pst->prop[USB_VOOC_FAST_CHG_TYPE]) & 0x7F;

	return fast_chg_type;
}

static int oplus_get_usb_icl(void)
{
	int rc = 0;
	int prop_id = 0;
	static int usb_icl = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read usb icl fail, rc=%d\n", rc);
		return usb_icl;
	}
	usb_icl = DIV_ROUND_CLOSEST((int)pst->prop[prop_id], 1000);

	if(usb_icl <= 0)
		return 500;

	return usb_icl;
}

static void oplus_check_abnormal_usbin_status_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	if (!chip) {
		chg_err("chip is NULL!\n");
		return;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;

	if (bcdev->usb_in_status)
		chip->charger_volt = chip->chg_ops->get_charger_volt();
	if (bcdev->usb_in_status &&
	    oplus_vooc_get_fastchg_started() == false &&
	    !chip->usbin_abnormal_status &&
	    chip->charger_volt <= VOLTAGE_800MV &&
	    chip->usb_present_vbus0_count <= MAX_VBUS_CHECK_COUNTS) {
		chip->usb_present_vbus0_count++;

		if (chip->usb_present_vbus0_count == MAX_VBUS_CHECK_COUNTS ||
		    !bcdev->cid_status) {
			chip->usbin_abnormal_status = true;
			chip->check_usbin_from_adsp_cnt = 0;
			chip->batt_full = false;
			oplus_adsp_voocphy_clear_status();
			chg_err("abnormal abnormal_status %d present_vbus0_count %d\n",
				chip->usbin_abnormal_status,
				chip->usb_present_vbus0_count);
		}
	}

	if ((!bcdev->usb_in_status) ||
	    (bcdev->usb_in_status && chip->charger_volt > VOLTAGE_800MV)) {
		chip->usbin_abnormal_status = false;
		chip->usb_present_vbus0_count = 0;
		chip->check_usbin_from_adsp_cnt = 0;
	}

	if (bcdev->usb_in_status && chip->usbin_abnormal_status &&
	    chip->charger_volt <= VOLTAGE_800MV &&
	    chip->check_usbin_from_adsp_cnt <= GET_INFO_FROMADS_MAXIMIT) {
		usbin_status_test = 0;
		chip->check_usbin_from_adsp_cnt++;
		schedule_delayed_work(&bcdev->plugin_irq_work, msecs_to_jiffies(3000));
		chg_err("from_adsp_cnt %d\n",
			chip->check_usbin_from_adsp_cnt);
	}

	chg_err("[%d  %d  %d  %d  %d]\n",
		chip->usbin_abnormal_status, chip->usb_present_vbus0_count,
		chip->check_usbin_from_adsp_cnt, bcdev->usb_in_status,
		chip->charger_volt);
}

bool oplus_chg_is_usb_present(void)
{
	bool vbus_rising = false;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	vbus_rising = bcdev->usb_in_status;

	if (oplus_vchg_trig_is_support() == true
			&& oplus_get_vchg_trig_status() == 1 && vbus_rising == true) {
		/*chg_err("vchg_trig is high, vbus: 0\n");*/
		vbus_rising = false;
	}

	if (vbus_rising == false && (oplus_wpc_get_wireless_charge_start() || oplus_chg_is_wls_present())) {
		chg_err("USBIN_PLUGIN_RT_STS_BIT low but wpc has started\n");
		vbus_rising = true;
	}

	if ((oplus_chg_get_voocphy_support() != ADSP_VOOCPHY) &&
	    vbus_rising == false && oplus_vooc_get_fastchg_started() == true) {
		chg_err("USBIN_PLUGIN_RT_STS_BIT low but fastchg started true and chg vol > 2V\n");
		vbus_rising = true;
	}

	if (chip->support_check_usbin_status) {
		schedule_delayed_work(&bcdev->check_abnormal_usbin_status, 0);
	}
	return vbus_rising;
}

int qpnp_get_battery_voltage(void)
{
	return 3800;/*Not use anymore*/
}

/*#if 0
static int get_boot_mode(void)
{
	return 0;
}
#endif*/

int smbchg_get_boot_reason(void)
{
	return 0;
}

int oplus_chg_get_shutdown_soc(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	rc = read_property_id(bcdev, pst, BATT_RTC_SOC);
	if (rc < 0) {
		chg_err("read battery rtc soc fail, rc=%d\n", rc);
		return 0;
	}
	chg_err("read battery rtc soc success, rtc_soc=%d\n", pst->prop[BATT_RTC_SOC]);

	return pst->prop[BATT_RTC_SOC];
}

int oplus_chg_backup_soc(int backup_soc)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return 0;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	rc = write_property_id(bcdev, pst, BATT_RTC_SOC, backup_soc);
	if (rc) {
		chg_err("set battery rtc soc fail, rc=%d\n", rc);
		return 0;
	}
	chg_err("write battery rtc soc success, rtc_soc=%d\n", backup_soc);

	return 0;
}

static int smbchg_get_aicl_level_ma(void)
{
	return 0;
}

static void smbchg_rerun_aicl(void)
{
	/*smbchg_aicl_enable(false);*/
	/* Add a delay so that AICL successfully clears */
	/*msleep(50);*/
	/*smbchg_aicl_enable(true);*/
}

#ifdef CONFIG_OPLUS_RTC_DET_SUPPORT
static int rtc_reset_check(void)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc = 0;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return 0;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Error reading rtc device (%s) : %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	if ((tm.tm_year == 70) && (tm.tm_mon == 0) && (tm.tm_mday <= 1)) {
		chg_debug(": Sec: %d, Min: %d, Hour: %d, Day: %d, Mon: %d, Year: %d  @@@ wday: %d, yday: %d, isdst: %d\n",
			tm.tm_sec, tm.tm_min, tm.tm_hour, tm.tm_mday, tm.tm_mon, tm.tm_year,
			tm.tm_wday, tm.tm_yday, tm.tm_isdst);
		rtc_class_close(rtc);
		return 1;
	}

	chg_debug(": Sec: %d, Min: %d, Hour: %d, Day: %d, Mon: %d, Year: %d  ###  wday: %d, yday: %d, isdst: %d\n",
		tm.tm_sec, tm.tm_min, tm.tm_hour, tm.tm_mday, tm.tm_mon, tm.tm_year,
		tm.tm_wday, tm.tm_yday, tm.tm_isdst);

close_time:
	rtc_class_close(rtc);
	return 0;
}
#endif /* CONFIG_OPLUS_RTC_DET_SUPPORT */

#ifdef CONFIG_OPLUS_SHORT_C_BATT_CHECK
/* This function is getting the dynamic aicl result/input limited in mA.
 * If charger was suspended, it must return 0(mA).
 * It meets the requirements in SDM660 platform.
 */
static int oplus_chg_get_dyna_aicl_result(void)
{
	struct power_supply *usb_psy = NULL;
	union power_supply_propval pval = {0, };

	usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		power_supply_get_property(usb_psy,
				POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
				&pval);
		return pval.intval / 1000;
	}

	return 1000;
}
#endif /* CONFIG_OPLUS_SHORT_C_BATT_CHECK */

int oplus_chg_get_charger_subtype(void)
{
	int rc = 0;
	int prop_id = 0;
	static int charg_subtype = CHARGER_SUBTYPE_DEFAULT;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return CHARGER_SUBTYPE_DEFAULT;
	}

	if (!chip->charger_exist) {
		return CHARGER_SUBTYPE_DEFAULT;
	}

	charg_subtype = oplus_ufcs_get_fastchg_type();
	if (charg_subtype != CHARGER_SUBTYPE_DEFAULT) {
		return CHARGER_SUBTYPE_UFCS;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_USB_TYPE);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read charger type fail, rc=%d\n", rc);
		if (!chip->charger_exist)
			charg_subtype = CHARGER_SUBTYPE_DEFAULT;
		return charg_subtype;
	}
	switch (pst->prop[prop_id]) {
	case POWER_SUPPLY_USB_TYPE_PD:
	case POWER_SUPPLY_USB_TYPE_PD_DRP:
		charg_subtype = CHARGER_SUBTYPE_PD;
		break;
	case POWER_SUPPLY_USB_TYPE_PD_PPS:
		charg_subtype = CHARGER_SUBTYPE_PD;
		if (oplus_pps_get_chg_status() != PPS_NOT_SUPPORT &&
		    oplus_get_pps_type() == true &&
		    (chip->pd_svooc || oplus_pps_check_third_pps_support()))
			charg_subtype = CHARGER_SUBTYPE_PPS;
		break;
	default:
		charg_subtype = CHARGER_SUBTYPE_DEFAULT;
		break;
	}

	if (charg_subtype == CHARGER_SUBTYPE_DEFAULT) {
		rc = read_property_id(bcdev, pst, USB_ADAP_SUBTYPE);
		if (rc < 0) {
			chg_err("read charger subtype fail, rc=%d\n", rc);
			if (!chip->charger_exist)
				charg_subtype = CHARGER_SUBTYPE_DEFAULT;
			return charg_subtype;
		}
		switch (pst->prop[USB_ADAP_SUBTYPE]) {
		case CHARGER_SUBTYPE_FASTCHG_VOOC:
			charg_subtype = CHARGER_SUBTYPE_FASTCHG_VOOC;
			break;
		case CHARGER_SUBTYPE_FASTCHG_SVOOC:
			charg_subtype = CHARGER_SUBTYPE_FASTCHG_SVOOC;
			break;
		case CHARGER_SUBTYPE_QC:
			charg_subtype = CHARGER_SUBTYPE_QC;
			break;
		default:
			charg_subtype = CHARGER_SUBTYPE_DEFAULT;
			break;
		}
	}

	return charg_subtype;
}

int oplus_sm8150_get_pd_type(void)
{
	int rc = 0;
	int prop_id = 0;
	static int is_pd_type = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}

	if (!chip->charger_exist) {
		return false;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_USB_TYPE);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read usb pd_type fail, rc=%d\n", rc);
		if (!chip->charger_exist)
			is_pd_type = 0;
		return is_pd_type;
	}
	chg_err("oplus_sm8150_get_pd_type, pst->prop[prop_id]=%d\n", pst->prop[prop_id]);
	switch (pst->prop[prop_id]) {
	case POWER_SUPPLY_USB_TYPE_PD:
	case POWER_SUPPLY_USB_TYPE_PD_DRP:
		is_pd_type = PD_ACTIVE;
		break;
	case POWER_SUPPLY_USB_TYPE_PD_PPS:
		if (oplus_pps_get_chg_status() != PPS_NOT_SUPPORT &&
		    oplus_get_pps_type() == true) {
			is_pd_type = PD_PPS_ACTIVE;
		} else {
			is_pd_type = PD_ACTIVE;
		}
		break;
	default:
		is_pd_type = 0;
		break;
	}

	return is_pd_type;
}

bool oplus_get_pps_type(void)
{
	int rc = 0;
	bool is_pps_type = false;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_GET_PPS_TYPE);
	if (rc < 0) {
		chg_err("read usb pps_type fail, rc=%d\n", rc);
		if (!chip->charger_exist)
			is_pps_type = false;
		return is_pps_type;
	}

	is_pps_type = pst->prop[USB_GET_PPS_TYPE];
	chg_err("%s: %d\n", __func__, is_pps_type);
	return is_pps_type;
}

int oplus_chg_get_r_cool_down(void) {
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_PPS_GET_R_COOL_DOWN);
	if (rc < 0) {
		chg_err(" fail, rc = %d\n", rc);
		return -1;
	}
	chg_err("cool_down = %d\n", pst->prop[USB_PPS_GET_R_COOL_DOWN]);

	return pst->prop[USB_PPS_GET_R_COOL_DOWN];
}

u32 oplus_chg_get_pps_status(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}
	chg_err("%s\n", __func__);

	oplus_chg_get_r_cool_down();

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_GET_PPS_STATUS);
	if (rc < 0) {
		chg_err("get pps status fail, rc = %d\n", rc);
		return -1;
	}

	chg_err("PPS status = %d\n", pst->prop[USB_GET_PPS_STATUS]);

	return pst->prop[USB_GET_PPS_STATUS];
}

int oplus_check_cc_mode(void) {
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;

	if (!chip) {
		chg_err("[OPLUS_CHG][%s]: chip not ready!\n", __func__);
		return MODE_DEFAULT;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_TYPEC_MODE);
	if (rc < 0) {
		chg_err("[OPLUS_CHG][%s]: 111 Couldn't read 0x2b44 rc=%d\n", __func__, rc);
		return MODE_DEFAULT;
	}

	chg_err("[OPLUS_CHG][%s]:111 reg0x2b44[0x%x]\n", __func__, pst->prop[USB_TYPEC_MODE]);

	if (pst->prop[USB_TYPEC_MODE] == 0)
		return MODE_SINK;
	else
		return MODE_SRC;
}

int oplus_chg_set_pps_config(int vbus_mv, int ibus_ma)
{
	int rc1, rc2 = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}
	chg_err("%s\n", __func__);
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc1 = write_property_id(bcdev, pst, USB_SET_PPS_VOLT, vbus_mv);
	rc2 = write_property_id(bcdev, pst, USB_SET_PPS_CURR, ibus_ma);
	if (rc1 < 0 || rc2 < 0) {
		chg_err("set pps config fail, rc1,rc2 = %d, %d\n", rc1, rc2);
		return -1;
	}

	return 0;
}

int oplus_chg_pps_get_max_cur(int vbus_mv)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}
	chg_err("%s\n", __func__);
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_GET_PPS_MAX_CURR, vbus_mv);
	if (rc < 0) {
		chg_err("set pps vbus fail, rc = %d\n", rc);
		return -1;
	}

	rc = read_property_id(bcdev, pst, USB_GET_PPS_MAX_CURR);
	if (rc < 0) {
		chg_err("get pps max cur fail, rc = %d\n", rc);
		return -1;
	}

	chg_err("PPS max curr = %d, %d\n", vbus_mv, pst->prop[USB_GET_PPS_MAX_CURR]);

	return pst->prop[USB_GET_PPS_MAX_CURR];
}

int  oplus_pps_pd_exit(void)
{
	msleep(100);
	oplus_chg_set_pps_config(5000, 3000);
	return 0;
}

extern  int oplus_get_vbatt_pdqc_to_9v_thr(void);
#define PDQC_5V	5000
#define PDQC_9V	9000
#define HIGH_CHARGER_VOL_THR	7500
#define VBATT_PDQC_TO_9V_THR	4100
int oplus_chg_set_pd_config(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	int vbatt_pdqc_to_9v_thr_dt = oplus_get_vbatt_pdqc_to_9v_thr();

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (!is_ext_chg_ops() && (vbatt_pdqc_to_9v_thr_dt > VBATT_PDQC_TO_9V_THR)) {
		chip->limits.vbatt_pdqc_to_9v_thr = vbatt_pdqc_to_9v_thr_dt;
	}

	if ((chip->limits.vbatt_pdqc_to_5v_thr > 0 && chip->charger_volt > HIGH_CHARGER_VOL_THR && chip->batt_volt > chip->limits.vbatt_pdqc_to_5v_thr) ||
	    (chip->limits.tbatt_pdqc_to_5v_thr > 0 && chip->temperature > chip->limits.tbatt_pdqc_to_5v_thr) ||
	    chip->cool_down_force_5v) {
		chip->chg_ops->input_current_write(500);
		if(is_ext_chg_ops())
			oplus_chg_suspend_charger();
		oplus_chg_config_charger_vsys_threshold(0x03);/*set Vsys Skip threshold 101%*/
		rc = write_property_id(bcdev, pst, BATT_SET_PDO, PDQC_5V);
		if (rc)
			chg_err("set PDO 5V fail, rc=%d\n", rc);
		else
			chg_err("set PDO 5V OK\n");
		msleep(300);
		if(is_ext_chg_ops())
			oplus_chg_unsuspend_charger();
	} else if (chip->batt_volt < chip->limits.vbatt_pdqc_to_9v_thr) {
		if (oplus_chg_get_voocphy_support() == AP_SINGLE_CP_VOOCPHY ||
		    oplus_chg_get_voocphy_support() == AP_DUAL_CP_VOOCPHY) {
			oplus_voocphy_set_pdqc_config();
		}
		if(is_ext_chg_ops())
			oplus_chg_suspend_charger();
		oplus_chg_config_charger_vsys_threshold(0x02);/*set Vsys Skip threshold 104%*/
		oplus_chg_enable_burst_mode(false);
		rc = write_property_id(bcdev, pst, BATT_SET_PDO, PDQC_9V);
		if (rc)
			chg_err("set PDO 9V fail, rc=%d\n", rc);
		else
			chg_err("set PDO 9V OK\n");
		msleep(300);
		if(is_ext_chg_ops())
			oplus_chg_unsuspend_charger();
	}

	return rc;
}

int oplus_chg_set_pd_5v(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if(is_ext_chg_ops()) {
		chip->chg_ops->input_current_write(500);
		oplus_chg_suspend_charger();
		oplus_chg_config_charger_vsys_threshold(0x03);/*set Vsys Skip threshold 101%*/
	}
	rc = write_property_id(bcdev, pst, BATT_SET_PDO, PDQC_5V);
	if (rc)
		chg_err("set PDO 5V fail, rc=%d\n", rc);
	else
		chg_err("set PDO 5V OK\n");

	if(is_ext_chg_ops()) {
		msleep(300);
		oplus_chg_unsuspend_charger();
	}

	return rc;
}

#define NON_STANDARD_QC_VOL_THR	8000
int oplus_chg_set_qc_config(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	int vbatt_pdqc_to_9v_thr_dt = oplus_get_vbatt_pdqc_to_9v_thr();

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (!is_ext_chg_ops() && (vbatt_pdqc_to_9v_thr_dt > VBATT_PDQC_TO_9V_THR)) {
		chip->limits.vbatt_pdqc_to_9v_thr = vbatt_pdqc_to_9v_thr_dt;
	}

	if ((chip->limits.vbatt_pdqc_to_5v_thr > 0 && chip->charger_volt > HIGH_CHARGER_VOL_THR && chip->batt_volt > chip->limits.vbatt_pdqc_to_5v_thr) ||
	    (chip->limits.tbatt_pdqc_to_5v_thr > 0 && chip->temperature > chip->limits.tbatt_pdqc_to_5v_thr) ||
	    chip->cool_down_force_5v) {
		chip->chg_ops->input_current_write(500);
		if(is_ext_chg_ops())
			oplus_chg_suspend_charger();
		oplus_chg_config_charger_vsys_threshold(0x03);/*set Vsys Skip threshold 101%*/
		rc = write_property_id(bcdev, pst, BATT_SET_QC, PDQC_5V);
		if (rc)
			chg_err("set QC 5V fail, rc=%d\n", rc);
		else
			chg_err("set QC 5V OK\n");
		msleep(400);
		if(is_ext_chg_ops())
			oplus_chg_unsuspend_charger();
	} else if (chip->batt_volt < chip->limits.vbatt_pdqc_to_9v_thr) {
		if (oplus_chg_get_voocphy_support() == AP_SINGLE_CP_VOOCPHY ||
		    oplus_chg_get_voocphy_support() == AP_DUAL_CP_VOOCPHY) {
			oplus_voocphy_set_pdqc_config();
		}
		if(is_ext_chg_ops())
			oplus_chg_suspend_charger();
		oplus_chg_config_charger_vsys_threshold(0x02);/*set Vsys Skip threshold 104%*/
		oplus_chg_enable_burst_mode(false);
		rc = write_property_id(bcdev, pst, BATT_SET_QC, PDQC_9V);
		if (rc)
			chg_err("set QC 9V fail, rc=%d\n", rc);
		else
			chg_err("set QC 9V OK\n");
		msleep(300);
		oplus_chg_update_voltage();
		msleep(50);
		if (oplus_chg_get_charger_voltage() < NON_STANDARD_QC_VOL_THR) {
			chg_err("Non-standard QC-liked adapter detected,unabled to request 9V,falls back to 5V");
			rc = write_property_id(bcdev, pst, BATT_SET_QC, PDQC_5V);
				if (rc)
					chg_err("Fall back to QC 5V fail, rc=%d\n", rc);
				else
					chg_err("Fall back to QC 5V OK\n");
		}
		if(is_ext_chg_ops())
			oplus_chg_unsuspend_charger();
	}

	return rc;
}

static int oplus_chg_set_pdo_5v(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	rc = write_property_id(bcdev, pst, BATT_SET_QC, PDQC_5V);
	if (rc)
		chg_err("set QC 5V fail, rc=%d\n", rc);
	else
		chg_err("set QC 5V OK\n");

	return rc;
}

int oplus_chg_enable_qc_detect(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	chg_err("set bcdev->hvdcp_disable %d\n", bcdev->hvdcp_disable);

	if (bcdev->hvdcp_disable == true) {
		chg_err("hvdcp_disable!\n");
		return -1;
	}
	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) {
		bcdev->qc_enable_status = 1;
		chg_err("disable adsp voocphy!\n");
		oplus_adsp_voocphy_enable(false);
		oplus_chg_suspend_charger();
		msleep(1500);
		oplus_chg_unsuspend_charger();
		msleep(500);
	}
	rc = write_property_id(bcdev, pst, BATT_SET_QC, 0);
	bcdev->hvdcp_detect_time = cpu_clock(smp_processor_id()) / CPU_CLOCK_TIME_MS;
	chg_err(" HVDCP2 detect: %d, the detect time: %lu\n",
		bcdev->hvdcp_detect_ok, bcdev->hvdcp_detect_time);

	return rc;
}

#endif /* OPLUS_FEATURE_CHG_BASIC */

#ifdef OPLUS_FEATURE_CHG_BASIC
static int oplus_input_current_limit_ctrl_by_vooc_write(int current_ma)
{
	int rc = 0;
	int cur_usb_icl = 0;
	int temp_curr = 0;

	cur_usb_icl = oplus_get_usb_icl();
	chg_err(" get cur_usb_icl = %d\n", cur_usb_icl);

	if (current_ma > cur_usb_icl) {
		for (temp_curr = cur_usb_icl; temp_curr < current_ma; temp_curr += 500) {
			msleep(35);
			rc = oplus_chg_set_input_current_with_no_aicl(temp_curr);
			chg_err("[up] set input_current = %d\n", temp_curr);
		}
	} else {
		for (temp_curr = cur_usb_icl; temp_curr > current_ma; temp_curr -= 500) {
			msleep(35);
			rc = oplus_chg_set_input_current_with_no_aicl(temp_curr);
			chg_err("[down] set input_current = %d\n", temp_curr);
		}
	}

	rc = oplus_chg_set_input_current_with_no_aicl(current_ma);
	return rc;
}
#endif

#ifdef OPLUS_FEATURE_CHG_BASIC
struct oplus_chg_operations  battery_chg_ops = {
	.get_charger_cycle = oplus_get_charger_cycle,
	.dump_registers = dump_regs,
	.kick_wdt = smbchg_kick_wdt,
	.hardware_init = oplus_chg_hw_init,
	.charging_current_write_fast = smbchg_set_fastchg_current_raw,
	.set_wls_boost_en = smbchg_set_wls_boost_en,
	.set_aicl_point = smbchg_set_aicl_point,
	.input_current_write = oplus_chg_set_input_current,
	.input_current_write_without_aicl = oplus_chg_set_input_current_with_no_aicl,
	.wls_input_current_write = oplus_chg_set_wls_input_current,
	.float_voltage_write = smbchg_float_voltage_set,
	.term_current_set = smbchg_term_current_set,
	.charging_enable = smbchg_charging_enable,
	.charging_disable = smbchg_charging_disable,
	.get_charging_enable = smbchg_get_charge_enable,
	.charger_suspend = smbchg_usb_suspend_enable,
	.charger_unsuspend = smbchg_usb_suspend_disable,
	.charger_suspend_check = smbchg_usb_check_suspend_charger,
	.set_rechg_vol = smbchg_set_rechg_vol,
	.reset_charger = smbchg_reset_charger,
	.read_full = smbchg_read_full,
	.otg_enable = smbchg_otg_enable,
	.otg_disable = smbchg_otg_disable,
	.set_charging_term_disable = oplus_set_chging_term_disable,
	.check_charger_resume = qcom_check_charger_resume,
	.get_chargerid_volt = smbchg_get_chargerid_volt,
	.set_chargerid_switch_val = smbchg_set_chargerid_switch_val,
	.get_chargerid_switch_val = smbchg_get_chargerid_switch_val,
	.need_to_check_ibatt = smbchg_need_to_check_ibatt,
	.get_chg_current_step = smbchg_get_chg_current_step,
	.get_charger_type = opchg_get_charger_type,
	.get_charger_volt = qpnp_get_prop_charger_voltage_now,
	.get_charger_current = oplus_get_ibus_current,
	.check_chrdet_status = oplus_chg_is_usb_present,
	.get_instant_vbatt = qpnp_get_battery_voltage,
	.get_boot_mode = get_boot_mode,
	.get_boot_reason = smbchg_get_boot_reason,
	.get_rtc_soc = oplus_chg_get_shutdown_soc,
	.set_rtc_soc = oplus_chg_backup_soc,
	.get_aicl_ma = smbchg_get_aicl_level_ma,
	.rerun_aicl = smbchg_rerun_aicl,
#ifdef CONFIG_OPLUS_RTC_DET_SUPPORT
	.check_rtc_reset = rtc_reset_check,
#endif
#ifdef CONFIG_OPLUS_SHORT_C_BATT_CHECK
	.get_dyna_aicl_result = oplus_chg_get_dyna_aicl_result,
#endif
	.get_charger_subtype = oplus_chg_get_charger_subtype,
	.oplus_chg_get_pd_type = oplus_sm8150_get_pd_type,
	.oplus_chg_get_pps_status = oplus_chg_get_pps_status,
	.oplus_chg_get_max_cur = oplus_chg_pps_get_max_cur,
	.oplus_chg_pd_setup = oplus_chg_set_pd_config,
	.set_qc_config = oplus_chg_set_qc_config,
	.enable_qc_detect = oplus_chg_enable_qc_detect,
	.adsp_voocphy_set_match_temp = oplus_adsp_voocphy_set_match_temp,
	.input_current_ctrl_by_vooc_write = oplus_input_current_limit_ctrl_by_vooc_write,
	.get_props_from_adsp_by_buffer = oplus_get_props_from_adsp_by_buffer,
	.set_bcc_curr_to_voocphy = oplus_set_bcc_curr_to_voocphy,
	.pdo_5v = oplus_chg_set_pdo_5v,
	.get_subboard_temp = oplus_get_subboard_temp,
	.check_cc_mode = oplus_check_cc_mode,
	.get_ccdetect_online = sm8550_get_ccdetect_online,
	.get_abnormal_adapter_disconnect_cnt = oplus_get_abnormal_adapter_disconnect_cnt,
};
#endif /* OPLUS_FEATURE_CHG_BASIC */

#ifdef OPLUS_FEATURE_CHG_BASIC
int oplus_sm8350_read_input_voltage(void) {
	chg_err("%s\n", __func__);
	return qpnp_get_prop_charger_voltage_now();
}

int oplus_sm8350_read_vbat0_voltage(void) {
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}
	chg_err("%s\n", __func__);
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_PPS_READ_VBAT0_VOLT);
	if (rc < 0) {
		chg_err("get pps vbat0 volt fail, rc = %d\n", rc);
		return -1;
	}

	chg_err("PPS vbat0 volt = %d\n", pst->prop[USB_PPS_READ_VBAT0_VOLT]);

	return pst->prop[USB_PPS_READ_VBAT0_VOLT];
}

int oplus_sm8350_check_btb_temp(void) {
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}
	chg_err("%s\n", __func__);
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_PPS_CHECK_BTB_TEMP);
	if (rc < 0) {
		chg_err("pps check btb temp fail, rc = %d\n", rc);
		return -1;
	}

	return pst->prop[USB_PPS_CHECK_BTB_TEMP];
}

int oplus_sm8350_pps_mos_ctrl(int on) {
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}
	chg_err("%s, on[%d]\n", __func__, on);
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_PPS_MOS_CTRL, on);
	if (rc < 0) {
		chg_err("USB_PPS_MOS_CTRL fail, rc = %d\n", rc);
		return -1;
	}

	return 0;
}

int oplus_sm8350_pps_get_authentiate(void) {
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return false;
	}
	chg_err("%s\n", __func__);
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_PPS_GET_AUTHENTICATE);
	if (rc < 0) {
		chg_err("oplus_pps_get_authentiate fail, rc = %d\n", rc);
		return -1;
	}

	chg_err("oplus_pps_get_authentiate  = %d\n", pst->prop[USB_PPS_GET_AUTHENTICATE]);

	return pst->prop[USB_PPS_GET_AUTHENTICATE];
}
#endif

/**********************************************************************
 * battery gauge ops *
 **********************************************************************/
#ifdef OPLUS_FEATURE_CHG_BASIC
static int fg_bq27541_get_battery_mvolts(void)
{
	int rc = 0;
	int prop_id = 0;
	static int volt = 4000;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY
		&& oplus_pps_get_chg_status() != PPS_CHARGERING) {
		volt = DIV_ROUND_CLOSEST(bcdev->read_buffer_dump.data_buffer[2], 1000);
		return volt;
	}

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_VOLTAGE_NOW);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read battery volt fail, rc=%d\n", rc);
		return volt;
	}
	volt = DIV_ROUND_CLOSEST(pst->prop[prop_id], 1000);

	return volt;
}

static int fg_bq27541_get_battery_temperature(void)
{
	int rc = 0;
	int prop_id = 0;
	static int temp = 250;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) {
		temp = bcdev->read_buffer_dump.data_buffer[0];
		if (bcdev->gauge_data_initialized == true)
			goto HIGH_TEMP;
	}

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_TEMP);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read battery temp fail, rc=%d\n", rc);
		return temp;
	}
	temp = DIV_ROUND_CLOSEST((int)pst->prop[prop_id], 10);

HIGH_TEMP:
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	if ((get_eng_version() == HIGH_TEMP_AGING) || (get_eng_version() == PTCRB)) {
		chg_err("[OPLUS_CHG]CONFIG_HIGH_TEMP_VERSION enable here, \
				disable high tbat shutdown \n");
		if (temp > 690)
			temp = 690;
	}
#endif

	return temp;
}

static int fg_bq27541_get_batt_remaining_capacity(void)
{
	int rc = 0;
	static int batt_rm = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return batt_rm;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) {
		batt_rm = bcdev->read_buffer_dump.data_buffer[4];
		chip->batt_rm = batt_rm * chip->vbatt_num;
		return batt_rm;
	}

	rc = read_property_id(bcdev, pst, BATT_CHG_COUNTER);
	if (rc < 0) {
		chg_err("read battery chg counter fail, rc=%d\n", rc);
		return batt_rm;
	}
	batt_rm = DIV_ROUND_CLOSEST(pst->prop[BATT_CHG_COUNTER], 1000);

	chip->batt_rm = batt_rm * chip->vbatt_num;
	return batt_rm;
}

static int fg_bq27541_get_battery_soc(void)
{
	int rc = 0;
	int prop_id = 0;
	static int soc = 50;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) {
		soc = DIV_ROUND_CLOSEST(bcdev->read_buffer_dump.data_buffer[3], 100);
		return soc;
	}

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_CAPACITY);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read battery soc fail, rc=%d\n", rc);
		return soc;
	}
	soc = DIV_ROUND_CLOSEST(pst->prop[prop_id], 100);

	return soc;
}

static int fg_bq27541_get_average_current(void)
{
	int rc = 0;
	int prop_id = 0;
	static int curr = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY && !chip->charger_exist && !chip->read_by_reg) {
		curr = DIV_ROUND_CLOSEST((int)bcdev->read_buffer_dump.data_buffer[1], 1000);
		return curr;
	}

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_CURRENT_NOW);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read battery curr fail, rc=%d\n", rc);
		return curr;
	}
	curr = DIV_ROUND_CLOSEST((int)pst->prop[prop_id], 1000);

	return curr;
}

static int fg_bq27541_get_battery_fcc(void)
{
	static int fcc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) {
		fcc = bcdev->read_buffer_dump.data_buffer[6];
		return fcc;
	}

	return fcc;
}

static int fg_bq27541_get_battery_cc(void)
{
	static int cc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) {
		cc = bcdev->read_buffer_dump.data_buffer[7];
		return cc;
	}

	return cc;
}

static int fg_bq27541_get_battery_soh(void)
{
	static int soh = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY) {
		soh = bcdev->read_buffer_dump.data_buffer[8];
		return soh;
	}

	return soh;
}

static bool fg_bq27541_get_battery_authenticate(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	rc = read_property_id(bcdev, pst, BATT_BATTERY_AUTH);
	if (rc < 0) {
		chg_err("read battery auth fail, rc=%d\n", rc);
		return false;
	}
	chg_err("read battery auth success, auth=%d\n", pst->prop[BATT_BATTERY_AUTH]);

	return pst->prop[BATT_BATTERY_AUTH];
}

static bool fg_bq27541_get_battery_hmac(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	rc = read_property_id(bcdev, pst, BATT_BATTERY_HMAC);
	if (rc < 0) {
		chg_err("read battery hmac fail, rc=%d\n", rc);
		return false;
	}
	chg_err("read battery hmac success, auth=%d\n", pst->prop[BATT_BATTERY_HMAC]);

	return pst->prop[BATT_BATTERY_HMAC];
}

static void fg_bq27541_set_battery_full(bool full)
{
	/*Do nothing*/
}
/*
static int fg_bq27541_get_prev_battery_mvolts(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	return chip->batt_volt;
}

static int fg_bq27541_get_prev_battery_temperature(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	return chip->temperature;
}

static int fg_bq27541_get_prev_battery_soc(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	return chip->soc;
}

static int fg_bq27541_get_prev_average_current(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	return chip->icharging;
}

static int fg_bq27541_get_prev_batt_remaining_capacity(void)
{
	return 0;
}
*/
static int fg_bq27541_get_battery_mvolts_2cell_max(void)
{
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	return chip->batt_volt;
}

static int fg_bq27541_get_battery_mvolts_2cell_min(void)
{
	static int volt = 4000;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return -1;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY)
		volt = bcdev->read_buffer_dump.data_buffer[13];
	else
		volt = chip->batt_volt;

	return volt;
}
/*
static int fg_bq27541_get_prev_battery_mvolts_2cell_max(void)
{
	return 4000;
}

static int fg_bq27541_get_prev_battery_mvolts_2cell_min(void)
{
	return 4000;
}
*/
static int fg_bq28z610_modify_dod0(void)
{
	return 0;
}

static int fg_bq28z610_update_soc_smooth_parameter(void)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	int sleep_mode_status = -1;
	int retry_count = 0;

	if (!g_oplus_chip) {
		chg_err("g_oplus_chip is NULL!\n");
		return -1;
	}
	bcdev = g_oplus_chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	rc = write_property_id(bcdev, pst, BATT_UPDATE_SOC_SMOOTH_PARAM, 1);
	if (rc) {
		chg_err("set smooth fail, rc=%d\n", rc);
		return -1;
	}

read_parameter:
	rc = read_property_id(bcdev, pst, BATT_UPDATE_SOC_SMOOTH_PARAM);
	if (rc) {
		chg_err("read debug reg fail, rc=%d\n", rc);
	} else {
		sleep_mode_status = pst->prop[BATT_UPDATE_SOC_SMOOTH_PARAM];
	}

	chg_debug("bq8z610 sleep mode status = %d\n", sleep_mode_status);
	if (sleep_mode_status != 1 && retry_count < 3) {
		msleep(2000);
		retry_count++;
		goto read_parameter;
	}

	return 0;
}

static int fg_bq28z610_get_battery_balancing_status(void)
{
	return 0;
}

static struct oplus_gauge_operations battery_gauge_ops = {
	.get_battery_mvolts = fg_bq27541_get_battery_mvolts,
	.get_battery_temperature = fg_bq27541_get_battery_temperature,
	.get_batt_remaining_capacity = fg_bq27541_get_batt_remaining_capacity,
	.get_battery_soc = fg_bq27541_get_battery_soc,
	.get_average_current = fg_bq27541_get_average_current,
	.get_battery_fcc = fg_bq27541_get_battery_fcc,
	.get_prev_batt_fcc = fg_bq27541_get_battery_fcc,
	.get_battery_cc = fg_bq27541_get_battery_cc,
	.get_battery_soh = fg_bq27541_get_battery_soh,
	.get_battery_authenticate = fg_bq27541_get_battery_authenticate,
	.get_battery_hmac = fg_bq27541_get_battery_hmac,
	.set_battery_full = fg_bq27541_set_battery_full,
	.get_prev_battery_mvolts = fg_bq27541_get_battery_mvolts,
	.get_prev_battery_temperature = fg_bq27541_get_battery_temperature,
	.get_prev_battery_soc = fg_bq27541_get_battery_soc,
	.get_prev_average_current = fg_bq27541_get_average_current,
	.get_prev_batt_remaining_capacity = fg_bq27541_get_batt_remaining_capacity,
	.get_battery_mvolts_2cell_max = fg_bq27541_get_battery_mvolts_2cell_max,
	.get_battery_mvolts_2cell_min = fg_bq27541_get_battery_mvolts_2cell_min,
	.get_prev_battery_mvolts_2cell_max = fg_bq27541_get_battery_mvolts_2cell_max,
	.get_prev_battery_mvolts_2cell_min = fg_bq27541_get_battery_mvolts_2cell_min,
	.update_battery_dod0 = fg_bq28z610_modify_dod0,
	.update_soc_smooth_parameter = fg_bq28z610_update_soc_smooth_parameter,
	.get_battery_cb_status = fg_bq28z610_get_battery_balancing_status,
	.get_bcc_parameters = oplus_get_bcc_parameters_from_adsp,
	.set_bcc_parameters = oplus_set_bcc_debug_parameters,
};
#endif /* OPLUS_FEATURE_CHG_BASIC */

#ifdef OPLUS_FEATURE_CHG_BASIC
static int smbchg_lcm_en(bool en)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!probe_done)
		return 0;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -ENODEV;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	if (en)
		rc = write_property_id(bcdev, pst, USB_POWER_SUPPLY_RELEASE_FIXED_FREQUENCE, 0);
	else
		rc = write_property_id(bcdev, pst, USB_POWER_SUPPLY_RELEASE_FIXED_FREQUENCE, 1);
	if (rc < 0)
		pr_err("set lcm to %u error, rc=%d\n", en, rc);
	else
		pr_info("set lcm to %d \n", en);

	return rc;
}

static int smbchg_wls_input_enable(struct oplus_chg_ic_dev *ic_dev, bool en)
{
	if (!probe_done)
		return 0;

	/*if (en)
		smbchg_usb_suspend_disable();
	else
		smbchg_usb_suspend_enable();*/

	return 0;
}

static int smbchg_wls_output_enable(struct oplus_chg_ic_dev *ic_dev, bool en)
{
	int rc = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;

	if (!probe_done)
		return 0;

	if (en)
		rc = smbchg_charging_enable();
	else
		rc = smbchg_charging_disable();
	return rc;

	if (ic_dev == NULL) {
		pr_err("oplus_chg_ic_dev is NULL");
		return -ENODEV;
	}
	bcdev = oplus_chg_ic_get_drvdata(ic_dev);
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	rc = write_property_id(bcdev, pst, BATT_CHG_EN, en ? 1 : 0);
	if (rc)
		pr_err("set %s charging fail, rc=%d\n", en ? "enable": "disable", rc);

	return rc;
}

static int smbchg_wls_set_icl(struct oplus_chg_ic_dev *ic_dev, int icl_ma)
{
	int rc = 0;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *chip;

	if (!probe_done)
		return 0;

#ifdef WLS_QI_DEBUG
	if (wls_dbg_icl_ma != 0)
		icl_ma = wls_dbg_icl_ma;
#endif

	if (ic_dev == NULL) {
		pr_err("oplus_chg_ic_dev is NULL");
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_drvdata(ic_dev);
	pst = &chip->psy_list[PSY_TYPE_WLS];

	rc = write_property_id(chip, pst, WLS_INPUT_CURR_LIMIT, icl_ma * 1000);
	if (rc < 0)
		pr_err("set wls icl to %u error, rc=%d\n", icl_ma, rc);
	else
		pr_info("set icl to %d mA\n", icl_ma);

	return rc;
}

#define WLS_FCC_MAX_MA	2500
static int smbchg_wls_set_fcc(struct oplus_chg_ic_dev *ic_dev, int fcc_ma)
{
	int rc = 0;
	int prop_id = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;

	if (!probe_done)
		return 0;

#ifdef WLS_QI_DEBUG
	if (wls_dbg_fcc_ma != 0)
		fcc_ma = wls_dbg_fcc_ma;
#endif

	if (fcc_ma > WLS_FCC_MAX_MA)
		fcc_ma = WLS_FCC_MAX_MA;

	if (ic_dev == NULL) {
		pr_err("oplus_chg_ic_dev is NULL");
		return -ENODEV;
	}
	bcdev = oplus_chg_ic_get_drvdata(ic_dev);
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	prop_id = BATT_CHG_CTRL_LIM;
	rc = write_property_id(bcdev, pst, prop_id, fcc_ma * 1000);
	if (rc)
		pr_err("set fcc to %d mA fail, rc=%d\n", fcc_ma, rc);
	else
		pr_info("set fcc to %d mA\n", fcc_ma);

	return rc;
}

static int smbchg_wls_set_fv(struct oplus_chg_ic_dev *ic_dev, int fv_mv)
{
	int rc = 0;

	if (!probe_done)
		return 0;

	rc = smbchg_float_voltage_set(fv_mv);
	if (rc < 0)
		pr_err("set wls fv to %u error, rc=%d\n", fv_mv, rc);

	return rc;
}

static int smbchg_wls_rechg_vol(struct oplus_chg_ic_dev *ic_dev, int fv_mv)
{
	int rc = 0;

	if (!probe_done)
		return 0;

	return rc;
}

static int smbchg_wls_get_input_curr(struct oplus_chg_ic_dev *ic_dev, int *curr)
{
	int rc = 0;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *chip;

	if (!probe_done)
		return 0;

	if (ic_dev == NULL) {
		pr_err("oplus_chg_ic_dev is NULL");
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_drvdata(ic_dev);
	pst = &chip->psy_list[PSY_TYPE_WLS];

	rc = read_property_id(chip, pst, WLS_CURR_NOW);
	if (rc < 0) {
		pr_err("get wls input curr error, rc=%d\n", rc);
		return rc;
	}
	*curr = pst->prop[WLS_CURR_NOW] / 1000;

	return rc;
}

static int smbchg_wls_get_input_vol(struct oplus_chg_ic_dev *ic_dev, int *vol)
{
	int rc = 0;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *chip;

	if (!probe_done)
		return 0;

	if (ic_dev == NULL) {
		pr_err("oplus_chg_ic_dev is NULL");
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_drvdata(ic_dev);
	pst = &chip->psy_list[PSY_TYPE_WLS];

	rc = read_property_id(chip, pst, WLS_VOLT_NOW);
	if (rc < 0) {
		pr_err("set wls input vol error, rc=%d\n", rc);
		return rc;
	}
	*vol = pst->prop[WLS_VOLT_NOW] / 1000;

	return rc;
}

static bool is_ext_boost(struct battery_chg_dev *bcdev)
{
	if (!bcdev) {
		pr_err("bcdev not ready!\n");
		return false;
	}
	return oplus_get_wired_chg_present();
}

static int smbchg_wls_set_boost_en(struct oplus_chg_ic_dev *ic_dev, bool en)
{
	int rc = 0;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *chip;
	static bool pre_wired_chg_present = false;

	if (!probe_done)
		return 0;

	if (ic_dev == NULL) {
		pr_err("oplus_chg_ic_dev is NULL");
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_drvdata(ic_dev);
	pst = &chip->psy_list[PSY_TYPE_WLS];

	if ((is_ext_boost(chip) && en) || pre_wired_chg_present) {
		if (en) {
			oplus_set_tx_ovp_en_val(1);
			/*for HW spec,need 100ms delay*/
			msleep(100);
			oplus_set_otg_boost_en_val(1);
			pre_wired_chg_present = true;
		} else {
			pre_wired_chg_present = false;
			oplus_set_otg_boost_en_val(0);
			/*for HW spec,need 100ms delay*/
			msleep(100);
			oplus_set_tx_ovp_en_val(0);
		}
		return 0;
	}
	if (en) {
		oplus_set_wrx_otg_en_val(1);
		/*for HW spec,need 100ms delay*/
		msleep(100);
	}

	if (en && chip->wls_boost_soft_start && chip->wls_set_boost_vol != PM8350B_BOOST_VOL_MIN_MV) {
		rc = write_property_id(chip, pst, WLS_BOOST_VOLT, PM8350B_BOOST_VOL_MIN_MV);
		if (rc < 0) {
			pr_err("set boost vol to PM8350B_BOOST_VOL_MIN_MV error, rc=%d\n", rc);
			return rc;
		}
	}

	rc = write_property_id(chip, pst, WLS_BOOST_EN, en ? 1 : 0);
	if (rc < 0) {
		pr_err("set boost %s error, rc=%d\n", en ? "enable" : "disable", rc);
		return rc;
	}

	if (en && chip->wls_boost_soft_start && chip->wls_set_boost_vol != PM8350B_BOOST_VOL_MIN_MV) {
		msleep(2);
		rc = write_property_id(chip, pst, WLS_BOOST_VOLT, chip->wls_set_boost_vol);
		if (rc < 0)
			pr_err("set boost vol to %d mV error, rc=%d\n", chip->wls_set_boost_vol, rc);
	}

	if (!en) {
		/*for HW spec,need 100ms delay*/
		msleep(100);
		oplus_set_wrx_otg_en_val(0);
	}

	return rc;
}

#define WLS_BOOST_VOLT_MAX	9000
#define WLS_BOOST_VOLT_START	6000
static int smbchg_wls_set_boost_vol(struct oplus_chg_ic_dev *ic_dev, int vol_mv)
{
	int rc = 0;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *chip;

	if (!probe_done)
		return 0;

	if (ic_dev == NULL) {
		pr_err("oplus_chg_ic_dev is NULL");
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_drvdata(ic_dev);
	pst = &chip->psy_list[PSY_TYPE_WLS];

	if (is_support_tx_boost(chip) || is_ext_boost(chip))
		return 0;

	if (vol_mv == WLS_TRX_MODE_VOL_MV)
		vol_mv = WLS_BOOST_VOLT_START;
	if (vol_mv > WLS_BOOST_VOLT_MAX)
		vol_mv = WLS_BOOST_VOLT_MAX;

	rc = write_property_id(chip, pst, WLS_BOOST_VOLT, vol_mv);
	if (rc < 0)
		pr_err("set boost vol to %d mV error, rc=%d\n", vol_mv, rc);
	else
		chip->wls_set_boost_vol = vol_mv;

	return rc;
}

static int smbchg_wls_set_aicl_enable(struct oplus_chg_ic_dev *ic_dev, bool en)
{
	int rc = 0;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *chip;

	if (!probe_done)
		return 0;

	if (ic_dev == NULL) {
		pr_err("oplus_chg_ic_dev is NULL");
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_drvdata(ic_dev);
	pst = &chip->psy_list[PSY_TYPE_WLS];

	rc = write_property_id(chip, pst, WLS_BOOST_AICL_ENABLE, en);
	if (rc < 0)
		pr_err("can't %s aicl, rc=%d\n", en ? "enable" : " disable", rc);

	return rc;
}

static int smbchg_wls_rerun_aicl(struct oplus_chg_ic_dev *ic_dev)
{
	int rc = 0;
	struct psy_state *pst = NULL;
	struct battery_chg_dev *chip;

	if (!probe_done)
		return 0;

	if (ic_dev == NULL) {
		pr_err("oplus_chg_ic_dev is NULL");
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_drvdata(ic_dev);
	pst = &chip->psy_list[PSY_TYPE_WLS];

	rc = write_property_id(chip, pst, WLS_BOOST_AICL_RERUN, 1);
	if (rc < 0)
		pr_err("can't rerun aicl, rc=%d\n", rc);

	return rc;
}

static struct oplus_chg_ic_buck_ops pm8350b_dev_ops = {
	.chg_set_input_enable = smbchg_wls_input_enable,
	.chg_set_output_enable = smbchg_wls_output_enable,
	.chg_set_icl = smbchg_wls_set_icl,
	.chg_set_fcc = smbchg_wls_set_fcc,
	.chg_set_fv = smbchg_wls_set_fv,
	.chg_set_rechg_vol = smbchg_wls_rechg_vol,
	.chg_get_icl = NULL,
	.chg_get_input_curr = smbchg_wls_get_input_curr,
	.chg_get_input_vol = smbchg_wls_get_input_vol,
	.chg_set_boost_en = smbchg_wls_set_boost_en,
	.chg_set_boost_vol = smbchg_wls_set_boost_vol,
	.chg_set_boost_curr_limit =  NULL,
	.chg_set_aicl_enable = smbchg_wls_set_aicl_enable,
	.chg_set_aicl_rerun = smbchg_wls_rerun_aicl,
};
#endif /* OPLUS_FEATURE_CHG_BASIC */

#ifdef OPLUS_FEATURE_CHG_BASIC
void oplus_set_flash_screen_ctrl_by_pcb_version(struct oplus_chg_chip *chip)
{
	if (!chip)
		return;

	chip->flash_screen_ctrl_status &= ~FLASH_SCREEN_CTRL_DTSI;
	chg_err("flash_screen_ctrl_dtsi=%d\n", chip->flash_screen_ctrl_status);
}
#endif

#ifdef OPLUS_FEATURE_CHG_BASIC
static enum oplus_chg_mod_property oplus_chg_usb_props[] = {
	OPLUS_CHG_PROP_ONLINE,
};

static enum oplus_chg_mod_property oplus_chg_usb_uevent_props[] = {
	OPLUS_CHG_PROP_ONLINE,
};

static int oplus_chg_usb_get_prop(struct oplus_chg_mod *ocm,
			enum oplus_chg_mod_property prop,
			union oplus_chg_mod_propval *pval)
{
	return -EINVAL;
}

static int oplus_chg_usb_set_prop(struct oplus_chg_mod *ocm,
			enum oplus_chg_mod_property prop,
			const union oplus_chg_mod_propval *pval)
{
	return -EINVAL;
}

static int oplus_chg_usb_prop_is_writeable(struct oplus_chg_mod *ocm,
				enum oplus_chg_mod_property prop)
{
	return 0;
}

static const struct oplus_chg_mod_desc oplus_chg_usb_mod_desc = {
	.name = "usb",
	.type = OPLUS_CHG_MOD_USB,
	.properties = oplus_chg_usb_props,
	.num_properties = ARRAY_SIZE(oplus_chg_usb_props),
	.uevent_properties = oplus_chg_usb_uevent_props,
	.uevent_num_properties = ARRAY_SIZE(oplus_chg_usb_uevent_props),
	.exten_properties = NULL,
	.num_exten_properties = 0,
	.get_property = oplus_chg_usb_get_prop,
	.set_property = oplus_chg_usb_set_prop,
	.property_is_writeable	= oplus_chg_usb_prop_is_writeable,
};

static int oplus_chg_usb_event_notifier_call(struct notifier_block *nb,
		unsigned long val, void *v)
{
	struct oplus_chg_mod *owner_ocm = v;

	switch(val) {
	case OPLUS_CHG_EVENT_PRESENT:
	case OPLUS_CHG_EVENT_OFFLINE:
		if (owner_ocm == NULL) {
			pr_err("This event(=%d) does not support anonymous sending\n",
				val);
			return NOTIFY_BAD;
		}

		if (!strcmp(owner_ocm->desc->name, "wireless")) {
			pr_info("%s wls present\n", __func__);
			oplus_chg_wake_update_work();
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int oplus_chg_usb_init_mod(struct battery_chg_dev *bcdev)
{
	struct oplus_chg_mod_config ocm_cfg = {};
	int rc;

	ocm_cfg.drv_data = bcdev;
	ocm_cfg.of_node = bcdev->dev->of_node;

	bcdev->usb_ocm = oplus_chg_mod_register(bcdev->dev,
					   &oplus_chg_usb_mod_desc,
					   &ocm_cfg);
	if (IS_ERR(bcdev->usb_ocm)) {
		pr_err("Couldn't register usb ocm\n");
		rc = PTR_ERR(bcdev->usb_ocm);
		return rc;
	}

	bcdev->usb_event_nb.notifier_call = oplus_chg_usb_event_notifier_call;
	rc = oplus_chg_reg_event_notifier(&bcdev->usb_event_nb);
	if (rc) {
		pr_err("register usb event notifier error, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

#define OTG_SKIN_TEMP_HIGH 450
#define OTG_SKIN_TEMP_MAX 540
static int oplus_get_bat_info_for_otg_status_check(int *soc, int *ichaging)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct psy_state *pst = NULL;
	int rc = 0;
	int prop_id = 0;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return 0;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_CURRENT_NOW);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read battery curr fail, rc=%d\n", rc);
		return -1;
	}
	*ichaging = DIV_ROUND_CLOSEST((int)pst->prop[prop_id], 1000);

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_CAPACITY);
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0) {
		chg_err("read battery soc fail, rc=%d\n", rc);
		return -1;
	}
	*soc = DIV_ROUND_CLOSEST(pst->prop[prop_id], 100);

	return 0;
}

#define OTG_PROHIBITED_CURR_HIGH_THR	3000
#define OTG_PROHIBITED_CURR_LOW_THR	1700
static void oplus_otg_status_check_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct battery_chg_dev *bcdev = container_of(dwork,
	struct battery_chg_dev, otg_status_check_work);
	int rc;
	int skin_temp = 0, batt_current = 0, real_soc = 0;
	bool contion1 = false, contion2 = false, contion3 = false, contion4 = false, contion5 = false;
	struct oplus_chg_chip *chip = g_oplus_chip;
	static int otg_protect_cnt = 0;


	if (!chip) {
		pr_err("chip  NULL\n");
		return;
	}

	if (bcdev == NULL) {
		pr_err("battery_chg_dev is NULL\n");
		return;
	}

	skin_temp = oplus_chg_get_shell_temp();

	rc = oplus_get_bat_info_for_otg_status_check(&real_soc, &batt_current);
	if (rc < 0) {
		pr_err("Error oplus_get_bat_info_for_otg_status_check, rc = %d\n", rc);
		return;
	}
	real_soc = chip->soc;
	pr_err("oplus_otg_status_check_work, batt_current = %d, skin_temp = %d, real_soc = %d, otg_protect_cnt(%d)\n",
		batt_current, skin_temp, real_soc, otg_protect_cnt);

	contion1 = ((batt_current > OTG_PROHIBITED_CURR_LOW_THR) && (skin_temp > OTG_SKIN_TEMP_HIGH));
	contion2 = (batt_current > OTG_PROHIBITED_CURR_HIGH_THR);
	contion3 = (skin_temp > OTG_SKIN_TEMP_MAX);
	contion4 = ((real_soc < 10) && (batt_current > OTG_PROHIBITED_CURR_LOW_THR));
	contion5 = ((skin_temp < 0) && (batt_current > OTG_PROHIBITED_CURR_LOW_THR));

#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	if ((contion1 || contion2 || contion3 || contion4 || contion5) && (get_eng_version() != HIGH_TEMP_AGING)) {
#else
	if ((contion1 || contion2 || contion3 || contion4 || contion5)) {
#endif
		otg_protect_cnt++;
		if (otg_protect_cnt >= 2) {
			if (!bcdev->otg_prohibited) {
				bcdev->otg_prohibited = true;
				schedule_delayed_work(&bcdev->otg_vbus_enable_work, 0);
				pr_err("OTG prohibited, batt_current = %d, skin_temp = %d, real_soc = %d\n",
					batt_current, skin_temp, real_soc);
			}
		}
	} else {
		otg_protect_cnt = 0;
	}

	if (!bcdev->otg_online) {
		if (bcdev->otg_prohibited) {
			bcdev->otg_prohibited = false;
		}
		pr_err("otg_online is false, exit\n");
		return;
	}

	schedule_delayed_work(&bcdev->otg_status_check_work, msecs_to_jiffies(1000));
}


#ifdef PM_REG_DEBUG
static ssize_t proc_debug_reg_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	uint8_t ret = 0;
	char page[10];
	int rc = 0;
	int reg_data = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return 0;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, pst, USB_DEBUG_REG);
	if (rc) {
		chg_err("read debug reg fail, rc=%d\n", rc);
	} else {
		chg_err("read debug reg success, rc=%d\n", rc);
	}

	reg_data = pst->prop[USB_DEBUG_REG];

	sprintf(page, "0x%x\n", reg_data);
	ret = simple_read_from_buffer(buf, count, ppos, page, strlen(page));

	return ret;
}

static ssize_t proc_debug_reg_write(struct file *file, const char __user *buf, size_t count, loff_t *lo)
{
	int rc = 0;
	char buffer[10] = {0};
	int add_data = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	if (count > 10) {
		chg_err("%s: count so len.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buffer, buf, count)) {
		chg_err("%s: read proc input error.\n", __func__);
		return -EFAULT;
	}

	if (1 != sscanf(buffer, "0x%x", &add_data)) {
		chg_err("invalid content: '%s', length = %zd\n", buf, count);
		return -EFAULT;
	}
	chg_err("%s: add:0x%x, data:0x%x\n", __func__, (add_data >> 8) & 0xffff, (add_data & 0xff));

	rc = write_property_id(bcdev, pst, USB_DEBUG_REG, add_data);
	if (rc) {
		chg_err("set usb_debug_reg fail, rc=%d\n", rc);
	} else {
		chg_err("set usb_debug_reg success, rc=%d\n", rc);
	}

	return count;
}

static const struct proc_ops proc_debug_reg_ops =
{
	.proc_read = proc_debug_reg_read,
	.proc_write  = proc_debug_reg_write,
	.proc_open  = simple_open,
	.proc_lseek = seq_lseek,
};

#ifdef WLS_QI_DEBUG
static ssize_t proc_icl_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	uint8_t ret = 0;
	char page[16];

	sprintf(page, "%d\n", wls_dbg_icl_ma);
	ret = simple_read_from_buffer(buf, count, ppos, page, strlen(page));

	return ret;
}

static ssize_t proc_icl_write(struct file *file, const char __user *buf, size_t count, loff_t *lo)
{
	int rc = 0;
	char buffer[16] = {0};
	int icl_data = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_WLS];

	if (count > sizeof(buffer)) {
		chg_err("%s: count > buffer.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buffer, buf, count)) {
		chg_err("%s: read proc icl error.\n", __func__);
		return -EFAULT;
	}

	sscanf(buffer, "%d", &icl_data);
	rc = write_property_id(bcdev, pst, WLS_INPUT_CURR_LIMIT, icl_data * 1000);
	if (rc) {
		chg_err("set wls icl fail, rc=%d\n", rc);
	} else {
		wls_dbg_icl_ma = icl_data;
		chg_err("set wls icl[%d]ma success\n", icl_data);
	}

	return count;
}

static const struct proc_ops proc_icl_ops =
{
	.proc_read = proc_icl_read,
	.proc_write  = proc_icl_write,
	.proc_open  = simple_open,
	.proc_lseek = seq_lseek,
};

static ssize_t proc_fcc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	uint8_t ret = 0;
	char page[16];

	sprintf(page, "%d\n", wls_dbg_fcc_ma);
	ret = simple_read_from_buffer(buf, count, ppos, page, strlen(page));

	return ret;
}

static ssize_t proc_fcc_write(struct file *file, const char __user *buf, size_t count, loff_t *lo)
{
	int rc = 0;
	char buffer[16] = {0};
	int fcc_data = 0;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		chg_err("chip is NULL!\n");
		return -1;
	}
	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (count > sizeof(buffer)) {
		chg_err("%s: count > buffer.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buffer, buf, count)) {
		chg_err("%s: read proc input error.\n", __func__);
		return -EFAULT;
	}

	sscanf(buffer, "%d", &fcc_data);
	rc = write_property_id(bcdev, pst, BATT_CHG_CTRL_LIM, fcc_data * 1000);
	if (rc) {
		chg_err("set fcc fail, rc=%d\n", rc);
	} else {
		wls_dbg_fcc_ma = fcc_data;
		chg_err("set fcc[%d]ma success\n", fcc_data);
	}

	return count;
}

static const struct proc_ops proc_fcc_ops =
{
	.proc_read = proc_fcc_read,
	.proc_write  = proc_fcc_write,
	.proc_open  = simple_open,
	.proc_lseek = seq_lseek,
};
#endif /*WLS_QI_DEBUG*/
#endif

#ifdef PM_REG_DEBUG
static int init_debug_reg_proc(struct oplus_chg_chip *da)
{
	int ret = 0;
	struct proc_dir_entry *pr_entry_da = NULL;
	struct proc_dir_entry *pr_entry_tmp = NULL;

	pr_entry_da = proc_mkdir("8350_reg", NULL);
	if (pr_entry_da == NULL) {
		ret = -ENOMEM;
		chg_debug("%s: Couldn't create debug_reg proc entry\n", __func__);
	}

	pr_entry_tmp = proc_create_data("reg", 0644, pr_entry_da, &proc_debug_reg_ops, da);
	if (pr_entry_tmp == NULL) {
		ret = -ENOMEM;
		chg_debug("%s: Couldn't create proc entry, %d\n", __func__, __LINE__);
	}

#ifdef WLS_QI_DEBUG
	pr_entry_tmp = proc_create_data("icl_ma", 0644, pr_entry_da, &proc_icl_ops, da);
	if (pr_entry_tmp == NULL) {
		ret = -ENOMEM;
		chg_debug("%s: Couldn't create proc entry, %d\n", __func__, __LINE__);
	}

	pr_entry_tmp = proc_create_data("fcc_ma", 0644, pr_entry_da, &proc_fcc_ops, da);
	if (pr_entry_tmp == NULL) {
		ret = -ENOMEM;
		chg_debug("%s: Couldn't create proc entry, %d\n", __func__, __LINE__);
	}
#endif

	return 0;
}
#endif

static int get_current_time(unsigned long *now_tm_sec)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -EINVAL;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Error reading rtc device (%s) : %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}
	rtc_tm_to_time(&tm, now_tm_sec);

close_time:
	rtc_class_close(rtc);
	return rc;
}

static unsigned long suspend_tm_sec = 0;
static int battery_chg_pm_resume(struct device *dev)
{
	int rc = 0;
	unsigned long resume_tm_sec = 0;
	unsigned long sleep_time = 0;

	if (!g_oplus_chip)
		return 0;

	rc = get_current_time(&resume_tm_sec);
	if (rc || suspend_tm_sec == -1) {
		chg_err("RTC read failed\n");
		sleep_time = 0;
	} else {
		sleep_time = resume_tm_sec - suspend_tm_sec;
	}

	if (sleep_time < 0) {
		sleep_time = 0;
	}

	if (is_ext_chg_ops())
		return 0;

	g_oplus_chip->soc_resume_sleep_time = sleep_time;
	schedule_delayed_work(&g_oplus_chip->soc_update_when_resume_work, 0);

	return 0;
}

static int battery_chg_pm_suspend(struct device *dev)
{
	if (!g_oplus_chip)
		return 0;

	if (get_current_time(&suspend_tm_sec)) {
		chg_err("RTC read failed\n");
		suspend_tm_sec = -1;
	}

	return 0;
}

static const struct dev_pm_ops battery_chg_pm_ops = {
	.resume		= battery_chg_pm_resume,
	.suspend	= battery_chg_pm_suspend,
};
#endif

#ifdef OPLUS_FEATURE_CHG_BASIC
static int oplus_chg_ssr_notifier_cb(struct notifier_block *nb,
				unsigned long code, void *data)
{
	struct battery_chg_dev *bcdev = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip)
		return -1;

	pr_err("code: %lu\n", code);
	bcdev = chip->pmic_spmi.bcdev_chip;

	switch (code) {
	case QCOM_SSR_BEFORE_SHUTDOWN:
		oplus_turn_off_power_when_adsp_crash();
		oplus_chg_track_upload_adsp_err_info(
			bcdev, TRACK_ADSP_ERR_SSR_BEFORE_SHUTDOWN);
		break;
	case QCOM_SSR_AFTER_POWERUP:
		oplus_adsp_crash_recover_work();
		oplus_chg_track_upload_adsp_err_info(
			bcdev, TRACK_ADSP_ERR_SSR_AFTER_POWERUP);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

#define OPLUS_REG_INDEX_MAX			120
#define OPLUS_REG_INDEX_START			15
#define TRACK_LOCAL_T_NS_TO_S_THD		1000000000
#define TRACK_UPLOAD_COUNT_MAX		10
#define TRACK_DEVICE_ABNORMAL_UPLOAD_PERIOD	(24 * 3600)
static void oplus_chg_track_dump_reg_info(char *dump_info, int len)
{
	int i;
	int index = 0;
	struct oplus_chg_chip *chip = g_oplus_chip;
	struct battery_chg_dev *bcdev;

	if(!chip || !dump_info)
		return;

	bcdev = chip->pmic_spmi.bcdev_chip;
	if(!bcdev)
		return;

	if(!chip->charger_exist)
		return;

	oem_read_buffer(bcdev);
	index += snprintf(
		&(dump_info[index]), len - index,
		"chg_en=%d, suspend=%d, pd_svooc=%d, subtype=0x%02x,"
		"usb_commmu=%d, typec_mode=%d, cid=0x%02x, usb_in=%d,",
		smbchg_get_charge_enable(),
		bcdev->read_buffer_dump.data_buffer[9],
		bcdev->read_buffer_dump.data_buffer[11],
		oplus_chg_get_charger_subtype(),
		bcdev->read_buffer_dump.data_buffer[10],
		bcdev->read_buffer_dump.data_buffer[12],
		bcdev->cid_status, bcdev->usb_in_status);
	for (i = OPLUS_REG_INDEX_START; i < OPLUS_REG_INDEX_MAX &&
	      i < (MAX_OEM_PROPERTY_DATA_SIZE - 1); i += 2) {
		index += snprintf(
			&(dump_info[index]), len - index,
			"0x%4x=0x%02x,",
			bcdev->read_buffer_dump.data_buffer[i],
			bcdev->read_buffer_dump.data_buffer[i + 1]);
		pr_info("index:%d, size:%d\n", index, len - index);
	}
}

static int oplus_chg_track_get_local_time_s(void)
{
	int local_time_s;

	local_time_s = local_clock() / TRACK_LOCAL_T_NS_TO_S_THD;
	pr_info("local_time_s:%d\n", local_time_s);

	return local_time_s;
}

static int oplus_chg_track_upload_adsp_err_info(
	struct battery_chg_dev *bcdev, int err_type)
{
	int index = 0;
	int curr_time;
	static int upload_count = 0;
	static int pre_upload_time = 0;

	if (!bcdev)
		return -EINVAL;

	mutex_lock(&bcdev->track_upload_lock);
	memset(bcdev->err_reason, 0, sizeof(bcdev->err_reason));
	curr_time = oplus_chg_track_get_local_time_s();
	if (curr_time - pre_upload_time > TRACK_DEVICE_ABNORMAL_UPLOAD_PERIOD)
		upload_count = 0;

	if (err_type == TRACK_ADSP_ERR_DEFAULT) {
		mutex_unlock(&bcdev->track_upload_lock);
		return -EINVAL;
	}

	if (upload_count > TRACK_UPLOAD_COUNT_MAX) {
		mutex_unlock(&bcdev->track_upload_lock);
		return 0;
	}

	mutex_lock(&bcdev->track_adsp_err_lock);
	if (bcdev->adsp_err_uploading) {
		pr_info("adsp_err_uploading, should return\n");
		mutex_unlock(&bcdev->track_adsp_err_lock);
		mutex_unlock(&bcdev->track_upload_lock);
		return 0;
	}

	if (bcdev->adsp_err_load_trigger)
		kfree(bcdev->adsp_err_load_trigger);
	bcdev->adsp_err_load_trigger = kzalloc(sizeof(oplus_chg_track_trigger), GFP_KERNEL);
	if (!bcdev->adsp_err_load_trigger) {
		pr_err("adsp_err_load_trigger memery alloc fail\n");
		mutex_unlock(&bcdev->track_adsp_err_lock);
		mutex_unlock(&bcdev->track_upload_lock);
		return -ENOMEM;
	}
	bcdev->adsp_err_load_trigger->type_reason =
		TRACK_NOTIFY_TYPE_DEVICE_ABNORMAL;
	bcdev->adsp_err_load_trigger->flag_reason =
		TRACK_NOTIFY_FLAG_PLAT_PMIC_ABNORMAL;
	bcdev->adsp_err_uploading = true;
	upload_count++;
	pre_upload_time = oplus_chg_track_get_local_time_s();
	mutex_unlock(&bcdev->track_adsp_err_lock);

	index += snprintf(
		&(bcdev->adsp_err_load_trigger->crux_info[index]),
		OPLUS_CHG_TRACK_CURX_INFO_LEN - index, "$$device_id@@%s",
		"pm8550b");
	index += snprintf(
		&(bcdev->adsp_err_load_trigger->crux_info[index]),
		OPLUS_CHG_TRACK_CURX_INFO_LEN - index, "$$err_scene@@%s",
		OPLUS_CHG_TRACK_SCENE_ADSP_ERR);

	oplus_chg_track_get_adsp_err_reason(err_type, bcdev->err_reason, sizeof(bcdev->err_reason));
	index += snprintf(
		&(bcdev->adsp_err_load_trigger->crux_info[index]),
		OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
		"$$err_reason@@%s", bcdev->err_reason);

	schedule_delayed_work(&bcdev->adsp_err_load_trigger_work, 0);
	mutex_unlock(&bcdev->track_upload_lock);
	pr_info("success\n");

	return 0;
}

static int oplus_chg_track_upload_icl_err_info(
	struct battery_chg_dev *bcdev, int err_type)
{
	int index = 0;
	int curr_time;
	static int upload_count = 0;
	static int pre_upload_time = 0;

	if (!bcdev)
		return -EINVAL;

	mutex_lock(&bcdev->track_upload_lock);
	memset(bcdev->chg_power_info, 0, sizeof(bcdev->chg_power_info));
	memset(bcdev->err_reason, 0, sizeof(bcdev->err_reason));
	memset(bcdev->dump_info, 0, sizeof(bcdev->dump_info));
	curr_time = oplus_chg_track_get_local_time_s();
	if (curr_time - pre_upload_time > TRACK_DEVICE_ABNORMAL_UPLOAD_PERIOD)
		upload_count = 0;

	if (err_type != TRACK_PMIC_ERR_ICL_VBUS_COLLAPSE &&
	    err_type != TRACK_PMIC_ERR_ICL_VBUS_LOW_POINT) {
		mutex_unlock(&bcdev->track_upload_lock);
		return -EINVAL;
	}

	if (upload_count > TRACK_UPLOAD_COUNT_MAX) {
		mutex_unlock(&bcdev->track_upload_lock);
		return 0;
	}

	mutex_lock(&bcdev->track_icl_err_lock);
	if (bcdev->icl_err_uploading) {
		pr_info("icl_err_uploading, should return\n");
		mutex_unlock(&bcdev->track_icl_err_lock);
		mutex_unlock(&bcdev->track_upload_lock);
		return 0;
	}

	if (bcdev->icl_err_load_trigger)
		kfree(bcdev->icl_err_load_trigger);
	bcdev->icl_err_load_trigger = kzalloc(sizeof(oplus_chg_track_trigger), GFP_KERNEL);
	if (!bcdev->icl_err_load_trigger) {
		pr_err("icl_err_load_trigger memery alloc fail\n");
		mutex_unlock(&bcdev->track_icl_err_lock);
		mutex_unlock(&bcdev->track_upload_lock);
		return -ENOMEM;
	}
	bcdev->icl_err_load_trigger->type_reason =
		TRACK_NOTIFY_TYPE_DEVICE_ABNORMAL;
	bcdev->icl_err_load_trigger->flag_reason =
		TRACK_NOTIFY_FLAG_PLAT_PMIC_ABNORMAL;
	bcdev->icl_err_uploading = true;
	upload_count++;
	pre_upload_time = oplus_chg_track_get_local_time_s();
	mutex_unlock(&bcdev->track_icl_err_lock);

	index += snprintf(
		&(bcdev->icl_err_load_trigger->crux_info[index]),
		OPLUS_CHG_TRACK_CURX_INFO_LEN - index, "$$device_id@@%s",
		"pm8550b");
	index += snprintf(
		&(bcdev->icl_err_load_trigger->crux_info[index]),
		OPLUS_CHG_TRACK_CURX_INFO_LEN - index, "$$err_scene@@%s",
		OPLUS_CHG_TRACK_SCENE_PMIC_ICL_ERR);

	oplus_chg_track_get_pmic_err_reason(err_type, bcdev->err_reason, sizeof(bcdev->err_reason));
	index += snprintf(
		&(bcdev->icl_err_load_trigger->crux_info[index]),
		OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
		"$$err_reason@@%s", bcdev->err_reason);

	oplus_chg_track_obtain_power_info(bcdev->chg_power_info, sizeof(bcdev->chg_power_info));
	index += snprintf(&(bcdev->icl_err_load_trigger->crux_info[index]),
			OPLUS_CHG_TRACK_CURX_INFO_LEN - index, "%s", bcdev->chg_power_info);
	oplus_chg_track_dump_reg_info(bcdev->dump_info, sizeof(bcdev->dump_info));
	index += snprintf(&(bcdev->icl_err_load_trigger->crux_info[index]),
			OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
			"$$reg_info@@%s", bcdev->dump_info);
	schedule_delayed_work(&bcdev->icl_err_load_trigger_work, 0);
	mutex_unlock(&bcdev->track_upload_lock);
	pr_info("success\n");

	return 0;
}

static void oplus_chg_track_icl_err_load_trigger_work(
	struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct battery_chg_dev *bcdev =
		container_of(dwork, struct battery_chg_dev, icl_err_load_trigger_work);

	if (!bcdev->icl_err_load_trigger)
		return;

	oplus_chg_track_upload_trigger_data(*(bcdev->icl_err_load_trigger));
	mutex_lock(&bcdev->track_icl_err_lock);
	kfree(bcdev->icl_err_load_trigger);
	bcdev->icl_err_load_trigger = NULL;
	bcdev->icl_err_uploading = false;
	mutex_unlock(&bcdev->track_icl_err_lock);
}

static void oplus_chg_track_adsp_err_load_trigger_work(
	struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct battery_chg_dev *bcdev =
		container_of(dwork, struct battery_chg_dev, adsp_err_load_trigger_work);

	if (!bcdev->adsp_err_load_trigger)
		return;

	oplus_chg_track_upload_trigger_data(*(bcdev->adsp_err_load_trigger));
	mutex_lock(&bcdev->track_adsp_err_lock);
	kfree(bcdev->adsp_err_load_trigger);
	bcdev->adsp_err_load_trigger = NULL;
	bcdev->adsp_err_uploading = false;
	mutex_unlock(&bcdev->track_adsp_err_lock);
}

u32 oplus_chg_track_get_adsp_debug(void)
{
	/*int rc;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return 0;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_ADSP_TRACK_DEBUG);
	if (rc < 0) {
		pr_err("get adsp debug fail, rc:%d\n", rc);
		return 0;
	}
	pr_info("get adsp_debug[%d]\n", pst->prop[USB_ADSP_TRACK_DEBUG]);

	return pst->prop[USB_ADSP_TRACK_DEBUG];*/
	return 0;
}

void oplus_chg_track_set_adsp_debug(u32 val)
{
	/*int rc;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	struct oplus_chg_chip *chip = g_oplus_chip;

	if (!chip) {
		return;
	}

	bcdev = chip->pmic_spmi.bcdev_chip;
	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = write_property_id(bcdev, pst, USB_ADSP_TRACK_DEBUG, val);
	if (rc < 0) {
		pr_err("set adsp debug fail, rc:%d\n", rc);
		return;
	}*/

	pr_info("set adsp_debug[%d]\n", val);
}

static int oplus_chg_track_debugfs_init(struct battery_chg_dev *bcdev)
{
	int ret = 0;
	struct dentry *debugfs_root;
	struct dentry *debugfs_pm8550b;

	debugfs_root = oplus_chg_track_get_debugfs_root();
	if (!debugfs_root) {
		ret = -ENOENT;
		return ret;
	}

	debugfs_pm8550b = debugfs_create_dir("pm8550b", debugfs_root);
	if (!debugfs_pm8550b) {
		ret = -ENOENT;
		return ret;
	}

	bcdev->debug_force_icl_err = 0;
	bcdev->debug_pmic_glink_err = 0;
	debugfs_create_u32("debug_force_icl_err", 0644,
	    debugfs_pm8550b, &(bcdev->debug_force_icl_err));
	debugfs_create_u32("debug_pmic_glink_err", 0644,
	    debugfs_pm8550b, &(bcdev->debug_pmic_glink_err));

	return ret;
}

static int oplus_chg_track_init(struct battery_chg_dev *bcdev)
{
	int rc;

	if (!bcdev)
		return - EINVAL;

	mutex_init(&bcdev->track_icl_err_lock);
	mutex_init(&bcdev->track_adsp_err_lock);
	mutex_init(&bcdev->track_upload_lock);
	bcdev->icl_err_uploading = false;
	bcdev->icl_err_load_trigger = NULL;
	bcdev->adsp_err_uploading = false;
	bcdev->adsp_err_load_trigger = NULL;

	INIT_DELAYED_WORK(&bcdev->icl_err_load_trigger_work,
	    oplus_chg_track_icl_err_load_trigger_work);
	INIT_DELAYED_WORK(&bcdev->adsp_err_load_trigger_work,
	    oplus_chg_track_adsp_err_load_trigger_work);

	rc = oplus_chg_track_debugfs_init(bcdev);
	if (rc < 0) {
		pr_err(" pm8350b debugfs init error, rc=%d\n", rc);
	}

	return rc;
}

#endif

static int battery_chg_probe(struct platform_device *pdev)
{
#ifdef OPLUS_FEATURE_CHG_BASIC
	struct oplus_gauge_chip *gauge_chip;
	struct oplus_chg_chip *oplus_chip;
	enum oplus_chg_ic_type ic_type;
	int ic_index;
#endif
	struct battery_chg_dev *bcdev;
	struct device *dev = &pdev->dev;
	struct pmic_glink_client_data client_data = { };
	int rc, i;

#ifdef OPLUS_FEATURE_CHG_BASIC
	pr_info("battery_chg_probe start...\n");

	if (pdev->dev.of_node && !of_property_read_bool(pdev->dev.of_node, "oplus,adsp_chg_use_ap_gauge")) {
		gauge_chip = devm_kzalloc(&pdev->dev, sizeof(*gauge_chip), GFP_KERNEL);
		if (!gauge_chip) {
			pr_err("oplus_gauge_chip devm_kzalloc failed.\n");
			return -ENOMEM;
		}
		gauge_chip->gauge_ops = &battery_gauge_ops;
		oplus_gauge_init(gauge_chip);
	}

	oplus_chip = devm_kzalloc(&pdev->dev, sizeof(*oplus_chip), GFP_KERNEL);
	if (!oplus_chip) {
		pr_err("oplus_chg_chip devm_kzalloc failed.\n");
		return -ENOMEM;
	}

	oplus_chip->dev = &pdev->dev;
	rc = oplus_chg_parse_svooc_dt(oplus_chip);

	if (oplus_gauge_check_chip_is_null()) {
		chg_err("gauge chip null, will do after bettery init.\n");
		return -EPROBE_DEFER;
	}

	oplus_chip->chg_ops = oplus_chg_ops_get();
	if (!oplus_chip->chg_ops) {
		chg_err("oplus_chg_ops_get null, fatal error!!!\n");
		return -EPROBE_DEFER;
	}

	g_oplus_chip = oplus_chip;
#endif /*OPLUS_FEATURE_CHG_BASIC*/

	bcdev = devm_kzalloc(&pdev->dev, sizeof(*bcdev), GFP_KERNEL);
	if (!bcdev)
		return -ENOMEM;

#ifdef OPLUS_FEATURE_CHG_BASIC
	bcdev->gauge_data_initialized = false;
	oplus_chip->pmic_spmi.bcdev_chip = bcdev;
	oplus_chip->usbtemp_wq_init_finished = false;
	bcdev->hvdcp_detect_time = 0;
	bcdev->hvdcp_detach_time = 0;
	bcdev->hvdcp_detect_ok = false;
	bcdev->hvdcp_disable = false;
	bcdev->adsp_voocphy_err_check = false;
	bcdev->usb_in_status = 0;
	bcdev->chg_en = false;
	bcdev->cid_status = false;
	bcdev->pd_type_checked = false;
#endif

	bcdev->psy_list[PSY_TYPE_BATTERY].map = battery_prop_map;
	bcdev->psy_list[PSY_TYPE_BATTERY].prop_count = BATT_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_BATTERY].opcode_get = BC_BATTERY_STATUS_GET;
	bcdev->psy_list[PSY_TYPE_BATTERY].opcode_set = BC_BATTERY_STATUS_SET;
	bcdev->psy_list[PSY_TYPE_USB].map = usb_prop_map;
	bcdev->psy_list[PSY_TYPE_USB].prop_count = USB_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_USB].opcode_get = BC_USB_STATUS_GET;
	bcdev->psy_list[PSY_TYPE_USB].opcode_set = BC_USB_STATUS_SET;
	bcdev->psy_list[PSY_TYPE_WLS].map = wls_prop_map;
	bcdev->psy_list[PSY_TYPE_WLS].prop_count = WLS_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_WLS].opcode_get = BC_WLS_STATUS_GET;
	bcdev->psy_list[PSY_TYPE_WLS].opcode_set = BC_WLS_STATUS_SET;

	for (i = 0; i < PSY_TYPE_MAX; i++) {
		bcdev->psy_list[i].prop =
			devm_kcalloc(&pdev->dev, bcdev->psy_list[i].prop_count,
					sizeof(u32), GFP_KERNEL);
		if (!bcdev->psy_list[i].prop)
			return -ENOMEM;
	}

	bcdev->psy_list[PSY_TYPE_BATTERY].model =
		devm_kzalloc(&pdev->dev, MAX_STR_LEN, GFP_KERNEL);
	if (!bcdev->psy_list[PSY_TYPE_BATTERY].model)
		return -ENOMEM;

	mutex_init(&bcdev->rw_lock);
#ifdef OPLUS_FEATURE_CHG_BASIC
	mutex_init(&bcdev->chg_en_lock);
	mutex_init(&bcdev->oplus_custom_gpio.pinctrl_mutex);
	mutex_init(&bcdev->read_buffer_lock);
	init_completion(&bcdev->oem_read_ack);
	mutex_init(&bcdev->bcc_read_buffer_lock);
	init_completion(&bcdev->bcc_read_ack);
	mutex_init(&bcdev->adsp_track_read_buffer_lock);
	init_completion(&bcdev->adsp_track_read_ack);
	mutex_init(&bcdev->read_pmic_buffer_lock);
	init_completion(&bcdev->read_pmic_buffer_ack);
#endif
#ifdef OPLUS_FEATURE_CHG_BASIC
	mutex_init(&bcdev->pps_read_buffer_lock);
	init_completion(&bcdev->pps_read_ack);
#endif
	init_completion(&bcdev->ack);
	init_completion(&bcdev->fw_buf_ack);
	init_completion(&bcdev->fw_update_ack);
	INIT_WORK(&bcdev->subsys_up_work, battery_chg_subsys_up_work);
#ifndef OPLUS_FEATURE_CHG_BASIC
	INIT_WORK(&bcdev->usb_type_work, battery_chg_update_usb_type_work);
#else
	INIT_DELAYED_WORK(&bcdev->usb_type_work, battery_chg_update_usb_type_work);
#endif
#ifdef OPLUS_FEATURE_CHG_BASIC
	INIT_DELAYED_WORK(&bcdev->adsp_voocphy_status_work, oplus_adsp_voocphy_status_func);
	INIT_DELAYED_WORK(&bcdev->unsuspend_usb_work, oplus_unsuspend_usb_work);
	INIT_DELAYED_WORK(&bcdev->otg_init_work, oplus_otg_init_status_func);
	INIT_DELAYED_WORK(&bcdev->ccdetect_work, oplus_ccdetect_work);
	INIT_DELAYED_WORK(&bcdev->usbtemp_recover_work, oplus_usbtemp_recover_work);
	INIT_DELAYED_WORK(&bcdev->cid_status_change_work, oplus_cid_status_change_work);
	INIT_DELAYED_WORK(&bcdev->adsp_crash_recover_work, oplus_adsp_crash_recover_func);
	INIT_DELAYED_WORK(&bcdev->check_charger_out_work, oplus_check_charger_out_func);
	INIT_DELAYED_WORK(&bcdev->adsp_voocphy_enable_check_work, oplus_adsp_voocphy_enable_check_func);
	INIT_DELAYED_WORK(&bcdev->otg_vbus_enable_work, otg_notification_handler);
	INIT_DELAYED_WORK(&bcdev->hvdcp_disable_work, oplus_hvdcp_disable_work);
	INIT_DELAYED_WORK(&bcdev->typec_state_change_work, oplus_typec_state_change_work);
	INIT_DELAYED_WORK(&bcdev->chg_status_send_work, oplus_chg_status_send_adsp_work);
	INIT_DELAYED_WORK(&bcdev->suspend_check_work, oplus_suspend_check_work);
	INIT_DELAYED_WORK(&bcdev->vbus_adc_enable_work, oplus_vbus_enable_adc_work);
	INIT_DELAYED_WORK(&bcdev->adsp_voocphy_err_work, oplus_adsp_voocphy_err_work);
	INIT_DELAYED_WORK(&bcdev->plugin_irq_work, oplus_plugin_irq_work);
	INIT_DELAYED_WORK(&bcdev->recheck_input_current_work, oplus_recheck_input_current_work);
	INIT_DELAYED_WORK(&bcdev->apsd_done_work, oplus_apsd_done_work);
	INIT_DELAYED_WORK(&bcdev->ctrl_lcm_frequency, oplus_chg_ctrl_lcm_work);
	INIT_DELAYED_WORK(&bcdev->otg_status_check_work, oplus_otg_status_check_work);/*OTG software OCP/OTP protect work*/
	INIT_DELAYED_WORK(
		&bcdev->adsp_track_notify_work, adsp_track_notification_handler);
	INIT_DELAYED_WORK(&bcdev->pd_type_check_work, oplus_pd_type_check_work);
	INIT_DELAYED_WORK(&bcdev->apsd_force_svooc_work, oplus_adsp_force_svooc_work);
#endif
#ifdef OPLUS_FEATURE_CHG_BASIC
	INIT_DELAYED_WORK(&bcdev->vchg_trig_work, oplus_vchg_trig_work);
	INIT_DELAYED_WORK(&bcdev->wait_wired_charge_on, oplus_wait_wired_charge_on_work);
	INIT_DELAYED_WORK(&bcdev->wait_wired_charge_off, oplus_wait_wired_charge_off_work);

	INIT_DELAYED_WORK(&bcdev->status_keep_clean_work, oplus_chg_wls_status_keep_clean_work);
	INIT_DELAYED_WORK(&bcdev->status_keep_delay_unlock_work, oplus_chg_wls_status_keep_delay_unlock_work);
	INIT_DELAYED_WORK(&bcdev->mcu_en_init_work, oplus_chg_mcu_en_init_work);
	INIT_DELAYED_WORK(&bcdev->adspvoocphy_plugin_cnt_check_work, oplus_chg_plugin_cnt_check_work);
	INIT_DELAYED_WORK(&bcdev->check_abnormal_usbin_status, oplus_check_abnormal_usbin_status_work);
	bcdev->status_wake_lock = wakeup_source_register(bcdev->dev, "status_wake_lock");
	bcdev->status_wake_lock_on = false;
#endif

	atomic_set(&bcdev->state, PMIC_GLINK_STATE_UP);
	bcdev->dev = dev;

	client_data.id = MSG_OWNER_BC;
	client_data.name = "battery_charger";
	client_data.msg_cb = battery_chg_callback;
	client_data.priv = bcdev;
	client_data.state_cb = battery_chg_state_cb;

	bcdev->client = pmic_glink_register_client(dev, &client_data);
	if (IS_ERR(bcdev->client)) {
		rc = PTR_ERR(bcdev->client);
		if (rc != -EPROBE_DEFER)
			dev_err(dev, "Error in registering with pmic_glink %d\n",
				rc);
		return rc;
	}

	bcdev->initialized = true;
	bcdev->reboot_notifier.notifier_call = battery_chg_ship_mode;
	bcdev->reboot_notifier.priority = 255;
	register_reboot_notifier(&bcdev->reboot_notifier);
#ifdef OPLUS_FEATURE_CHG_BASIC
	oplus_ap_init_adsp_gague();
#endif

	rc = battery_chg_parse_dt(bcdev);
	if (rc < 0) {
		dev_err(dev, "Failed to parse dt rc=%d\n", rc);
		goto error;
	}

	bcdev->restrict_fcc_ua = DEFAULT_RESTRICT_FCC_UA;
	platform_set_drvdata(pdev, bcdev);
	bcdev->fake_soc = -EINVAL;
	rc = battery_chg_init_psy(bcdev);
	if (rc < 0)
		goto error;

	bcdev->battery_class.name = "qcom-battery";
	bcdev->battery_class.class_groups = battery_class_groups;
	rc = class_register(&bcdev->battery_class);
	if (rc < 0) {
		dev_err(dev, "Failed to create battery_class rc=%d\n", rc);
		goto error;
	}

#ifdef OPLUS_FEATURE_CHG_BASIC
	bcdev->ssr_nb.notifier_call = oplus_chg_ssr_notifier_cb;
	bcdev->subsys_handle = qcom_register_ssr_notifier(
							"lpass",
							&bcdev->ssr_nb);
	if (IS_ERR(bcdev->subsys_handle)) {
		rc = PTR_ERR(bcdev->subsys_handle);
		pr_err("Failed in qcom_register_ssr_notifier rc=%d\n", rc);
	}
#endif

#ifdef OPLUS_FEATURE_CHG_BASIC
	oplus_chg_track_init(bcdev);
	oplus_usbtemp_iio_init(oplus_chip);
	oplus_subboard_temp_iio_init(oplus_chip);
	oplus_btbtemp_iio_init(oplus_chip);
	oplus_mosbtb_iio_init(oplus_chip);
	oplus_chg_parse_custom_dt(oplus_chip);
	oplus_chg_parse_charger_dt(oplus_chip);
	oplus_set_flash_screen_ctrl_by_pcb_version(oplus_chip);
	oplus_chg_2uart_pinctrl_init(oplus_chip);
	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY)
		oplus_get_props_from_adsp_by_buffer();
	oplus_chg_init(oplus_chip);
	oplus_chg_configfs_init(oplus_chip);
	oplus_get_usbin_status();
	oplus_chip->temperature = oplus_chg_match_temp_for_chging();
	if (oplus_usbtemp_check_is_support() == true) {
		oplus_usbtemp_thread_init();
	}

	if (qpnp_is_power_off_charging() == false) {
		oplus_tbatt_power_off_task_init(oplus_chip);
	}

	if (oplus_vchg_trig_is_support() == true) {
		schedule_delayed_work(&bcdev->vchg_trig_work, msecs_to_jiffies(3000));
		oplus_vchg_trig_irq_register(bcdev);
	}
#endif /*OPLUS_FEATURE_CHG_BASIC*/

	battery_chg_add_debugfs(bcdev);
	battery_chg_notify_enable(bcdev);
	device_init_wakeup(bcdev->dev, true);
	if (!oplus_is_rf_ftm_mode()) {
		oplus_chip->chg_ops->charging_disable();
		oplus_chip->chg_ops->charging_enable();
	}
#ifndef OPLUS_FEATURE_CHG_BASIC
	schedule_work(&bcdev->usb_type_work);
#else
	schedule_delayed_work(&bcdev->usb_type_work, round_jiffies_relative(msecs_to_jiffies(3000)));
#endif
#ifdef OPLUS_FEATURE_CHG_BASIC
	if (oplus_ccdetect_check_is_gpio(oplus_chip) == true) {
		oplus_ccdetect_irq_register(oplus_chip);
	}
#endif

#ifdef OPLUS_FEATURE_CHG_BASIC
	oplus_adsp_voocphy_set_match_temp();
	/*oplus_dwc3_config_usbphy_pfunc(&oplus_is_pd_svooc);*/
	schedule_delayed_work(&bcdev->otg_init_work, 0);
#ifdef PM_REG_DEBUG
	init_debug_reg_proc(oplus_chip);
#endif
#endif

#ifdef OPLUS_FEATURE_CHG_BASIC
	rc = oplus_chg_usb_init_mod(bcdev);
	if (rc < 0)
		goto error;
	bcdev->wls_boost_soft_start = of_property_read_bool(bcdev->dev->of_node, "oplus,wls_boost_soft_start");
	bcdev->wls_sim_detect_wr = of_property_read_bool(bcdev->dev->of_node, "oplus,wls_sim_detect_wr");
	rc = of_property_read_u32(bcdev->dev->of_node, "oplus,ic_type", &ic_type);
	if (rc < 0) {
		pr_err("can't get ic type, rc=%d\n", rc);
		goto ic_reg_err;
	}
	rc = of_property_read_u32(bcdev->dev->of_node, "oplus,ic_index", &ic_index);
	if (rc < 0) {
		pr_err("can't get ic index, rc=%d\n", rc);
		goto ic_reg_err;
	}
	bcdev->ic_dev = devm_oplus_chg_ic_register(dev, bcdev->dev->of_node->name, ic_index);
	if (!bcdev->ic_dev) {
		rc = -ENODEV;
		pr_err("register %s error\n", bcdev->dev->of_node->name);
		goto ic_reg_err;
	}
	bcdev->ic_dev->dev_ops = &pm8350b_dev_ops;
	bcdev->ic_dev->type = ic_type;
	probe_done = true;
	if (bcdev->usb_ocm) {
		oplus_chg_global_event(bcdev->usb_ocm, OPLUS_CHG_EVENT_ADSP_STARTED);
		if (bcdev->usb_in_status == 1)
			oplus_chg_global_event(bcdev->usb_ocm, OPLUS_CHG_EVENT_ONLINE);
	} else {
		pr_err("usb ocm not fount\n");
	}
	schedule_delayed_work(&bcdev->adsp_voocphy_enable_check_work,
		round_jiffies_relative(msecs_to_jiffies(1500)));
	schedule_delayed_work(&bcdev->mcu_en_init_work, 0);
	/* Adapter has no insertion interrupt circumvention scheme */
	if (!bcdev->plugin_already_run && bcdev->usb_in_status)
		schedule_delayed_work(&bcdev->plugin_irq_work, 0);
	if (oplus_chg_get_voocphy_support() == ADSP_VOOCPHY)
		schedule_delayed_work(&bcdev->adspvoocphy_plugin_cnt_check_work, 0);
	pr_info("battery_chg_probe end...\n");
#endif
	return 0;

#ifdef OPLUS_FEATURE_CHG_BASIC
ic_reg_err:
	oplus_chg_mod_unregister(bcdev->usb_ocm);
	if (bcdev->icl_err_load_trigger)
		kfree(bcdev->icl_err_load_trigger);
	if (bcdev->adsp_err_load_trigger)
		kfree(bcdev->adsp_err_load_trigger);
#endif
error:
	bcdev->initialized = false;
	complete(&bcdev->ack);
	pmic_glink_unregister_client(bcdev->client);
	unregister_reboot_notifier(&bcdev->reboot_notifier);
	return rc;
}

static int battery_chg_remove(struct platform_device *pdev)
{
	struct battery_chg_dev *bcdev = platform_get_drvdata(pdev);
	int rc;

#ifdef OPLUS_FEATURE_CHG_BASIC
	probe_done = true;
	devm_oplus_chg_ic_unregister(bcdev->dev, bcdev->ic_dev);
	oplus_chg_mod_unregister(bcdev->usb_ocm);
#endif
	device_init_wakeup(bcdev->dev, false);
#ifdef OPLUS_FEATURE_CHG_BASIC
	oplus_chg_configfs_exit();
#endif
	debugfs_remove_recursive(bcdev->debugfs_dir);
	class_unregister(&bcdev->battery_class);
	unregister_reboot_notifier(&bcdev->reboot_notifier);
	rc = pmic_glink_unregister_client(bcdev->client);
	if (rc < 0) {
		pr_err("Error unregistering from pmic_glink, rc=%d\n", rc);
		return rc;
	}

#if IS_ENABLED(CONFIG_OPLUS_CHG_TEST_KIT)
	oplus_test_kit_unregister();
#endif

	return 0;
}

#ifdef OPLUS_FEATURE_CHG_BASIC
static void battery_chg_shutdown(struct platform_device *pdev)
{
	struct battery_chg_dev *bcdev = NULL;
	union oplus_chg_mod_propval pval;

        if (!g_oplus_chip) {
                return;
        }
        if (g_oplus_chip->transfer_timeout_count > TRANSFER_TIMOUT_LIMIT) {
                chg_err("g_oplus_chip->transfer_timeout_count");
                return;
        }

	if (g_oplus_chip && g_oplus_chip->pmic_spmi.bcdev_chip->wls_sim_detect_wr && oplus_chg_wls_is_present(g_oplus_chip)) {
		if (is_wls_ocm_available(g_oplus_chip)) {
			pval.intval = 0;
			oplus_chg_mod_set_property(g_oplus_chip->wls_ocm,
				OPLUS_CHG_PROP_CHG_ENABLE, &pval);
			/*workaround: need about 1s sleep*/
			msleep(1000);
		}
	}
	smbchg_lcm_en(true);
	if (g_oplus_chip) {
		chg_err("disable adsp voocphy");
		bcdev = g_oplus_chip->pmic_spmi.bcdev_chip;
		cancel_delayed_work_sync(&bcdev->adsp_voocphy_enable_check_work);
		oplus_typec_disable();
		oplus_adsp_voocphy_enable(false);
	}

	if (g_oplus_chip
		&& g_oplus_chip->chg_ops->charger_suspend
		&& g_oplus_chip->chg_ops->charger_unsuspend) {
		if (g_oplus_chip->voocphy_support != ADSP_VOOCPHY) {
			cancel_delayed_work(&g_oplus_chip->update_work);
			oplus_vooc_reset_mcu();
			smbchg_set_chargerid_switch_val(0);
			oplus_vooc_switch_mode(NORMAL_CHARGER_MODE);
			msleep(SWITCH_TO_NORMAL_DELAY_MS);
		}
		g_oplus_chip->chg_ops->charger_suspend();
		msleep(1000);
		g_oplus_chip->chg_ops->charger_unsuspend();
	}

	if((oplus_chg_get_voocphy_support() == AP_SINGLE_CP_VOOCPHY) ||
	    (oplus_chg_get_voocphy_support() == AP_DUAL_CP_VOOCPHY)) {
		oplus_voocphy_set_adc_enable(false);
	}

	if (g_oplus_chip && g_oplus_chip->enable_shipmode) {
		smbchg_enter_shipmode(g_oplus_chip);
		msleep(1000);
	}
}
#endif /* OPLUS_FEATURE_CHG_BASIC */

static const struct of_device_id battery_chg_match_table[] = {
	{ .compatible = "qcom,battery-charger" },
	{},
};

static struct platform_driver battery_chg_driver = {
	.driver = {
		.name = "qti_battery_charger",
		.of_match_table = battery_chg_match_table,
#ifdef OPLUS_FEATURE_CHG_BASIC
		.pm	= &battery_chg_pm_ops,
#endif
	},
	.probe = battery_chg_probe,
	.remove = battery_chg_remove,
#ifdef OPLUS_FEATURE_CHG_BASIC
	.shutdown = battery_chg_shutdown,
#endif
};

#ifdef OPLUS_FEATURE_CHG_BASIC
static int __init sm8350_chg_init(void)
{
	int ret;

	oplus_chg_ops_register("plat-pmic", &battery_chg_ops);

#ifdef OPLUS_FEATURE_CHG_BASIC
	oplus_pps_cp_init();
#endif
	adsp_voocphy_init();
	bq27541_driver_init();
	sc8517_subsys_init();
	/* tps6128xd_subsys_init(); */
	ret = platform_driver_register(&battery_chg_driver);
	return ret;
}

static void __exit sm8350_chg_exit(void)
{
	platform_driver_unregister(&battery_chg_driver);
	/*mp2650_driver_exit();*/
	sc8517_subsys_exit();
	/* tps6128xd_subsys_exit(); */
	bq27541_driver_exit();
	adsp_voocphy_exit();
	oplus_pps_cp_deinit();
	oplus_pps_ops_deinit();
	oplus_chg_ops_deinit();
}

oplus_chg_module_register(sm8350_chg);
#else
module_platform_driver(battery_chg_driver);
#endif

MODULE_DESCRIPTION("QTI Glink battery charger driver");
MODULE_LICENSE("GPL v2");
