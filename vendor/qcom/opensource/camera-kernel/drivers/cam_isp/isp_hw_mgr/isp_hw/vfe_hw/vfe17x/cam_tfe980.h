/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_TFE980_H_
#define _CAM_TFE980_H_
#include "cam_vfe_top_ver4.h"
#include "cam_vfe_core.h"
#include "cam_vfe_bus_ver3.h"
#include "cam_irq_controller.h"

#define CAM_TFE_980_NUM_TOP_DBG_REG          17
#define CAM_TFE_980_NUM_BAYER_DBG_REG        10
#define CAM_TFE_BUS_VER3_980_MAX_CLIENTS     28

static struct cam_vfe_top_ver4_module_desc tfe980_ipp_mod_desc[] = {
	{
		.id = 0,
		.desc = "CLC_STATS_AWB_BG_TINTLESS",
	},
	{
		.id  = 1,
		.desc = "CLC_STATS_AWB_BG_AE",
	},
	{
		.id = 2,
		.desc = "CLC_STATS_BHIST_AEC",
	},
	{
		.id = 3,
		.desc = "CLC_STATS_RS",
	},
	{
		.id = 4,
		.desc = "CLC_STATS_BFW_AWB",
	},
	{
		.id = 5,
		.desc = "CLC_STATS_AWB_BG_AWB",
	},
	{
		.id = 6,
		.desc = "CLC_STATS_BHIST_AF",
	},
	{
		.id = 7,
		.desc = "CLC_STATS_AWB_BG_ALSC",
	},
	{
		.id = 8,
		.desc = "CLC_STATS_BHIST_TMC",
	},
	{
		.id = 9,
		.desc = "CLC_COMPDECOMP_FD",
	},
	{
		.id = 10,
		.desc = "CLC_COLOR_CORRECT",
	},
	{
		.id = 11,
		.desc = "CLC_GTM",
	},
	{
		.id = 12,
		.desc = "CLC_GLUT",
	},
	{
		.id = 13,
		.desc = "CLC_COLOR_XFORM",
	},
	{
		.id = 14,
		.desc = "CLC_DOWNSCALE_MN_Y",
	},
	{
		.id  = 15,
		.desc = "CLC_DOWNSCALE_MN_C",
	},
	{
		.id = 16,
		.desc = "CLC_CROP_RND_CLAMP_FD_Y",
	},
	{
		.id = 17,
		.desc = "CLC_CROP_RND_CLAMP_FD_C",
	},
	{
		.id = 18,
		.desc = "CLC_BDS2_DEMO",
	},
	{
		.id = 19,
		.desc = "CLC_PUNCH_BDS2",
	},
	{
		.id = 20,
		.desc = "CLC_PUNCH_DS4_MUX",
	},
	{
		.id = 21,
		.desc = "CLC_BAYER_DS_4_DS4",
	},
	{
		.id = 22,
		.desc = "CLC_CROP_RND_CLAMP_DS4"
	},
	{
		.id = 23,
		.desc = "CLC_PUNCH_DS16"
	},
	{
		.id = 24,
		.desc = "CLC_BAYER_DS_4_DS16",
	},
	{
		.id = 25,
		.desc = "CLC_CROP_RND_CLAMP_DS16",
	},
	{
		.id = 26,
		.desc = "CLC_CROP_RND_CLAMP_DS2",
	},
	{
		.id = 27,
		.desc = "CLC_RCS_DS2",
	},
	{
		.id = 28,
		.desc = "CLC_CROP_RND_CLAMP_FULL_OUT",
	},
	{
		.id = 29,
		.desc = "CLC_COMPDECOMP_BYPASS",
	},
	{
		.id = 30,
		.desc = "CLC_CROP_RND_CLAMP_BYPASS",
	},
	{
		.id = 31,
		.desc = "CLC_RCS_FULL_OUT",
	},
};

struct cam_vfe_top_ver4_module_desc tfe980_bayer_mod_desc[] = {
	{
		.id = 0,
		.desc = "CLC_DEMUX",
	},
	{
		.id = 1,
		.desc = "CLC_BPC_PDPC_GIC",
	},
	{
		.id = 2,
		.desc = "CLC_PDPC_BPC_1D",
	},
	{
		.id = 3,
		.desc = "CLC_ABF_BINC",
	},
	{
		.id = 4,
		.desc = "CLC_CHANNEL_GAINS",
	},
	{
		.id = 5,
		.desc = "CLC_LSC",
	},
	{
		.id = 6,
		.desc = "CLC_FCG",
	},
	{
		.id = 7,
		.desc = "CLC_WB_GAIN",
	},
	{
		.id = 8,
		.desc = "CLC_COMPDECOMP_BAYER",
	},
	{
		.id = 9,
		.desc = "CLC_CROP_RND_CLAMP_WIRC",
	},
};

static struct cam_vfe_top_ver4_wr_client_desc tfe980_wr_client_desc[] = {
	{
		.wm_id = 0,
		.desc = "VIDEO",
	},
	{
		.wm_id = 1,
		.desc = "DS4_Y",
	},
	{
		.wm_id = 2,
		.desc = "DS4_C",
	},
	{
		.wm_id = 3,
		.desc  = "DS16_Y",
	},
	{
		.wm_id = 4,
		.desc = "DS16_C",
	},
	{
		.wm_id = 5,
		.desc = "DS2_Y",
	},
	{
		.wm_id = 6,
		.desc = "DS2_C",
	},
	{
		.wm_id = 7,
		.desc = "FD_Y",
	},
	{
		.wm_id = 8,
		.desc = "FD_C",
	},
	{
		.wm_id = 9,
		.desc = "IR_OUT",
	},
	{
		.wm_id = 10,
		.desc = "STATS_AEC_BE",
	},
	{
		.wm_id = 11,
		.desc = "STATS_AEC_BHIST",
	},
	{
		.wm_id = 12,
		.desc = "STATS_TINTLESS_BG",
	},
	{
		.wm_id = 13,
		.desc = "STATS_AWB_BG",
	},
	{
		.wm_id = 14,
		.desc = "STATS_AWB_BFW",
	},
	{
		.wm_id = 15,
		.desc = "STATS_AF_BHIST",
	},
	{
		.wm_id = 16,
		.desc = "STATS_ALSC_BG",
	},
	{
		.wm_id = 17,
		.desc = "STATS_FLICKER",
	},
	{
		.wm_id = 18,
		.desc = "STATS_TMC_BHIST",
	},
	{
		.wm_id = 19,
		.desc = "PDAF_0_STATS",
	},
	{
		.wm_id = 20,
		.desc = "PDAF_1_PREPROCESS_2PD",
	},
	{
		.wm_id = 21,
		.desc = "PDAF_2_PARSED_DATA",
	},
	{
		.wm_id = 22,
		.desc = "PDAF_3_CAF",
	},
	{
		.wm_id = 23,
		.desc = "RDI0",
	},
	{
		.wm_id = 24,
		.desc = "RDI1",
	},
	{
		.wm_id = 25,
		.desc = "RDI2",
	},
	{
		.wm_id = 26,
		.desc = "RDI3",
	},
	{
		.wm_id = 27,
		.desc = "RDI4",
	},
};

static struct cam_vfe_top_ver4_top_err_irq_desc tfe980_top_irq_err_desc[] = {
	{
		.bitmask = BIT(2),
		.err_name = "BAYER_HM violation",
		.desc = "CLC CCIF Violation",
	},
	{
		.bitmask = BIT(24),
		.err_name = "DYNAMIC PDAF SWITCH VIOLATION",
		.desc =
			"HAF RDI exposure select changes dynamically, the common vbi is insufficient",
	},
	{
		.bitmask = BIT(25),
		.err_name  = "HAF violation",
		.desc = "CLC_HAF Violation",
	},
	{
		.bitmask = BIT(26),
		.err_name = "PP VIOLATION",
		.desc = "CCIF protocol violation",
	},
	{
		.bitmask  = BIT(27),
		.err_name = "DIAG VIOLATION",
		.desc = "Sensor: The HBI at TFE input is less than the spec (64 cycles)",
		.debug = "Check sensor config",
	},
};

static struct cam_vfe_top_ver4_pdaf_violation_desc tfe980_haf_violation_desc[] = {
	{
		.bitmask = BIT(0),
		.desc = "Sim monitor 1 violation - SAD output",
	},
	{
		.bitmask = BIT(1),
		.desc = "Sim monitor 2 violation - pre-proc output",
	},
	{
		.bitmask = BIT(2),
		.desc = "Sim monitor 3 violation - parsed output",
	},
	{
		.bitmask = BIT(3),
		.desc = "Sim monitor 4 violation - CAF output",
	},
	{
		.bitmask  = BIT(4),
		.desc = "PDAF constraint violation",
	},
	{
		.bitmask = BIT(5),
		.desc = "CAF constraint violation",
	},
};

static struct cam_vfe_top_ver4_pdaf_lcr_res_info tfe980_pdaf_haf_res_mask[] = {
	{
		.res_id = CAM_ISP_HW_VFE_IN_RDI0,
		.val = 0,
	},
	{
		.res_id = CAM_ISP_HW_VFE_IN_RDI1,
		.val = 1,
	},
	{
		.res_id = CAM_ISP_HW_VFE_IN_RDI2,
		.val = 2,
	},
};

static struct cam_irq_register_set tfe980_top_irq_reg_set = {
	.mask_reg_offset   = 0x00000080,
	.clear_reg_offset  = 0x00000084,
	.status_reg_offset = 0x00000088,
	.set_reg_offset    = 0x0000008C,
	.test_set_val      = BIT(0),
	.test_sub_val      = BIT(0),
};

static struct cam_irq_controller_reg_info tfe980_top_irq_reg_info = {
	.num_registers = 1,
	.irq_reg_set = &tfe980_top_irq_reg_set,
	.global_irq_cmd_offset = 0x0000007C,
	.global_clear_bitmask  = 0x00000001,
	.global_set_bitmask    = 0x00000010,
	.clear_all_bitmask     = 0xFFFFFFFF,
};

static uint32_t tfe980_top_debug_reg[] = {
	0x00000190,
	0x00000194,
	0x00000198,
	0x0000019C,
	0x000001A0,
	0x000001A4,
	0x000001A8,
	0x000001AC,
	0x000001B0,
	0x000001B4,
	0x000001B8,
	0x000001BC,
	0x000001C0,
	0x000001C4,
	0x000001C8,
	0x000001CC,
	0x000001D0,
};

static uint32_t tfe980_bayer_debug_reg[] = {
	0x0000C1BC,
	0x0000C1C0,
	0x0000C1C4,
	0x0000C1C8,
	0x0000C1CC,
	0x0000C1D0,
	0x0000C1D4,
	0x0000C1D8,
	0x0000C1DC,
	0x0000C1E0,
};

