/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_CPASTOP_HW_H_
#define _CAM_CPASTOP_HW_H_

#include "cam_cpas_api.h"
#include "cam_cpas_hw.h"

/* Camera Hw parts array indices */
#define CAM_CPAS_PART_MAX_FUSE_BITS 8
#define CAM_CPAS_PART_MAX_FUSE_BIT_INFO 2

/**
 * enum cam_camnoc_hw_irq_type - Enum for camnoc error types
 *
 * @CAM_CAMNOC_HW_IRQ_SLAVE_ERROR                  : Each slave port in CAMNOC
 *                                                  (3 QSB ports and 1 QHB port)
 *                                                   has an error logger. The
 *                                                   error observed at any slave
 *                                                   port is logged into the
 *                                                   error logger register and
 *                                                   an IRQ is triggered
 * @CAM_CAMNOC_HW_IRQ_IFE_UBWC_ENCODE_ERROR        : Triggered if any error
 *                                                   detected in the IFE UBWC
 *                                                   encoder instance
 * @CAM_CAMNOC_HW_IRQ_IFE_UBWC_STATS_ENCODE_ERROR  : Triggered if any error
 *                                                   detected in the IFE UBWC-
 *                                                   Stats encoder instance
 * @CAM_CAMNOC_HW_IRQ_IFE_UBWC_STATS_1_ENCODE_ERROR: Triggered if any error
 *                                                   detected in the IFE UBWC-
 *                                                   Stats 1 encoder instance
 * @CAM_CAMNOC_HW_IRQ_IFE02_UBWC_ENCODE_ERROR      : Triggered if any error
 *                                                   detected in the IFE0 UBWC
 *                                                   encoder instance
 * @CAM_CAMNOC_HW_IRQ_IFE13_UBWC_ENCODE_ERROR      : Triggered if any error
 *                                                   detected in the IFE1 or
 *                                                   IFE3 UBWC encoder instance
 * @CAM_CAMNOC_HW_IRQ_IFE0_UBWC_ENCODE_ERROR       : Triggered if any UBWC error
 *                                                   is detected in IFE0 write
 *                                                   path
 * @CAM_CAMNOC_HW_IRQ_IFE1_WRITE_UBWC_ENCODE_ERROR : Triggered if any UBWC error
 *                                                   is detected in IFE1 write
 *                                                   path slave  times out after
 *                                                   4000 AHB cycles
 * @CAM_CAMNOC_HW_IRQ_IPE_UBWC_ENCODE_ERROR        : Triggered if any error
 *                                                   detected in the IPE
 *                                                   UBWC encoder instance
 * @CAM_CAMNOC_HW_IRQ_BPS_UBWC_ENCODE_ERROR        : Triggered if any error
 *                                                   detected in the BPS
 *                                                   UBWC encoder instance
 * @CAM_CAMNOC_HW_IRQ_IPE1_BPS_UBWC_DECODE_ERROR   : Triggered if any error
 *                                                   detected in the IPE1/BPS
 *                                                   read path decoder instance
 * @CAM_CAMNOC_HW_IRQ_IPE0_UBWC_DECODE_ERROR       : Triggered if any error
 *                                                   detected in the IPE0 read
 *                                                   path decoder instance
 * @CAM_CAMNOC_HW_IRQ_IPE1_UBWC_DECODE_ERROR       : Triggered if any error
 *                                                   detected in the IPE1 read
 *                                                   path decoder instance
 * @CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_DECODE_ERROR    : Triggered if any error
 *                                                   detected in the IPE/BPS
 *                                                   UBWC decoder instance
 * @CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_ENCODE_ERROR    : Triggered if any error
 *                                                   detected in the IPE/BPS
 *                                                   UBWC encoder instance
 * @CAM_CAMNOC_HW_IRQ_OFE_UBWC_WRITE_ENCODE_ERROR  : Triggered if any error
 *                                                   detected in the OFE write
 *                                                   path enconder instance
 * @CAM_CAMNOC_HW_IRQ_OFE_UBWC_READ_DECODE_ERROR   : Triggered if any error
 *                                                   detected in the OFE read
 *                                                   path enconder instance
 * @CAM_CAMNOC_HW_IRQ_TFE_UBWC_ENCODE_ERROR        : Triggered if any error
 *                                                   detected in the TFE
 *                                                   UBWC enconder instance
 * @CAM_CAMNOC_HW_IRQ_TFE_UBWC_1_ENCODE_ERROR      : Triggered if any error
 *                                                   detected in the TFE
 *                                                   UBWC enconder instance
 * @CAM_CAMNOC_HW_IRQ_RESERVED1                    : Reserved
 * @CAM_CAMNOC_HW_IRQ_RESERVED2                    : Reserved
 * @CAM_CAMNOC_HW_IRQ_CAMNOC_TEST                  : To test the IRQ logic
 */
