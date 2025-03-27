/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_ffl.c
** Description : oplus ffl feature
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/

#include <linux/mutex.h>
#include "dsi_display.h"
#include "oplus_display_utils.h"
#include "oplus_display_device_ioctl.h"

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
#include "oplus_onscreenfingerprint.h"
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

/*#include "oplus_mm_kevent_fb.h"*/


#define FFL_LEVEL_START 2
#define FFL_LEVEL_END  236
#define FFLUPRARE  1
#define BACKUPRATE 6
#define FFL_PENDING_END 600
#define FFL_EXIT_CONTROL 0
#define FFL_TRIGGLE_CONTROL 1
#define FFL_EXIT_FULLY_CONTROL 2
#define FFL_FP_LEVEL 150

bool oplus_ffl_trigger_finish = true;
bool ffl_work_running = false;
int is_ffl_enable = FFL_EXIT_CONTROL;
struct task_struct *oplus_ffl_thread;
struct kthread_worker oplus_ffl_worker;
struct kthread_work oplus_ffl_work;
static DEFINE_MUTEX(oplus_ffl_lock);

EXPORT_SYMBOL(oplus_ffl_trigger_finish);

int oplus_panel_parse_ffc_config(struct dsi_panel *panel)
{
	int rc = 0;
	int i;
	u32 length = 0;
	u32 count = 0;
	u32 size = 0;
	u32 *arr_32 = NULL;
	const u32 *arr;
	struct dsi_parser_utils *utils = &panel->utils;
	struct oplus_clk_osc *seq;

	if (panel->host_config.ext_bridge_mode)
		return 0;

	panel->oplus_panel.ffc_enabled = utils->read_bool(utils->data,
			"oplus,ffc-enabled");
	if (!panel->oplus_panel.ffc_enabled) {
		rc = -EFAULT;
		goto error;
	}

	arr = utils->get_property(utils->data,
			"oplus,clk-osc-sequence", &length);
	if (!arr) {
		OPLUS_DSI_ERR("[%s] oplus,clk-osc-sequence not found\n",
				panel->oplus_panel.vendor_name);
		rc = -EINVAL;
		goto error;
	}
	if (length & 0x1) {
		OPLUS_DSI_ERR("[%s] syntax error for oplus,clk-osc-sequence\n",
				panel->oplus_panel.vendor_name);
		rc = -EINVAL;
		goto error;
	}

	length = length / sizeof(u32);
	OPLUS_DSI_INFO("[%s] oplus,clk-osc-sequence length=%d\n",
			panel->oplus_panel.vendor_name, length);

	size = length * sizeof(u32);
	arr_32 = kzalloc(size, GFP_KERNEL);
	if (!arr_32) {
		rc = -ENOMEM;
		goto error;
	}

	rc = utils->read_u32_array(utils->data, "oplus,clk-osc-sequence",
					arr_32, length);
	if (rc) {
		OPLUS_DSI_ERR("[%s] cannot read oplus,clk-osc-sequence\n",
				panel->oplus_panel.vendor_name);
		goto error_free_arr_32;
	}

	count = length / 2;
	if (count > FFC_MODE_MAX_COUNT) {
		OPLUS_DSI_ERR("[%s] invalid ffc mode count:%d, greater than maximum:%d\n",
				panel->oplus_panel.vendor_name, count, FFC_MODE_MAX_COUNT);
		rc = -EINVAL;
		goto error_free_arr_32;
	}

	size = count * sizeof(*seq);
	seq = kzalloc(size, GFP_KERNEL);
	if (!seq) {
		rc = -ENOMEM;
		goto error_free_arr_32;
	}

	panel->oplus_panel.clk_osc_seq = seq;
	panel->oplus_panel.ffc_delay_frames = 0;
	panel->oplus_panel.ffc_mode_count = count;
	panel->oplus_panel.ffc_mode_index = 0;
	panel->oplus_panel.clk_rate_cur = arr_32[0];
	panel->oplus_panel.osc_rate_cur = arr_32[1];

	for (i = 0; i < length; i += 2) {
		OPLUS_DSI_INFO("[%s] clk osc seq: index=%d <%d %d>\n",
				panel->oplus_panel.vendor_name, i / 2, arr_32[i], arr_32[i+1]);
		seq->clk_rate = arr_32[i];
		seq->osc_rate = arr_32[i + 1];
		seq++;
	}

error_free_arr_32:
	kfree(arr_32);
error:
	if (rc) {
		panel->oplus_panel.ffc_enabled = false;
	}

	OPLUS_DSI_INFO("[%s] oplus,ffc-enabled: %s\n",
			panel->oplus_panel.vendor_name,
			panel->oplus_panel.ffc_enabled ? "true" : "false");

	return rc;
}

void oplus_ffl_set(int enable)
{
	/*unsigned char payload[150] = "";*/

	mutex_lock(&oplus_ffl_lock);

	if (enable != is_ffl_enable) {
		OPLUS_DSI_DEBUG("set_ffl_setting need change is_ffl_enable\n");
		is_ffl_enable = enable;

		if ((is_ffl_enable == FFL_TRIGGLE_CONTROL) && ffl_work_running) {
			oplus_ffl_trigger_finish = false;
			kthread_queue_work(&oplus_ffl_worker, &oplus_ffl_work);
		}
	}

	mutex_unlock(&oplus_ffl_lock);

	if ((is_ffl_enable == FFL_TRIGGLE_CONTROL) && ffl_work_running) {
		/*scnprintf(payload, sizeof(payload), "NULL$$EventID@@%d$$fflset@@%d",
			  OPLUS_MM_DIRVER_FB_EVENT_ID_FFLSET, enable);
		upload_mm_kevent_fb_data(OPLUS_MM_DIRVER_FB_EVENT_MODULE_DISPLAY, payload);*/
	}
}

