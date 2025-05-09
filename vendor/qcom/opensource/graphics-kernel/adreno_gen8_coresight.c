// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/amba/bus.h>

#include "adreno.h"
#include "adreno_coresight.h"
#include "adreno_gen8.h"

static struct adreno_coresight_register gen8_coresight_regs[] = {
	{ GEN8_DBGC_CFG_DBGBUS_SEL_A },
	{ GEN8_DBGC_CFG_DBGBUS_SEL_B },
	{ GEN8_DBGC_CFG_DBGBUS_SEL_C },
	{ GEN8_DBGC_CFG_DBGBUS_SEL_D },
	{ GEN8_DBGC_CFG_DBGBUS_CNTLT },
	{ GEN8_DBGC_CFG_DBGBUS_CNTLM },
	{ GEN8_DBGC_CFG_DBGBUS_OPL },
	{ GEN8_DBGC_CFG_DBGBUS_OPE },
	{ GEN8_DBGC_CFG_DBGBUS_IVTL_0 },
	{ GEN8_DBGC_CFG_DBGBUS_IVTL_1 },
	{ GEN8_DBGC_CFG_DBGBUS_IVTL_2 },
	{ GEN8_DBGC_CFG_DBGBUS_IVTL_3 },
	{ GEN8_DBGC_CFG_DBGBUS_MASKL_0 },
	{ GEN8_DBGC_CFG_DBGBUS_MASKL_1 },
	{ GEN8_DBGC_CFG_DBGBUS_MASKL_2 },
	{ GEN8_DBGC_CFG_DBGBUS_MASKL_3 },
	{ GEN8_DBGC_CFG_DBGBUS_BYTEL_0 },
	{ GEN8_DBGC_CFG_DBGBUS_BYTEL_1 },
	{ GEN8_DBGC_CFG_DBGBUS_IVTE_0 },
	{ GEN8_DBGC_CFG_DBGBUS_IVTE_1 },
	{ GEN8_DBGC_CFG_DBGBUS_IVTE_2 },
	{ GEN8_DBGC_CFG_DBGBUS_IVTE_3 },
	{ GEN8_DBGC_CFG_DBGBUS_MASKE_0 },
	{ GEN8_DBGC_CFG_DBGBUS_MASKE_1 },
	{ GEN8_DBGC_CFG_DBGBUS_MASKE_2 },
	{ GEN8_DBGC_CFG_DBGBUS_MASKE_3 },
	{ GEN8_DBGC_CFG_DBGBUS_NIBBLEE },
	{ GEN8_DBGC_CFG_DBGBUS_PTRC0 },
	{ GEN8_DBGC_CFG_DBGBUS_PTRC1 },
	{ GEN8_DBGC_CFG_DBGBUS_LOADREG },
	{ GEN8_DBGC_CFG_DBGBUS_IDX },
	{ GEN8_DBGC_CFG_DBGBUS_CLRC },
	{ GEN8_DBGC_CFG_DBGBUS_LOADIVT },
	{ GEN8_DBGC_VBIF_DBG_CNTL },
	{ GEN8_DBGC_DBG_LO_HI_GPIO },
	{ GEN8_DBGC_EXT_TRACE_BUS_CNTL },
	{ GEN8_DBGC_READ_AHB_THROUGH_DBG },
	{ GEN8_DBGC_CFG_DBGBUS_TRACE_BUF1 },
	{ GEN8_DBGC_CFG_DBGBUS_TRACE_BUF2 },
	{ GEN8_DBGC_EVT_CFG },
	{ GEN8_DBGC_EVT_INTF_SEL_0 },
	{ GEN8_DBGC_EVT_INTF_SEL_1 },
	{ GEN8_DBGC_ECO_CNTL },
	{ GEN8_DBGC_AHB_DBG_CNTL },
};