enum cam_camnoc_hw_irq_type {
	CAM_CAMNOC_HW_IRQ_SLAVE_ERROR =
		CAM_CAMNOC_IRQ_SLAVE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE_UBWC_STATS_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE_UBWC_STATS_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE_UBWC_STATS_1_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE_UBWC_STATS_1_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE02_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE02_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE13_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE13_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE0_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE0_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IFE1_WRITE_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IFE1_WRITE_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IPE_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_BPS_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_BPS_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE1_BPS_UBWC_DECODE_ERROR =
		CAM_CAMNOC_IRQ_IPE1_BPS_UBWC_DECODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE0_UBWC_DECODE_ERROR =
		CAM_CAMNOC_IRQ_IPE0_UBWC_DECODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE1_UBWC_DECODE_ERROR =
		CAM_CAMNOC_IRQ_IPE1_UBWC_DECODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_DECODE_ERROR =
		CAM_CAMNOC_IRQ_IPE_BPS_UBWC_DECODE_ERROR,
	CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_IPE_BPS_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_OFE_UBWC_WRITE_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_OFE_WR_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_OFE_UBWC_READ_DECODE_ERROR =
		CAM_CAMNOC_IRQ_OFE_RD_UBWC_DECODE_ERROR,
	CAM_CAMNOC_HW_IRQ_TFE_UBWC_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_TFE_UBWC_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_TFE_UBWC_1_ENCODE_ERROR =
		CAM_CAMNOC_IRQ_TFE_UBWC_1_ENCODE_ERROR,
	CAM_CAMNOC_HW_IRQ_AHB_TIMEOUT =
		CAM_CAMNOC_IRQ_AHB_TIMEOUT,
	CAM_CAMNOC_HW_IRQ_RESERVED1,
	CAM_CAMNOC_HW_IRQ_RESERVED2,
	CAM_CAMNOC_HW_IRQ_CAMNOC_TEST,
};

