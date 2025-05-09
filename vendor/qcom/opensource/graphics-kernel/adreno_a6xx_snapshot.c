// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_snapshot.h"

#define A6XX_NUM_CTXTS 2
#define A6XX_NUM_AXI_ARB_BLOCKS 2
#define A6XX_NUM_XIN_AXI_BLOCKS 5
#define A6XX_NUM_XIN_CORE_BLOCKS 4

static const unsigned int a6xx_gras_cluster[] = {
	0x8000, 0x8006, 0x8010, 0x8092, 0x8094, 0x809D, 0x80A0, 0x80A6,
	0x80AF, 0x80F1, 0x8100, 0x8107, 0x8109, 0x8109, 0x8110, 0x8110,
	0x8400, 0x840B,
};

static const unsigned int a6xx_ps_cluster_rac[] = {
	0x8800, 0x8806, 0x8809, 0x8811, 0x8818, 0x881E, 0x8820, 0x8865,
	0x8870, 0x8879, 0x8880, 0x8889, 0x8890, 0x8891, 0x8898, 0x8898,
	0x88C0, 0x88C1, 0x88D0, 0x88E3, 0x8900, 0x890C, 0x890F, 0x891A,
	0x8C00, 0x8C01, 0x8C08, 0x8C10, 0x8C17, 0x8C1F, 0x8C26, 0x8C33,
};

static const unsigned int a6xx_ps_cluster_rbp[] = {
	0x88F0, 0x88F3, 0x890D, 0x890E, 0x8927, 0x8928, 0x8BF0, 0x8BF1,
	0x8C02, 0x8C07, 0x8C11, 0x8C16, 0x8C20, 0x8C25,
};

static const unsigned int a6xx_vpc_ps_cluster[] = {
	0x9200, 0x9216, 0x9218, 0x9236, 0x9300, 0x9306,
};

static const unsigned int a6xx_fe_cluster[] = {
	0x9300, 0x9306, 0x9800, 0x9806, 0x9B00, 0x9B07, 0xA000, 0xA009,
	0xA00E, 0xA0EF, 0xA0F8, 0xA0F8,
};

static const unsigned int a660_fe_cluster[] = {
	0x9807, 0x9807,
};

static const unsigned int a6xx_pc_vs_cluster[] = {
	0x9100, 0x9108, 0x9300, 0x9306, 0x9980, 0x9981, 0x9B00, 0x9B07,
};

static const unsigned int a650_isense_registers[] = {
	0x22C00, 0x22C19, 0x22C26, 0x22C2D, 0x22C2F, 0x22C36, 0x22C40, 0x22C44,
	0x22C50, 0x22C57, 0x22C60, 0x22C67, 0x22C80, 0x22C87, 0x22D25, 0x22D2A,
	0x22D2C, 0x22D32, 0x22D3E, 0x22D3F, 0x22D4E, 0x22D55, 0x22D58, 0x22D60,
	0x22D64, 0x22D64, 0x22D66, 0x22D66, 0x22D68, 0x22D6B, 0x22D6E, 0x22D76,
	0x22D78, 0x22D78, 0x22D80, 0x22D87, 0x22D90, 0x22D97, 0x22DA0, 0x22DA0,
	0x22DB0, 0x22DB7, 0x22DC0, 0x22DC2, 0x22DC4, 0x22DE3, 0x2301A, 0x2301A,
	0x2301D, 0x2302A, 0x23120, 0x23121, 0x23133, 0x23133, 0x23156, 0x23157,
	0x23165, 0x23165, 0x2316D, 0x2316D, 0x23180, 0x23191,
};

static const struct sel_reg {
	unsigned int host_reg;
	unsigned int cd_reg;
	unsigned int val;
} _a6xx_rb_rac_aperture = {
	.host_reg = A6XX_RB_RB_SUB_BLOCK_SEL_CNTL_HOST,
	.cd_reg = A6XX_RB_RB_SUB_BLOCK_SEL_CNTL_CD,
	.val = 0x0,
},
_a6xx_rb_rbp_aperture = {
	.host_reg = A6XX_RB_RB_SUB_BLOCK_SEL_CNTL_HOST,
	.cd_reg = A6XX_RB_RB_SUB_BLOCK_SEL_CNTL_CD,
	.val = 0x9,
};

static struct a6xx_cluster_registers {
	unsigned int id;
	const unsigned int *regs;
	unsigned int num_sets;
	const struct sel_reg *sel;
	unsigned int offset0;
	unsigned int offset1;
} a6xx_clusters[] = {
	{ CP_CLUSTER_GRAS, a6xx_gras_cluster, ARRAY_SIZE(a6xx_gras_cluster)/2,
		NULL },
	{ CP_CLUSTER_PS, a6xx_ps_cluster_rac, ARRAY_SIZE(a6xx_ps_cluster_rac)/2,
		&_a6xx_rb_rac_aperture },
	{ CP_CLUSTER_PS, a6xx_ps_cluster_rbp, ARRAY_SIZE(a6xx_ps_cluster_rbp)/2,
		&_a6xx_rb_rbp_aperture },
	{ CP_CLUSTER_PS, a6xx_vpc_ps_cluster, ARRAY_SIZE(a6xx_vpc_ps_cluster)/2,
		NULL },
	{ CP_CLUSTER_FE, a6xx_fe_cluster, ARRAY_SIZE(a6xx_fe_cluster)/2,
		NULL },
	{ CP_CLUSTER_PC_VS, a6xx_pc_vs_cluster,
		ARRAY_SIZE(a6xx_pc_vs_cluster)/2, NULL },
	{ CP_CLUSTER_FE, a660_fe_cluster, ARRAY_SIZE(a660_fe_cluster)/2,
		NULL },
};

struct a6xx_cluster_regs_info {
	struct a6xx_cluster_registers *cluster;
	unsigned int ctxt_id;
};

static const unsigned int a6xx_sp_vs_hlsq_cluster[] = {
	0xB800, 0xB803, 0xB820, 0xB822,
};

static const unsigned int a6xx_sp_vs_sp_cluster[] = {
	0xA800, 0xA824, 0xA830, 0xA83C, 0xA840, 0xA864, 0xA870, 0xA895,
	0xA8A0, 0xA8AF, 0xA8C0, 0xA8C3,
};

static const unsigned int a6xx_hlsq_duplicate_cluster[] = {
	0xBB10, 0xBB11, 0xBB20, 0xBB29,
};

static const unsigned int a6xx_sp_duplicate_cluster[] = {
	0xAB00, 0xAB00, 0xAB04, 0xAB05, 0xAB10, 0xAB1B, 0xAB20, 0xAB20,
};

static const unsigned int a6xx_tp_duplicate_cluster[] = {
	0xB300, 0xB307, 0xB309, 0xB309, 0xB380, 0xB382,
};

static const unsigned int a6xx_sp_ps_hlsq_cluster[] = {
	0xB980, 0xB980, 0xB982, 0xB987, 0xB990, 0xB99B, 0xB9A0, 0xB9A2,
	0xB9C0, 0xB9C9,
};

static const unsigned int a6xx_sp_ps_sp_cluster[] = {
	0xA980, 0xA9A8, 0xA9B0, 0xA9BC, 0xA9D0, 0xA9D3, 0xA9E0, 0xA9F3,
	0xAA00, 0xAA00, 0xAA30, 0xAA31, 0xAAF2, 0xAAF2,
};

static const unsigned int a6xx_sp_ps_sp_2d_cluster[] = {
	0xACC0, 0xACC0,
};

static const unsigned int a6xx_sp_ps_tp_cluster[] = {
	0xB180, 0xB183, 0xB190, 0xB191,
};

static const unsigned int a6xx_sp_ps_tp_2d_cluster[] = {
	0xB4C0, 0xB4D1,
};