static struct adreno_coresight_register gen8_coresight_regs_cx[] = {
	{ GEN8_CX_DBGC_CFG_DBGBUS_SEL_A },
	{ GEN8_CX_DBGC_CFG_DBGBUS_SEL_B },
	{ GEN8_CX_DBGC_CFG_DBGBUS_SEL_C },
	{ GEN8_CX_DBGC_CFG_DBGBUS_SEL_D },
	{ GEN8_CX_DBGC_CFG_DBGBUS_CNTLT },
	{ GEN8_CX_DBGC_CFG_DBGBUS_CNTLM },
	{ GEN8_CX_DBGC_CFG_DBGBUS_OPL },
	{ GEN8_CX_DBGC_CFG_DBGBUS_OPE },
	{ GEN8_CX_DBGC_CFG_DBGBUS_IVTL_0 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_IVTL_1 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_IVTL_2 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_IVTL_3 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_MASKL_0 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_MASKL_1 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_MASKL_2 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_MASKL_3 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_BYTEL_0 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_BYTEL_1 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_IVTE_0 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_IVTE_1 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_IVTE_2 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_IVTE_3 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_MASKE_0 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_MASKE_1 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_MASKE_2 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_MASKE_3 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_NIBBLEE },
	{ GEN8_CX_DBGC_CFG_DBGBUS_PTRC0 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_PTRC1 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_LOADREG },
	{ GEN8_CX_DBGC_CFG_DBGBUS_IDX },
	{ GEN8_CX_DBGC_CFG_DBGBUS_CLRC },
	{ GEN8_CX_DBGC_CFG_DBGBUS_LOADIVT },
	{ GEN8_CX_DBGC_VBIF_DBG_CNTL },
	{ GEN8_CX_DBGC_DBG_LO_HI_GPIO },
	{ GEN8_CX_DBGC_EXT_TRACE_BUS_CNTL },
	{ GEN8_CX_DBGC_READ_AHB_THROUGH_DBG },
	{ GEN8_CX_DBGC_CFG_DBGBUS_TRACE_BUF1 },
	{ GEN8_CX_DBGC_CFG_DBGBUS_TRACE_BUF2 },
	{ GEN8_CX_DBGC_EVT_CFG },
	{ GEN8_CX_DBGC_EVT_INTF_SEL_0 },
	{ GEN8_CX_DBGC_EVT_INTF_SEL_1 },
	{ GEN8_CX_DBGC_PERF_ATB_CFG },
	{ GEN8_CX_DBGC_ECO_CNTL },
	{ GEN8_CX_DBGC_AHB_DBG_CNTL },
};

static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_a, &gen8_coresight_regs[0]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_b, &gen8_coresight_regs[1]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_c, &gen8_coresight_regs[2]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_d, &gen8_coresight_regs[3]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_cntlt, &gen8_coresight_regs[4]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_cntlm, &gen8_coresight_regs[5]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_opl, &gen8_coresight_regs[6]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ope, &gen8_coresight_regs[7]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_0, &gen8_coresight_regs[8]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_1, &gen8_coresight_regs[9]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_2, &gen8_coresight_regs[10]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_3, &gen8_coresight_regs[11]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_0, &gen8_coresight_regs[12]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_1, &gen8_coresight_regs[13]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_2, &gen8_coresight_regs[14]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_3, &gen8_coresight_regs[15]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_bytel_0, &gen8_coresight_regs[16]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_bytel_1, &gen8_coresight_regs[17]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_0, &gen8_coresight_regs[18]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_1, &gen8_coresight_regs[19]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_2, &gen8_coresight_regs[20]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_3, &gen8_coresight_regs[21]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_0, &gen8_coresight_regs[22]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_1, &gen8_coresight_regs[23]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_2, &gen8_coresight_regs[24]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_3, &gen8_coresight_regs[25]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_nibblee, &gen8_coresight_regs[26]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ptrc0, &gen8_coresight_regs[27]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ptrc1, &gen8_coresight_regs[28]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_loadreg, &gen8_coresight_regs[29]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_idx, &gen8_coresight_regs[30]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_clrc, &gen8_coresight_regs[31]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_loadivt, &gen8_coresight_regs[32]);
static ADRENO_CORESIGHT_ATTR(vbif_dbg_cntl, &gen8_coresight_regs[33]);
static ADRENO_CORESIGHT_ATTR(dbg_lo_hi_gpio, &gen8_coresight_regs[34]);
static ADRENO_CORESIGHT_ATTR(ext_trace_bus_cntl, &gen8_coresight_regs[35]);
static ADRENO_CORESIGHT_ATTR(read_ahb_through_dbg, &gen8_coresight_regs[36]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf1, &gen8_coresight_regs[37]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf2, &gen8_coresight_regs[38]);
static ADRENO_CORESIGHT_ATTR(evt_cfg, &gen8_coresight_regs[39]);
static ADRENO_CORESIGHT_ATTR(evt_intf_sel_0, &gen8_coresight_regs[40]);
static ADRENO_CORESIGHT_ATTR(evt_intf_sel_1, &gen8_coresight_regs[41]);
static ADRENO_CORESIGHT_ATTR(eco_cntl, &gen8_coresight_regs[42]);
static ADRENO_CORESIGHT_ATTR(ahb_dbg_cntl, &gen8_coresight_regs[43]);