/**
 * enum cam_camnoc_port_type - Enum for different camnoc hw ports. All CAMNOC
 *         settings like QoS, LUT mappings need to be configured for
 *         each of these ports.
 *
 * @CAM_CAMNOC_CDM: Indicates CDM HW connection to camnoc
 * @CAM_CAMNOC_SFE_RD: Indicates read data from all SFEs to cammnoc
 * @CAM_CAMNOC_IFE02: Indicates IFE0, IFE2 HW connection to camnoc
 * @CAM_CAMNOC_IFE13: Indicates IFE1, IFE3 HW connection to camnoc
 * @CAM_CAMNOC_IFE_LITE: Indicates all IFE lites connection to camnoc
 * @CAM_CAMNOC_IFE_LINEAR: Indicates linear data from all IFEs to cammnoc
 * @CAM_CAMNOC_IFE_LINEAR_STATS: Indicates linear and stats data from certan
 *         IFEs to cammnoc
 * @CAM_CAMNOC_IFE_LINEAR_STATS_1: Indicates linear and stats data from certan
 *         IFEs to cammnoc
 * @CAM_CAMNOC_IFE_PDAF: Indicates pdaf data from all IFEs to cammnoc
 * @CAM_CAMNOC_IFE_UBWC: Indicates ubwc from all IFEs to cammnoc
 * @CAM_CAMNOC_IFE_UBWC_STATS: Indicates ubwc+stats from certain IFEs to cammnoc
 * @CAM_CAMNOC_IFE_UBWC_STATS_1: Indicates ubwc+stats from certain
 *         IFEs to cammnoc
 * @CAM_CAMNOC_IFE_RDI_WR: Indicates RDI write data from certain IFEs to cammnoc
 * @CAM_CAMNOC_IFE_RDI_WR_1: Indicates RDI write data from certain
 *         IFEs to cammnoc
 * @CAM_CAMNOC_IFE_RDI_RD: Indicates RDI read data from all IFEs to cammnoc
 * @CAM_CAMNOC_IFE0123_RDI_WRITE: RDI write only for all IFEx
 * @CAM_CAMNOC_IFE0_NRDI_WRITE: IFE0 non-RDI write
 * @CAM_CAMNOC_IFE01_RDI_READ: IFE0/1 RDI READ
 * @CAM_CAMNOC_IFE1_NRDI_WRITE: IFE1 non-RDI write
 * @CAM_CAMNOC_IPE_BPS_LRME_READ: Indicates IPE, BPS, LRME Read HW
 *         connection to camnoc
 * @CAM_CAMNOC_IPE_BPS_LRME_WRITE: Indicates IPE, BPS, LRME Write HW
 *         connection to camnoc
 * @CAM_CAMNOC_IPE_VID_DISP_WRITE: Indicates IPE's VID/DISP Wrire HW
 *         connection to camnoc
 * @CAM_CAMNOC_IPE_WR: Indicates IPE HW's write connection to camnoc
 * @CAM_CAMNOC_IPE0_RD: Indicates IPE's Read0 HW connection to camnoc
 * @CAM_CAMNOC_IPE1_RD: Indicates IPE's Read1 HW connection to camnoc
 * @CAM_CAMNOC_IPE1_BPS_RD: Indicates IPE's Read1 + BPS Read HW connection
 *         to camnoc
 * @CAM_CAMNOC_IPE_BPS_WR: Indicates IPE+BPS Write HW connection to camnoc
 * @CAM_CAMNOC_BPS_WR: Indicates BPS HW's write connection to camnoc
 * @CAM_CAMNOC_BPS_RD: Indicates BPS HW's read connection to camnoc
 * @CAM_CAMNOC_JPEG: Indicates JPEG HW connection to camnoc
 * @CAM_CAMNOC_FD: Indicates FD HW connection to camnoc
 * @CAM_CAMNOC_ICP: Indicates ICP HW connection to camnoc
 * @CAM_CAMNOC_TFE: Indicates TFE0 HW connection to camnoc
 * @CAM_CAMNOC_TFE_1: Indicates TFE1 HW connection to camnoc
 * @CAM_CAMNOC_TFE_2: Indicates TFE2 HW connection to camnoc
 * @CAM_CAMNOC_OPE: Indicates OPE HW connection to camnoc
 */
 /* Deprecated, do not use this anymore. port_name serves the purpose instead of this */