static struct cam_vfe_top_ver4_reg_offset_common tfe980_top_common_reg = {
	.hw_version               = 0x00000000,
	.hw_capability            = 0x00000004,
	.main_feature             = 0x00000008,
	.bayer_feature            = 0x0000000C,
	.stats_feature            = 0x00000010,
	.fd_feature               = 0x00000014,
	.core_cgc_ovd_0           = 0x00000018,
	.ahb_cgc_ovd              = 0x00000020,
	.core_mux_cfg             = 0x00000024,
	.pdaf_input_cfg_0         = 0x00000028,
	.pdaf_input_cfg_1         = 0x0000002C,
	.stats_throttle_cfg_0     = 0x00000030,
	.stats_throttle_cfg_1     = 0x00000034,
	.stats_throttle_cfg_2     = 0x00000038,
	.core_cfg_4               = 0x0000003C,
	.pdaf_parsed_throttle_cfg = 0x00000040,
	.wirc_throttle_cfg        = 0x00000044,
	.fd_y_throttle_cfg        = 0x00000048,
	.fd_c_throttle_cfg        = 0x0000004C,
	.ds16_g_throttle_cfg      = 0x00000050,
	.ds16_br_throttle_cfg     = 0x00000054,
	.ds4_g_throttle_cfg       = 0x00000058,
	.ds4_br_throttle_cfg      = 0x0000005C,
	.ds2_g_throttle_cfg       = 0x00000060,
	.ds2_br_throttle_cfg      = 0x00000064,
	.full_out_throttle_cfg    = 0x00000068,
	.diag_config              = 0x00000094,
	.global_reset_cmd         = 0x0000007C,
	.diag_sensor_status       = {0x00000098, 0x0000009C},
	.diag_frm_cnt_status      = {0x000000A0, 0x000000A4, 0x000000A8},
	.ipp_violation_status     = 0x00000090,
	.bayer_violation_status   = 0x0000C024,
	.pdaf_violation_status    = 0x00009304,
	.dsp_status               = 0x0000006C,
	.bus_violation_status     = 0x00000864,
	.bus_overflow_status      = 0x00000868,
	.num_perf_counters        = 8,
	.perf_count_reg = {
		{
			.perf_count_cfg    = 0x000000AC,
			.perf_count_cfg_mc = 0x000000B0,
			.perf_pix_count    = 0x000000B4,
			.perf_line_count   = 0x000000B8,
			.perf_stall_count  = 0x000000BC,
			.perf_always_count = 0x000000C0,
			.perf_count_status = 0x000000C4,
		},
		{
			.perf_count_cfg    = 0x000000C8,
			.perf_count_cfg_mc = 0x000000CC,
			.perf_pix_count    = 0x000000D0,
			.perf_line_count   = 0x000000D4,
			.perf_stall_count  = 0x000000D8,
			.perf_always_count = 0x000000DC,
			.perf_count_status = 0x000000E0,
		},
		{
			.perf_count_cfg    = 0x000000E4,
			.perf_count_cfg_mc = 0x000000E8,
			.perf_pix_count    = 0x000000EC,
			.perf_line_count   = 0x000000F0,
			.perf_stall_count  = 0x000000F4,
			.perf_always_count = 0x000000F8,
			.perf_count_status = 0x000000FC,
		},
		{
			.perf_count_cfg    = 0x00000100,
			.perf_count_cfg_mc = 0x00000104,
			.perf_pix_count    = 0x00000108,
			.perf_line_count   = 0x0000010C,
			.perf_stall_count  = 0x00000110,
			.perf_always_count = 0x00000114,
			.perf_count_status = 0x00000118,
		},
		/*  Bayer per count regs from here onwards */
		{
			.perf_count_cfg    = 0x0000C028,
			.perf_count_cfg_mc = 0x0000C02C,
			.perf_pix_count    = 0x0000C030,
			.perf_line_count   = 0x0000C034,
			.perf_stall_count  = 0x0000C038,
			.perf_always_count = 0x0000C03C,
			.perf_count_status = 0x0000C040,
		},
		{
			.perf_count_cfg    = 0x0000C044,
			.perf_count_cfg_mc = 0x0000C048,
			.perf_pix_count    = 0x0000C04C,
			.perf_line_count   = 0x0000C050,
			.perf_stall_count  = 0x0000C054,
			.perf_always_count = 0x0000C058,
			.perf_count_status = 0x0000C05C,
		},
		{
			.perf_count_cfg    = 0x0000C060,
			.perf_count_cfg_mc = 0x0000C064,
			.perf_pix_count    = 0x0000C068,
			.perf_line_count   = 0x0000C06C,
			.perf_stall_count  = 0x0000C070,
			.perf_always_count = 0x0000C074,
			.perf_count_status = 0x0000C078,
		},
		{
			.perf_count_cfg    = 0x0000C07C,
			.perf_count_cfg_mc = 0x0000C080,
			.perf_pix_count    = 0x0000C084,
			.perf_line_count   = 0x0000C088,
			.perf_stall_count  = 0x0000C08C,
			.perf_always_count = 0x0000C090,
			.perf_count_status = 0x0000C094,
		},
	},
	.top_debug_cfg            = 0x000001EC,
	.bayer_debug_cfg          = 0x0000C1EC,
	.num_top_debug_reg        = CAM_TFE_980_NUM_TOP_DBG_REG,
	.top_debug = tfe980_top_debug_reg,
	.num_bayer_debug_reg = CAM_TFE_980_NUM_BAYER_DBG_REG,
	.bayer_debug = tfe980_bayer_debug_reg,
	.frame_timing_irq_reg_idx = CAM_IFE_IRQ_CAMIF_REG_STATUS0,
	.capabilities = CAM_VFE_COMMON_CAP_SKIP_CORE_CFG |
			CAM_VFE_COMMON_CAP_CORE_MUX_CFG,
};

static struct cam_vfe_ver4_path_reg_data tfe980_ipp_common_reg_data = {
	.sof_irq_mask                    = 0x150,
	.eof_irq_mask                    = 0x2A0,
	.error_irq_mask                  = 0xF000004,
	.ipp_violation_mask              = 0x4000000,
	.bayer_violation_mask            = 0x4,
	.pdaf_violation_mask             = 0x2000000,
	.diag_violation_mask             = 0x8000000,
	.diag_sensor_sel_mask            = 0x6,
	.diag_frm_count_mask_0           = 0xF000,
	.enable_diagnostic_hw            = 0x1,
	.top_debug_cfg_en                = 3,
	.is_mc_path                      = true,
	/* SOF and EOF mask combined for each context */
	.frm_irq_hw_ctxt_mask = {
		0x30,
		0xC0,
		0x300,
	},
};

static struct cam_vfe_ver4_path_reg_data tfe980_pdlib_reg_data = {
	.sof_irq_mask                    = 0x400,
	.eof_irq_mask                    = 0x800,
	.diag_sensor_sel_mask            = 0x8,
	.diag_frm_count_mask_0           = 0x40,
	.enable_diagnostic_hw            = 0x40,
	.top_debug_cfg_en                = 3,
};

static struct cam_vfe_ver4_path_reg_data tfe980_vfe_full_rdi_reg_data[5] = {
	{
		.sof_irq_mask                    = 0x1000,
		.eof_irq_mask                    = 0x2000,
		.error_irq_mask                  = 0x0,
		.diag_sensor_sel_mask            = 0xA,
		.diag_frm_count_mask_0           = 0x80,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 3,
	},
	{
		.sof_irq_mask                    = 0x4000,
		.eof_irq_mask                    = 0x8000,
		.error_irq_mask                  = 0x0,
		.diag_sensor_sel_mask            = 0xC,
		.diag_frm_count_mask_0           = 0x100,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 3,
	},
	{
		.sof_irq_mask                    = 0x10000,
		.eof_irq_mask                    = 0x20000,
		.error_irq_mask                  = 0x0,
		.diag_sensor_sel_mask            = 0xE,
		.diag_frm_count_mask_0           = 0x200,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 3,
	},
	{
		.sof_irq_mask                    = 0x40000,
		.eof_irq_mask                    = 0x80000,
		.error_irq_mask                  = 0x0,
		.diag_sensor_sel_mask            = 0x10,
		.diag_frm_count_mask_0           = 0x400,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 3,
	},
	{
		.sof_irq_mask                    = 0x100000,
		.eof_irq_mask                    = 0x200000,
		.error_irq_mask                  = 0x0,
		.diag_sensor_sel_mask            = 0x12,
		.diag_frm_count_mask_0           = 0x800,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 3,
	},
};

struct cam_vfe_ver4_path_hw_info
	tfe980_rdi_hw_info_arr[] = {
	{
		.common_reg     = &tfe980_top_common_reg,
		.reg_data       = &tfe980_vfe_full_rdi_reg_data[0],
	},
	{
		.common_reg     = &tfe980_top_common_reg,
		.reg_data       = &tfe980_vfe_full_rdi_reg_data[1],
	},
	{
		.common_reg     = &tfe980_top_common_reg,
		.reg_data       = &tfe980_vfe_full_rdi_reg_data[2],
	},
	{
		.common_reg     = &tfe980_top_common_reg,
		.reg_data       = &tfe980_vfe_full_rdi_reg_data[3],
	},
	{
		.common_reg     = &tfe980_top_common_reg,
		.reg_data       = &tfe980_vfe_full_rdi_reg_data[4],
	},
};

