/***************************************************************
** Copyright (C), 2024, OPLUS Mobile Comm Corp., Ltd
**
** File : oplus_display_effect.c
** Description : oplus display panel effect feature
** Version : 1.1
** Date : 2024/05/09
** Author : Display
******************************************************************/
#include "oplus_display_effect.h"
#include "oplus_display_utils.h"
#include "oplus_display_sysfs_attrs.h"
#include "oplus_debug.h"
#include <uapi/linux/sched/types.h>
#include "sde_trace.h"

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
#include "oplus_onscreenfingerprint.h"
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

int seed_mode = 0;
extern int oplus_seed_backlight;
/* outdoor hbm flag*/

DEFINE_MUTEX(oplus_seed_lock);
#define PANEL_TX_APL_BUF 12
static char oplus_rx_reg_apl[PANEL_TX_APL_BUF] = {0x0};
static char oplus_rx_len_apl = 0;
wait_queue_head_t my_queue;

int __oplus_get_seed_mode(void)
{
	int mode = 0;

	mutex_lock(&oplus_seed_lock);

	mode = seed_mode;

	mutex_unlock(&oplus_seed_lock);

	return mode;
}

int __oplus_set_seed_mode(int mode)
{
	mutex_lock(&oplus_seed_lock);

	if (mode != seed_mode) {
		seed_mode = mode;
	}

	mutex_unlock(&oplus_seed_lock);

	return 0;
}

int dsi_panel_seed_mode_unlock(struct dsi_panel *panel, int mode)
{
	int rc = 0;
	enum dsi_cmd_set_type type;

	if (!dsi_panel_initialized(panel)) {
		return -EINVAL;
	}

	switch (mode) {
	case 0:
		type = DSI_CMD_SEED_MODE0;
		break;
	case 1:
		type = DSI_CMD_SEED_MODE1;
		break;
	case 2:
		type = DSI_CMD_SEED_MODE0;
		break;
	case 3:
		type = DSI_CMD_SEED_MODE3;
		break;
	case 4:
		type = DSI_CMD_SEED_MODE4;
		break;
	default:
		type = DSI_CMD_SEED_OFF;
		OPLUS_DSI_ERR("[%s] Invalid seed mode %d\n",
				panel->oplus_panel.vendor_name, mode);
		break;
	}

	rc = dsi_panel_tx_cmd_set(panel, type, false);
	if (rc) {
		OPLUS_DSI_ERR("Failed to send panel seed mode cmd\n");
	}

	return rc;
}

int dsi_panel_loading_effect_mode_unlock(struct dsi_panel *panel, int mode)
{
	int rc = 0;
	enum dsi_cmd_set_type type;

	if (!dsi_panel_initialized(panel)) {
		return -EINVAL;
	}

	switch (mode) {
	case PANEL_LOADING_EFFECT_MODE1:
		type = DSI_CMD_LOADING_EFFECT_MODE1;
		break;
	case PANEL_LOADING_EFFECT_MODE2:
		type = DSI_CMD_LOADING_EFFECT_MODE2;
		break;
	case PANEL_LOADING_EFFECT_OFF:
		type = DSI_CMD_LOADING_EFFECT_OFF;
		break;
	case PANEL_UIR_ON_LOADING_EFFECT_MODE1:
		type = DSI_CMD_UIR_ON_LOADING_EFFECT_MODE1;
		break;
	case PANEL_UIR_ON_LOADING_EFFECT_MODE2:
		type = DSI_CMD_UIR_ON_LOADING_EFFECT_MODE2;
		break;
	case PANEL_UIR_ON_LOADING_EFFECT_MODE3:
		type = DSI_CMD_UIR_ON_LOADING_EFFECT_MODE3;
		break;
	case PANEL_UIR_OFF_LOADING_EFFECT_MODE1:
		type = DSI_CMD_UIR_OFF_LOADING_EFFECT_MODE1;
		break;
	case PANEL_UIR_OFF_LOADING_EFFECT_MODE2:
		type = DSI_CMD_UIR_OFF_LOADING_EFFECT_MODE2;
		break;
	case PANEL_UIR_OFF_LOADING_EFFECT_MODE3:
		type = DSI_CMD_UIR_OFF_LOADING_EFFECT_MODE3;
		break;
	case PANEL_UIR_LOADING_EFFECT_MODE1:
		type = DSI_CMD_UIR_LOADING_EFFECT_MODE1;
		break;
	case PANEL_UIR_LOADING_EFFECT_MODE2:
		type = DSI_CMD_UIR_LOADING_EFFECT_MODE2;
		break;
	case PANEL_UIR_LOADING_EFFECT_MODE3:
		type = DSI_CMD_UIR_LOADING_EFFECT_MODE3;
		break;
	default:
		type = DSI_CMD_LOADING_EFFECT_OFF;
		OPLUS_DSI_ERR("[%s] Invalid loading effect mode %d\n",
				panel->oplus_panel.vendor_name, mode);
		break;
	}

	rc = dsi_panel_tx_cmd_set(panel, type, false);
	if (rc) {
		OPLUS_DSI_ERR("Failed to send panel loading effect mode cmd\n");
	}

	return rc;
}