/*CX debug registers*/
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_a,
				&gen8_coresight_regs_cx[0]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_b,
				&gen8_coresight_regs_cx[1]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_c,
				&gen8_coresight_regs_cx[2]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_d,
				&gen8_coresight_regs_cx[3]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_cntlt,
				&gen8_coresight_regs_cx[4]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_cntlm,
				&gen8_coresight_regs_cx[5]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_opl,
				&gen8_coresight_regs_cx[6]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ope,
				&gen8_coresight_regs_cx[7]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_0,
				&gen8_coresight_regs_cx[8]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_1,
				&gen8_coresight_regs_cx[9]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_2,
				&gen8_coresight_regs_cx[10]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_3,
				&gen8_coresight_regs_cx[11]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_0,
				&gen8_coresight_regs_cx[12]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_1,
				&gen8_coresight_regs_cx[13]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_2,
				&gen8_coresight_regs_cx[14]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_3,
				&gen8_coresight_regs_cx[15]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_bytel_0,
				&gen8_coresight_regs_cx[16]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_bytel_1,
				&gen8_coresight_regs_cx[17]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_0,
				&gen8_coresight_regs_cx[18]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_1,
				&gen8_coresight_regs_cx[19]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_2,
				&gen8_coresight_regs_cx[20]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_3,
				&gen8_coresight_regs_cx[21]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_0,
				&gen8_coresight_regs_cx[22]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_1,
				&gen8_coresight_regs_cx[23]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_2,
				&gen8_coresight_regs_cx[24]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_3,
				&gen8_coresight_regs_cx[25]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_nibblee,
				&gen8_coresight_regs_cx[26]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ptrc0,
				&gen8_coresight_regs_cx[27]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ptrc1,
				&gen8_coresight_regs_cx[28]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_loadreg,
				&gen8_coresight_regs_cx[29]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_idx,
				&gen8_coresight_regs_cx[30]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_clrc,
				&gen8_coresight_regs_cx[31]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_loadivt,
				&gen8_coresight_regs_cx[32]);
static ADRENO_CORESIGHT_ATTR(cx_vbif_dbg_cntl,
				&gen8_coresight_regs_cx[33]);
static ADRENO_CORESIGHT_ATTR(cx_dbg_lo_hi_gpio,
				&gen8_coresight_regs_cx[34]);
static ADRENO_CORESIGHT_ATTR(cx_ext_trace_bus_cntl,
				&gen8_coresight_regs_cx[35]);
static ADRENO_CORESIGHT_ATTR(cx_read_ahb_through_dbg,
				&gen8_coresight_regs_cx[36]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_trace_buf1,
				&gen8_coresight_regs_cx[37]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_trace_buf2,
				&gen8_coresight_regs_cx[38]);
static ADRENO_CORESIGHT_ATTR(cx_evt_cfg,
				&gen8_coresight_regs_cx[39]);
static ADRENO_CORESIGHT_ATTR(cx_evt_intf_sel_0,
				&gen8_coresight_regs_cx[40]);
static ADRENO_CORESIGHT_ATTR(cx_evt_intf_sel_1,
				&gen8_coresight_regs_cx[41]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_cfg,
				&gen8_coresight_regs_cx[42]);
static ADRENO_CORESIGHT_ATTR(cx_eco_cntl,
				&gen8_coresight_regs_cx[43]);
static ADRENO_CORESIGHT_ATTR(cx_ahb_dbg_cntl,
				&gen8_coresight_regs_cx[44]);

static struct attribute *gen8_coresight_attrs[] = {
	&coresight_attr_cfg_dbgbus_sel_a.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_b.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_c.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_d.attr.attr,
	&coresight_attr_cfg_dbgbus_cntlt.attr.attr,
	&coresight_attr_cfg_dbgbus_cntlm.attr.attr,
	&coresight_attr_cfg_dbgbus_opl.attr.attr,
	&coresight_attr_cfg_dbgbus_ope.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_0.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_2.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_3.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_0.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_1.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_2.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_3.attr.attr,
	&coresight_attr_cfg_dbgbus_bytel_0.attr.attr,
	&coresight_attr_cfg_dbgbus_bytel_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_0.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_2.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_3.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_0.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_1.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_2.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_3.attr.attr,
	&coresight_attr_cfg_dbgbus_nibblee.attr.attr,
	&coresight_attr_cfg_dbgbus_ptrc0.attr.attr,
	&coresight_attr_cfg_dbgbus_ptrc1.attr.attr,
	&coresight_attr_cfg_dbgbus_loadreg.attr.attr,
	&coresight_attr_cfg_dbgbus_idx.attr.attr,
	&coresight_attr_cfg_dbgbus_clrc.attr.attr,
	&coresight_attr_cfg_dbgbus_loadivt.attr.attr,
	&coresight_attr_vbif_dbg_cntl.attr.attr,
	&coresight_attr_dbg_lo_hi_gpio.attr.attr,
	&coresight_attr_ext_trace_bus_cntl.attr.attr,
	&coresight_attr_read_ahb_through_dbg.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf1.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf2.attr.attr,
	&coresight_attr_evt_cfg.attr.attr,
	&coresight_attr_evt_intf_sel_0.attr.attr,
	&coresight_attr_evt_intf_sel_1.attr.attr,
	&coresight_attr_eco_cntl.attr.attr,
	&coresight_attr_ahb_dbg_cntl.attr.attr,
	NULL,
};

