/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_VFE880_H_
#define _CAM_VFE880_H_
#include "cam_vfe_top_ver4.h"
#include "cam_vfe_core.h"
#include "cam_vfe_bus_ver3.h"
#include "cam_irq_controller.h"

#define CAM_VFE_BUS_VER3_880_MAX_CLIENTS     27
#define CAM_VFE_880_NUM_DBG_REG              19

static struct cam_vfe_top_ver4_module_desc vfe880_ipp_mod_desc[] = {
	{
		.id = 0,
		.desc = "CLC_DEMUX",
	},
	{
		.id = 1,
		.desc = "CLC_CHANNEL_GAIN",
	},
	{
		.id = 2,
		.desc = "CLC_BPC_PDPC",
	},
	{
		.id = 3,
		.desc = "CLC_BINCORRECT",
	},
	{
		.id = 4,
		.desc = "CLC_COMPDECOMP",
	},
	{
		.id = 5,
		.desc = "CLC_LSC",
	},
	{
		.id = 6,
		.desc = "CLC_WB_GAIN",
	},
	{
		.id = 7,
		.desc = "CLC_GIC",
	},
	{
		.id = 8,
		.desc = "CLC_BPC_ABF",
	},
	{
		.id = 9,
		.desc = "CLC_BLS",
	},
	{
		.id = 10,
		.desc = "CLC_BAYER_GTM",
	},
	{
		.id = 11,
		.desc = "CLC_BAYER_LTM",
	},
	{
		.id = 12,
		.desc = "CLC_LCAC",
	},
	{
		.id = 13,
		.desc = "CLC_DEMOSAIC",
	},
	{
		.id = 14,
		.desc = "CLC_COLOR_CORRECT",
	},
	{
		.id = 15,
		.desc = "CLC_GTM",
	},
	{
		.id = 16,
		.desc = "CLC_GLUT",
	},
	{
		.id = 17,
		.desc = "CLC_COLOR_TRANSFORM",
	},
	{
		.id = 18,
		.desc = "CLC_UVG",
	},
	{
		.id = 19,
		.desc = "CLC_PREPROCESSOR",
	},
	{
		.id = 20,
		.desc = "CLC_CHROMA_UP",
	},
	{
		.id = 21,
		.desc = "Reserved",
	},
	{
		.id = 22,
		.desc = "Reserved",
	},
	{
		.id = 23,
		.desc = "CLC_COMPDECOMP_HVX_TX",
	},
	{
		.id = 24,
		.desc = "CLC_COMPDECOMP_HVX_RX",
	},
	{
		.id = 25,
		.desc = "CLC_GTM_FD_OUT",
	},
	{
		.id = 26,
		.desc = "CLC_CROP_RND_CLAMP_PIXEL_RAW_OUT",
	},
	{
		.id = 27,
		.desc = "CLC_DOWNSCALE_MN_Y_FD_OUT",
	},
	{
		.id = 28,
		.desc = "CLC_DOWNSCALE_MN_C_FD_OUT",
	},
	{
		.id = 29,
		.desc = "CLC_CLC_CROP_RND_CLAMP_POST_DOWNSCALE_MN_Y_FD_OUT",
	},
	{
		.id = 30,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_MN_C_FD_OUT",
	},
	{
		.id = 31,
		.desc = "CLC_DOWNSCALE_MN_Y_DISP_OUT",
	},
	{
		.id = 32,
		.desc = "CLC_DOWNSCALE_MN_C_DISP_OUT",
	},
	{
		.id = 33,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_MN_Y_DISP_OUT",
	},
	{
		.id = 34,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_MN_C_DISP_OUT",
	},
	{
		.id = 35,
		.desc = "CLC_DOWNSCALE_4TO1_Y_DISP_DS4_OUT",
	},
	{
		.id = 36,
		.desc = "CLC_DOWNSCALE_4TO1_C_DISP_DS4_OUT",
	},
	{
		.id = 37,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_Y_DISP_DS4_OUT",
	},
	{
		.id = 38,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_C_DISP_DS4_OUT",
	},
	{
		.id = 39,
		.desc = "CLC_DOWNSCALE_4TO1_Y_DISP_DS16_OUT",
	},
	{
		.id = 40,
		.desc = "CLC_DOWNSCALE_4TO1_C_DISP_DS16_OUT",
	},
	{
		.id = 41,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_Y_DISP_DS16_OUT",
	},
	{
		.id = 42,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_C_DISP_DS16_OUT",
	},
	{
		.id = 43,
		.desc = "CLC_DOWNSCALE_MN_Y_VID_OUT",
	},
	{
		.id = 44,
		.desc = "CLC_DOWNSCALE_MN_C_VID_OUT",
	},
	{
		.id = 45,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_MN_Y_VID_OUT",
	},
	{
		.id = 46,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_MN_C_VID_OUT",
	},
	{
		.id = 47,
		.desc = "CLC_DSX_Y_VID_OUT",
	},
	{
		.id = 48,
		.desc = "CLC_DSX_C_VID_OUT",
	},
	{
		.id = 49,
		.desc = "CLC_CROP_RND_CLAMP_POST_DSX_Y_VID_OUT",
	},
	{
		.id = 50,
		.desc = "CLC_CROP_RND_CLAMP_POST_DSX_C_VID_OUT",
	},
	{
		.id = 51,
		.desc = "CLC_DOWNSCALE_4TO1_Y_VID_DS16_OUT",
	},
	{
		.id = 52,
		.desc = "CLC_DOWNSCALE_4TO1_C_VID_DS16_OUT",
	},
	{
		.id = 53,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_Y_VID_DS16_OUT",
	},
	{
		.id = 54,
		.desc = "CLC_CROP_RND_CLAMP_POST_DOWNSCALE_4TO1_C_VID_DS16_OUT",
	},
	{
		.id = 55,
		.desc = "CLC_STATS_AEC_BE",
	},
	{
		.id = 56,
		.desc = "CLC_STATS_AEC_BHIST",
	},
	{
		.id = 57,
		.desc = "CLC_STATS_BHIST",
	},
	{
		.id = 58,
		.desc = "CLC_STATS_TINTLESS_BG",
	},
	{
		.id = 59,
		.desc = "CLC_STATS_AWB_BG",
	},
	{
		.id = 60,
		.desc = "CLC_STATS_BFW",
	},
	{
		.id = 61,
		.desc = "CLC_STATS_RS",
	},
	{
		.id = 62,
		.desc = "CLC_STATS_IHIST",
	},
	{
		.id = 63,
		.desc = "Reserved",
	},
	{
		.id = 64,
		.desc = "CLC_PDPC_BPC_1D",
	},
	{
		.id = 65,
		.desc = "CLC_PDPC_BPC_1D_POST_LSC",
	},
	{
		.id = 66,
		.desc = "CLC_STATS_CAF",
	},
	{
		.id = 67,
		.desc = "CLC_FCG",
	},
	{
		.id = 68,
		.desc = "CLC_PDPC_BPC_1D_ALSC_STATS",
	},
	{
		.id = 69,
		.desc = "CLC_PDPC_BPC_1D_POST_FCG",
	},
	{
		.id = 70,
		.desc = "CLC_STATS_ALSC_BG",
	},
};

static struct cam_vfe_top_ver4_wr_client_desc vfe880_wr_client_desc[] = {
	{
		.wm_id = 0,
		.desc = "VIDEO_FULL_Y",
	},
	{
		.wm_id = 1,
		.desc = "VIDEO_FULL_C",
	},
	{
		.wm_id = 2,
		.desc = "VIDEO_DS_4:1",
	},
	{
		.wm_id = 3,
		.desc = "VIDEO_DS_16:1",
	},
	{
		.wm_id = 4,
		.desc = "DISPLAY_FULL_Y",
	},
	{
		.wm_id = 5,
		.desc = "DISPLAY_FULL_C",
	},
	{
		.wm_id = 6,
		.desc = "DISPLAY_DS_4:1",
	},
	{
		.wm_id = 7,
		.desc = "DISPLAY_DS_16:1",
	},
	{
		.wm_id = 8,
		.desc = "FD_Y",
	},
	{
		.wm_id = 9,
		.desc = "FD_C",
	},
	{
		.wm_id = 10,
		.desc = "PIXEL_RAW",
	},
	{
		.wm_id = 11,
		.desc = "STATS_BE0",
	},
	{
		.wm_id = 12,
		.desc = "STATS_BHIST0",
	},
	{
		.wm_id = 13,
		.desc = "STATS_TINTLESS_BG",
	},
	{
		.wm_id = 14,
		.desc = "STATS_AWB_BG",
	},
	{
		.wm_id = 15,
		.desc = "STATS_AWB_BFW",
	},
	{
		.wm_id = 16,
		.desc = "STATS_CAF",
	},
	{
		.wm_id = 17,
		.desc = "STATS_BHIST",
	},
	{
		.wm_id = 18,
		.desc = "STATS_BAYER_RS",
	},
	{
		.wm_id = 19,
		.desc = "STATS_IHIST",
	},
	{
		.wm_id = 20,
		.desc = "PDAF_0_2PD",
	},
	{
		.wm_id = 21,
		.desc = "PDAF_1_PREPROCESS_2PD",
	},
	{
		.wm_id = 22,
		.desc = "PDAF_2_PARSED_DATA",
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
		.desc = "LTM_STATS",
	},
	{
		.wm_id = 27,
		.desc = "STATS_ALSC",
	},
};