int dsi_panel_seed_mode(struct dsi_panel *panel, int mode)
{
	int rc = 0;

	if (!panel) {
		OPLUS_DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	if (mode >= PANEL_LOADING_EFFECT_FLAG) {
		rc = dsi_panel_loading_effect_mode_unlock(panel, mode);
	} else {
		rc = dsi_panel_seed_mode_unlock(panel, mode);
	}

	return rc;
}

int dsi_display_seed_mode_lock(struct dsi_display *display, int mode)
{
	int rc = 0;

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = dsi_panel_seed_mode(display->panel, mode);

	if (rc) {
		OPLUS_DSI_ERR("[%s] failed to seed or loading_effect on, rc=%d\n",
				display->panel->oplus_panel.vendor_name, rc);
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return rc;
}

int oplus_display_panel_get_seed(void *data)
{
	uint32_t *temp = data;

	mutex_lock(&oplus_seed_lock);

	OPLUS_DSI_INFO("get seed mode = %d\n", seed_mode);
	(*temp) = seed_mode;

	mutex_unlock(&oplus_seed_lock);

	return 0;
}

int oplus_display_panel_set_seed(void *data)
{
	uint32_t *temp_save = data;
	uint32_t panel_id = (*temp_save >> 12);
	struct dsi_display *display = oplus_display_get_current_display();
	int mode = (*temp_save & 0x0fff);

	OPLUS_DSI_INFO("set seed mode = %d, panel_id = %d\n",
			mode, panel_id);

	if (1 == panel_id) {
		display = get_sec_display();
	}

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("Invalid params\n");
		return -EINVAL;
	}

	__oplus_set_seed_mode(mode);

#ifdef OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT
	if (oplus_ofp_is_supported() && !oplus_ofp_oled_capacitive_is_enabled()
			&& !oplus_ofp_local_hbm_is_enabled()) {
		if (oplus_ofp_get_hbm_state()) {
			OFP_INFO("should not set seed in hbm state\n");
			return 0;
		}
	}
#endif /* OPLUS_FEATURE_DISPLAY_ONSCREENFINGERPRINT */

	if (display->panel->power_mode != SDE_MODE_DPMS_ON) {
		OPLUS_DSI_ERR("[%s] failed to set seed mode:%d, because display is not on\n",
				display->panel->oplus_panel.vendor_name, mode);
		return -EINVAL;
	}

	dsi_display_seed_mode_lock(display, mode);

	return 0;
}

int __oplus_read_apl_thread_ctl(bool enable)
{
	static struct task_struct *read_apl_thread = NULL;
	struct dsi_display *display = oplus_display_get_current_display();
	struct sde_connector *sde_conn;

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("display is null\n");
		return -EINVAL;
	}
	sde_conn = to_sde_connector(display->drm_conn);

	if(display->panel->oplus_panel.is_apl_read_support) {
		if(enable) {
			if(!read_apl_thread) {
				struct sched_param sp = {0};
				init_waitqueue_head(&my_queue);
				if (sde_conn) {
					atomic_set(&sde_conn->oplus_conn.bl_apl_need_update, false);
				}
				read_apl_thread = kthread_run(oplus_display_panel_update_apl_value, NULL, "read_apl_thread");
				if (IS_ERR(read_apl_thread)) {
					pr_err("Failed to create read_apl_thread\n");
					return -EINVAL;
				}

				sp.sched_priority = 16;
				sched_setscheduler(read_apl_thread, SCHED_FIFO, &sp);
			} else if (sde_conn) {
				atomic_set(&sde_conn->oplus_conn.bl_apl_need_update, true);
				wake_up_interruptible(&my_queue);
			}
		} else {
			if(read_apl_thread) {
				kthread_stop(read_apl_thread);
				read_apl_thread = NULL;
			}
		}
	}

	return 0;
}