static struct a6xx_cluster_dbgahb_registers {
	unsigned int id;
	unsigned int regbase;
	unsigned int statetype;
	const unsigned int *regs;
	unsigned int num_sets;
	unsigned int offset0;
	unsigned int offset1;
} a6xx_dbgahb_ctx_clusters[] = {
	{ CP_CLUSTER_SP_VS, 0x0002E000, 0x41, a6xx_sp_vs_hlsq_cluster,
		ARRAY_SIZE(a6xx_sp_vs_hlsq_cluster) / 2 },
	{ CP_CLUSTER_SP_VS, 0x0002A000, 0x21, a6xx_sp_vs_sp_cluster,
		ARRAY_SIZE(a6xx_sp_vs_sp_cluster) / 2 },
	{ CP_CLUSTER_SP_VS, 0x0002E000, 0x41, a6xx_hlsq_duplicate_cluster,
		ARRAY_SIZE(a6xx_hlsq_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_VS, 0x0002A000, 0x21, a6xx_sp_duplicate_cluster,
		ARRAY_SIZE(a6xx_sp_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_VS, 0x0002C000, 0x1, a6xx_tp_duplicate_cluster,
		ARRAY_SIZE(a6xx_tp_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002E000, 0x42, a6xx_sp_ps_hlsq_cluster,
		ARRAY_SIZE(a6xx_sp_ps_hlsq_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002A000, 0x22, a6xx_sp_ps_sp_cluster,
		ARRAY_SIZE(a6xx_sp_ps_sp_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002B000, 0x26, a6xx_sp_ps_sp_2d_cluster,
		ARRAY_SIZE(a6xx_sp_ps_sp_2d_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002C000, 0x2, a6xx_sp_ps_tp_cluster,
		ARRAY_SIZE(a6xx_sp_ps_tp_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002D000, 0x6, a6xx_sp_ps_tp_2d_cluster,
		ARRAY_SIZE(a6xx_sp_ps_tp_2d_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002E000, 0x42, a6xx_hlsq_duplicate_cluster,
		ARRAY_SIZE(a6xx_hlsq_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002A000, 0x22, a6xx_sp_duplicate_cluster,
		ARRAY_SIZE(a6xx_sp_duplicate_cluster) / 2 },
	{ CP_CLUSTER_SP_PS, 0x0002C000, 0x2, a6xx_tp_duplicate_cluster,
		ARRAY_SIZE(a6xx_tp_duplicate_cluster) / 2 },
};

struct a6xx_cluster_dbgahb_regs_info {
	struct a6xx_cluster_dbgahb_registers *cluster;
	unsigned int ctxt_id;
};

static const unsigned int a6xx_hlsq_non_ctx_registers[] = {
	0xBE00, 0xBE01, 0xBE04, 0xBE05, 0xBE08, 0xBE09, 0xBE10, 0xBE15,
	0xBE20, 0xBE23,
};

static const unsigned int a6xx_sp_non_ctx_registers[] = {
	0xAE00, 0xAE04, 0xAE0C, 0xAE0C, 0xAE0F, 0xAE2B, 0xAE30, 0xAE32,
	0xAE35, 0xAE35, 0xAE3A, 0xAE3F, 0xAE50, 0xAE52,
};

static const unsigned int a6xx_tp_non_ctx_registers[] = {
	0xB600, 0xB601, 0xB604, 0xB605, 0xB610, 0xB61B, 0xB620, 0xB623,
};

static struct a6xx_non_ctx_dbgahb_registers {
	unsigned int regbase;
	unsigned int statetype;
	const unsigned int *regs;
	unsigned int num_sets;
	unsigned int offset;
} a6xx_non_ctx_dbgahb[] = {
	{ 0x0002F800, 0x40, a6xx_hlsq_non_ctx_registers,
		ARRAY_SIZE(a6xx_hlsq_non_ctx_registers) / 2 },
	{ 0x0002B800, 0x20, a6xx_sp_non_ctx_registers,
		ARRAY_SIZE(a6xx_sp_non_ctx_registers) / 2 },
	{ 0x0002D800, 0x0, a6xx_tp_non_ctx_registers,
		ARRAY_SIZE(a6xx_tp_non_ctx_registers) / 2 },
};

static const unsigned int a6xx_vbif_registers[] = {
	/* VBIF */
	0x3000, 0x3007, 0x300C, 0x3014, 0x3018, 0x302D, 0x3030, 0x3031,
	0x3034, 0x3036, 0x303C, 0x303D, 0x3040, 0x3040, 0x3042, 0x3042,
	0x3049, 0x3049, 0x3058, 0x3058, 0x305A, 0x3061, 0x3064, 0x3068,
	0x306C, 0x306D, 0x3080, 0x3088, 0x308B, 0x308C, 0x3090, 0x3094,
	0x3098, 0x3098, 0x309C, 0x309C, 0x30C0, 0x30C0, 0x30C8, 0x30C8,
	0x30D0, 0x30D0, 0x30D8, 0x30D8, 0x30E0, 0x30E0, 0x3100, 0x3100,
	0x3108, 0x3108, 0x3110, 0x3110, 0x3118, 0x3118, 0x3120, 0x3120,
	0x3124, 0x3125, 0x3129, 0x3129, 0x3131, 0x3131, 0x3154, 0x3154,
	0x3156, 0x3156, 0x3158, 0x3158, 0x315A, 0x315A, 0x315C, 0x315C,
	0x315E, 0x315E, 0x3160, 0x3160, 0x3162, 0x3162, 0x340C, 0x340C,
	0x3410, 0x3410, 0x3800, 0x3801,
};

static const unsigned int a6xx_gbif_registers[] = {
	/* GBIF */
	0x3C00, 0X3C0B, 0X3C40, 0X3C47, 0X3CC0, 0X3CD1, 0xE3A, 0xE3A,
};

static const unsigned int a6xx_gbif_reinit_registers[] = {
	/* GBIF with REINIT */
	0x3C00, 0X3C0B, 0X3C40, 0X3C47, 0X3C49, 0X3C4A, 0X3CC0, 0X3CD1,
	0xE3A, 0xE3A, 0x0016, 0x0017,
};

static const unsigned int a6xx_rb_rac_registers[] = {
	0x8E04, 0x8E05, 0x8E07, 0x8E08, 0x8E10, 0x8E1C, 0x8E20, 0x8E25,
	0x8E28, 0x8E28, 0x8E2C, 0x8E2F, 0x8E50, 0x8E52,
};

static const unsigned int a6xx_rb_rbp_registers[] = {
	0x8E01, 0x8E01, 0x8E0C, 0x8E0C, 0x8E3B, 0x8E3E, 0x8E40, 0x8E43,
	0x8E53, 0x8E5F, 0x8E70, 0x8E77,
};

/*
 * Set of registers to dump for A6XX on snapshot.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */

static const unsigned int a6xx_registers[] = {
	/* RBBM */
	0x0000, 0x0002, 0x0010, 0x0010, 0x0012, 0x0012, 0x0018, 0x001B,
	0x001e, 0x0032, 0x0038, 0x003C, 0x0042, 0x0042, 0x0044, 0x0044,
	0x0047, 0x0047, 0x0056, 0x0056, 0x00AD, 0x00AE, 0x00B0, 0x00FB,
	0x0100, 0x011D, 0x0200, 0x020D, 0x0218, 0x023D, 0x0400, 0x04F9,
	0x0500, 0x0500, 0x0505, 0x050B, 0x050E, 0x0511, 0x0533, 0x0533,
	0x0540, 0x0555,
	/* CP */
	0x0800, 0x0803, 0x0806, 0x0808, 0x0810, 0x0813, 0x0820, 0x0821,
	0x0823, 0x0824, 0x0826, 0x0827, 0x0830, 0x0833, 0x0840, 0x0845,
	0x084F, 0x086F, 0x0880, 0x088A, 0x08A0, 0x08AB, 0x08C0, 0x08C4,
	0x08D0, 0x08DD, 0x08F0, 0x08F3, 0x0900, 0x0903, 0x0908, 0x0911,
	0x0928, 0x093E, 0x0942, 0x094D, 0x0980, 0x0984, 0x098D, 0x0996,
	0x0998, 0x099E, 0x09A0, 0x09A6, 0x09A8, 0x09AE, 0x09B0, 0x09B1,
	0x09C2, 0x09C8, 0x0A00, 0x0A03,
	/* VSC */
	0x0C00, 0x0C04, 0x0C06, 0x0C06, 0x0C10, 0x0CD9, 0x0E00, 0x0E0E,
	/* UCHE */
	0x0E10, 0x0E13, 0x0E17, 0x0E19, 0x0E1C, 0x0E2B, 0x0E30, 0x0E32,
	0x0E38, 0x0E39,
	/* GRAS */
	0x8600, 0x8601, 0x8610, 0x861B, 0x8620, 0x8620, 0x8628, 0x862B,
	0x8630, 0x8637,
	/* VPC */
	0x9600, 0x9604, 0x9624, 0x9637,
	/* PC */
	0x9E00, 0x9E01, 0x9E03, 0x9E0E, 0x9E11, 0x9E16, 0x9E19, 0x9E19,
	0x9E1C, 0x9E1C, 0x9E20, 0x9E23, 0x9E30, 0x9E31, 0x9E34, 0x9E34,
	0x9E70, 0x9E72, 0x9E78, 0x9E79, 0x9E80, 0x9FFF,
	/* VFD */
	0xA600, 0xA601, 0xA603, 0xA603, 0xA60A, 0xA60A, 0xA610, 0xA617,
	0xA630, 0xA630,
	/* HLSQ */
	0xD002, 0xD003,
};

static const unsigned int a660_registers[] = {
	/* UCHE */
	0x0E3C, 0x0E3C,
	/* LPAC RBBM */
	0x05FC, 0x05FF,
	/* LPAC CP */
	0x0B00, 0x0B40, 0x0B80, 0x0B83,
};

/*
 * Set of registers to dump for A6XX before actually triggering crash dumper.
 * Registers in pairs - first value is the start offset, second
 * is the stop offset (inclusive)
 */
static const unsigned int a6xx_pre_crashdumper_registers[] = {
	/* RBBM: RBBM_STATUS - RBBM_STATUS3 */
	0x210, 0x213,
	/* CP: CP_STATUS_1 */
	0x825, 0x825,
};

static const unsigned int a6xx_gmu_wrapper_registers[] = {
	/* GMU CX */
	0x1f840, 0x1f840, 0x1f844, 0x1f845, 0x1f887, 0x1f889, 0x1f8d0, 0x1f8d0,
	/* GMU AO*/
	0x23b0C, 0x23b0E, 0x23b15, 0x23b15,
};

static const unsigned int a6xx_holi_gmu_wrapper_registers[] = {
	/* GMU SPTPRAC */
	0x1a880, 0x1a881,
	/* GMU CX */
	0x1f840, 0x1f840, 0x1f844, 0x1f845, 0x1f887, 0x1f889, 0x1f8d0, 0x1f8d0,
	/* GMU AO*/
	0x23b0c, 0x23b0e, 0x23b15, 0x23b15,
};

enum a6xx_debugbus_id {
	A6XX_DBGBUS_CP           = 0x1,
	A6XX_DBGBUS_RBBM         = 0x2,
	A6XX_DBGBUS_VBIF         = 0x3,
	A6XX_DBGBUS_HLSQ         = 0x4,
	A6XX_DBGBUS_UCHE         = 0x5,
	A6XX_DBGBUS_DPM          = 0x6,
	A6XX_DBGBUS_TESS         = 0x7,
	A6XX_DBGBUS_PC           = 0x8,
	A6XX_DBGBUS_VFDP         = 0x9,
	A6XX_DBGBUS_VPC          = 0xa,
	A6XX_DBGBUS_TSE          = 0xb,
	A6XX_DBGBUS_RAS          = 0xc,
	A6XX_DBGBUS_VSC          = 0xd,
	A6XX_DBGBUS_COM          = 0xe,
	A6XX_DBGBUS_LRZ          = 0x10,
	A6XX_DBGBUS_A2D          = 0x11,
	A6XX_DBGBUS_CCUFCHE      = 0x12,
	A6XX_DBGBUS_GMU_CX       = 0x13,
	A6XX_DBGBUS_RBP          = 0x14,
	A6XX_DBGBUS_DCS          = 0x15,
	A6XX_DBGBUS_RBBM_CFG     = 0x16,
	A6XX_DBGBUS_CX           = 0x17,
	A6XX_DBGBUS_GMU_GX       = 0x18,
	A6XX_DBGBUS_TPFCHE       = 0x19,
	A6XX_DBGBUS_GBIF_GX      = 0x1a,
	A6XX_DBGBUS_GPC          = 0x1d,
	A6XX_DBGBUS_LARC         = 0x1e,
	A6XX_DBGBUS_HLSQ_SPTP    = 0x1f,
	A6XX_DBGBUS_RB_0         = 0x20,
	A6XX_DBGBUS_RB_1         = 0x21,
	A6XX_DBGBUS_RB_2         = 0x22,
	A6XX_DBGBUS_UCHE_WRAPPER = 0x24,
	A6XX_DBGBUS_CCU_0        = 0x28,
	A6XX_DBGBUS_CCU_1        = 0x29,
	A6XX_DBGBUS_CCU_2        = 0x2a,
	A6XX_DBGBUS_VFD_0        = 0x38,
	A6XX_DBGBUS_VFD_1        = 0x39,
	A6XX_DBGBUS_VFD_2        = 0x3a,
	A6XX_DBGBUS_VFD_3        = 0x3b,
	A6XX_DBGBUS_VFD_4        = 0x3c,
	A6XX_DBGBUS_VFD_5        = 0x3d,
	A6XX_DBGBUS_SP_0         = 0x40,
	A6XX_DBGBUS_SP_1         = 0x41,
	A6XX_DBGBUS_SP_2         = 0x42,
	A6XX_DBGBUS_TPL1_0       = 0x48,
	A6XX_DBGBUS_TPL1_1       = 0x49,
	A6XX_DBGBUS_TPL1_2       = 0x4a,
	A6XX_DBGBUS_TPL1_3       = 0x4b,
	A6XX_DBGBUS_TPL1_4       = 0x4c,
	A6XX_DBGBUS_TPL1_5       = 0x4d,
	A6XX_DBGBUS_SPTP_0       = 0x58,
	A6XX_DBGBUS_SPTP_1       = 0x59,
	A6XX_DBGBUS_SPTP_2       = 0x5a,
	A6XX_DBGBUS_SPTP_3       = 0x5b,
	A6XX_DBGBUS_SPTP_4       = 0x5c,
	A6XX_DBGBUS_SPTP_5       = 0x5d,
};

static const struct adreno_debugbus_block a6xx_dbgc_debugbus_blocks[] = {
	{ A6XX_DBGBUS_CP, 0x100, },
	{ A6XX_DBGBUS_RBBM, 0x100, },
	{ A6XX_DBGBUS_HLSQ, 0x100, },
	{ A6XX_DBGBUS_UCHE, 0x100, },
	{ A6XX_DBGBUS_DPM, 0x100, },
	{ A6XX_DBGBUS_TESS, 0x100, },
	{ A6XX_DBGBUS_PC, 0x100, },
	{ A6XX_DBGBUS_VFDP, 0x100, },
	{ A6XX_DBGBUS_VPC, 0x100, },
	{ A6XX_DBGBUS_TSE, 0x100, },
	{ A6XX_DBGBUS_RAS, 0x100, },
	{ A6XX_DBGBUS_VSC, 0x100, },
	{ A6XX_DBGBUS_COM, 0x100, },
	{ A6XX_DBGBUS_LRZ, 0x100, },
	{ A6XX_DBGBUS_A2D, 0x100, },
	{ A6XX_DBGBUS_CCUFCHE, 0x100, },
	{ A6XX_DBGBUS_RBP, 0x100, },
	{ A6XX_DBGBUS_DCS, 0x100, },
	{ A6XX_DBGBUS_RBBM_CFG, 0x100, },
	{ A6XX_DBGBUS_GMU_GX, 0x100, },
	{ A6XX_DBGBUS_TPFCHE, 0x100, },
	{ A6XX_DBGBUS_GPC, 0x100, },
	{ A6XX_DBGBUS_LARC, 0x100, },
	{ A6XX_DBGBUS_HLSQ_SPTP, 0x100, },
	{ A6XX_DBGBUS_RB_0, 0x100, },
	{ A6XX_DBGBUS_RB_1, 0x100, },
	{ A6XX_DBGBUS_UCHE_WRAPPER, 0x100, },
	{ A6XX_DBGBUS_CCU_0, 0x100, },
	{ A6XX_DBGBUS_CCU_1, 0x100, },
	{ A6XX_DBGBUS_VFD_0, 0x100, },
	{ A6XX_DBGBUS_VFD_1, 0x100, },
	{ A6XX_DBGBUS_VFD_2, 0x100, },
	{ A6XX_DBGBUS_VFD_3, 0x100, },
	{ A6XX_DBGBUS_SP_0, 0x100, },
	{ A6XX_DBGBUS_SP_1, 0x100, },
	{ A6XX_DBGBUS_TPL1_0, 0x100, },
	{ A6XX_DBGBUS_TPL1_1, 0x100, },
	{ A6XX_DBGBUS_TPL1_2, 0x100, },
	{ A6XX_DBGBUS_TPL1_3, 0x100, },
};

static const struct adreno_debugbus_block a6xx_vbif_debugbus_blocks = {
	A6XX_DBGBUS_VBIF, 0x100,
};

static const struct adreno_debugbus_block a6xx_cx_dbgc_debugbus_blocks[] = {
	{ A6XX_DBGBUS_GMU_CX, 0x100, },
	{ A6XX_DBGBUS_CX, 0x100, },
};

static const struct adreno_debugbus_block a650_dbgc_debugbus_blocks[] = {
	{ A6XX_DBGBUS_RB_2, 0x100, },
	{ A6XX_DBGBUS_CCU_2, 0x100, },
	{ A6XX_DBGBUS_VFD_4, 0x100, },
	{ A6XX_DBGBUS_VFD_5, 0x100, },
	{ A6XX_DBGBUS_SP_2, 0x100, },
	{ A6XX_DBGBUS_TPL1_4, 0x100, },
	{ A6XX_DBGBUS_TPL1_5, 0x100, },
	{ A6XX_DBGBUS_SPTP_0, 0x100, },
	{ A6XX_DBGBUS_SPTP_1, 0x100, },
	{ A6XX_DBGBUS_SPTP_2, 0x100, },
	{ A6XX_DBGBUS_SPTP_3, 0x100, },
	{ A6XX_DBGBUS_SPTP_4, 0x100, },
	{ A6XX_DBGBUS_SPTP_5, 0x100, },
};

#define A6XX_NUM_SHADER_BANKS 3
#define A6XX_SHADER_STATETYPE_SHIFT 8

enum a6xx_shader_obj {
	A6XX_TP0_TMO_DATA               = 0x9,
	A6XX_TP0_SMO_DATA               = 0xa,
	A6XX_TP0_MIPMAP_BASE_DATA       = 0xb,
	A6XX_TP1_TMO_DATA               = 0x19,
	A6XX_TP1_SMO_DATA               = 0x1a,
	A6XX_TP1_MIPMAP_BASE_DATA       = 0x1b,
	A6XX_SP_INST_DATA               = 0x29,
	A6XX_SP_LB_0_DATA               = 0x2a,
	A6XX_SP_LB_1_DATA               = 0x2b,
	A6XX_SP_LB_2_DATA               = 0x2c,
	A6XX_SP_LB_3_DATA               = 0x2d,
	A6XX_SP_LB_4_DATA               = 0x2e,
	A6XX_SP_LB_5_DATA               = 0x2f,
	A6XX_SP_CB_BINDLESS_DATA        = 0x30,
	A6XX_SP_CB_LEGACY_DATA          = 0x31,
	A6XX_SP_UAV_DATA                = 0x32,
	A6XX_SP_INST_TAG                = 0x33,
	A6XX_SP_CB_BINDLESS_TAG         = 0x34,
	A6XX_SP_TMO_UMO_TAG             = 0x35,
	A6XX_SP_SMO_TAG                 = 0x36,
	A6XX_SP_STATE_DATA              = 0x37,
	A6XX_HLSQ_CHUNK_CVS_RAM         = 0x49,
	A6XX_HLSQ_CHUNK_CPS_RAM         = 0x4a,
	A6XX_HLSQ_CHUNK_CVS_RAM_TAG     = 0x4b,
	A6XX_HLSQ_CHUNK_CPS_RAM_TAG     = 0x4c,
	A6XX_HLSQ_ICB_CVS_CB_BASE_TAG   = 0x4d,
	A6XX_HLSQ_ICB_CPS_CB_BASE_TAG   = 0x4e,
	A6XX_HLSQ_CVS_MISC_RAM          = 0x50,
	A6XX_HLSQ_CPS_MISC_RAM          = 0x51,
	A6XX_HLSQ_INST_RAM              = 0x52,
	A6XX_HLSQ_GFX_CVS_CONST_RAM     = 0x53,
	A6XX_HLSQ_GFX_CPS_CONST_RAM     = 0x54,
	A6XX_HLSQ_CVS_MISC_RAM_TAG      = 0x55,
	A6XX_HLSQ_CPS_MISC_RAM_TAG      = 0x56,
	A6XX_HLSQ_INST_RAM_TAG          = 0x57,
	A6XX_HLSQ_GFX_CVS_CONST_RAM_TAG = 0x58,
	A6XX_HLSQ_GFX_CPS_CONST_RAM_TAG = 0x59,
	A6XX_HLSQ_PWR_REST_RAM          = 0x5a,
	A6XX_HLSQ_PWR_REST_TAG          = 0x5b,
	A6XX_HLSQ_DATAPATH_META         = 0x60,
	A6XX_HLSQ_FRONTEND_META         = 0x61,
	A6XX_HLSQ_INDIRECT_META         = 0x62,
	A6XX_HLSQ_BACKEND_META          = 0x63,
	A6XX_SP_LB_6_DATA               = 0x70,
	A6XX_SP_LB_7_DATA               = 0x71,
	A6XX_HLSQ_INST_RAM_1            = 0x73,
};

struct a6xx_shader_block {
	unsigned int statetype;
	unsigned int sz;
	uint64_t offset;
};

struct a6xx_shader_block_info {
	struct a6xx_shader_block *block;
	unsigned int bank;
	uint64_t offset;
};

static struct a6xx_shader_block a6xx_shader_blocks[] = {
	{A6XX_TP0_TMO_DATA,               0x200},
	{A6XX_TP0_SMO_DATA,               0x80,},
	{A6XX_TP0_MIPMAP_BASE_DATA,       0x3C0},
	{A6XX_TP1_TMO_DATA,               0x200},
	{A6XX_TP1_SMO_DATA,               0x80,},
	{A6XX_TP1_MIPMAP_BASE_DATA,       0x3C0},
	{A6XX_SP_INST_DATA,               0x800},
	{A6XX_SP_LB_0_DATA,               0x800},
	{A6XX_SP_LB_1_DATA,               0x800},
	{A6XX_SP_LB_2_DATA,               0x800},
	{A6XX_SP_LB_3_DATA,               0x800},
	{A6XX_SP_LB_4_DATA,               0x800},
	{A6XX_SP_LB_5_DATA,               0x200},
	{A6XX_SP_CB_BINDLESS_DATA,        0x800},
	{A6XX_SP_CB_LEGACY_DATA,          0x280,},
	{A6XX_SP_UAV_DATA,                0x80,},
	{A6XX_SP_INST_TAG,                0x80,},
	{A6XX_SP_CB_BINDLESS_TAG,         0x80,},
	{A6XX_SP_TMO_UMO_TAG,             0x80,},
	{A6XX_SP_SMO_TAG,                 0x80},
	{A6XX_SP_STATE_DATA,              0x3F},
	{A6XX_HLSQ_CHUNK_CVS_RAM,         0x1C0},
	{A6XX_HLSQ_CHUNK_CPS_RAM,         0x280},
	{A6XX_HLSQ_CHUNK_CVS_RAM_TAG,     0x40,},
	{A6XX_HLSQ_CHUNK_CPS_RAM_TAG,     0x40,},
	{A6XX_HLSQ_ICB_CVS_CB_BASE_TAG,   0x4,},
	{A6XX_HLSQ_ICB_CPS_CB_BASE_TAG,   0x4,},
	{A6XX_HLSQ_CVS_MISC_RAM,          0x1C0},
	{A6XX_HLSQ_CPS_MISC_RAM,          0x580},
	{A6XX_HLSQ_INST_RAM,              0x800},
	{A6XX_HLSQ_GFX_CVS_CONST_RAM,     0x800},
	{A6XX_HLSQ_GFX_CPS_CONST_RAM,     0x800},
	{A6XX_HLSQ_CVS_MISC_RAM_TAG,      0x8,},
	{A6XX_HLSQ_CPS_MISC_RAM_TAG,      0x4,},
	{A6XX_HLSQ_INST_RAM_TAG,          0x80,},
	{A6XX_HLSQ_GFX_CVS_CONST_RAM_TAG, 0xC,},
	{A6XX_HLSQ_GFX_CPS_CONST_RAM_TAG, 0x10},
	{A6XX_HLSQ_PWR_REST_RAM,          0x28},
	{A6XX_HLSQ_PWR_REST_TAG,          0x14},
	{A6XX_HLSQ_DATAPATH_META,         0x40,},
	{A6XX_HLSQ_FRONTEND_META,         0x40},
	{A6XX_HLSQ_INDIRECT_META,         0x40,},
	{A6XX_SP_LB_6_DATA,               0x200},
	{A6XX_SP_LB_7_DATA,               0x200},
	{A6XX_HLSQ_INST_RAM_1,            0x200},
};

static struct kgsl_memdesc *a6xx_capturescript;
static struct kgsl_memdesc *a6xx_crashdump_registers;
static bool crash_dump_valid;
static u32 *a6xx_cd_reg_end;

static struct reg_list {
	const unsigned int *regs;
	unsigned int count;
	const struct sel_reg *sel;
	uint64_t offset;
} a6xx_reg_list[] = {
	{ a6xx_registers, ARRAY_SIZE(a6xx_registers) / 2, NULL },
	{ a660_registers, ARRAY_SIZE(a660_registers) / 2, NULL },
	{ a6xx_rb_rac_registers, ARRAY_SIZE(a6xx_rb_rac_registers) / 2,
		&_a6xx_rb_rac_aperture },
	{ a6xx_rb_rbp_registers, ARRAY_SIZE(a6xx_rb_rbp_registers) / 2,
		&_a6xx_rb_rbp_aperture },
};

#define REG_PAIR_COUNT(_a, _i) \
	(((_a)[(2 * (_i)) + 1] - (_a)[2 * (_i)]) + 1)

static size_t a6xx_legacy_snapshot_registers(struct kgsl_device *device,
		u8 *buf, size_t remain, struct reg_list *regs)
{
	struct kgsl_snapshot_registers snapshot_regs = {
		.regs = regs->regs,
		.count = regs->count,
	};

	if (regs->sel)
		kgsl_regwrite(device, regs->sel->host_reg, regs->sel->val);

	return kgsl_snapshot_dump_registers(device, buf, remain,
		&snapshot_regs);
}

static size_t a6xx_snapshot_registers(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header = (struct kgsl_snapshot_regs *)buf;
	struct reg_list *regs = (struct reg_list *)priv;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src;
	unsigned int j, k;
	unsigned int count = 0;

	if (!crash_dump_valid)
		return a6xx_legacy_snapshot_registers(device, buf, remain,
			regs);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	src = a6xx_crashdump_registers->hostptr + regs->offset;
	remain -= sizeof(*header);

	for (j = 0; j < regs->count; j++) {
		unsigned int start = regs->regs[2 * j];
		unsigned int end = regs->regs[(2 * j) + 1];

		if (remain < ((end - start) + 1) * 8) {
			SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
			goto out;
		}

		remain -= ((end - start) + 1) * 8;

		for (k = start; k <= end; k++, count++) {
			*data++ = k;
			*data++ = *src++;
		}
	}

out:
	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}

static size_t a6xx_snapshot_pre_crashdump_regs(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_registers pre_cdregs = {
			.regs = a6xx_pre_crashdumper_registers,
			.count = ARRAY_SIZE(a6xx_pre_crashdumper_registers)/2,
	};

	return kgsl_snapshot_dump_registers(device, buf, remain, &pre_cdregs);
}

static size_t a6xx_legacy_snapshot_shader(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_shader *header =
		(struct kgsl_snapshot_shader *) buf;
	struct a6xx_shader_block_info *info =
		(struct a6xx_shader_block_info *) priv;
	struct a6xx_shader_block *block = info->block;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int read_sel, val;
	int i;

	if (!device->snapshot_legacy)
		return 0;

	if (remain < SHADER_SECTION_SZ(block->sz)) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	/*
	 * If crashdumper times out, accessing some readback states from
	 * AHB path might fail. Hence, skip SP_INST_TAG and SP_INST_DATA
	 * state types during snapshot dump in legacy flow.
	 */
	if (adreno_is_a660(ADRENO_DEVICE(device)) &&
		(block->statetype == A6XX_SP_INST_TAG ||
		 block->statetype == A6XX_SP_INST_DATA))
		return 0;

	header->type = block->statetype;
	header->index = info->bank;
	header->size = block->sz;

	read_sel = (block->statetype << A6XX_SHADER_STATETYPE_SHIFT) |
		info->bank;
	kgsl_regwrite(device, A6XX_HLSQ_DBG_READ_SEL, read_sel);

	/*
	 * An explicit barrier is needed so that reads do not happen before
	 * the register write.
	 */
	mb();

	for (i = 0; i < block->sz; i++) {
		kgsl_regread(device, (A6XX_HLSQ_DBG_AHB_READ_APERTURE + i),
			&val);
		*data++ = val;
	}

	return SHADER_SECTION_SZ(block->sz);
}

static size_t a6xx_snapshot_shader_memory(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_shader *header =
		(struct kgsl_snapshot_shader *) buf;
	struct a6xx_shader_block_info *info =
		(struct a6xx_shader_block_info *) priv;
	struct a6xx_shader_block *block = info->block;
	unsigned int *data = (unsigned int *) (buf + sizeof(*header));

	if (!crash_dump_valid)
		return a6xx_legacy_snapshot_shader(device, buf, remain, priv);

	if (remain < SHADER_SECTION_SZ(block->sz)) {
		SNAPSHOT_ERR_NOMEM(device, "SHADER MEMORY");
		return 0;
	}

	header->type = block->statetype;
	header->index = info->bank;
	header->size = block->sz;

	memcpy(data, a6xx_crashdump_registers->hostptr + info->offset,
		block->sz * sizeof(unsigned int));

	return SHADER_SECTION_SZ(block->sz);
}

static void a6xx_snapshot_shader(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	unsigned int i, j;
	struct a6xx_shader_block_info info;

	for (i = 0; i < ARRAY_SIZE(a6xx_shader_blocks); i++) {
		for (j = 0; j < A6XX_NUM_SHADER_BANKS; j++) {
			info.block = &a6xx_shader_blocks[i];
			info.bank = j;
			info.offset = a6xx_shader_blocks[i].offset +
				(j * a6xx_shader_blocks[i].sz);

			/* Shader working/shadow memory */
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_SHADER,
				snapshot, a6xx_snapshot_shader_memory, &info);
		}
	}
}

static void a650_snapshot_mempool(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	u32 val;

	/* set CP_CHICKEN_DBG[StabilizeMVC] to stabilize it while dumping */
	kgsl_regread(device, A6XX_CP_CHICKEN_DBG, &val);
	kgsl_regwrite(device, A6XX_CP_CHICKEN_DBG, val | BIT(2));

	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_MEM_POOL_DBG_ADDR, A6XX_CP_MEM_POOL_DBG_DATA,
		0, 0x2100);

	kgsl_regwrite(device, A6XX_CP_CHICKEN_DBG, val);
}

static void a6xx_snapshot_mempool(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	unsigned int pool_size;
	u8 *buf = snapshot->ptr;

	/* Set the mempool size to 0 to stabilize it while dumping */
	kgsl_regread(device, A6XX_CP_MEM_POOL_SIZE, &pool_size);
	kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, 0);

	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_MEM_POOL_DBG_ADDR, A6XX_CP_MEM_POOL_DBG_DATA,
		0, 0x2100);

	/*
	 * Data at offset 0x2000 in the mempool section is the mempool size.
	 * Since we set it to 0, patch in the original size so that the data
	 * is consistent.
	 */
	if (buf < snapshot->ptr) {
		unsigned int *data;

		/* Skip over the headers */
		buf += sizeof(struct kgsl_snapshot_section_header) +
				sizeof(struct kgsl_snapshot_indexed_regs);

		data = (unsigned int *)buf + 0x2000;
		*data = pool_size;
	}

	/* Restore the saved mempool size */
	kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, pool_size);
}

static inline unsigned int a6xx_read_dbgahb(struct kgsl_device *device,
				unsigned int regbase, unsigned int reg)
{
	unsigned int read_reg = A6XX_HLSQ_DBG_AHB_READ_APERTURE +
				reg - regbase / 4;
	unsigned int val;

	kgsl_regread(device, read_reg, &val);
	return val;
}

static size_t a6xx_legacy_snapshot_cluster_dbgahb(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
				(struct kgsl_snapshot_mvc_regs *)buf;
	struct a6xx_cluster_dbgahb_regs_info *info =
				(struct a6xx_cluster_dbgahb_regs_info *)priv;
	struct a6xx_cluster_dbgahb_registers *cur_cluster = info->cluster;
	unsigned int read_sel;
	unsigned int data_size = 0;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i, j;

	if (!device->snapshot_legacy)
		return 0;

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cur_cluster->id;

	read_sel = ((cur_cluster->statetype + info->ctxt_id * 2) & 0xff) << 8;
	kgsl_regwrite(device, A6XX_HLSQ_DBG_READ_SEL, read_sel);

	/*
	 * An explicit barrier is needed so that reads do not happen before
	 * the register write.
	 */
	mb();

	for (i = 0; i < cur_cluster->num_sets; i++) {
		unsigned int start = cur_cluster->regs[2 * i];
		unsigned int end = cur_cluster->regs[2 * i + 1];

		if (remain < (end - start + 3) * 4) {
			SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
			goto out;
		}

		remain -= (end - start + 3) * 4;
		data_size += (end - start + 3) * 4;

		*data++ = start | (1 << 31);
		*data++ = end;

		for (j = start; j <= end; j++) {
			unsigned int val;

			val = a6xx_read_dbgahb(device, cur_cluster->regbase, j);
			*data++ = val;

		}
	}

out:
	return data_size + sizeof(*header);
}

static size_t a6xx_snapshot_cluster_dbgahb(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
				(struct kgsl_snapshot_mvc_regs *)buf;
	struct a6xx_cluster_dbgahb_regs_info *info =
				(struct a6xx_cluster_dbgahb_regs_info *)priv;
	struct a6xx_cluster_dbgahb_registers *cluster = info->cluster;
	unsigned int data_size = 0;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int i, j;
	unsigned int *src;


	if (!crash_dump_valid)
		return a6xx_legacy_snapshot_cluster_dbgahb(device, buf, remain,
				info);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cluster->id;

	src = a6xx_crashdump_registers->hostptr +
		(header->ctxt_id ? cluster->offset1 : cluster->offset0);

	for (i = 0; i < cluster->num_sets; i++) {
		unsigned int start;
		unsigned int end;

		start = cluster->regs[2 * i];
		end = cluster->regs[2 * i + 1];

		if (remain < (end - start + 3) * 4) {
			SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
			goto out;
		}

		remain -= (end - start + 3) * 4;
		data_size += (end - start + 3) * 4;

		*data++ = start | (1 << 31);
		*data++ = end;
		for (j = start; j <= end; j++)
			*data++ = *src++;
	}
out:
	return data_size + sizeof(*header);
}

static size_t a6xx_legacy_snapshot_non_ctx_dbgahb(struct kgsl_device *device,
				u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header =
				(struct kgsl_snapshot_regs *)buf;
	struct a6xx_non_ctx_dbgahb_registers *regs =
				(struct a6xx_non_ctx_dbgahb_registers *)priv;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	int count = 0;
	unsigned int read_sel;
	int i, j;

	if (!device->snapshot_legacy)
		return 0;

	/* Figure out how many registers we are going to dump */
	for (i = 0; i < regs->num_sets; i++) {
		int start = regs->regs[i * 2];
		int end = regs->regs[i * 2 + 1];

		count += (end - start + 1);
	}

	if (remain < (count * 8) + sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	header->count = count;

	read_sel = (regs->statetype & 0xff) << 8;
	kgsl_regwrite(device, A6XX_HLSQ_DBG_READ_SEL, read_sel);

	for (i = 0; i < regs->num_sets; i++) {
		unsigned int start = regs->regs[2 * i];
		unsigned int end = regs->regs[2 * i + 1];

		for (j = start; j <= end; j++) {
			unsigned int val;

			val = a6xx_read_dbgahb(device, regs->regbase, j);
			*data++ = j;
			*data++ = val;

		}
	}
	return (count * 8) + sizeof(*header);
}

static size_t a6xx_snapshot_non_ctx_dbgahb(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_regs *header =
				(struct kgsl_snapshot_regs *)buf;
	struct a6xx_non_ctx_dbgahb_registers *regs =
				(struct a6xx_non_ctx_dbgahb_registers *)priv;
	unsigned int count = 0;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int i, k;
	unsigned int *src;

	if (!crash_dump_valid)
		return a6xx_legacy_snapshot_non_ctx_dbgahb(device, buf, remain,
				regs);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	src = a6xx_crashdump_registers->hostptr + regs->offset;

	for (i = 0; i < regs->num_sets; i++) {
		unsigned int start;
		unsigned int end;

		start = regs->regs[2 * i];
		end = regs->regs[(2 * i) + 1];

		if (remain < (end - start + 1) * 8) {
			SNAPSHOT_ERR_NOMEM(device, "REGISTERS");
			goto out;
		}

		remain -= ((end - start) + 1) * 8;

		for (k = start; k <= end; k++, count++) {
			*data++ = k;
			*data++ = *src++;
		}
	}
out:
	header->count = count;

	/* Return the size of the section */
	return (count * 8) + sizeof(*header);
}

static void a6xx_snapshot_dbgahb_regs(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(a6xx_dbgahb_ctx_clusters); i++) {
		struct a6xx_cluster_dbgahb_registers *cluster =
				&a6xx_dbgahb_ctx_clusters[i];
		struct a6xx_cluster_dbgahb_regs_info info;

		info.cluster = cluster;
		for (j = 0; j < A6XX_NUM_CTXTS; j++) {
			info.ctxt_id = j;

			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC, snapshot,
				a6xx_snapshot_cluster_dbgahb, &info);
		}
	}

	for (i = 0; i < ARRAY_SIZE(a6xx_non_ctx_dbgahb); i++) {
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_REGS, snapshot,
			a6xx_snapshot_non_ctx_dbgahb, &a6xx_non_ctx_dbgahb[i]);
	}
}

static size_t a6xx_legacy_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
					(struct kgsl_snapshot_mvc_regs *)buf;
	struct a6xx_cluster_regs_info *info =
					(struct a6xx_cluster_regs_info *)priv;
	struct a6xx_cluster_registers *cur_cluster = info->cluster;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int ctxt = info->ctxt_id;
	unsigned int start, end, i, j, aperture_cntl = 0;
	unsigned int data_size = 0;

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cur_cluster->id;

	/*
	 * Set the AHB control for the Host to read from the
	 * cluster/context for this iteration.
	 */
	aperture_cntl = ((cur_cluster->id & 0x7) << 8) | (ctxt << 4) | ctxt;
	kgsl_regwrite(device, A6XX_CP_APERTURE_CNTL_HOST, aperture_cntl);

	if (cur_cluster->sel)
		kgsl_regwrite(device, cur_cluster->sel->host_reg,
			cur_cluster->sel->val);

	for (i = 0; i < cur_cluster->num_sets; i++) {
		start = cur_cluster->regs[2 * i];
		end = cur_cluster->regs[2 * i + 1];

		if (remain < (end - start + 3) * 4) {
			SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
			goto out;
		}

		remain -= (end - start + 3) * 4;
		data_size += (end - start + 3) * 4;

		*data++ = start | (1 << 31);
		*data++ = end;
		for (j = start; j <= end; j++) {
			unsigned int val;

			kgsl_regread(device, j, &val);
			*data++ = val;
		}
	}
out:
	return data_size + sizeof(*header);
}

static size_t a6xx_snapshot_mvc(struct kgsl_device *device, u8 *buf,
				size_t remain, void *priv)
{
	struct kgsl_snapshot_mvc_regs *header =
				(struct kgsl_snapshot_mvc_regs *)buf;
	struct a6xx_cluster_regs_info *info =
				(struct a6xx_cluster_regs_info *)priv;
	struct a6xx_cluster_registers *cluster = info->cluster;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int *src;
	int i, j;
	unsigned int start, end;
	size_t data_size = 0;

	if (!crash_dump_valid)
		return a6xx_legacy_snapshot_mvc(device, buf, remain, info);

	if (remain < sizeof(*header)) {
		SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
		return 0;
	}

	remain -= sizeof(*header);

	header->ctxt_id = info->ctxt_id;
	header->cluster_id = cluster->id;

	src = a6xx_crashdump_registers->hostptr +
		(header->ctxt_id ? cluster->offset1 : cluster->offset0);

	for (i = 0; i < cluster->num_sets; i++) {
		start = cluster->regs[2 * i];
		end = cluster->regs[2 * i + 1];

		if (remain < (end - start + 3) * 4) {
			SNAPSHOT_ERR_NOMEM(device, "MVC REGISTERS");
			goto out;
		}

		remain -= (end - start + 3) * 4;
		data_size += (end - start + 3) * 4;

		*data++ = start | (1 << 31);
		*data++ = end;
		for (j = start; j <= end; j++)
			*data++ = *src++;
	}

out:
	return data_size + sizeof(*header);

}

static void a6xx_snapshot_mvc_regs(struct kgsl_device *device,
				struct kgsl_snapshot *snapshot)
{
	int i, j;
	struct a6xx_cluster_regs_info info;

	for (i = 0; i < ARRAY_SIZE(a6xx_clusters); i++) {
		struct a6xx_cluster_registers *cluster = &a6xx_clusters[i];

		/* Skip registers that dont exists on targets other than A660 */
		if (!adreno_is_a660(ADRENO_DEVICE(device)) &&
				(cluster->regs == a660_fe_cluster))
			continue;

		info.cluster = cluster;
		for (j = 0; j < A6XX_NUM_CTXTS; j++) {
			info.ctxt_id = j;

			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_MVC, snapshot,
				a6xx_snapshot_mvc, &info);
		}
	}
}