static struct cam_vfe_top_ver4_top_err_irq_desc vfe880_top_irq_err_desc[] = {
	{
		.bitmask = BIT(4),
		.err_name = "PP VIOLATION",
		.desc = "CLC CCIF violation",
	},
	{
		.bitmask = BIT(6),
		.err_name = "PDAF VIOLATION",
		.desc = "CLC PDAF violation",
	},
	{
		.bitmask = BIT(7),
		.err_name = "DYNAMIC PDAF SWITCH VIOLATION",
		.desc = "PD exposure select changes dynamically, the common vbi is insufficient",
	},
	{
		.bitmask = BIT(8),
		.err_name = "LCR PD INPUT TIMING PROTOCOL VIOLATION",
		.desc =
			"Sensor/SW: Input protocol timing on the LCR and PD path is not met, protocol expects SOF of LCR data to come before PD SOF, and LCR payload should only come after PD SOF",
	},
	{
		.bitmask = BIT(12),
		.err_name = "DSP IFE PROTOCOL VIOLATION",
		.desc = "CCIF protocol violation on the output Data",
	},
	{
		.bitmask = BIT(13),
		.err_name = "IFE DSP TX PROTOCOL VIOLATION",
		.desc = "CCIF protocol violation on the outgoing data to the DSP interface",
	},
	{
		.bitmask = BIT(14),
		.err_name = "DSP IFE RX PROTOCOL VIOLATION",
		.desc = "CCIF protocol violation on the incoming data from DSP before processed",
	},
	{
		.bitmask = BIT(15),
		.err_name = "DSP TX FIFO OVERFLOW",
		.desc = "Overflow on DSP interface TX path FIFO",
	},
	{
		.bitmask = BIT(16),
		.err_name = "DSP RX FIFO OVERFLOW",
		.desc = "Overflow on DSP interface RX path FIFO",
	},
	{
		.bitmask = BIT(17),
		.err_name = "DSP ERROR VIOLATION",
		.desc = "When DSP sends a error signal",
	},
	{
		.bitmask = BIT(18),
		.err_name = "DIAG VIOLATION",
		.desc = "Sensor: The HBI at IFE input is less than the spec",
		.debug = "Check sensor config",
	},
};

static struct cam_vfe_top_ver4_pdaf_violation_desc vfe880_pdaf_violation_desc[] = {
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
		.desc = "Constraint violation",
	},
};

