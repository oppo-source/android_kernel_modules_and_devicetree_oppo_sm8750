/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

#include "tcl_data_cmd.h"
#include "phyrx_rssi_legacy.h"
#include "hal_be_hw_headers.h"
#include "hal_internal.h"
#include "cdp_txrx_mon_struct.h"
#include "qdf_trace.h"
#include "hal_rx.h"
#include "hal_tx.h"
#include "dp_types.h"
#include "hal_api_mon.h"

#define DSCP_TID_TABLE_SIZE 24
#define NUM_WORDS_PER_DSCP_TID_TABLE (DSCP_TID_TABLE_SIZE / 4)

/**
 * hal_tx_set_dscp_tid_map_2072() - Configure default DSCP to TID map table
 * @hal_soc: HAL SoC context
 * @map: DSCP-TID mapping table
 * @id: mapping table ID - 0-31
 *
 * DSCP are mapped to 8 TID values using TID values programmed
 * in any of the 32 DSCP_TID_MAPS (id = 0-31).
 *
 * Return: none
 */
static void hal_tx_set_dscp_tid_map_2072(struct hal_soc *hal_soc, uint8_t *map,
					 uint8_t id)
{
	int i;
	uint32_t addr, cmn_reg_addr;
	uint32_t value = 0, regval;
	uint8_t val[DSCP_TID_TABLE_SIZE], cnt = 0;

	struct hal_soc *soc = (struct hal_soc *)hal_soc;

	if (id >= HAL_MAX_HW_DSCP_TID_MAPS_11AX)
		return;

	cmn_reg_addr = HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_ADDR(
					MAC_TCL_REG_REG_BASE);

	addr = HWIO_TCL_R0_DSCP_TID_MAP_n_ADDR(
				MAC_TCL_REG_REG_BASE,
				id * NUM_WORDS_PER_DSCP_TID_TABLE);

	/* Enable read/write access */
	regval = HAL_REG_READ(soc, cmn_reg_addr);
	regval |=
	    (1 <<
	    HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_DSCP_TID_MAP_PROGRAM_EN_SHFT);

	HAL_REG_WRITE(soc, cmn_reg_addr, regval);

	/* Write 8 (24 bits) DSCP-TID mappings in each iteration */
	for (i = 0; i < 64; i += 8) {
		value = (map[i] |
			(map[i + 1] << 0x3) |
			(map[i + 2] << 0x6) |
			(map[i + 3] << 0x9) |
			(map[i + 4] << 0xc) |
			(map[i + 5] << 0xf) |
			(map[i + 6] << 0x12) |
			(map[i + 7] << 0x15));

		qdf_mem_copy(&val[cnt], (void *)&value, 3);
		cnt += 3;
	}

	for (i = 0; i < DSCP_TID_TABLE_SIZE; i += 4) {
		regval = *(uint32_t *)(val + i);
		HAL_REG_WRITE(soc, addr,
			      (regval & HWIO_TCL_R0_DSCP_TID_MAP_n_RMSK));
		addr += 4;
	}

	/* Disable read/write access */
	regval = HAL_REG_READ(soc, cmn_reg_addr);
	regval &=
	~(HWIO_TCL_R0_CONS_RING_CMN_CTRL_REG_DSCP_TID_MAP_PROGRAM_EN_BMSK);

	HAL_REG_WRITE(soc, cmn_reg_addr, regval);
}

/**
 * hal_tx_update_dscp_tid_2072() - Update the dscp tid map table as updated
 *					by the user
 * @hal_soc: HAL SoC context
 * @tid: TID
 * @id : MAP ID
 * @dscp: DSCP_TID map index
 *
 * Return: void
 */