static struct cam_vfe_top_ver4_debug_reg_info tfe980_top_dbg_reg_info[
	CAM_TFE_980_NUM_TOP_DBG_REG][8] = {
	VFE_DBG_INFO_ARRAY_4bit("test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved"
	),
	{
		VFE_DBG_INFO_WITH_IDLE(0, "STATS_AWB_BG_TINTLESS",
			0x000001D4, (BIT(0) | BIT(1) | BIT(2))),
		VFE_DBG_INFO_WITH_IDLE(4, "STATS_AWB_BG_AE",
			0x000001D4, (BIT(3) | BIT(4) | BIT(5))),
		VFE_DBG_INFO_WITH_IDLE(8, "STATS_BHIST_AEC",
			0x000001D4, (BIT(6) | BIT(7) | BIT(8))),
		VFE_DBG_INFO_WITH_IDLE(12, "STATS_RS",
			0x000001D4, (BIT(9) | BIT(10) | BIT(11))),
		VFE_DBG_INFO_WITH_IDLE(16, "STATS_BFW_AWB",
			0x000001D4, (BIT(12) | BIT(13) | BIT(14))),
		VFE_DBG_INFO_WITH_IDLE(20, "STATS_AWB_BG_AWB",
			0x000001D4, (BIT(15) | BIT(16) | BIT(17))),
		VFE_DBG_INFO_WITH_IDLE(24, "STATS_BHIST_AF",
			0x000001D4, (BIT(18) | BIT(19) | BIT(20))),
		VFE_DBG_INFO_WITH_IDLE(28, "STATS_AWB_BG_ALSC",
			0x000001D4, (BIT(21) | BIT(22) | BIT(23))),
	},
	{
		VFE_DBG_INFO_WITH_IDLE(0, "STATS_BHIST_TMC",
			0x000001D4, (BIT(24) | BIT(25) | BIT(26))),
		VFE_DBG_INFO_WITH_IDLE(4, "compdecomp_fd",
			0x000001D4, BIT(27)),
		VFE_DBG_INFO_WITH_IDLE(8, "color_correct",
			0x000001D4, BIT(28)),
		VFE_DBG_INFO_WITH_IDLE(12, "gtm",
			0x000001D4, BIT(29)),
		VFE_DBG_INFO_WITH_IDLE(16, "glut",
			0x000001D4, BIT(30)),
		VFE_DBG_INFO_WITH_IDLE(20, "color_xform",
			0x000001D4, BIT(31)),
		VFE_DBG_INFO_WITH_IDLE(24, "downscale_mn_y",
			0x000001D8, BIT(0)),
		VFE_DBG_INFO_WITH_IDLE(28, "downscale_mn_c",
			0x000001D8, BIT(1)),
	},
	{
		VFE_DBG_INFO_WITH_IDLE(0, "crop_rnd_clamp_fd_y",
			0x000001D8, BIT(2)),
		VFE_DBG_INFO_WITH_IDLE(4, "crop_rnd_clamp_fd_c",
			0x000001D8, BIT(3)),
		VFE_DBG_INFO_WITH_IDLE(8, "bds2_demo",
			0x000001D8, (BIT(4) | BIT(5) | BIT(6))),
		VFE_DBG_INFO_WITH_IDLE(12, "punch_bds2",
			0x000001D8, (BIT(7) | BIT(8) | BIT(9))),
		VFE_DBG_INFO_WITH_IDLE(16, "punch_ds4_mux",
			0x000001D8, (BIT(10) | BIT(11) | BIT(12))),
		VFE_DBG_INFO_WITH_IDLE(20, "bayer_ds_4_ds4",
			0x000001D8, (BIT(13) | BIT(14) | BIT(15))),
		VFE_DBG_INFO_WITH_IDLE(24, "crop_rnd_clamp_ds4",
			0x000001D8, (BIT(16) | BIT(17) | BIT(18))),
		VFE_DBG_INFO_WITH_IDLE(28, "punch_ds16",
			0x000001D8, (BIT(19) | BIT(20) | BIT(21))),
	},
	{
		VFE_DBG_INFO_WITH_IDLE(0, "bayer_ds_4_ds16",
			0x000001D8, (BIT(22) | BIT(23) | BIT(24))),
		VFE_DBG_INFO_WITH_IDLE(4, "crop_rnd_clamp_ds16",
			0x000001D8, (BIT(25) | BIT(26) | BIT(27))),
		VFE_DBG_INFO_WITH_IDLE(8, "crop_rnd_clamp_ds2",
			0x000001D8, (BIT(28) | BIT(29) | BIT(30))),
		VFE_DBG_INFO_WITH_IDLE(12, "clc_haf",
			0x000001D8, BIT(31)),
		VFE_DBG_INFO_WITH_IDLE(16, "clc_rcs_ds2",
			0x000001DC, (BIT(0) | BIT(1) | BIT(2))),
		VFE_DBG_INFO_WITH_IDLE(20, "clc_crop_rnd_clamp_full_out",
			0x000001DC, (BIT(3) | BIT(4) | BIT(5))),
		VFE_DBG_INFO_WITH_IDLE(24, "clc_compdecomp_bypass",
			0x000001DC, (BIT(6) | BIT(7) | BIT(8))),
		VFE_DBG_INFO_WITH_IDLE(28, "clc_crop_rnd_clamp_bypass",
			0x000001DC, (BIT(9) | BIT(10) | BIT(11))),
	},
	{
		VFE_DBG_INFO_WITH_IDLE(0, "clc_rcs_full_out",
			0x000001DC, (BIT(12) | BIT(13) | BIT(14))),
		VFE_DBG_INFO_WITH_IDLE(4, "clc_haf",
			0x000001DC, BIT(15)),
		VFE_DBG_INFO_WITH_IDLE(8, "csid_tfe_ipp",
			0x000001DC, (BIT(16) | BIT(17) | BIT(18))),
		VFE_DBG_INFO_WITH_IDLE(12, "ppp_repeater",
			0x000001DC, BIT(19)),
		VFE_DBG_INFO_WITH_IDLE(16, "stats_awb_bg_tintless_throttle",
			0x000001DC, (BIT(20) | BIT(21) | BIT(22))),
		VFE_DBG_INFO_WITH_IDLE(20, "stats_awb_bg_ae_throttle",
			0x000001DC, (BIT(23) | BIT(24) | BIT(25))),
		VFE_DBG_INFO_WITH_IDLE(24, "stats_ae_bhist_throttle",
			0x000001DC, (BIT(26) | BIT(27) | BIT(28))),
		VFE_DBG_INFO_WITH_IDLE(28, "stats_bayer_rs_throttle",
			0x000001DC, (BIT(29) | BIT(30) | BIT(31))),
	},
	{
		VFE_DBG_INFO_WITH_IDLE(0, "stats_bayer_bfw_throttle",
			0x000001E0, (BIT(0) | BIT(1) | BIT(2))),
		VFE_DBG_INFO_WITH_IDLE(4, "stats_awb_bg_awb_throttle",
			0x000001E0, (BIT(3) | BIT(4) | BIT(5))),
		VFE_DBG_INFO_WITH_IDLE(8, "stats_bhist_af_throttle",
			0x000001E0, (BIT(6) | BIT(7) | BIT(8))),
		VFE_DBG_INFO_WITH_IDLE(12, "full_out_throttle",
			0x000001E0, (BIT(9) | BIT(10) | BIT(11))),
		VFE_DBG_INFO_WITH_IDLE(16, "ds4_out_y_throttle",
			0x000001E0, (BIT(12) | BIT(13) | BIT(14))),
		VFE_DBG_INFO_WITH_IDLE(20, "ds4_out_c_throttle",
			0x000001E0, (BIT(15) | BIT(16) | BIT(17))),
		VFE_DBG_INFO_WITH_IDLE(24, "ds16_out_y_throttle",
			0x000001E0, (BIT(18) | BIT(19) | BIT(20))),
		VFE_DBG_INFO_WITH_IDLE(28, "ds16_out_c_throttle",
			0x000001E0, (BIT(21) | BIT(22) | BIT(23))),
	},
	{
		VFE_DBG_INFO_WITH_IDLE(0, "ds2_out_y_throttle",
			0x000001E0, (BIT(24) | BIT(25) | BIT(26))),
		VFE_DBG_INFO_WITH_IDLE(4, "ds2_out_c_throttle",
			0x000001E0, (BIT(27) | BIT(28) | BIT(29))),
		VFE_DBG_INFO_WITH_IDLE(8, "tfe_w_ir_throttle",
			0x000001E4, (BIT(0) | BIT(1) | BIT(2))),
		VFE_DBG_INFO_WITH_IDLE(12, "fd_out_y_throttle",
			0x000001E4, (BIT(3) | BIT(4) | BIT(5))),
		VFE_DBG_INFO_WITH_IDLE(16, "fd_out_c_throttle",
			0x000001E4, (BIT(6) | BIT(7) | BIT(8))),
		VFE_DBG_INFO_WITH_IDLE(20, "haf_sad_stats_throttle",
			0x000001E0, BIT(30)),
		VFE_DBG_INFO_WITH_IDLE(24, "haf_caf_stats_throttle",
			0x000001E0, BIT(31)),
		VFE_DBG_INFO_WITH_IDLE(28, "haf_parsed_throttle",
			0x000001E4, BIT(9)),
	},
	{
		VFE_DBG_INFO_WITH_IDLE(0, "haf_pre_processed",
			0x000001E4, BIT(10)),
		VFE_DBG_INFO(4, "full_out"),
		VFE_DBG_INFO(8, "ubwc_stats"),
		VFE_DBG_INFO(12, "ds4_out_y"),
		VFE_DBG_INFO(16, "ds4_out_c"),
		VFE_DBG_INFO(20, "ds16_out_y"),
		VFE_DBG_INFO(24, "ds16_out_c"),
		VFE_DBG_INFO(28, "ds2_out_y"),
	},
	VFE_DBG_INFO_ARRAY_4bit(
		"ubwc_stats",
		"ds2_out_c",
		"fd_out_y",
		"fd_out_c",
		"raw_out",
		"stats_awb_bg_ae",
		"stats_ae_bhist",
		"stats_awb_bg_tintless"
	),
	{
		VFE_DBG_INFO_WITH_IDLE(0, "stats_awb_bg_alsc",
			0x000001E4, (BIT(20) | BIT(21) | BIT(22))),
		VFE_DBG_INFO(4, "stats_throttle_to_bus_awb_bg_awb"),
		VFE_DBG_INFO(8, "stats_throttle_to_bus_bayer_bfw"),
		VFE_DBG_INFO(12, "stats_throttle_to_bus_bhist_af"),
		VFE_DBG_INFO(16, "stats_throttle_to_bus_awb_bg_alsc"),
		VFE_DBG_INFO(20, "stats_throttle_to_bus_bayer_rs"),
		VFE_DBG_INFO(24, "stats_throttle_to_bus_bhist_tmc"),
		VFE_DBG_INFO(28, "stats_throttle_to_bus_sad"),

	},
	VFE_DBG_INFO_ARRAY_4bit(
		"tfe_haf_processed_to_bus",
		"tfe_haf_parsed_to_bus",
		"tfe_stats_throttle_to_bus",
		"rdi0_splitter_to_bus_wr",
		"rdi1_splitter_to_bus_wr",
		"rdi2_splitter_to_bus_wr",
		"rdi3_splitter_to_bus_wr",
		"rdi4_splitter_to_bus_wr"
	),
	{
		VFE_DBG_INFO_WITH_IDLE(0, "stats_bhist_tmc_throttle",
			0x000001E4, (BIT(23) | BIT(24) | BIT(25))),
		VFE_DBG_INFO(4, "reserved"),
		VFE_DBG_INFO(8, "reserved"),
		VFE_DBG_INFO(12, "reserved"),
		VFE_DBG_INFO(16, "reserved"),
		VFE_DBG_INFO(20, "reserved"),
		VFE_DBG_INFO(24, "reserved"),
		VFE_DBG_INFO(28, "reserved"),
	},
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
	},
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
	},
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
	},
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
	},
};

static struct cam_vfe_top_ver4_debug_reg_info tfe980_bayer_dbg_reg_info[
	CAM_TFE_980_NUM_BAYER_DBG_REG][8] = {
	VFE_DBG_INFO_ARRAY_4bit("test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved"
	),
	{
		VFE_DBG_INFO_WITH_IDLE(0, "clc_demux_w0",
			0x0000C1E4, (BIT(0) | BIT(1) | BIT(2))),
		VFE_DBG_INFO_WITH_IDLE(4, "clc_bpc_pdpc_gic_w0",
			0x0000C1E4, (BIT(3) | BIT(4) | BIT(5))),
		VFE_DBG_INFO_WITH_IDLE(8, "clc_pdpc_bpc_1d_w0",
			0x0000C1E4, (BIT(6) | BIT(7) | BIT(8))),
		VFE_DBG_INFO_WITH_IDLE(12, "clc_abf_binc_w0",
			0x0000C1E4, (BIT(9) | BIT(10) | BIT(11))),
		VFE_DBG_INFO_WITH_IDLE(16, "clc_channel_gains_w0",
			0x0000C1E4, (BIT(12) | BIT(13) | BIT(14))),
		VFE_DBG_INFO_WITH_IDLE(20, "clc_lsc_w3",
			0x0000C1E4, (BIT(15) | BIT(16) | BIT(17))),
		VFE_DBG_INFO_WITH_IDLE(24, "clc_fcg_w2",
			0x0000C1E4, (BIT(18) | BIT(19) | BIT(20))),
		VFE_DBG_INFO_WITH_IDLE(28, "clc_wb_gain_w6",
			0x0000C1E4, (BIT(21) | BIT(22) | BIT(23))),
	},
	{
		VFE_DBG_INFO_WITH_IDLE(0, "clc_compdecomp_bayer_w0",
			0x0000C1E4, (BIT(24) | BIT(25) | BIT(26))),
		VFE_DBG_INFO_WITH_IDLE(4, "clc_crop_rnd_clamp_wirc_w10",
			0x0000C1E4, BIT(27)),
		VFE_DBG_INFO(8, "reserved"),
		VFE_DBG_INFO(12, "reserved"),
		VFE_DBG_INFO(16, "reserved"),
		VFE_DBG_INFO(20, "reserved"),
		VFE_DBG_INFO(24, "reserved"),
		VFE_DBG_INFO(28, "reserved"),
	},
	VFE_DBG_INFO_ARRAY_4bit(
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved",
		"reserved"
	),
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
		VFE_DBG_INFO(32, "non_ccif_0"),
	},
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
		VFE_DBG_INFO(32, "non_ccif_1"),
	},
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
		VFE_DBG_INFO(32, "non_ccif_2"),
	},
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
		VFE_DBG_INFO(32, "non_ccif_3"),
	},
};

static struct cam_vfe_top_ver4_diag_reg_info tfe980_diag_reg_info[] = {
	{
		.bitmask = 0x3FFF,
		.name    = "SENSOR_HBI",
	},
	{
		.bitmask = 0x4000,
		.name    = "SENSOR_NEQ_HBI",
	},
	{
		.bitmask = 0x8000,
		.name    = "SENSOR_HBI_MIN_ERROR",
	},
	{
		.bitmask = 0xFFFFFF,
		.name    = "SENSOR_VBI",
	},
	{
		.bitmask = 0xFF,
		.name    = "FRAME_CNT_PPP_PIPE",
	},
	{
		.bitmask = 0xFF00,
		.name    = "FRAME_CNT_RDI_0_PIPE",
	},
	{
		.bitmask = 0xFF0000,
		.name    = "FRAME_CNT_RDI_1_PIPE",
	},
	{
		.bitmask = 0xFF000000,
		.name    = "FRAME_CNT_RDI_2_PIPE",
	},
	{
		.bitmask = 0xFF,
		.name    = "FRAME_CNT_RDI_3_PIPE",
	},
	{
		.bitmask = 0xFF00,
		.name    = "FRAME_CNT_RDI_4_PIPE",
	},
	{
		.bitmask = 0xFF,
		.name    = "FRAME_CNT_IPP_CONTEXT0_PIPE",
	},
	{
		.bitmask = 0xFF00,
		.name    = "FRAME_CNT_IPP_CONTEXT1_PIPE",
	},
	{
		.bitmask = 0xFF0000,
		.name    = "FRAME_CNT_IPP_CONTEXT2_PIPE",
	},
	{
		.bitmask = 0xFF000000,
		.name    = "FRAME_CNT_IPP_ALL_CONTEXT_PIPE",
	},
};

static struct cam_vfe_top_ver4_diag_reg_fields tfe980_diag_sensor_field[] = {
	{
		.num_fields = 3,
		.field      = &tfe980_diag_reg_info[0],
	},
	{
		.num_fields = 1,
		.field      = &tfe980_diag_reg_info[3],
	},
};

static struct cam_vfe_top_ver4_diag_reg_fields tfe980_diag_frame_field[] = {
	{
		.num_fields = 4,
		.field      = &tfe980_diag_reg_info[4],
	},
	{
		.num_fields = 2,
		.field      = &tfe980_diag_reg_info[8],
	},
	{
		.num_fields = 4,
		.field      = &tfe980_diag_reg_info[10],
	},
};

static struct cam_vfe_ver4_fcg_module_info tfe980_fcg_module_info = {
	.max_fcg_ch_ctx                      = 3,
	.max_fcg_predictions                 = 3,
	.fcg_index_shift                     = 16,
	.max_reg_val_pair_size               = 6,
	.fcg_type_size                       = 2,
	.fcg_phase_index_cfg_0               = 0x0000DE70,
	.fcg_phase_index_cfg_1               = 0x0000DE74,
	.fcg_reg_ctxt_shift                  = 0x0,
	.fcg_reg_ctxt_sel                    = 0x0000DFF4,
	.fcg_reg_ctxt_mask                   = 0x7,
};