static struct cam_vfe_top_ver4_pdaf_lcr_res_info vfe880_pdaf_lcr_res_mask[] = {
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

static struct cam_irq_register_set vfe880_top_irq_reg_set[2] = {
	{
		.mask_reg_offset   = 0x00000034,
		.clear_reg_offset  = 0x0000003C,
		.status_reg_offset = 0x00000044,
		.set_reg_offset    = 0x0000004C,
		.test_set_val      = BIT(1),
		.test_sub_val      = BIT(0),
	},
	{
		.mask_reg_offset   = 0x00000038,
		.clear_reg_offset  = 0x00000040,
		.status_reg_offset = 0x00000048,
	},
};

static struct cam_irq_controller_reg_info vfe880_top_irq_reg_info = {
	.num_registers = 2,
	.irq_reg_set = vfe880_top_irq_reg_set,
	.global_irq_cmd_offset = 0x00000030,
	.global_clear_bitmask  = 0x00000001,
	.global_set_bitmask    = 0x00000010,
	.clear_all_bitmask     = 0xFFFFFFFF,
};

static uint32_t vfe880_top_debug_reg[] = {
	0x000000A0,
	0x000000A4,
	0x000000A8,
	0x000000AC,
	0x000000B0,
	0x000000B4,
	0x000000B8,
	0x000000BC,
	0x000000C0,
	0x000000C4,
	0x000000C8,
	0x000000CC,
	0x000000D0,
	0x000000D4,
	0x000000D8,
	0x000000DC,
	0x000000E0,
	0x000000E4,
	0x000000E8,
};

static struct cam_vfe_top_ver4_reg_offset_common vfe880_top_common_reg = {
	.hw_version               = 0x00000000,
	.hw_capability            = 0x00000004,
	.lens_feature             = 0x00000008,
	.stats_feature            = 0x0000000C,
	.color_feature            = 0x00000010,
	.zoom_feature             = 0x00000014,
	.core_cfg_0               = 0x00000024,
	.core_cfg_1               = 0x00000028,
	.core_cfg_2               = 0x0000002C,
	.global_reset_cmd         = 0x00000030,
	.diag_config              = 0x00000050,
	.diag_sensor_status       = {0x00000054, 0x00000058},
	.diag_frm_cnt_status      = {0x0000005C, 0x00000060},
	.ipp_violation_status     = 0x00000064,
	.pdaf_violation_status    = 0x00000404,
	.core_cgc_ovd_0           = 0x00000018,
	.core_cgc_ovd_1           = 0x0000001C,
	.ahb_cgc_ovd              = 0x00000020,
	.dsp_status               = 0x0000006C,
	.stats_throttle_cfg_0     = 0x00000070,
	.stats_throttle_cfg_1     = 0x00000074,
	.stats_throttle_cfg_2     = 0x00000078,
	.core_cfg_4               = 0x00000080,
	.core_cfg_5               = 0x00000084,
	.core_cfg_6               = 0x00000088,
	.period_cfg               = 0x0000008C,
	.irq_sub_pattern_cfg      = 0x00000090,
	.epoch0_pattern_cfg       = 0x00000094,
	.epoch1_pattern_cfg       = 0x00000098,
	.epoch_height_cfg         = 0x0000009C,
	.bus_violation_status     = 0x00000C64,
	.bus_overflow_status      = 0x00000C68,
	.num_perf_counters        = 2,
	.perf_count_reg = {
		{
			.perf_count_cfg    = 0x00000100,
			.perf_pix_count    = 0x00000104,
			.perf_line_count   = 0x00000108,
			.perf_stall_count  = 0x0000010C,
			.perf_always_count = 0x00000110,
			.perf_count_status = 0x00000114,
		},
		{
			.perf_count_cfg    = 0x00000118,
			.perf_pix_count    = 0x0000011C,
			.perf_line_count   = 0x00000120,
			.perf_stall_count  = 0x00000124,
			.perf_always_count = 0x00000128,
			.perf_count_status = 0x0000012C,
		},
	},
	.top_debug_cfg            = 0x000000FC,
	.num_top_debug_reg        = CAM_VFE_880_NUM_DBG_REG,
	.pdaf_input_cfg_0         = 0x00000130,
	.pdaf_input_cfg_1         = 0x00000134,
	.top_debug = vfe880_top_debug_reg,
	.frame_timing_irq_reg_idx = CAM_IFE_IRQ_CAMIF_REG_STATUS1,
};

static struct cam_vfe_ver4_path_reg_data vfe880_pp_common_reg_data = {
	.sof_irq_mask                    = 0x00000001,
	.epoch0_irq_mask                 = 0x10000,
	.epoch1_irq_mask                 = 0x20000,
	.eof_irq_mask                    = 0x00000002,
	.error_irq_mask                  = 0x7F1D0,
	.enable_diagnostic_hw            = 0x1,
	.top_debug_cfg_en                = 3,
	.ipp_violation_mask              = 0x10,
	.pdaf_violation_mask             = 0x40,
	.diag_violation_mask             = 0x40000,
	.diag_sensor_sel_mask            = 0x0,
	.diag_frm_count_mask_0           = 0x10,
};

static struct cam_vfe_ver4_path_reg_data vfe880_vfe_full_rdi_reg_data[3] = {
	{
		.sof_irq_mask                    = 0x100,
		.eof_irq_mask                    = 0x200,
		.error_irq_mask                  = 0x0,
		.diag_sensor_sel_mask            = 0x2,
		.diag_frm_count_mask_0           = 0x40,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 3,
	},
	{
		.sof_irq_mask                    = 0x400,
		.eof_irq_mask                    = 0x800,
		.error_irq_mask                  = 0x0,
		.diag_sensor_sel_mask            = 0x4,
		.diag_frm_count_mask_0           = 0x80,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 3,
	},
	{
		.sof_irq_mask                    = 0x1000,
		.eof_irq_mask                    = 0x2000,
		.error_irq_mask                  = 0x0,
		.diag_sensor_sel_mask            = 0x6,
		.diag_frm_count_mask_0           = 0x100,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 3,
	},
};

static struct cam_vfe_ver4_path_reg_data vfe880_pdlib_reg_data = {
	.sof_irq_mask                    = 0x4,
	.eof_irq_mask                    = 0x8,
	.error_irq_mask                  = 0x0,
	.diag_sensor_sel_mask            = 0x8,
	.diag_frm_count_mask_0           = 0x20,
	.enable_diagnostic_hw            = 0x1,
	.top_debug_cfg_en                = 3,
};

struct cam_vfe_ver4_path_hw_info
	vfe880_rdi_hw_info_arr[] = {
	{
		.common_reg     = &vfe880_top_common_reg,
		.reg_data       = &vfe880_vfe_full_rdi_reg_data[0],
	},
	{
		.common_reg     = &vfe880_top_common_reg,
		.reg_data       = &vfe880_vfe_full_rdi_reg_data[1],
	},
	{
		.common_reg     = &vfe880_top_common_reg,
		.reg_data       = &vfe880_vfe_full_rdi_reg_data[2],
	},
};

static struct cam_vfe_top_ver4_debug_reg_info vfe880_dbg_reg_info[CAM_VFE_880_NUM_DBG_REG][8] = {
	VFE_DBG_INFO_ARRAY_4bit("test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"STATS_IHIST",
		"STATS_RS",
		"STATS_CAF",
		"GTM_BHIST",
		"TINTLESS_BG",
		"STATS_BFW",
		"STATS_BG",
		"STATS_BHIST"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"STATS_BE",
		"R2PD_DS16_C_VID",
		"R2PD_DS16_Y_VID",
		"crop_rnd_clamp_post_downscale_C_DS16_VID",
		"4to1_C_DS16_VID",
		"crop_rnd_clamp_post_downscale_Y_DS16_VID",
		"4to1_Y_DS16_VID",
		"crop_rnd_clamp_post_dsx_C_VID"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"R2PD_DS4_VID_C",
		"R2PD_DS4_VID_Y",
		"DSX_C",
		"crop_rnd_clamp_post_dsx_Y_VID",
		"DSX_Y",
		"crop_rnd_clamp_post_downscale_mn_C_VID",
		"downscale_mn_C_VID",
		"crop_rnd_clamp_post_downscale_mn_Y_VID"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"MNDS_Y_VID",
		"R2PD_DS16_C_DISP",
		"R2PD_DS16_Y_DISP",
		"crop_rnd_clamp_post_downscale_C_DS16_DISP",
		"4to1_C_DS16_DISP",
		"crop_rnd_clamp_post_downscale_Y_DS16_DISP",
		"4to1_Y_DS16_DISP",
		"R2PD_DS4_C_DISP"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"R2PD_DS4_Y_DISP",
		"crop_rnd_clamp_post_downscale_C_DS4_DISP",
		"4to1_C_DS4_DISP",
		"crop_rnd_clamp_post_downscale_Y_DS4_DISP",
		"4to1_Y_DS4_DISP",
		"crop_rnd_clamp_post_downscale_mn_C_DISP",
		"downscale_mn_C_DISP",
		"crop_rnd_clamp_post_downscale_mn_Y_DISP"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"downscale_mn_Y_DISP",
		"crop_rnd_clamp_post_downscale_mn_C_FD",
		"downscale_mn_C_FD",
		"crop_rnd_clamp_post_downscale_mn_Y_FD",
		"downscale_mn_Y_FD",
		"gtm_fd_out",
		"uvg",
		"color_xform"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"glut",
		"gtm",
		"color_correct",
		"demosaic",
		"hvx_tap2",
		"lcac",
		"bayer_ltm",
		"bayer_gtm"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"bls",
		"bpc_abf",
		"gic",
		"wb_gain",
		"lsc",
		"compdecomp_hxv_rx",
		"compdecomp_hxv_tx",
		"hvx_tap1"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"decompand",
		"reserved",
		"bincorrect",
		"bpc_pdpc",
		"channel_gain",
		"bayer_argb_ccif_converter",
		"crop_rnd_clamp_pre_argb_packer",
		"chroma_up_uv"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"chroma_up_y",
		"demux",
		"hxv_tap0",
		"preprocess",
		"reserved",
		"reserved",
		"bayer_ltm_bus_wr",
		"RDI2"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"RDI1",
		"RDI0",
		"reserved",
		"pdaf_2_bus_wr",
		"pdaf_1_bus_wr",
		"pdaf_1_bus_wr",
		"ihist_bus_wr",
		"flicker_rs_bus_wr"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"gtm_bhist_bus_wr",
		"caf_bus_wr",
		"bfw_bus_wr",
		"bg_bus_wr",
		"tintless_bg_bus_wr",
		"bhist_bus_wr",
		"be_bus_wr",
		"pixel_raw_bus_wr"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"fd_c_bus_wr",
		"fd_y_bus_wr",
		"disp_ds16_bus_wr",
		"disp_ds4_bus_wr",
		"disp_c_bus_wr",
		"disp_y_bus_wr",
		"vid_ds16_bus_Wr",
		"vid_ds4_bus_Wr"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"vid_c_bus_wr",
		"vid_y_bus_wr",
		"CLC_PDAF",
		"PIX_PP",
		"reserved",
		"reserved",
		"reserved",
		"reserved"
	),
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "lcr_pd_timing_debug"),
		VFE_DBG_INFO(32, "lcr_pd_timing_debug"),
		VFE_DBG_INFO(32, "lcr_pd_timing_debug"),
		VFE_DBG_INFO(32, "lcr_pd_timing_debug"),
		VFE_DBG_INFO(32, "lcr_pd_timing_debug"),
		VFE_DBG_INFO(32, "lcr_pd_timing_debug"),
		VFE_DBG_INFO(32, "lcr_pd_timing_debug"),
		VFE_DBG_INFO(32, "lcr_pd_timing_debug"),
	},
	VFE_DBG_INFO_ARRAY_4bit(
		"r2pd_reserved",
		"r2pd_reserved",
		"r2pd_reserved",
		"r2pd_reserved",
		"r2pd_reserved",
		"r2pd_reserved",
		"r2pd_reserved",
		"r2pd_reserved"
	),
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "lcr_pd_monitor"),
		VFE_DBG_INFO(32, "lcr_pd_monitor"),
		VFE_DBG_INFO(32, "lcr_pd_monitor"),
		VFE_DBG_INFO(32, "lcr_pd_monitor"),
		VFE_DBG_INFO(32, "lcr_pd_monitor"),
		VFE_DBG_INFO(32, "lcr_pd_monitor"),
		VFE_DBG_INFO(32, "lcr_pd_monitor"),
		VFE_DBG_INFO(32, "lcr_pd_monitor"),
	},
	{
		/* needs to be parsed separately, doesn't conform to I, V, R */
		VFE_DBG_INFO(32, "bus_wr_src_idle"),
		VFE_DBG_INFO(32, "bus_wr_src_idle"),
		VFE_DBG_INFO(32, "bus_wr_src_idle"),
		VFE_DBG_INFO(32, "bus_wr_src_idle"),
		VFE_DBG_INFO(32, "bus_wr_src_idle"),
		VFE_DBG_INFO(32, "bus_wr_src_idle"),
		VFE_DBG_INFO(32, "bus_wr_src_idle"),
		VFE_DBG_INFO(32, "bus_wr_src_idle"),
	},
};

