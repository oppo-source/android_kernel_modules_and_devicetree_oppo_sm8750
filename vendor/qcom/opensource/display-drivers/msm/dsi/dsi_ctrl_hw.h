/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DSI_CTRL_HW_H_
#define _DSI_CTRL_HW_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>

#include "dsi_defs.h"
#include "dsi_hw.h"

#ifdef OPLUS_FEATURE_DISPLAY
#include "oplus_debug.h"
#endif /* OPLUS_FEATURE_DISPLAY */

#define DSI_CTRL_HW_DBG(c, fmt, ...)	DRM_DEV_DEBUG(NULL, "[msm-dsi-debug]: DSI_%d: "\
		fmt, c ? c->index : -1,	##__VA_ARGS__)
#define DSI_CTRL_HW_ERR(c, fmt, ...)	DRM_DEV_ERROR(NULL, "[msm-dsi-error]: DSI_%d: "\
		fmt, c ? c->index : -1,	##__VA_ARGS__)
#define DSI_CTRL_HW_INFO(c, fmt, ...)	DRM_DEV_INFO(NULL, "[msm-dsi-info]: DSI_%d: "\
		fmt, c ? c->index : -1,	##__VA_ARGS__)

#define DSI_MMSS_MISC_R32(dsi_ctrl_hw, off) DSI_GEN_R32((dsi_ctrl_hw)->mmss_misc_base, off)
#define DSI_MMSS_MISC_W32(dsi_ctrl_hw, off, val) \
	DSI_GEN_W32_DEBUG((dsi_ctrl_hw)->mmss_misc_base, (dsi_ctrl_hw)->index, off, val)

#define DSI_DISP_CC_R32(dsi_ctrl_hw, off) DSI_GEN_R32((dsi_ctrl_hw)->disp_cc_base, off)
#define DSI_DISP_CC_W32(dsi_ctrl_hw, off, val) \
	DSI_GEN_W32_DEBUG((dsi_ctrl_hw)->disp_cc_base, (dsi_ctrl_hw)->index, off, val)

#define DSI_MDP_INTF_R32(dsi_ctrl_hw, off) DSI_GEN_R32((dsi_ctrl_hw)->mdp_intf_base, off)
#define DSI_MDP_INTF_W32(dsi_ctrl_hw, off, val) \
	DSI_GEN_W32_DEBUG((dsi_ctrl_hw)->mdp_intf_base, (dsi_ctrl_hw)->index, off, val)