static struct cam_vfe_top_ver4_hw_info tfe980_top_hw_info = {
	.common_reg = &tfe980_top_common_reg,
	.vfe_full_hw_info = {
		.common_reg     = &tfe980_top_common_reg,
		.reg_data       = &tfe980_ipp_common_reg_data,
	},
	.pdlib_hw_info = {
		.common_reg     = &tfe980_top_common_reg,
		.reg_data       = &tfe980_pdlib_reg_data,
	},
	.rdi_hw_info            = tfe980_rdi_hw_info_arr,
	.wr_client_desc         = tfe980_wr_client_desc,
	.ipp_module_desc        = tfe980_ipp_mod_desc,
	.bayer_module_desc      = tfe980_bayer_mod_desc,
	.num_mux = 7,
	.mux_type = {
		CAM_VFE_CAMIF_VER_4_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_PDLIB_VER_1_0,
	},
	.num_path_port_map = 3,
	.path_port_map = {
		{CAM_ISP_HW_VFE_IN_PDLIB, CAM_ISP_IFE_OUT_RES_2PD},
		{CAM_ISP_HW_VFE_IN_PDLIB, CAM_ISP_IFE_OUT_RES_PREPROCESS_2PD},
		{CAM_ISP_HW_VFE_IN_PDLIB, CAM_ISP_IFE_OUT_RES_PDAF_PARSED_DATA},
	},
	.num_rdi                         = ARRAY_SIZE(tfe980_rdi_hw_info_arr),
	.num_top_errors                  = ARRAY_SIZE(tfe980_top_irq_err_desc),
	.top_err_desc                    = tfe980_top_irq_err_desc,
	.num_pdaf_violation_errors       = ARRAY_SIZE(tfe980_haf_violation_desc),
	.pdaf_violation_desc             = tfe980_haf_violation_desc,
	.top_debug_reg_info              = &tfe980_top_dbg_reg_info,
	.bayer_debug_reg_info            = &tfe980_bayer_dbg_reg_info,
	.pdaf_lcr_res_mask               = tfe980_pdaf_haf_res_mask,
	.num_pdaf_lcr_res                = ARRAY_SIZE(tfe980_pdaf_haf_res_mask),
	.fcg_module_info                 = &tfe980_fcg_module_info,
	.fcg_mc_supported                = true,
	.diag_sensor_info                = tfe980_diag_sensor_field,
	.diag_frame_info                 = tfe980_diag_frame_field,
};

static struct cam_irq_register_set tfe980_bus_irq_reg[2] = {
	{
		.mask_reg_offset   = 0x00000818,
		.clear_reg_offset  = 0x00000820,
		.status_reg_offset = 0x00000828,
	},
	{
		.mask_reg_offset   = 0x0000081C,
		.clear_reg_offset  = 0x00000824,
		.status_reg_offset = 0x0000082C,
	},
};

static struct cam_vfe_bus_ver3_reg_offset_ubwc_client
	tfe980_ubwc_regs_client_0 = {
	.meta_addr        = 0x00000D40,
	.meta_cfg         = 0x00000D44,
	.mode_cfg         = 0x00000D48,
	.stats_ctrl       = 0x00000D4C,
	.ctrl_2           = 0x00000D50,
	.lossy_thresh0    = 0x00000D54,
	.lossy_thresh1    = 0x00000D58,
	.off_lossy_var    = 0x00000D5C,
	.bw_limit         = 0x00000D1C,
	.ubwc_comp_en_bit = BIT(1),
};

static struct cam_vfe_bus_ver3_reg_offset_ubwc_client
	tfe980_ubwc_regs_client_1 = {
	.meta_addr        = 0x00000E40,
	.meta_cfg         = 0x00000E44,
	.mode_cfg         = 0x00000E48,
	.stats_ctrl       = 0x00000E4C,
	.ctrl_2           = 0x00000E50,
	.lossy_thresh0    = 0x00000E54,
	.lossy_thresh1    = 0x00000E58,
	.off_lossy_var    = 0x00000E5C,
	.bw_limit         = 0x00000E1C,
	.ubwc_comp_en_bit = BIT(1),
};

static struct cam_vfe_bus_ver3_reg_offset_ubwc_client
	tfe980_ubwc_regs_client_2 = {
	.meta_addr        = 0x00000F40,
	.meta_cfg         = 0x00000F44,
	.mode_cfg         = 0x00000F48,
	.stats_ctrl       = 0x00000F4C,
	.ctrl_2           = 0x00000F50,
	.lossy_thresh0    = 0x00000F54,
	.lossy_thresh1    = 0x00000F58,
	.off_lossy_var    = 0x00000F5C,
	.bw_limit         = 0x00000F1C,
	.ubwc_comp_en_bit = BIT(1),
};

static struct cam_vfe_bus_ver3_reg_offset_ubwc_client
	tfe980_ubwc_regs_client_3 = {
	.meta_addr        = 0x00001040,
	.meta_cfg         = 0x00001044,
	.mode_cfg         = 0x00001048,
	.stats_ctrl       = 0x0000104C,
	.ctrl_2           = 0x00001050,
	.lossy_thresh0    = 0x00001054,
	.lossy_thresh1    = 0x00001058,
	.off_lossy_var    = 0x0000105C,
	.bw_limit         = 0x0000101C,
	.ubwc_comp_en_bit = BIT(1),
};

static struct cam_vfe_bus_ver3_reg_offset_ubwc_client
	tfe980_ubwc_regs_client_4 = {
	.meta_addr        = 0x00001140,
	.meta_cfg         = 0x00001144,
	.mode_cfg         = 0x00001148,
	.stats_ctrl       = 0x0000114C,
	.ctrl_2           = 0x00001150,
	.lossy_thresh0    = 0x00001154,
	.lossy_thresh1    = 0x00001158,
	.off_lossy_var    = 0x0000115C,
	.bw_limit         = 0x0000111C,
	.ubwc_comp_en_bit = BIT(1),
};

static struct cam_vfe_bus_ver3_reg_offset_ubwc_client
	tfe980_ubwc_regs_client_5 = {
	.meta_addr        = 0x00001240,
	.meta_cfg         = 0x00001244,
	.mode_cfg         = 0x00001248,
	.stats_ctrl       = 0x0000124C,
	.ctrl_2           = 0x00001250,
	.lossy_thresh0    = 0x00001254,
	.lossy_thresh1    = 0x00001258,
	.off_lossy_var    = 0x0000125C,
	.bw_limit         = 0x0000121C,
	.ubwc_comp_en_bit = BIT(1),
};

static struct cam_vfe_bus_ver3_reg_offset_ubwc_client
	tfe980_ubwc_regs_client_6 = {
	.meta_addr        = 0x00001340,
	.meta_cfg         = 0x00001344,
	.mode_cfg         = 0x00001348,
	.stats_ctrl       = 0x0000134C,
	.ctrl_2           = 0x00001350,
	.lossy_thresh0    = 0x00001354,
	.lossy_thresh1    = 0x00001358,
	.off_lossy_var    = 0x0000135C,
	.bw_limit         = 0x0000131C,
	.ubwc_comp_en_bit = BIT(1),
};

static struct cam_vfe_bus_ver3_reg_offset_ubwc_client
	tfe980_ubwc_regs_client_20 = {
	.meta_addr        = 0x00002140,
	.meta_cfg         = 0x00002144,
	.mode_cfg         = 0x00002148,
	.stats_ctrl       = 0x0000214C,
	.ctrl_2           = 0x00002150,
	.lossy_thresh0    = 0x00002154,
	.lossy_thresh1    = 0x00002158,
	.off_lossy_var    = 0x0000215C,
	.bw_limit         = 0x0000211C,
	.ubwc_comp_en_bit = BIT(1),
};

static uint32_t tfe980_out_port_mid[][12] = {
	{58},
	{59},
	{60},
	{61},
	{62},
	{32, 34, 36, 33, 35, 37},
	{32, 34, 36, 33, 35, 37, 38, 40, 42, 39, 41, 43},
	{44, 46, 48, 45, 47, 49, 50, 52, 54, 51, 53, 55},
	{38, 40, 42, 39, 41, 43, 44, 46, 48, 45, 47, 49},
	{8, 9, 10},
	{56, 57},
	{32, 33, 34},
	{35, 36, 37},
	{38, 39, 40},
	{41, 42, 43},
	{44, 45, 46},
	{47, 48, 49},
	{50, 51, 52},
	{53, 54, 55},
	{56, 57, 58},
	{11},
	{50, 51},
	{12},
	{13},
};