enum cam_camnoc_port_type {
	CAM_CAMNOC_CDM,
	CAM_CAMNOC_SFE_RD,
	CAM_CAMNOC_IFE02,
	CAM_CAMNOC_IFE13,
	CAM_CAMNOC_IFE_LITE,
	CAM_CAMNOC_IFE_LINEAR,
	CAM_CAMNOC_IFE_LINEAR_STATS,
	CAM_CAMNOC_IFE_LINEAR_STATS_1,
	CAM_CAMNOC_IFE_PDAF,
	CAM_CAMNOC_IFE_UBWC,
	CAM_CAMNOC_IFE_UBWC_STATS,
	CAM_CAMNOC_IFE_UBWC_STATS_1,
	CAM_CAMNOC_IFE_RDI_WR,
	CAM_CAMNOC_IFE_RDI_WR_1,
	CAM_CAMNOC_IFE_RDI_RD,
	CAM_CAMNOC_IFE0123_RDI_WRITE,
	CAM_CAMNOC_IFE0_NRDI_WRITE,
	CAM_CAMNOC_IFE01_RDI_READ,
	CAM_CAMNOC_IFE1_NRDI_WRITE,
	CAM_CAMNOC_IPE_BPS_LRME_READ,
	CAM_CAMNOC_IPE_BPS_LRME_WRITE,
	CAM_CAMNOC_IPE_VID_DISP_WRITE,
	CAM_CAMNOC_IPE_WR,
	CAM_CAMNOC_IPE0_RD,
	CAM_CAMNOC_IPE1_RD,
	CAM_CAMNOC_IPE1_BPS_RD,
	CAM_CAMNOC_IPE_BPS_WR,
	CAM_CAMNOC_BPS_WR,
	CAM_CAMNOC_BPS_RD,
	CAM_CAMNOC_JPEG,
	CAM_CAMNOC_FD,
	CAM_CAMNOC_ICP,
	CAM_CAMNOC_TFE_BAYER_STATS,
	CAM_CAMNOC_TFE_BAYER_STATS_1,
	CAM_CAMNOC_TFE_BAYER_STATS_2,
	CAM_CAMNOC_TFE_RAW,
	CAM_CAMNOC_TFE_RAW_1,
	CAM_CAMNOC_TFE_RAW_2,
	CAM_CAMNOC_TFE,
	CAM_CAMNOC_TFE_1,
	CAM_CAMNOC_TFE_2,
	CAM_CAMNOC_OPE,
	CAM_CAMNOC_OPE_BPS_WR,
	CAM_CAMNOC_OPE_BPS_CDM_RD,
	CAM_CAMNOC_CRE,
	CAM_CAMNOC_IFE01234_RDI_WRITE,
	CAM_CAMNOC_IFE01_NRDI_WRITE,
	CAM_CAMNOC_IFE2_NRDI_WRITE,
};

/**
 * struct cam_camnoc_specific : CPAS camnoc specific settings
 *
 * @port_type: Port type
 * @port_name: Port name
 * @enable: Whether to enable settings for this connection
 * @priority_lut_low: Priority Low LUT mapping for this connection
 * @priority_lut_high: Priority High LUT mapping for this connection
 * @urgency: Urgency (QoS) settings for this connection
 * @danger_lut: Danger LUT mapping for this connection
 * @safe_lut: Safe LUT mapping for this connection
 * @ubwc_ctl: UBWC control settings for this connection
 * @qosgen_mainctl: qosgen shaping control configuration for this connection
 * @qosgen_shaping_low: qosgen shaping low configuration for this connection
 * @qosgen_shaping_high: qosgen shaping high configuration for this connection
 * @maxwr_low: maxwr low configuration for this connection
 * @maxrd_low: maxrd low configuration for this connection
 * @dynattr_mainctl: Dynamic attribute main control register for this connection
 *
 */
struct cam_camnoc_specific {
	enum cam_camnoc_port_type port_type;
	const char *port_name;
	bool enable;
	struct cam_cpas_reg priority_lut_low;
	struct cam_cpas_reg priority_lut_high;
	struct cam_cpas_reg urgency;
	struct cam_cpas_reg danger_lut;
	struct cam_cpas_reg safe_lut;
	struct cam_cpas_reg ubwc_ctl;
	struct cam_cpas_reg flag_out_set0_low;
	struct cam_cpas_reg qosgen_mainctl;
	struct cam_cpas_reg qosgen_shaping_low;
	struct cam_cpas_reg qosgen_shaping_high;
	struct cam_cpas_reg maxwr_low;
	struct cam_cpas_reg maxrd_low;
	struct cam_cpas_reg dynattr_mainctl;
};

/**
 * struct cam_camnoc_irq_sbm : Sideband manager settings for all CAMNOC IRQs
 *
 * @sbm_enable: SBM settings for IRQ enable
 * @sbm_status: SBM settings for IRQ status
 * @sbm_clear: SBM settings for IRQ clear
 *
 */
