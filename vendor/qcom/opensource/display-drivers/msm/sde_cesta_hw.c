// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"[sde_cesta_hw:%s:%d]: " fmt, __func__, __LINE__

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/iopoll.h>

#include "sde_cesta.h"
#include "sde_dbg.h"

#define RSCC_SEQ_PWR_CTRL_STATUS	0x2d0

#define RSCC_WRAPPER_CTRL		0x0
#define RSCC_WRAPPER_DEBUG_BUS		0x10
#define RSCC_WRAPPER_DEBUG_BUS_DATA	0x14
#define RSCC_PWR_CTRL			0x24
#define RSCC_WRAPPER_SCC_CLK_GATE_ALLOW	0x40

#define SCC_CTRL			0x0
#define SCC_OVERRIDE_CTRL		0x4
#define SCC_CLK_GATE_SEL		0x8
#define SCC_HW_STATE_READBACK		0x10
#define SCC_DEBUG_FLUSH_MISSED		0x20
#define DEBUG_FLUSH_MISSED_CLEAR	0x24
#define DISP_CC_MISC_CMD		0x0

extern void __iomem *sde_crmc_base, *sde_crm_client_base, *sde_crmb_pt_base;

void _sde_cesta_hw_init(struct sde_cesta *cesta)
{
	int i;

	for (i = 0; i < cesta->scc_count; i++) {
		dss_reg_w(&cesta->scc_io[i], SCC_CLK_GATE_SEL, 0x1, cesta->debug_mode);
		dss_reg_w(&cesta->wrapper_io, RSCC_WRAPPER_SCC_CLK_GATE_ALLOW + (0x4 * i),
				0x1, cesta->debug_mode);
	}
}

static void _sde_cesta_update_clk_gate_en(struct sde_cesta *cesta, bool en_mdp_clk_gate)
{
	u32 misc_cmd_r, misc_cmd_w;

	misc_cmd_r = dss_reg_r(&cesta->disp_cc_io, DISP_CC_MISC_CMD, cesta->debug_mode);

	if (en_mdp_clk_gate)
		cesta->mdp_clk_gate_disable_cnt--;
	else if (!en_mdp_clk_gate)
		cesta->mdp_clk_gate_disable_cnt++;

	misc_cmd_w = misc_cmd_r;

	if (cesta->mdp_clk_gate_disable_cnt == 1)
		misc_cmd_w = misc_cmd_r & ~BIT(4);
	else if (cesta->mdp_clk_gate_disable_cnt == 0)
		misc_cmd_w = misc_cmd_r | BIT(4);

	if (misc_cmd_r != misc_cmd_w) {
		dss_reg_w(&cesta->disp_cc_io, DISP_CC_MISC_CMD,	misc_cmd_w, cesta->debug_mode);
		wmb(); /* finish setting this */
	}
}

void _sde_cesta_hw_force_db_update(struct sde_cesta *cesta, u32 idx,
		bool en_auto_active, enum sde_cesta_ctrl_pwr_req_mode req_mode, bool en_hw_sleep,
		bool en_mdp_clk_gate, bool cmd_mode)
{
	u32 ctl_val, override_val;

	ctl_val = dss_reg_r(&cesta->scc_io[idx], SCC_CTRL, cesta->debug_mode);
	override_val = dss_reg_r(&cesta->scc_io[idx], SCC_OVERRIDE_CTRL, cesta->debug_mode);

	if (en_auto_active)
		ctl_val |= BIT(3); /* set auto-active-on-panic */
	else
		ctl_val &= ~BIT(3);

	if (en_hw_sleep)
		ctl_val |= BIT(0); /* set hw sleep enable */
	else
		ctl_val &= ~BIT(0);

	if (cmd_mode)
		ctl_val |= BIT(8); /* always set clk gate enable in scc_ctrl during flush*/

	/* clear & set the pwr_req mode */
	ctl_val &= ~(BIT(1) | BIT(2));
	ctl_val |= (req_mode << 1);

	override_val |= BIT(0); /* set override force-db-update */

	if (cmd_mode)
		_sde_cesta_update_clk_gate_en(cesta, en_mdp_clk_gate);

	dss_reg_w(&cesta->scc_io[idx], SCC_CTRL, ctl_val, cesta->debug_mode);
	dss_reg_w(&cesta->scc_io[idx], SCC_OVERRIDE_CTRL, override_val, cesta->debug_mode);
	wmb(); /* for reset to be applied immediately */
}