static struct cam_vfe_top_ver4_diag_reg_info vfe880_diag_reg_info[] = {
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
		.name    = "FRAME_CNT_PIXEL_PIPE",
	},
	{
		.bitmask = 0xFF00,
		.name    = "FRAME_CNT_PDAF_PIPE",
	},
	{
		.bitmask = 0xFF0000,
		.name    = "FRAME_CNT_RDI_0_PIPE",
	},
	{
		.bitmask = 0xFF000000,
		.name    = "FRAME_CNT_RDI_1_PIPE",
	},
	{
		.bitmask = 0xFF,
		.name    = "FRAME_CNT_RDI_2_PIPE",
	},
	{
		.bitmask = 0xFF00,
		.name    = "FRAME_CNT_DSP_PIPE",
	},
};

static struct cam_vfe_top_ver4_diag_reg_fields vfe880_diag_sensor_field[] = {
	{
		.num_fields = 3,
		.field      = &vfe880_diag_reg_info[0],
	},
	{
		.num_fields = 1,
		.field      = &vfe880_diag_reg_info[3],
	},
};

static struct cam_vfe_top_ver4_diag_reg_fields vfe880_diag_frame_field[] = {
	{
		.num_fields = 4,
		.field      = &vfe880_diag_reg_info[4],
	},
	{
		.num_fields = 2,
		.field      = &vfe880_diag_reg_info[8],
	},
};

static struct cam_vfe_ver4_fcg_module_info vfe880_fcg_module_info = {
	.max_fcg_ch_ctx                      = 1,
	.max_fcg_predictions                 = 3,
	.fcg_index_shift                     = 16,
	.max_reg_val_pair_size               = 4,
	.fcg_type_size                       = 2,
	.fcg_phase_index_cfg_0               = 0xC870,
	.fcg_phase_index_cfg_1               = 0xC874,
};

static struct cam_vfe_top_ver4_hw_info vfe880_top_hw_info = {
	.common_reg = &vfe880_top_common_reg,
	.vfe_full_hw_info = {
		.common_reg     = &vfe880_top_common_reg,
		.reg_data       = &vfe880_pp_common_reg_data,
	},
	.pdlib_hw_info = {
		.common_reg     = &vfe880_top_common_reg,
		.reg_data       = &vfe880_pdlib_reg_data,
	},
	.rdi_hw_info            = vfe880_rdi_hw_info_arr,
	.wr_client_desc         = vfe880_wr_client_desc,
	.ipp_module_desc        = vfe880_ipp_mod_desc,
	.num_mux = 5,
	.mux_type = {
		CAM_VFE_CAMIF_VER_4_0,
		CAM_VFE_PDLIB_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
	},
	.num_path_port_map = 3,
	.path_port_map = {
		{CAM_ISP_HW_VFE_IN_PDLIB, CAM_ISP_IFE_OUT_RES_2PD},
		{CAM_ISP_HW_VFE_IN_PDLIB, CAM_ISP_IFE_OUT_RES_PREPROCESS_2PD},
		{CAM_ISP_HW_VFE_IN_PDLIB, CAM_ISP_IFE_OUT_RES_PDAF_PARSED_DATA},
	},
	.num_rdi                         = ARRAY_SIZE(vfe880_rdi_hw_info_arr),
	.num_top_errors                  = ARRAY_SIZE(vfe880_top_irq_err_desc),
	.top_err_desc                    = vfe880_top_irq_err_desc,
	.num_pdaf_violation_errors       = ARRAY_SIZE(vfe880_pdaf_violation_desc),
	.pdaf_violation_desc             = vfe880_pdaf_violation_desc,
	.top_debug_reg_info              = &vfe880_dbg_reg_info,
	.pdaf_lcr_res_mask               = vfe880_pdaf_lcr_res_mask,
	.num_pdaf_lcr_res                = ARRAY_SIZE(vfe880_pdaf_lcr_res_mask),
	.fcg_module_info                 = &vfe880_fcg_module_info,
	.fcg_supported                   = true,
	.diag_sensor_info                = vfe880_diag_sensor_field,
	.diag_frame_info                 = vfe880_diag_frame_field,
};

static struct cam_irq_register_set vfe880_bus_irq_reg[2] = {
	{
		.mask_reg_offset   = 0x00000C18,
		.clear_reg_offset  = 0x00000C20,
		.status_reg_offset = 0x00000C28,
		.set_reg_offset    = 0x00000C50,
	},
	{
		.mask_reg_offset   = 0x00000C1C,
		.clear_reg_offset  = 0x00000C24,
		.status_reg_offset = 0x00000C2C,
		.set_reg_offset    = 0x00000C54,
	},
};