/* a6xx_dbgc_debug_bus_read() - Read data from trace bus */
static void a6xx_dbgc_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = (block_id << A6XX_DBGC_CFG_DBGBUS_SEL_PING_BLK_SEL_SHIFT) |
			(index << A6XX_DBGC_CFG_DBGBUS_SEL_PING_INDEX_SHIFT);

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_SEL_A, reg);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_SEL_B, reg);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_SEL_C, reg);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	kgsl_regread(device, A6XX_DBGC_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	kgsl_regread(device, A6XX_DBGC_CFG_DBGBUS_TRACE_BUF1, val);
}

/* a6xx_snapshot_dbgc_debugbus_block() - Capture debug data for a gpu block */
static size_t a6xx_snapshot_dbgc_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	struct adreno_debugbus_block *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int dwords;
	unsigned int block_id;
	size_t size;

	dwords = block->dwords;

	/* For a6xx each debug bus data unit is 2 DWORDS */
	size = (dwords * sizeof(unsigned int) * 2) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = block->block_id;
	if ((block->block_id == A6XX_DBGBUS_VBIF) && !adreno_is_a630(adreno_dev))
		header->id = A6XX_DBGBUS_GBIF_GX;
	header->count = dwords * 2;

	block_id = block->block_id;
	/* GMU_GX data is read using the GMU_CX block id on A630 */
	if ((adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) &&
		(block_id == A6XX_DBGBUS_GMU_GX))
		block_id = A6XX_DBGBUS_GMU_CX;

	for (i = 0; i < dwords; i++)
		a6xx_dbgc_debug_bus_read(device, block_id, i, &data[i*2]);

	return size;
}

