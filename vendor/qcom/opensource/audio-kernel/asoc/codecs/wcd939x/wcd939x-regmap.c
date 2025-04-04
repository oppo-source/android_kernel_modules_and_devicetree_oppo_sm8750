// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/regmap.h>
#include <linux/device.h>
#include "wcd939x-registers.h"

extern const u8 wcd939x_reg_access[WCD939X_NUM_REGISTERS];

static struct reg_default wcd939x_defaults[] = {
	{WCD939X_ANA_PAGE,                       0x00},
	{WCD939X_BIAS,                           0x00},
	{WCD939X_RX_SUPPLIES,                    0x00},
	{WCD939X_HPH,                            0x0c},
	{WCD939X_EAR,                            0x00},
	{WCD939X_EAR_COMPANDER_CTL,              0x02},
	{WCD939X_TX_CH1,                         0x20},
	{WCD939X_TX_CH2,                         0x00},
	{WCD939X_TX_CH3,                         0x20},
	{WCD939X_TX_CH4,                         0x00},
	{WCD939X_MICB1_MICB2_DSP_EN_LOGIC,       0x00},
	{WCD939X_MICB3_DSP_EN_LOGIC,             0x00},
	{WCD939X_MBHC_MECH,                      0x39},
	{WCD939X_MBHC_ELECT,                     0x08},
	{WCD939X_MBHC_ZDET,                      0x00},
	{WCD939X_MBHC_RESULT_1,                  0x00},
	{WCD939X_MBHC_RESULT_2,                  0x00},
	{WCD939X_MBHC_RESULT_3,                  0x00},
	{WCD939X_MBHC_BTN0,                      0x00},
	{WCD939X_MBHC_BTN1,                      0x10},
	{WCD939X_MBHC_BTN2,                      0x20},
	{WCD939X_MBHC_BTN3,                      0x30},
	{WCD939X_MBHC_BTN4,                      0x40},
	{WCD939X_MBHC_BTN5,                      0x50},
	{WCD939X_MBHC_BTN6,                      0x60},
	{WCD939X_MBHC_BTN7,                      0x70},
	{WCD939X_MICB1,                          0x10},
	{WCD939X_MICB2,                          0x10},
	{WCD939X_MICB2_RAMP,                     0x00},
	{WCD939X_MICB3,                          0x00},
	{WCD939X_MICB4,                          0x00},
	{WCD939X_CTL,                            0x2a},
	{WCD939X_VBG_FINE_ADJ,                   0x55},
	{WCD939X_VDDCX_ADJUST,                   0x01},
	{WCD939X_DISABLE_LDOL,                   0x00},
	{WCD939X_CTL_CLK,                        0x00},
	{WCD939X_CTL_ANA,                        0x00},
	{WCD939X_ZDET_VNEG_CTL,                  0x00},
	{WCD939X_ZDET_BIAS_CTL,                  0x46},
	{WCD939X_CTL_BCS,                        0x00},
	{WCD939X_MOISTURE_DET_FSM_STATUS,        0x00},
	{WCD939X_TEST_CTL,                       0x00},
	{WCD939X_MODE,                           0x2b},
	{WCD939X_LDOH_BIAS,                      0x68},
	{WCD939X_STB_LOADS,                      0x00},
	{WCD939X_SLOWRAMP,                       0x50},
	{WCD939X_TEST_CTL_1,                     0x1a},
	{WCD939X_TEST_CTL_2,                     0x00},
	{WCD939X_TEST_CTL_3,                     0xa4},
	{WCD939X_MICB2_TEST_CTL_1,               0x1a},
	{WCD939X_MICB2_TEST_CTL_2,               0x00},
	{WCD939X_MICB2_TEST_CTL_3,               0x24},
	{WCD939X_MICB3_TEST_CTL_1,               0x9a},
	{WCD939X_MICB3_TEST_CTL_2,               0x80},
	{WCD939X_MICB3_TEST_CTL_3,               0x24},
	{WCD939X_MICB4_TEST_CTL_1,               0x1a},
	{WCD939X_MICB4_TEST_CTL_2,               0x80},
	{WCD939X_MICB4_TEST_CTL_3,               0x24},
	{WCD939X_ADC_VCM,                        0x39},
	{WCD939X_BIAS_ATEST,                     0xe0},
	{WCD939X_SPARE1,                         0x00},
	{WCD939X_SPARE2,                         0x00},
	{WCD939X_TXFE_DIV_CTL,                   0x22},
	{WCD939X_TXFE_DIV_START,                 0x00},
	{WCD939X_SPARE3,                         0x00},
	{WCD939X_SPARE4,                         0x00},
	{WCD939X_TEST_EN,                        0xcc},
	{WCD939X_ADC_IB,                         0xe9},
	{WCD939X_ATEST_REFCTL,                   0x0b},
	{WCD939X_TX_1_2_TEST_CTL,                0x38},
	{WCD939X_TEST_BLK_EN1,                   0xff},
	{WCD939X_TXFE1_CLKDIV,                   0x00},
	{WCD939X_SAR2_ERR,                       0x00},
	{WCD939X_SAR1_ERR,                       0x00},
	{WCD939X_TX_3_4_TEST_EN,                 0xcc},
	{WCD939X_TX_3_4_ADC_IB,                  0xe9},
	{WCD939X_TX_3_4_ATEST_REFCTL,            0x0b},
	{WCD939X_TX_3_4_TEST_CTL,                0x38},
	{WCD939X_TEST_BLK_EN3,                   0xff},
	{WCD939X_TXFE3_CLKDIV,                   0x00},
	{WCD939X_SAR4_ERR,                       0x00},
	{WCD939X_SAR3_ERR,                       0x00},
	{WCD939X_TEST_BLK_EN2,                   0xfb},
	{WCD939X_TXFE2_CLKDIV,                   0x00},
	{WCD939X_TX_3_4_SPARE1,                  0x00},
	{WCD939X_TEST_BLK_EN4,                   0xfb},
	{WCD939X_TXFE4_CLKDIV,                   0x00},
	{WCD939X_TX_3_4_SPARE2,                  0x00},
	{WCD939X_MODE_1,                         0x40},
	{WCD939X_MODE_2,                         0x3a},
	{WCD939X_MODE_3,                         0xf0},
	{WCD939X_CTRL_VCL_1,                     0x7c},
	{WCD939X_CTRL_VCL_2,                     0x82},
	{WCD939X_CTRL_CCL_1,                     0x31},
	{WCD939X_CTRL_CCL_2,                     0x80},
	{WCD939X_CTRL_CCL_3,                     0x80},
	{WCD939X_CTRL_CCL_4,                     0x51},
	{WCD939X_CTRL_CCL_5,                     0x00},
	{WCD939X_BUCK_TMUX_A_D,                  0x00},
	{WCD939X_BUCK_SW_DRV_CNTL,               0x77},
	{WCD939X_SPARE,                          0x80},
	{WCD939X_EN,                             0x4e},
	{WCD939X_VNEG_CTRL_1,                    0x0b},
	{WCD939X_VNEG_CTRL_2,                    0x45},
	{WCD939X_VNEG_CTRL_3,                    0x14},
	{WCD939X_VNEG_CTRL_4,                    0xdb},
	{WCD939X_VNEG_CTRL_5,                    0x83},
	{WCD939X_VNEG_CTRL_6,                    0x98},
	{WCD939X_VNEG_CTRL_7,                    0xa9},
	{WCD939X_VNEG_CTRL_8,                    0x68},
	{WCD939X_VNEG_CTRL_9,                    0x66},
	{WCD939X_VNEGDAC_CTRL_1,                 0xed},
	{WCD939X_VNEGDAC_CTRL_2,                 0xf8},
	{WCD939X_VNEGDAC_CTRL_3,                 0xa6},
	{WCD939X_CTRL_1,                         0x65},
	{WCD939X_FLYBACK_TEST_CTL,               0x02},
	{WCD939X_AUX_SW_CTL,                     0x00},
	{WCD939X_PA_AUX_IN_CONN,                 0x01},
	{WCD939X_TIMER_DIV,                      0x32},
	{WCD939X_OCP_CTL,                        0x1f},
	{WCD939X_OCP_COUNT,                      0x77},
	{WCD939X_BIAS_EAR_DAC,                   0xa0},
	{WCD939X_BIAS_EAR_AMP,                   0xaa},
	{WCD939X_BIAS_HPH_LDO,                   0xa9},
	{WCD939X_BIAS_HPH_PA,                    0xaa},
	{WCD939X_BIAS_HPH_RDACBUFF_CNP2,         0xca},
	{WCD939X_BIAS_HPH_RDAC_LDO,              0x88},
	{WCD939X_BIAS_HPH_CNP1,                  0x82},
	{WCD939X_BIAS_HPH_LOWPOWER,              0x82},
	{WCD939X_BIAS_AUX_DAC,                   0xa0},
	{WCD939X_BIAS_AUX_AMP,                   0xaa},
	{WCD939X_BIAS_VNEGDAC_BLEEDER,           0x50},
	{WCD939X_BIAS_MISC,                      0x00},
	{WCD939X_BIAS_BUCK_RST,                  0x08},
	{WCD939X_BIAS_BUCK_VREF_ERRAMP,          0x44},
	{WCD939X_BIAS_FLYB_ERRAMP,               0x40},
	{WCD939X_BIAS_FLYB_BUFF,                 0xaa},
	{WCD939X_BIAS_FLYB_MID_RST,              0x14},
	{WCD939X_L_STATUS,                       0x04},
	{WCD939X_R_STATUS,                       0x04},
	{WCD939X_CNP_EN,                         0x80},
	{WCD939X_CNP_WG_CTL,                     0x9a},
	{WCD939X_CNP_WG_TIME,                    0x14},
	{WCD939X_HPH_OCP_CTL,                    0x28},
	{WCD939X_AUTO_CHOP,                      0x56},
	{WCD939X_CHOP_CTL,                       0x83},
	{WCD939X_PA_CTL1,                        0x46},
	{WCD939X_PA_CTL2,                        0x50},
	{WCD939X_L_EN,                           0x80},
	{WCD939X_L_TEST,                         0xe0},
	{WCD939X_L_ATEST,                        0x50},
	{WCD939X_R_EN,                           0x80},
	{WCD939X_R_TEST,                         0xe0},
	{WCD939X_R_ATEST,                        0x50},
	{WCD939X_RDAC_CLK_CTL1,                  0x80},
	{WCD939X_RDAC_CLK_CTL2,                  0x0b},
	{WCD939X_RDAC_LDO_CTL,                   0x33},
	{WCD939X_RDAC_CHOP_CLK_LP_CTL,           0x00},
	{WCD939X_REFBUFF_UHQA_CTL,               0x00},
	{WCD939X_REFBUFF_LP_CTL,                 0x8e},
	{WCD939X_L_DAC_CTL,                      0x20},
	{WCD939X_R_DAC_CTL,                      0x20},
	{WCD939X_HPHLR_SURGE_COMP_SEL,           0x55},
	{WCD939X_HPHLR_SURGE_EN,                 0x19},
	{WCD939X_HPHLR_SURGE_MISC1,              0xa0},
	{WCD939X_HPHLR_SURGE_STATUS,             0x00},
	{WCD939X_EAR_EN_REG,                     0x22},
	{WCD939X_EAR_PA_CON,                     0x44},
	{WCD939X_EAR_SP_CON,                     0xdb},
	{WCD939X_EAR_DAC_CON,                    0x80},
	{WCD939X_EAR_CNP_FSM_CON,                0xb2},
	{WCD939X_EAR_TEST_CTL,                   0x00},
	{WCD939X_STATUS_REG_1,                   0x00},
	{WCD939X_STATUS_REG_2,                   0x08},
	{WCD939X_FLYBACK_NEW_CTRL_2,             0x00},
	{WCD939X_FLYBACK_NEW_CTRL_3,             0x00},
	{WCD939X_FLYBACK_NEW_CTRL_4,             0x44},
	{WCD939X_ANA_NEW_PAGE,                   0x00},
	{WCD939X_ANA_HPH2,                       0x00},
	{WCD939X_ANA_HPH3,                       0x00},
	{WCD939X_SLEEP_CTL,                      0x18},
	{WCD939X_WATCHDOG_CTL,                   0x00},
	{WCD939X_ELECT_REM_CLAMP_CTL,            0x00},
	{WCD939X_CTL_1,                          0x02},
	{WCD939X_CTL_2,                          0x05},
	{WCD939X_PLUG_DETECT_CTL,                0xe9},
	{WCD939X_ZDET_ANA_CTL,                   0x0f},
	{WCD939X_ZDET_RAMP_CTL,                  0x00},
	{WCD939X_FSM_STATUS,                     0x00},
	{WCD939X_ADC_RESULT,                     0x00},
	{WCD939X_TX_CH12_MUX,                    0x11},
	{WCD939X_TX_CH34_MUX,                    0x23},
	{WCD939X_DIE_CRK_DET_EN,                 0x00},
	{WCD939X_DIE_CRK_DET_OUT,                0x00},
	{WCD939X_RDAC_GAIN_CTL,                  0x00},
	{WCD939X_PA_GAIN_CTL_L,                  0x00},
	{WCD939X_RDAC_VREF_CTL,                  0x08},
	{WCD939X_RDAC_OVERRIDE_CTL,              0x00},
	{WCD939X_PA_GAIN_CTL_R,                  0x00},
	{WCD939X_PA_MISC1,                       0x32},
	{WCD939X_PA_MISC2,                       0x00},
	{WCD939X_PA_RDAC_MISC,                   0x00},
	{WCD939X_HPH_TIMER1,                     0xfe},
	{WCD939X_HPH_TIMER2,                     0x02},
	{WCD939X_HPH_TIMER3,                     0x4e},
	{WCD939X_HPH_TIMER4,                     0x54},
	{WCD939X_PA_RDAC_MISC2,                  0x0b},
	{WCD939X_PA_RDAC_MISC3,                  0x00},
	{WCD939X_RDAC_HD2_CTL_L,                 0xa0},
	{WCD939X_RDAC_HD2_CTL_R,                 0xa0},
	{WCD939X_HPH_RDAC_BIAS_LOHIFI,           0x64},
	{WCD939X_HPH_RDAC_BIAS_ULP,              0x01},
	{WCD939X_HPH_RDAC_LDO_LP,                0x11},
	{WCD939X_MOISTURE_DET_DC_CTRL,           0x57},
	{WCD939X_MOISTURE_DET_POLLING_CTRL,      0x01},
	{WCD939X_MECH_DET_CURRENT,               0x00},
	{WCD939X_ZDET_CLK_AND_MOISTURE_CTL_NEW,  0x47},
	{WCD939X_EAR_CHOPPER_CON,                0xa8},
	{WCD939X_CNP_VCM_CON1,                   0x42},
	{WCD939X_CNP_VCM_CON2,                   0x22},
	{WCD939X_EAR_DYNAMIC_BIAS,               0x00},
	{WCD939X_WATCHDOG_CTL_1,                 0x0a},
	{WCD939X_WATCHDOG_CTL_2,                 0x0a},
	{WCD939X_DIE_CRK_DET_INT1,               0x02},
	{WCD939X_DIE_CRK_DET_INT2,               0x60},
	{WCD939X_TXFE_DIVSTOP_L2,                0xff},
	{WCD939X_TXFE_DIVSTOP_L1,                0x7f},
	{WCD939X_TXFE_DIVSTOP_L0,                0x3f},
	{WCD939X_TXFE_DIVSTOP_ULP1P2M,           0x1f},
	{WCD939X_TXFE_DIVSTOP_ULP0P6M,           0x0f},
	{WCD939X_TXFE_ICTRL_STG1_L2L1,           0xd7},
	{WCD939X_TXFE_ICTRL_STG1_L0,             0xc8},
	{WCD939X_TXFE_ICTRL_STG1_ULP,            0xc6},
	{WCD939X_TXFE_ICTRL_STG2MAIN_L2L1,       0x95},
	{WCD939X_TXFE_ICTRL_STG2MAIN_L0,         0x6a},
	{WCD939X_TXFE_ICTRL_STG2MAIN_ULP,        0x05},
	{WCD939X_TXFE_ICTRL_STG2CASC_L2L1L0,     0xa5},
	{WCD939X_TXFE_ICTRL_STG2CASC_ULP,        0x13},
	{WCD939X_TXADC_SCBIAS_L2L1,              0x88},
	{WCD939X_TXADC_SCBIAS_L0ULP,             0x42},
	{WCD939X_TXADC_INT_L2,                   0xff},
	{WCD939X_TXADC_INT_L1,                   0x64},
	{WCD939X_TXADC_INT_L0,                   0x64},
	{WCD939X_TXADC_INT_ULP,                  0x77},
	{WCD939X_DIGITAL_PAGE,                   0x00},
	{WCD939X_CHIP_ID0,                       0x00},
	{WCD939X_CHIP_ID1,                       0x00},
	{WCD939X_CHIP_ID2,                       0x0e},
	{WCD939X_CHIP_ID3,                       0x01},
	{WCD939X_SWR_TX_CLK_RATE,                0x00},
	{WCD939X_CDC_RST_CTL,                    0x03},
	{WCD939X_TOP_CLK_CFG,                    0x00},
	{WCD939X_CDC_ANA_CLK_CTL,                0x00},
	{WCD939X_CDC_DIG_CLK_CTL,                0xf0},
	{WCD939X_SWR_RST_EN,                     0x00},
	{WCD939X_CDC_PATH_MODE,                  0x55},
	{WCD939X_CDC_RX_RST,                     0x00},
	{WCD939X_CDC_RX0_CTL,                    0xfc},
	{WCD939X_CDC_RX1_CTL,                    0xfc},
	{WCD939X_CDC_RX2_CTL,                    0xfc},
	{WCD939X_CDC_TX_ANA_MODE_0_1,            0x00},
	{WCD939X_CDC_TX_ANA_MODE_2_3,            0x00},
	{WCD939X_CDC_COMP_CTL_0,                 0x00},
	{WCD939X_CDC_ANA_TX_CLK_CTL,             0x1e},
	{WCD939X_CDC_HPH_DSM_A1_0,               0x00},
	{WCD939X_CDC_HPH_DSM_A1_1,               0x01},
	{WCD939X_CDC_HPH_DSM_A2_0,               0x63},
	{WCD939X_CDC_HPH_DSM_A2_1,               0x04},
	{WCD939X_CDC_HPH_DSM_A3_0,               0xac},
	{WCD939X_CDC_HPH_DSM_A3_1,               0x04},
	{WCD939X_CDC_HPH_DSM_A4_0,               0x1a},
	{WCD939X_CDC_HPH_DSM_A4_1,               0x03},
	{WCD939X_CDC_HPH_DSM_A5_0,               0xbc},
	{WCD939X_CDC_HPH_DSM_A5_1,               0x02},
	{WCD939X_CDC_HPH_DSM_A6_0,               0xc7},
	{WCD939X_CDC_HPH_DSM_A7_0,               0xf8},
	{WCD939X_CDC_HPH_DSM_C_0,                0x47},
	{WCD939X_CDC_HPH_DSM_C_1,                0x43},
	{WCD939X_CDC_HPH_DSM_C_2,                0xb1},
	{WCD939X_CDC_HPH_DSM_C_3,                0x17},
	{WCD939X_CDC_HPH_DSM_R1,                 0x4d},
	{WCD939X_CDC_HPH_DSM_R2,                 0x29},
	{WCD939X_CDC_HPH_DSM_R3,                 0x34},
	{WCD939X_CDC_HPH_DSM_R4,                 0x59},
	{WCD939X_CDC_HPH_DSM_R5,                 0x66},
	{WCD939X_CDC_HPH_DSM_R6,                 0x87},
	{WCD939X_CDC_HPH_DSM_R7,                 0x64},
	{WCD939X_CDC_EAR_DSM_A1_0,               0x00},
	{WCD939X_CDC_EAR_DSM_A1_1,               0x01},
	{WCD939X_CDC_EAR_DSM_A2_0,               0x96},
	{WCD939X_CDC_EAR_DSM_A2_1,               0x09},
	{WCD939X_CDC_EAR_DSM_A3_0,               0xab},
	{WCD939X_CDC_EAR_DSM_A3_1,               0x05},
	{WCD939X_CDC_EAR_DSM_A4_0,               0x1c},
	{WCD939X_CDC_EAR_DSM_A4_1,               0x02},
	{WCD939X_CDC_EAR_DSM_A5_0,               0x17},
	{WCD939X_CDC_EAR_DSM_A5_1,               0x02},
	{WCD939X_CDC_EAR_DSM_A6_0,               0xaa},
	{WCD939X_CDC_EAR_DSM_A7_0,               0xe3},
	{WCD939X_CDC_EAR_DSM_C_0,                0x69},
	{WCD939X_CDC_EAR_DSM_C_1,                0x54},
	{WCD939X_CDC_EAR_DSM_C_2,                0x02},
	{WCD939X_CDC_EAR_DSM_C_3,                0x15},
	{WCD939X_CDC_EAR_DSM_R1,                 0xa4},
	{WCD939X_CDC_EAR_DSM_R2,                 0xb5},
	{WCD939X_CDC_EAR_DSM_R3,                 0x86},
	{WCD939X_CDC_EAR_DSM_R4,                 0x85},
	{WCD939X_CDC_EAR_DSM_R5,                 0xaa},
	{WCD939X_CDC_EAR_DSM_R6,                 0xe2},
	{WCD939X_CDC_EAR_DSM_R7,                 0x62},
	{WCD939X_CDC_HPH_GAIN_RX_0,              0x55},
	{WCD939X_CDC_HPH_GAIN_RX_1,              0xa9},
	{WCD939X_CDC_HPH_GAIN_DSD_0,             0x3d},
	{WCD939X_CDC_HPH_GAIN_DSD_1,             0x2e},
	{WCD939X_CDC_HPH_GAIN_DSD_2,             0x01},
	{WCD939X_CDC_EAR_GAIN_DSD_0,             0x00},
	{WCD939X_CDC_EAR_GAIN_DSD_1,             0xfc},
	{WCD939X_CDC_EAR_GAIN_DSD_2,             0x01},
	{WCD939X_CDC_HPH_GAIN_CTL,               0x00},
	{WCD939X_CDC_EAR_GAIN_CTL,               0x00},
	{WCD939X_CDC_EAR_PATH_CTL,               0x00},
	{WCD939X_CDC_SWR_CLH,                    0x00},
	{WCD939X_SWR_CLH_BYP,                    0x00},
	{WCD939X_CDC_TX0_CTL,                    0x68},
	{WCD939X_CDC_TX1_CTL,                    0x68},
	{WCD939X_CDC_TX2_CTL,                    0x68},
	{WCD939X_CDC_TX_RST,                     0x00},
	{WCD939X_CDC_REQ_CTL,                    0x01},
	{WCD939X_CDC_RST,                        0x00},
	{WCD939X_CDC_AMIC_CTL,                   0x0f},
	{WCD939X_CDC_DMIC_CTL,                   0x04},
	{WCD939X_CDC_DMIC1_CTL,                  0x01},
	{WCD939X_CDC_DMIC2_CTL,                  0x01},
	{WCD939X_CDC_DMIC3_CTL,                  0x01},
	{WCD939X_CDC_DMIC4_CTL,                  0x01},
	{WCD939X_EFUSE_PRG_CTL,                  0x00},
	{WCD939X_EFUSE_CTL,                      0x2b},
	{WCD939X_CDC_DMIC_RATE_1_2,              0x11},
	{WCD939X_CDC_DMIC_RATE_3_4,              0x11},
	{WCD939X_PDM_WD_CTL0,                    0x00},
	{WCD939X_PDM_WD_CTL1,                    0x00},
	{WCD939X_PDM_WD_CTL2,                    0x00},
	{WCD939X_INTR_MODE,                      0x00},
	{WCD939X_INTR_MASK_0,                    0xff},
	{WCD939X_INTR_MASK_1,                    0xe7},
	{WCD939X_INTR_MASK_2,                    0x0e},
	{WCD939X_INTR_STATUS_0,                  0x00},
	{WCD939X_INTR_STATUS_1,                  0x00},
	{WCD939X_INTR_STATUS_2,                  0x00},
	{WCD939X_INTR_CLEAR_0,                   0x00},
	{WCD939X_INTR_CLEAR_1,                   0x00},
	{WCD939X_INTR_CLEAR_2,                   0x00},
	{WCD939X_INTR_LEVEL_0,                   0x00},
	{WCD939X_INTR_LEVEL_1,                   0x00},
	{WCD939X_INTR_LEVEL_2,                   0x00},
	{WCD939X_INTR_SET_0,                     0x00},
	{WCD939X_INTR_SET_1,                     0x00},
	{WCD939X_INTR_SET_2,                     0x00},
	{WCD939X_INTR_TEST_0,                    0x00},
	{WCD939X_INTR_TEST_1,                    0x00},
	{WCD939X_INTR_TEST_2,                    0x00},
	{WCD939X_TX_MODE_DBG_EN,                 0x00},
	{WCD939X_TX_MODE_DBG_0_1,                0x00},
	{WCD939X_TX_MODE_DBG_2_3,                0x00},
	{WCD939X_LB_IN_SEL_CTL,                  0x00},
	{WCD939X_LOOP_BACK_MODE,                 0x00},
	{WCD939X_SWR_DAC_TEST,                   0x00},
	{WCD939X_SWR_HM_TEST_RX_0,               0x40},
	{WCD939X_SWR_HM_TEST_TX_0,               0x40},
	{WCD939X_SWR_HM_TEST_RX_1,               0x00},
	{WCD939X_SWR_HM_TEST_TX_1,               0x00},
	{WCD939X_SWR_HM_TEST_TX_2,               0x00},
	{WCD939X_SWR_HM_TEST_0,                  0x00},
	{WCD939X_SWR_HM_TEST_1,                  0x00},
	{WCD939X_PAD_CTL_SWR_0,                  0x8f},
	{WCD939X_PAD_CTL_SWR_1,                  0x06},
	{WCD939X_I2C_CTL,                        0x00},
	{WCD939X_CDC_TX_TANGGU_SW_MODE,          0x00},
	{WCD939X_EFUSE_TEST_CTL_0,               0x00},
	{WCD939X_EFUSE_TEST_CTL_1,               0x00},
	{WCD939X_EFUSE_T_DATA_0,                 0x00},
	{WCD939X_EFUSE_T_DATA_1,                 0x00},
	{WCD939X_PAD_CTL_PDM_RX0,                0xf1},
	{WCD939X_PAD_CTL_PDM_RX1,                0xf1},
	{WCD939X_PAD_CTL_PDM_TX0,                0xf1},
	{WCD939X_PAD_CTL_PDM_TX1,                0xf1},
	{WCD939X_PAD_CTL_PDM_TX2,                0xf1},
	{WCD939X_PAD_INP_DIS_0,                  0x00},
	{WCD939X_PAD_INP_DIS_1,                  0x00},
	{WCD939X_DRIVE_STRENGTH_0,               0x00},
	{WCD939X_DRIVE_STRENGTH_1,               0x00},
	{WCD939X_DRIVE_STRENGTH_2,               0x00},
	{WCD939X_RX_DATA_EDGE_CTL,               0x1f},
	{WCD939X_TX_DATA_EDGE_CTL,               0x80},
	{WCD939X_GPIO_MODE,                      0x00},
	{WCD939X_PIN_CTL_OE,                     0x00},
	{WCD939X_PIN_CTL_DATA_0,                 0x00},
	{WCD939X_PIN_CTL_DATA_1,                 0x00},
	{WCD939X_PIN_STATUS_0,                   0x00},
	{WCD939X_PIN_STATUS_1,                   0x00},
	{WCD939X_DIG_DEBUG_CTL,                  0x00},
	{WCD939X_DIG_DEBUG_EN,                   0x00},
	{WCD939X_ANA_CSR_DBG_ADD,                0x00},
	{WCD939X_ANA_CSR_DBG_CTL,                0x48},
	{WCD939X_SSP_DBG,                        0x00},
	{WCD939X_MODE_STATUS_0,                  0x00},
	{WCD939X_MODE_STATUS_1,                  0x00},
	{WCD939X_SPARE_0,                        0x00},
	{WCD939X_SPARE_1,                        0x00},
	{WCD939X_SPARE_2,                        0x00},
	{WCD939X_EFUSE_REG_0,                    0x00},
	{WCD939X_EFUSE_REG_1,                    0xff},
	{WCD939X_EFUSE_REG_2,                    0xff},
	{WCD939X_EFUSE_REG_3,                    0xff},
	{WCD939X_EFUSE_REG_4,                    0xff},
	{WCD939X_EFUSE_REG_5,                    0xff},
	{WCD939X_EFUSE_REG_6,                    0xff},
	{WCD939X_EFUSE_REG_7,                    0xff},
	{WCD939X_EFUSE_REG_8,                    0xff},
	{WCD939X_EFUSE_REG_9,                    0xff},
	{WCD939X_EFUSE_REG_10,                   0xff},
	{WCD939X_EFUSE_REG_11,                   0xff},
	{WCD939X_EFUSE_REG_12,                   0xff},
	{WCD939X_EFUSE_REG_13,                   0xff},
	{WCD939X_EFUSE_REG_14,                   0xff},
	{WCD939X_EFUSE_REG_15,                   0xff},
	{WCD939X_EFUSE_REG_16,                   0xff},
	{WCD939X_EFUSE_REG_17,                   0xff},
	{WCD939X_EFUSE_REG_18,                   0xff},
	{WCD939X_EFUSE_REG_19,                   0xff},
	{WCD939X_EFUSE_REG_20,                   0x0e},
	{WCD939X_EFUSE_REG_21,                   0x00},
	{WCD939X_EFUSE_REG_22,                   0x00},
	{WCD939X_EFUSE_REG_23,                   0xf6},
	{WCD939X_EFUSE_REG_24,                   0x18},
	{WCD939X_EFUSE_REG_25,                   0x00},
	{WCD939X_EFUSE_REG_26,                   0x00},
	{WCD939X_EFUSE_REG_27,                   0x00},
	{WCD939X_EFUSE_REG_28,                   0x00},
	{WCD939X_EFUSE_REG_29,                   0x0f},
	{WCD939X_EFUSE_REG_30,                   0x49},
	{WCD939X_EFUSE_REG_31,                   0x00},
	{WCD939X_TX_REQ_FB_CTL_0,                0x88},
	{WCD939X_TX_REQ_FB_CTL_1,                0x88},
	{WCD939X_TX_REQ_FB_CTL_2,                0x88},
	{WCD939X_TX_REQ_FB_CTL_3,                0x88},
	{WCD939X_TX_REQ_FB_CTL_4,                0x88},
	{WCD939X_DEM_BYPASS_DATA0,               0x55},
	{WCD939X_DEM_BYPASS_DATA1,               0x55},
	{WCD939X_DEM_BYPASS_DATA2,               0x55},
	{WCD939X_DEM_BYPASS_DATA3,               0x01},
	{WCD939X_DEM_SECOND_ORDER,               0x03},
	{WCD939X_DSM_CTRL,                       0x00},
	{WCD939X_DSM_0_STATIC_DATA_0,            0x00},
	{WCD939X_DSM_0_STATIC_DATA_1,            0x00},
	{WCD939X_DSM_0_STATIC_DATA_2,            0x00},
	{WCD939X_DSM_0_STATIC_DATA_3,            0x00},
	{WCD939X_DSM_1_STATIC_DATA_0,            0x00},
	{WCD939X_DSM_1_STATIC_DATA_1,            0x00},
	{WCD939X_DSM_1_STATIC_DATA_2,            0x00},
	{WCD939X_DSM_1_STATIC_DATA_3,            0x00},
	{WCD939X_RX_TOP_PAGE,                    0x00},
	{WCD939X_TOP_CFG0,                       0x00},
	{WCD939X_HPHL_COMP_WR_LSB,               0x00},
	{WCD939X_HPHL_COMP_WR_MSB,               0x00},
	{WCD939X_HPHL_COMP_LUT,                  0x00},
	{WCD939X_HPHL_COMP_RD_LSB,               0x00},
	{WCD939X_HPHL_COMP_RD_MSB,               0x00},
	{WCD939X_HPHR_COMP_WR_LSB,               0x00},
	{WCD939X_HPHR_COMP_WR_MSB,               0x00},
	{WCD939X_HPHR_COMP_LUT,                  0x00},
	{WCD939X_HPHR_COMP_RD_LSB,               0x00},
	{WCD939X_HPHR_COMP_RD_MSB,               0x00},
	{WCD939X_DSD0_DEBUG_CFG1,                0x05},
	{WCD939X_DSD0_DEBUG_CFG2,                0x08},
	{WCD939X_DSD0_DEBUG_CFG3,                0x00},
	{WCD939X_DSD0_DEBUG_CFG4,                0x00},
	{WCD939X_DSD0_DEBUG_CFG5,                0x00},
	{WCD939X_DSD0_DEBUG_CFG6,                0x00},
	{WCD939X_DSD1_DEBUG_CFG1,                0x03},
	{WCD939X_DSD1_DEBUG_CFG2,                0x08},
	{WCD939X_DSD1_DEBUG_CFG3,                0x00},
	{WCD939X_DSD1_DEBUG_CFG4,                0x00},
	{WCD939X_DSD1_DEBUG_CFG5,                0x00},
	{WCD939X_DSD1_DEBUG_CFG6,                0x00},
	{WCD939X_HPHL_RX_PATH_CFG0,              0x00},
	{WCD939X_HPHL_RX_PATH_CFG1,              0x00},
	{WCD939X_HPHR_RX_PATH_CFG0,              0x00},
	{WCD939X_HPHR_RX_PATH_CFG1,              0x00},
	{WCD939X_RX_PATH_CFG2,                   0x00},
	{WCD939X_HPHL_RX_PATH_SEC0,              0x00},
	{WCD939X_HPHL_RX_PATH_SEC1,              0x00},
	{WCD939X_HPHL_RX_PATH_SEC2,              0x00},
	{WCD939X_HPHL_RX_PATH_SEC3,              0x00},
	{WCD939X_HPHR_RX_PATH_SEC0,              0x00},
	{WCD939X_HPHR_RX_PATH_SEC1,              0x00},
	{WCD939X_HPHR_RX_PATH_SEC2,              0x00},
	{WCD939X_HPHR_RX_PATH_SEC3,              0x00},
	{WCD939X_RX_PATH_SEC4,                   0x00},
	{WCD939X_RX_PATH_SEC5,                   0x00},
	{WCD939X_CTL0,                           0x60},
	{WCD939X_CTL1,                           0xdb},
	{WCD939X_CTL2,                           0xff},
	{WCD939X_CTL3,                           0x35},
	{WCD939X_CTL4,                           0xff},
	{WCD939X_CTL5,                           0x00},
	{WCD939X_CTL6,                           0x01},
	{WCD939X_CTL7,                           0x08},
	{WCD939X_CTL8,                           0x00},
	{WCD939X_CTL9,                           0x00},
	{WCD939X_CTL10,                          0x06},
	{WCD939X_CTL11,                          0x12},
	{WCD939X_CTL12,                          0x1e},
	{WCD939X_CTL13,                          0x2a},
	{WCD939X_CTL14,                          0x36},
	{WCD939X_CTL15,                          0x3c},
	{WCD939X_CTL16,                          0xc4},
	{WCD939X_CTL17,                          0x00},
	{WCD939X_CTL18,                          0x0c},
	{WCD939X_CTL19,                          0x16},
	{WCD939X_R_CTL0,                         0x60},
	{WCD939X_R_CTL1,                         0xdb},
	{WCD939X_R_CTL2,                         0xff},
	{WCD939X_R_CTL3,                         0x35},
	{WCD939X_R_CTL4,                         0xff},
	{WCD939X_R_CTL5,                         0x00},
	{WCD939X_R_CTL6,                         0x01},
	{WCD939X_R_CTL7,                         0x08},
	{WCD939X_R_CTL8,                         0x00},
	{WCD939X_R_CTL9,                         0x00},
	{WCD939X_R_CTL10,                        0x06},
	{WCD939X_R_CTL11,                        0x12},
	{WCD939X_R_CTL12,                        0x1e},
	{WCD939X_R_CTL13,                        0x2a},
	{WCD939X_R_CTL14,                        0x36},
	{WCD939X_R_CTL15,                        0x3c},
	{WCD939X_R_CTL16,                        0xc4},
	{WCD939X_R_CTL17,                        0x00},
	{WCD939X_R_CTL18,                        0x0c},
	{WCD939X_R_CTL19,                        0x16},
	{WCD939X_PATH_CTL,                       0x00},
	{WCD939X_CFG0,                           0x07},
	{WCD939X_CFG1,                           0x3c},
	{WCD939X_CFG2,                           0x00},
	{WCD939X_CFG3,                           0x00},
	{WCD939X_DSD_HPHL_PATH_CTL,              0x00},
	{WCD939X_DSD_HPHL_CFG0,                  0x00},
	{WCD939X_DSD_HPHL_CFG1,                  0x00},
	{WCD939X_DSD_HPHL_CFG2,                  0x22},
	{WCD939X_DSD_HPHL_CFG3,                  0x00},
	{WCD939X_CFG4,                           0x1a},
	{WCD939X_CFG5,                           0x00},
	{WCD939X_DSD_HPHR_PATH_CTL,              0x00},
	{WCD939X_DSD_HPHR_CFG0,                  0x00},
	{WCD939X_DSD_HPHR_CFG1,                  0x00},
	{WCD939X_DSD_HPHR_CFG2,                  0x22},
	{WCD939X_DSD_HPHR_CFG3,                  0x00},
	{WCD939X_DSD_HPHR_CFG4,                  0x1a},
	{WCD939X_DSD_HPHR_CFG5,                  0x00},
};

static bool wcd939x_writeable_register(struct device *dev, unsigned int reg)
{
	if (reg <= WCD939X_BASE + 1)
		return 0;

	return wcd939x_reg_access[WCD939X_REG(reg)] & WR_REG;
}

static bool wcd939x_volatile_register(struct device *dev, unsigned int reg)
{
	if (reg <= WCD939X_BASE + 1)
		return 0;

#ifdef OPLUS_ARCH_EXTENDS
/* workAround for mic mute after ESD. case07374404 */
	if (reg == WCD939X_BIAS)
		return 1;
#endif /* OPLUS_ARCH_EXTENDS */

	return ((wcd939x_reg_access[WCD939X_REG(reg)] & RD_REG) &&
		!(wcd939x_reg_access[WCD939X_REG(reg)] & WR_REG));
}

struct regmap_config wcd939x_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = wcd939x_defaults,
	.num_reg_defaults = ARRAY_SIZE(wcd939x_defaults),
	.max_register = WCD939X_MAX_REGISTER,
	.volatile_reg = wcd939x_volatile_register,
	.writeable_reg = wcd939x_writeable_register,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.can_multi_write = true,
	.use_single_read = true,
};