static struct cam_vfe_bus_ver3_reg_offset_ubwc_client
	vfe880_ubwc_regs_client_0 = {
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
	vfe880_ubwc_regs_client_1 = {
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
	vfe880_ubwc_regs_client_4 = {
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
	vfe880_ubwc_regs_client_5 = {
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

static uint32_t vfe880_out_port_mid[][4] = {
	{34, 0, 0, 0},
	{35, 0, 0, 0},
	{36, 0, 0, 0},
	{8, 9, 10, 11},
	{11, 0, 0, 0},
	{12, 0, 0, 0},
	{32, 33, 0, 0},
	{27, 28, 29, 0},
	{8, 0, 0, 0},
	{18, 0, 0, 0},
	{21, 0, 0, 0},
	{19, 0, 0, 0},
	{17, 0, 0, 0},
	{23, 0, 0, 0},
	{24, 0, 0, 0},
	{12, 13, 14, 15},
	{13, 0, 0, 0},
	{14, 0, 0, 0},
	{9, 0, 0, 0},
	{20, 0, 0, 0},
	{10, 0, 0, 0},
	{16, 0, 0, 0},
	{25, 26, 0, 0},
	{22, 0, 0, 0},
	{30, 0, 0, 0},
};

static struct cam_vfe_bus_ver3_err_irq_desc vfe880_bus_irq_err_desc_0[] = {
	{
		.bitmask = BIT(26),
		.err_name = "IPCC_FENCE_DATA_ERR",
		.desc = "IPCC or FENCE Data was not available in the Input Fifo",
	},
	{
		.bitmask = BIT(27),
		.err_name = "IPCC_FENCE_ADDR_ERR",
		.desc = "IPCC or FENCE address fifo was empty and read was attempted",
	},
	{
		.bitmask = BIT(28),
		.err_name = "CONS_VIOLATION",
		.desc = "Programming of software registers violated the constraints",
	},
	{
		.bitmask = BIT(30),
		.err_name = "VIOLATION",
		.desc = "Client has a violation in ccif protocol at input",
	},
	{
		.bitmask = BIT(31),
		.err_name = "IMAGE_SIZE_VIOLATION",
		.desc = "Programmed image size is not same as image size from the CCIF",
	},
};

static struct cam_vfe_bus_ver3_err_irq_desc vfe880_bus_irq_err_desc_1[] = {
	{
		.bitmask = BIT(28),
		.err_name = "EARLY_DONE",
		.desc = "Buf done for each client. Early done irq for clients STATS_BAF",
	},
	{
		.bitmask = BIT(29),
		.err_name = "EARLY_DONE",
		.desc = "Buf done for each client. Early done irq for clients STATS_BAF",
	},
};

static struct cam_vfe_bus_ver3_hw_info vfe880_bus_hw_info = {
	.common_reg = {
		.hw_version                       = 0x00000C00,
		.cgc_ovd                          = 0x00000C08,
		.if_frameheader_cfg               = {
			0x00000C34,
			0x00000C38,
			0x00000C3C,
			0x00000C40,
			0x00000C44,
		},
		.ubwc_static_ctrl                 = 0x00000C58,
		.pwr_iso_cfg                      = 0x00000C5C,
		.overflow_status_clear            = 0x00000C60,
		.ccif_violation_status            = 0x00000C64,
		.overflow_status                  = 0x00000C68,
		.image_size_violation_status      = 0x00000C70,
		.debug_status_top_cfg             = 0x00000CF0,
		.debug_status_top                 = 0x00000CF4,
		.test_bus_ctrl                    = 0x00000CDC,
		.wm_mode_shift                    = 16,
		.wm_mode_val                      = { 0x0, 0x1, 0x2 },
		.wm_en_shift                      = 0,
		.frmheader_en_shift               = 2,
		.virtual_frm_en_shift		  = 1,
		.irq_reg_info = {
			.num_registers            = 2,
			.irq_reg_set              = vfe880_bus_irq_reg,
			.global_irq_cmd_offset    = 0x00000C30,
			.global_clear_bitmask     = 0x00000001,
		},
	},
	.num_client = CAM_VFE_BUS_VER3_880_MAX_CLIENTS,
	.bus_client_reg = {
		/* BUS Client 0 FULL Y */
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
			.addr_status_0            = 0x00000E78,
			.addr_status_1            = 0x00000E7C,
			.addr_status_2            = 0x00000E80,
			.addr_status_3            = 0x00000E84,
			.debug_status_cfg         = 0x00000E88,
			.debug_status_0           = 0x00000E8C,
			.debug_status_1           = 0x00000E90,
			.bw_limiter_addr          = 0x00000E1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = &vfe880_ubwc_regs_client_0,
			.supported_formats        = BIT_ULL(CAM_FORMAT_UBWC_NV12) |
				BIT_ULL(CAM_FORMAT_UBWC_NV12_4R) | BIT_ULL(CAM_FORMAT_NV21) |
				BIT_ULL(CAM_FORMAT_NV12) | BIT_ULL(CAM_FORMAT_Y_ONLY) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) | BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10),
		},
		/* BUS Client 1 FULL C */
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
			.addr_cfg                 = 0x00000E70,
			.addr_status_0            = 0x00000F78,
			.addr_status_1            = 0x00000F7C,
			.addr_status_2            = 0x00000F80,
			.addr_status_3            = 0x00000F84,
			.debug_status_cfg         = 0x00000F88,
			.debug_status_0           = 0x00000F8C,
			.debug_status_1           = 0x00000F90,
			.bw_limiter_addr          = 0x00000F1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = &vfe880_ubwc_regs_client_1,
			.supported_formats        = BIT_ULL(CAM_FORMAT_UBWC_NV12) |
				BIT_ULL(CAM_FORMAT_UBWC_NV12_4R) | BIT_ULL(CAM_FORMAT_NV21) |
				BIT_ULL(CAM_FORMAT_NV12) | BIT_ULL(CAM_FORMAT_Y_ONLY) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) | BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10),
		},
		/* BUS Client 2 DS4 */
		{
			.cfg                      = 0x00001000,
			.image_addr               = 0x00001004,
			.frame_incr               = 0x00001008,
			.image_cfg_0              = 0x0000100C,
			.image_cfg_1              = 0x00001010,
			.image_cfg_2              = 0x00001014,
			.packer_cfg               = 0x00001018,
			.irq_subsample_period     = 0x00001030,
			.irq_subsample_pattern    = 0x00001034,
			.framedrop_period         = 0x00001038,
			.framedrop_pattern        = 0x0000103C,
			.mmu_prefetch_cfg         = 0x00001060,
			.mmu_prefetch_max_offset  = 0x00001064,
			.system_cache_cfg         = 0x00001068,
			.addr_cfg                 = 0x00001070,
			.addr_status_0            = 0x00001078,
			.addr_status_1            = 0x0000107C,
			.addr_status_2            = 0x00001080,
			.addr_status_3            = 0x00001084,
			.debug_status_cfg         = 0x00001088,
			.debug_status_0           = 0x0000108C,
			.debug_status_1           = 0x00001090,
			.bw_limiter_addr          = 0x0000101C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 3 DS16 */
		{
			.cfg                      = 0x00001100,
			.image_addr               = 0x00001104,
			.frame_incr               = 0x00001108,
			.image_cfg_0              = 0x0000110C,
			.image_cfg_1              = 0x00001110,
			.image_cfg_2              = 0x00001114,
			.packer_cfg               = 0x00001118,
			.irq_subsample_period     = 0x00001130,
			.irq_subsample_pattern    = 0x00001134,
			.framedrop_period         = 0x00001138,
			.framedrop_pattern        = 0x0000113C,
			.mmu_prefetch_cfg         = 0x00001160,
			.mmu_prefetch_max_offset  = 0x00001164,
			.system_cache_cfg         = 0x00001168,
			.addr_cfg                 = 0x00001170,
			.addr_status_0            = 0x00001178,
			.addr_status_1            = 0x0000117C,
			.addr_status_2            = 0x00001180,
			.addr_status_3            = 0x00001184,
			.debug_status_cfg         = 0x00001188,
			.debug_status_0           = 0x0000118C,
			.debug_status_1           = 0x00001190,
			.bw_limiter_addr          = 0x0000111C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 4 DISP Y */
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
			.addr_status_0            = 0x00001278,
			.addr_status_1            = 0x0000127C,
			.addr_status_2            = 0x00001280,
			.addr_status_3            = 0x00001284,
			.debug_status_cfg         = 0x00001288,
			.debug_status_0           = 0x0000128C,
			.debug_status_1           = 0x00001290,
			.bw_limiter_addr          = 0x0000121C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_1,
			.ubwc_regs                = &vfe880_ubwc_regs_client_4,
			.supported_formats        = BIT_ULL(CAM_FORMAT_UBWC_NV12) |
				BIT_ULL(CAM_FORMAT_UBWC_NV12_4R) | BIT_ULL(CAM_FORMAT_NV21) |
				BIT_ULL(CAM_FORMAT_NV12) | BIT_ULL(CAM_FORMAT_Y_ONLY) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) | BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10),

		},
		/* BUS Client 5 DISP C */
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
			.addr_status_0            = 0x00001378,
			.addr_status_1            = 0x0000137C,
			.addr_status_2            = 0x00001380,
			.addr_status_3            = 0x00001384,
			.debug_status_cfg         = 0x00001388,
			.debug_status_0           = 0x0000138C,
			.debug_status_1           = 0x00001390,
			.bw_limiter_addr          = 0x0000131C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_1,
			.ubwc_regs                = &vfe880_ubwc_regs_client_5,
			.supported_formats        = BIT_ULL(CAM_FORMAT_UBWC_NV12) |
				BIT_ULL(CAM_FORMAT_UBWC_NV12_4R) | BIT_ULL(CAM_FORMAT_NV21) |
				BIT_ULL(CAM_FORMAT_NV12) | BIT_ULL(CAM_FORMAT_Y_ONLY) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) | BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10),
		},
		/* BUS Client 6 DISP DS4 */
		{
			.cfg                      = 0x00001400,
			.image_addr               = 0x00001404,
			.frame_incr               = 0x00001408,
			.image_cfg_0              = 0x0000140C,
			.image_cfg_1              = 0x00001410,
			.image_cfg_2              = 0x00001414,
			.packer_cfg               = 0x00001418,
			.irq_subsample_period     = 0x00001430,
			.irq_subsample_pattern    = 0x00001434,
			.framedrop_period         = 0x00001438,
			.framedrop_pattern        = 0x0000143C,
			.mmu_prefetch_cfg         = 0x00001460,
			.mmu_prefetch_max_offset  = 0x00001464,
			.system_cache_cfg         = 0x00001468,
			.addr_cfg                 = 0x00001470,
			.addr_status_0            = 0x00001478,
			.addr_status_1            = 0x0000147C,
			.addr_status_2            = 0x00001480,
			.addr_status_3            = 0x00001484,
			.debug_status_cfg         = 0x00001488,
			.debug_status_0           = 0x0000148C,
			.debug_status_1           = 0x00001490,
			.bw_limiter_addr          = 0x0000141C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_1,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 7 DISP DS16 */
		{
			.cfg                      = 0x00001500,
			.image_addr               = 0x00001504,
			.frame_incr               = 0x00001508,
			.image_cfg_0              = 0x0000150C,
			.image_cfg_1              = 0x00001510,
			.image_cfg_2              = 0x00001514,
			.packer_cfg               = 0x00001518,
			.irq_subsample_period     = 0x00001530,
			.irq_subsample_pattern    = 0x00001534,
			.framedrop_period         = 0x00001538,
			.framedrop_pattern        = 0x0000153C,
			.mmu_prefetch_cfg         = 0x00001560,
			.mmu_prefetch_max_offset  = 0x00001564,
			.system_cache_cfg         = 0x00001568,
			.addr_cfg                 = 0x00001570,
			.addr_status_0            = 0x00001578,
			.addr_status_1            = 0x0000157C,
			.addr_status_2            = 0x00001580,
			.addr_status_3            = 0x00001584,
			.debug_status_cfg         = 0x00001588,
			.debug_status_0           = 0x0000158C,
			.debug_status_1           = 0x00001590,
			.bw_limiter_addr          = 0x0000151C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_1,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 8 FD Y */
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
			.addr_status_0            = 0x00001678,
			.addr_status_1            = 0x0000167C,
			.addr_status_2            = 0x00001680,
			.addr_status_3            = 0x00001684,
			.debug_status_cfg         = 0x00001688,
			.debug_status_0           = 0x0000168C,
			.debug_status_1           = 0x00001690,
			.bw_limiter_addr          = 0x0000161C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_UBWC_NV12) |
				BIT_ULL(CAM_FORMAT_UBWC_NV12_4R) | BIT_ULL(CAM_FORMAT_NV21) |
				BIT_ULL(CAM_FORMAT_NV12) | BIT_ULL(CAM_FORMAT_Y_ONLY) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) | BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10),
		},
		/* BUS Client 9 FD C */
		{
			.cfg                      = 0x00001700,
			.image_addr               = 0x00001704,
			.frame_incr               = 0x00001708,
			.image_cfg_0              = 0x0000170C,
			.image_cfg_1              = 0x00001710,
			.image_cfg_2              = 0x00001714,
			.packer_cfg               = 0x00001718,
			.irq_subsample_period     = 0x00001730,
			.irq_subsample_pattern    = 0x00001734,
			.framedrop_period         = 0x00001738,
			.framedrop_pattern        = 0x0000173C,
			.mmu_prefetch_cfg         = 0x00001760,
			.mmu_prefetch_max_offset  = 0x00001764,
			.system_cache_cfg         = 0x00001768,
			.addr_cfg                 = 0x00001770,
			.addr_status_0            = 0x00001778,
			.addr_status_1            = 0x0000177C,
			.addr_status_2            = 0x00001780,
			.addr_status_3            = 0x00001784,
			.debug_status_cfg         = 0x00001788,
			.debug_status_0           = 0x0000178C,
			.debug_status_1           = 0x00001790,
			.bw_limiter_addr          = 0x0000171C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_UBWC_NV12) |
				BIT_ULL(CAM_FORMAT_UBWC_NV12_4R) | BIT_ULL(CAM_FORMAT_NV21) |
				BIT_ULL(CAM_FORMAT_NV12) | BIT_ULL(CAM_FORMAT_Y_ONLY) |
				BIT_ULL(CAM_FORMAT_UBWC_TP10) | BIT_ULL(CAM_FORMAT_TP10) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10),
		},
		/* BUS Client 10 PIXEL RAW */
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
			.addr_status_0            = 0x00001878,
			.addr_status_1            = 0x0000187C,
			.addr_status_2            = 0x00001880,
			.addr_status_3            = 0x00001884,
			.debug_status_cfg         = 0x00001888,
			.debug_status_0           = 0x0000188C,
			.debug_status_1           = 0x00001890,
			.bw_limiter_addr          = 0x0000181C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_3,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_MIPI_RAW_6) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_8) | BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_12) | BIT_ULL(CAM_FORMAT_MIPI_RAW_14) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_16) | BIT_ULL(CAM_FORMAT_MIPI_RAW_20) |
				BIT_ULL(CAM_FORMAT_PLAIN8) | BIT_ULL(CAM_FORMAT_PLAIN16_8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10) | BIT_ULL(CAM_FORMAT_PLAIN16_12) |
				BIT_ULL(CAM_FORMAT_PLAIN16_14) | BIT_ULL(CAM_FORMAT_PLAIN16_16) |
				BIT_ULL(CAM_FORMAT_PLAIN32_20) | BIT_ULL(CAM_FORMAT_PLAIN64) |
				BIT_ULL(CAM_FORMAT_PLAIN128),
		},
		/* BUS Client 11 STATS BE 0 */
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
			.addr_status_0            = 0x00001978,
			.addr_status_1            = 0x0000197C,
			.addr_status_2            = 0x00001980,
			.addr_status_3            = 0x00001984,
			.debug_status_cfg         = 0x00001988,
			.debug_status_0           = 0x0000198C,
			.debug_status_1           = 0x00001990,
			.bw_limiter_addr          = 0x0000191C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_4,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 12 STATS BHIST 0 */
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
			.addr_status_0            = 0x00001A78,
			.addr_status_1            = 0x00001A7C,
			.addr_status_2            = 0x00001A80,
			.addr_status_3            = 0x00001A84,
			.debug_status_cfg         = 0x00001A88,
			.debug_status_0           = 0x00001A8C,
			.debug_status_1           = 0x00001A90,
			.bw_limiter_addr          = 0x00001A1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_4,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 13 STATS TINTLESS BG */
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
			.addr_status_0            = 0x00001B78,
			.addr_status_1            = 0x00001B7C,
			.addr_status_2            = 0x00001B80,
			.addr_status_3            = 0x00001B84,
			.debug_status_cfg         = 0x00001B88,
			.debug_status_0           = 0x00001B8C,
			.debug_status_1           = 0x00001B90,
			.bw_limiter_addr          = 0x00001B1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_5,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 14 STATS AWB BG */
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
			.addr_status_0            = 0x00001C78,
			.addr_status_1            = 0x00001C7C,
			.addr_status_2            = 0x00001C80,
			.addr_status_3            = 0x00001C84,
			.debug_status_cfg         = 0x00001C88,
			.debug_status_0           = 0x00001C8C,
			.debug_status_1           = 0x00001C90,
			.bw_limiter_addr          = 0x00001C1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_6,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 15 STATS AWB BFW */
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
			.addr_status_0            = 0x00001D78,
			.addr_status_1            = 0x00001D7C,
			.addr_status_2            = 0x00001D80,
			.addr_status_3            = 0x00001D84,
			.debug_status_cfg         = 0x00001D88,
			.debug_status_0           = 0x00001D8C,
			.debug_status_1           = 0x00001D90,
			.bw_limiter_addr          = 0x00001D1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_6,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
		/* BUS Client 16 STATS CAF */
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
			.addr_status_0            = 0x00001E78,
			.addr_status_1            = 0x00001E7C,
			.addr_status_2            = 0x00001E80,
			.addr_status_3            = 0x00001E84,
			.debug_status_cfg         = 0x00001E88,
			.debug_status_0           = 0x00001E8C,
			.debug_status_1           = 0x00001E90,
			.bw_limiter_addr          = 0x00001E1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_7,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 17 STATS BHIST */
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
			.addr_status_0            = 0x00001F78,
			.addr_status_1            = 0x00001F7C,
			.addr_status_2            = 0x00001F80,
			.addr_status_3            = 0x00001F84,
			.debug_status_cfg         = 0x00001F88,
			.debug_status_0           = 0x00001F8C,
			.debug_status_1           = 0x00001F90,
			.bw_limiter_addr          = 0x00001F1C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_8,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 18 STATS BAYER RS */
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
			.addr_status_0            = 0x00002078,
			.addr_status_1            = 0x0000207C,
			.addr_status_2            = 0x00002080,
			.addr_status_3            = 0x00002084,
			.debug_status_cfg         = 0x00002088,
			.debug_status_0           = 0x0000208C,
			.debug_status_1           = 0x00002090,
			.bw_limiter_addr          = 0x0000201C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_9,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 19 STATS IHIST */
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
			.addr_status_0            = 0x00002178,
			.addr_status_1            = 0x0000217C,
			.addr_status_2            = 0x00002180,
			.addr_status_3            = 0x00002184,
			.debug_status_cfg         = 0x00002188,
			.debug_status_0           = 0x0000218C,
			.debug_status_1           = 0x00002190,
			.bw_limiter_addr          = 0x0000211C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_10,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 20 PDAF_0_2PD */
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
			.addr_status_0            = 0x00002278,
			.addr_status_1            = 0x0000227C,
			.addr_status_2            = 0x00002280,
			.addr_status_3            = 0x00002284,
			.debug_status_cfg         = 0x00002288,
			.debug_status_0           = 0x0000228C,
			.debug_status_1           = 0x00002290,
			.bw_limiter_addr          = 0x0000221C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_11,
			.ubwc_regs                = NULL,
			.supported_formats        = 0xFFFFFFFFFFFFFFFF,
		},
		/* BUS Client 21 PDAF V2.0 PD DATA PDAF_1_PREPROCESS_2PD */
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
			.addr_status_0            = 0x00002378,
			.addr_status_1            = 0x0000237C,
			.addr_status_2            = 0x00002380,
			.addr_status_3            = 0x00002384,
			.debug_status_cfg         = 0x00002388,
			.debug_status_0           = 0x0000238C,
			.debug_status_1           = 0x00002390,
			.bw_limiter_addr          = 0x0000231C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_11,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN16_8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10) | BIT_ULL(CAM_FORMAT_PLAIN16_12) |
				BIT_ULL(CAM_FORMAT_PLAIN16_14) | BIT_ULL(CAM_FORMAT_PLAIN16_16),
		},
		/* BUS Client 22 PDAF V2.0 PDAF_2_PARSED_DATA */
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
			.addr_status_0            = 0x00002478,
			.addr_status_1            = 0x0000247C,
			.addr_status_2            = 0x00002480,
			.addr_status_3            = 0x00002484,
			.debug_status_cfg         = 0x00002488,
			.debug_status_0           = 0x0000248C,
			.debug_status_1           = 0x00002490,
			.bw_limiter_addr          = 0x0000241C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_11,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN16_8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10) | BIT_ULL(CAM_FORMAT_PLAIN16_12) |
				BIT_ULL(CAM_FORMAT_PLAIN16_14) | BIT_ULL(CAM_FORMAT_PLAIN16_16),
		},
		/* BUS Client 23 RDI0 */
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
			.line_done_cfg            = 0x0000252C,
			.irq_subsample_period     = 0x00002530,
			.irq_subsample_pattern    = 0x00002534,
			.framedrop_period         = 0x00002538,
			.framedrop_pattern        = 0x0000253C,
			.mmu_prefetch_cfg         = 0x00002560,
			.mmu_prefetch_max_offset  = 0x00002564,
			.system_cache_cfg         = 0x00002568,
			.addr_cfg                 = 0x00002570,
			.addr_status_0            = 0x00002578,
			.addr_status_1            = 0x0000257C,
			.addr_status_2            = 0x00002580,
			.addr_status_3            = 0x00002584,
			.debug_status_cfg         = 0x00002588,
			.debug_status_0           = 0x0000258C,
			.debug_status_1           = 0x00002590,
			.bw_limiter_addr          = 0x0000251C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_12,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_6) | BIT_ULL(CAM_FORMAT_MIPI_RAW_8) |
				BIT_ULL(CAM_FORMAT_YUV422) | BIT_ULL(CAM_FORMAT_MIPI_RAW_12) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_14) | BIT_ULL(CAM_FORMAT_MIPI_RAW_16) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_20) | BIT_ULL(CAM_FORMAT_PLAIN128) |
				BIT_ULL(CAM_FORMAT_PLAIN32_20) | BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10) | BIT_ULL(CAM_FORMAT_PLAIN16_12) |
				BIT_ULL(CAM_FORMAT_PLAIN16_14) | BIT_ULL(CAM_FORMAT_PLAIN16_16) |
				BIT_ULL(CAM_FORMAT_PLAIN64) | BIT_ULL(CAM_FORMAT_YUV422_10),
		},
		/* BUS Client 24 RDI1 */
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
			.line_done_cfg            = 0x0000262C,
			.irq_subsample_period     = 0x00002630,
			.irq_subsample_pattern    = 0x00002634,
			.framedrop_period         = 0x00002638,
			.framedrop_pattern        = 0x0000263C,
			.mmu_prefetch_cfg         = 0x00002660,
			.mmu_prefetch_max_offset  = 0x00002664,
			.system_cache_cfg         = 0x00002668,
			.addr_cfg                 = 0x00002670,
			.addr_status_0            = 0x00002678,
			.addr_status_1            = 0x0000267C,
			.addr_status_2            = 0x00002680,
			.addr_status_3            = 0x00002684,
			.debug_status_cfg         = 0x00002688,
			.debug_status_0           = 0x0000268C,
			.debug_status_1           = 0x00002690,
			.bw_limiter_addr          = 0x0000261C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_13,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_6) | BIT_ULL(CAM_FORMAT_MIPI_RAW_8) |
				BIT_ULL(CAM_FORMAT_YUV422) | BIT_ULL(CAM_FORMAT_MIPI_RAW_12) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_14) | BIT_ULL(CAM_FORMAT_MIPI_RAW_16) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_20) | BIT_ULL(CAM_FORMAT_PLAIN128) |
				BIT_ULL(CAM_FORMAT_PLAIN32_20) | BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10) | BIT_ULL(CAM_FORMAT_PLAIN16_12) |
				BIT_ULL(CAM_FORMAT_PLAIN16_14) | BIT_ULL(CAM_FORMAT_PLAIN16_16) |
				BIT_ULL(CAM_FORMAT_PLAIN64) | BIT_ULL(CAM_FORMAT_YUV422_10),
		},
		/* BUS Client 25 RDI2 */
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
			.line_done_cfg            = 0x0000272C,
			.irq_subsample_period     = 0x00002730,
			.irq_subsample_pattern    = 0x00002734,
			.framedrop_period         = 0x00002738,
			.framedrop_pattern        = 0x0000273C,
			.mmu_prefetch_cfg         = 0x00002760,
			.mmu_prefetch_max_offset  = 0x00002764,
			.system_cache_cfg         = 0x00002768,
			.addr_cfg                 = 0x00002770,
			.addr_status_0            = 0x00002778,
			.addr_status_1            = 0x0000277C,
			.addr_status_2            = 0x00002780,
			.addr_status_3            = 0x00002784,
			.debug_status_cfg         = 0x00002788,
			.debug_status_0           = 0x0000278C,
			.debug_status_1           = 0x00002790,
			.bw_limiter_addr          = 0x0000271C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_14,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_MIPI_RAW_10) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_6) | BIT_ULL(CAM_FORMAT_MIPI_RAW_8) |
				BIT_ULL(CAM_FORMAT_YUV422) | BIT_ULL(CAM_FORMAT_MIPI_RAW_12) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_14) | BIT_ULL(CAM_FORMAT_MIPI_RAW_16) |
				BIT_ULL(CAM_FORMAT_MIPI_RAW_20) | BIT_ULL(CAM_FORMAT_PLAIN128) |
				BIT_ULL(CAM_FORMAT_PLAIN32_20) | BIT_ULL(CAM_FORMAT_PLAIN8) |
				BIT_ULL(CAM_FORMAT_PLAIN16_10) | BIT_ULL(CAM_FORMAT_PLAIN16_12) |
				BIT_ULL(CAM_FORMAT_PLAIN16_14) | BIT_ULL(CAM_FORMAT_PLAIN16_16) |
				BIT_ULL(CAM_FORMAT_PLAIN64) | BIT_ULL(CAM_FORMAT_YUV422_10),
		},
		/* BUS Client 26 LTM STATS */
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
			.addr_status_0            = 0x00002878,
			.addr_status_1            = 0x0000287C,
			.addr_status_2            = 0x00002880,
			.addr_status_3            = 0x00002884,
			.debug_status_cfg         = 0x00002888,
			.debug_status_0           = 0x0000288C,
			.debug_status_1           = 0x00002890,
			.bw_limiter_addr          = 0x0000281C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_3,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN32),
		},
		/* BUS Client 27 ALSC BG */
		{
			.cfg                      = 0x00002900,
			.image_addr               = 0x00002904,
			.frame_incr               = 0x00002908,
			.image_cfg_0              = 0x0000290C,
			.image_cfg_1              = 0x00002910,
			.image_cfg_2              = 0x00002914,
			.packer_cfg               = 0x00002918,
			.frame_header_addr        = 0x00002920,
			.frame_header_incr        = 0x00002924,
			.frame_header_cfg         = 0x00002928,
			.irq_subsample_period     = 0x00002930,
			.irq_subsample_pattern    = 0x00002934,
			.framedrop_period         = 0x00002938,
			.framedrop_pattern        = 0x0000293C,
			.mmu_prefetch_cfg         = 0x00002960,
			.mmu_prefetch_max_offset  = 0x00002964,
			.system_cache_cfg         = 0x00002968,
			.addr_cfg                 = 0x00002970,
			.addr_status_0            = 0x00002978,
			.addr_status_1            = 0x0000297C,
			.addr_status_2            = 0x00002980,
			.addr_status_3            = 0x00002984,
			.debug_status_cfg         = 0x00002988,
			.debug_status_0           = 0x0000298C,
			.debug_status_1           = 0x00002990,
			.bw_limiter_addr          = 0x0000291C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_15,
			.ubwc_regs                = NULL,
			.supported_formats        = BIT_ULL(CAM_FORMAT_PLAIN64),
		},
	},
	.num_out = 25,
	.vfe_out_hw_info = {
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI0,
			.max_width     = 16384,
			.max_height    = 16384,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_2,
			.mid           = vfe880_out_port_mid[0],
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
			.mid           = vfe880_out_port_mid[1],
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
			.mid           = vfe880_out_port_mid[2],
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
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_FULL,
			.max_width     = 4928,
			.max_height    = 4096,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[3],
			.num_mid       = 4,
			.num_wm        = 2,
			.line_based    = 1,
			.wm_idx        = {
				0,
				1,
			},
			.name          = {
				"FULL_Y",
				"FULL_C",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_DS4,
			.max_width     = 1696,
			.max_height    = 1080,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[4],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				2,
			},
			.name          = {
				"DS_4",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_DS16,
			.max_width     = 424,
			.max_height    = 1080,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[5],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				3,
			},
			.name          = {
				"DS_16",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RAW_DUMP,
			.max_width     = 7296,
			.max_height    = 16384,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[6],
			.num_mid       = 2,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				10,
			},
			.name          = {
				"PIXEL_RAW",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_FD,
			.max_width     = 2304,
			.max_height    = 1080,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[7],
			.num_mid       = 3,
			.num_wm        = 2,
			.line_based    = 1,
			.wm_idx        = {
				8,
				9,
			},
			.name          = {
				"FD_Y",
				"FD_C",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_2PD,
			.max_width     = 14592,
			.max_height    = 4096,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_1,
			.mid           = vfe880_out_port_mid[8],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				20,
			},
			.name          = {
				"PDAF_0_2PD",
			},
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER3_VFE_OUT_STATS_TL_BG,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[9],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				13,
			},
			.name          = {
				"STATS_TL_BG",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_CAF,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[10],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				16,
			},
			.name          = {
				"STATS_BF",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_AWB_BG,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[11],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				14,
			},
			.name          = {
				"STATS_AWB_BGB",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_BHIST,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[12],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				12,
			},
			.name          = {
				"STATS_BHIST",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_BAYER_RS,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[13],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				18,
			},
			.name          = {
				"STATS_RS",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_IHIST,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[14],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				19,
			},
			.name          = {
				"STATS_IHIST",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_FULL_DISP,
			.max_width     = 4928,
			.max_height    = 4096,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[15],
			.num_mid       = 4,
			.num_wm        = 2,
			.line_based    = 1,
			.wm_idx        = {
				4,
				5,
			},
			.name          = {
				"FULL_DISP_Y",
				"FULL_DISP_C",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_DS4_DISP,
			.max_width     = 1232,
			.max_height    = 1080,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[16],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				6,
			},
			.name          = {
				"DISP_DS_4",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_DS16_DISP,
			.max_width     = 308,
			.max_height    = 1080,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[17],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				7,
			},
			.name          = {
				"DISP_DS_16",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_2PD,
			.max_width     = 1920,
			.max_height    = 1080,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_1,
			.mid           = vfe880_out_port_mid[18],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				21,
			},
			.name          = {
				"PDAF_1_PREPROCESS_2PD",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_AWB_BFW,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[19],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				15,
			},
			.name          = {
				"AWB_BFW",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_PDAF_PARSED,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_1,
			.mid           = vfe880_out_port_mid[20],
			.num_mid       = 1,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				22,
			},
			.name          = {
				"PDAF_2_PARSED_DATA",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_AEC_BE,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[21],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				11,
			},
			.name          = {
				"AEC_BE",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_LTM_STATS,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[22],
			.num_mid       = 2,
			.num_wm        = 1,
			.line_based    = 1,
			.wm_idx        = {
				26,
			},
			.name          = {
				"LTM",
			},
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER3_VFE_OUT_STATS_GTM_BHIST,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[23],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				17,
			},
			.name          = {
				"GTM_BHIST",
			},
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER3_VFE_OUT_STATS_ALSC,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.mid           = vfe880_out_port_mid[24],
			.num_mid       = 1,
			.num_wm        = 1,
			.wm_idx        = {
				27,
			},
			.name          = {
				"STATS_ALSC",
			},
		},
	},

	.num_cons_err = 29,
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
			.error_description = "Pack 32 BPP format Not Supported"
		},
		{
			.bitmask = BIT(7),
			.error_description = "Pack 64 BPP format Not Supported"
		},
		{
			.bitmask = BIT(8),
			.error_description = "Pack MIPI 20 format Not Supported"
		},
		{
			.bitmask = BIT(9),
			.error_description = "Pack MIPI 14 format Not Supported"
		},
		{
			.bitmask = BIT(10),
			.error_description = "Pack MIPI 12 format Not Supported"
		},
		{
			.bitmask = BIT(11),
			.error_description = "Pack MIPI 10 format Not Supported"
		},
		{
			.bitmask = BIT(12),
			.error_description = "Pack 128 BPP format Not Supported"
		},
		{
			.bitmask = BIT(13),
			.error_description = "UBWC NV12 format Not Supported"
		},
		{
			.bitmask = BIT(14),
			.error_description = "UBWC NV12 4R format Not Supported"
		},
		{
			.bitmask = BIT(15),
			.error_description = "UBWC TP10 format Not Supported"
		},
		{
			.bitmask = BIT(16),
			.error_description = "Frame based Mode Not Supported"
		},
		{
			.bitmask = BIT(17),
			.error_description = "Index based Mode Not Supported"
		},
		{
			.bitmask = BIT(18),
			.error_description = "FIFO image addr unalign"
		},
		{
			.bitmask = BIT(19),
			.error_description = "FIFO ubwc addr unalign"
		},
		{
			.bitmask = BIT(20),
			.error_description = "FIFO framehdr addr unalign"
		},
		{
			.bitmask = BIT(21),
			.error_description = "Image address unalign"
		},
		{
			.bitmask = BIT(22),
			.error_description = "UBWC address unalign"
		},
		{
			.bitmask = BIT(23),
			.error_description = "Frame Header address unalign"
		},
		{
			.bitmask = BIT(24),
			.error_description = "Stride unalign"
		},
		{
			.bitmask = BIT(25),
			.error_description = "X Initialization unalign"
		},
		{
			.bitmask = BIT(26),
			.error_description = "Image Width unalign"
		},
		{
			.bitmask = BIT(27),
			.error_description = "Image Height unalign"
		},
		{
			.bitmask = BIT(28),
			.error_description = "Meta Stride unalign"
		},
	},
	.num_bus_errors_0      = ARRAY_SIZE(vfe880_bus_irq_err_desc_0),
	.num_bus_errors_1      = ARRAY_SIZE(vfe880_bus_irq_err_desc_1),
	.bus_err_desc_0        = vfe880_bus_irq_err_desc_0,
	.bus_err_desc_1        = vfe880_bus_irq_err_desc_1,
	.num_comp_grp          = 16,
	.support_consumed_addr = true,
	.comp_done_mask = {
		BIT(0), BIT(1), BIT(2), BIT(3), BIT(4),
		BIT(5), BIT(6), BIT(7), BIT(8),
		BIT(9), BIT(10), BIT(13),
		BIT(14), BIT(15), BIT(16), BIT(11),
	},
	.top_irq_shift         = 0,
	.max_out_res           = CAM_ISP_IFE_OUT_RES_BASE + 37,
	.pack_align_shift      = 5,
	.max_bw_counter_limit  = 0xFF,
};

static struct cam_vfe_irq_hw_info vfe880_irq_hw_info = {
	.reset_mask    = 0,
	.supported_irq = CAM_VFE_HW_IRQ_CAP_EXT_CSID,
	.top_irq_reg   = &vfe880_top_irq_reg_info,
};

static struct cam_vfe_hw_info cam_vfe880_hw_info = {
	.irq_hw_info                  = &vfe880_irq_hw_info,

	.bus_version                   = CAM_VFE_BUS_VER_3_0,
	.bus_hw_info                   = &vfe880_bus_hw_info,

	.top_version                   = CAM_VFE_TOP_VER_4_0,
	.top_hw_info                   = &vfe880_top_hw_info,
};

#endif /* _CAM_VFE880_H_ */