void _sde_cesta_hw_reset(struct sde_cesta *cesta, u32 idx, bool en)
{
	dss_reg_w(&cesta->scc_io[idx], SCC_OVERRIDE_CTRL, en ? BIT(31) : 0, cesta->debug_mode);
	wmb(); /* for reset to be applied immediately */
}

void _sde_cesta_hw_override_ctrl_setup(struct sde_cesta *cesta, u32 idx, u32 force_flags)
{
	u32 val = 0;

	if (force_flags & SDE_CESTA_OVERRIDE_FORCE_DB_UPDATE)
		val |= BIT(0);
	if (force_flags & SDE_CESTA_OVERRIDE_FORCE_IDLE)
		val |= BIT(1);
	if (force_flags & SDE_CESTA_OVERRIDE_FORCE_ACTIVE)
		val |= BIT(2);
	if (force_flags & SDE_CESTA_OVERRIDE_FORCE_CHN_UPDATE)
		val |= BIT(3);

	dss_reg_w(&cesta->scc_io[idx], SCC_OVERRIDE_CTRL, val, cesta->debug_mode);
	wmb(); /* for force votes to be applied immediately */
}

void _sde_cesta_hw_ctrl_setup(struct sde_cesta *cesta, u32 idx, struct sde_cesta_ctrl_cfg *cfg)
{
	u32 val = 0;

	if (!cfg || !cfg->enable) {
		dss_reg_w(&cesta->scc_io[idx], SCC_CTRL, 0xf0, cesta->debug_mode);
		_sde_cesta_hw_override_ctrl_setup(cesta, idx, SDE_CESTA_OVERRIDE_FORCE_DB_UPDATE);
		return;
	}

	if (cfg->avr_enable)
		val |= BIT(9);

	val |= BIT(8);

	val |= (cfg->req_mode << 1);

	if (cfg->wb)
		val |= (0xE << 4);
	else if (cfg->dual_dsi)
		val |= (0xD << 4);
	else
		val |= (cfg->intf << 4);

	if (cfg->auto_active_on_panic)
		val |= BIT(3);

	if (cfg->hw_sleep_enable)
		val |= BIT(0);

	dss_reg_w(&cesta->scc_io[idx], SCC_CTRL, val, cesta->debug_mode);
}

int _sde_cesta_hw_poll_handshake(struct sde_cesta *cesta, u32 idx)
{
	void __iomem *addr = cesta->scc_io[idx].base + SCC_HW_STATE_READBACK;
	u32 handshake_mask = BIT(4) | BIT(5);
	u32 handshake_vote_req = 0x1 << 4;
	u32 val;

	return readl_relaxed_poll_timeout(addr, val,
			(val & handshake_mask) != handshake_vote_req,
			100, 1000);
}