/* a6xx_snapshot_vbif_debugbus_block() - Capture debug data for VBIF block */
static size_t a6xx_snapshot_vbif_debugbus_block(struct kgsl_device *device,
			u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	struct adreno_debugbus_block *block = priv;
	int i, j;
	/*
	 * Total number of VBIF data words considering 3 sections:
	 * 2 arbiter blocks of 16 words
	 * 5 AXI XIN blocks of 18 dwords each
	 * 4 core clock side XIN blocks of 12 dwords each
	 */
	unsigned int dwords = (16 * A6XX_NUM_AXI_ARB_BLOCKS) +
			(18 * A6XX_NUM_XIN_AXI_BLOCKS) +
			(12 * A6XX_NUM_XIN_CORE_BLOCKS);
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	size_t size;
	unsigned int reg_clk;

	size = (dwords * sizeof(unsigned int)) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}
	header->id = block->block_id;
	header->count = dwords;

	kgsl_regread(device, A6XX_VBIF_CLKON, &reg_clk);
	kgsl_regwrite(device, A6XX_VBIF_CLKON, reg_clk |
			(A6XX_VBIF_CLKON_FORCE_ON_TESTBUS_MASK <<
			A6XX_VBIF_CLKON_FORCE_ON_TESTBUS_SHIFT));
	kgsl_regwrite(device, A6XX_VBIF_TEST_BUS1_CTRL0, 0);
	kgsl_regwrite(device, A6XX_VBIF_TEST_BUS_OUT_CTRL,
			(A6XX_VBIF_TEST_BUS_OUT_CTRL_EN_MASK <<
			A6XX_VBIF_TEST_BUS_OUT_CTRL_EN_SHIFT));

	for (i = 0; i < A6XX_NUM_AXI_ARB_BLOCKS; i++) {
		kgsl_regwrite(device, A6XX_VBIF_TEST_BUS2_CTRL0,
			(1 << (i + 16)));
		for (j = 0; j < 16; j++) {
			kgsl_regwrite(device, A6XX_VBIF_TEST_BUS2_CTRL1,
				((j & A6XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_MASK)
				<< A6XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A6XX_VBIF_TEST_BUS_OUT,
					data);
			data++;
		}
	}

	/* XIN blocks AXI side */
	for (i = 0; i < A6XX_NUM_XIN_AXI_BLOCKS; i++) {
		kgsl_regwrite(device, A6XX_VBIF_TEST_BUS2_CTRL0, 1 << i);
		for (j = 0; j < 18; j++) {
			kgsl_regwrite(device, A6XX_VBIF_TEST_BUS2_CTRL1,
				((j & A6XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_MASK)
				<< A6XX_VBIF_TEST_BUS2_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A6XX_VBIF_TEST_BUS_OUT,
				data);
			data++;
		}
	}
	kgsl_regwrite(device, A6XX_VBIF_TEST_BUS2_CTRL0, 0);

	/* XIN blocks core clock side */
	for (i = 0; i < A6XX_NUM_XIN_CORE_BLOCKS; i++) {
		kgsl_regwrite(device, A6XX_VBIF_TEST_BUS1_CTRL0, 1 << i);
		for (j = 0; j < 12; j++) {
			kgsl_regwrite(device, A6XX_VBIF_TEST_BUS1_CTRL1,
				((j & A6XX_VBIF_TEST_BUS1_CTRL1_DATA_SEL_MASK)
				<< A6XX_VBIF_TEST_BUS1_CTRL1_DATA_SEL_SHIFT));
			kgsl_regread(device, A6XX_VBIF_TEST_BUS_OUT,
				data);
			data++;
		}
	}
	/* restore the clock of VBIF */
	kgsl_regwrite(device, A6XX_VBIF_CLKON, reg_clk);
	return size;
}

/* a6xx_cx_dbgc_debug_bus_read() - Read data from trace bus */
static void a6xx_cx_debug_bus_read(struct kgsl_device *device,
	unsigned int block_id, unsigned int index, unsigned int *val)
{
	unsigned int reg;

	reg = (block_id << A6XX_CX_DBGC_CFG_DBGBUS_SEL_PING_BLK_SEL_SHIFT) |
			(index << A6XX_CX_DBGC_CFG_DBGBUS_SEL_PING_INDEX_SHIFT);

	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_SEL_A, reg);
	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_SEL_B, reg);
	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_SEL_C, reg);
	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_SEL_D, reg);

	/*
	 * There needs to be a delay of 1 us to ensure enough time for correct
	 * data is funneled into the trace buffer
	 */
	udelay(1);

	kgsl_regread(device, A6XX_CX_DBGC_CFG_DBGBUS_TRACE_BUF2, val);
	val++;
	kgsl_regread(device, A6XX_CX_DBGC_CFG_DBGBUS_TRACE_BUF1, val);
}