/*cx*/
static struct attribute *gen8_coresight_attrs_cx[] = {
	&coresight_attr_cx_cfg_dbgbus_sel_a.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_sel_b.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_sel_c.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_sel_d.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_cntlt.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_cntlm.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_opl.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ope.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_bytel_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_bytel_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_nibblee.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ptrc0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ptrc1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_loadreg.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_idx.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_clrc.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_loadivt.attr.attr,
	&coresight_attr_cx_vbif_dbg_cntl.attr.attr,
	&coresight_attr_cx_dbg_lo_hi_gpio.attr.attr,
	&coresight_attr_cx_ext_trace_bus_cntl.attr.attr,
	&coresight_attr_cx_read_ahb_through_dbg.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_trace_buf1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_trace_buf2.attr.attr,
	&coresight_attr_cx_evt_cfg.attr.attr,
	&coresight_attr_cx_evt_intf_sel_0.attr.attr,
	&coresight_attr_cx_evt_intf_sel_1.attr.attr,
	&coresight_attr_cx_perf_atb_cfg.attr.attr,
	&coresight_attr_cx_eco_cntl.attr.attr,
	&coresight_attr_cx_ahb_dbg_cntl.attr.attr,
	NULL,
};

static const struct attribute_group gen8_coresight_group = {
	.attrs = gen8_coresight_attrs,
};

static const struct attribute_group *gen8_coresight_groups[] = {
	&gen8_coresight_group,
	NULL,
};

static const struct attribute_group gen8_coresight_group_cx = {
	.attrs = gen8_coresight_attrs_cx,
};

static const struct attribute_group *gen8_coresight_groups_cx[] = {
	&gen8_coresight_group_cx,
	NULL,
};

static const struct adreno_coresight gen8_coresight = {
	.registers = gen8_coresight_regs,
	.count = ARRAY_SIZE(gen8_coresight_regs),
	.groups = gen8_coresight_groups,
};

static const struct adreno_coresight gen8_coresight_cx = {
	.registers = gen8_coresight_regs_cx,
	.count = ARRAY_SIZE(gen8_coresight_regs_cx),
	.groups = gen8_coresight_groups_cx,
};

static int name_match(struct device *dev, void *data)
{
	char *child_name = data;

	return strcmp(child_name, dev_name(dev)) == 0;
}

void gen8_coresight_init(struct adreno_device *adreno_dev)
{
	struct adreno_funnel_device *funnel_gfx = &adreno_dev->funnel_gfx;
	struct device *amba_dev;

	/* Find the amba funnel device associated with gfx coresight funnel */
	amba_dev = bus_find_device_by_name(&amba_bustype, NULL, "10963000.funnel");
	if (!amba_dev)
		return;

	funnel_gfx->funnel_dev = device_find_child(amba_dev, "coresight-funnel-gfx", name_match);
	if (funnel_gfx->funnel_dev == NULL)
		return;

	funnel_gfx->funnel_csdev = to_coresight_device(funnel_gfx->funnel_dev);
	if (funnel_gfx->funnel_csdev == NULL)
		return;

	/*
	 * Since coresight_funnel_gfx component is in graphics block, GPU has to be powered up
	 * before enabling the funnel. Currently the generic coresight driver doesn't handle that.
	 * Override the funnel ops set by coresight driver with graphics funnel ops, so that the
	 * GPU can be brought up before enabling the funnel.
	 */
	funnel_gfx->funnel_ops = funnel_gfx->funnel_csdev->ops;
	funnel_gfx->funnel_csdev->ops = NULL;

	/*
	 * The read-only sysfs node (funnel_ctrl) associated with gfx funnel reads the control
	 * register and could cause a NOC error when gpu is in slumber. Since we do not require
	 * such node, remove the attribute groups for the funnel.
	 */
	sysfs_remove_groups(&funnel_gfx->funnel_dev->kobj, funnel_gfx->funnel_csdev->dev.groups);

	adreno_coresight_add_device(adreno_dev, "qcom,gpu-coresight-gx",
		&gen8_coresight, &adreno_dev->gx_coresight);

	adreno_coresight_add_device(adreno_dev, "qcom,gpu-coresight-cx",
		&gen8_coresight_cx, &adreno_dev->cx_coresight);
}
