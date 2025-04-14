/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_ext.c
** Description : oplus display panel ext feature
** Version : 1.0
** Date : 2024/05/09
** Author : Display
******************************************************************/
#include <linux/notifier.h>
#include <soc/oplus/fpga_notify.h>
#include "sde_encoder_phys.h"
#include "sde_connector.h"
#include "oplus_display_ext.h"
#include "oplus_display_device_ioctl.h"
#include "oplus_debug.h"

#define to_sde_encoder_phys_cmd(x) \
	container_of(x, struct sde_encoder_phys_cmd, base)

#define WAIT_FPGA_RESET_TIME 4000

extern int dc_apollo_enable;
extern int oplus_dimlayer_hbm;
extern unsigned int oplus_display_log_type;
extern int oplus_debug_max_brightness;
extern u32 bl_lvl;
bool is_lhbm_panel = false;
extern int lcd_closebl_flag;
extern u32 oplus_last_backlight;
extern bool is_lhbm_panel;
extern int oplus_display_private_api_init(void);
extern void  oplus_display_private_api_exit(void);
extern int is_fpga_work_okay(void);

struct notifier_block oplus_fpga_state_notifier_block = {0};
wait_queue_head_t fpga_wq;

int oplus_panel_init(struct dsi_panel *panel)
{
	int rc = 0;
	static bool panel_need_init = true;
	struct dsi_display *display = container_of(&panel, struct dsi_display, panel);

	if (!panel_need_init)
		return 0;

	if (!display) {
		OPLUS_DSI_ERR("display is null\n");
		return 0;
	}

	OPLUS_DSI_INFO("Send panel init dcs\n");

	mutex_lock(&panel->panel_lock);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_PANEL_INIT, false);
	if (!rc)
		panel_need_init = false;

	mutex_unlock(&panel->panel_lock);

	return rc;
}

/*add for panel id compatibility by qcom,mdss-dsi-on-command*/
bool oplus_panel_id_compatibility(struct dsi_panel *panel)
{
	struct dsi_display *display = to_dsi_display(panel->host);

	if (!display) {
		OPLUS_DSI_ERR("display is NULL\n");
		return false;
	}
	/* power on printf panel id */
	OPLUS_DSI_INFO("panel id ID1 = 0x%02X, ID2 = 0x%02X, ID3 = 0x%02X\n",
			display->oplus_display.panel_id1, display->oplus_display.panel_id2,
			display->oplus_display.panel_id3);

	if ((display->oplus_display.panel_id1 == 0x47) && (display->oplus_display.panel_id2 < 0x04)) {
		return true;
	}

	return false;
}

int oplus_set_osc_status(struct drm_encoder *drm_enc) {
	struct sde_encoder_virt *sde_enc = NULL;
	struct sde_encoder_phys *phys_encoder = NULL;
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display = NULL;
	struct sde_connector_state *c_state;
	int rc = 0;
	struct sde_encoder_phys_cmd *cmd_enc = NULL;
	bool sync_osc;
	u32 osc_status;

	sde_enc = to_sde_encoder_virt(drm_enc);
	phys_encoder = sde_enc->phys_encs[0];

	if (phys_encoder == NULL)
		return -EFAULT;
	if (phys_encoder->connector == NULL)
		return -EFAULT;

	c_conn = to_sde_connector(phys_encoder->connector);
	if (c_conn == NULL)
		return -EFAULT;

	if (c_conn->connector_type != DRM_MODE_CONNECTOR_DSI)
		return 0;

	c_state = to_sde_connector_state(c_conn->base.state);

	display = c_conn->display;
	if (display == NULL)
		return -EFAULT;

	cmd_enc = to_sde_encoder_phys_cmd(phys_encoder);
	if (cmd_enc == NULL) {
		return -EFAULT;
	}

	sync_osc = c_conn->oplus_conn.osc_need_update;
	c_conn->oplus_conn.osc_need_update = false;

	if (sync_osc) {
		char tag_name[32];
		osc_status = sde_connector_get_property(c_conn->base.state, CONNECTOR_PROP_SET_OSC_STATUS);
		snprintf(tag_name, sizeof(tag_name), "sync_osc_status:%d", osc_status);
		SDE_ATRACE_BEGIN(tag_name);
		OPLUS_DSI_INFO("osc_status = %d", osc_status);
		oplus_display_panel_set_osc_track(osc_status);
		SDE_ATRACE_END(tag_name);
	}

	return rc;
}