/*
 * a6xx_snapshot_cx_dbgc_debugbus_block() - Capture debug data for a gpu
 * block from the CX DBGC block
 */
static size_t a6xx_snapshot_cx_dbgc_debugbus_block(struct kgsl_device *device,
	u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debugbus *header =
		(struct kgsl_snapshot_debugbus *)buf;
	struct adreno_debugbus_block *block = priv;
	int i;
	unsigned int *data = (unsigned int *)(buf + sizeof(*header));
	unsigned int dwords;
	size_t size;

	dwords = block->dwords;

	/* For a6xx each debug bus data unit is 2 DWRODS */
	size = (dwords * sizeof(unsigned int) * 2) + sizeof(*header);

	if (remain < size) {
		SNAPSHOT_ERR_NOMEM(device, "DEBUGBUS");
		return 0;
	}

	header->id = block->block_id;
	header->count = dwords * 2;

	for (i = 0; i < dwords; i++)
		a6xx_cx_debug_bus_read(device, block->block_id, i,
					&data[i*2]);

	return size;
}

/* a6xx_snapshot_debugbus() - Capture debug bus data */
static void a6xx_snapshot_debugbus(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	int i;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_CNTLT,
		(0xf << A6XX_DBGC_CFG_DBGBUS_CNTLT_SEGT_SHIFT) |
		(0x0 << A6XX_DBGC_CFG_DBGBUS_CNTLT_GRANU_SHIFT) |
		(0x0 << A6XX_DBGC_CFG_DBGBUS_CNTLT_TRACEEN_SHIFT));

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_CNTLM,
		0xf << A6XX_DBGC_CFG_DBGBUS_CTLTM_ENABLE_SHIFT);

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_IVTL_0, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_IVTL_1, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_IVTL_2, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_IVTL_3, 0);

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_BYTEL_0,
		(0 << A6XX_DBGC_CFG_DBGBUS_BYTEL0_SHIFT) |
		(1 << A6XX_DBGC_CFG_DBGBUS_BYTEL1_SHIFT) |
		(2 << A6XX_DBGC_CFG_DBGBUS_BYTEL2_SHIFT) |
		(3 << A6XX_DBGC_CFG_DBGBUS_BYTEL3_SHIFT) |
		(4 << A6XX_DBGC_CFG_DBGBUS_BYTEL4_SHIFT) |
		(5 << A6XX_DBGC_CFG_DBGBUS_BYTEL5_SHIFT) |
		(6 << A6XX_DBGC_CFG_DBGBUS_BYTEL6_SHIFT) |
		(7 << A6XX_DBGC_CFG_DBGBUS_BYTEL7_SHIFT));
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_BYTEL_1,
		(8 << A6XX_DBGC_CFG_DBGBUS_BYTEL8_SHIFT) |
		(9 << A6XX_DBGC_CFG_DBGBUS_BYTEL9_SHIFT) |
		(10 << A6XX_DBGC_CFG_DBGBUS_BYTEL10_SHIFT) |
		(11 << A6XX_DBGC_CFG_DBGBUS_BYTEL11_SHIFT) |
		(12 << A6XX_DBGC_CFG_DBGBUS_BYTEL12_SHIFT) |
		(13 << A6XX_DBGC_CFG_DBGBUS_BYTEL13_SHIFT) |
		(14 << A6XX_DBGC_CFG_DBGBUS_BYTEL14_SHIFT) |
		(15 << A6XX_DBGC_CFG_DBGBUS_BYTEL15_SHIFT));

	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_MASKL_0, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_MASKL_1, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_MASKL_2, 0);
	kgsl_regwrite(device, A6XX_DBGC_CFG_DBGBUS_MASKL_3, 0);

	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_CNTLT,
		(0xf << A6XX_DBGC_CFG_DBGBUS_CNTLT_SEGT_SHIFT) |
		(0x0 << A6XX_DBGC_CFG_DBGBUS_CNTLT_GRANU_SHIFT) |
		(0x0 << A6XX_DBGC_CFG_DBGBUS_CNTLT_TRACEEN_SHIFT));

	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_CNTLM,
		0xf << A6XX_CX_DBGC_CFG_DBGBUS_CNTLM_ENABLE_SHIFT);

	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_IVTL_0, 0);
	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_IVTL_1, 0);
	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_IVTL_2, 0);
	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_IVTL_3, 0);

	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_BYTEL_0,
		(0 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL0_SHIFT) |
		(1 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL1_SHIFT) |
		(2 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL2_SHIFT) |
		(3 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL3_SHIFT) |
		(4 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL4_SHIFT) |
		(5 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL5_SHIFT) |
		(6 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL6_SHIFT) |
		(7 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL7_SHIFT));
	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_BYTEL_1,
		(8 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL8_SHIFT) |
		(9 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL9_SHIFT) |
		(10 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL10_SHIFT) |
		(11 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL11_SHIFT) |
		(12 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL12_SHIFT) |
		(13 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL13_SHIFT) |
		(14 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL14_SHIFT) |
		(15 << A6XX_CX_DBGC_CFG_DBGBUS_BYTEL15_SHIFT));

	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_MASKL_0, 0);
	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_MASKL_1, 0);
	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_MASKL_2, 0);
	kgsl_regwrite(device, A6XX_CX_DBGC_CFG_DBGBUS_MASKL_3, 0);

	for (i = 0; i < ARRAY_SIZE(a6xx_dbgc_debugbus_blocks); i++) {
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, a6xx_snapshot_dbgc_debugbus_block,
			(void *) &a6xx_dbgc_debugbus_blocks[i]);
	}

	if (adreno_is_a650_family(adreno_dev)) {
		for (i = 0; i < ARRAY_SIZE(a650_dbgc_debugbus_blocks); i++) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, a6xx_snapshot_dbgc_debugbus_block,
				(void *) &a650_dbgc_debugbus_blocks[i]);
		}
	}

	/*
	 * GBIF has same debugbus as of other GPU blocks hence fall back to
	 * default path if GPU uses GBIF.
	 * GBIF uses exactly same ID as of VBIF so use it as it is.
	 */
	if (!adreno_is_a630(adreno_dev))
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, a6xx_snapshot_dbgc_debugbus_block,
			(void *) &a6xx_vbif_debugbus_blocks);
	else
		kgsl_snapshot_add_section(device,
			KGSL_SNAPSHOT_SECTION_DEBUGBUS,
			snapshot, a6xx_snapshot_vbif_debugbus_block,
			(void *) &a6xx_vbif_debugbus_blocks);

	/* Dump the CX debugbus data if the block exists */
	if (kgsl_regmap_valid_offset(&device->regmap, A6XX_CX_DBGC_CFG_DBGBUS_SEL_A)) {
		for (i = 0; i < ARRAY_SIZE(a6xx_cx_dbgc_debugbus_blocks); i++) {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot, a6xx_snapshot_cx_dbgc_debugbus_block,
				(void *) &a6xx_cx_dbgc_debugbus_blocks[i]);
		}
		/*
		 * Get debugbus for GBIF CX part if GPU has GBIF block
		 * GBIF uses exactly same ID as of VBIF so use
		 * it as it is.
		 */
		if (!adreno_is_a630(adreno_dev))
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_DEBUGBUS,
				snapshot,
				a6xx_snapshot_cx_dbgc_debugbus_block,
				(void *) &a6xx_vbif_debugbus_blocks);
	}
}