struct cam_camnoc_irq_sbm {
	struct cam_cpas_reg sbm_enable;
	struct cam_cpas_reg sbm_status;
	struct cam_cpas_reg sbm_clear;
};

/**
 * struct cam_camnoc_irq_err : Error settings specific to each CAMNOC IRQ
 *
 * @irq_type: Type of IRQ
 * @enable: Whether to enable error settings for this IRQ
 * @sbm_port: Corresponding SBM port for this IRQ
 * @err_enable: Error enable settings for this IRQ
 * @err_status: Error status settings for this IRQ
 * @err_clear: Error clear settings for this IRQ
 *
 */
struct cam_camnoc_irq_err {
	enum cam_camnoc_hw_irq_type irq_type;
	bool enable;
	uint32_t sbm_port;
	struct cam_cpas_reg err_enable;
	struct cam_cpas_reg err_status;
	struct cam_cpas_reg err_clear;
};

/**
 * struct cam_cpas_hw_errata_wa : Struct for HW errata workaround info
 *
 * @enable: Whether to enable this errata workround
 * @data: HW Errata workaround data
 *
 */
struct cam_cpas_hw_errata_wa {
	bool enable;
	union {
		struct cam_cpas_reg reg_info;
	} data;
};

/**
 * struct cam_cpas_subpart_info : Struct for camera Hw parts info
 *
 * @num_bits: Number of entries in hw_bitmap_mask
 * @hw_bitmap_mask: Contains Fuse flag and hw_map info
 *
 */
struct cam_cpas_subpart_info {
	uint32_t num_bits;
	uint32_t hw_bitmap_mask[CAM_CPAS_PART_MAX_FUSE_BITS][CAM_CPAS_PART_MAX_FUSE_BIT_INFO];
};

/**
 * struct cam_cpas_hw_errata_wa_list : List of HW Errata workaround info
 *
 * @camnoc_flush_slave_pending_trans: Errata workaround info for flushing
 *         camnoc slave pending transactions before turning off CPAS_TOP gdsc
 * @tcsr_camera_hf_sf_ares_glitch: Errata workaround info from ignoring
 *         erroneous signals at camera start
 * @enable_icp_clk_for_qchannel: Need to enable ICP clk while qchannel handshake
 */
struct cam_cpas_hw_errata_wa_list {
	struct cam_cpas_hw_errata_wa camnoc_flush_slave_pending_trans;
	struct cam_cpas_hw_errata_wa tcsr_camera_hf_sf_ares_glitch;
	struct cam_cpas_hw_errata_wa enable_icp_clk_for_qchannel;
};

/**
 * struct cam_camnoc_err_logger_info : CAMNOC error logger register offsets
 *
 * @mainctrl: Register offset for mainctrl
 * @errvld: Register offset for errvld
 * @errlog0_low: Register offset for errlog0_low
 * @errlog0_high: Register offset for errlog0_high
 * @errlog1_low: Register offset for errlog1_low
 * @errlog1_high: Register offset for errlog1_high
 * @errlog2_low: Register offset for errlog2_low
 * @errlog2_high: Register offset for errlog2_high
 * @errlog3_low: Register offset for errlog3_low
 * @errlog3_high: Register offset for errlog3_high
 *
 */
struct cam_camnoc_err_logger_info {
	uint32_t mainctrl;
	uint32_t errvld;
	uint32_t errlog0_low;
	uint32_t errlog0_high;
	uint32_t errlog1_low;
	uint32_t errlog1_high;
	uint32_t errlog2_low;
	uint32_t errlog2_high;
	uint32_t errlog3_low;
	uint32_t errlog3_high;
};

/**
 * struct cam_cpas_test_irq_info : CAMNOC Test IRQ mask information
 *
 * @sbm_enable_mask: sbm mask to enable camnoc test irq
 * @sbm_clear_mask: sbm mask to clear camnoc test irq
 *
 */
struct cam_cpas_test_irq_info {
	uint32_t sbm_enable_mask;
	uint32_t sbm_clear_mask;
};