static void hal_tx_update_dscp_tid_2072(struct hal_soc *hal_soc, uint8_t tid,
					uint8_t id, uint8_t dscp)
{
	int index;
	uint32_t addr;
	uint32_t value;
	uint32_t regval;
	struct hal_soc *soc = (struct hal_soc *)hal_soc;

	addr = HWIO_TCL_R0_DSCP_TID_MAP_n_ADDR(
			MAC_TCL_REG_REG_BASE, id);

	index = dscp % HAL_TX_NUM_DSCP_PER_REGISTER;
	addr += 4 * (dscp / HAL_TX_NUM_DSCP_PER_REGISTER);
	value = tid << (HAL_TX_BITS_PER_TID * index);

	regval = HAL_REG_READ(soc, addr);
	regval &= ~(HAL_TX_TID_BITS_MASK << (HAL_TX_BITS_PER_TID * index));
	regval |= value;

	HAL_REG_WRITE(soc, addr, (regval & HWIO_TCL_R0_DSCP_TID_MAP_n_RMSK));
}

/**
 * hal_tx_init_cmd_credit_ring_2072() - Initialize command/credit SRNG
 * @hal_soc_hdl: Handle to HAL SoC structure
 * @hal_ring_hdl: Handle to HAL SRNG structure
 *
 * Return: none
 */
static inline void
hal_tx_init_cmd_credit_ring_2072(hal_soc_handle_t hal_soc_hdl,
				 hal_ring_handle_t hal_ring_hdl)
{
}

#ifdef DP_TX_IMPLICIT_RBM_MAPPING

#define RBM_MAPPING_BMSK HWIO_TCL_R0_RBM_MAPPING0_SW2TCL1_RING_BMSK
#define RBM_MAPPING_SHFT HWIO_TCL_R0_RBM_MAPPING0_SW2TCL2_RING_SHFT

#define RBM_PPE2TCL_OFFSET \
			(HWIO_TCL_R0_RBM_MAPPING0_PPE2TCL1_RING_SHFT >> 2)
#define RBM_TCL_CMD_CREDIT_OFFSET \
			(HWIO_TCL_R0_RBM_MAPPING0_SW2TCL_CREDIT_RING_SHFT >> 2)

/**
 * hal_tx_config_rbm_mapping_be_2072() - Update return buffer manager ring id
 * @hal_soc_hdl: HAL SoC context
 * @hal_ring_hdl: Source ring pointer
 * @rbm_id: return buffer manager ring id
 *
 * Return: void
 */
static inline void
hal_tx_config_rbm_mapping_be_2072(hal_soc_handle_t hal_soc_hdl,
				  hal_ring_handle_t hal_ring_hdl,
				  uint8_t rbm_id)
{
	struct hal_srng *srng = (struct hal_srng *)hal_ring_hdl;
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr = 0;
	uint32_t reg_val = 0;
	uint32_t val = 0;
	uint8_t ring_num;
	enum hal_ring_type ring_type;

	ring_type = srng->ring_type;
	ring_num = hal_soc->hw_srng_table[ring_type].start_ring_id;
	ring_num = srng->ring_id - ring_num;

	reg_addr = HWIO_TCL_R0_RBM_MAPPING0_ADDR(MAC_TCL_REG_REG_BASE);

	if (ring_type == PPE2TCL)
		ring_num = ring_num + RBM_PPE2TCL_OFFSET;
	else if (ring_type == TCL_CMD_CREDIT)
		ring_num = ring_num + RBM_TCL_CMD_CREDIT_OFFSET;

	/* get current value stored in register address */
	val = HAL_REG_READ(hal_soc, reg_addr);

	/* mask out other stored value */
	val &= (~(RBM_MAPPING_BMSK << (RBM_MAPPING_SHFT * ring_num)));

	reg_val = val | ((RBM_MAPPING_BMSK & rbm_id) <<
			 (RBM_MAPPING_SHFT * ring_num));

	/* write rbm mapped value to register address */
	HAL_REG_WRITE(hal_soc, reg_addr, reg_val);
}
#else
static inline void
hal_tx_config_rbm_mapping_be_2072(hal_soc_handle_t hal_soc_hdl,
				  hal_ring_handle_t hal_ring_hdl,
				  uint8_t rbm_id)
{
}
#endif