void oplus_panel_switch_vid_mode(struct dsi_display *display, struct dsi_display_mode *mode)
{
	int rc = 0;
	int refresh_rate = 0;
	int dsi_cmd_vid_switch = 0;
	struct dsi_panel *panel = NULL;

	if (!display && !display->panel) {
		OPLUS_DSI_INFO("display/panel is null!\n");
		return;
	}

	if (!mode) {
		OPLUS_DSI_INFO("dsi_display_mode is null!\n");
		return;
	}

	panel = display->panel;
	if (panel->power_mode != SDE_MODE_DPMS_ON) {
		OPLUS_DSI_INFO("display panel in off status\n");
		return;
	}

	if (!dsi_panel_initialized(panel)) {
		OPLUS_DSI_ERR("should not set panel hbm if panel is not initialized\n");
		return;
	}

	if (!panel->oplus_panel.vid_timming_switch_enabled) {
		return;
	}

	refresh_rate = mode->timing.refresh_rate;
		OPLUS_DSI_INFO("oplus_panel_switch_vid_mode refresh %d\n", refresh_rate);

	if (refresh_rate == 120) {
		dsi_cmd_vid_switch = DSI_CMD_VID_120_SWITCH;
	} else if (refresh_rate == 60) {
		dsi_cmd_vid_switch = DSI_CMD_VID_60_SWITCH;
	} else {
		return;
	}

	SDE_ATRACE_BEGIN("oplus_panel_switch_vid_mode");

	mutex_lock(&panel->panel_lock);
	rc = dsi_panel_tx_cmd_set(panel, dsi_cmd_vid_switch, false);
	mutex_unlock(&panel->panel_lock);
	if (rc) {
		OPLUS_DSI_INFO("[%s] failed to send DSI_CMD_VID_SWITCH cmds, rc=%d\n",
			panel->name, rc);
	}
	SDE_ATRACE_END("oplus_panel_switch_vid_mode");

	return;
}

bool sde_encoder_is_disabled(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct sde_encoder_phys *phys;

	sde_enc = to_sde_encoder_virt(drm_enc);
	phys = sde_enc->phys_encs[0];

	return (phys->enable_state == SDE_ENC_DISABLED);
}

void oplus_panel_switch_to_sync_te(struct dsi_panel *panel)
{
	s64 us_per_frame;
	s64 duration;
	u32 vsync_width;
	ktime_t last_te_timestamp;
	int delay = 0;
	u32 vsync_cost = 0;
	u32 debounce_time = 500;
	u32 frame_end = 0;
	struct dsi_display *display = NULL;
	struct sde_encoder_virt *sde_enc;


	if (panel->power_mode != SDE_MODE_DPMS_ON || !panel->panel_initialized) {
		OPLUS_DSI_INFO("display panel in off status\n");
		return;
	}

	us_per_frame = panel->oplus_panel.last_us_per_frame;
	vsync_width = panel->oplus_panel.last_vsync_width;
	last_te_timestamp = panel->oplus_panel.te_timestamp;

	if(!strcmp(panel->type, "primary")) {
		display = get_main_display();
	} else if (!strcmp(panel->type, "secondary")) {
		display = get_sec_display();
	} else {
		OPLUS_DSI_ERR("[DISP][ERR][%s:%d]dsi_display error\n", __func__, __LINE__);
		return;
	}

	sde_enc = to_sde_encoder_virt(display->bridge->base.encoder);
	if (!sde_enc) {
		DSI_ERR("invalid encoder params\n");
		return;
	}

	duration = ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp));
	if(duration > 3 * us_per_frame || sde_enc->rc_state == 4) {
		SDE_ATRACE_BEGIN("timing_delay_prepare");
		oplus_sde_early_wakeup(panel);
		if (duration > 12 * us_per_frame) {
			oplus_wait_for_vsync(panel);
		}
		SDE_ATRACE_END("timing_delay_prepare");
	}

	last_te_timestamp = panel->oplus_panel.te_timestamp;
	vsync_cost = ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp)) % us_per_frame;
	delay = vsync_width - vsync_cost;

	SDE_ATRACE_BEGIN("oplus_panel_switch_to_sync_te");
	if (delay >= 0) {
		if (panel->oplus_panel.last_refresh_rate == 120) {
			if (vsync_cost < 1000)
				usleep_range(1 * 1000, (1 * 1000) + 100);
		} else if (panel->oplus_panel.last_refresh_rate == 60) {
			usleep_range(delay + 200, delay + 300);
		} else if (panel->oplus_panel.last_refresh_rate == 90) {
			if((2100 < vsync_cost) && (vsync_cost < 3100))
				usleep_range(2 * 1000, (2 * 1000) + 100);
		}
	} else if (vsync_cost > vsync_width) {
		frame_end = us_per_frame - vsync_cost;
		if (frame_end < debounce_time) {
			if (panel->oplus_panel.last_refresh_rate == 60) {
				usleep_range(9 * 1000, (9 * 1000) + 100);
			} else if (panel->oplus_panel.last_refresh_rate == 120) {
				usleep_range(2 * 1000, (2 * 1000) + 100);
			}
		}
	}
	SDE_ATRACE_END("oplus_panel_switch_to_sync_te");

	return;
}