int oplus_display_panel_get_ffl(void *buf)
{
	unsigned int *enable = buf;

	(*enable) = is_ffl_enable;

	return 0;
}

int oplus_display_panel_set_ffl(void *buf)
{
	unsigned int *enable = buf;

	OPLUS_DSI_INFO("oplus_set_ffl_setting = %d\n", (*enable));
	oplus_ffl_set(*enable);

	return 0;
}

void oplus_ffl_setting_thread(struct kthread_work *work)
{
	struct dsi_display *display = get_main_display();
	int index = 0;
	int pending = 0;
	int system_backlight_target;
	int rc;

	if (is_ffl_enable != FFL_TRIGGLE_CONTROL) {
		return;
	}

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("failed to find display panel \n");
		return;
	}

	if (display->panel->power_mode == SDE_MODE_DPMS_OFF) {
		return;
	}

	if (!ffl_work_running) {
		return;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);

	if (rc) {
		OPLUS_DSI_ERR("[%s] failed to enable DSI core clocks, rc=%d\n",
				display->name, rc);
		return;
	}

	for (index = FFL_LEVEL_START; index < FFL_LEVEL_END;
			index = index + FFLUPRARE) {
		if ((is_ffl_enable == FFL_EXIT_CONTROL) ||
				(is_ffl_enable == FFL_EXIT_FULLY_CONTROL) ||
				!ffl_work_running) {
			break;
		}

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	if (oplus_ofp_is_supported() && !oplus_ofp_oled_capacitive_is_enabled()
			&& !oplus_ofp_ultrasonic_is_enabled()) {
		/*
		* max backlight level should be FFL_FP_LEVEL in hbm state
		*/
		if (oplus_ofp_get_hbm_state() && index > FFL_FP_LEVEL) {
			break;
		}
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

		mutex_lock(&display->panel->panel_lock);
		dsi_panel_set_backlight(display->panel, index);
		mutex_unlock(&display->panel->panel_lock);
		usleep_range(1000, 1100);
	}

	for (pending = 0; pending <= FFL_PENDING_END; pending++) {
		if ((is_ffl_enable == FFL_EXIT_CONTROL) ||
				(is_ffl_enable == FFL_EXIT_FULLY_CONTROL) ||
				!ffl_work_running) {
			break;
		}

		usleep_range(8000, 8100);
	}

	system_backlight_target = display->panel->bl_config.bl_level;

	if (index < system_backlight_target) {
		for (index = 0; index < system_backlight_target; index = index + BACKUPRATE) {
			if ((is_ffl_enable == FFL_EXIT_FULLY_CONTROL) ||
					!ffl_work_running) {
				break;
			}

			mutex_lock(&display->panel->panel_lock);
			dsi_panel_set_backlight(display->panel, index);
			mutex_unlock(&display->panel->panel_lock);
			usleep_range(6000, 6100);
		}

	} else if (index > system_backlight_target) {
		for (index = system_backlight_target; index > system_backlight_target;
				index = index - BACKUPRATE) {
			if ((is_ffl_enable == FFL_EXIT_FULLY_CONTROL) ||
					!ffl_work_running) {
				break;
			}

			mutex_lock(&display->panel->panel_lock);
			dsi_panel_set_backlight(display->panel, index);
			mutex_unlock(&display->panel->panel_lock);
			usleep_range(6000, 6100);
		}
	}

	mutex_lock(&display->panel->panel_lock);
	system_backlight_target = display->panel->bl_config.bl_level;
	dsi_panel_set_backlight(display->panel, system_backlight_target);
	oplus_ffl_trigger_finish = true;
	mutex_unlock(&display->panel->panel_lock);

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);

	if (rc) {
		OPLUS_DSI_ERR("[%s] failed to disable DSI core clocks, rc=%d\n",
				display->name, rc);
	}
}

void oplus_start_ffl_thread(void)
{
	mutex_lock(&oplus_ffl_lock);

	ffl_work_running = true;

	if (is_ffl_enable == FFL_TRIGGLE_CONTROL) {
		oplus_ffl_trigger_finish = false;
		kthread_queue_work(&oplus_ffl_worker, &oplus_ffl_work);
	}

	mutex_unlock(&oplus_ffl_lock);
}
EXPORT_SYMBOL(oplus_start_ffl_thread);

void oplus_stop_ffl_thread(void)
{
	mutex_lock(&oplus_ffl_lock);

	oplus_ffl_trigger_finish = true;
	ffl_work_running = false;
	kthread_flush_worker(&oplus_ffl_worker);

	mutex_unlock(&oplus_ffl_lock);
}

int oplus_ffl_thread_init(void)
{
	kthread_init_worker(&oplus_ffl_worker);
	kthread_init_work(&oplus_ffl_work, &oplus_ffl_setting_thread);
	oplus_ffl_thread = kthread_run(kthread_worker_fn,
			&oplus_ffl_worker, "oplus_ffl");

	if (IS_ERR(oplus_ffl_thread)) {
		OPLUS_DSI_ERR("fail to start oplus_ffl_thread\n");
		oplus_ffl_thread = NULL;
		return -1;
	}

	return 0;
}

void oplus_ffl_thread_exit(void)
{
	if (oplus_ffl_thread) {
		is_ffl_enable = FFL_EXIT_FULLY_CONTROL;
		kthread_flush_worker(&oplus_ffl_worker);
		kthread_stop(oplus_ffl_thread);
		oplus_ffl_thread = NULL;
	}
}