void _sde_cesta_curr_crm_votes(struct sde_cesta *cesta, u32 idx)
{
	u32 vcd0_curr_perf_ol, vcd1_curr_perf_ol, vcd0_seq_status, vcd1_seq_status;
	u32 hwclient_p_node_q_bw_vote_status, hc2_node1, hc2_node2;
	u32 hc2_node3, hc2_node4, hc2_node5;
	u32 node0_tcs_cmd_data, n1_tcs_data, n2_tcs_data, n3_tcs_data, n4_tcs_data, n5_tcs_data;
	u32 nd0_tcs_cmd_status, nd1_tcs_cmd_status, nd2_tcs_cmd_status, nd3_tcs_cmd_status, nd4_tcs_cmd_status, nd5_tcs_cmd_status;
	u32 pt_client_fsm_status, scc0_hw_state_readback, scc1_hw_state_readback, scc2_hw_state_readback;

	if (!sde_crmc_base || !sde_crm_client_base || !sde_crmb_pt_base)
		return;

	vcd0_curr_perf_ol = readl_relaxed(sde_crmc_base + 0x18); //dsi 0p9 1p2
	vcd1_curr_perf_ol = readl_relaxed(sde_crmc_base + 0x280); // mdp clk

	// read for VCD1_SEQ_STATUS
	vcd0_seq_status = readl_relaxed(sde_crmc_base + 0x40);
	vcd1_seq_status = readl_relaxed(sde_crmc_base + 0x2a8);


	// individual hw client votes ab, ib
	hwclient_p_node_q_bw_vote_status = readl_relaxed(sde_crm_client_base + 0x38 + ((0x1000) * idx) + 0x0);
	hc2_node1 = readl_relaxed(sde_crm_client_base + 0x38 + ((0x1000) * idx) + (0x14 * 1));
	hc2_node2 = readl_relaxed(sde_crm_client_base + 0x38 + ((0x1000) * idx) + (0x14 * 2));
	hc2_node3 = readl_relaxed(sde_crm_client_base + 0x38 + ((0x1000) * idx) + (0x14 * 3));
	hc2_node4 = readl_relaxed(sde_crm_client_base + 0x38 + ((0x1000) * idx) + (0x14 * 4));
	hc2_node5 = readl_relaxed(sde_crm_client_base + 0x38 + ((0x1000) * idx) + (0x14 * 5));

	// final values on epcb [0:13] -> ib, [14:27] -> ab
	node0_tcs_cmd_data = readl_relaxed(sde_crmb_pt_base + 0x0);
	n1_tcs_data = readl_relaxed(sde_crmb_pt_base + (0x14 *1));
	n2_tcs_data = readl_relaxed(sde_crmb_pt_base + (0x14 *2));
	n3_tcs_data = readl_relaxed(sde_crmb_pt_base + (0x14 *3));
	n4_tcs_data = readl_relaxed(sde_crmb_pt_base + (0x14 *4));
	n5_tcs_data = readl_relaxed(sde_crmb_pt_base + (0x14 *5));

	// read for NDn_TCS_CMD_STATUS
	nd0_tcs_cmd_status = readl_relaxed(sde_crmb_pt_base + 0xc + (0 * 0x14));
	nd1_tcs_cmd_status = readl_relaxed(sde_crmb_pt_base + 0xc + (1 * 0x14));
	nd2_tcs_cmd_status = readl_relaxed(sde_crmb_pt_base + 0xc + (2 * 0x14));
	nd3_tcs_cmd_status = readl_relaxed(sde_crmb_pt_base + 0xc + (3 * 0x14));
	nd4_tcs_cmd_status = readl_relaxed(sde_crmb_pt_base + 0xc + (4 * 0x14));
	nd5_tcs_cmd_status = readl_relaxed(sde_crmb_pt_base + 0xc + (5 * 0x14));

	// read for PT_CLIENT_FSM_STATUS
	pt_client_fsm_status = readl_relaxed(sde_crmb_pt_base + 0x7c);

	// read for SCC_2_HW_STATE_READBACK
	scc0_hw_state_readback = dss_reg_r(&cesta->scc_io[0], SCC_HW_STATE_READBACK, cesta->debug_mode);
	scc1_hw_state_readback = dss_reg_r(&cesta->scc_io[1], SCC_HW_STATE_READBACK, cesta->debug_mode);
	scc2_hw_state_readback = dss_reg_r(&cesta->scc_io[2], SCC_HW_STATE_READBACK, cesta->debug_mode);

	SDE_EVT32(vcd0_curr_perf_ol, vcd1_curr_perf_ol, hwclient_p_node_q_bw_vote_status, hc2_node1, hc2_node2, hc2_node3, hc2_node4, hc2_node5);
	SDE_EVT32(vcd0_seq_status,vcd1_seq_status);
	SDE_EVT32(nd0_tcs_cmd_status, nd1_tcs_cmd_status, nd2_tcs_cmd_status, nd3_tcs_cmd_status, nd4_tcs_cmd_status, nd5_tcs_cmd_status);
	SDE_EVT32(pt_client_fsm_status);
	SDE_EVT32(scc0_hw_state_readback, scc1_hw_state_readback, scc2_hw_state_readback);
	SDE_EVT32(node0_tcs_cmd_data, n1_tcs_data, n2_tcs_data, n3_tcs_data, n4_tcs_data, n5_tcs_data);
}