int oplus_display_read_serial_number(struct dsi_display *display, unsigned long *serial_number)
{
	int ret = 0;
	int i = 0;
	unsigned char read[30] = {0};
	PANEL_SERIAL_INFO panel_serial_info = {0};
	struct dsi_display_ctrl *m_ctrl = NULL;

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("display is null\n");
		return -EINVAL;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	if (!m_ctrl) {
		OPLUS_DSI_ERR("ctrl is null\n");
		return -EINVAL;
	}

	if (!display->panel->oplus_panel.serial_number.serial_number_support) {
		OPLUS_DSI_INFO("display panel serial number not support\n");
		return -EINVAL;
	}

	if (display->panel->power_mode != SDE_MODE_DPMS_ON) {
		OPLUS_DSI_INFO("display panel in off status\n");
		return -EINVAL;
	}

	if (!display->panel->panel_initialized) {
		OPLUS_DSI_ERR("panel initialized = false\n");
		return -EINVAL;
	}

	/*
	 * for some unknown reason, the panel_serial_info may read dummy,
	 * retry when found panel_serial_info is abnormal.
	 */
	for (i = 0; i < 5; i++) {
		mutex_lock(&display->display_lock);
		mutex_lock(&display->panel->panel_lock);

		if (display->panel->power_mode != SDE_MODE_DPMS_ON) {
			OPLUS_DSI_ERR("display panel in off status\n");
			ret = -EINVAL;
			goto error;
		}
		if (!display->panel->panel_initialized) {
			OPLUS_DSI_ERR("panel initialized = false\n");
			ret = -EINVAL;
			goto error;
		}

		if (display->panel->oplus_panel.serial_number.is_switch_page) {
			ret = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_PANEL_DATE_SWITCH, false);
			if (ret) {
				OPLUS_DSI_ERR("get serial number switch page failed\n");
				ret = -EINVAL;
				goto error;
			}
		}

		ret = dsi_panel_read_panel_reg_unlock(m_ctrl, display->panel, display->panel->oplus_panel.serial_number.serial_number_reg,
				read, display->panel->oplus_panel.serial_number.serial_number_conut);
		if (ret < 0) {
			OPLUS_DSI_ERR("get panel serial number failed\n");
			ret = -EINVAL;
			goto error;
		}

		if (display->panel->oplus_panel.serial_number.is_switch_page) {
			/* switch default page */
			ret = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_DEFAULT_SWITCH_PAGE, false);
			if (ret) {
				OPLUS_DSI_ERR("%s Failed to set DSI_CMD_DEFAULT_SWITCH_PAGE\n", __func__);
				ret = -EINVAL;
				goto error;
			}
		}

		mutex_unlock(&display->panel->panel_lock);
		mutex_unlock(&display->display_lock);

		/*  0xA1               11th        12th    13th    14th    15th
		 *  HEX                0x32        0x0C    0x0B    0x29    0x37
		 *  Bit           [D7:D4][D3:D0] [D5:D0] [D5:D0] [D5:D0] [D5:D0]
		 *  exp              3      2       C       B       29      37
		 *  Yyyy,mm,dd      2014   2m      12d     11h     41min   55sec
		*/
		panel_serial_info.reg_index = display->panel->oplus_panel.serial_number.serial_number_index;

		panel_serial_info.year = (read[panel_serial_info.reg_index] & 0xF0) >> 0x4;

		panel_serial_info.month		= read[panel_serial_info.reg_index]	& 0x0F;
		panel_serial_info.day		= read[panel_serial_info.reg_index + 1]	& 0x1F;
		panel_serial_info.hour		= read[panel_serial_info.reg_index + 2]	& 0x1F;
		panel_serial_info.minute	= read[panel_serial_info.reg_index + 3]	& 0x3F;
		panel_serial_info.second	= read[panel_serial_info.reg_index + 4]	& 0x3F;
		panel_serial_info.reserved[0] = read[panel_serial_info.reg_index + 5];
		panel_serial_info.reserved[1] = read[panel_serial_info.reg_index + 6];

		*serial_number = (panel_serial_info.year		<< 56)\
				+ (panel_serial_info.month		<< 48)\
				+ (panel_serial_info.day		<< 40)\
				+ (panel_serial_info.hour		<< 32)\
				+ (panel_serial_info.minute	<< 24)\
				+ (panel_serial_info.second	<< 16)\
				+ (panel_serial_info.reserved[0] << 8)\
				+ (panel_serial_info.reserved[1]);

		if (!panel_serial_info.year) {
			/*
			 * the panel we use always large than 2011, so
			 * force retry when year is 2011
			 */
			OPLUS_DSI_INFO("panel serial number is 0, retry\n");
			msleep(20);
			continue;
		}
		OPLUS_DSI_INFO("read panel serial_number = [%016lX]\n", *serial_number);
		break;
	}

	return ret;

