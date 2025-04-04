/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#if defined(QCC2072_HEADERS_DEF)

#undef UMAC
#define WLAN_HEADERS 1

#include "beryllium_top_reg.h"
#include "wcss_version.h"

#define MISSING 0

#define SOC_RESET_CONTROL_OFFSET MISSING
#define GPIO_PIN0_OFFSET                        MISSING
#define GPIO_PIN1_OFFSET                        MISSING
#define GPIO_PIN0_CONFIG_MASK                   MISSING
#define GPIO_PIN1_CONFIG_MASK                   MISSING
#define LOCAL_SCRATCH_OFFSET 0x18
#define GPIO_PIN10_OFFSET MISSING
#define GPIO_PIN11_OFFSET MISSING
#define GPIO_PIN12_OFFSET MISSING
#define GPIO_PIN13_OFFSET MISSING
#define MBOX_BASE_ADDRESS MISSING
#define INT_STATUS_ENABLE_ERROR_LSB MISSING
#define INT_STATUS_ENABLE_ERROR_MASK MISSING
#define INT_STATUS_ENABLE_CPU_LSB MISSING
#define INT_STATUS_ENABLE_CPU_MASK MISSING
#define INT_STATUS_ENABLE_COUNTER_LSB MISSING
#define INT_STATUS_ENABLE_COUNTER_MASK MISSING
#define INT_STATUS_ENABLE_MBOX_DATA_LSB MISSING
#define INT_STATUS_ENABLE_MBOX_DATA_MASK MISSING
#define ERROR_STATUS_ENABLE_RX_UNDERFLOW_LSB MISSING
#define ERROR_STATUS_ENABLE_RX_UNDERFLOW_MASK MISSING
#define ERROR_STATUS_ENABLE_TX_OVERFLOW_LSB MISSING
#define ERROR_STATUS_ENABLE_TX_OVERFLOW_MASK MISSING
#define COUNTER_INT_STATUS_ENABLE_BIT_LSB MISSING
#define COUNTER_INT_STATUS_ENABLE_BIT_MASK MISSING
#define INT_STATUS_ENABLE_ADDRESS MISSING
#define CPU_INT_STATUS_ENABLE_BIT_LSB MISSING
#define CPU_INT_STATUS_ENABLE_BIT_MASK MISSING
#define HOST_INT_STATUS_ADDRESS MISSING
#define CPU_INT_STATUS_ADDRESS MISSING
#define ERROR_INT_STATUS_ADDRESS MISSING
#define ERROR_INT_STATUS_WAKEUP_MASK MISSING
#define ERROR_INT_STATUS_WAKEUP_LSB MISSING
#define ERROR_INT_STATUS_RX_UNDERFLOW_MASK MISSING
#define ERROR_INT_STATUS_RX_UNDERFLOW_LSB MISSING
#define ERROR_INT_STATUS_TX_OVERFLOW_MASK MISSING
#define ERROR_INT_STATUS_TX_OVERFLOW_LSB MISSING
#define COUNT_DEC_ADDRESS MISSING
#define HOST_INT_STATUS_CPU_MASK MISSING
#define HOST_INT_STATUS_CPU_LSB MISSING
#define HOST_INT_STATUS_ERROR_MASK MISSING
#define HOST_INT_STATUS_ERROR_LSB MISSING
#define HOST_INT_STATUS_COUNTER_MASK MISSING
#define HOST_INT_STATUS_COUNTER_LSB MISSING
#define RX_LOOKAHEAD_VALID_ADDRESS MISSING
#define WINDOW_DATA_ADDRESS MISSING
#define WINDOW_READ_ADDR_ADDRESS MISSING
#define WINDOW_WRITE_ADDR_ADDRESS MISSING
/* GPIO Register */
#define GPIO_ENABLE_W1TS_LOW_ADDRESS            MISSING
#define GPIO_PIN0_CONFIG_LSB                    MISSING
#define GPIO_PIN0_PAD_PULL_LSB                  MISSING
#define GPIO_PIN0_PAD_PULL_MASK                 MISSING
/* SI reg */
#define SI_CONFIG_ERR_INT_MASK                  MISSING
#define SI_CONFIG_ERR_INT_LSB                   MISSING