#ifdef OPLUS_FEATURE_DISPLAY
#undef DSI_CTRL_HW_ERR
#ifdef OPLUS_TRACKPOINT_REPORT
#include <soc/oplus/oplus_trackpoint_report.h>
#define DSI_CTRL_HW_ERR(c, fmt, ...) \
	do { \
		DRM_DEV_ERROR(NULL, "[msm-dsi-error]: DSI_%d: "\
			fmt, c ? c->index : -1,	##__VA_ARGS__); \
		display_exception_trackpoint_report("DisplayDriverID@@%d$$" pr_fmt(fmt), \
			OPLUS_DISP_Q_ERROR_CTRL_HW, ##__VA_ARGS__); \
	} while(0)
#else
#define DSI_CTRL_HW_ERR(c, fmt, ...) \
	do { \
		DRM_DEV_ERROR(NULL, "[msm-dsi-error]: DSI_%d: "\
			fmt, c ? c->index : -1,	##__VA_ARGS__); \
	} while(0)
#endif /* OPLUS_TRACKPOINT_REPORT */
#endif /* OPLUS_FEATURE_DISPLAY */

/**
 * Modifier flag for command transmission. If this flag is set, command
 * information is programmed to hardware and transmission is not triggered.
 * Caller should call the trigger_command_dma() to start the transmission. This
 * flag is valed for kickoff_command() and kickoff_fifo_command() operations.
 */
#define DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER            0x1

/**
 * enum dsi_ctrl_tpg_pattern - type of TPG pattern
 * @DSI_CTRL_TPG_COUNTER:
 * @DSI_CTRL_TPG_FIXED:
 * @DSI_CTRL_TPG_COLOR_RAMP_64L_64P:
 * @DSI_CTRL_TPG_COLOR_RAMP_64L_256P:
 * @DSI_CTRL_TPG_GRAYSCALE_RAMP:
 * @DSI_CTRL_TPG_COLOR_SQUARE:
 * @DSI_CTRL_TPG_CHECKERED_RECTANGLE:
 * @DSI_CTRL_TPG_BASIC_COLOR_CHANGING:
 */
enum dsi_ctrl_tpg_pattern {
	DSI_CTRL_TPG_COUNTER = 0,
	DSI_CTRL_TPG_FIXED,
	DSI_CTRL_TPG_COLOR_RAMP_64L_64P,
	DSI_CTRL_TPG_COLOR_RAMP_64L_256P,
	DSI_CTRL_TPG_BLACK_WHITE_VERTICAL_LINES,
	DSI_CTRL_TPG_GRAYSCALE_RAMP,
	DSI_CTRL_TPG_COLOR_SQUARE,
	DSI_CTRL_TPG_CHECKERED_RECTANGLE,
	DSI_CTRL_TPG_BASIC_COLOR_CHANGING
};

/**
 * enum dsi_ctrl_version - version of the dsi host controller
 * @DSI_CTRL_VERSION_UNKNOWN: Unknown controller version
 * @DSI_CTRL_VERSION_2_2:     DSI host v2.2 controller
 * @DSI_CTRL_VERSION_2_3:     DSI host v2.3 controller
 * @DSI_CTRL_VERSION_2_4:     DSI host v2.4 controller
 * @DSI_CTRL_VERSION_2_5:     DSI host v2.5 controller
 * @DSI_CTRL_VERSION_2_6:     DSI host v2.6 controller
 * @DSI_CTRL_VERSION_2_7:     DSI host v2.7 controller
 * @DSI_CTRL_VERSION_2_8:     DSI host v2.8 controller
 * @DSI_CTRL_VERSION_2_9:     DSI host v2.9 controller
 * @DSI_CTRL_VERSION_MAX:     max version
 */
enum dsi_ctrl_version {
	DSI_CTRL_VERSION_UNKNOWN,
	DSI_CTRL_VERSION_2_2,
	DSI_CTRL_VERSION_2_3,
	DSI_CTRL_VERSION_2_4,
	DSI_CTRL_VERSION_2_5,
	DSI_CTRL_VERSION_2_6,
	DSI_CTRL_VERSION_2_7,
	DSI_CTRL_VERSION_2_8,
	DSI_CTRL_VERSION_2_9,
	DSI_CTRL_VERSION_MAX
};

/**
 * enum dsi_ctrl_hw_features - features supported by dsi host controller
 * @DSI_CTRL_VIDEO_TPG:               Test pattern support for video mode.
 * @DSI_CTRL_CMD_TPG:                 Test pattern support for command mode.
 * @DSI_CTRL_VARIABLE_REFRESH_RATE:   variable panel timing
 * @DSI_CTRL_DYNAMIC_REFRESH:         variable pixel clock rate
 * @DSI_CTRL_NULL_PACKET_INSERTION:   NULL packet insertion
 * @DSI_CTRL_DESKEW_CALIB:            Deskew calibration support
 * @DSI_CTRL_DPHY:                    Controller support for DPHY
 * @DSI_CTRL_CPHY:                    Controller support for CPHY
 * @DSI_CTRL_MAX_FEATURES:
 */
enum dsi_ctrl_hw_features {
	DSI_CTRL_VIDEO_TPG,
	DSI_CTRL_CMD_TPG,
	DSI_CTRL_VARIABLE_REFRESH_RATE,
	DSI_CTRL_DYNAMIC_REFRESH,
	DSI_CTRL_NULL_PACKET_INSERTION,
	DSI_CTRL_DESKEW_CALIB,
	DSI_CTRL_DPHY,
	DSI_CTRL_CPHY,
	DSI_CTRL_MAX_FEATURES
};

/**
 * enum dsi_test_pattern - test pattern type
 * @DSI_TEST_PATTERN_FIXED:     Test pattern is fixed, based on init value.
 * @DSI_TEST_PATTERN_INC:       Incremental test pattern, base on init value.
 * @DSI_TEST_PATTERN_POLY:      Pattern generated from polynomial and init val.
 * @DSI_TEST_PATTERN_GENERAL:   MDSS general test pattern.
 * @DSI_TEST_PATTERN_MAX:
 */
enum dsi_test_pattern {
	DSI_TEST_PATTERN_FIXED = 0,
	DSI_TEST_PATTERN_INC,
	DSI_TEST_PATTERN_POLY,
	DSI_TEST_PATTERN_GENERAL,
	DSI_TEST_PATTERN_MAX
};

/**
 * enum dsi_status_int_index - index of interrupts generated by DSI controller
 * @DSI_SINT_CMD_MODE_DMA_DONE:        Command mode DMA packets are sent out.
 * @DSI_SINT_CMD_STREAM0_FRAME_DONE:   A frame of cmd mode stream0 is sent out.
 * @DSI_SINT_CMD_STREAM1_FRAME_DONE:   A frame of cmd mode stream1 is sent out.
 * @DSI_SINT_CMD_STREAM2_FRAME_DONE:   A frame of cmd mode stream2 is sent out.
 * @DSI_SINT_VIDEO_MODE_FRAME_DONE:    A frame of video mode stream is sent out.
 * @DSI_SINT_BTA_DONE:                 A BTA is completed.
 * @DSI_SINT_CMD_FRAME_DONE:           A frame of selected cmd mode stream is
 *                                     sent out by MDP.
 * @DSI_SINT_DYN_REFRESH_DONE:         The dynamic refresh operation completed.
 * @DSI_SINT_DESKEW_DONE:              The deskew calibration operation done.
 * @DSI_SINT_DYN_BLANK_DMA_DONE:       The dynamic blankin DMA operation has
 *                                     completed.
 * @DSI_SINT_ERROR:                    DSI error has happened.
 */
enum dsi_status_int_index {
	DSI_SINT_CMD_MODE_DMA_DONE = 0,
	DSI_SINT_CMD_STREAM0_FRAME_DONE = 1,
	DSI_SINT_CMD_STREAM1_FRAME_DONE = 2,
	DSI_SINT_CMD_STREAM2_FRAME_DONE = 3,
	DSI_SINT_VIDEO_MODE_FRAME_DONE = 4,
	DSI_SINT_BTA_DONE = 5,
	DSI_SINT_CMD_FRAME_DONE = 6,
	DSI_SINT_DYN_REFRESH_DONE = 7,
	DSI_SINT_DESKEW_DONE = 8,
	DSI_SINT_DYN_BLANK_DMA_DONE = 9,
	DSI_SINT_ERROR = 10,

	DSI_STATUS_INTERRUPT_COUNT
};

/**
 * enum dsi_status_int_type - status interrupts generated by DSI controller
 * @DSI_CMD_MODE_DMA_DONE:        Command mode DMA packets are sent out.
 * @DSI_CMD_STREAM0_FRAME_DONE:   A frame of command mode stream0 is sent out.
 * @DSI_CMD_STREAM1_FRAME_DONE:   A frame of command mode stream1 is sent out.
 * @DSI_CMD_STREAM2_FRAME_DONE:   A frame of command mode stream2 is sent out.
 * @DSI_VIDEO_MODE_FRAME_DONE:    A frame of video mode stream is sent out.
 * @DSI_BTA_DONE:                 A BTA is completed.
 * @DSI_CMD_FRAME_DONE:           A frame of selected command mode stream is
 *                                sent out by MDP.
 * @DSI_DYN_REFRESH_DONE:         The dynamic refresh operation has completed.
 * @DSI_DESKEW_DONE:              The deskew calibration operation has completed
 * @DSI_DYN_BLANK_DMA_DONE:       The dynamic blankin DMA operation has
 *                                completed.
 * @DSI_ERROR:                    DSI error has happened.
 */
enum dsi_status_int_type {
	DSI_CMD_MODE_DMA_DONE = BIT(DSI_SINT_CMD_MODE_DMA_DONE),
	DSI_CMD_STREAM0_FRAME_DONE = BIT(DSI_SINT_CMD_STREAM0_FRAME_DONE),
	DSI_CMD_STREAM1_FRAME_DONE = BIT(DSI_SINT_CMD_STREAM1_FRAME_DONE),
	DSI_CMD_STREAM2_FRAME_DONE = BIT(DSI_SINT_CMD_STREAM2_FRAME_DONE),
	DSI_VIDEO_MODE_FRAME_DONE = BIT(DSI_SINT_VIDEO_MODE_FRAME_DONE),
	DSI_BTA_DONE = BIT(DSI_SINT_BTA_DONE),
	DSI_CMD_FRAME_DONE = BIT(DSI_SINT_CMD_FRAME_DONE),
	DSI_DYN_REFRESH_DONE = BIT(DSI_SINT_DYN_REFRESH_DONE),
	DSI_DESKEW_DONE = BIT(DSI_SINT_DESKEW_DONE),
	DSI_DYN_BLANK_DMA_DONE = BIT(DSI_SINT_DYN_BLANK_DMA_DONE),
	DSI_ERROR = BIT(DSI_SINT_ERROR)
};

/**
 * enum dsi_error_int_index - index of error interrupts from DSI controller
 * @DSI_EINT_RDBK_SINGLE_ECC_ERR:        Single bit ECC error in read packet.
 * @DSI_EINT_RDBK_MULTI_ECC_ERR:         Multi bit ECC error in read packet.
 * @DSI_EINT_RDBK_CRC_ERR:               CRC error in read packet.
 * @DSI_EINT_RDBK_INCOMPLETE_PKT:        Incomplete read packet.
 * @DSI_EINT_PERIPH_ERROR_PKT:           Error packet returned from peripheral,
 * @DSI_EINT_LP_RX_TIMEOUT:              Low power reverse transmission timeout.
 * @DSI_EINT_HS_TX_TIMEOUT:              High speed fwd transmission timeout.
 * @DSI_EINT_BTA_TIMEOUT:                BTA timeout.
 * @DSI_EINT_PLL_UNLOCK:                 PLL has unlocked.
 * @DSI_EINT_DLN0_ESC_ENTRY_ERR:         Incorrect LP Rx escape entry.
 * @DSI_EINT_DLN0_ESC_SYNC_ERR:          LP Rx data is not byte aligned.
 * @DSI_EINT_DLN0_LP_CONTROL_ERR:        Incorrect LP Rx state sequence.
 * @DSI_EINT_PANEL_SPECIFIC_ERR:         DSI Protocol violation error.
 * @DSI_EINT_INTERLEAVE_OP_CONTENTION:   Interleave operation contention.
 * @DSI_EINT_CMD_DMA_FIFO_UNDERFLOW:     Command mode DMA FIFO underflow.
 * @DSI_EINT_CMD_MDP_FIFO_UNDERFLOW:     Command MDP FIFO underflow (failed to
 *                                       receive one complete line from MDP).
 * @DSI_EINT_DLN0_HS_FIFO_OVERFLOW:      High speed FIFO data lane 0 overflows.
 * @DSI_EINT_DLN1_HS_FIFO_OVERFLOW:      High speed FIFO data lane 1 overflows.
 * @DSI_EINT_DLN2_HS_FIFO_OVERFLOW:      High speed FIFO data lane 2 overflows.
 * @DSI_EINT_DLN3_HS_FIFO_OVERFLOW:      High speed FIFO data lane 3 overflows.
 * @DSI_EINT_DLN0_HS_FIFO_UNDERFLOW:     High speed FIFO data lane 0 underflows.
 * @DSI_EINT_DLN1_HS_FIFO_UNDERFLOW:     High speed FIFO data lane 1 underflows.
 * @DSI_EINT_DLN2_HS_FIFO_UNDERFLOW:     High speed FIFO data lane 2 underflows.
 * @DSI_EINT_DLN3_HS_FIFO_UNDERFLOW:     High speed FIFO data lane 3 undeflows.
 * @DSI_EINT_DLN0_LP0_CONTENTION:        PHY level contention while lane 0 low.
 * @DSI_EINT_DLN1_LP0_CONTENTION:        PHY level contention while lane 1 low.
 * @DSI_EINT_DLN2_LP0_CONTENTION:        PHY level contention while lane 2 low.
 * @DSI_EINT_DLN3_LP0_CONTENTION:        PHY level contention while lane 3 low.
 * @DSI_EINT_DLN0_LP1_CONTENTION:        PHY level contention while lane 0 high.
 * @DSI_EINT_DLN1_LP1_CONTENTION:        PHY level contention while lane 1 high.
 * @DSI_EINT_DLN2_LP1_CONTENTION:        PHY level contention while lane 2 high.
 * @DSI_EINT_DLN3_LP1_CONTENTION:        PHY level contention while lane 3 high.
 */
enum dsi_error_int_index {
	DSI_EINT_RDBK_SINGLE_ECC_ERR = 0,
	DSI_EINT_RDBK_MULTI_ECC_ERR = 1,
	DSI_EINT_RDBK_CRC_ERR = 2,
	DSI_EINT_RDBK_INCOMPLETE_PKT = 3,
	DSI_EINT_PERIPH_ERROR_PKT = 4,
	DSI_EINT_LP_RX_TIMEOUT = 5,
	DSI_EINT_HS_TX_TIMEOUT = 6,
	DSI_EINT_BTA_TIMEOUT = 7,
	DSI_EINT_PLL_UNLOCK = 8,
	DSI_EINT_DLN0_ESC_ENTRY_ERR = 9,
	DSI_EINT_DLN0_ESC_SYNC_ERR = 10,
	DSI_EINT_DLN0_LP_CONTROL_ERR = 11,
	DSI_EINT_PANEL_SPECIFIC_ERR = 12,
	DSI_EINT_INTERLEAVE_OP_CONTENTION = 13,
	DSI_EINT_CMD_DMA_FIFO_UNDERFLOW = 14,
	DSI_EINT_CMD_MDP_FIFO_UNDERFLOW = 15,
	DSI_EINT_DLN0_HS_FIFO_OVERFLOW = 16,
	DSI_EINT_DLN1_HS_FIFO_OVERFLOW = 17,
	DSI_EINT_DLN2_HS_FIFO_OVERFLOW = 18,
	DSI_EINT_DLN3_HS_FIFO_OVERFLOW = 19,
	DSI_EINT_DLN0_HS_FIFO_UNDERFLOW = 20,
	DSI_EINT_DLN1_HS_FIFO_UNDERFLOW = 21,
	DSI_EINT_DLN2_HS_FIFO_UNDERFLOW = 22,
	DSI_EINT_DLN3_HS_FIFO_UNDERFLOW = 23,
	DSI_EINT_DLN0_LP0_CONTENTION = 24,
	DSI_EINT_DLN1_LP0_CONTENTION = 25,
	DSI_EINT_DLN2_LP0_CONTENTION = 26,
	DSI_EINT_DLN3_LP0_CONTENTION = 27,
	DSI_EINT_DLN0_LP1_CONTENTION = 28,
	DSI_EINT_DLN1_LP1_CONTENTION = 29,
	DSI_EINT_DLN2_LP1_CONTENTION = 30,
	DSI_EINT_DLN3_LP1_CONTENTION = 31,

	DSI_ERROR_INTERRUPT_COUNT
};

/**
 * enum dsi_error_int_type - error interrupts generated by DSI controller
 * @DSI_RDBK_SINGLE_ECC_ERR:        Single bit ECC error in read packet.
 * @DSI_RDBK_MULTI_ECC_ERR:         Multi bit ECC error in read packet.
 * @DSI_RDBK_CRC_ERR:               CRC error in read packet.
 * @DSI_RDBK_INCOMPLETE_PKT:        Incomplete read packet.
 * @DSI_PERIPH_ERROR_PKT:           Error packet returned from peripheral,
 * @DSI_LP_RX_TIMEOUT:              Low power reverse transmission timeout.
 * @DSI_HS_TX_TIMEOUT:              High speed forward transmission timeout.
 * @DSI_BTA_TIMEOUT:                BTA timeout.
 * @DSI_PLL_UNLOCK:                 PLL has unlocked.
 * @DSI_DLN0_ESC_ENTRY_ERR:         Incorrect LP Rx escape entry.
 * @DSI_DLN0_ESC_SYNC_ERR:          LP Rx data is not byte aligned.
 * @DSI_DLN0_LP_CONTROL_ERR:        Incorrect LP Rx state sequence.
 * @DSI_PANEL_SPECIFIC_ERR:         DSI Protocol violation.
 * @DSI_INTERLEAVE_OP_CONTENTION:   Interleave operation contention.
 * @DSI_CMD_DMA_FIFO_UNDERFLOW:     Command mode DMA FIFO underflow.
 * @DSI_CMD_MDP_FIFO_UNDERFLOW:     Command MDP FIFO underflow (failed to
 *                                  receive one complete line from MDP).
 * @DSI_DLN0_HS_FIFO_OVERFLOW:      High speed FIFO for data lane 0 overflows.
 * @DSI_DLN1_HS_FIFO_OVERFLOW:      High speed FIFO for data lane 1 overflows.
 * @DSI_DLN2_HS_FIFO_OVERFLOW:      High speed FIFO for data lane 2 overflows.
 * @DSI_DLN3_HS_FIFO_OVERFLOW:      High speed FIFO for data lane 3 overflows.
 * @DSI_DLN0_HS_FIFO_UNDERFLOW:     High speed FIFO for data lane 0 underflows.
 * @DSI_DLN1_HS_FIFO_UNDERFLOW:     High speed FIFO for data lane 1 underflows.
 * @DSI_DLN2_HS_FIFO_UNDERFLOW:     High speed FIFO for data lane 2 underflows.
 * @DSI_DLN3_HS_FIFO_UNDERFLOW:     High speed FIFO for data lane 3 undeflows.
 * @DSI_DLN0_LP0_CONTENTION:        PHY level contention while lane 0 is low.
 * @DSI_DLN1_LP0_CONTENTION:        PHY level contention while lane 1 is low.
 * @DSI_DLN2_LP0_CONTENTION:        PHY level contention while lane 2 is low.
 * @DSI_DLN3_LP0_CONTENTION:        PHY level contention while lane 3 is low.
 * @DSI_DLN0_LP1_CONTENTION:        PHY level contention while lane 0 is high.
 * @DSI_DLN1_LP1_CONTENTION:        PHY level contention while lane 1 is high.
 * @DSI_DLN2_LP1_CONTENTION:        PHY level contention while lane 2 is high.
 * @DSI_DLN3_LP1_CONTENTION:        PHY level contention while lane 3 is high.
 */
enum dsi_error_int_type {
	DSI_RDBK_SINGLE_ECC_ERR = BIT(DSI_EINT_RDBK_SINGLE_ECC_ERR),
	DSI_RDBK_MULTI_ECC_ERR = BIT(DSI_EINT_RDBK_MULTI_ECC_ERR),
	DSI_RDBK_CRC_ERR = BIT(DSI_EINT_RDBK_CRC_ERR),
	DSI_RDBK_INCOMPLETE_PKT = BIT(DSI_EINT_RDBK_INCOMPLETE_PKT),
	DSI_PERIPH_ERROR_PKT = BIT(DSI_EINT_PERIPH_ERROR_PKT),
	DSI_LP_RX_TIMEOUT = BIT(DSI_EINT_LP_RX_TIMEOUT),
	DSI_HS_TX_TIMEOUT = BIT(DSI_EINT_HS_TX_TIMEOUT),
	DSI_BTA_TIMEOUT = BIT(DSI_EINT_BTA_TIMEOUT),
	DSI_PLL_UNLOCK = BIT(DSI_EINT_PLL_UNLOCK),
	DSI_DLN0_ESC_ENTRY_ERR = BIT(DSI_EINT_DLN0_ESC_ENTRY_ERR),
	DSI_DLN0_ESC_SYNC_ERR = BIT(DSI_EINT_DLN0_ESC_SYNC_ERR),
	DSI_DLN0_LP_CONTROL_ERR = BIT(DSI_EINT_DLN0_LP_CONTROL_ERR),
	DSI_PANEL_SPECIFIC_ERR = BIT(DSI_EINT_PANEL_SPECIFIC_ERR),
	DSI_INTERLEAVE_OP_CONTENTION = BIT(DSI_EINT_INTERLEAVE_OP_CONTENTION),
	DSI_CMD_DMA_FIFO_UNDERFLOW = BIT(DSI_EINT_CMD_DMA_FIFO_UNDERFLOW),
	DSI_CMD_MDP_FIFO_UNDERFLOW = BIT(DSI_EINT_CMD_MDP_FIFO_UNDERFLOW),
	DSI_DLN0_HS_FIFO_OVERFLOW = BIT(DSI_EINT_DLN0_HS_FIFO_OVERFLOW),
	DSI_DLN1_HS_FIFO_OVERFLOW = BIT(DSI_EINT_DLN1_HS_FIFO_OVERFLOW),
	DSI_DLN2_HS_FIFO_OVERFLOW = BIT(DSI_EINT_DLN2_HS_FIFO_OVERFLOW),
	DSI_DLN3_HS_FIFO_OVERFLOW = BIT(DSI_EINT_DLN3_HS_FIFO_OVERFLOW),
	DSI_DLN0_HS_FIFO_UNDERFLOW = BIT(DSI_EINT_DLN0_HS_FIFO_UNDERFLOW),
	DSI_DLN1_HS_FIFO_UNDERFLOW = BIT(DSI_EINT_DLN1_HS_FIFO_UNDERFLOW),
	DSI_DLN2_HS_FIFO_UNDERFLOW = BIT(DSI_EINT_DLN2_HS_FIFO_UNDERFLOW),
	DSI_DLN3_HS_FIFO_UNDERFLOW = BIT(DSI_EINT_DLN3_HS_FIFO_UNDERFLOW),
	DSI_DLN0_LP0_CONTENTION = BIT(DSI_EINT_DLN0_LP0_CONTENTION),
	DSI_DLN1_LP0_CONTENTION = BIT(DSI_EINT_DLN1_LP0_CONTENTION),
	DSI_DLN2_LP0_CONTENTION = BIT(DSI_EINT_DLN2_LP0_CONTENTION),
	DSI_DLN3_LP0_CONTENTION = BIT(DSI_EINT_DLN3_LP0_CONTENTION),
	DSI_DLN0_LP1_CONTENTION = BIT(DSI_EINT_DLN0_LP1_CONTENTION),
	DSI_DLN1_LP1_CONTENTION = BIT(DSI_EINT_DLN1_LP1_CONTENTION),
	DSI_DLN2_LP1_CONTENTION = BIT(DSI_EINT_DLN2_LP1_CONTENTION),
	DSI_DLN3_LP1_CONTENTION = BIT(DSI_EINT_DLN3_LP1_CONTENTION),
};

/**
 * struct dsi_ctrl_cmd_dma_info - command buffer information
 * @offset:        IOMMU VA for command buffer address.
 * @length:        Length of the command buffer.
 * @datatype:      Datatype of cmd.
 * @en_broadcast:  Enable broadcast mode if set to true.
 * @is_master:     Is master in broadcast mode.
 * @use_lpm:       Use low power mode for command transmission.
 */
struct dsi_ctrl_cmd_dma_info {
	u32 offset;
	u32 length;
	u8  datatype;
	bool en_broadcast;
	bool is_master;
	bool use_lpm;
};

/**
 * struct dsi_ctrl_cmd_dma_fifo_info - command payload tp be sent using FIFO
 * @command:        VA for command buffer.
 * @size:           Size of the command buffer.
 * @en_broadcast:   Enable broadcast mode if set to true.
 * @is_master:      Is master in broadcast mode.
 * @use_lpm:        Use low power mode for command transmission.
 */
struct dsi_ctrl_cmd_dma_fifo_info {
	u32 *command;
	u32 size;
	bool en_broadcast;
	bool is_master;
	bool use_lpm;
};

struct dsi_ctrl_hw;

struct ctrl_ulps_config_ops {
	/**
	 * ulps_request() - request ulps entry for specified lanes
	 * @ctrl:          Pointer to the controller host hardware.
	 * @lanes:         ORed list of lanes (enum dsi_data_lanes) which need
	 *                 to enter ULPS.
	 *
	 * Caller should check if lanes are in ULPS mode by calling
	 * get_lanes_in_ulps() operation.
	 */
	void (*ulps_request)(struct dsi_ctrl_hw *ctrl, u32 lanes);

	/**
	 * ulps_exit() - exit ULPS on specified lanes
	 * @ctrl:          Pointer to the controller host hardware.
	 * @lanes:         ORed list of lanes (enum dsi_data_lanes) which need
	 *                 to exit ULPS.
	 *
	 * Caller should check if lanes are in active mode by calling
	 * get_lanes_in_ulps() operation.
	 */
	void (*ulps_exit)(struct dsi_ctrl_hw *ctrl, u32 lanes);

	/**
	 * get_lanes_in_ulps() - returns the list of lanes in ULPS mode
	 * @ctrl:          Pointer to the controller host hardware.
	 *
	 * Returns an ORed list of lanes (enum dsi_data_lanes) that are in ULPS
	 * state. If 0 is returned, all the lanes are active.
	 *
	 * Return: List of lanes in ULPS state.
	 */
	u32 (*get_lanes_in_ulps)(struct dsi_ctrl_hw *ctrl);
};

/**
 * struct dsi_ctrl_hw_ops - operations supported by dsi host hardware
 */
struct dsi_ctrl_hw_ops {

	/**
	 * host_setup() - Setup DSI host configuration
	 * @ctrl:          Pointer to controller host hardware.
	 * @config:        Configuration for DSI host controller
	 */
	void (*host_setup)(struct dsi_ctrl_hw *ctrl,
			   struct dsi_host_common_cfg *config);

	/**
	 * video_engine_en() - enable DSI video engine
	 * @ctrl:          Pointer to controller host hardware.
	 * @on:            Enable/disabel video engine.
	 */
	void (*video_engine_en)(struct dsi_ctrl_hw *ctrl, bool on);

	/**
	 * setup_avr() - set the AVR_SUPPORT_ENABLE bit in DSI_VIDEO_MODE_CTRL
	 * @ctrl:	   Pointer to controller host hardware.
	 * @enable:	   Controls whether this bit is set or cleared
	 */
	void (*setup_avr)(struct dsi_ctrl_hw *ctrl, bool enable);

	/**
	 * video_engine_setup() - Setup dsi host controller for video mode
	 * @ctrl:          Pointer to controller host hardware.
	 * @common_cfg:    Common configuration parameters.
	 * @cfg:           Video mode configuration.
	 *
	 * Set up DSI video engine with a specific configuration. Controller and
	 * video engine are not enabled as part of this function.
	 */
	void (*video_engine_setup)(struct dsi_ctrl_hw *ctrl,
				   struct dsi_host_common_cfg *common_cfg,
				   struct dsi_video_engine_cfg *cfg);

	/**
	 * set_video_timing() - set up the timing for video frame
	 * @ctrl:          Pointer to controller host hardware.
	 * @mode:          Video mode information.
	 *
	 * Set up the video timing parameters for the DSI video mode operation.
	 */
	void (*set_video_timing)(struct dsi_ctrl_hw *ctrl,
				 struct dsi_mode_info *mode);

	/**
	 * cmd_engine_setup() - setup dsi host controller for command mode
	 * @ctrl:          Pointer to the controller host hardware.
	 * @common_cfg:    Common configuration parameters.
	 * @cfg:           Command mode configuration.
	 *
	 * Setup DSI CMD engine with a specific configuration. Controller and
	 * command engine are not enabled as part of this function.
	 */
	void (*cmd_engine_setup)(struct dsi_ctrl_hw *ctrl,
				 struct dsi_host_common_cfg *common_cfg,
				 struct dsi_cmd_engine_cfg *cfg);

	/**
	 * setup_cmd_stream() - set up parameters for command pixel streams
	 * @ctrl:              Pointer to controller host hardware.
	 * @mode:              Pointer to mode information.
	 * @cfg:               DSI host configuration that is common to both
	 *                     video and command modes.
	 * @vc_id:             stream_id.
	 *
	 * Setup parameters for command mode pixel stream size.
	 */
	void (*setup_cmd_stream)(struct dsi_ctrl_hw *ctrl,
				 struct dsi_mode_info *mode,
				 struct dsi_host_common_cfg *cfg,
				 u32 vc_id,
				 struct dsi_rect *roi);

	/**
	 * ctrl_en() - enable DSI controller engine
	 * @ctrl:          Pointer to the controller host hardware.
	 * @on:            turn on/off the DSI controller engine.
	 */
	void (*ctrl_en)(struct dsi_ctrl_hw *ctrl, bool on);

	/**
	 * cmd_engine_en() - enable DSI controller command engine
	 * @ctrl:          Pointer to the controller host hardware.
	 * @on:            Turn on/off the DSI command engine.
	 */
	void (*cmd_engine_en)(struct dsi_ctrl_hw *ctrl, bool on);

	/**
	 * phy_sw_reset() - perform a soft reset on the PHY.
	 * @ctrl:        Pointer to the controller host hardware.
	 */
	void (*phy_sw_reset)(struct dsi_ctrl_hw *ctrl);

	/**
	 * config_clk_gating() - enable/disable DSI PHY clk gating
	 * @ctrl:          Pointer to the controller host hardware.
	 * @enable:        enable/disable DSI PHY clock gating.
	 * @clk_selection:        clock to enable/disable clock gating.
	 */
	void (*config_clk_gating)(struct dsi_ctrl_hw *ctrl, bool enable,
			enum dsi_clk_gate_type clk_selection);

	/**
	 * soft_reset() - perform a soft reset on DSI controller
	 * @ctrl:          Pointer to the controller host hardware.
	 *
	 * The video, command and controller engines will be disabled before the
	 * reset is triggered. After, the engines will be re-enabled to the same
	 * state as before the reset.
	 *
	 * If the reset is done while MDP timing engine is turned on, the video
	 * engine should be re-enabled only during the vertical blanking time.
	 */
	void (*soft_reset)(struct dsi_ctrl_hw *ctrl);

	/**
	 * setup_lane_map() - setup mapping between logical and physical lanes
	 * @ctrl:          Pointer to the controller host hardware.
	 * @lane_map:      Structure defining the mapping between DSI logical
	 *                 lanes and physical lanes.
	 */
	void (*setup_lane_map)(struct dsi_ctrl_hw *ctrl,
			       struct dsi_lane_map *lane_map);

	/**
	 * kickoff_command() - transmits commands stored in memory
	 * @ctrl:          Pointer to the controller host hardware.
	 * @cmd:           Command information.
	 * @flags:         Modifiers for command transmission.
	 *
	 * The controller hardware is programmed with address and size of the
	 * command buffer. The transmission is kicked off if
	 * DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER flag is not set. If this flag is
	 * set, caller should make a separate call to trigger_command_dma() to
	 * transmit the command.
	 */
	void (*kickoff_command)(struct dsi_ctrl_hw *ctrl,
				struct dsi_ctrl_cmd_dma_info *cmd,
				u32 flags);

	/**
	 * kickoff_command_non_embedded_mode() - cmd in non embedded mode
	 * @ctrl:          Pointer to the controller host hardware.
	 * @cmd:           Command information.
	 * @flags:         Modifiers for command transmission.
	 *
	 * If command length is greater than DMA FIFO size of 256 bytes we use
	 * this non- embedded mode.
	 * The controller hardware is programmed with address and size of the
	 * command buffer. The transmission is kicked off if
	 * DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER flag is not set. If this flag is
	 * set, caller should make a separate call to trigger_command_dma() to
	 * transmit the command.
	 */

	void (*kickoff_command_non_embedded_mode)(struct dsi_ctrl_hw *ctrl,
				struct dsi_ctrl_cmd_dma_info *cmd,
				u32 flags);

	/**
	 * kickoff_fifo_command() - transmits a command using FIFO in dsi
	 *                          hardware.
	 * @ctrl:          Pointer to the controller host hardware.
	 * @cmd:           Command information.
	 * @flags:         Modifiers for command transmission.
	 *
	 * The controller hardware FIFO is programmed with command header and
	 * payload. The transmission is kicked off if
	 * DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER flag is not set. If this flag is
	 * set, caller should make a separate call to trigger_command_dma() to
	 * transmit the command.
	 */
	void (*kickoff_fifo_command)(struct dsi_ctrl_hw *ctrl,
				     struct dsi_ctrl_cmd_dma_fifo_info *cmd,
				     u32 flags);

	void (*reset_cmd_fifo)(struct dsi_ctrl_hw *ctrl);
	/**
	 * trigger_command_dma() - trigger transmission of command buffer.
	 * @ctrl:          Pointer to the controller host hardware.
	 *
	 * This trigger can be only used if there was a prior call to
	 * kickoff_command() of kickoff_fifo_command() with
	 * DSI_CTRL_HW_CMD_WAIT_FOR_TRIGGER flag.
	 */
	void (*trigger_command_dma)(struct dsi_ctrl_hw *ctrl);

	/**
	 * get_cmd_read_data() - get data read from the peripheral
	 * @ctrl:           Pointer to the controller host hardware.
	 * @rd_buf:         Buffer where data will be read into.
	 * @read_offset:    Offset from where to read.
	 * @rx_byte:        Number of bytes to be read.
	 * @pkt_size:        Size of response expected.
	 * @hw_read_cnt:    Actual number of bytes read by HW.
	 */
	u32 (*get_cmd_read_data)(struct dsi_ctrl_hw *ctrl,
				 u8 *rd_buf,
				 u32 read_offset,
				 u32 rx_byte,
				 u32 pkt_size,
				 u32 *hw_read_cnt);

	/**
	 * wait_for_lane_idle() - wait for DSI lanes to go to idle state
	 * @ctrl:          Pointer to the controller host hardware.
	 * @lanes:         ORed list of lanes (enum dsi_data_lanes) which need
	 *                 to be checked to be in idle state.
	 */
	int (*wait_for_lane_idle)(struct dsi_ctrl_hw *ctrl, u32 lanes);

	struct ctrl_ulps_config_ops ulps_ops;

	/**
	 * clamp_enable() - enable DSI clamps
	 * @ctrl:         Pointer to the controller host hardware.
	 * @lanes:        ORed list of lanes which need to have clamps released.
	 * @enable_ulps: ulps state.
	 */

	/**
	 * clamp_enable() - enable DSI clamps to keep PHY driving a stable link
	 * @ctrl:         Pointer to the controller host hardware.
	 * @lanes:        ORed list of lanes which need to have clamps released.
	 * @enable_ulps: TODO:??
	 */
	void (*clamp_enable)(struct dsi_ctrl_hw *ctrl,
			     u32 lanes,
			     bool enable_ulps);

	/**
	 * clamp_disable() - disable DSI clamps
	 * @ctrl:         Pointer to the controller host hardware.
	 * @lanes:        ORed list of lanes which need to have clamps released.
	 * @disable_ulps: ulps state.
	 */
	void (*clamp_disable)(struct dsi_ctrl_hw *ctrl,
			      u32 lanes,
			      bool disable_ulps);

	/**
	 * phy_reset_config() - Disable/enable propagation of  reset signal
	 *	from ahb domain to DSI PHY
	 * @ctrl:         Pointer to the controller host hardware.
	 * @enable:	True to mask the reset signal, false to unmask
	 */
	void (*phy_reset_config)(struct dsi_ctrl_hw *ctrl,
			     bool enable);

	/**
	 * get_interrupt_status() - returns the interrupt status
	 * @ctrl:          Pointer to the controller host hardware.
	 *
	 * Returns the ORed list of interrupts(enum dsi_status_int_type) that
	 * are active. This list does not include any error interrupts. Caller
	 * should call get_error_status for error interrupts.
	 *
	 * Return: List of active interrupts.
	 */
	u32 (*get_interrupt_status)(struct dsi_ctrl_hw *ctrl);

	/**
	 * clear_interrupt_status() - clears the specified interrupts
	 * @ctrl:          Pointer to the controller host hardware.
	 * @ints:          List of interrupts to be cleared.
	 */
	void (*clear_interrupt_status)(struct dsi_ctrl_hw *ctrl, u32 ints);

	/**
	 * poll_dma_status()- API to poll DMA status
	 * @ctrl:                 Pointer to the controller host hardware.
	 */
	u32 (*poll_dma_status)(struct dsi_ctrl_hw *ctrl);

	/**
	 * enable_status_interrupts() - enable the specified interrupts
	 * @ctrl:          Pointer to the controller host hardware.
	 * @ints:          List of interrupts to be enabled.
	 *
	 * Enables the specified interrupts. This list will override the
	 * previous interrupts enabled through this function. Caller has to
	 * maintain the state of the interrupts enabled. To disable all
	 * interrupts, set ints to 0.
	 */
	void (*enable_status_interrupts)(struct dsi_ctrl_hw *ctrl, u32 ints);

	/**
	 * get_error_status() - returns the error status
	 * @ctrl:          Pointer to the controller host hardware.
	 *
	 * Returns the ORed list of errors(enum dsi_error_int_type) that are
	 * active. This list does not include any status interrupts. Caller
	 * should call get_interrupt_status for status interrupts.
	 *
	 * Return: List of active error interrupts.
	 */
	u64 (*get_error_status)(struct dsi_ctrl_hw *ctrl);

	/**
	 * clear_error_status() - clears the specified errors
	 * @ctrl:          Pointer to the controller host hardware.
	 * @errors:          List of errors to be cleared.
	 */
	void (*clear_error_status)(struct dsi_ctrl_hw *ctrl, u64 errors);

	/**
	 * enable_error_interrupts() - enable the specified interrupts
	 * @ctrl:          Pointer to the controller host hardware.
	 * @errors:        List of errors to be enabled.
	 *
	 * Enables the specified interrupts. This list will override the
	 * previous interrupts enabled through this function. Caller has to
	 * maintain the state of the interrupts enabled. To disable all
	 * interrupts, set errors to 0.
	 */
	void (*enable_error_interrupts)(struct dsi_ctrl_hw *ctrl, u64 errors);

	/**
	 * video_test_pattern_setup() - setup test pattern engine for video mode
	 * @ctrl:          Pointer to the controller host hardware.
	 * @type:          Type of test pattern.
	 * @init_val:      Initial value to use for generating test pattern.
	 */
	void (*video_test_pattern_setup)(struct dsi_ctrl_hw *ctrl,
					 enum dsi_test_pattern type,
					 u32 init_val);

	/**
	 * cmd_test_pattern_setup() - setup test patttern engine for cmd mode
	 * @ctrl:          Pointer to the controller host hardware.
	 * @type:          Type of test pattern.
	 * @init_val:      Initial value to use for generating test pattern.
	 * @stream_id:     Stream Id on which packets are generated.
	 */
	void (*cmd_test_pattern_setup)(struct dsi_ctrl_hw *ctrl,
				       enum dsi_test_pattern  type,
				       u32 init_val,
				       u32 stream_id);

	/**
	 * test_pattern_enable() - enable test pattern engine
	 * @ctrl:          Pointer to the controller host hardware.
	 * @enable:        Enable/Disable test pattern engine.
	 * @pattern:       Type of TPG pattern
	 * @panel_mode:    DSI operation mode
	 */
	void (*test_pattern_enable)(struct dsi_ctrl_hw *ctrl, bool enable,
					   enum dsi_ctrl_tpg_pattern pattern,
					   enum dsi_op_mode panel_mode);

	/**
	 * clear_phy0_ln_err() - clear DSI PHY lane-0 errors
	 * @ctrl:          Pointer to the controller host hardware.
	 */
	void (*clear_phy0_ln_err)(struct dsi_ctrl_hw *ctrl);

	/**
	 * trigger_cmd_test_pattern() - trigger a command mode frame update with
	 *                              test pattern
	 * @ctrl:          Pointer to the controller host hardware.
	 * @stream_id:     Stream on which frame update is sent.
	 */
	void (*trigger_cmd_test_pattern)(struct dsi_ctrl_hw *ctrl,
					 u32 stream_id);

	ssize_t (*reg_dump_to_buffer)(struct dsi_ctrl_hw *ctrl,
				      char *buf,
				      u32 size);

	/**
	 * setup_misr() - Setup frame MISR
	 * @ctrl:         Pointer to the controller host hardware.
	 * @panel_mode:   CMD or VIDEO mode indicator
	 * @enable:       Enable/disable MISR.
	 * @frame_count:  Number of frames to accumulate MISR.
	 */
	void (*setup_misr)(struct dsi_ctrl_hw *ctrl,
			   enum dsi_op_mode panel_mode,
			   bool enable, u32 frame_count);

	/**
	 * collect_misr() - Read frame MISR
	 * @ctrl:         Pointer to the controller host hardware.
	 * @panel_mode:   CMD or VIDEO mode indicator
	 */
	u32 (*collect_misr)(struct dsi_ctrl_hw *ctrl,
			    enum dsi_op_mode panel_mode);

	/**
	 * set_timing_db() - enable/disable Timing DB register
	 * @ctrl:          Pointer to controller host hardware.
	 * @enable:        Enable/Disable flag.
	 *
	 * Enable or Disabe the Timing DB register.
	 */
	void (*set_timing_db)(struct dsi_ctrl_hw *ctrl,
				 bool enable);
	/**
	 * clear_rdbk_register() - Clear and reset read back register
	 * @ctrl:         Pointer to the controller host hardware.
	 */
	void (*clear_rdbk_register)(struct dsi_ctrl_hw *ctrl);

	/** schedule_dma_cmd() - Schdeule DMA command transfer on a
	 *                       particular blanking line.
	 * @ctrl:                Pointer to the controller host hardware.
	 * @line_no:             Blanking line number on whihch DMA command
	 *                       needs to be sent.
	 * @do_peripheral_flush: Flag for sending this command with peripheral flush.
	 */
	void (*schedule_dma_cmd)(struct dsi_ctrl_hw *ctrl, int line_no, bool do_peripheral_flush);

	/**
	 * ctrl_reset() - Reset DSI lanes to recover from DSI errors
	 * @ctrl:         Pointer to the controller host hardware.
	 * @mask:         Indicates the error type.
	 */
	int (*ctrl_reset)(struct dsi_ctrl_hw *ctrl, int mask);

	/**
	 * mask_error_int() - Mask/Unmask particular DSI error interrupts
	 * @ctrl:         Pointer to the controller host hardware.
	 * @idx:	  Indicates the errors to be masked.
	 * @en:		  Bool for mask or unmask of the error
	 */
	void (*mask_error_intr)(struct dsi_ctrl_hw *ctrl, u32 idx, bool en);

	/**
	 * error_intr_ctrl() - Mask/Unmask master DSI error interrupt
	 * @ctrl:         Pointer to the controller host hardware.
	 * @en:		  Bool for mask or unmask of DSI error
	 */
	void (*error_intr_ctrl)(struct dsi_ctrl_hw *ctrl, bool en);

	/**
	 * get_error_mask() - get DSI error interrupt mask status
	 * @ctrl:         Pointer to the controller host hardware.
	 */
	u32 (*get_error_mask)(struct dsi_ctrl_hw *ctrl);

	/**
	 * get_hw_version() - get DSI controller hw version
	 * @ctrl:         Pointer to the controller host hardware.
	 */
	u32 (*get_hw_version)(struct dsi_ctrl_hw *ctrl);

	/**
	 * wait_for_cmd_mode_mdp_idle() - wait for command mode engine not to
	 *                           be busy sending data from display engine
	 * @ctrl:         Pointer to the controller host hardware.
	 */
	int (*wait_for_cmd_mode_mdp_idle)(struct dsi_ctrl_hw *ctrl);

	/**
	 * hw.ops.set_continuous_clk() - Set continuous clock
	 * @ctrl:         Pointer to the controller host hardware.
	 * @enable:	  Bool to control continuous clock request.
	 */
	void (*set_continuous_clk)(struct dsi_ctrl_hw *ctrl, bool enable);

	/**
	 * hw.ops.wait4dynamic_refresh_done() - Wait for dynamic refresh done
	 * @ctrl:         Pointer to the controller host hardware.
	 */
	int (*wait4dynamic_refresh_done)(struct dsi_ctrl_hw *ctrl);

	/**
	 * hw.ops.vid_engine_busy() - Returns true if vid engine is busy
	 * @ctrl:	Pointer to the controller host hardware.
	 */
	bool (*vid_engine_busy)(struct dsi_ctrl_hw *ctrl);

	/**
	 * hw.ops.hs_req_sel() - enable continuous clk support through phy
	 * @ctrl:	Pointer to the controller host hardware.
	 * @sel_phy:	Bool to control whether to select phy or controller
	 */
	void (*hs_req_sel)(struct dsi_ctrl_hw *ctrl, bool sel_phy);

	/**
	 * hw.ops.configure_cmddma_window() - configure DMA window for CMD TX
	 * @ctrl:	Pointer to the controller host hardware.
	 * @cmd:	Pointer to the DSI DMA command info.
	 * @line_no:	Line number at which the CMD needs to be triggered.
	 * @window:	Width of the DMA CMD window.
	 */
	void (*configure_cmddma_window)(struct dsi_ctrl_hw *ctrl,
			struct dsi_ctrl_cmd_dma_info *cmd,
			u32 line_no, u32 window);

	/**
	 * hw.ops.reset_trig_ctrl() - resets trigger control of DSI controller
	 * @ctrl:	Pointer to the controller host hardware.
	 * @cfg:	Common configuration parameters.
	 */
	void (*reset_trig_ctrl)(struct dsi_ctrl_hw *ctrl,
			struct dsi_host_common_cfg *cfg);

	/**
	 * hw.ops.init_cmddma_trig_ctrl() - Initialize the default trigger used
	 *                             for command mode DMA path.
	 * @ctrl:                Pointer to the controller host hardware.
	 * @cfg:                 Common configuration parameters.
	 * @do_peripheral_flush: Flag for sending this command with peripheral flush.
	 */
	void (*init_cmddma_trig_ctrl)(struct dsi_ctrl_hw *ctrl,
			struct dsi_host_common_cfg *cfg, bool do_peripheral_flush);

	/**
	 * hw.ops.log_line_count() - reads the MDP interface line count
	 *			     registers.
	 * @ctrl:	Pointer to the controller host hardware.
	 * @cmd_mode:	Boolean to indicate command mode operation.
	 */
	u32 (*log_line_count)(struct dsi_ctrl_hw *ctrl, bool cmd_mode);

	/**
	 * hw.ops.splitlink_cmd_setup() - configure the sublink to transfer
	 * @ctrl:       Pointer to the controller host hardware.
	 * @common_cfg: Common configuration parameters.
	 * @sublink:    Which sublink to transfer the command.
	 */
	void (*splitlink_cmd_setup)(struct dsi_ctrl_hw *ctrl,
			struct dsi_host_common_cfg *common_cfg, u32 sublink);
};

/*
 * struct dsi_ctrl_hw - DSI controller hardware object specific to an instance
 * @base:                   VA for the DSI controller base address.
 * @length:                 Length of the DSI controller register map.
 * @mmss_misc_base:         Base address of mmss_misc register map.
 * @mmss_misc_length:       Length of mmss_misc register map.
 * @disp_cc_base:           Base address of disp_cc register map.
 * @disp_cc_length:         Length of disp_cc register map.
 * @mdp_intf_base:	    Base address of mdp_intf register map. Addresses of
 *			    MDP_TEAR_INTF_TEAR_LINE_COUNT and MDP_TEAR_INTF_LINE_COUNT
 *			    are mapped using the base address to test and validate
 *			    the RD ptr value and line count value respectively when
 *			    a CMD is triggered and it succeeds.
 * @index:                  Instance ID of the controller.
 * @feature_map:            Features supported by the DSI controller.
 * @ops:                    Function pointers to the operations supported by the
 *                          controller.
 * @supported_interrupts:   Number of supported interrupts.
 * @supported_errors:       Number of supported errors.
 * @phy_pll_bypass:         A boolean property that enables skipping HW access in
 *                          PHY/PLL drivers for running on emulation platforms.
 * @null_insertion_enabled:  A boolean property to allow dsi controller to
 *                           insert null packet.
 * @widebus_support:        48 bit wide data bus is supported.
 * @reset_trig_ctrl:		Boolean to indicate if trigger control needs to
 *				be reset to default.
 */
struct dsi_ctrl_hw {
	void __iomem *base;
	u32 length;
	void __iomem *mmss_misc_base;
	u32 mmss_misc_length;
	void __iomem *disp_cc_base;
	u32 disp_cc_length;
	void __iomem *mdp_intf_base;
	u32 index;

	/* features */
	DECLARE_BITMAP(feature_map, DSI_CTRL_MAX_FEATURES);
	struct dsi_ctrl_hw_ops ops;

	/* capabilities */
	u32 supported_interrupts;
	u64 supported_errors;

	bool phy_pll_bypass;
	bool null_insertion_enabled;
	bool widebus_support;
	bool reset_trig_ctrl;
};

#endif /* _DSI_CTRL_HW_H_ */