error:
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return ret;
}

int oplus_display_read_panel_id(struct dsi_display *display, struct panel_id *panel_id)
{
	int ret = 0;
	unsigned char read[30] = {0};
	struct dsi_display_ctrl *m_ctrl = NULL;

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("display is null\n");
		return -EINVAL;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];
	if (!m_ctrl) {
		OPLUS_DSI_ERR("ctrl is null\n");
		return -EINVAL;
	}


	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (display->panel->power_mode != SDE_MODE_DPMS_ON) {
		OPLUS_DSI_ERR("display panel in off status\n");
		ret = -EINVAL;
		goto error;
	}
	if (!display->panel->panel_initialized) {
		OPLUS_DSI_ERR("panel initialized = false\n");
		ret = -EINVAL;
		goto error;
	}

	if (display->panel->oplus_panel.panel_id_switch_page) {
		ret = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_PANEL_INFO_SWITCH_PAGE, false);
		if (ret) {
			OPLUS_DSI_ERR("get serial number switch page failed\n");
			ret = -EINVAL;
			goto error;
		}
	}

	ret = dsi_panel_read_panel_reg_unlock(m_ctrl, display->panel, 0xDA, read, 1);
	if (ret < 0) {
		OPLUS_DSI_ERR("failed to read DA ret=%d\n", ret);
		ret = -EINVAL;
		goto error;
	}

	panel_id->DA = (uint32_t)read[0];

	ret = dsi_panel_read_panel_reg_unlock(m_ctrl, display->panel, 0xDB, read, 1);
	if (ret < 0) {
		OPLUS_DSI_ERR("failed to read DB ret=%d\n", ret);
		ret = -EINVAL;
		goto error;
	}

	panel_id->DB = (uint32_t)read[0];

	ret = dsi_panel_read_panel_reg_unlock(m_ctrl, display->panel, 0xDC, read, 1);
	if (ret < 0) {
		OPLUS_DSI_ERR("failed to read DC ret=%d\n", ret);
		ret = -EINVAL;
		goto error;
	}

	panel_id->DC = (uint32_t)read[0];

	if (display->panel->oplus_panel.panel_id_switch_page) {
		/* switch default page */
		ret = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_DEFAULT_SWITCH_PAGE, false);
		if (ret) {
			OPLUS_DSI_ERR("%s Failed to set DSI_CMD_DEFAULT_SWITCH_PAGE\n", __func__);
			ret = -EINVAL;
			goto error;
		}
	}

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	OPLUS_DSI_INFO("read panel id: DA = 0x%02X, DB = 0x%02X, DC = 0x%02X\n", panel_id->DA,
			panel_id->DB, panel_id->DC);

	return ret;