static void _a6xx_do_crashdump(struct kgsl_device *device)
{
	u32 val = 0;
	ktime_t timeout;

	crash_dump_valid = false;

	if (!device->snapshot_crashdumper)
		return;

	if (IS_ERR_OR_NULL(a6xx_capturescript) ||
		IS_ERR_OR_NULL(a6xx_crashdump_registers))
		return;

	/* IF the SMMU is stalled we cannot do a crash dump */
	if (adreno_smmu_is_stalled(ADRENO_DEVICE(device)))
		return;

	/* Turn on APRIV for legacy targets so we can access the buffers */
	if (!ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_APRIV))
		kgsl_regwrite(device, A6XX_CP_MISC_CNTL, 1);

	kgsl_regwrite(device, A6XX_CP_CRASH_SCRIPT_BASE_LO,
			lower_32_bits(a6xx_capturescript->gpuaddr));
	kgsl_regwrite(device, A6XX_CP_CRASH_SCRIPT_BASE_HI,
			upper_32_bits(a6xx_capturescript->gpuaddr));
	kgsl_regwrite(device, A6XX_CP_CRASH_DUMP_CNTL, 1);

	timeout = ktime_add_ms(ktime_get(), CP_CRASH_DUMPER_TIMEOUT);

	if (!device->snapshot_atomic)
		might_sleep();

	for (;;) {
		/* make sure we're reading the latest value */
		rmb();
		if ((*a6xx_cd_reg_end) != 0xaaaaaaaa)
			break;

		if (ktime_compare(ktime_get(), timeout) > 0)
			break;

		/* Wait 1msec to avoid unnecessary looping */
		if (!device->snapshot_atomic)
			usleep_range(100, 1000);
	}

	kgsl_regread(device, A6XX_CP_CRASH_DUMP_STATUS, &val);

	if (!ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_APRIV))
		kgsl_regwrite(device, A6XX_CP_MISC_CNTL, 0);

	if (!(val & 0x2)) {
		dev_err(device->dev, "Crash dump timed out: 0x%X\n", val);
		return;
	}

	crash_dump_valid = true;
}