static struct cam_vfe_bus_ver3_hw_info tfe980_bus_hw_info = {
	.common_reg = {
		.hw_version                       = 0x00000800,
		.cgc_ovd                          = 0x00000808,
		.ctxt_sel                         = 0x00000924,
		.ubwc_static_ctrl                 = 0x00000858,
		.pwr_iso_cfg                      = 0x0000085C,
		.overflow_status_clear            = 0x00000860,
		.ccif_violation_status            = 0x00000864,
		.overflow_status                  = 0x00000868,
		.image_size_violation_status      = 0x00000870,
		.debug_status_top_cfg             = 0x000008F0,
		.debug_status_top                 = 0x000008F4,
		.test_bus_ctrl                    = 0x00000928,
		.mc_read_sel_shift                = 0x5,
		.mc_write_sel_shift               = 0x0,
		.mc_ctxt_mask                     = 0x7,
		.wm_mode_shift                    = 16,
		.wm_mode_val                      = { 0x0, 0x1, 0x2 },
		.wm_en_shift                      = 0,
		.frmheader_en_shift               = 2,
		.virtual_frm_en_shift             = 1,
		.irq_reg_info = {
			.num_registers            = 2,
			.irq_reg_set              = tfe980_bus_irq_reg,
			.global_irq_cmd_offset    = 0x00000830,
			.global_clear_bitmask     = 0x00000001,
		},
	},
	.num_client = CAM_TFE_BUS_VER3_980_MAX_CLIENTS,
	.bus_client_reg = {
		/* BUS Client 0 FULL */
		{
			.cfg                      = 0x00000D00,
			.image_addr               = 0x00000D04,
			.frame_incr               = 0x00000D08,
			.image_cfg_0              = 0x00000D0C,
			.image_cfg_1              = 0x00000D10,
			.image_cfg_2              = 0x00000D14,
			.packer_cfg               = 0x00000D18,
			.frame_header_addr        = 0x00000D20,
			.frame_header_incr        = 0x00000D24,
			.frame_header_cfg         = 0x00000D28,
			.irq_subsample_period     = 0x00000D30,
			.irq_subsample_pattern    = 0x00000D34,
			.framedrop_period         = 0x00000D38,
			.framedrop_pattern        = 0x00000D3C,
			.mmu_prefetch_cfg         = 0x00000D60,
			.mmu_prefetch_max_offset  = 0x00000D64,
			.system_cache_cfg         = 0x00000D68,
			.addr_cfg                 = 0x00000D70,
			.ctxt_cfg                 = 0x00000D78,
			.addr_status_0            = 0x00000D90,
			.addr_status_1            = 0x00000D94,
			.addr_status_2            = 0x00000D98,
			.addr_status_3            = 0x00000D9C,
			.debug_status_cfg         = 0x00000D7C,
			.debug_status_0           = 0x00000D80,
			.debug_status_1           = 0x00000D84,
			.bw_limiter_addr          = 0x00000D1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = &tfe980_ubwc_regs_client_0,
			.supported_formats        = BIT_ULL(CAM_FORMAT_BAYER_UBWC_TP10)|
				BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_12) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_14) |
				BIT_ULL(CAM_FORMAT_PLAIN8) | BIT_ULL(CAM_FORMAT_PLAIN16_8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10) | BIT_ULL(CAM_FORMAT_PLAIN16_12) |
				BIT_ULL(CAM_FORMAT_PLAIN16_14) | BIT_ULL(CAM_FORMAT_PLAIN16_16)
#ifdef OPLUS_FEATURE_CAMERA_COMMON
				|BIT_ULL(CAM_FORMAT_PLAIN16_10_LSB)
#endif
				,
			.rcs_en_mask             =  0x200,
		},
		/* BUS Client 1 DS4_Y */
		{
			.cfg                      = 0x00000E00,
			.image_addr               = 0x00000E04,
			.frame_incr               = 0x00000E08,
			.image_cfg_0              = 0x00000E0C,
			.image_cfg_1              = 0x00000E10,
			.image_cfg_2              = 0x00000E14,
			.packer_cfg               = 0x00000E18,
			.frame_header_addr        = 0x00000E20,
			.frame_header_incr        = 0x00000E24,
			.frame_header_cfg         = 0x00000E28,
			.irq_subsample_period     = 0x00000E30,
			.irq_subsample_pattern    = 0x00000E34,
			.framedrop_period         = 0x00000E38,
			.framedrop_pattern        = 0x00000E3C,
			.mmu_prefetch_cfg         = 0x00000E60,
			.mmu_prefetch_max_offset  = 0x00000E64,
			.system_cache_cfg         = 0x00000E68,
			.addr_cfg                 = 0x00000E70,
			.ctxt_cfg                 = 0x00000E78,
			.addr_status_0            = 0x00000E90,
			.addr_status_1            = 0x00000E94,
			.addr_status_2            = 0x00000E98,
			.addr_status_3            = 0x00000E9C,
			.debug_status_cfg         = 0x00000E7C,
			.debug_status_0           = 0x00000E80,
			.debug_status_1           = 0x00000E84,
			.bw_limiter_addr          = 0x00000E1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = &tfe980_ubwc_regs_client_1,
			.supported_formats        = BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_UBWC_TP10),
		},
		/* BUS Client 2 DS4_C */
		{
			.cfg                      = 0x00000F00,
			.image_addr               = 0x00000F04,
			.frame_incr               = 0x00000F08,
			.image_cfg_0              = 0x00000F0C,
			.image_cfg_1              = 0x00000F10,
			.image_cfg_2              = 0x00000F14,
			.packer_cfg               = 0x00000F18,
			.frame_header_addr        = 0x00000F20,
			.frame_header_incr        = 0x00000F24,
			.frame_header_cfg         = 0x00000F28,
			.irq_subsample_period     = 0x00000F30,
			.irq_subsample_pattern    = 0x00000F34,
			.framedrop_period         = 0x00000F38,
			.framedrop_pattern        = 0x00000F3C,
			.mmu_prefetch_cfg         = 0x00000F60,
			.mmu_prefetch_max_offset  = 0x00000F64,
			.system_cache_cfg         = 0x00000F68,
			.addr_cfg                 = 0x00000F70,
			.ctxt_cfg                 = 0x00000F78,
			.addr_status_0            = 0x00000F90,
			.addr_status_1            = 0x00000F94,
			.addr_status_2            = 0x00000F98,
			.addr_status_3            = 0x00000F9C,
			.debug_status_cfg         = 0x00000F7C,
			.debug_status_0           = 0x00000F80,
			.debug_status_1           = 0x00000F84,
			.bw_limiter_addr          = 0x00000F1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = &tfe980_ubwc_regs_client_2,
			.supported_formats        = BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_UBWC_TP10),
		},
		/* BUS Client 3 DS16_Y */
		{
			.cfg                      = 0x00001000,
			.image_addr               = 0x00001004,
			.frame_incr               = 0x00001008,
			.image_cfg_0              = 0x0000100C,
			.image_cfg_1              = 0x00001010,
			.image_cfg_2              = 0x00001014,
			.packer_cfg               = 0x00001018,
			.frame_header_addr        = 0x00001020,
			.frame_header_incr        = 0x00001024,
			.frame_header_cfg         = 0x00001028,
			.irq_subsample_period     = 0x00001030,
			.irq_subsample_pattern    = 0x00001034,
			.framedrop_period         = 0x00001038,
			.framedrop_pattern        = 0x0000103C,
			.mmu_prefetch_cfg         = 0x00001060,
			.mmu_prefetch_max_offset  = 0x00001064,
			.system_cache_cfg         = 0x00001068,
			.addr_cfg                 = 0x00001070,
			.ctxt_cfg                 = 0x00001078,
			.addr_status_0            = 0x00001090,
			.addr_status_1            = 0x00001094,
			.addr_status_2            = 0x00001098,
			.addr_status_3            = 0x0000109C,
			.debug_status_cfg         = 0x0000107C,
			.debug_status_0           = 0x00001080,
			.debug_status_1           = 0x00001084,
			.bw_limiter_addr          = 0x0000101C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = &tfe980_ubwc_regs_client_3,
			.supported_formats        = BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_UBWC_TP10),
		},
		/* BUS Client 4 DS16_C */
		{
			.cfg                      = 0x00001100,
			.image_addr               = 0x00001104,
			.frame_incr               = 0x00001108,
			.image_cfg_0              = 0x0000110C,
			.image_cfg_1              = 0x00001110,
			.image_cfg_2              = 0x00001114,
			.packer_cfg               = 0x00001118,
			.frame_header_addr        = 0x00001120,
			.frame_header_incr        = 0x00001124,
			.frame_header_cfg         = 0x00001128,
			.irq_subsample_period     = 0x00001130,
			.irq_subsample_pattern    = 0x00001134,
			.framedrop_period         = 0x00001138,
			.framedrop_pattern        = 0x0000113C,
			.mmu_prefetch_cfg         = 0x00001160,
			.mmu_prefetch_max_offset  = 0x00001164,
			.system_cache_cfg         = 0x00001168,
			.addr_cfg                 = 0x00001170,
			.ctxt_cfg                 = 0x00001178,
			.addr_status_0            = 0x00001190,
			.addr_status_1            = 0x00001194,
			.addr_status_2            = 0x00001198,
			.addr_status_3            = 0x0000119C,
			.debug_status_cfg         = 0x0000117C,
			.debug_status_0           = 0x00001180,
			.debug_status_1           = 0x00001184,
			.bw_limiter_addr          = 0x0000111C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = &tfe980_ubwc_regs_client_4,
			.supported_formats        = BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_UBWC_TP10),
		},
		/* BUS Client 5 DS2_Y */
		{
			.cfg                      = 0x00001200,
			.image_addr               = 0x00001204,
			.frame_incr               = 0x00001208,
			.image_cfg_0              = 0x0000120C,
			.image_cfg_1              = 0x00001210,
			.image_cfg_2              = 0x00001214,
			.packer_cfg               = 0x00001218,
			.frame_header_addr        = 0x00001220,
			.frame_header_incr        = 0x00001224,
			.frame_header_cfg         = 0x00001228,
			.irq_subsample_period     = 0x00001230,
			.irq_subsample_pattern    = 0x00001234,
			.framedrop_period         = 0x00001238,
			.framedrop_pattern        = 0x0000123C,
			.mmu_prefetch_cfg         = 0x00001260,
			.mmu_prefetch_max_offset  = 0x00001264,
			.system_cache_cfg         = 0x00001268,
			.addr_cfg                 = 0x00001270,
			.ctxt_cfg                 = 0x00001278,
			.addr_status_0            = 0x00001290,
			.addr_status_1            = 0x00001294,
			.addr_status_2            = 0x00001298,
			.addr_status_3            = 0x0000129C,
			.debug_status_cfg         = 0x0000127C,
			.debug_status_0           = 0x00001280,
			.debug_status_1           = 0x00001284,
			.bw_limiter_addr          = 0x0000121C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = &tfe980_ubwc_regs_client_5,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_8) | BIT_ULL(CAM_FORMAT_PLAIN16_10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_12) | BIT_ULL(CAM_FORMAT_PLAIN16_14) |
				BIT_ULL(CAM_FORMAT_PLAIN16_16) |
#ifdef OPLUS_FEATURE_CAMERA_COMMON
				BIT_ULL(CAM_FORMAT_PLAIN16_10_LSB)|
#endif
				BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_UBWC_TP10) |
				BIT_ULL(CAM_FORMAT_BAYER_UBWC_TP10),
			.rcs_en_mask             =  0x200,
		},
		/* BUS Client 6 DS2_C */
		{
			.cfg                      = 0x00001300,
			.image_addr               = 0x00001304,
			.frame_incr               = 0x00001308,
			.image_cfg_0              = 0x0000130C,
			.image_cfg_1              = 0x00001310,
			.image_cfg_2              = 0x00001314,
			.packer_cfg               = 0x00001318,
			.frame_header_addr        = 0x00001320,
			.frame_header_incr        = 0x00001324,
			.frame_header_cfg         = 0x00001328,
			.irq_subsample_period     = 0x00001330,
			.irq_subsample_pattern    = 0x00001334,
			.framedrop_period         = 0x00001338,
			.framedrop_pattern        = 0x0000133C,
			.mmu_prefetch_cfg         = 0x00001360,
			.mmu_prefetch_max_offset  = 0x00001364,
			.system_cache_cfg         = 0x00001368,
			.addr_cfg                 = 0x00001370,
			.ctxt_cfg                 = 0x00001378,
			.addr_status_0            = 0x00001390,
			.addr_status_1            = 0x00001394,
			.addr_status_2            = 0x00001398,
			.addr_status_3            = 0x0000139C,
			.debug_status_cfg         = 0x0000137C,
			.debug_status_0           = 0x00001380,
			.debug_status_1           = 0x00001384,
			.bw_limiter_addr          = 0x0000131C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = &tfe980_ubwc_regs_client_6,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_8) | BIT_ULL(CAM_FORMAT_PLAIN16_10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_12) | BIT_ULL(CAM_FORMAT_PLAIN16_14) |
				BIT_ULL(CAM_FORMAT_PLAIN16_16) |
#ifdef OPLUS_FEATURE_CAMERA_COMMON
				BIT_ULL(CAM_FORMAT_PLAIN16_10_LSB)|
#endif
				BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_TP10) |
				BIT_ULL(CAM_FORMAT_GBR_UBWC_TP10) |
				BIT_ULL(CAM_FORMAT_BAYER_UBWC_TP10),

			.rcs_en_mask             =  0x200,
		},
		/* BUS Client 7 FD_Y */
		{
			.cfg                      = 0x00001400,
			.image_addr               = 0x00001404,
			.frame_incr               = 0x00001408,
			.image_cfg_0              = 0x0000140C,
			.image_cfg_1              = 0x00001410,
			.image_cfg_2              = 0x00001414,
			.packer_cfg               = 0x00001418,
			.frame_header_addr        = 0x00001420,
			.frame_header_incr        = 0x00001424,
			.frame_header_cfg         = 0x00001428,
			.irq_subsample_period     = 0x00001430,
			.irq_subsample_pattern    = 0x00001434,
			.framedrop_period         = 0x00001438,
			.framedrop_pattern        = 0x0000143C,
			.mmu_prefetch_cfg         = 0x00001460,
			.mmu_prefetch_max_offset  = 0x00001464,
			.system_cache_cfg         = 0x00001468,
			.addr_cfg                 = 0x00001470,
			.ctxt_cfg                 = 0x00001478,
			.addr_status_0            = 0x00001490,
			.addr_status_1            = 0x00001494,
			.addr_status_2            = 0x00001498,
			.addr_status_3            = 0x0000149C,
			.debug_status_cfg         = 0x0000147C,
			.debug_status_0           = 0x00001480,
			.debug_status_1           = 0x00001484,
			.bw_limiter_addr          = 0x0000141C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_1,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_NV21) |
				BIT_ULL(CAM_FORMAT_NV12),
		},
		/* BUS Client 8 FD_C */
		{
			.cfg                      = 0x00001500,
			.image_addr               = 0x00001504,
			.frame_incr               = 0x00001508,
			.image_cfg_0              = 0x0000150C,
			.image_cfg_1              = 0x00001510,
			.image_cfg_2              = 0x00001514,
			.packer_cfg               = 0x00001518,
			.frame_header_addr        = 0x00001520,
			.frame_header_incr        = 0x00001524,
			.frame_header_cfg         = 0x00001528,
			.irq_subsample_period     = 0x00001530,
			.irq_subsample_pattern    = 0x00001534,
			.framedrop_period         = 0x00001538,
			.framedrop_pattern        = 0x0000153C,
			.mmu_prefetch_cfg         = 0x00001560,
			.mmu_prefetch_max_offset  = 0x00001564,
			.system_cache_cfg         = 0x00001568,
			.addr_cfg                 = 0x00001570,
			.ctxt_cfg                 = 0x00001578,
			.addr_status_0            = 0x00001590,
			.addr_status_1            = 0x00001594,
			.addr_status_2            = 0x00001598,
			.addr_status_3            = 0x0000159C,
			.debug_status_cfg         = 0x0000157C,
			.debug_status_0           = 0x00001580,
			.debug_status_1           = 0x00001584,
			.bw_limiter_addr          = 0x0000151C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_1,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_NV21) |
				BIT_ULL(CAM_FORMAT_NV12),
		},
		/* BUS Client 9 IR */
		{
			.cfg                      = 0x00001600,
			.image_addr               = 0x00001604,
			.frame_incr               = 0x00001608,
			.image_cfg_0              = 0x0000160C,
			.image_cfg_1              = 0x00001610,
			.image_cfg_2              = 0x00001614,
			.packer_cfg               = 0x00001618,
			.frame_header_addr        = 0x00001620,
			.frame_header_incr        = 0x00001624,
			.frame_header_cfg         = 0x00001628,
			.irq_subsample_period     = 0x00001630,
			.irq_subsample_pattern    = 0x00001634,
			.framedrop_period         = 0x00001638,
			.framedrop_pattern        = 0x0000163C,
			.mmu_prefetch_cfg         = 0x00001660,
			.mmu_prefetch_max_offset  = 0x00001664,
			.system_cache_cfg         = 0x00001668,
			.addr_cfg                 = 0x00001670,
			.ctxt_cfg                 = 0x00001678,
			.addr_status_0            = 0x00001690,
			.addr_status_1            = 0x00001694,
			.addr_status_2            = 0x00001698,
			.addr_status_3            = 0x0000169C,
			.debug_status_cfg         = 0x0000167C,
			.debug_status_0           = 0x00001680,
			.debug_status_1           = 0x00001684,
			.bw_limiter_addr          = 0x0000161C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_8) | BIT_ULL(CAM_FORMAT_PLAIN16_10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_12) | BIT_ULL(CAM_FORMAT_PLAIN16_14) |
				BIT_ULL(CAM_FORMAT_PLAIN16_16) |
#ifdef OPLUS_FEATURE_CAMERA_COMMON
				BIT_ULL(CAM_FORMAT_PLAIN16_10_LSB)|