/**
 * struct cam_cpas_cesta_crm_type : CESTA crm type information
 *
 * @CAM_CESTA_CRMB: CRM for bandwidth
 * @CAM_CESTA_CRMC: CRM for clocks
 *
 */
enum cam_cpas_cesta_crm_type {
	CAM_CESTA_CRMB = 0,
	CAM_CESTA_CRMC,
};

/**
 * struct cam_vcd_info : cpas vcd(virtual clk domain) information
 *
 * @vcd_index: vcd number of each clk
 * @type: type of clk domain CESTA_CRMB, CESTA_CRMC
 * @clk_name: name of each vcd clk, exmp: cam_cc_ife_0_clk_src
 *
 */
struct cam_cpas_vcd_info {
	uint8_t index;
	enum cam_cpas_cesta_crm_type type;
	const char *clk;
};

/**
 * struct cam_cpas_cesta_vcd_curr_lvl : cesta vcd operating level information
 *
 * @reg_offset: register offset
 * @vcd_base_inc: each vcd base addr offset
 * @num_vcds: number of vcds
 *
 */
struct cam_cpas_cesta_vcd_curr_lvl {
	uint32_t reg_offset;
	uint32_t vcd_base_inc;
	uint8_t num_vcds;
};

/**
 * struct cam_cpas_cesta_vcd_reg_info : to hold all cesta register information
 *
 * @vcd_currol: vcd current perf level reg info
 *
 */
struct cam_cpas_cesta_vcd_reg_info {
	struct cam_cpas_cesta_vcd_curr_lvl vcd_currol;
};

/**
 * struct cam_cpas_cesta_info : to hold all cesta register information
 *
 * @vcd_info: vcd info
 * @num_vcds: number of vcds
 * @cesta_reg_info: cesta vcds reg info
 *
 */
struct cam_cpas_cesta_info {
	struct cam_cpas_vcd_info *vcd_info;
	int num_vcds;
	struct cam_cpas_cesta_vcd_reg_info *cesta_reg_info;
};

/**
 * struct cam_cpas_hw_cap_info : CPAS Hardware capability information
 *
 * @num_caps_registers: number of hw capability registers
 * @hw_caps_offsets: array of hw cap register offsets
 *
 */
struct cam_cpas_hw_cap_info {
	uint32_t num_caps_registers;
	uint32_t hw_caps_offsets[CAM_CPAS_MAX_CAPS_REGS];
};

/**
 * struct cam_camnoc_addr_trans_client_reg_info : CPAS Address translator supported client
 *                                                register information
 *
 * @client_name: Name of the client
 * @reg_enable: Register offset to enable address translator
 * @reg_offset0: Register offset for offset 0
 * @reg_base1: Register offset for base 1
 * @reg_offset1: Register offset for offset 1
 * @reg_base2: Register offset for base 2
 * @reg_offset2: Register offset for offset 2
 * @reg_base3: Register offset for base 3
 * @reg_offset3: Register offset for offset 3
 *
 */
struct cam_camnoc_addr_trans_client_info {
	const char *client_name;
	uint32_t reg_enable;
	uint32_t reg_offset0;
	uint32_t reg_base1;
	uint32_t reg_offset1;
	uint32_t reg_base2;
	uint32_t reg_offset2;
	uint32_t reg_base3;
	uint32_t reg_offset3;
};

/**
 * struct cam_camnoc_addr_trans_info : CPAS Address translator generic info
 *
 * @num_supported_clients: Number of clients that support address translator
 * @addr_trans_client_info: Client information with address translator supported
 *
 */
struct cam_camnoc_addr_trans_info {
	uint8_t num_supported_clients;
	struct cam_camnoc_addr_trans_client_info *addr_trans_client_info;
};