error:
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return ret;
}

int oplus_panel_fpga_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	int ret = 0;
	struct sde_connector *conn = NULL;
	struct dsi_display *display = NULL;
	struct dsi_display *primary_display = get_main_display();
	struct dsi_panel *panel = NULL;

	OPLUS_DSI_INFO("fpga event is %lu\n", event);

	display = oplus_display_get_current_display();
	if (!display) {
		OPLUS_DSI_ERR("display is NULL\n");
		return NOTIFY_DONE;
	}

	panel = display->panel;
	if (!panel) {
		OPLUS_DSI_ERR("penel is NULL\n");
		return NOTIFY_DONE;
	}

	if (panel->power_mode == SDE_MODE_DPMS_OFF) {
		OPLUS_DSI_INFO("penel power mode is SDE_MODE_DPMS_OFF\n");
		return NOTIFY_DONE;
	}

	if (!primary_display || !primary_display->panel) {
		OPLUS_DSI_ERR("primary display or primary_display->panel is null\n");
		return NOTIFY_DONE;
	}

	conn = to_sde_connector(display->drm_conn);
	if (!conn) {
		OPLUS_DSI_ERR("connector is NULL\n");
		return NOTIFY_DONE;
	}

	if (event == FPGA_NONE) {
		return ret;
	} else if (event == FPGA_RST_END || event == FPGA_GEN_ERRCODE) {
		if (primary_display->panel->oplus_panel.skip_panel_recovery) {
			OPLUS_DSI_INFO("fpga reset, but esd will recovery panel, skip this panel recovery\n");
			primary_display->panel->oplus_panel.skip_panel_recovery = false;
		} else {
			OPLUS_DSI_INFO("fpga reset, panel recovery\n");
			_sde_connector_report_panel_dead(conn, true);
		}

		primary_display->panel->oplus_panel.fpga_reset_completed = true;
		wake_up_all(&fpga_wq);
	}

	return NOTIFY_DONE;
}

int oplus_panel_register_fpga_notifier(void)
{
	int ret = 0;

	oplus_fpga_state_notifier_block.notifier_call = oplus_panel_fpga_psy_event;

	ret = fpga_register_notifier(&oplus_fpga_state_notifier_block);
	if (ret) {
		OPLUS_DSI_ERR("panel fpga register notifier failed!\n");
		return ret;
	}

	init_waitqueue_head(&fpga_wq);
	OPLUS_DSI_INFO("successfully register fpga notifier\n");

	return ret;
}

void oplus_panel_check_fpga(struct dsi_panel *panel)
{
	int rc = 0;
	unsigned long timeout = msecs_to_jiffies(WAIT_FPGA_RESET_TIME);
	struct dsi_display *primary_display = get_main_display();

	if (!primary_display || !primary_display->panel) {
		OPLUS_DSI_ERR("primary display or primary_display->panel is null\n");
		return;
	}

	if (!primary_display->panel->oplus_panel.fpga_support) {
		OPLUS_DSI_INFO("panel isn't support fpga\n");
		return;
	}

	primary_display->panel->oplus_panel.fpga_reset_completed = false;
	rc = is_fpga_work_okay();
	if (rc) {
		OPLUS_DSI_ERR("fpga work failed, need reset\n");
		rc = wait_event_timeout(fpga_wq, primary_display->panel->oplus_panel.fpga_reset_completed,
				timeout);
		if (!rc || !primary_display->panel->oplus_panel.fpga_reset_completed) {
			OPLUS_DSI_ERR("fpga did not reset successfully\n");
		} else {
			OPLUS_DSI_INFO("fpga had reset successfully\n");
		}
	} else {
		OPLUS_DSI_INFO("fpga self-check pass\n");
	}

	return;
}