static size_t a6xx_snapshot_cp_roq(struct kgsl_device *device, u8 *buf,
		size_t remain, void *priv)
{
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *) buf;
	u32 size, *data = (u32 *) (buf + sizeof(*header));
	int i;

	kgsl_regread(device, A6XX_CP_ROQ_THRESHOLDS_2, &size);
	size >>= 14;

	if (remain < DEBUG_SECTION_SZ(size)) {
		SNAPSHOT_ERR_NOMEM(device, "CP ROQ DEBUG");
		return 0;
	}

	header->type = SNAPSHOT_DEBUG_CP_ROQ;
	header->size = size;

	kgsl_regwrite(device, A6XX_CP_ROQ_DBG_ADDR, 0x0);
	for (i = 0; i < size; i++)
		kgsl_regread(device, A6XX_CP_ROQ_DBG_DATA, &data[i]);

	return DEBUG_SECTION_SZ(size);
}

static inline bool a6xx_has_gbif_reinit(struct adreno_device *adreno_dev)
{
	/*
	 * Some targets in a6xx do not have reinit support in hardware.
	 * This check is only for hardware capability and not for finding
	 * whether gbif reinit sequence in software is enabled or not.
	 */
	return !(adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev) ||
		 adreno_is_a640_family(adreno_dev));
}

/*
 * a6xx_snapshot() - A6XX GPU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A6XX specific bits and pieces are grabbed
 * into the snapshot memory
 */
void a6xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	bool sptprac_on;
	unsigned int i;
	u32 hi, lo;

	/*
	 * Dump debugbus data here to capture it for both
	 * GMU and GPU snapshot. Debugbus data can be accessed
	 * even if the gx headswitch or sptprac is off. If gx
	 * headswitch is off, data for gx blocks will show as
	 * 0x5c00bd00.
	 */
	a6xx_snapshot_debugbus(adreno_dev, snapshot);

	/* Isense registers are on cx */
	if (adreno_is_a650_family(adreno_dev) &&
		kgsl_regmap_valid_offset(&device->regmap, a650_isense_registers[0])) {
		struct kgsl_snapshot_registers r;

		r.regs = a650_isense_registers;
		r.count = ARRAY_SIZE(a650_isense_registers) / 2;

		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
			snapshot, kgsl_snapshot_dump_registers, &r);
	}

	if (!gmu_core_isenabled(device)) {
		if (adreno_is_a619_holi(adreno_dev))
			adreno_snapshot_registers(device, snapshot,
					a6xx_holi_gmu_wrapper_registers,
					ARRAY_SIZE(a6xx_holi_gmu_wrapper_registers) / 2);
		else
			adreno_snapshot_registers(device, snapshot,
					a6xx_gmu_wrapper_registers,
					ARRAY_SIZE(a6xx_gmu_wrapper_registers) / 2);
	}

	sptprac_on = a6xx_gmu_sptprac_is_on(adreno_dev);

	if (!adreno_gx_is_on(adreno_dev))
		return;

	kgsl_regread(device, A6XX_CP_IB1_BASE, &lo);
	kgsl_regread(device, A6XX_CP_IB1_BASE_HI, &hi);

	snapshot->ib1base = (((u64) hi) << 32) | lo;

	kgsl_regread(device, A6XX_CP_IB2_BASE, &lo);
	kgsl_regread(device, A6XX_CP_IB2_BASE_HI, &hi);

	snapshot->ib2base = (((u64) hi) << 32) | lo;

	kgsl_regread(device, A6XX_CP_IB1_REM_SIZE, &snapshot->ib1size);
	kgsl_regread(device, A6XX_CP_IB2_REM_SIZE, &snapshot->ib2size);

	/* Assert the isStatic bit before triggering snapshot */
	if (adreno_is_a660(adreno_dev))
		kgsl_regwrite(device, A6XX_RBBM_SNAPSHOT_STATUS, 0x1);

	/* Dump the registers which get affected by crash dumper trigger */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
		snapshot, a6xx_snapshot_pre_crashdump_regs, NULL);

	/* Dump vbif registers as well which get affected by crash dumper */
	if (adreno_is_a630(adreno_dev))
		SNAPSHOT_REGISTERS(device, snapshot, a6xx_vbif_registers);
	else if (a6xx_has_gbif_reinit(adreno_dev))
		adreno_snapshot_registers(device, snapshot,
					  a6xx_gbif_reinit_registers,
					  ARRAY_SIZE(a6xx_gbif_reinit_registers) / 2);
	else
		adreno_snapshot_registers(device, snapshot,
			a6xx_gbif_registers,
			ARRAY_SIZE(a6xx_gbif_registers) / 2);

	/* Try to run the crash dumper */
	if (sptprac_on)
		_a6xx_do_crashdump(device);

	for (i = 0; i < ARRAY_SIZE(a6xx_reg_list); i++) {
		/* Skip registers that dont exists on targets other than A660 */
		if (!adreno_is_a660(adreno_dev) &&
				(a6xx_reg_list[i].regs == a660_registers))
			continue;

		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_REGS,
			snapshot, a6xx_snapshot_registers, &a6xx_reg_list[i]);
	}

	/* CP_SQE indexed registers */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_SQE_STAT_ADDR, A6XX_CP_SQE_STAT_DATA, 0, 0x33);

	/* CP_DRAW_STATE */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_DRAW_STATE_ADDR, A6XX_CP_DRAW_STATE_DATA,
		0, 0x100);

	 /* SQE_UCODE Cache */
	kgsl_snapshot_indexed_registers(device, snapshot,
		A6XX_CP_SQE_UCODE_DBG_ADDR, A6XX_CP_SQE_UCODE_DBG_DATA,
		0, 0x8000);

	/* CP LPAC indexed registers */
	if (adreno_is_a660(adreno_dev)) {
		u32 roq_size;

		kgsl_snapshot_indexed_registers(device, snapshot,
			 A6XX_CP_SQE_AC_STAT_ADDR, A6XX_CP_SQE_AC_STAT_DATA,
				0, 0x33);
		kgsl_snapshot_indexed_registers(device, snapshot,
			A6XX_CP_LPAC_DRAW_STATE_ADDR,
				A6XX_CP_LPAC_DRAW_STATE_DATA, 0, 0x100);
		kgsl_snapshot_indexed_registers(device, snapshot,
			A6XX_CP_SQE_AC_UCODE_DBG_ADDR,
				A6XX_CP_SQE_AC_UCODE_DBG_DATA, 0, 0x8000);

		kgsl_regread(device, A6XX_CP_LPAC_ROQ_THRESHOLDS_2, &roq_size);
		roq_size = roq_size >> 14;
		kgsl_snapshot_indexed_registers(device, snapshot,
			A6XX_CP_LPAC_ROQ_DBG_ADDR,
				A6XX_CP_LPAC_ROQ_DBG_DATA, 0, roq_size);

		kgsl_snapshot_indexed_registers(device, snapshot,
			A6XX_CP_LPAC_FIFO_DBG_ADDR, A6XX_CP_LPAC_FIFO_DBG_DATA,
			0, 0x40);
	}
	/*
	 * CP ROQ dump units is 4dwords. The number of units is stored
	 * in CP_ROQ_THRESHOLDS_2[31:16]. Read the value and convert to
	 * dword units.
	 */
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
		snapshot, a6xx_snapshot_cp_roq, NULL);

	/* Mempool debug data */
	if (adreno_is_a650_family(adreno_dev))
		a650_snapshot_mempool(device, snapshot);
	else
		a6xx_snapshot_mempool(device, snapshot);

	if (sptprac_on) {
		/* MVC register section */
		a6xx_snapshot_mvc_regs(device, snapshot);

		/* registers dumped through DBG AHB */
		a6xx_snapshot_dbgahb_regs(device, snapshot);

		/* Shader memory */
		a6xx_snapshot_shader(device, snapshot);

		if (!adreno_smmu_is_stalled(adreno_dev))
			memset(a6xx_crashdump_registers->hostptr, 0xaa,
					a6xx_crashdump_registers->size);
	}

	if (adreno_is_a660(adreno_dev)) {
		u32 val;

		kgsl_regread(device, A6XX_RBBM_SNAPSHOT_STATUS, &val);

		if (!val)
			dev_err(device->dev,
				"Interface signals may have changed during snapshot\n");

		kgsl_regwrite(device, A6XX_RBBM_SNAPSHOT_STATUS, 0x0);
	}

	/* Preemption record */
	adreno_snapshot_preemption_record(device, snapshot);
}

static int _a6xx_crashdump_init_mvc(struct adreno_device *adreno_dev,
	uint64_t *ptr, uint64_t *offset)
{
	int qwords = 0;
	unsigned int i, j, k;
	unsigned int count;

	for (i = 0; i < ARRAY_SIZE(a6xx_clusters); i++) {
		struct a6xx_cluster_registers *cluster = &a6xx_clusters[i];

		/* Skip registers that dont exists on targets other than A660 */
		if (!adreno_is_a660(adreno_dev) &&
				(cluster->regs == a660_fe_cluster))
			continue;

		/* The VPC registers are driven by VPC_PS cluster on a650 */
		if (adreno_is_a650_family(adreno_dev) &&
				(cluster->regs == a6xx_vpc_ps_cluster))
			cluster->id = CP_CLUSTER_VPC_PS;

		if (cluster->sel) {
			ptr[qwords++] = cluster->sel->val;
			ptr[qwords++] = ((uint64_t)cluster->sel->cd_reg << 44) |
				(1 << 21) | 1;
		}

		cluster->offset0 = *offset;
		for (j = 0; j < A6XX_NUM_CTXTS; j++) {

			if (j == 1)
				cluster->offset1 = *offset;

			ptr[qwords++] = (cluster->id << 8) | (j << 4) | j;
			ptr[qwords++] =
				((uint64_t)A6XX_CP_APERTURE_CNTL_CD << 44) |
				(1 << 21) | 1;

			for (k = 0; k < cluster->num_sets; k++) {
				count = REG_PAIR_COUNT(cluster->regs, k);
				ptr[qwords++] =
				a6xx_crashdump_registers->gpuaddr + *offset;
				ptr[qwords++] =
				(((uint64_t)cluster->regs[2 * k]) << 44) |
						count;

				*offset += count * sizeof(unsigned int);
			}
		}
	}

	return qwords;
}