int oplus_display_panel_update_apl_value(void *data)
{
	struct dsi_display *display = get_main_display();
	struct dsi_display_ctrl *m_ctrl = NULL;
	struct sde_connector *sde_conn;
	int ret = 0;
	int index = 0;

	if (!display || !display->panel) {
		OPLUS_DSI_ERR("display is null\n");
		return -EFAULT;
	}

	for (index = 0; index < PANEL_TX_APL_BUF; ++index) {
		oplus_rx_reg_apl[index] = 0;
	}
	oplus_rx_len_apl = 0;

	sde_conn = to_sde_connector(display->drm_conn);
	while(!kthread_should_stop()) {
		wait_event_interruptible(my_queue, atomic_read(&sde_conn->oplus_conn.bl_apl_need_update));
		if((display->panel->power_mode == SDE_MODE_DPMS_ON)
			&& sde_connector_get_property(sde_conn->base.state, CONNECTOR_PROP_UPDATE_PANEL_APL_VALUE)) {
			int len = 1;
			char switch_page[] = {0x5A, 0xA5, 0x1E};
			char read_reg1 = 0xAF;
			char read_reg2 = 0xB0;
			char reg[PANEL_TX_APL_BUF] = {0x0};
			SDE_ATRACE_BEGIN("update_panel_apl_value");

			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);

			if (display->panel->panel_initialized) {
				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle,
							DSI_CORE_CLK | DSI_LINK_CLK, DSI_CLK_ON);
				}

				ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xFF, switch_page, sizeof(switch_page));

				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle,
							DSI_CORE_CLK | DSI_LINK_CLK, DSI_CLK_OFF);
				}
			}

			m_ctrl = &display->ctrl[display->cmd_master_idx];
			ret = dsi_panel_read_panel_reg_unlock(m_ctrl, display->panel, read_reg1, &reg[0], len);
			if (ret < 0) {
				OPLUS_DSI_ERR("failed to read da ret=%d\n", ret);
			}
			oplus_rx_len_apl = len;

			ret = dsi_panel_read_panel_reg_unlock(m_ctrl, display->panel, read_reg2, &reg[1], len);
			if (ret < 0) {
				OPLUS_DSI_ERR("failed to read da ret=%d\n", ret);
			}
			oplus_rx_len_apl += len;

			for (index=0; index <= len; index++) {
				OPLUS_DSI_DEBUG("update apl reg[%d] = %x\n", index, reg[index]);
			}

			memcpy(oplus_rx_reg_apl, reg, PANEL_TX_APL_BUF);
			mutex_unlock(&display->panel->panel_lock);
			mutex_unlock(&display->display_lock);
			SDE_ATRACE_END("update_panel_apl_value");

			SDE_ATRACE_BEGIN("switch default page");
			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);
			ret = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_DEFAULT_SWITCH_PAGE, false);
			if (ret < 0) {
				OPLUS_DSI_ERR("failed to switch default page ret=%d\n", ret);
			}
			mutex_unlock(&display->panel->panel_lock);
			mutex_unlock(&display->display_lock);
			SDE_ATRACE_END("switch default page");
		}
		atomic_set(&sde_conn->oplus_conn.bl_apl_need_update, false);

	}

	return ret;
}

int oplus_get_panel_apl_value(char *buf)
{
	int i, cnt = 0;

	for (i = 0; i < oplus_rx_len_apl; i++)
		cnt += snprintf(buf + cnt, PANEL_TX_APL_BUF - cnt,
				"%02x ", oplus_rx_reg_apl[i]);

	cnt += snprintf(buf + cnt, PANEL_TX_APL_BUF - cnt, "\n");

	for (i = 0; i < PANEL_TX_APL_BUF; ++i) {
		oplus_rx_reg_apl[i] = 0;
	}
	oplus_rx_len_apl = 0;

	return cnt;
}