#endif
				BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_12) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_14),
		},
		/* BUS Client 10 STATS_AEC_BE */
		{
			.cfg                      = 0x00001700,
			.image_addr               = 0x00001704,
			.frame_incr               = 0x00001708,
			.image_cfg_0              = 0x0000170C,
			.image_cfg_1              = 0x00001710,
			.image_cfg_2              = 0x00001714,
			.packer_cfg               = 0x00001718,
			.frame_header_addr        = 0x00001720,
			.frame_header_incr        = 0x00001724,
			.frame_header_cfg         = 0x00001728,
			.irq_subsample_period     = 0x00001730,
			.irq_subsample_pattern    = 0x00001734,
			.framedrop_period         = 0x00001738,
			.framedrop_pattern        = 0x0000173C,
			.mmu_prefetch_cfg         = 0x00001760,
			.mmu_prefetch_max_offset  = 0x00001764,
			.system_cache_cfg         = 0x00001768,
			.addr_cfg                 = 0x00001770,
			.ctxt_cfg                 = 0x00001778,
			.addr_status_0            = 0x00001790,
			.addr_status_1            = 0x00001794,
			.addr_status_2            = 0x00001798,
			.addr_status_3            = 0x0000179C,
			.debug_status_cfg         = 0x0000177C,
			.debug_status_0           = 0x00001780,
			.debug_status_1           = 0x00001784,
			.bw_limiter_addr          = 0x0000171C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 11 STATS_AEC_BHIST */
		{
			.cfg                      = 0x00001800,
			.image_addr               = 0x00001804,
			.frame_incr               = 0x00001808,
			.image_cfg_0              = 0x0000180C,
			.image_cfg_1              = 0x00001810,
			.image_cfg_2              = 0x00001814,
			.packer_cfg               = 0x00001818,
			.frame_header_addr        = 0x00001820,
			.frame_header_incr        = 0x00001824,
			.frame_header_cfg         = 0x00001828,
			.irq_subsample_period     = 0x00001830,
			.irq_subsample_pattern    = 0x00001834,
			.framedrop_period         = 0x00001838,
			.framedrop_pattern        = 0x0000183C,
			.mmu_prefetch_cfg         = 0x00001860,
			.mmu_prefetch_max_offset  = 0x00001864,
			.system_cache_cfg         = 0x00001868,
			.addr_cfg                 = 0x00001870,
			.ctxt_cfg                 = 0x00001878,
			.addr_status_0            = 0x00001890,
			.addr_status_1            = 0x00001894,
			.addr_status_2            = 0x00001898,
			.addr_status_3            = 0x0000189C,
			.debug_status_cfg         = 0x0000187C,
			.debug_status_0           = 0x00001880,
			.debug_status_1           = 0x00001884,
			.bw_limiter_addr          = 0x0000181C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 12 STATS_TINTLESS_BG */
		{
			.cfg                      = 0x00001900,
			.image_addr               = 0x00001904,
			.frame_incr               = 0x00001908,
			.image_cfg_0              = 0x0000190C,
			.image_cfg_1              = 0x00001910,
			.image_cfg_2              = 0x00001914,
			.packer_cfg               = 0x00001918,
			.frame_header_addr        = 0x00001920,
			.frame_header_incr        = 0x00001924,
			.frame_header_cfg         = 0x00001928,
			.irq_subsample_period     = 0x00001930,
			.irq_subsample_pattern    = 0x00001934,
			.framedrop_period         = 0x00001938,
			.framedrop_pattern        = 0x0000193C,
			.mmu_prefetch_cfg         = 0x00001960,
			.mmu_prefetch_max_offset  = 0x00001964,
			.system_cache_cfg         = 0x00001968,
			.addr_cfg                 = 0x00001970,
			.ctxt_cfg                 = 0x00001978,
			.addr_status_0            = 0x00001990,
			.addr_status_1            = 0x00001994,
			.addr_status_2            = 0x00001998,
			.addr_status_3            = 0x0000199C,
			.debug_status_cfg         = 0x0000197C,
			.debug_status_0           = 0x00001980,
			.debug_status_1           = 0x00001984,
			.bw_limiter_addr          = 0x0000191C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 13 STATS_AWB_BG */
		{
			.cfg                      = 0x00001A00,
			.image_addr               = 0x00001A04,
			.frame_incr               = 0x00001A08,
			.image_cfg_0              = 0x00001A0C,
			.image_cfg_1              = 0x00001A10,
			.image_cfg_2              = 0x00001A14,
			.packer_cfg               = 0x00001A18,
			.frame_header_addr        = 0x00001A20,
			.frame_header_incr        = 0x00001A24,
			.frame_header_cfg         = 0x00001A28,
			.irq_subsample_period     = 0x00001A30,
			.irq_subsample_pattern    = 0x00001A34,
			.framedrop_period         = 0x00001A38,
			.framedrop_pattern        = 0x00001A3C,
			.mmu_prefetch_cfg         = 0x00001A60,
			.mmu_prefetch_max_offset  = 0x00001A64,
			.system_cache_cfg         = 0x00001A68,
			.addr_cfg                 = 0x00001A70,
			.ctxt_cfg                 = 0x00001A78,
			.addr_status_0            = 0x00001A90,
			.addr_status_1            = 0x00001A94,
			.addr_status_2            = 0x00001A98,
			.addr_status_3            = 0x00001A9C,
			.debug_status_cfg         = 0x00001A7C,
			.debug_status_0           = 0x00001A80,
			.debug_status_1           = 0x00001A84,
			.bw_limiter_addr          = 0x00001A1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 14 STATS_AWB_BFW */
		{
			.cfg                      = 0x00001B00,
			.image_addr               = 0x00001B04,
			.frame_incr               = 0x00001B08,
			.image_cfg_0              = 0x00001B0C,
			.image_cfg_1              = 0x00001B10,
			.image_cfg_2              = 0x00001B14,
			.packer_cfg               = 0x00001B18,
			.frame_header_addr        = 0x00001B20,
			.frame_header_incr        = 0x00001B24,
			.frame_header_cfg         = 0x00001B28,
			.irq_subsample_period     = 0x00001B30,
			.irq_subsample_pattern    = 0x00001B34,
			.framedrop_period         = 0x00001B38,
			.framedrop_pattern        = 0x00001B3C,
			.mmu_prefetch_cfg         = 0x00001B60,
			.mmu_prefetch_max_offset  = 0x00001B64,
			.system_cache_cfg         = 0x00001B68,
			.addr_cfg                 = 0x00001B70,
			.ctxt_cfg                 = 0x00001B78,
			.addr_status_0            = 0x00001B90,
			.addr_status_1            = 0x00001B94,
			.addr_status_2            = 0x00001B98,
			.addr_status_3            = 0x00001B9C,
			.debug_status_cfg         = 0x00001B7C,
			.debug_status_0           = 0x00001B80,
			.debug_status_1           = 0x00001B84,
			.bw_limiter_addr          = 0x00001B1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 15 STATS_AF_BHIST */
		{
			.cfg                      = 0x00001C00,
			.image_addr               = 0x00001C04,
			.frame_incr               = 0x00001C08,
			.image_cfg_0              = 0x00001C0C,
			.image_cfg_1              = 0x00001C10,
			.image_cfg_2              = 0x00001C14,
			.packer_cfg               = 0x00001C18,
			.frame_header_addr        = 0x00001C20,
			.frame_header_incr        = 0x00001C24,
			.frame_header_cfg         = 0x00001C28,
			.irq_subsample_period     = 0x00001C30,
			.irq_subsample_pattern    = 0x00001C34,
			.framedrop_period         = 0x00001C38,
			.framedrop_pattern        = 0x00001C3C,
			.mmu_prefetch_cfg         = 0x00001C60,
			.mmu_prefetch_max_offset  = 0x00001C64,
			.system_cache_cfg         = 0x00001C68,
			.addr_cfg                 = 0x00001C70,
			.ctxt_cfg                 = 0x00001C78,
			.addr_status_0            = 0x00001C90,
			.addr_status_1            = 0x00001C94,
			.addr_status_2            = 0x00001C98,
			.addr_status_3            = 0x00001C9C,
			.debug_status_cfg         = 0x00001C7C,
			.debug_status_0           = 0x00001C80,
			.debug_status_1           = 0x00001C84,
			.bw_limiter_addr          = 0x00001C1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 16 STATS_ALSC_BG */
		{
			.cfg                      = 0x00001D00,
			.image_addr               = 0x00001D04,
			.frame_incr               = 0x00001D08,
			.image_cfg_0              = 0x00001D0C,
			.image_cfg_1              = 0x00001D10,
			.image_cfg_2              = 0x00001D14,
			.packer_cfg               = 0x00001D18,
			.frame_header_addr        = 0x00001D20,
			.frame_header_incr        = 0x00001D24,
			.frame_header_cfg         = 0x00001D28,
			.irq_subsample_period     = 0x00001D30,
			.irq_subsample_pattern    = 0x00001D34,
			.framedrop_period         = 0x00001D38,
			.framedrop_pattern        = 0x00001D3C,
			.mmu_prefetch_cfg         = 0x00001D60,
			.mmu_prefetch_max_offset  = 0x00001D64,
			.system_cache_cfg         = 0x00001D68,
			.addr_cfg                 = 0x00001D70,
			.ctxt_cfg                 = 0x00001D78,
			.addr_status_0            = 0x00001D90,
			.addr_status_1            = 0x00001D94,
			.addr_status_2            = 0x00001D98,
			.addr_status_3            = 0x00001D9C,
			.debug_status_cfg         = 0x00001D7C,
			.debug_status_0           = 0x00001D80,
			.debug_status_1           = 0x00001D84,
			.bw_limiter_addr          = 0x00001D1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 17 STATS_FLICKER_BAYERS */
		{
			.cfg                      = 0x00001E00,
			.image_addr               = 0x00001E04,
			.frame_incr               = 0x00001E08,
			.image_cfg_0              = 0x00001E0C,
			.image_cfg_1              = 0x00001E10,
			.image_cfg_2              = 0x00001E14,
			.packer_cfg               = 0x00001E18,
			.frame_header_addr        = 0x00001E20,
			.frame_header_incr        = 0x00001E24,
			.frame_header_cfg         = 0x00001E28,
			.irq_subsample_period     = 0x00001E30,
			.irq_subsample_pattern    = 0x00001E34,
			.framedrop_period         = 0x00001E38,
			.framedrop_pattern        = 0x00001E3C,
			.mmu_prefetch_cfg         = 0x00001E60,
			.mmu_prefetch_max_offset  = 0x00001E64,
			.system_cache_cfg         = 0x00001E68,
			.addr_cfg                 = 0x00001E70,
			.ctxt_cfg                 = 0x00001E78,
			.addr_status_0            = 0x00001E90,
			.addr_status_1            = 0x00001E94,
			.addr_status_2            = 0x00001E98,
			.addr_status_3            = 0x00001E9C,
			.debug_status_cfg         = 0x00001E7C,
			.debug_status_0           = 0x00001E80,
			.debug_status_1           = 0x00001E84,
			.bw_limiter_addr          = 0x00001E1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN32),
		},
		/* BUS Client 18 STATS_TMC_BHIST */
		{
			.cfg                      = 0x00001F00,
			.image_addr               = 0x00001F04,
			.frame_incr               = 0x00001F08,
			.image_cfg_0              = 0x00001F0C,
			.image_cfg_1              = 0x00001F10,
			.image_cfg_2              = 0x00001F14,
			.packer_cfg               = 0x00001F18,
			.frame_header_addr        = 0x00001F20,
			.frame_header_incr        = 0x00001F24,
			.frame_header_cfg         = 0x00001F28,
			.irq_subsample_period     = 0x00001F30,
			.irq_subsample_pattern    = 0x00001F34,
			.framedrop_period         = 0x00001F38,
			.framedrop_pattern        = 0x00001F3C,
			.mmu_prefetch_cfg         = 0x00001F60,
			.mmu_prefetch_max_offset  = 0x00001F64,
			.system_cache_cfg         = 0x00001F68,
			.addr_cfg                 = 0x00001F70,
			.ctxt_cfg                 = 0x00001F78,
			.addr_status_0            = 0x00001F90,
			.addr_status_1            = 0x00001F94,
			.addr_status_2            = 0x00001F98,
			.addr_status_3            = 0x00001F9C,
			.debug_status_cfg         = 0x00001F7C,
			.debug_status_0           = 0x00001F80,
			.debug_status_1           = 0x00001F84,
			.bw_limiter_addr          = 0x00001F1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 19 PDAF_0 */ /* Note: PDAF_SAD == 2PD*/
		{
			.cfg                      = 0x00002000,
			.image_addr               = 0x00002004,
			.frame_incr               = 0x00002008,
			.image_cfg_0              = 0x0000200C,
			.image_cfg_1              = 0x00002010,
			.image_cfg_2              = 0x00002014,
			.packer_cfg               = 0x00002018,
			.frame_header_addr        = 0x00002020,
			.frame_header_incr        = 0x00002024,
			.frame_header_cfg         = 0x00002028,
			.irq_subsample_period     = 0x00002030,
			.irq_subsample_pattern    = 0x00002034,
			.framedrop_period         = 0x00002038,
			.framedrop_pattern        = 0x0000203C,
			.mmu_prefetch_cfg         = 0x00002060,
			.mmu_prefetch_max_offset  = 0x00002064,
			.system_cache_cfg         = 0x00002068,
			.addr_cfg                 = 0x00002070,
			.ctxt_cfg                 = 0x00002078,
			.addr_status_0            = 0x00002090,
			.addr_status_1            = 0x00002094,
			.addr_status_2            = 0x00002098,
			.addr_status_3            = 0x0000209C,
			.debug_status_cfg         = 0x0000207C,
			.debug_status_0           = 0x00002080,
			.debug_status_1           = 0x00002084,
			.bw_limiter_addr          = 0x0000201C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_3,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 20 PDAF_1 */
		{
			.cfg                      = 0x00002100,
			.image_addr               = 0x00002104,
			.frame_incr               = 0x00002108,
			.image_cfg_0              = 0x0000210C,
			.image_cfg_1              = 0x00002110,
			.image_cfg_2              = 0x00002114,
			.packer_cfg               = 0x00002118,
			.frame_header_addr        = 0x00002120,
			.frame_header_incr        = 0x00002124,
			.frame_header_cfg         = 0x00002128,
			.irq_subsample_period     = 0x00002130,
			.irq_subsample_pattern    = 0x00002134,
			.framedrop_period         = 0x00002138,
			.framedrop_pattern        = 0x0000213C,
			.mmu_prefetch_cfg         = 0x00002160,
			.mmu_prefetch_max_offset  = 0x00002164,
			.system_cache_cfg         = 0x00002168,
			.addr_cfg                 = 0x00002170,
			.ctxt_cfg                 = 0x00002178,
			.addr_status_0            = 0x00002190,
			.addr_status_1            = 0x00002194,
			.addr_status_2            = 0x00002198,
			.addr_status_3            = 0x0000219C,
			.debug_status_cfg         = 0x0000217C,
			.debug_status_0           = 0x00002180,
			.debug_status_1           = 0x00002184,
			.bw_limiter_addr          = 0x0000211C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_3,
			.ubwc_regs                = &tfe980_ubwc_regs_client_20,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN16_8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10) | BIT_ULL(CAM_FORMAT_PLAIN16_12) |
				BIT_ULL(CAM_FORMAT_PLAIN16_14) | BIT_ULL(CAM_FORMAT_PLAIN16_16) |
				BIT_ULL(CAM_FORMAT_UBWC_P016)
#ifdef OPLUS_FEATURE_CAMERA_COMMON
				|BIT_ULL(CAM_FORMAT_PLAIN16_10_LSB)
#endif
				,
		},
		/* BUS Client 21 PDAF_2 */
		{
			.cfg                      = 0x00002200,
			.image_addr               = 0x00002204,
			.frame_incr               = 0x00002208,
			.image_cfg_0              = 0x0000220C,
			.image_cfg_1              = 0x00002210,
			.image_cfg_2              = 0x00002214,
			.packer_cfg               = 0x00002218,
			.frame_header_addr        = 0x00002220,
			.frame_header_incr        = 0x00002224,
			.frame_header_cfg         = 0x00002228,
			.irq_subsample_period     = 0x00002230,
			.irq_subsample_pattern    = 0x00002234,
			.framedrop_period         = 0x00002238,
			.framedrop_pattern        = 0x0000223C,
			.mmu_prefetch_cfg         = 0x00002260,
			.mmu_prefetch_max_offset  = 0x00002264,
			.system_cache_cfg         = 0x00002268,
			.addr_cfg                 = 0x00002270,
			.ctxt_cfg                 = 0x00002278,
			.addr_status_0            = 0x00002290,
			.addr_status_1            = 0x00002294,
			.addr_status_2            = 0x00002298,
			.addr_status_3            = 0x0000229C,
			.debug_status_cfg         = 0x0000227C,
			.debug_status_0           = 0x00002280,
			.debug_status_1           = 0x00002284,
			.bw_limiter_addr          = 0x0000221C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_3,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_8) | BIT_ULL(CAM_FORMAT_PLAIN16_10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_12) | BIT_ULL(CAM_FORMAT_PLAIN16_14) |
				BIT_ULL(CAM_FORMAT_PLAIN16_16)
#ifdef OPLUS_FEATURE_CAMERA_COMMON
				|BIT_ULL(CAM_FORMAT_PLAIN16_10_LSB)
#endif
				,
		},
		/* BUS Client 22 PDAF_3 */
		{
			.cfg                      = 0x00002300,
			.image_addr               = 0x00002304,
			.frame_incr               = 0x00002308,
			.image_cfg_0              = 0x0000230C,
			.image_cfg_1              = 0x00002310,
			.image_cfg_2              = 0x00002314,
			.packer_cfg               = 0x00002318,
			.frame_header_addr        = 0x00002320,
			.frame_header_incr        = 0x00002324,
			.frame_header_cfg         = 0x00002328,
			.irq_subsample_period     = 0x00002330,
			.irq_subsample_pattern    = 0x00002334,
			.framedrop_period         = 0x00002338,
			.framedrop_pattern        = 0x0000233C,
			.mmu_prefetch_cfg         = 0x00002360,
			.mmu_prefetch_max_offset  = 0x00002364,
			.system_cache_cfg         = 0x00002368,
			.addr_cfg                 = 0x00002370,
			.ctxt_cfg                 = 0x00002378,
			.addr_status_0            = 0x00002390,
			.addr_status_1            = 0x00002394,
			.addr_status_2            = 0x00002398,
			.addr_status_3            = 0x0000239C,
			.debug_status_cfg         = 0x0000237C,
			.debug_status_0           = 0x00002380,
			.debug_status_1           = 0x00002384,
			.bw_limiter_addr          = 0x0000231C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_4,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 23 RDI_0 */
		{
			.cfg                      = 0x00002400,
			.image_addr               = 0x00002404,
			.frame_incr               = 0x00002408,
			.image_cfg_0              = 0x0000240C,
			.image_cfg_1              = 0x00002410,
			.image_cfg_2              = 0x00002414,
			.packer_cfg               = 0x00002418,
			.frame_header_addr        = 0x00002420,
			.frame_header_incr        = 0x00002424,
			.frame_header_cfg         = 0x00002428,
			.irq_subsample_period     = 0x00002430,
			.irq_subsample_pattern    = 0x00002434,
			.framedrop_period         = 0x00002438,
			.framedrop_pattern        = 0x0000243C,
			.mmu_prefetch_cfg         = 0x00002460,
			.mmu_prefetch_max_offset  = 0x00002464,
			.system_cache_cfg         = 0x00002468,
			.addr_cfg                 = 0x00002470,
			.ctxt_cfg                 = 0x00002478,
			.addr_status_0            = 0x00002490,
			.addr_status_1            = 0x00002494,
			.addr_status_2            = 0x00002498,
			.addr_status_3            = 0x0000249C,
			.debug_status_cfg         = 0x0000247C,
			.debug_status_0           = 0x00002480,
			.debug_status_1           = 0x00002484,
			.bw_limiter_addr          = 0x0000241C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_5,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_12) | BIT_ULL(CAM_FORMAT_MIPI_RAW_14) |
				BIT_ULL(CAM_FORMAT_PLAIN128) | BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_8) | BIT_ULL(CAM_FORMAT_PLAIN16_10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_12) | BIT_ULL(CAM_FORMAT_PLAIN16_14) |
				BIT_ULL(CAM_FORMAT_PLAIN16_16) | BIT_ULL(CAM_FORMAT_PLAIN64)
#ifdef OPLUS_FEATURE_CAMERA_COMMON
				|BIT_ULL(CAM_FORMAT_PLAIN16_10_LSB)
#endif
				,
		},
		/* BUS Client 24 RDI_1 */
		{
			.cfg                      = 0x00002500,
			.image_addr               = 0x00002504,
			.frame_incr               = 0x00002508,
			.image_cfg_0              = 0x0000250C,
			.image_cfg_1              = 0x00002510,
			.image_cfg_2              = 0x00002514,
			.packer_cfg               = 0x00002518,
			.frame_header_addr        = 0x00002520,
			.frame_header_incr        = 0x00002524,
			.frame_header_cfg         = 0x00002528,
			.irq_subsample_period     = 0x00002530,
			.irq_subsample_pattern    = 0x00002534,
			.framedrop_period         = 0x00002538,
			.framedrop_pattern        = 0x0000253C,
			.mmu_prefetch_cfg         = 0x00002560,
			.mmu_prefetch_max_offset  = 0x00002564,
			.system_cache_cfg         = 0x00002568,
			.addr_cfg                 = 0x00002570,
			.ctxt_cfg                 = 0x00002578,
			.addr_status_0            = 0x00002590,
			.addr_status_1            = 0x00002594,
			.addr_status_2            = 0x00002598,
			.addr_status_3            = 0x0000259C,
			.debug_status_cfg         = 0x0000257C,
			.debug_status_0           = 0x00002580,
			.debug_status_1           = 0x00002584,
			.bw_limiter_addr          = 0x0000251C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_6,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_12) | BIT_ULL(CAM_FORMAT_MIPI_RAW_14) |
				BIT_ULL(CAM_FORMAT_PLAIN128) | BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_8) | BIT_ULL(CAM_FORMAT_PLAIN16_10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_12) | BIT_ULL(CAM_FORMAT_PLAIN16_14) |
				BIT_ULL(CAM_FORMAT_PLAIN16_16) | BIT_ULL(CAM_FORMAT_PLAIN64)
#ifdef OPLUS_FEATURE_CAMERA_COMMON
				|BIT_ULL(CAM_FORMAT_PLAIN16_10_LSB)
#endif
				,
		},
		/* BUS Client 25 RDI_2 */
		{
			.cfg                      = 0x00002600,
			.image_addr               = 0x00002604,
			.frame_incr               = 0x00002608,
			.image_cfg_0              = 0x0000260C,
			.image_cfg_1              = 0x00002610,
			.image_cfg_2              = 0x00002614,
			.packer_cfg               = 0x00002618,
			.frame_header_addr        = 0x00002620,
			.frame_header_incr        = 0x00002624,
			.frame_header_cfg         = 0x00002628,
			.irq_subsample_period     = 0x00002630,
			.irq_subsample_pattern    = 0x00002634,
			.framedrop_period         = 0x00002638,
			.framedrop_pattern        = 0x0000263C,
			.mmu_prefetch_cfg         = 0x00002660,
			.mmu_prefetch_max_offset  = 0x00002664,
			.system_cache_cfg         = 0x00002668,
			.addr_cfg                 = 0x00002670,
			.ctxt_cfg                 = 0x00002678,
			.addr_status_0            = 0x00002690,
			.addr_status_1            = 0x00002694,
			.addr_status_2            = 0x00002698,
			.addr_status_3            = 0x0000269C,
			.debug_status_cfg         = 0x0000267C,
			.debug_status_0           = 0x00002680,
			.debug_status_1           = 0x00002684,
			.bw_limiter_addr          = 0x0000261C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_7,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_12) | BIT_ULL(CAM_FORMAT_MIPI_RAW_14) |
				BIT_ULL(CAM_FORMAT_PLAIN128) | BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_8) | BIT_ULL(CAM_FORMAT_PLAIN16_10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_12) | BIT_ULL(CAM_FORMAT_PLAIN16_14) |
				BIT_ULL(CAM_FORMAT_PLAIN16_16) | BIT_ULL(CAM_FORMAT_PLAIN64)
#ifdef OPLUS_FEATURE_CAMERA_COMMON
				|BIT_ULL(CAM_FORMAT_PLAIN16_10_LSB)
#endif
				,
		},
		/* BUS Client 26 RDI_3 */
		{
			.cfg                      = 0x00002700,
			.image_addr               = 0x00002704,
			.frame_incr               = 0x00002708,
			.image_cfg_0              = 0x0000270C,
			.image_cfg_1              = 0x00002710,
			.image_cfg_2              = 0x00002714,
			.packer_cfg               = 0x00002718,
			.frame_header_addr        = 0x00002720,
			.frame_header_incr        = 0x00002724,
			.frame_header_cfg         = 0x00002728,
			.irq_subsample_period     = 0x00002730,
			.irq_subsample_pattern    = 0x00002734,
			.framedrop_period         = 0x00002738,
			.framedrop_pattern        = 0x0000273C,
			.mmu_prefetch_cfg         = 0x00002760,
			.mmu_prefetch_max_offset  = 0x00002764,
			.system_cache_cfg         = 0x00002768,
			.addr_cfg                 = 0x00002770,
			.ctxt_cfg                 = 0x00002778,
			.addr_status_0            = 0x00002790,
			.addr_status_1            = 0x00002794,
			.addr_status_2            = 0x00002798,
			.addr_status_3            = 0x0000279C,
			.debug_status_cfg         = 0x0000277C,
			.debug_status_0           = 0x00002780,
			.debug_status_1           = 0x00002784,
			.bw_limiter_addr          = 0x0000271C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_8,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN128),
		},
		/* BUS Client 27 RDI_4 */
		{
			.cfg                      = 0x00002800,
			.image_addr               = 0x00002804,
			.frame_incr               = 0x00002808,
			.image_cfg_0              = 0x0000280C,
			.image_cfg_1              = 0x00002810,
			.image_cfg_2              = 0x00002814,
			.packer_cfg               = 0x00002818,
			.frame_header_addr        = 0x00002820,
			.frame_header_incr        = 0x00002824,
			.frame_header_cfg         = 0x00002828,
			.irq_subsample_period     = 0x00002830,
			.irq_subsample_pattern    = 0x00002834,
			.framedrop_period         = 0x00002838,
			.framedrop_pattern        = 0x0000283C,
			.mmu_prefetch_cfg         = 0x00002860,
			.mmu_prefetch_max_offset  = 0x00002864,
			.system_cache_cfg         = 0x00002868,
			.addr_cfg                 = 0x00002870,
			.ctxt_cfg                 = 0x00002878,
			.addr_status_0            = 0x00002890,
			.addr_status_1            = 0x00002894,
			.addr_status_2            = 0x00002898,
			.addr_status_3            = 0x0000289C,
			.debug_status_cfg         = 0x0000287C,
			.debug_status_0           = 0x00002880,
			.debug_status_1           = 0x00002884,
			.bw_limiter_addr          = 0x0000281C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_9,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN128),
		},
	},
	.num_out = 24,
	.vfe_out_hw_info = {
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI0,
			.max_width     = 16384,
			.max_height    = 16384,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_2,
			.mid           = tfe980_out_port_mid[0],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				23,
			},
			.name          = {
				"RDI_0",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI1,
			.max_width     = 16384,
			.max_height    = 16384,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_3,
			.mid           = tfe980_out_port_mid[1],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				24,
			},
			.name          = {
				"RDI_1",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI2,
			.max_width     = 16384,
			.max_height    = 16384,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_4,
			.mid           = tfe980_out_port_mid[2],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				25,
			},
			.name          = {
				"RDI_2",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI3,
			.max_width     = 16384,
			.max_height    = 16384,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_5,
			.mid           = tfe980_out_port_mid[3],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				26,
			},
			.name          = {
				"RDI_3",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI4,
			.max_width     = 16384,
			.max_height    = 16384,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_6,
			.mid           = tfe980_out_port_mid[4],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				27,
			},
			.name          = {
				"RDI_4",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_FULL,
			.max_width     = 4672,
			.max_height    = 16384,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[5],
			.num_mid       = 6,
			.num_wm        = 1,
			.line_based    = 1,
			.mc_based      = true,
			.mc_grp_shift  = 0,
			.wm_idx        = {
				0,
			},
			.name          = {
				"FULL",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_DS4,
			.max_width     = 1168,
			.max_height    = 4096,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[6],
			.num_mid       = 12,
			.num_wm        = 2,
			.line_based    = 1,
			.mc_based      = true,
			.mc_grp_shift  = 0,
			.wm_idx        = {
				1,
				2,
			},
			.name          = {
				"DS4_Y",
				"DS4_C"
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_DS16,
			.max_width     = 292,
			.max_height    = 1024,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[7],
			.num_mid       = 12,
			.num_wm        = 2,
			.line_based    = 1,
			.mc_based      = true,
			.mc_grp_shift  = 0,
			.wm_idx        = {
				3,
				4,
			},
			.name          = {
				"DS16_Y",
				"DS16_C",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_DS2,
			.max_width     = 4672,
			.max_height    = 8192,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[8],
			.num_mid       = 12,
			.num_wm        = 2,
			.line_based    = 1,
			.mc_based      = true,
			.wm_idx        = {
				5,
				6,
			},
			.name          = {
				"DS2_Y",
				"DS2_C",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_FD,
			.max_width     = 9312,
			.max_height    = 16384,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[9],
			.num_mid       = 3,
			.num_wm        = 2,
			.line_based    = 1,
			.cntxt_cfg_except = true,
			.wm_idx        = {
				7,
				8,
			},
			.name          = {
				"FD_Y",
				"FD_C",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_IR,
			.max_width     = 9312,
			.max_height    = 8192,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[10],
			.num_mid       = 2,
			.num_wm        = 1,
			.line_based    = 1,
			.cntxt_cfg_except = true,
			.wm_idx        = {
				9,
			},
			.name          = {
				"IR",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_AEC_BE,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[11],
			.num_mid       = 3,
			.num_wm        = 1,
			.mc_based      = true,
			.mc_grp_shift  = 4,
			.wm_idx        = {
				10,
			},
			.name          = {
				"STATS_AEC_BE",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_AEC_BHIST,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[12],
			.num_mid       = 3,
			.num_wm        = 1,
			.mc_based      = true,
			.mc_grp_shift  = 4,
			.wm_idx        = {
				11,
			},
			.name          = {
				"STATS_BHIST",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_TL_BG,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[13],
			.num_mid       = 3,
			.num_wm        = 1,
			.mc_based      = true,
			.mc_grp_shift  = 4,
			.wm_idx        = {
				12,
			},
			.name          = {
				"STATS_TL_BG",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_AWB_BG,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[14],
			.num_mid       = 3,
			.num_wm        = 1,
			.mc_based      = true,
			.mc_grp_shift  = 4,
			.wm_idx        = {
				13,
			},
			.name          = {
				"STATS_AWB_BG",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_AWB_BFW,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[15],
			.num_mid       = 3,
			.num_wm        = 1,
			.mc_based      = true,
			.mc_grp_shift  = 4,
			.wm_idx        = {
				14,
			},
			.name          = {
				"AWB_BFW",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_AF_BHIST,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[16],
			.num_mid       = 3,
			.num_wm        = 1,
			.mc_based      = true,
			.mc_grp_shift  = 4,
			.wm_idx        = {
				15,
			},
			.name          = {
				"AF_BHIST",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_ALSC,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[17],
			.num_mid       = 3,
			.num_wm        = 1,
			.mc_based      = true,
			.mc_grp_shift  = 4,
			.wm_idx        = {
				16,
			},
			.name          = {
				"ALSC_BG",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_BAYER_RS,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[18],
			.num_mid       = 3,
			.num_wm        = 1,
			.mc_based      = true,
			.mc_grp_shift  = 4,
			.wm_idx        = {
				17,
			},
			.name          = {
				"STATS_RS",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_TMC_BHIST,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = tfe980_out_port_mid[19],
			.num_mid       = 3,
			.num_wm        = 1,
			.mc_based      = true,
			.mc_grp_shift  = 4,
			.wm_idx        = {
				18,
			},
			.name          = {
				"STATS_TMC_BHIST",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_2PD,
			.max_width     = 14592,
			.max_height    = 4096,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_1,
			.mid           = tfe980_out_port_mid[20],
			.num_mid       = 1,
			.num_wm        = 1,
			.early_done_mask = BIT(28),
			.wm_idx        = {
				19,
			},
			.name          = {
				"PDAF_0_2PD",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_2PD,
			.max_width     = 1920,
			.max_height    = 1080,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_1,
			.mid           = tfe980_out_port_mid[21],
			.num_mid       = 2,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				20,
			},
			.name          = {
				"PDAF_1_PREPROCESS_2PD",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_PDAF_PARSED,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_1,
			.mid           = tfe980_out_port_mid[22],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				21,
			},
			.name          = {
				"PDAF_2_PARSED_DATA",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_CAF,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_1,
			.mid           = tfe980_out_port_mid[23],
			.num_mid       = 1,
			.num_wm        = 1,
			.early_done_mask = BIT(29),
			.mc_based      = false,
			.mc_grp_shift  = 4,
			.wm_idx        = {
				22,
			},
			.name          = {
				"STATS_CAF",
			},
		},
	},
	.num_cons_err = 32,
	.constraint_error_list = {
		{
			.bitmask = BIT(0),
			.error_description = "PPC 1x1 input Not Supported"
		},
		{
			.bitmask = BIT(1),
			.error_description = "PPC 1x2 input Not Supported"
		},
		{
			.bitmask = BIT(2),
			.error_description = "PPC 2x1 input Not Supported"
		},
		{
			.bitmask = BIT(3),
			.error_description = "PPC 2x2 input Not Supported"
		},
		{
			.bitmask = BIT(4),
			.error_description = "Pack 8 BPP format Not Supported"
		},
		{
			.bitmask = BIT(5),
			.error_description = "Pack 16 BPP format Not Supported"
		},
		{
			.bitmask = BIT(6),
			.error_description = "Pack 24 BPP format Not Supported"
		},
		{
			.bitmask = BIT(7),
			.error_description = "Pack 32 BPP format Not Supported"
		},
		{
			.bitmask = BIT(8),
			.error_description = "Pack 64 BPP format Not Supported"
		},
		{
			.bitmask = BIT(9),
			.error_description = "Pack MIPI 20 format Not Supported"
		},
		{
			.bitmask = BIT(10),
			.error_description = "Pack MIPI 14 format Not Supported"
		},
		{
			.bitmask = BIT(11),
			.error_description = "Pack MIPI 12 format Not Supported"
		},
		{
			.bitmask = BIT(12),
			.error_description = "Pack MIPI 10 format Not Supported"
		},
		{
			.bitmask = BIT(13),
			.error_description = "Pack 128 BPP format Not Supported"
		},
		{
			.bitmask = BIT(14),
			.error_description = "UBWC P016 format Not Supported"
		},
		{
			.bitmask = BIT(15),
			.error_description = "UBWC P010 format Not Supported"
		},
		{
			.bitmask = BIT(16),
			.error_description = "UBWC NV12 format Not Supported"
		},
		{
			.bitmask = BIT(17),
			.error_description = "UBWC NV12 4R format Not Supported"
		},
		{
			.bitmask = BIT(18),
			.error_description = "UBWC TP10 format Not Supported"
		},
		{
			.bitmask = BIT(19),
			.error_description = "Frame based Mode Not Supported"
		},
		{
			.bitmask = BIT(20),
			.error_description = "Index based Mode Not Supported"
		},
		{
			.bitmask = BIT(21),
			.error_description = "FIFO image addr unalign"
		},
		{
			.bitmask = BIT(22),
			.error_description = "FIFO ubwc addr unalign"
		},
		{
			.bitmask = BIT(23),
			.error_description = "FIFO framehdr addr unalign"
		},
		{
			.bitmask = BIT(24),
			.error_description = "Image address unalign"
		},
		{
			.bitmask = BIT(25),
			.error_description = "UBWC address unalign"
		},
		{
			.bitmask = BIT(26),
			.error_description = "Frame Header address unalign"
		},
		{
			.bitmask = BIT(27),
			.error_description = "Stride unalign"
		},
		{
			.bitmask = BIT(28),
			.error_description = "X Initialization unalign"
		},
		{
			.bitmask = BIT(29),
			.error_description = "Image Width unalign",
		},
		{
			.bitmask = BIT(30),
			.error_description = "Image Height unalign",
		},
		{
			.bitmask = BIT(31),
			.error_description = "Meta Stride unalign",
		},
	},
	.num_comp_grp          = 10,
	.support_consumed_addr = true,
	.mc_comp_done_mask = {
		BIT(24), 0x0, BIT(25), 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0,
	},
	.comp_done_mask = {
		0x7, BIT(3), 0x70, BIT(7), BIT(8), BIT(16),
		BIT(17), BIT(18), BIT(19), BIT(20),
	},
	.top_irq_shift         = 0,
	.max_out_res           = CAM_ISP_IFE_OUT_RES_BASE + 43,
	.pack_align_shift      = 5,
	.max_bw_counter_limit  = 0xFF,
	.skip_regdump          = true,
	.skip_regdump_start_offset = 0x800,
	.skip_regdump_stop_offset = 0x209C,
};

static struct cam_vfe_irq_hw_info tfe980_irq_hw_info = {
	.reset_mask    = 0,
	.supported_irq = CAM_VFE_HW_IRQ_CAP_EXT_CSID,
	.top_irq_reg   = &tfe980_top_irq_reg_info,
};

static struct cam_vfe_hw_info cam_tfe980_hw_info = {
	.irq_hw_info                  = &tfe980_irq_hw_info,

	.bus_version                   = CAM_VFE_BUS_VER_3_0,
	.bus_hw_info                   = &tfe980_bus_hw_info,

	.top_version                   = CAM_VFE_TOP_VER_4_0,
	.top_hw_info                   = &tfe980_top_hw_info,
};
#endif /* _CAM_TFE980_H_ */