#define RTC_SOC_BASE_ADDRESS MISSING
#define RTC_WMAC_BASE_ADDRESS MISSING
#define SOC_CORE_BASE_ADDRESS MISSING
#define WLAN_MAC_BASE_ADDRESS MISSING
#define GPIO_BASE_ADDRESS MISSING
#define ANALOG_INTF_BASE_ADDRESS MISSING
#define CE0_BASE_ADDRESS MISSING
#define CE1_BASE_ADDRESS MISSING
#define CE_COUNT		12
#define CE_WRAPPER_BASE_ADDRESS MISSING
#define SI_BASE_ADDRESS MISSING
#define DRAM_BASE_ADDRESS MISSING

#define WLAN_SYSTEM_SLEEP_DISABLE_LSB MISSING
#define WLAN_SYSTEM_SLEEP_DISABLE_MASK MISSING
#define CLOCK_CONTROL_OFFSET MISSING
#define CLOCK_CONTROL_SI0_CLK_MASK MISSING
#define RESET_CONTROL_SI0_RST_MASK MISSING
#define WLAN_RESET_CONTROL_OFFSET MISSING
#define WLAN_RESET_CONTROL_COLD_RST_MASK MISSING
#define WLAN_RESET_CONTROL_WARM_RST_MASK MISSING
#define CPU_CLOCK_OFFSET MISSING

#define CPU_CLOCK_STANDARD_LSB MISSING
#define CPU_CLOCK_STANDARD_MASK MISSING
#define LPO_CAL_ENABLE_LSB MISSING
#define LPO_CAL_ENABLE_MASK MISSING
#define WLAN_SYSTEM_SLEEP_OFFSET MISSING

#define SOC_CHIP_ID_ADDRESS	  MISSING
#define SOC_CHIP_ID_REVISION_MASK MISSING
#define SOC_CHIP_ID_REVISION_LSB  MISSING
#define SOC_CHIP_ID_REVISION_MSB  MISSING

#define FW_IND_EVENT_PENDING MISSING
#define FW_IND_INITIALIZED MISSING

#define MSDU_LINK_EXT_3_TCP_OVER_IPV4_CHECKSUM_EN_MASK MISSING
#define MSDU_LINK_EXT_3_TCP_OVER_IPV6_CHECKSUM_EN_MASK MISSING
#define MSDU_LINK_EXT_3_UDP_OVER_IPV4_CHECKSUM_EN_MASK MISSING
#define MSDU_LINK_EXT_3_UDP_OVER_IPV6_CHECKSUM_EN_MASK MISSING
#define MSDU_LINK_EXT_3_TCP_OVER_IPV4_CHECKSUM_EN_LSB  MISSING
#define MSDU_LINK_EXT_3_TCP_OVER_IPV6_CHECKSUM_EN_LSB  MISSING
#define MSDU_LINK_EXT_3_UDP_OVER_IPV4_CHECKSUM_EN_LSB  MISSING
#define MSDU_LINK_EXT_3_UDP_OVER_IPV6_CHECKSUM_EN_LSB  MISSING

#define SR_WR_INDEX_ADDRESS MISSING
#define DST_WATERMARK_ADDRESS MISSING

#define DST_WR_INDEX_ADDRESS MISSING
#define SRC_WATERMARK_ADDRESS MISSING
#define SRC_WATERMARK_LOW_MASK MISSING
#define SRC_WATERMARK_HIGH_MASK MISSING
#define DST_WATERMARK_LOW_MASK MISSING
#define DST_WATERMARK_HIGH_MASK MISSING
#define CURRENT_SRRI_ADDRESS MISSING
#define CURRENT_DRRI_ADDRESS MISSING
#define HOST_IS_SRC_RING_HIGH_WATERMARK_MASK MISSING
#define HOST_IS_SRC_RING_LOW_WATERMARK_MASK MISSING
#define HOST_IS_DST_RING_HIGH_WATERMARK_MASK MISSING
#define HOST_IS_DST_RING_LOW_WATERMARK_MASK MISSING
#define HOST_IS_ADDRESS MISSING
#define MISC_IS_ADDRESS MISSING
#define HOST_IS_COPY_COMPLETE_MASK MISSING
#define CE_WRAPPER_BASE_ADDRESS MISSING
#define CE_WRAPPER_INTERRUPT_SUMMARY_ADDRESS MISSING
#define CE_DDR_ADDRESS_FOR_RRI_LOW MISSING
#define CE_DDR_ADDRESS_FOR_RRI_HIGH MISSING