static int _a6xx_crashdump_init_shader(struct a6xx_shader_block *block,
		uint64_t *ptr, uint64_t *offset)
{
	int qwords = 0;
	unsigned int j;

	/* Capture each bank in the block */
	for (j = 0; j < A6XX_NUM_SHADER_BANKS; j++) {
		/* Program the aperture */
		ptr[qwords++] =
			(block->statetype << A6XX_SHADER_STATETYPE_SHIFT) | j;
		ptr[qwords++] = (((uint64_t) A6XX_HLSQ_DBG_READ_SEL << 44)) |
			(1 << 21) | 1;

		/* Read all the data in one chunk */
		ptr[qwords++] = a6xx_crashdump_registers->gpuaddr + *offset;
		ptr[qwords++] =
			(((uint64_t) A6XX_HLSQ_DBG_AHB_READ_APERTURE << 44)) |
			block->sz;

		/* Remember the offset of the first bank for easy access */
		if (j == 0)
			block->offset = *offset;

		*offset += block->sz * sizeof(unsigned int);
	}

	return qwords;
}

static int _a6xx_crashdump_init_ctx_dbgahb(uint64_t *ptr, uint64_t *offset)
{
	int qwords = 0;
	unsigned int i, j, k;
	unsigned int count;

	for (i = 0; i < ARRAY_SIZE(a6xx_dbgahb_ctx_clusters); i++) {
		struct a6xx_cluster_dbgahb_registers *cluster =
				&a6xx_dbgahb_ctx_clusters[i];

		cluster->offset0 = *offset;

		for (j = 0; j < A6XX_NUM_CTXTS; j++) {
			if (j == 1)
				cluster->offset1 = *offset;

			/* Program the aperture */
			ptr[qwords++] =
				((cluster->statetype + j * 2) & 0xff) << 8;
			ptr[qwords++] =
				(((uint64_t)A6XX_HLSQ_DBG_READ_SEL << 44)) |
					(1 << 21) | 1;

			for (k = 0; k < cluster->num_sets; k++) {
				unsigned int start = cluster->regs[2 * k];

				count = REG_PAIR_COUNT(cluster->regs, k);
				ptr[qwords++] =
				a6xx_crashdump_registers->gpuaddr + *offset;
				ptr[qwords++] =
				(((uint64_t)(A6XX_HLSQ_DBG_AHB_READ_APERTURE +
					start - cluster->regbase / 4) << 44)) |
							count;

				*offset += count * sizeof(unsigned int);
			}
		}
	}
	return qwords;
}

static int _a6xx_crashdump_init_non_ctx_dbgahb(uint64_t *ptr, uint64_t *offset)
{
	int qwords = 0;
	unsigned int i, k;
	unsigned int count;

	for (i = 0; i < ARRAY_SIZE(a6xx_non_ctx_dbgahb); i++) {
		struct a6xx_non_ctx_dbgahb_registers *regs =
				&a6xx_non_ctx_dbgahb[i];

		regs->offset = *offset;

		/* Program the aperture */
		ptr[qwords++] = (regs->statetype & 0xff) << 8;
		ptr[qwords++] =	(((uint64_t)A6XX_HLSQ_DBG_READ_SEL << 44)) |
					(1 << 21) | 1;

		for (k = 0; k < regs->num_sets; k++) {
			unsigned int start = regs->regs[2 * k];

			count = REG_PAIR_COUNT(regs->regs, k);
			ptr[qwords++] =
				a6xx_crashdump_registers->gpuaddr + *offset;
			ptr[qwords++] =
				(((uint64_t)(A6XX_HLSQ_DBG_AHB_READ_APERTURE +
					start - regs->regbase / 4) << 44)) |
							count;

			*offset += count * sizeof(unsigned int);
		}
	}
	return qwords;
}

void a6xx_crashdump_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int script_size = 0;
	unsigned int data_size = 0;
	unsigned int i, j, k, ret;
	uint64_t *ptr;
	uint64_t offset = 0;

	if (!IS_ERR_OR_NULL(a6xx_capturescript) &&
		!IS_ERR_OR_NULL(a6xx_crashdump_registers))
		return;

	/*
	 * We need to allocate two buffers:
	 * 1 - the buffer to hold the draw script
	 * 2 - the buffer to hold the data
	 */

	/*
	 * To save the registers, we need 16 bytes per register pair for the
	 * script and a dword for each register in the data
	 */
	for (i = 0; i < ARRAY_SIZE(a6xx_reg_list); i++) {
		struct reg_list *regs = &a6xx_reg_list[i];

		/* Skip registers that dont exists on targets other than A660 */
		if (!adreno_is_a660(adreno_dev) &&
			(regs->regs == a660_registers))
			continue;

		/* 16 bytes for programming the aperture */
		if (regs->sel)
			script_size += 16;

		/* Each pair needs 16 bytes (2 qwords) */
		script_size += regs->count * 16;

		/* Each register needs a dword in the data */
		for (j = 0; j < regs->count; j++)
			data_size += REG_PAIR_COUNT(regs->regs, j) *
				sizeof(unsigned int);

	}

	/*
	 * To save the shader blocks for each block in each type we need 32
	 * bytes for the script (16 bytes to program the aperture and 16 to
	 * read the data) and then a block specific number of bytes to hold
	 * the data
	 */
	for (i = 0; i < ARRAY_SIZE(a6xx_shader_blocks); i++) {
		script_size += 32 * A6XX_NUM_SHADER_BANKS;
		data_size += a6xx_shader_blocks[i].sz * sizeof(unsigned int) *
			A6XX_NUM_SHADER_BANKS;
	}

	/* Calculate the script and data size for MVC registers */
	for (i = 0; i < ARRAY_SIZE(a6xx_clusters); i++) {
		struct a6xx_cluster_registers *cluster = &a6xx_clusters[i];

		/* Skip registers that dont exists on targets other than A660 */
		if (!adreno_is_a660(adreno_dev) &&
				(cluster->regs == a660_fe_cluster))
			continue;

		/* 16 bytes if cluster sel exists */
		if (cluster->sel)
			script_size += 16;

		for (j = 0; j < A6XX_NUM_CTXTS; j++) {

			/* 16 bytes for programming the aperture */
			script_size += 16;

			/* Reading each pair of registers takes 16 bytes */
			script_size += 16 * cluster->num_sets;

			/* A dword per register read from the cluster list */
			for (k = 0; k < cluster->num_sets; k++)
				data_size += REG_PAIR_COUNT(cluster->regs, k) *
						sizeof(unsigned int);
		}
	}

	/* Calculate the script and data size for debug AHB registers */
	for (i = 0; i < ARRAY_SIZE(a6xx_dbgahb_ctx_clusters); i++) {
		struct a6xx_cluster_dbgahb_registers *cluster =
				&a6xx_dbgahb_ctx_clusters[i];

		for (j = 0; j < A6XX_NUM_CTXTS; j++) {

			/* 16 bytes for programming the aperture */
			script_size += 16;

			/* Reading each pair of registers takes 16 bytes */
			script_size += 16 * cluster->num_sets;

			/* A dword per register read from the cluster list */
			for (k = 0; k < cluster->num_sets; k++)
				data_size += REG_PAIR_COUNT(cluster->regs, k) *
						sizeof(unsigned int);
		}
	}

	/*
	 * Calculate the script and data size for non context debug
	 * AHB registers
	 */
	for (i = 0; i < ARRAY_SIZE(a6xx_non_ctx_dbgahb); i++) {
		struct a6xx_non_ctx_dbgahb_registers *regs =
				&a6xx_non_ctx_dbgahb[i];

		/* 16 bytes for programming the aperture */
		script_size += 16;

		/* Reading each pair of registers takes 16 bytes */
		script_size += 16 * regs->num_sets;

		/* A dword per register read from the cluster list */
		for (k = 0; k < regs->num_sets; k++)
			data_size += REG_PAIR_COUNT(regs->regs, k) *
				sizeof(unsigned int);
	}

	/* 16 bytes (2 qwords) for last entry in CD script */
	script_size += 16;
	/* Increment data size to store last entry in CD */
	data_size += sizeof(unsigned int);

	/* Now allocate the script and data buffers */

	/* The script buffers needs 2 extra qwords on the end */
	ret = adreno_allocate_global(device, &a6xx_capturescript,
		script_size + 16, 0, KGSL_MEMFLAGS_GPUREADONLY,
		KGSL_MEMDESC_PRIVILEGED, "capturescript");
	if (ret)
		return;

	ret = adreno_allocate_global(device, &a6xx_crashdump_registers,
		data_size, 0, 0, KGSL_MEMDESC_PRIVILEGED,
		"capturescript_regs");
	if (ret)
		return;

	/* Build the crash script */

	ptr = (uint64_t *)a6xx_capturescript->hostptr;

	/* For the registers, program a read command for each pair */
	for (i = 0; i < ARRAY_SIZE(a6xx_reg_list); i++) {
		struct reg_list *regs = &a6xx_reg_list[i];

		/* Skip registers that dont exists on targets other than A660 */
		if (!adreno_is_a660(adreno_dev) &&
			(regs->regs == a660_registers))
			continue;

		regs->offset = offset;

		/* Program the SEL_CNTL_CD register appropriately */
		if (regs->sel) {
			*ptr++ = regs->sel->val;
			*ptr++ = (((uint64_t)regs->sel->cd_reg << 44)) |
					(1 << 21) | 1;
		}

		for (j = 0; j < regs->count; j++) {
			unsigned int r = REG_PAIR_COUNT(regs->regs, j);
			*ptr++ = a6xx_crashdump_registers->gpuaddr + offset;
			*ptr++ = (((uint64_t) regs->regs[2 * j]) << 44) | r;
			offset += r * sizeof(unsigned int);
		}
	}

	/* Program each shader block */
	for (i = 0; i < ARRAY_SIZE(a6xx_shader_blocks); i++) {
		ptr += _a6xx_crashdump_init_shader(&a6xx_shader_blocks[i], ptr,
							&offset);
	}

	/* Program the capturescript for the MVC regsiters */
	ptr += _a6xx_crashdump_init_mvc(adreno_dev, ptr, &offset);

	if (!adreno_is_a663(adreno_dev)) {
		ptr += _a6xx_crashdump_init_ctx_dbgahb(ptr, &offset);

		ptr += _a6xx_crashdump_init_non_ctx_dbgahb(ptr, &offset);
	}

	/* Save CD register end pointer to check CD status completion */
	a6xx_cd_reg_end = a6xx_crashdump_registers->hostptr + offset;

	memset(a6xx_crashdump_registers->hostptr, 0xaa,
			a6xx_crashdump_registers->size);

	/* Program the capturescript to read the last register entry */
	*ptr++ = a6xx_crashdump_registers->gpuaddr + offset;
	*ptr++ = (((uint64_t) A6XX_CP_CRASH_DUMP_STATUS) << 44) | (uint64_t) 1;

	*ptr++ = 0;
	*ptr++ = 0;
}