void _sde_cesta_hw_get_status(struct sde_cesta *cesta, u32 idx, struct sde_cesta_scc_status *status)
{
	u32 val;
	u32 debug1 = 0xc, debug2 = 0xd;
	u32 debug_val1, debug_val2;

	val = dss_reg_r(&cesta->scc_io[idx], SCC_HW_STATE_READBACK, cesta->debug_mode);

	status->frame_region = (val >> 8) & 0x3;
	status->sch_handshake = (val >> 4) & 0x3;
	status->fsm_state = val & 0x3;

	val = dss_reg_r(&cesta->scc_io[idx], SCC_DEBUG_FLUSH_MISSED, cesta->debug_mode);
	status->flush_missed_counter = val;

	/* clear flush_missed counter */
	dss_reg_w(&cesta->scc_io[idx], DEBUG_FLUSH_MISSED_CLEAR, 0x1, cesta->debug_mode);

	if (get_eng_version() == FACTORY || get_eng_version() == AGING || get_eng_version() == HIGH_TEMP_AGING) {
		debug_val1 = (debug1 << 1) | BIT(0);
		dss_reg_w(&cesta->wrapper_io, 0x10, debug_val1, cesta->debug_mode);
		wmb();
		debug_val1 = dss_reg_r(&cesta->wrapper_io, 0x14, cesta->debug_mode);

		dss_reg_w(&cesta->wrapper_io, 0x10, 0x0, cesta->debug_mode);
		wmb();

		debug_val2 = (debug2 << 1) | BIT(0);
		dss_reg_w(&cesta->wrapper_io, 0x10, debug_val2, cesta->debug_mode);
		wmb();
		debug_val2 = dss_reg_r(&cesta->wrapper_io, 0x14, cesta->debug_mode);

		dss_reg_w(&cesta->wrapper_io, 0x10, 0x0, cesta->debug_mode);
		wmb();

		SDE_EVT32(idx, debug1, debug_val1, debug2, debug_val2);
		_sde_cesta_curr_crm_votes(cesta, idx);
	}

}

u32 _sde_cesta_hw_get_pwr_event(struct sde_cesta *cesta)
{
	return dss_reg_r(&cesta->wrapper_io, RSCC_PWR_CTRL, cesta->debug_mode);
}

u32 _sde_get_rscc_pwr_ctrl_status(struct sde_cesta *cesta)
{
	return dss_reg_r(&cesta->rscc_io, RSCC_SEQ_PWR_CTRL_STATUS, cesta->debug_mode);
}

void sde_cesta_hw_init(struct sde_cesta *cesta)
{
	cesta->hw_ops.init = _sde_cesta_hw_init;
	cesta->hw_ops.ctrl_setup = _sde_cesta_hw_ctrl_setup;
	cesta->hw_ops.poll_handshake = _sde_cesta_hw_poll_handshake;
	cesta->hw_ops.get_status = _sde_cesta_hw_get_status;
	cesta->hw_ops.get_pwr_event = _sde_cesta_hw_get_pwr_event;
	cesta->hw_ops.override_ctrl_setup = _sde_cesta_hw_override_ctrl_setup;
	cesta->hw_ops.reset_ctrl = _sde_cesta_hw_reset;
	cesta->hw_ops.force_db_update = _sde_cesta_hw_force_db_update;
	cesta->hw_ops.get_rscc_pwr_ctrl_status = _sde_get_rscc_pwr_ctrl_status;
}