#if (defined(WCSS_VERSION) && (WCSS_VERSION >= 72))
#define HOST_IE_ADDRESS UMAC_CE_COMMON_WFSS_CE_COMMON_R0_CE_HOST_IE_0
#define HOST_IE_ADDRESS_2 UMAC_CE_COMMON_WFSS_CE_COMMON_R0_CE_HOST_IE_1
#else /* WCSS_VERSION < 72 */
#define HOST_IE_ADDRESS UMAC_CE_COMMON_CE_HOST_IE_0
#define HOST_IE_ADDRESS_2 UMAC_CE_COMMON_CE_HOST_IE_1
#endif /* WCSS_VERSION */

#define HOST_IE_COPY_COMPLETE_MASK MISSING
#define SR_BA_ADDRESS MISSING
#define SR_BA_ADDRESS_HIGH MISSING
#define SR_SIZE_ADDRESS MISSING
#define CE_CTRL1_ADDRESS MISSING
#define CE_CTRL1_DMAX_LENGTH_MASK MISSING
#define DR_BA_ADDRESS MISSING
#define DR_BA_ADDRESS_HIGH MISSING
#define DR_SIZE_ADDRESS MISSING
#define CE_CMD_REGISTER MISSING
#define CE_MSI_ADDRESS MISSING
#define CE_MSI_ADDRESS_HIGH MISSING
#define CE_MSI_DATA MISSING
#define CE_MSI_ENABLE_BIT MISSING
#define MISC_IE_ADDRESS MISSING
#define MISC_IS_AXI_ERR_MASK MISSING
#define MISC_IS_DST_ADDR_ERR_MASK MISSING
#define MISC_IS_SRC_LEN_ERR_MASK MISSING
#define MISC_IS_DST_MAX_LEN_VIO_MASK MISSING
#define MISC_IS_DST_RING_OVERFLOW_MASK MISSING
#define MISC_IS_SRC_RING_OVERFLOW_MASK MISSING
#define SRC_WATERMARK_LOW_LSB MISSING
#define SRC_WATERMARK_HIGH_LSB MISSING
#define DST_WATERMARK_LOW_LSB MISSING
#define DST_WATERMARK_HIGH_LSB MISSING
#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_MASK MISSING
#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_LSB MISSING
#define CE_CTRL1_DMAX_LENGTH_LSB MISSING
#define CE_CTRL1_SRC_RING_BYTE_SWAP_EN_MASK MISSING
#define CE_CTRL1_DST_RING_BYTE_SWAP_EN_MASK MISSING
#define CE_CTRL1_SRC_RING_BYTE_SWAP_EN_LSB MISSING
#define CE_CTRL1_DST_RING_BYTE_SWAP_EN_LSB MISSING
#define CE_CTRL1_IDX_UPD_EN_MASK MISSING
#define CE_WRAPPER_DEBUG_OFFSET MISSING
#define CE_WRAPPER_DEBUG_SEL_MSB MISSING
#define CE_WRAPPER_DEBUG_SEL_LSB MISSING
#define CE_WRAPPER_DEBUG_SEL_MASK MISSING
#define CE_DEBUG_OFFSET MISSING
#define CE_DEBUG_SEL_MSB MISSING
#define CE_DEBUG_SEL_LSB MISSING
#define CE_DEBUG_SEL_MASK MISSING
#define CE0_BASE_ADDRESS MISSING
#define CE1_BASE_ADDRESS MISSING
#define A_WIFI_APB_3_A_WCMN_APPS_CE_INTR_ENABLES MISSING
#define A_WIFI_APB_3_A_WCMN_APPS_CE_INTR_STATUS MISSING

#define QCC2072_BOARD_DATA_SZ MISSING
#define QCC2072_BOARD_EXT_DATA_SZ MISSING

#define MY_TARGET_DEF QCC2072_TARGETdef
#define MY_HOST_DEF QCC2072_HOSTdef
#define MY_CEREG_DEF QCC2072_CE_TARGETdef
#define MY_TARGET_BOARD_DATA_SZ QCC2072_BOARD_DATA_SZ
#define MY_TARGET_BOARD_EXT_DATA_SZ QCC2072_BOARD_EXT_DATA_SZ
#include "targetdef.h"
#include "hostdef.h"
#else
#include "common_drv.h"
#include "targetdef.h"
#include "hostdef.h"
struct targetdef_s *QCC2072_TARGETdef;
struct hostdef_s *QCC2072_HOSTdef;
#endif /*QCC2072_HEADERS_DEF */