/**
 * struct cam_camnoc_info : Overall CAMNOC settings info
 *
 * @camnoc_type: type of camnoc (RT/NRT/COMBINED)
 * @camnoc_name: name of camnoc (CAMNOC_RT/CAMNOC_NRT/CAMNOC_COMBINED)
 * @reg_base: register base for camnoc RT/NRT/COMBINED register space
 * @specific: Pointer to CAMNOC SPECIFICTONTTPTR settings
 * @specific_size: Array size of SPECIFICTONTTPTR settings
 * @irq_sbm: Pointer to CAMNOC IRQ SBM settings
 * @irq_err: Pointer to CAMNOC IRQ Error settings
 * @irq_err_size: Array size of IRQ Error settings
 * @err_logger: Pointer to CAMNOC IRQ Error logger read registers
 * @errata_wa_list: HW Errata workaround info
 * @test_irq_info: CAMNOC Test IRQ info
 * @cesta_info: cpas cesta reg info
 * @addr_trans_info: CAMNOC address translator info
 *
 */
struct cam_camnoc_info {
	/* Below fields populated at probe on camera version */
	enum cam_camnoc_hw_type camnoc_type;
	char *camnoc_name;
	enum cam_cpas_reg_base reg_base;

	/* Below fields populated from the cpas header */
	struct cam_camnoc_specific *specific;
	int specific_size;
	struct cam_camnoc_irq_sbm *irq_sbm;
	struct cam_camnoc_irq_err *irq_err;
	int irq_err_size;
	struct cam_camnoc_err_logger_info *err_logger;
	struct cam_cpas_hw_errata_wa_list *errata_wa_list;
	struct cam_cpas_test_irq_info test_irq_info;
	struct cam_cpas_cesta_info *cesta_info;
	struct cam_camnoc_addr_trans_info *addr_trans_info;
};

/**
 * struct cam_cpas_work_payload : Struct for cpas work payload data
 *
 * @camnoc_idx: index to camnoc info array
 * @hw: Pointer to HW info
 * @irq_status: IRQ status value
 * @irq_data: IRQ data
 * @workq_scheduled_ts: workqueue scheduled timestamp
 * @work: Work handle
 *
 */
struct cam_cpas_work_payload {
	int8_t camnoc_idx;
	struct cam_hw_info *hw;
	uint32_t irq_status;
	uint32_t irq_data;
	ktime_t workq_scheduled_ts;
	struct work_struct work;
};

/**
 * struct cam_cpas_camnoc_qchannel : Cpas camnoc qchannel info
 *
 * @qchannel_ctrl: offset to configure to control camnoc qchannel interface
 * @qchannel_status: offset to read camnoc qchannel interface status
 *
 */
struct cam_cpas_camnoc_qchannel {
	uint32_t qchannel_ctrl;
	uint32_t qchannel_status;
};

/**
 * struct cam_cpas_secure_info : Cpas secure info
 *
 * @secure_access_ctrl_offset: Offset value of secure access ctrl register
 * @seurec_access_ctrl_value:  Value to set for core secure access
 *
 */
struct cam_cpas_secure_info {
	uint32_t secure_access_ctrl_offset;
	uint32_t secure_access_ctrl_value;
};

/**
 * struct cam_cpas_info: CPAS information
 *
 * @qchannel_info: CPAS qchannel info
 * @hw_cap_info: CPAS Hardware capability information
 * @hw_caps_secure_info: CPAS Hardware secure information
 * @num_qchannel: Number of qchannel
 */
struct cam_cpas_info {
	struct cam_cpas_camnoc_qchannel *qchannel_info[CAM_CAMNOC_QCHANNEL_MAX];
	struct cam_cpas_hw_cap_info hw_caps_info;
	uint8_t num_qchannel;
	struct cam_cpas_secure_info *hw_caps_secure_info;
};

/**
 * struct cam_cpas_top_regs : CPAS Top registers
 * @tpg_mux_sel_shift:     TPG mux select shift value
 * @tpg_mux_sel:           For selecting TPG
 * @tpg_mux_sel_enabled:   TPG mux select enabled or not
 *
 */
struct cam_cpas_top_regs {
	uint32_t tpg_mux_sel_shift;
	uint32_t tpg_mux_sel;
	bool     tpg_mux_sel_enabled;
};

#endif /* _CAM_CPASTOP_HW_H_ */
