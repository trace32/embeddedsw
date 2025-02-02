/******************************************************************************
* Copyright (c) 2018 - 2022 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

/*****************************************************************************/
/**
*
* @file xpsmfw_power.c
*
* This file contains power handler functions for PS Power islands and FPD
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver	Who	Date		Changes
* ---- ---- -------- ------------------------------
* 1.00	rp	07/13/2018 	Initial release
*
* </pre>
*
* @note
*
******************************************************************************/

#include "xpsmfw_api.h"
#include "xpsmfw_default.h"
#include "xpsmfw_power.h"
#include "xpsmfw_init.h"
#include "apu.h"
#include "psmx_global.h"
#include "rpu.h"
#include "crl.h"
#include "crf.h"
#include "pmc_global.h"
#include <assert.h>
#define CHECK_BIT(reg, mask)	(((reg) & (mask)) == (mask))

/**
 * NOTE: Older PsmToPlmEvent version (0x1U) only consists Event array
 *       while new version (0x2U) adds CpuIdleFlag and ResumeAddress in it.
 */
#define PSM_TO_PLM_EVENT_VERSION		(0x2U)
#define PWR_UP_EVT						(0x1U)
#define PWR_DWN_EVT                     (0x100U)
static volatile struct PsmToPlmEvent_t PsmToPlmEvent = {
	.Version	= PSM_TO_PLM_EVENT_VERSION,
	.Event		= {0x0},
	.CpuIdleFlag 	= {0x0},
	.ResumeAddress 	= {0x0},
};

static u8 ApuClusterState[4U] = {0U};

static struct XPsmFwPwrCtrl_t Acpu0_Core0PwrCtrl = {
	.Id = ACPU_0,
	.ResetCfgAddr = APU_CLUSTER0_RVBARADDR0L,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_APU0_CORE0_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_APU0_CORE0_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_APU0_CORE0_PWR_STATUS,
	.ClkCtrlAddr = PSX_CRF_ACPU0_CLK_CTRL,
	.ClkCtrlMask = PSX_CRF_ACPU_CTRL_CLKACT_MASK,
	.ClkPropTime = XPSMFW_ACPU_CTRL_CLK_PROP_TIME,
	.RstAddr = PSX_CRF_RST_APU0,
	.WarmRstMask = PSX_CRF_RST_APU_CORE0_WARM_RST_MASK,
	.ClusterPstate = APU_PCLI_CLUSTER_0_PSTATE,
	.ClusterPstateMask = APU_PCLI_CLUSTER_PSTATE_MASK,
	.ClusterPstateValue = APU_PCLI_CLUSTER_PSTATE_VAL,
	.ClusterPreq = APU_PCLI_CLUSTER_0_PREQ,
	.ClusterPreqMask = APU_PCLI_CLUSTER_PREQ_MASK,
	.CorePstate = APU_PCLI_CORE_0_PSTATE,
	.CorePstateMask = APU_PCLI_CORE_PSTATE_MASK,
	.CorePstateVal = APU_PCLI_CORE_PSTATE_VAL,
	.CorePreq = APU_PCLI_CORE_0_PREQ,
	.CorePreqMask = APU_PCLI_CORE_PREQ_MASK,
	.CorePactive = APU_PCLI_CORE_0_PACTIVE,
	.CorePacceptMask = APU_PCLI_CORE_PACCEPT_MASK,
	.ClusterPactive = APU_PCLI_CLUSTER_0_PACTIVE,
	.ClusterPacceptMask = APU_PCLI_CLUSTER_PACCEPT_MASK,
	.ClusterId = CLUSTER_0,
};

static struct XPsmFwPwrCtrl_t Acpu0_Core1PwrCtrl = {
	.Id = ACPU_1,
	.ResetCfgAddr = APU_CLUSTER0_RVBARADDR1L,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_APU0_CORE1_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_APU0_CORE1_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_APU0_CORE1_PWR_STATUS,
	.ClkCtrlAddr = PSX_CRF_ACPU0_CLK_CTRL,
	.ClkCtrlMask = PSX_CRF_ACPU_CTRL_CLKACT_MASK,
	.ClkPropTime = XPSMFW_ACPU_CTRL_CLK_PROP_TIME,
	.RstAddr = PSX_CRF_RST_APU0,
	.WarmRstMask = PSX_CRF_RST_APU_CORE1_WARM_RST_MASK,
	.ClusterPstate = APU_PCLI_CLUSTER_0_PSTATE,
	.ClusterPstateMask = APU_PCLI_CLUSTER_PSTATE_MASK,
	.ClusterPstateValue = APU_PCLI_CLUSTER_PSTATE_VAL,
	.ClusterPreq = APU_PCLI_CLUSTER_0_PREQ,
	.ClusterPreqMask = APU_PCLI_CLUSTER_PREQ_MASK,
	.CorePstate = APU_PCLI_CORE_1_PSTATE,
	.CorePstateMask = APU_PCLI_CORE_PSTATE_MASK,
	.CorePstateVal = APU_PCLI_CORE_PSTATE_VAL,
	.CorePreq = APU_PCLI_CORE_1_PREQ,
	.CorePreqMask = APU_PCLI_CORE_PREQ_MASK,
	.CorePactive = APU_PCLI_CORE_1_PACTIVE,
	.CorePacceptMask = APU_PCLI_CORE_PACCEPT_MASK,
	.ClusterPactive = APU_PCLI_CLUSTER_0_PACTIVE,
	.ClusterPacceptMask = APU_PCLI_CLUSTER_PACCEPT_MASK,
	.ClusterId = CLUSTER_0,
};

static struct XPsmFwPwrCtrl_t Acpu0_Core2PwrCtrl = {
	.Id = ACPU_2,
	.ResetCfgAddr = APU_CLUSTER0_RVBARADDR2L,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_APU0_CORE2_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_APU0_CORE2_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_APU0_CORE2_PWR_STATUS,
	.ClkCtrlAddr = PSX_CRF_ACPU0_CLK_CTRL,
	.ClkCtrlMask = PSX_CRF_ACPU_CTRL_CLKACT_MASK,
	.ClkPropTime = XPSMFW_ACPU_CTRL_CLK_PROP_TIME,
	.RstAddr = PSX_CRF_RST_APU0,
	.WarmRstMask = PSX_CRF_RST_APU_CORE2_WARM_RST_MASK,
	.ClusterPstate = APU_PCLI_CLUSTER_0_PSTATE,
	.ClusterPstateMask = APU_PCLI_CLUSTER_PSTATE_MASK,
	.ClusterPstateValue = APU_PCLI_CLUSTER_PSTATE_VAL,
	.ClusterPreq = APU_PCLI_CLUSTER_0_PREQ,
	.ClusterPreqMask = APU_PCLI_CLUSTER_PREQ_MASK,
	.CorePstate = APU_PCLI_CORE_2_PSTATE,
	.CorePstateMask = APU_PCLI_CORE_PSTATE_MASK,
	.CorePstateVal = APU_PCLI_CORE_PSTATE_VAL,
	.CorePreq = APU_PCLI_CORE_2_PREQ,
	.CorePreqMask = APU_PCLI_CORE_PREQ_MASK,
	.CorePactive = APU_PCLI_CORE_2_PACTIVE,
	.CorePacceptMask = APU_PCLI_CORE_PACCEPT_MASK,
	.ClusterPactive = APU_PCLI_CLUSTER_0_PACTIVE,
	.ClusterPacceptMask = APU_PCLI_CLUSTER_PACCEPT_MASK,
	.ClusterId = CLUSTER_0,
};

static struct XPsmFwPwrCtrl_t Acpu0_Core3PwrCtrl = {
	.Id = ACPU_3,
	.ResetCfgAddr = APU_CLUSTER0_RVBARADDR3L,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_APU0_CORE3_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_APU0_CORE3_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_APU0_CORE3_PWR_STATUS,
	.ClkCtrlAddr = PSX_CRF_ACPU0_CLK_CTRL,
	.ClkCtrlMask = PSX_CRF_ACPU_CTRL_CLKACT_MASK,
	.ClkPropTime = XPSMFW_ACPU_CTRL_CLK_PROP_TIME,
	.RstAddr = PSX_CRF_RST_APU0,
	.WarmRstMask = PSX_CRF_RST_APU_CORE3_WARM_RST_MASK,
	.ClusterPstate = APU_PCLI_CLUSTER_0_PSTATE,
	.ClusterPstateMask = APU_PCLI_CLUSTER_PSTATE_MASK,
	.ClusterPstateValue = APU_PCLI_CLUSTER_PSTATE_VAL,
	.ClusterPreq = APU_PCLI_CLUSTER_0_PREQ,
	.ClusterPreqMask = APU_PCLI_CLUSTER_PREQ_MASK,
	.CorePstate = APU_PCLI_CORE_3_PSTATE,
	.CorePstateMask = APU_PCLI_CORE_PSTATE_MASK,
	.CorePstateVal = APU_PCLI_CORE_PSTATE_VAL,
	.CorePreq = APU_PCLI_CORE_3_PREQ,
	.CorePreqMask = APU_PCLI_CORE_PREQ_MASK,
	.CorePactive = APU_PCLI_CORE_3_PACTIVE,
	.CorePacceptMask = APU_PCLI_CORE_PACCEPT_MASK,
	.ClusterPactive = APU_PCLI_CLUSTER_0_PACTIVE,
	.ClusterPacceptMask = APU_PCLI_CLUSTER_PACCEPT_MASK,
	.ClusterId = CLUSTER_0,
};

static struct XPsmFwPwrCtrl_t Acpu1_Core0PwrCtrl = {
	.Id = ACPU_4,
	.ResetCfgAddr = APU_CLUSTER1_RVBARADDR0L,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_APU1_CORE0_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_APU1_CORE0_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_APU1_CORE0_PWR_STATUS,
	.ClkCtrlAddr = PSX_CRF_ACPU1_CLK_CTRL,
	.ClkCtrlMask = PSX_CRF_ACPU_CTRL_CLKACT_MASK,
	.ClkPropTime = XPSMFW_ACPU_CTRL_CLK_PROP_TIME,
	.RstAddr = PSX_CRF_RST_APU1,
	.WarmRstMask = PSX_CRF_RST_APU_CORE0_WARM_RST_MASK,
	.ClusterPstate = APU_PCLI_CLUSTER_1_PSTATE,
	.ClusterPstateMask = APU_PCLI_CLUSTER_PSTATE_MASK,
	.ClusterPstateValue = APU_PCLI_CLUSTER_PSTATE_VAL,
	.ClusterPreq = APU_PCLI_CLUSTER_1_PREQ,
	.ClusterPreqMask = APU_PCLI_CLUSTER_PREQ_MASK,
	.CorePstate = APU_PCLI_CORE_4_PSTATE,
	.CorePstateMask = APU_PCLI_CORE_PSTATE_MASK,
	.CorePstateVal = APU_PCLI_CORE_PSTATE_VAL,
	.CorePreq = APU_PCLI_CORE_4_PREQ,
	.CorePreqMask = APU_PCLI_CORE_PREQ_MASK,
	.CorePactive = APU_PCLI_CORE_4_PACTIVE,
	.CorePacceptMask = APU_PCLI_CORE_PACCEPT_MASK,
	.ClusterPactive = APU_PCLI_CLUSTER_1_PACTIVE,
	.ClusterPacceptMask = APU_PCLI_CLUSTER_PACCEPT_MASK,
	.ClusterId = CLUSTER_1,
};

static struct XPsmFwPwrCtrl_t Acpu1_Core1PwrCtrl = {
	.Id = ACPU_5,
	.ResetCfgAddr = APU_CLUSTER1_RVBARADDR1L,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_APU1_CORE1_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_APU1_CORE1_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_APU1_CORE1_PWR_STATUS,
	.ClkCtrlAddr = PSX_CRF_ACPU1_CLK_CTRL,
	.ClkCtrlMask = PSX_CRF_ACPU_CTRL_CLKACT_MASK,
	.ClkPropTime = XPSMFW_ACPU_CTRL_CLK_PROP_TIME,
	.RstAddr = PSX_CRF_RST_APU1,
	.WarmRstMask = PSX_CRF_RST_APU_CORE1_WARM_RST_MASK,
	.ClusterPstate = APU_PCLI_CLUSTER_1_PSTATE,
	.ClusterPstateMask = APU_PCLI_CLUSTER_PSTATE_MASK,
	.ClusterPstateValue = APU_PCLI_CLUSTER_PSTATE_VAL,
	.ClusterPreq = APU_PCLI_CLUSTER_1_PREQ,
	.ClusterPreqMask = APU_PCLI_CLUSTER_PREQ_MASK,
	.CorePstate = APU_PCLI_CORE_5_PSTATE,
	.CorePstateMask = APU_PCLI_CORE_PSTATE_MASK,
	.CorePstateVal = APU_PCLI_CORE_PSTATE_VAL,
	.CorePreq = APU_PCLI_CORE_5_PREQ,
	.CorePreqMask = APU_PCLI_CORE_PREQ_MASK,
	.CorePactive = APU_PCLI_CORE_5_PACTIVE,
	.CorePacceptMask = APU_PCLI_CORE_PACCEPT_MASK,
	.ClusterPactive = APU_PCLI_CLUSTER_1_PACTIVE,
	.ClusterPacceptMask = APU_PCLI_CLUSTER_PACCEPT_MASK,
	.ClusterId = CLUSTER_1,
};

static struct XPsmFwPwrCtrl_t Acpu1_Core2PwrCtrl = {
	.Id = ACPU_6,
	.ResetCfgAddr = APU_CLUSTER1_RVBARADDR2L,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_APU1_CORE2_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_APU1_CORE2_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_APU1_CORE2_PWR_STATUS,
	.ClkCtrlAddr = PSX_CRF_ACPU1_CLK_CTRL,
	.ClkCtrlMask = PSX_CRF_ACPU_CTRL_CLKACT_MASK,
	.ClkPropTime = XPSMFW_ACPU_CTRL_CLK_PROP_TIME,
	.RstAddr = PSX_CRF_RST_APU1,
	.WarmRstMask = PSX_CRF_RST_APU_CORE2_WARM_RST_MASK,
	.ClusterPstate = APU_PCLI_CLUSTER_1_PSTATE,
	.ClusterPstateMask = APU_PCLI_CLUSTER_PSTATE_MASK,
	.ClusterPstateValue = APU_PCLI_CLUSTER_PSTATE_VAL,
	.ClusterPreq = APU_PCLI_CLUSTER_1_PREQ,
	.ClusterPreqMask = APU_PCLI_CLUSTER_PREQ_MASK,
	.CorePstate = APU_PCLI_CORE_6_PSTATE,
	.CorePstateMask = APU_PCLI_CORE_PSTATE_MASK,
	.CorePstateVal = APU_PCLI_CORE_PSTATE_VAL,
	.CorePreq = APU_PCLI_CORE_6_PREQ,
	.CorePreqMask = APU_PCLI_CORE_PREQ_MASK,
	.CorePactive = APU_PCLI_CORE_6_PACTIVE,
	.CorePacceptMask = APU_PCLI_CORE_PACCEPT_MASK,
	.ClusterPactive = APU_PCLI_CLUSTER_1_PACTIVE,
	.ClusterPacceptMask = APU_PCLI_CLUSTER_PACCEPT_MASK,
	.ClusterId = CLUSTER_1,
};

static struct XPsmFwPwrCtrl_t Acpu1_Core3PwrCtrl = {
	.Id = ACPU_7,
	.ResetCfgAddr = APU_CLUSTER1_RVBARADDR3L,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_APU1_CORE3_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_APU1_CORE3_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_APU1_CORE3_PWR_STATUS,
	.ClkCtrlAddr = PSX_CRF_ACPU1_CLK_CTRL,
	.ClkCtrlMask = PSX_CRF_ACPU_CTRL_CLKACT_MASK,
	.ClkPropTime = XPSMFW_ACPU_CTRL_CLK_PROP_TIME,
	.RstAddr = PSX_CRF_RST_APU1,
	.WarmRstMask = PSX_CRF_RST_APU_CORE3_WARM_RST_MASK,
	.ClusterPstate = APU_PCLI_CLUSTER_1_PSTATE,
	.ClusterPstateMask = APU_PCLI_CLUSTER_PSTATE_MASK,
	.ClusterPstateValue = APU_PCLI_CLUSTER_PSTATE_VAL,
	.ClusterPreq = APU_PCLI_CLUSTER_1_PREQ,
	.ClusterPreqMask = APU_PCLI_CLUSTER_PREQ_MASK,
	.CorePstate = APU_PCLI_CORE_7_PSTATE,
	.CorePstateMask = APU_PCLI_CORE_PSTATE_MASK,
	.CorePstateVal = APU_PCLI_CORE_PSTATE_VAL,
	.CorePreq = APU_PCLI_CORE_7_PREQ,
	.CorePreqMask = APU_PCLI_CORE_PREQ_MASK,
	.CorePactive = APU_PCLI_CORE_7_PACTIVE,
	.CorePacceptMask = APU_PCLI_CORE_PACCEPT_MASK,
	.ClusterPactive = APU_PCLI_CLUSTER_1_PACTIVE,
	.ClusterPacceptMask = APU_PCLI_CLUSTER_PACCEPT_MASK,
	.ClusterId = CLUSTER_1,
};

static struct XPsmFwPwrCtrl_t Rpu0_Core0PwrCtrl = {
	.Id = RPU0_0,
	.ResetCfgAddr = RPU_RPU0_CORE0_CFG0,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_RPU_A_CORE0_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_RPU_A_CORE0_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_RPU_A_CORE0_PWR_STATUS,
	.ClkCtrlAddr = CRL_CPU_R5_CTRL,
	.ClkCtrlMask = CRL_CPU_R5_CTRL_CLKACT_CORE_MASK,
	.ClkPropTime = XPSMFW_RPU_CTRL_CLK_PROP_TIME,
	.RstCtrlMask = PSX_CRL_RST_RPU_CORE0A_MASK,
	.CorePstate = LPD_SLCR_RPU_PCIL_A0_PS,
	.CorePstateMask = LPD_SLCR_RPU_PCIL_A0_PS_PSTATE_MASK,
	.CorePreq = LPD_SLCR_RPU_PCIL_A0_PR,
	.CorePreqMask = LPD_SLCR_RPU_PCIL_A0_PR_PREQ_MASK,
	.CorePactive = LPD_SLCR_RPU_PCIL_A0_PA,
	.CorePactiveMask = LPD_SLCR_RPU_PCIL_A0_PA_PACTIVE_MASK,
	.CorePacceptMask = LPD_SLCR_RPU_PCIL_A0_PA_PACCEPT_MASK,
	.IntrDisableAddr = LPD_SLCR_RPU_PCIL_A0_IDS,
	.IntrDisableMask = LPD_SLCR_RPU_PCIL_A0_IDS_PACTIVE1_MASK,
	.ClusterId = CLUSTER_0,
	.VectTableAddr = PSX_RPU_CLUSTER_A0_CORE_0_VECTABLE,

};

static struct XPsmFwPwrCtrl_t Rpu0_Core1PwrCtrl = {
	.Id = RPU0_1,
	.ResetCfgAddr = RPU_RPU0_CORE1_CFG0,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_RPU_A_CORE1_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_RPU_A_CORE1_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_RPU_A_CORE1_PWR_STATUS,
	.ClkCtrlAddr = CRL_CPU_R5_CTRL,
	.ClkCtrlMask = CRL_CPU_R5_CTRL_CLKACT_CORE_MASK,
	.ClkPropTime = XPSMFW_RPU_CTRL_CLK_PROP_TIME,
	.RstCtrlMask = PSX_CRL_RST_RPU_CORE1A_MASK,
	.CorePstate = LPD_SLCR_RPU_PCIL_A1_PS,
	.CorePstateMask = LPD_SLCR_RPU_PCIL_A1_PS_PSTATE_MASK,
	.CorePreq = LPD_SLCR_RPU_PCIL_A1_PR,
	.CorePreqMask = LPD_SLCR_RPU_PCIL_A1_PR_PREQ_MASK,
	.CorePactive = LPD_SLCR_RPU_PCIL_A1_PA,
	.CorePactiveMask = LPD_SLCR_RPU_PCIL_A1_PA_PACTIVE_MASK,
	.CorePacceptMask = LPD_SLCR_RPU_PCIL_A1_PA_PACCEPT_MASK,
	.IntrDisableAddr = LPD_SLCR_RPU_PCIL_A1_IDS,
	.IntrDisableMask = LPD_SLCR_RPU_PCIL_A1_IDS_PACTIVE1_MASK,
	.ClusterId = CLUSTER_0,
	.VectTableAddr = PSX_RPU_CLUSTER_A1_CORE_1_VECTABLE,

};

static struct XPsmFwPwrCtrl_t Rpu1_Core0PwrCtrl = {
	.Id = RPU1_0,
	.ResetCfgAddr = RPU_RPU1_CORE0_CFG0,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_RPU_B_CORE0_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_RPU_B_CORE0_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_RPU_B_CORE0_PWR_STATUS,
	.ClkCtrlAddr = CRL_CPU_R5_CTRL,
	.ClkCtrlMask = CRL_CPU_R5_CTRL_CLKACT_CORE_MASK,
	.ClkPropTime = XPSMFW_RPU_CTRL_CLK_PROP_TIME,
	.RstCtrlMask = PSX_CRL_RST_RPU_CORE0B_MASK,
	.CorePstate = LPD_SLCR_RPU_PCIL_B0_PS,
	.CorePstateMask = LPD_SLCR_RPU_PCIL_B0_PS_PSTATE_MASK,
	.CorePreq = LPD_SLCR_RPU_PCIL_B0_PR,
	.CorePreqMask = LPD_SLCR_RPU_PCIL_B0_PR_PREQ_MASK,
	.CorePactive = LPD_SLCR_RPU_PCIL_B0_PA,
	.CorePactiveMask = LPD_SLCR_RPU_PCIL_B0_PA_PACTIVE_MASK,
	.CorePacceptMask = LPD_SLCR_RPU_PCIL_B0_PA_PACCEPT_MASK,
	.IntrDisableAddr = LPD_SLCR_RPU_PCIL_B0_IDS,
	.IntrDisableMask = LPD_SLCR_RPU_PCIL_B0_IDS_PACTIVE1_MASK,
	.ClusterId = CLUSTER_1,
	.VectTableAddr = PSX_RPU_CLUSTER_B0_CORE_0_VECTABLE,
};

static struct XPsmFwPwrCtrl_t Rpu1_Core1PwrCtrl = {
	.Id = RPU1_1,
	.ResetCfgAddr = RPU_RPU1_CORE1_CFG0,
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_RPU_B_CORE1_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_RPU_B_CORE1_PWR_CNTRL,
	.PwrStatusAddr = PSMX_LOCAL_REG_RPU_B_CORE1_PWR_STATUS,
	.ClkCtrlAddr = CRL_CPU_R5_CTRL,
	.ClkCtrlMask = CRL_CPU_R5_CTRL_CLKACT_CORE_MASK,
	.ClkPropTime = XPSMFW_RPU_CTRL_CLK_PROP_TIME,
	.RstCtrlMask = PSX_CRL_RST_RPU_CORE1B_MASK,
	.CorePstate = LPD_SLCR_RPU_PCIL_B1_PS,
	.CorePstateMask = LPD_SLCR_RPU_PCIL_B1_PS_PSTATE_MASK,
	.CorePreq = LPD_SLCR_RPU_PCIL_B1_PR,
	.CorePreqMask = LPD_SLCR_RPU_PCIL_B1_PR_PREQ_MASK,
	.CorePactive = LPD_SLCR_RPU_PCIL_B1_PA,
	.CorePactiveMask = LPD_SLCR_RPU_PCIL_B1_PA_PACTIVE_MASK,
	.CorePacceptMask = LPD_SLCR_RPU_PCIL_B1_PA_PACCEPT_MASK,
	.IntrDisableAddr = LPD_SLCR_RPU_PCIL_B1_IDS,
	.IntrDisableMask = LPD_SLCR_RPU_PCIL_B1_IDS_PACTIVE1_MASK,
	.ClusterId = CLUSTER_1,
	.VectTableAddr = PSX_RPU_CLUSTER_B1_CORE_1_VECTABLE,
};

static struct XPsmFwMemPwrCtrl_t Ocm_B0_I0_PwrCtrl = {
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_OCM_B0_I0_MASK,
	.ChipEnAddr = PSMX_LOCAL_REG_OCM_CE_CNTRL,
	.ChipEnMask = PSMX_LOCAL_REG_OCM_CE_CNTRL_B0_I0_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_OCM_PWR_CNTRL,
	.PwrCtrlMask = PSMX_LOCAL_REG_OCM_PWR_CNTRL_B0_I0_MASK,
	.PwrStatusAddr = PSMX_LOCAL_REG_OCM_PWR_STATUS,
	.PwrStatusMask = PSMX_LOCAL_REG_OCM_PWR_STATUS_B0_I0_MASK,
	.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND0_MASK,
	.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND0_RET_MASK,
	.RetCtrlAddr = PSMX_LOCAL_REG_OCM_RET_CNTRL,
	.RetCtrlMask = PSMX_LOCAL_REG_OCM_RET_CNTRL_B0_I0_MASK,
	.PwrStateAckTimeout = XPSMFW_OCM_B0_I0_PWR_STATE_ACK_TIMEOUT,
	.PwrUpWaitTime = XPSMFW_OCM_B0_I0_PWR_UP_WAIT_TIME,
};

static struct XPsmFwMemPwrCtrl_t Ocm_B0_I1_PwrCtrl = {
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_OCM_B0_I1_MASK,
	.ChipEnAddr = PSMX_LOCAL_REG_OCM_CE_CNTRL,
	.ChipEnMask = PSMX_LOCAL_REG_OCM_CE_CNTRL_B0_I1_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_OCM_PWR_CNTRL,
	.PwrCtrlMask = PSMX_LOCAL_REG_OCM_PWR_CNTRL_B0_I1_MASK,
	.PwrStatusAddr = PSMX_LOCAL_REG_OCM_PWR_STATUS,
	.PwrStatusMask = PSMX_LOCAL_REG_OCM_PWR_STATUS_B0_I1_MASK,
	.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND1_MASK,
	.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND1_RET_MASK,
	.RetCtrlAddr = PSMX_LOCAL_REG_OCM_RET_CNTRL,
	.RetCtrlMask = PSMX_LOCAL_REG_OCM_RET_CNTRL_B0_I1_MASK,
	.PwrStateAckTimeout = XPSMFW_OCM_B0_I1_PWR_STATE_ACK_TIMEOUT,
	.PwrUpWaitTime = XPSMFW_OCM_B0_I1_PWR_UP_WAIT_TIME,
};

static struct XPsmFwMemPwrCtrl_t Ocm_B0_I2_PwrCtrl = {
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_OCM_B0_I2_MASK,
	.ChipEnAddr = PSMX_LOCAL_REG_OCM_CE_CNTRL,
	.ChipEnMask = PSMX_LOCAL_REG_OCM_CE_CNTRL_B0_I2_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_OCM_PWR_CNTRL,
	.PwrCtrlMask = PSMX_LOCAL_REG_OCM_PWR_CNTRL_B0_I2_MASK,
	.PwrStatusAddr = PSMX_LOCAL_REG_OCM_PWR_STATUS,
	.PwrStatusMask = PSMX_LOCAL_REG_OCM_PWR_STATUS_B0_I2_MASK,
	.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND2_MASK,
	.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND2_RET_MASK,
	.RetCtrlAddr = PSMX_LOCAL_REG_OCM_RET_CNTRL,
	.RetCtrlMask = PSMX_LOCAL_REG_OCM_RET_CNTRL_B0_I2_MASK,
	.PwrStateAckTimeout = XPSMFW_OCM_B0_I2_PWR_STATE_ACK_TIMEOUT,
	.PwrUpWaitTime = XPSMFW_OCM_B0_I2_PWR_UP_WAIT_TIME,
};

static struct XPsmFwMemPwrCtrl_t Ocm_B0_I3_PwrCtrl = {
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_OCM_B0_I3_MASK,
	.ChipEnAddr = PSMX_LOCAL_REG_OCM_CE_CNTRL,
	.ChipEnMask = PSMX_LOCAL_REG_OCM_CE_CNTRL_B0_I3_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_OCM_PWR_CNTRL,
	.PwrCtrlMask = PSMX_LOCAL_REG_OCM_PWR_CNTRL_B0_I3_MASK,
	.PwrStatusAddr = PSMX_LOCAL_REG_OCM_PWR_STATUS,
	.PwrStatusMask = PSMX_LOCAL_REG_OCM_PWR_STATUS_B0_I3_MASK,
	.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND3_MASK,
	.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND3_RET_MASK,
	.RetCtrlAddr = PSMX_LOCAL_REG_OCM_RET_CNTRL,
	.RetCtrlMask = PSMX_LOCAL_REG_OCM_RET_CNTRL_B0_I3_MASK,
	.PwrStateAckTimeout = XPSMFW_OCM_B0_I3_PWR_STATE_ACK_TIMEOUT,
	.PwrUpWaitTime = XPSMFW_OCM_B0_I3_PWR_UP_WAIT_TIME,
};

static struct XPsmFwMemPwrCtrl_t Ocm_B1_I0_PwrCtrl = {
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_OCM_B1_I0_MASK,
	.ChipEnAddr = PSMX_LOCAL_REG_OCM_CE_CNTRL,
	.ChipEnMask = PSMX_LOCAL_REG_OCM_CE_CNTRL_B1_I0_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_OCM_PWR_CNTRL,
	.PwrCtrlMask = PSMX_LOCAL_REG_OCM_PWR_CNTRL_B1_I0_MASK,
	.PwrStatusAddr = PSMX_LOCAL_REG_OCM_PWR_STATUS,
	.PwrStatusMask = PSMX_LOCAL_REG_OCM_PWR_STATUS_B1_I0_MASK,
	.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND4_MASK,
	.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND4_RET_MASK,
	.RetCtrlAddr = PSMX_LOCAL_REG_OCM_RET_CNTRL,
	.RetCtrlMask = PSMX_LOCAL_REG_OCM_RET_CNTRL_B1_I0_MASK,
	.PwrStateAckTimeout = XPSMFW_OCM_B1_I0_PWR_STATE_ACK_TIMEOUT,
	.PwrUpWaitTime = XPSMFW_OCM_B1_I0_PWR_UP_WAIT_TIME,
};

static struct XPsmFwMemPwrCtrl_t Ocm_B1_I1_PwrCtrl = {
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_OCM_B1_I1_MASK,
	.ChipEnAddr = PSMX_LOCAL_REG_OCM_CE_CNTRL,
	.ChipEnMask = PSMX_LOCAL_REG_OCM_CE_CNTRL_B1_I1_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_OCM_PWR_CNTRL,
	.PwrCtrlMask = PSMX_LOCAL_REG_OCM_PWR_CNTRL_B1_I1_MASK,
	.PwrStatusAddr = PSMX_LOCAL_REG_OCM_PWR_STATUS,
	.PwrStatusMask = PSMX_LOCAL_REG_OCM_PWR_STATUS_B1_I1_MASK,
	.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND5_MASK,
	.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND5_RET_MASK,
	.RetCtrlAddr = PSMX_LOCAL_REG_OCM_RET_CNTRL,
	.RetCtrlMask = PSMX_LOCAL_REG_OCM_RET_CNTRL_B1_I1_MASK,
	.PwrStateAckTimeout = XPSMFW_OCM_B1_I1_PWR_STATE_ACK_TIMEOUT,
	.PwrUpWaitTime = XPSMFW_OCM_B1_I1_PWR_UP_WAIT_TIME,
};

static struct XPsmFwMemPwrCtrl_t Ocm_B1_I2_PwrCtrl = {
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_OCM_B1_I2_MASK,
	.ChipEnAddr = PSMX_LOCAL_REG_OCM_CE_CNTRL,
	.ChipEnMask = PSMX_LOCAL_REG_OCM_CE_CNTRL_B1_I2_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_OCM_PWR_CNTRL,
	.PwrCtrlMask = PSMX_LOCAL_REG_OCM_PWR_CNTRL_B1_I2_MASK,
	.PwrStatusAddr = PSMX_LOCAL_REG_OCM_PWR_STATUS,
	.PwrStatusMask = PSMX_LOCAL_REG_OCM_PWR_STATUS_B1_I2_MASK,
	.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND6_MASK,
	.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND6_RET_MASK,
	.RetCtrlAddr = PSMX_LOCAL_REG_OCM_RET_CNTRL,
	.RetCtrlMask = PSMX_LOCAL_REG_OCM_RET_CNTRL_B1_I2_MASK,
	.PwrStateAckTimeout = XPSMFW_OCM_B1_I2_PWR_STATE_ACK_TIMEOUT,
	.PwrUpWaitTime = XPSMFW_OCM_B1_I2_PWR_UP_WAIT_TIME,
};

static struct XPsmFwMemPwrCtrl_t Ocm_B1_I3_PwrCtrl = {
	.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_OCM_B1_I3_MASK,
	.ChipEnAddr = PSMX_LOCAL_REG_OCM_CE_CNTRL,
	.ChipEnMask = PSMX_LOCAL_REG_OCM_CE_CNTRL_B1_I3_MASK,
	.PwrCtrlAddr = PSMX_LOCAL_REG_OCM_PWR_CNTRL,
	.PwrCtrlMask = PSMX_LOCAL_REG_OCM_PWR_CNTRL_B1_I3_MASK,
	.PwrStatusAddr = PSMX_LOCAL_REG_OCM_PWR_STATUS,
	.PwrStatusMask = PSMX_LOCAL_REG_OCM_PWR_STATUS_B1_I3_MASK,
	.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND7_MASK,
	.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND7_RET_MASK,
	.RetCtrlAddr = PSMX_LOCAL_REG_OCM_RET_CNTRL,
	.RetCtrlMask = PSMX_LOCAL_REG_OCM_RET_CNTRL_B1_I3_MASK,
	.PwrStateAckTimeout = XPSMFW_OCM_B1_I3_PWR_STATE_ACK_TIMEOUT,
	.PwrUpWaitTime = XPSMFW_OCM_B1_I3_PWR_UP_WAIT_TIME,
};

static struct XPsmTcmPwrCtrl_t TcmA0PwrCtrl = {
	.TcmMemPwrCtrl = {
		.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_TCMA0_MASK,
		.ChipEnAddr = PSMX_LOCAL_REG_TCM_CE_CNTRL,
		.ChipEnMask = PSMX_LOCAL_REG_TCM_CE_CNTRL_TCMA0_MASK,
		.PwrCtrlAddr = PSMX_LOCAL_REG_TCM_PWR_CNTRL,
		.PwrCtrlMask = PSMX_LOCAL_REG_TCM_PWR_CNTRL_TCMA0_MASK,
		.PwrStatusAddr = PSMX_LOCAL_REG_TCM_PWR_STATUS,
		.PwrStatusMask = PSMX_LOCAL_REG_TCM_PWR_STATUS_TCMA0_MASK,
		.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_TCM0A_MASK,
		.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_TCM0A_RET_MASK,
		.PwrStateAckTimeout = XPSMFW_TCM0A_PWR_STATE_ACK_TIMEOUT,
		.PwrUpWaitTime = XPSMFW_TCM0A_PWR_UP_WAIT_TIME,

	},

	.Id = TCM_A_0,
	.PowerState = STATE_POWER_DEFAULT,
};

static struct XPsmTcmPwrCtrl_t TcmA1PwrCtrl = {
	.TcmMemPwrCtrl = {
		.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_TCMA1_MASK,
		.ChipEnAddr = PSMX_LOCAL_REG_TCM_CE_CNTRL,
		.ChipEnMask = PSMX_LOCAL_REG_TCM_CE_CNTRL_TCMA1_MASK,
		.PwrCtrlAddr = PSMX_LOCAL_REG_TCM_PWR_CNTRL,
		.PwrCtrlMask = PSMX_LOCAL_REG_TCM_PWR_CNTRL_TCMA1_MASK,
		.PwrStatusAddr = PSMX_LOCAL_REG_TCM_PWR_STATUS,
		.PwrStatusMask = PSMX_LOCAL_REG_TCM_PWR_STATUS_TCMA1_MASK,
		.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_TCM1A_MASK,
		.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_TCM1A_RET_MASK,
		.PwrStateAckTimeout = XPSMFW_TCM1A_PWR_STATE_ACK_TIMEOUT,
		.PwrUpWaitTime = XPSMFW_TCM1A_PWR_UP_WAIT_TIME,

	},

	.Id = TCM_A_1,
	.PowerState = STATE_POWER_DEFAULT,
};

static struct XPsmTcmPwrCtrl_t TcmB0PwrCtrl = {
	.TcmMemPwrCtrl = {
		.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_TCMB0_MASK,
		.ChipEnAddr = PSMX_LOCAL_REG_TCM_CE_CNTRL,
		.ChipEnMask = PSMX_LOCAL_REG_TCM_CE_CNTRL_TCMB0_MASK,
		.PwrCtrlAddr = PSMX_LOCAL_REG_TCM_PWR_CNTRL,
		.PwrCtrlMask = PSMX_LOCAL_REG_TCM_PWR_CNTRL_TCMB0_MASK,
		.PwrStatusAddr = PSMX_LOCAL_REG_TCM_PWR_STATUS,
		.PwrStatusMask = PSMX_LOCAL_REG_TCM_PWR_STATUS_TCMB0_MASK,
		.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_TCM0B_MASK,
		.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_TCM0B_RET_MASK,
		.PwrStateAckTimeout = XPSMFW_TCM0B_PWR_STATE_ACK_TIMEOUT,
		.PwrUpWaitTime = XPSMFW_TCM0B_PWR_UP_WAIT_TIME,

	},

	.Id = TCM_B_0,
	.PowerState = STATE_POWER_DEFAULT,
};

static struct XPsmTcmPwrCtrl_t TcmB1PwrCtrl = {
	.TcmMemPwrCtrl = {
		.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE0_TCMB1_MASK,
		.ChipEnAddr = PSMX_LOCAL_REG_TCM_CE_CNTRL,
		.ChipEnMask = PSMX_LOCAL_REG_TCM_CE_CNTRL_TCMB1_MASK,
		.PwrCtrlAddr = PSMX_LOCAL_REG_TCM_PWR_CNTRL,
		.PwrCtrlMask = PSMX_LOCAL_REG_TCM_PWR_CNTRL_TCMB1_MASK,
		.PwrStatusAddr = PSMX_LOCAL_REG_TCM_PWR_STATUS,
		.PwrStatusMask = PSMX_LOCAL_REG_TCM_PWR_STATUS_TCMB1_MASK,
		.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_TCM1B_MASK,
		.RetMask = PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_TCM1B_RET_MASK,
		.PwrStateAckTimeout = XPSMFW_TCM1B_PWR_STATE_ACK_TIMEOUT,
		.PwrUpWaitTime = XPSMFW_TCM1B_PWR_UP_WAIT_TIME,

	},

	.Id = TCM_B_1,
	.PowerState = STATE_POWER_DEFAULT,
};

static struct XPsmFwGemPwrCtrl_t Gem0PwrCtrl = {
	.GemMemPwrCtrl = {
		.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE1_GEM0_MASK,
		.ChipEnAddr = PSMX_LOCAL_REG_GEM_CE_CNTRL,
		.ChipEnMask = PSMX_LOCAL_REG_GEM_CE_CNTRL_GEM0_MASK,
		.PwrCtrlAddr = PSMX_LOCAL_REG_GEM_PWR_CNTRL,
		.PwrCtrlMask = PSMX_LOCAL_REG_GEM_PWR_CNTRL_GEM0_MASK,
		.PwrStatusAddr = PSMX_LOCAL_REG_GEM_PWR_STATUS,
		.PwrStatusMask = PSMX_LOCAL_REG_GEM_PWR_STATUS_GEM0_MASK,
		.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_GEM0_MASK,
		.PwrStateAckTimeout = XPSMFW_GEM0_PWR_STATE_ACK_TIMEOUT,
		.PwrUpWaitTime = XPSMFW_GEM0_PWR_UP_WAIT_TIME,
	},
        .ClkCtrlAddr = CRL_GEM0_REF_CTRL,
        .ClkCtrlMask = CRL_GEM0_REF_CTRL_CLKACT_MASK,
        .RstCtrlAddr = CRL_RST_GEM0,
        .RstCtrlMask = CRL_RST_GEM0_RESET_MASK,
};

static struct XPsmFwGemPwrCtrl_t Gem1PwrCtrl = {
	.GemMemPwrCtrl = {
		.PwrStateMask = PSMX_LOCAL_REG_LOC_PWR_STATE1_GEM1_MASK,
		.ChipEnAddr = PSMX_LOCAL_REG_GEM_CE_CNTRL,
		.ChipEnMask = PSMX_LOCAL_REG_GEM_CE_CNTRL_GEM1_MASK,
		.PwrCtrlAddr = PSMX_LOCAL_REG_GEM_PWR_CNTRL,
		.PwrCtrlMask = PSMX_LOCAL_REG_GEM_PWR_CNTRL_GEM1_MASK,
		.PwrStatusAddr = PSMX_LOCAL_REG_GEM_PWR_STATUS,
		.PwrStatusMask = PSMX_LOCAL_REG_GEM_PWR_STATUS_GEM1_MASK,
		.GlobPwrStatusMask = PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_GEM1_MASK,
		.PwrStateAckTimeout = XPSMFW_GEM1_PWR_STATE_ACK_TIMEOUT,
		.PwrUpWaitTime = XPSMFW_GEM1_PWR_UP_WAIT_TIME,
	},
        .ClkCtrlAddr = CRL_GEM1_REF_CTRL,
        .ClkCtrlMask = CRL_GEM1_REF_CTRL_CLKACT_MASK,
        .RstCtrlAddr = CRL_RST_GEM1,
        .RstCtrlMask = CRL_RST_GEM1_RESET_MASK,
};

enum XPsmFWPwrUpDwnType {
	XPSMFW_PWR_UPDWN_DIRECT,
	XPSMFW_PWR_UPDWN_REQUEST,
};

static void XPsmFwACPUxPwrUp(struct XPsmFwPwrCtrl_t *Args)
{

	/*TBD: ignore below 2 steps if it is powering up from emulated
		pwrdwn or debug recovery pwrdwn*/
	/*Enables Power to the Core*/
	XPsmFw_Write32(Args->PwrCtrlAddr,PSMX_LOCAL_REG_APU0_CORE0_PWR_CNTRL_PWR_GATES_MASK);

	/*Removes Isolation to the APU*/
	XPsmFw_RMW32(Args->PwrCtrlAddr,PSMX_LOCAL_REG_APU0_CORE0_PWR_CNTRL_ISOLATION_MASK,
			~PSMX_LOCAL_REG_APU0_CORE0_PWR_CNTRL_ISOLATION_MASK);

	XPsmFw_RMW32(Args->CorePstate,Args->CorePstateMask,Args->CorePstateVal);
	XPsmFw_RMW32(Args->CorePreq,Args->CorePreqMask,Args->CorePreqMask);

}

static XStatus XPsmFwACPUxDirectPwrUp(struct XPsmFwPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;
	u32 LowAddress, HighAddress;

	if(ApuClusterState[Args->ClusterId] != A78_CLUSTER_CONFIGURED){
		/* APU PSTATE, PREQ configuration */
		XPsmFw_RMW32(Args->ClusterPstate,Args->ClusterPstateMask,Args->ClusterPstateValue);
		XPsmFw_RMW32(Args->ClusterPreq,Args->ClusterPreqMask,Args->ClusterPreqMask);

		/* ACPU clock config */
		XPsmFw_RMW32(Args->ClkCtrlAddr,Args->ClkCtrlMask,Args->ClkCtrlMask);

		/* APU cluster release cold & warm reset */
		XPsmFw_RMW32(Args->RstAddr,ACPU_CLUSTER_COLD_WARM_RST_MASK,0);

		Status = XPsmFw_UtilPollForMask(Args->ClusterPactive,Args->ClusterPacceptMask,ACPU_PACCEPT_TIMEOUT);
		if (Status != XST_SUCCESS) {
			XPsmFw_Printf(DEBUG_ERROR,"A78 Cluster PACCEPT timeout..\n");
			goto done;
		}
		ApuClusterState[Args->ClusterId] = A78_CLUSTER_CONFIGURED;
	}

	XPsmFwACPUxPwrUp(Args);

	/*set start address*/
	LowAddress = (u32)(PsmToPlmEvent.ResumeAddress[Args->Id] & 0xfffffffeULL);
	HighAddress = (u32)(PsmToPlmEvent.ResumeAddress[Args->Id] >> 32ULL);
	XPsmFw_Write32(Args->ResetCfgAddr,LowAddress);
	XPsmFw_Write32(Args->ResetCfgAddr+0x4,HighAddress);
	PsmToPlmEvent.ResumeAddress[Args->Id]=0;

	/* APU core release warm reset */
	XPsmFw_RMW32(Args->RstAddr,Args->WarmRstMask,~Args->WarmRstMask);
	Status = XPsmFw_UtilPollForMask(Args->CorePactive,Args->CorePacceptMask,ACPU_PACCEPT_TIMEOUT);
	if (Status != XST_SUCCESS) {
		XPsmFw_Printf(DEBUG_ERROR,"A78 Cluster PACCEPT timeout..\n");
		goto done;
	}

	/* Mark ACPUx powered up in LOCAL_PWR_STATUS register */
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE0, Args->PwrStateMask, Args->PwrStateMask);

	/* Disable and clear ACPUx direct wake-up interrupt request */
	XPsmFw_Write32(PSMX_GLOBAL_REG_WAKEUP0_IRQ_STATUS, Args->PwrStateMask);

	/*
	 * Unmask interrupt for all Power-up Requests and Reset Requests that
	 * are triggered but have their interrupt masked.
	 */
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP0_INT_EN, XPsmFw_Read32(PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS));
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_SWRST_INT_EN, XPsmFw_Read32(PSMX_GLOBAL_REG_REQ_SWRST_STATUS));

done:
	return Status;
}

static XStatus XPsmFwACPUxReqPwrUp(struct XPsmFwPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;
	u32 RegVal;

	/* Disable and clear ACPUx req pwr-up interrupt request */
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS, Args->PwrStateMask);

	/*Mask the Power Up Interrupt*/
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP0_INT_DIS, Args->PwrStateMask);

	/* Check if already power up */
	RegVal = XPsmFw_Read32(PSMX_GLOBAL_REG_PWR_STATE0);
	if (CHECK_BIT(RegVal, Args->PwrStateMask)) {
		Status = XST_SUCCESS;
		goto done;
	}

	XPsmFwACPUxPwrUp(Args);

	/* APU core release warm reset */
	XPsmFw_RMW32(Args->RstAddr,Args->WarmRstMask,~Args->WarmRstMask);
	Status = XPsmFw_UtilPollForMask(Args->CorePactive,Args->CorePacceptMask,ACPU_PACCEPT_TIMEOUT);
	if (Status != XST_SUCCESS) {
		XPsmFw_Printf(DEBUG_ERROR,"A78 Cluster PACCEPT timeout..\n");
		goto done;
	}

	/* Mark ACPUx powered up in LOCAL_PWR_STATUS register */
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE0, Args->PwrStateMask, Args->PwrStateMask);

done:
	return Status;
}

static void XPsmFwACPUxPwrDwn(struct XPsmFwPwrCtrl_t *Args)
{
	/*TBD: check for emulated power down/debug recovery pwrdwn*/
	XPsmFw_RMW32(Args->RstAddr,Args->WarmRstMask&PSX_CRF_RST_APU_WARM_RST_MASK,
			Args->WarmRstMask);

	/*TBD: for emulation and debug recovery pwrdwn modes
		no need to enable isolation and no need to disable power*/
	/* enable isolation */
	XPsmFw_RMW32(Args->PwrCtrlAddr,PSM_LOCAL_PWR_CTRL_ISO_MASK,PSM_LOCAL_PWR_CTRL_ISO_MASK);

	/* disable power to the core */
	XPsmFw_RMW32(Args->PwrCtrlAddr,PSM_LOCAL_PWR_CTRL_GATES_MASK,~PSM_LOCAL_PWR_CTRL_GATES_MASK);

	/* unmask the wakeup interrupt */
	XPsmFw_Write32(PSMX_GLOBAL_REG_WAKEUP0_IRQ_EN,Args->PwrStateMask);

}

static XStatus XPsmFwACPUxDirectPwrDwn(struct XPsmFwPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;

	/*Disable the Scan Clear and Mem Clear triggers*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_SCAN_CLEAR_TRIGGER, Args->PwrStateMask, ~Args->PwrStateMask);
	XPsmFw_RMW32(PSMX_GLOBAL_REG_MEM_CLEAR_TRIGGER, Args->PwrStateMask, ~Args->PwrStateMask);

	/* poll for power state change */
	Status = XPsmFw_UtilPollForMask(Args->CorePactive,Args->CorePacceptMask,ACPU_PACCEPT_TIMEOUT);
	if (Status != XST_SUCCESS) {
		XPsmFw_Printf(DEBUG_ERROR,"A78 Core PACCEPT timeout..\n");
		goto done;
	}

	XPsmFwACPUxPwrDwn(Args);

	/* Unmask the Power Up Interrupt */
	XPsmFw_Write32(PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_EN,Args->PwrStateMask);

	/* Clear the Interrupt */
	XPsmFw_Write32(PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_STATUS,Args->PwrStateMask);

	/*Mark ACPUx powered down in LOCAL_PWR_STATUS register */
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE0,Args->PwrStateMask,~Args->PwrStateMask);

	Status = XST_SUCCESS;

done:
	return Status;
}

static XStatus XPsmFwACPUxReqPwrDwn(struct XPsmFwPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;

	/*Disable the Scan Clear and Mem Clear triggers*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_SCAN_CLEAR_TRIGGER, Args->PwrStateMask, ~Args->PwrStateMask);
	XPsmFw_RMW32(PSMX_GLOBAL_REG_MEM_CLEAR_TRIGGER, Args->PwrStateMask, ~Args->PwrStateMask);

	u32 Edprcr = (CORESIGHT_APU0CORE0_DBG_EDPRCR + (Args->Id*0x20000) + (Args->ClusterId*0x100000));
	/*check for emulated pwr down*/
	if(CORESIGHT_APU0CORE0_DBG_EDPRCR_CORENPDRQ_MASK == (XPsmFw_Read32(Edprcr)&CORESIGHT_APU0CORE0_DBG_EDPRCR_CORENPDRQ_MASK)){
		/*Set the PSTATE field for emulated pwrdwn*/
		XPsmFw_RMW32(Args->CorePstate, Args->CorePstateMask, 0x1);
	}else{
		/*Set the PSTATE field*/
		XPsmFw_RMW32(Args->CorePstate, Args->CorePstateMask, 0);
	}

	/*set PREQ field*/
	XPsmFw_RMW32(Args->CorePreq,Args->CorePreqMask,Args->CorePreqMask);

	/* poll for power state change */
	Status = XPsmFw_UtilPollForMask(Args->CorePactive,Args->CorePacceptMask,ACPU_PACCEPT_TIMEOUT);
	if (Status != XST_SUCCESS) {
		XPsmFw_Printf(DEBUG_ERROR,"A78 Core PACCEPT timeout..\n");
		goto done;
	}

	XPsmFwACPUxPwrDwn(Args);

	/* Unmask the Power Up Interrupt */
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP0_INT_EN,Args->PwrStateMask);

	/*Mark ACPUx powered down in LOCAL_PWR_STATUS register */
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE0,Args->PwrStateMask,~Args->PwrStateMask);

	/* clear the Power dwn Interrupt */
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS,Args->PwrStateMask);

done:
	return Status;
}

static XStatus XPsmFwRPUxPwrUp(struct XPsmFwPwrCtrl_t *Args){

	XStatus Status = XST_FAILURE;

	/* Mask RPU PCIL Interrupts */
    XPsmFw_RMW32(Args->IntrDisableAddr,Args->IntrDisableMask,Args->IntrDisableMask);

	/*TBD: check if powering up from emulated pwr dwn state,if so skip below 4 instructions*/
	/* Restore Power to Core */
    XPsmFw_RMW32(Args->PwrCtrlAddr,PSMX_LOCAL_REG_RPU_A_CORE0_PWR_CNTRL_PWR_GATES_MASK,
			PSMX_LOCAL_REG_RPU_A_CORE0_PWR_CNTRL_PWR_GATES_MASK);

	/*Remove isolation */
    XPsmFw_RMW32(Args->PwrCtrlAddr,PSMX_LOCAL_REG_RPU_A_CORE0_PWR_CNTRL_ISOLATION_MASK,
		 ~PSMX_LOCAL_REG_RPU_A_CORE0_PWR_CNTRL_ISOLATION_MASK);

	/* Restore Power to the RPU core cache RAMs */
    XPsmFw_RMW32(PSMX_LOCAL_REG_RPU_CACHE_PWR_CNTRL,Args->PwrStateMask >> 16,Args->PwrStateMask >> 16);

	/* Enable the caches */
    XPsmFw_RMW32(PSMX_LOCAL_REG_RPU_CACHE_CE_CNTRL,Args->PwrStateMask >> 16,Args->PwrStateMask >> 16);

	/* set pstate field */
    XPsmFw_RMW32(Args->CorePstate,Args->CorePstateMask,~Args->CorePstateMask);

	/* set preq field to request power state change */
    XPsmFw_RMW32(Args->CorePreq,Args->CorePreqMask,Args->CorePreqMask);

	/* release reset */
    XPsmFw_RMW32(PSX_CRL_RST_RPU,Args->RstCtrlMask,~Args->RstCtrlMask);

	Status = XPsmFw_UtilPollForMask(Args->CorePactive,Args->CorePacceptMask,RPU_PACTIVE_TIMEOUT);
    if (XST_SUCCESS != Status) {
		XPsmFw_Printf(DEBUG_ERROR,"R52 Core PACCEPT timeout..\n");
        goto done;
	}

	/*Mark RPUx powered up in LOCAL_PWR_STATE register */
    XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE0,Args->PwrStateMask,Args->PwrStateMask);

done:
	return Status;
}

static XStatus XPsmFwRPUxDirectPwrUp(struct XPsmFwPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;
	u32 LowAddress;

	/*reset assert*/
	XPsmFw_RMW32(PSX_CRL_RST_RPU,Args->RstCtrlMask,Args->RstCtrlMask);

	/*Set the start address */
	LowAddress = (u32)(PsmToPlmEvent.ResumeAddress[Args->Id] & 0xffffffe0ULL);
	if(0U != PsmToPlmEvent.ResumeAddress[Args->Id] & 1ULL){
		XPsmFw_Write32(Args->VectTableAddr, LowAddress);
		if(0U == LowAddress){
			XPsmFw_RMW32(Args->ResetCfgAddr,RPU_TCMBOOT_MASK,RPU_TCMBOOT_MASK);
		}else{
			XPsmFw_RMW32(Args->ResetCfgAddr,RPU_TCMBOOT_MASK,~RPU_TCMBOOT_MASK);
		}
		PsmToPlmEvent.ResumeAddress[Args->Id] = 0U;
	}

	/* Mask wake interrupt */
	XPsmFw_RMW32(PSMX_GLOBAL_REG_WAKEUP1_IRQ_EN,Args->PwrStateMask >> 14, 0);

	Status = XPsmFwRPUxPwrUp(Args);
	if(XST_SUCCESS != Status){
		goto done;
	}

	/* Disable and clear RPUx direct wake-up interrupt request */
	XPsmFw_Write32(PSMX_GLOBAL_REG_WAKEUP1_IRQ_STATUS, Args->PwrStateMask >> 14);

	/*
	 * Unmask interrupt for all Power-up Requests and Reset Requests that
	 * are triggered but have their interrupt masked.
	 */
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP0_INT_EN, XPsmFw_Read32(PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS));
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_SWRST_INT_EN, XPsmFw_Read32(PSMX_GLOBAL_REG_REQ_SWRST_STATUS));

done:
	return Status;
}

static XStatus XPsmFwRPUxPwrDwn(struct XPsmFwPwrCtrl_t *Args)
{
	/*TBD:poll for pactive[1] bit to go low */

	/* set pstate bit */
	XPsmFw_RMW32(Args->CorePstate,Args->CorePstateMask,Args->CorePstateMask);

	/* set preq field to request power state change */
	XPsmFw_RMW32(Args->CorePreq,Args->CorePreqMask,Args->CorePreqMask);

	/*TBD: poll for paccept bit */

	/*TBD: check if it emulated pwr dwn, if so skip below 4 instructions*/
	/* Enable isolation */
	XPsmFw_RMW32(Args->PwrCtrlAddr,PSMX_LOCAL_REG_RPU_A_CORE0_PWR_CNTRL_ISOLATION_MASK,
		PSMX_LOCAL_REG_RPU_A_CORE0_PWR_CNTRL_ISOLATION_MASK);

	/* disable power to rpu core */
	XPsmFw_RMW32(Args->PwrCtrlAddr,PSMX_LOCAL_REG_RPU_A_CORE0_PWR_CNTRL_PWR_GATES_MASK,
		~PSMX_LOCAL_REG_RPU_A_CORE0_PWR_CNTRL_PWR_GATES_MASK);

	/* Disable the RPU core caches */
	XPsmFw_RMW32(PSMX_LOCAL_REG_RPU_CACHE_CE_CNTRL,Args->PwrStateMask >> 16,~(Args->PwrStateMask >> 16));

	/* Power gate the RPU core cache RAMs */
	XPsmFw_RMW32(PSMX_LOCAL_REG_RPU_CACHE_PWR_CNTRL,Args->PwrStateMask >> 16,~(Args->PwrStateMask >> 16));

	/* Unmask the RPU PCIL Interrupt */
	XPsmFw_Write32(Args->IntrDisableAddr,Args->IntrDisableMask);

	/*Mark RPUx powered down in LOCAL_PWR_STATE register */
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE0,Args->PwrStateMask,~Args->PwrStateMask);

	return XST_SUCCESS;

}

static XStatus XPsmFwRPUxDirectPwrDwn(struct XPsmFwPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;

	Status = XPsmFwRPUxPwrDwn(Args);
	if(XST_SUCCESS != Status){
		goto done;
	}

	/* Unmask the corresponding Wake Interrupt RPU core */
	XPsmFw_RMW32(PSMX_GLOBAL_REG_WAKEUP1_IRQ_EN,Args->PwrStateMask >> 14, ~(Args->PwrStateMask >> 14));

done:
	return Status;
}

static XStatus XPsmFwMemPwrDwn(struct XPsmFwMemPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;

	/*clear the interrupt*/
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS, Args->GlobPwrStatusMask);
	u32 Retention = XPsmFw_Read32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS) & Args->RetMask;

	/*Clear the retention bit*/
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS, Args->RetMask);
	if(0 != Retention){
		/*Set the retention bit*/
		XPsmFw_RMW32(PSMX_LOCAL_REG_OCM_RET_CNTRL,Args->PwrStatusMask,Args->PwrStatusMask);
		/*Check the retention mode is enabled or not*/
		if((XPsmFw_Read32(PSMX_LOCAL_REG_LOC_AUX_PWR_STATE)&Args->PwrStateMask) != Args->PwrStateMask){
			XPsmFw_Printf(DEBUG_ERROR,"Retention mode is not set\n");
			/*TBD: PSMX_LOCAL_REG_LOC_AUX_PWR_STATE bit is not setting to 1,uncomment once it is fixed*/
			//goto done;
		}
	}else{

		/*power down the OCM RAMs without Retention*/
		XPsmFw_RMW32(Args->RetCtrlAddr, Args->RetCtrlMask, ~Args->RetCtrlAddr);

		/*poll for disable retention*/
		Status = XPsmFw_UtilPollForZero(PSMX_LOCAL_REG_LOC_AUX_PWR_STATE, Args->PwrStateMask, Args->PwrStateAckTimeout);
		if (Status != XST_SUCCESS) {
			XPsmFw_Printf(DEBUG_ERROR,"Retenstion is not disabled\n");
			goto done;
		}
	}

	/*Disable power to ocm banks*/
	XPsmFw_RMW32(Args->PwrCtrlAddr, Args->PwrCtrlMask, ~Args->PwrCtrlMask);

	/*Disable chip enable signal*/
	XPsmFw_RMW32(Args->ChipEnAddr, Args->ChipEnMask, ~Args->ChipEnMask);

	/*reset bit in local reg*/
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE0, Args->PwrStateMask, ~Args->PwrStateMask);

	/*Read the OCM Power Status register*/
	Status = XPsmFw_UtilPollForZero(Args->PwrStatusAddr, Args->PwrStatusMask, Args->PwrStateAckTimeout);
	if (Status != XST_SUCCESS) {
		XPsmFw_Printf(DEBUG_ERROR,"bit is not set\n");
		goto done;
	}

	/*Unmask the OCM Power Up Interrupt*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRUP1_INT_EN, Args->GlobPwrStatusMask, Args->GlobPwrStatusMask);

	/*Clear the interrupt*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS, Args->GlobPwrStatusMask, Args->GlobPwrStatusMask);

done:
	return Status;
}

static XStatus XPsmFwMemPwrUp(struct XPsmFwMemPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;

	/*set chip enable*/
	XPsmFw_RMW32(Args->ChipEnAddr, Args->ChipEnMask, Args->ChipEnMask);

	/* enable power*/
	XPsmFw_RMW32(Args->PwrCtrlAddr, Args->PwrCtrlMask, Args->PwrCtrlMask);

	/*set bit in local reg*/
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE0, Args->PwrStateMask, Args->PwrStateMask);

	Status = XPsmFw_UtilPollForMask(Args->PwrStatusAddr, Args->PwrStatusMask, Args->PwrStateAckTimeout);
	if (Status != XST_SUCCESS) {
		XPsmFw_Printf(DEBUG_ERROR,"bit is not set\n");
		goto done;
	}

	/* Unmask the OCM Power Down Interrupt  and retention mask*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRDWN1_INT_EN, Args->GlobPwrStatusMask, Args->GlobPwrStatusMask);
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRDWN1_INT_EN, Args->RetMask, Args->RetMask);

	/*Clear the interrupt*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS, Args->GlobPwrStatusMask, Args->GlobPwrStatusMask);

done:
	return Status;
}

static XStatus XTcmPwrUp(struct XPsmTcmPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;

	struct XPsmFwMemPwrCtrl_t *Tcm = &Args->TcmMemPwrCtrl;

	/*Clear the interrupt*/
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS, Tcm->GlobPwrStatusMask);

	/*enable the chip enable signal*/
	XPsmFw_RMW32(Tcm->ChipEnAddr, Tcm->ChipEnMask, Tcm->ChipEnMask);
	/*Enable power for corresponding TCM bank*/
	XPsmFw_RMW32(Tcm->PwrCtrlAddr, Tcm->PwrCtrlMask, Tcm->PwrCtrlMask);

	/* Mark tcm bank powered up in LOCAL_PWR_STATE0 register */
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE0, Tcm->PwrStateMask, Tcm->PwrStateMask);
	Status = XPsmFw_UtilPollForMask(Tcm->PwrStatusAddr, Tcm->PwrStatusMask, Tcm->PwrStateAckTimeout);
	if (Status != XST_SUCCESS) {
		XPsmFw_Printf(DEBUG_ERROR,"TCM bit is not set\n");
		goto done;
	}

	/* Wait for power to ramp up */
	XPsmFw_UtilWait(Tcm->PwrUpWaitTime);

	/* Unmask the OCM Power Down Interrupt  and retention mask*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRDWN1_INT_EN, Tcm->GlobPwrStatusMask, Tcm->GlobPwrStatusMask);
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRDWN1_INT_EN, Tcm->RetMask, Tcm->RetMask);

done:
	return Status;
}

static XStatus XTcmPwrDown(struct XPsmTcmPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;
	struct XPsmFwMemPwrCtrl_t *Tcm = &Args->TcmMemPwrCtrl;

	/*Clear the interrupt*/
	XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS, Tcm->GlobPwrStatusMask);
	u32 Retention = XPsmFw_Read32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS) & Tcm->RetMask;
	if(0 != Retention){
		XPsmFw_RMW32(PSMX_LOCAL_REG_TCM_RET_CNTRL,Tcm->PwrStatusMask,Tcm->PwrStatusMask);
		XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS, Tcm->RetMask);
		/*Ensure for Retention Mode taken effect*/
		if((XPsmFw_Read32(PSMX_LOCAL_REG_LOC_AUX_PWR_STATE)&Tcm->PwrStateMask) != Tcm->PwrStateMask){
			XPsmFw_Printf(DEBUG_ERROR,"Retention mode is not set\n");
			/*TBD: PSMX_LOCAL_REG_LOC_AUX_PWR_STATE bit is not setting to 1,uncomment below line once it is fixed*/
			//goto done;
		}
	}

	/* disable power gate*/
	XPsmFw_RMW32(Tcm->PwrCtrlAddr, Tcm->PwrStateMask, ~Tcm->PwrStateMask);

	/*disable chip enable signal*/
	XPsmFw_RMW32(Tcm->ChipEnAddr, Tcm->ChipEnMask, ~Tcm->ChipEnMask);

	/* reset bit in local reg*/
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE0, Tcm->PwrStateMask, ~Tcm->PwrStateMask);
	Status = XPsmFw_UtilPollForZero(Tcm->PwrStatusAddr, Tcm->PwrStatusMask, Tcm->PwrStateAckTimeout);
	if (Status != XST_SUCCESS) {
		XPsmFw_Printf(DEBUG_ERROR,"TCM bit is not reset\n");
		goto done;
	}

	/* unmask tcm powerup interrupt*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRUP1_INT_DIS, Tcm->PwrStateMask, Tcm->PwrStateMask);

done:
	return Status;
}

static XStatus XPsmFwMemPwrUp_Gem(struct XPsmFwMemPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;

	/*set chip enable*/
	XPsmFw_RMW32(Args->ChipEnAddr, Args->ChipEnMask, Args->ChipEnMask);

	/* enable power*/
	XPsmFw_RMW32(Args->PwrCtrlAddr, Args->PwrCtrlMask, Args->PwrCtrlMask);

	/*set bit in local reg*/
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE1, Args->PwrStateMask, Args->PwrStateMask);

	Status = XPsmFw_UtilPollForMask(Args->PwrStatusAddr, Args->PwrStatusMask, Args->PwrStateAckTimeout);
	if (Status != XST_SUCCESS) {
		XPsmFw_Printf(DEBUG_ERROR,"bit is not set\n");
		goto done;
	}

	/* Unmask the Power Down Interrupt*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRDWN1_INT_EN, Args->GlobPwrStatusMask, Args->GlobPwrStatusMask);

	/*Clear the interrupt*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS, Args->GlobPwrStatusMask, Args->GlobPwrStatusMask);

done:
	return Status;
}

static XStatus XPsmFwMemPwrDwn_Gem(struct XPsmFwMemPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;

	/*Disable power to gem banks*/
	XPsmFw_RMW32(Args->PwrCtrlAddr, Args->PwrCtrlMask, ~Args->PwrCtrlMask);

	/*Disable chip enable signal*/
	XPsmFw_RMW32(Args->ChipEnAddr, Args->ChipEnMask, ~Args->ChipEnMask);

	/*reset bit in local reg*/
	XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE1, Args->PwrStateMask, ~Args->PwrStateMask);

	/*Read the gem Power Status register*/
	Status = XPsmFw_UtilPollForZero(Args->PwrStatusAddr, Args->PwrStatusMask, Args->PwrStateAckTimeout);
	if (Status != XST_SUCCESS) {
		XPsmFw_Printf(DEBUG_ERROR,"bit is not set\n");
		goto done;
	}

	/*Unmask the gem Power Up Interrupt*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRUP1_INT_EN, Args->GlobPwrStatusMask, Args->GlobPwrStatusMask);

	/*Clear the interrupt*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS, Args->GlobPwrStatusMask, Args->GlobPwrStatusMask);

done:
	return Status;
}

/**
 * PowerUp_TCMA0() - Power up TCM0A memory
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_TCMA0(void)
{
	return XTcmPwrUp(&TcmA0PwrCtrl);
}

/**
 * PowerUp_TCMB0() - Power up TCM0B memory
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_TCMB0(void)
{
	return XTcmPwrUp(&TcmB0PwrCtrl);
}

/**
 * PowerUp_TCMA1() - Power up TCM1A memory
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_TCMA1(void)
{
	return XTcmPwrUp(&TcmA1PwrCtrl);
}

/**
 * PowerUp_TCMB1() - Power up TCM1B memory
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_TCMB1(void)
{
	return XTcmPwrUp(&TcmB1PwrCtrl);
}

/**
 * PowerDwn_TCMA0() - Power down TCMA0 memory
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_TCMA0(void)
{
	return XTcmPwrDown(&TcmA0PwrCtrl);
}

/**
 * PowerDwn_TCMB0() - Power down TCMB0 memory
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_TCMB0(void)
{
	return XTcmPwrDown(&TcmB0PwrCtrl);
}

/**
 * PowerDwn_TCMA1() - Power down TCMA1 memory
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_TCMA1(void)
{
	return XTcmPwrDown(&TcmA1PwrCtrl);
}

/**
 * PowerDwn_TCMB1() - Power down TCMB1 memory
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_TCMB1(void)
{
	return XTcmPwrDown(&TcmB1PwrCtrl);
}

/**
 * PowerUp_OCM_B0_I0() - Power up OCM BANK0 Island0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_OCM_B0_I0(void)
{
	return XPsmFwMemPwrUp(&Ocm_B0_I0_PwrCtrl);
}

/**
 * PowerUp_OCM_B0_I1() - Power up OCM BANK0 Island1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_OCM_B0_I1(void)
{
	return XPsmFwMemPwrUp(&Ocm_B0_I1_PwrCtrl);
}

/**
 * PowerUp_OCM_B0_I2() - Power up OCM BANK0 Island3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_OCM_B0_I2(void)
{
	return XPsmFwMemPwrUp(&Ocm_B0_I2_PwrCtrl);
}

/**
 * PowerUp_OCM_B0_I3() - Power up OCM BANK0 Island3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_OCM_B0_I3(void)
{
	return XPsmFwMemPwrUp(&Ocm_B0_I3_PwrCtrl);
}

/**
 * PowerUp_OCM_B1_I0() - Power up OCM BANK1 Island0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_OCM_B1_I0(void)
{
	return XPsmFwMemPwrUp(&Ocm_B1_I0_PwrCtrl);
}


/**
 * PowerUp_OCM_B1_I1() - Power up OCM BANK1 Island1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_OCM_B1_I1(void)
{
	return XPsmFwMemPwrUp(&Ocm_B1_I1_PwrCtrl);
}

/**
 * PowerUp_OCM_B1_I2() - Power up OCM BANK1 Island2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_OCM_B1_I2(void)
{
	return XPsmFwMemPwrUp(&Ocm_B1_I2_PwrCtrl);
}

/**
 * PowerUp_OCM_B1_I3() - Power up OCM BANK1 Island3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_OCM_B1_I3(void)
{
	return XPsmFwMemPwrUp(&Ocm_B1_I3_PwrCtrl);
}

/**
 * PowerDwn_OCM_B0_I0() - Power down OCM BANK0 Island0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_OCM_B0_I0(void)
{
	return XPsmFwMemPwrDwn(&Ocm_B0_I0_PwrCtrl);
}

/**
 * PowerDwn_OCM_B0_I1() - Power down OCM BANK0 Island1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_OCM_B0_I1(void)
{
	return XPsmFwMemPwrDwn(&Ocm_B0_I1_PwrCtrl);
}

/**
 * PowerDwn_OCM_B0_I2() - Power down OCM BANK0 Island2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_OCM_B0_I2(void)
{
	return XPsmFwMemPwrDwn(&Ocm_B0_I2_PwrCtrl);
}

/**
 * PowerDwn_OCM_B0_I3() - Power down OCM BANK0 Island3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_OCM_B0_I3(void)
{
	return XPsmFwMemPwrDwn(&Ocm_B0_I3_PwrCtrl);
}

/**
 * PowerDwn_OCM_B1_I0() - Power down OCM BANK1 Island0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_OCM_B1_I0(void)
{
	return XPsmFwMemPwrDwn(&Ocm_B1_I0_PwrCtrl);
}

/**
 * PowerDwn_OCM_B1_I1() - Power down OCM BANK1 Island1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_OCM_B1_I1(void)
{
	return XPsmFwMemPwrDwn(&Ocm_B1_I1_PwrCtrl);
}

/**
 * PowerDwn_OCM_B1_I2() - Power down OCM BANK1 Island2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_OCM_B1_I2(void)
{
	return XPsmFwMemPwrDwn(&Ocm_B1_I2_PwrCtrl);
}

/**
 * PowerDwn_OCM_B1_I3() - Power down OCM BANK1 Island3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_OCM_B1_I3(void)
{
	return XPsmFwMemPwrDwn(&Ocm_B1_I3_PwrCtrl);
}

/**
 * PowerUp_GEM0() - Power up GEM0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_GEM0(void)
{
	return XPsmFwMemPwrUp_Gem(&Gem0PwrCtrl.GemMemPwrCtrl);
}

/**
 * PowerUp_GEM1() - Power up GEM1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_GEM1(void)
{
	return XPsmFwMemPwrUp_Gem(&Gem1PwrCtrl.GemMemPwrCtrl);
}

/**
 * PowerDwn_GEM0() - Power down GEM0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_GEM0(void)
{
	return XPsmFwMemPwrDwn_Gem(&Gem0PwrCtrl.GemMemPwrCtrl);
}

/**
 * PowerDwn_GEM1() - Power down GEM1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_GEM1(void)
{
	return XPsmFwMemPwrDwn_Gem(&Gem1PwrCtrl.GemMemPwrCtrl);
}

/**
 * PowerUp_FP() - Power up FPD
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_FP(void)
{
	XStatus Status = XST_FAILURE;

	/* Instead of trigeering this interrupt
	FPD CDO should be reexecuted by XilPM */

	Status = XST_SUCCESS;

	return Status;
}

/**
 * PowerDwn_FP() - Power down FPD
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_FP(void)
{
	XStatus Status = XST_FAILURE;
	u32 RegVal;

	/* Check if already power down */
	RegVal = XPsmFw_Read32(PSMX_LOCAL_REG_LOC_PWR_STATE1);
	if (CHECK_BIT(RegVal, PSMX_LOCAL_REG_LOC_PWR_STATE1_FP_MASK)) {
		/* Enable isolation between FPD and LPD, PLD */
		XPsmFw_RMW32(PSMX_LOCAL_REG_DOMAIN_ISO_CNTRL,
			     PSMX_LOCAL_REG_DOMAIN_ISO_CNTRL_LPD_FPD_DFX_MASK | PSMX_LOCAL_REG_DOMAIN_ISO_CNTRL_LPD_FPD_MASK,
			     PSMX_LOCAL_REG_DOMAIN_ISO_CNTRL_LPD_FPD_DFX_MASK | PSMX_LOCAL_REG_DOMAIN_ISO_CNTRL_LPD_FPD_MASK);

		/* Disable alarms associated with FPD */
		XPsmFw_Write32(PSMX_GLOBAL_REG_PWR_CTRL0_IRQ_DIS,
			       PSMX_GLOBAL_REG_PWR_CTRL0_IRQ_DIS_FPD_SUPPLY_MASK);

		/* Clear power down request status */
		/* This is already handled by common handler so no need to handle here */

		/* Mark the FP as Powered Down */
		XPsmFw_RMW32(PSMX_LOCAL_REG_LOC_PWR_STATE1, PSMX_LOCAL_REG_LOC_PWR_STATE1_FP_MASK,
			     ~PSMX_LOCAL_REG_LOC_PWR_STATE1_FP_MASK);
	}

	Status = XST_SUCCESS;

	return Status;
}

/**
 * PowerUp_ACPU0_0() - Power up ACPU0 Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_ACPU0_0(void)
{
	return XPsmFwACPUxReqPwrUp(&Acpu0_Core0PwrCtrl);
}

/**
 * PowerUp_ACPU0_1() - Power up ACPU0 Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_ACPU0_1(void)
{
	return XPsmFwACPUxReqPwrUp(&Acpu0_Core1PwrCtrl);
}

/**
 * PowerUp_ACPU0_2() - Power up ACPU0 Core2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_ACPU0_2(void)
{
	return XPsmFwACPUxReqPwrUp(&Acpu0_Core2PwrCtrl);
}

/**
 * PowerUp_ACPU0_3() - Power up ACPU0 Core3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_ACPU0_3(void)
{
	return XPsmFwACPUxReqPwrUp(&Acpu0_Core3PwrCtrl);
}

/**
 * PowerUp_ACPU1_0() - Power up ACPU1 Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_ACPU1_0(void)
{
	return XPsmFwACPUxReqPwrUp(&Acpu1_Core0PwrCtrl);
}

/**
 * PowerUp_ACPU1_0() - Power up ACPU1 Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_ACPU1_1(void)
{
	return XPsmFwACPUxReqPwrUp(&Acpu1_Core1PwrCtrl);
}

/**
 * PowerUp_ACPU1_0() - Power up ACPU1 Core2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_ACPU1_2(void)
{
	return XPsmFwACPUxReqPwrUp(&Acpu1_Core2PwrCtrl);
}

/**
 * PowerUp_ACPU1_0() - Power up ACPU1 Core3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_ACPU1_3(void)
{
	return XPsmFwACPUxReqPwrUp(&Acpu1_Core3PwrCtrl);
}

/**
 * PowerDwn_ACPU0_0() - Power down ACPU0 Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_ACPU0_0(void)
{
	return XPsmFwACPUxReqPwrDwn(&Acpu0_Core0PwrCtrl);
}

/**
 * PowerDwn_ACPU0_1() - Power down ACPU0 Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_ACPU0_1(void)
{
	return XPsmFwACPUxReqPwrDwn(&Acpu0_Core1PwrCtrl);
}

/**
 * PowerDwn_ACPU0_2() - Power down ACPU0 Core2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_ACPU0_2(void)
{
	return XPsmFwACPUxReqPwrDwn(&Acpu0_Core2PwrCtrl);
}

/**
 * PowerDwn_ACPU0_3() - Power down ACPU0 Core3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_ACPU0_3(void)
{
	return XPsmFwACPUxReqPwrDwn(&Acpu0_Core3PwrCtrl);
}

/**
 * PowerDwn_ACPU1_0() - Power down ACPU1 Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_ACPU1_0(void)
{
	return XPsmFwACPUxReqPwrDwn(&Acpu1_Core0PwrCtrl);
}

/**
 * PowerDwn_ACPU1_1() - Power down ACPU1 Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_ACPU1_1(void)
{
	return XPsmFwACPUxReqPwrDwn(&Acpu1_Core1PwrCtrl);
}

/**
 * PowerDwn_ACPU1_2() - Power down ACPU1 Core2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_ACPU1_2(void)
{
	return XPsmFwACPUxReqPwrDwn(&Acpu1_Core2PwrCtrl);
}

/**
 * PowerDwn_ACPU1_3() - Power down ACPU1 Core
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_ACPU1_3(void)
{
	return XPsmFwACPUxReqPwrDwn(&Acpu1_Core3PwrCtrl);
}

static XStatus XPsmFwRPUxReqPwrUp(struct XPsmFwPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;
	u32 RegVal;

	/* Check if already power up */
	RegVal = XPsmFw_Read32(PSMX_GLOBAL_REG_PWR_STATE0);
	if (CHECK_BIT(RegVal, Args->PwrStateMask)) {
		Status = XST_SUCCESS;
		goto done;
	}

	/*mask powerup interrupt*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRUP1_INT_EN , Args->PwrStateMask >> 18,0);

	Status = XPsmFwRPUxPwrUp(Args);
	if(XST_SUCCESS != Status){
		goto done;
	}

done:
	return Status;
}

/**
 * PowerUp_RPUA_0() - Power up RPUA Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_RPUA_0(void)
{
	return XPsmFwRPUxReqPwrUp(&Rpu0_Core0PwrCtrl);
}

/**
 * PowerUp_RPUA_1() - Power up RPUA Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_RPUA_1(void)
{
	return XPsmFwRPUxReqPwrUp(&Rpu0_Core1PwrCtrl);
}

/**
 * PowerUp_RPUB_0() - Power up RPUB Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_RPUB_0(void)
{
	return XPsmFwRPUxReqPwrUp(&Rpu1_Core0PwrCtrl);
}

/**
 * PowerUp_RPUB_1() - Power up RPUB Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerUp_RPUB_1(void)
{
	return XPsmFwRPUxReqPwrUp(&Rpu1_Core1PwrCtrl);
}

static XStatus XPsmFwRPUxReqPwrDwn(struct XPsmFwPwrCtrl_t *Args)
{
	XStatus Status = XST_FAILURE;
	u32 RegVal;

	/* Check if already power down */
	RegVal = XPsmFw_Read32(PSMX_GLOBAL_REG_PWR_STATE0);
	if (!CHECK_BIT(RegVal, Args->PwrStateMask)) {
		Status = XST_SUCCESS;
		goto done;
	}
	/*mask powerup interrupt*/
	XPsmFw_RMW32(PSMX_GLOBAL_REG_REQ_PWRDWN1_INT_EN , Args->PwrStateMask >> 18,0);
	Status = XPsmFwRPUxPwrDwn(Args);

done:
	return Status;
}

/**
 * PowerDwn_RPUA_0() - Power up RPUA Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_RPUA_0(void)
{
	return XPsmFwRPUxReqPwrDwn(&Rpu0_Core0PwrCtrl);
}

/**
 * PowerDwn_RPUA_1() - Power up RPUA Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_RPUA_1(void)
{
	return XPsmFwRPUxReqPwrDwn(&Rpu0_Core1PwrCtrl);
}


/**
 * PowerDwn_RPUB_0() - Power up RPUB Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_RPUB_0(void)
{
	return XPsmFwRPUxReqPwrDwn(&Rpu1_Core0PwrCtrl);
}

/**
 * PowerDwn_RPUB_1() - Power up RPUB Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus PowerDwn_RPUB_1(void)
{
	return XPsmFwRPUxReqPwrDwn(&Rpu1_Core1PwrCtrl);
}

/* Structure for power up/down handler table */
static struct PwrHandlerTable_t PwrUpDwn0HandlerTable[] = {
	{PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS_FP_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS_FP_MASK, PowerUp_FP, PowerDwn_FP},
	{PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS_APU0_CORE0_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS_APU0_CORE0_MASK, PowerUp_ACPU0_0, PowerDwn_ACPU0_0},
	{PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS_APU0_CORE1_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS_APU0_CORE1_MASK, PowerUp_ACPU0_1, PowerDwn_ACPU0_1},
	{PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS_APU0_CORE2_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS_APU0_CORE2_MASK, PowerUp_ACPU0_2, PowerDwn_ACPU0_2},
	{PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS_APU0_CORE3_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS_APU0_CORE3_MASK, PowerUp_ACPU0_3, PowerDwn_ACPU0_3},
	{PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS_APU1_CORE0_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS_APU1_CORE0_MASK, PowerUp_ACPU1_0, PowerDwn_ACPU1_0},
	{PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS_APU1_CORE1_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS_APU1_CORE1_MASK, PowerUp_ACPU1_1, PowerDwn_ACPU1_1},
	{PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS_APU1_CORE2_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS_APU1_CORE2_MASK, PowerUp_ACPU1_2, PowerDwn_ACPU1_2},
	{PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS_APU1_CORE3_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS_APU1_CORE3_MASK, PowerUp_ACPU1_3, PowerDwn_ACPU1_3},
};

static struct PwrHandlerTable_t PwrUpDwn1HandlerTable[] = {
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_GEM0_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_GEM0_MASK, PowerUp_GEM0, PowerDwn_GEM0},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_GEM1_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_GEM1_MASK, PowerUp_GEM1, PowerDwn_GEM1},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND0_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND0_MASK, PowerUp_OCM_B0_I0, PowerDwn_OCM_B0_I0},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND1_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND1_MASK, PowerUp_OCM_B0_I1, PowerDwn_OCM_B0_I1},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND2_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND2_MASK, PowerUp_OCM_B0_I2, PowerDwn_OCM_B0_I2},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND3_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND3_MASK, PowerUp_OCM_B0_I3, PowerDwn_OCM_B0_I3},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND4_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND4_MASK, PowerUp_OCM_B1_I0, PowerDwn_OCM_B1_I0},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND5_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND5_MASK, PowerUp_OCM_B1_I1, PowerDwn_OCM_B1_I1},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND6_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND6_MASK, PowerUp_OCM_B1_I2, PowerDwn_OCM_B1_I2},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_OCM_ISLAND7_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_OCM_ISLAND7_MASK, PowerUp_OCM_B1_I3, PowerDwn_OCM_B1_I3},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_TCM0A_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_TCM0A_MASK, PowerUp_TCMA0, PowerDwn_TCMA0},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_TCM0B_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_TCM0B_MASK, PowerUp_TCMB0, PowerDwn_TCMB0},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_TCM1A_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_TCM1A_MASK, PowerUp_TCMA1, PowerDwn_TCMA1},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_TCM1B_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_TCM1B_MASK, PowerUp_TCMB1, PowerDwn_TCMB1},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_RPU_A_CORE0_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_RPU_A_CORE0_MASK, PowerUp_RPUA_0, PowerDwn_RPUA_0},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_RPU_A_CORE1_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_RPU_A_CORE1_MASK, PowerUp_RPUA_1, PowerDwn_RPUA_1},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_RPU_B_CORE0_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_RPU_B_CORE0_MASK, PowerUp_RPUB_0, PowerDwn_RPUB_0},
	{PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS_RPU_B_CORE1_MASK, PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS_RPU_B_CORE1_MASK, PowerUp_RPUB_1, PowerDwn_RPUB_1},
};

/**
 * XPsmFw_DispatchPwrUp0Handler() - Power-up interrupt handler
 *
 * @PwrUpStatus    Power Up status register value
 * @PwrUpIntMask   Power Up interrupt mask register value
 *
 * @return         XST_SUCCESS or error code
 */
XStatus XPsmFw_DispatchPwrUp0Handler(u32 PwrUpStatus, u32 PwrUpIntMask)
{
	XStatus Status = XST_FAILURE;
	u32 Index;

	for (Index = 0U; Index < ARRAYSIZE(PwrUpDwn0HandlerTable); Index++) {
		if ((CHECK_BIT(PwrUpStatus, PwrUpDwn0HandlerTable[Index].PwrUpMask)) &&
		    !(CHECK_BIT(PwrUpIntMask, PwrUpDwn0HandlerTable[Index].PwrUpMask))) {
			/* Call power up handler */
			Status = PwrUpDwn0HandlerTable[Index].PwrUpHandler();

			/* Ack the service */
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS, PwrUpDwn0HandlerTable[Index].PwrUpMask);
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP0_INT_DIS, PwrUpDwn0HandlerTable[Index].PwrUpMask);
		} else if (CHECK_BIT(PwrUpStatus, PwrUpDwn0HandlerTable[Index].PwrUpMask)){
			/* Ack the service if status is 1 but interrupt is not enabled */
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP0_STATUS, PwrUpDwn0HandlerTable[Index].PwrUpMask);
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP0_INT_DIS, PwrUpDwn0HandlerTable[Index].PwrUpMask);
			Status = XST_SUCCESS;
		} else {
			Status = XST_SUCCESS;
		}
	}

	return Status;
}

/**
 * XPsmFw_DispatchPwrUp1Handler() - Power-up interrupt handler
 *
 * @PwrUpStatus    Power Up status register value
 * @PwrUpIntMask   Power Up interrupt mask register value
 *
 * @return         XST_SUCCESS or error code
 */
XStatus XPsmFw_DispatchPwrUp1Handler(u32 PwrUpStatus, u32 PwrUpIntMask)
{
	XStatus Status = XST_FAILURE;
	u32 Index;

	for (Index = 0U; Index < ARRAYSIZE(PwrUpDwn1HandlerTable); Index++) {
		if ((CHECK_BIT(PwrUpStatus, PwrUpDwn1HandlerTable[Index].PwrUpMask)) &&
		    !(CHECK_BIT(PwrUpIntMask, PwrUpDwn1HandlerTable[Index].PwrUpMask))) {
			/* Call power up handler */
			Status = PwrUpDwn1HandlerTable[Index].PwrUpHandler();

			/* Ack the service */
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS, PwrUpDwn1HandlerTable[Index].PwrUpMask);
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP1_INT_DIS, PwrUpDwn1HandlerTable[Index].PwrUpMask);
		} else if (CHECK_BIT(PwrUpStatus, PwrUpDwn1HandlerTable[Index].PwrUpMask)){
			/* Ack the service if status is 1 but interrupt is not enabled */
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP1_STATUS, PwrUpDwn1HandlerTable[Index].PwrUpMask);
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRUP1_INT_DIS, PwrUpDwn1HandlerTable[Index].PwrUpMask);
			Status = XST_SUCCESS;
		} else {
			Status = XST_SUCCESS;
		}
	}

	return Status;
}

/**
 * XPsmFw_DispatchPwrDwn0Handler() - Power-down interrupt handler
 *
 * @PwrDwnStatus   Power Down status register value
 * @pwrDwnIntMask  Power Down interrupt mask register value
 * @PwrUpStatus    Power Up status register value
 * @PwrUpIntMask   Power Up interrupt mask register value
 *
 * @return         XST_SUCCESS or error code
 */
XStatus XPsmFw_DispatchPwrDwn0Handler(u32 PwrDwnStatus, u32 pwrDwnIntMask,
		u32 PwrUpStatus, u32 PwrUpIntMask)
{
	XStatus Status = XST_FAILURE;
	u32 Index;

	for (Index = 0U; Index < ARRAYSIZE(PwrUpDwn0HandlerTable); Index++) {
		if ((CHECK_BIT(PwrDwnStatus, PwrUpDwn0HandlerTable[Index].PwrDwnMask)) &&
		    !(CHECK_BIT(pwrDwnIntMask, PwrUpDwn0HandlerTable[Index].PwrDwnMask)) &&
		    !(CHECK_BIT(PwrUpStatus, PwrUpDwn0HandlerTable[Index].PwrUpMask)) &&
		    (CHECK_BIT(PwrUpIntMask, PwrUpDwn0HandlerTable[Index].PwrUpMask))) {
			/* Call power down handler */
			Status = PwrUpDwn0HandlerTable[Index].PwrDwnHandler();

			/* Ack the service */
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS, PwrUpDwn0HandlerTable[Index].PwrDwnMask);
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN0_INT_DIS, PwrUpDwn0HandlerTable[Index].PwrDwnMask);
		} else if (CHECK_BIT(PwrDwnStatus, PwrUpDwn0HandlerTable[Index].PwrDwnMask)) {
			/* Ack the service  if power up and power down interrupt arrives simultaneously */
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN0_STATUS, PwrUpDwn0HandlerTable[Index].PwrDwnMask);
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN0_INT_DIS, PwrUpDwn0HandlerTable[Index].PwrDwnMask);
			Status = XST_SUCCESS;
		} else {
			Status = XST_SUCCESS;
		}
	}

	return Status;
}
/**
 * XPsmFw_DispatchPwrDwn1Handler() - Power-down interrupt handler
 *
 * @PwrDwnStatus   Power Down status register value
 * @pwrDwnIntMask  Power Down interrupt mask register value
 * @PwrUpStatus    Power Up status register value
 * @PwrUpIntMask   Power Up interrupt mask register value
 *
 * @return         XST_SUCCESS or error code
 */
XStatus XPsmFw_DispatchPwrDwn1Handler(u32 PwrDwnStatus, u32 pwrDwnIntMask,
		u32 PwrUpStatus, u32 PwrUpIntMask)
{
	XStatus Status = XST_FAILURE;
	u32 Index;

	for (Index = 0U; Index < ARRAYSIZE(PwrUpDwn1HandlerTable); Index++) {
		if ((CHECK_BIT(PwrDwnStatus, PwrUpDwn1HandlerTable[Index].PwrDwnMask)) &&
		    !(CHECK_BIT(pwrDwnIntMask, PwrUpDwn1HandlerTable[Index].PwrDwnMask)) &&
		    !(CHECK_BIT(PwrUpStatus, PwrUpDwn1HandlerTable[Index].PwrUpMask)) &&
		    (CHECK_BIT(PwrUpIntMask, PwrUpDwn1HandlerTable[Index].PwrUpMask))) {
			/* Call power down handler */

			Status = PwrUpDwn1HandlerTable[Index].PwrDwnHandler();

			/* Ack the service */
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS, PwrUpDwn1HandlerTable[Index].PwrDwnMask);
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN1_INT_DIS, PwrUpDwn1HandlerTable[Index].PwrDwnMask);
		} else if (CHECK_BIT(PwrDwnStatus, PwrUpDwn1HandlerTable[Index].PwrDwnMask)) {
			/* Ack the service  if power up and power down interrupt arrives simultaneously */
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS, PwrUpDwn1HandlerTable[Index].PwrDwnMask);
			XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN1_INT_DIS, PwrUpDwn1HandlerTable[Index].PwrDwnMask);
			Status = XST_SUCCESS;
		} else {
			Status = XST_SUCCESS;
		}
	}
	if(Index==ARRAYSIZE(PwrUpDwn1HandlerTable)){
		/* disable the interrupt if the interrupt is not found in the PwrUpDwn1HandlerTable*/
		XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN1_STATUS, PwrDwnStatus);
		XPsmFw_Write32(PSMX_GLOBAL_REG_REQ_PWRDWN1_INT_DIS, PwrDwnStatus);
	}

	return Status;
}

/**
 * ACPU0_Core0Wakeup() - Wake up ACPU0_CORE0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU0_Core0Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_0] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_0]) {
		Status = XPsmFwACPUxDirectPwrUp(&Acpu0_Core0PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_0] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU0_Core1Wakeup() - Wake up ACPU0_CORE1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU0_Core1Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_1] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_1]) {
		Status = XPsmFwACPUxDirectPwrUp(&Acpu0_Core1PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_1] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU0_Core2Wakeup() - Wake up ACPU0_CORE2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU0_Core2Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_2] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_2]) {
		Status = XPsmFwACPUxDirectPwrUp(&Acpu0_Core2PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_2] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU0_Core3Wakeup() - Wake up ACPU0_CORE3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU0_Core3Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_3] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_3]) {
		Status = XPsmFwACPUxDirectPwrUp(&Acpu0_Core3PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_3] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU1_Core0Wakeup() - Wake up ACPU1_CORE0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU1_Core0Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_4] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_4]) {
		Status = XPsmFwACPUxDirectPwrUp(&Acpu1_Core0PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_4] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU1_Core1Wakeup() - Wake up ACPU1_CORE1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU1_Core1Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_5] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_5]) {
		Status = XPsmFwACPUxDirectPwrUp(&Acpu1_Core1PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_5] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU1_Core2Wakeup() - Wake up ACPU1_CORE2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU1_Core2Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_6] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_6]) {
		Status = XPsmFwACPUxDirectPwrUp(&Acpu1_Core2PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_6] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU1_Core3Wakeup() - Wake up ACPU1_CORE3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU1_Core3Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_7] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_7]) {
		Status = XPsmFwACPUxDirectPwrUp(&Acpu1_Core3PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_7] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU0_Core0Sleep() - Direct power down ACPU0 Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU0_Core0Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_0] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_0]) {
		Status = XPsmFwACPUxDirectPwrDwn(&Acpu0_Core0PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_0] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU0_Core1Sleep() - Direct power down ACPU0 Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU0_Core1Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_1] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_1]) {
		Status = XPsmFwACPUxDirectPwrDwn(&Acpu0_Core1PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_1] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU0_Core2Sleep() - Direct power down ACPU0 Core2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU0_Core2Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_2] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_2]) {
		Status = XPsmFwACPUxDirectPwrDwn(&Acpu0_Core2PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_2] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU0_Core3Sleep() - Direct power down ACPU0 Core3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU0_Core3Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_3] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_3]) {
		Status = XPsmFwACPUxDirectPwrDwn(&Acpu0_Core3PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_3] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU1_Core0Sleep() - Direct power down ACPU1 Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU1_Core0Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_4] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_4]) {
		Status = XPsmFwACPUxDirectPwrDwn(&Acpu1_Core0PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_4] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU1_Core1Sleep() - Direct power down ACPU1 Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU1_Core1Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_5] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_5]) {
		Status = XPsmFwACPUxDirectPwrDwn(&Acpu1_Core1PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_5] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU1_Core2Sleep() - Direct power down ACPU1 Core2
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU1_Core2Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_6] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_6]) {
		Status = XPsmFwACPUxDirectPwrDwn(&Acpu1_Core2PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_6] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * ACPU1_Core3Sleep() - Direct power down ACPU1 Core3
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus ACPU1_Core3Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[ACPU_7] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[ACPU_7]) {
		Status = XPsmFwACPUxDirectPwrDwn(&Acpu1_Core3PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[ACPU_7] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * RPU0_Core0Wakeup() - Wake up Rpu0 Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus RPU0_Core0Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[RPU0_0] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[RPU0_0]) {
		Status = XPsmFwRPUxDirectPwrUp(&Rpu0_Core0PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[RPU0_0] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * RPU0_Core1Wakeup() - Wake up Rpu0 Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus RPU0_Core1Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[RPU0_1] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[RPU0_1]) {
		Status = XPsmFwRPUxDirectPwrUp(&Rpu0_Core1PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[RPU0_1] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * RPU1_Core0Wakeup() - Wake up Rpu1 Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus RPU1_Core0Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[RPU1_0] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[RPU1_0]) {
		Status = XPsmFwRPUxDirectPwrUp(&Rpu1_Core0PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[RPU1_0] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * RPU1_Core1Wakeup() - Wake up Rpu1 Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus RPU1_Core1Wakeup(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[RPU1_1] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[RPU1_1]) {
		Status = XPsmFwRPUxDirectPwrUp(&Rpu1_Core1PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[RPU1_1] = PWR_UP_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * RPU0_Core0Sleep() - Direct power down Rpu0 Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus RPU0_Core0Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[RPU0_0] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[RPU0_0]) {
		Status = XPsmFwRPUxDirectPwrDwn(&Rpu0_Core0PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[RPU0_0] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * RPU0_Core1Sleep() - Direct power down Rpu0 Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus RPU0_Core1Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[RPU0_1] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[RPU0_1]) {
		Status = XPsmFwRPUxDirectPwrDwn(&Rpu0_Core1PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[RPU0_1] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * RPU1_Core0Sleep() - Direct power down Rpu1 Core0
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus RPU1_Core0Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[RPU1_0] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[RPU1_0]) {
		Status = XPsmFwRPUxDirectPwrDwn(&Rpu1_Core0PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[RPU1_0] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/**
 * RPU1_Core1Sleep() - Direct power down Rpu1 Core1
 *
 * @return    XST_SUCCESS or error code
 */
static XStatus RPU1_Core1Sleep(void)
{
	XStatus Status = XST_FAILURE;

	/* Check for any pending event */
	assert(PsmToPlmEvent.Event[RPU1_1] == 0U);

	if (1U == PsmToPlmEvent.CpuIdleFlag[RPU1_1]) {
		Status = XPsmFwRPUxDirectPwrDwn(&Rpu1_Core1PwrCtrl);
		if (XST_SUCCESS != Status) {
			goto done;
		}
	}

	/* Set the event bit for PLM */
	PsmToPlmEvent.Event[RPU1_1] = PWR_DWN_EVT;
	Status = XPsmFw_NotifyPlmEvent();

done:
	return Status;
}

/****************************************************************************/
/**
 * @brief	Direct power down processor
 *
 * @param DeviceId	Device ID of processor
 *
 * @return	XST_SUCCESS or error code
 *
 * @note	None
 *
 ****************************************************************************/
XStatus XPsmFw_DirectPwrDwn(const u32 DeviceId)
{
	XStatus Status = XST_FAILURE;

	switch (DeviceId) {
		case XPSMFW_DEV_CLUSTER0_ACPU_0:
			Status = XPsmFwACPUxDirectPwrDwn(&Acpu0_Core0PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER0_ACPU_1:
			Status = XPsmFwACPUxDirectPwrDwn(&Acpu0_Core1PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER0_ACPU_2:
			Status = XPsmFwACPUxDirectPwrDwn(&Acpu0_Core2PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER0_ACPU_3:
			Status = XPsmFwACPUxDirectPwrDwn(&Acpu0_Core3PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_ACPU_0:
			Status = XPsmFwACPUxDirectPwrDwn(&Acpu1_Core0PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_ACPU_1:
			Status = XPsmFwACPUxDirectPwrDwn(&Acpu1_Core1PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_ACPU_2:
			Status = XPsmFwACPUxDirectPwrDwn(&Acpu1_Core2PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_ACPU_3:
			Status = XPsmFwACPUxDirectPwrDwn(&Acpu1_Core3PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER0_RPU0_0:
			Status = XPsmFwRPUxDirectPwrDwn(&Rpu0_Core0PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER0_RPU0_1:
			Status = XPsmFwRPUxDirectPwrDwn(&Rpu0_Core1PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_RPU0_0:
			Status = XPsmFwRPUxDirectPwrDwn(&Rpu1_Core0PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_RPU0_1:
			Status = XPsmFwRPUxDirectPwrDwn(&Rpu1_Core1PwrCtrl);
			break;
		default:
			Status = XST_INVALID_PARAM;
			break;
	}

	return Status;
}

/****************************************************************************/
/**
 * @brief	Direct power up processor
 *
 * @param DeviceId	Device ID of processor
 *
 * @return	XST_SUCCESS or error code
 *
 * @note	None
 *
 ****************************************************************************/
XStatus XPsmFw_DirectPwrUp(const u32 DeviceId)
{
	XStatus Status = XST_FAILURE;

	switch (DeviceId) {
		case XPSMFW_DEV_CLUSTER0_ACPU_0:
			Status = XPsmFwACPUxDirectPwrUp(&Acpu0_Core0PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER0_ACPU_1:
			Status = XPsmFwACPUxDirectPwrUp(&Acpu0_Core1PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER0_ACPU_2:
			Status = XPsmFwACPUxDirectPwrUp(&Acpu0_Core2PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER0_ACPU_3:
			Status = XPsmFwACPUxDirectPwrUp(&Acpu0_Core3PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_ACPU_0:
			Status = XPsmFwACPUxDirectPwrUp(&Acpu1_Core0PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_ACPU_1:
			Status = XPsmFwACPUxDirectPwrUp(&Acpu1_Core1PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_ACPU_2:
			Status = XPsmFwACPUxDirectPwrUp(&Acpu1_Core2PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_ACPU_3:
			Status = XPsmFwACPUxDirectPwrUp(&Acpu1_Core3PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER0_RPU0_0:
			Status = XPsmFwRPUxDirectPwrUp(&Rpu0_Core0PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER0_RPU0_1:
			Status = XPsmFwRPUxDirectPwrUp(&Rpu0_Core1PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_RPU0_0:
			Status = XPsmFwRPUxDirectPwrUp(&Rpu1_Core0PwrCtrl);
			break;
		case XPSMFW_DEV_CLUSTER1_RPU0_1:
			Status = XPsmFwRPUxDirectPwrUp(&Rpu1_Core1PwrCtrl);
			break;
		default:
			Status = XST_INVALID_PARAM;
			break;
	}

	return Status;
}

static struct PwrCtlWakeupHandlerTable_t APUWakeupHandlerTable[] = {
	{ PSMX_GLOBAL_REG_WAKEUP0_IRQ_STATUS_APU0_CORE0_MASK, ACPU0_Core0Wakeup},
	{ PSMX_GLOBAL_REG_WAKEUP0_IRQ_STATUS_APU0_CORE1_MASK, ACPU0_Core1Wakeup},
	{ PSMX_GLOBAL_REG_WAKEUP0_IRQ_STATUS_APU0_CORE2_MASK, ACPU0_Core2Wakeup},
	{ PSMX_GLOBAL_REG_WAKEUP0_IRQ_STATUS_APU0_CORE3_MASK, ACPU0_Core3Wakeup},
	{ PSMX_GLOBAL_REG_WAKEUP0_IRQ_STATUS_APU1_CORE0_MASK, ACPU1_Core0Wakeup},
	{ PSMX_GLOBAL_REG_WAKEUP0_IRQ_STATUS_APU1_CORE1_MASK, ACPU1_Core1Wakeup},
	{ PSMX_GLOBAL_REG_WAKEUP0_IRQ_STATUS_APU1_CORE2_MASK, ACPU1_Core2Wakeup},
	{ PSMX_GLOBAL_REG_WAKEUP0_IRQ_STATUS_APU1_CORE3_MASK, ACPU1_Core3Wakeup},
};

static struct PwrCtlWakeupHandlerTable_t RPUWakeupHandlerTable[] = {
	{ PSMX_GLOBAL_REG_WAKEUP1_IRQ_STATUS_RPU_A_CORE0_MASK, RPU0_Core0Wakeup},
	{ PSMX_GLOBAL_REG_WAKEUP1_IRQ_STATUS_RPU_A_CORE1_MASK, RPU0_Core1Wakeup},
	{ PSMX_GLOBAL_REG_WAKEUP1_IRQ_STATUS_RPU_B_CORE0_MASK, RPU1_Core0Wakeup},
	{ PSMX_GLOBAL_REG_WAKEUP1_IRQ_STATUS_RPU_B_CORE1_MASK, RPU1_Core1Wakeup},
};

static struct PwrCtlWakeupHandlerTable_t SleepHandlerTable[] = {
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_STATUS_APU0_CORE0_PWRDWN_MASK, ACPU0_Core0Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_STATUS_APU0_CORE1_PWRDWN_MASK, ACPU0_Core1Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_STATUS_APU0_CORE2_PWRDWN_MASK, ACPU0_Core2Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_STATUS_APU0_CORE3_PWRDWN_MASK, ACPU0_Core3Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_STATUS_APU1_CORE0_PWRDWN_MASK, ACPU1_Core0Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_STATUS_APU1_CORE1_PWRDWN_MASK, ACPU1_Core1Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_STATUS_APU1_CORE2_PWRDWN_MASK, ACPU1_Core2Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_STATUS_APU1_CORE3_PWRDWN_MASK, ACPU1_Core3Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_MASK_RPU_A_CORE0_PWRDWN_MASK, RPU0_Core0Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_MASK_RPU_A_CORE1_PWRDWN_MASK, RPU0_Core1Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_MASK_RPU_B_CORE0_PWRDWN_MASK, RPU1_Core0Sleep},
	{ PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_MASK_RPU_B_CORE1_PWRDWN_MASK, RPU1_Core1Sleep},
};

/**
 * XPsmFw_DispatchAPUWakeupHandler() - Wakeup up interrupt handler
 *
 * @WakeupStatus    Wake Up status register value
 * @WakeupIntMask   Wake Up interrupt mask register value
 *
 * @return         XST_SUCCESS or error code
 */
XStatus XPsmFw_DispatchAPUWakeupHandler(u32 WakeupStatus, u32 WakeupIntMask)
{
	XStatus Status = XST_FAILURE;
	u32 Index;

	for (Index = 0U; Index < ARRAYSIZE(APUWakeupHandlerTable); Index++) {
		if ((CHECK_BIT(WakeupStatus, APUWakeupHandlerTable[Index].Mask)) &&
		    !(CHECK_BIT(WakeupIntMask, APUWakeupHandlerTable[Index].Mask))) {
			/* Call power up handler */
			Status = APUWakeupHandlerTable[Index].Handler();

			/* Disable wake-up interrupt */
			XPsmFw_Write32(PSMX_GLOBAL_REG_WAKEUP0_IRQ_DIS, APUWakeupHandlerTable[Index].Mask);
		} else {
			Status = XST_SUCCESS;
		}
	}

	return Status;
}

/**
 * XPsmFw_DispatchRPUWakeupHandler() - RPU Wakeup up interrupt handler
 *
 * @WakeupStatus    Wake Up status register value
 * @WakeupIntMask   Wake Up interrupt mask register value
 *
 * @return         XST_SUCCESS or error code
 */
XStatus XPsmFw_DispatchRPUWakeupHandler(u32 WakeupStatus, u32 WakeupIntMask)
{
	XStatus Status = XST_FAILURE;
	u32 Index;
	for (Index = 0U; Index < ARRAYSIZE(RPUWakeupHandlerTable); Index++) {
		if ((CHECK_BIT(WakeupStatus, RPUWakeupHandlerTable[Index].Mask)) &&
		    !(CHECK_BIT(WakeupIntMask, RPUWakeupHandlerTable[Index].Mask))) {
			/* Call power up handler */
			Status = RPUWakeupHandlerTable[Index].Handler();

			/* Disable wake-up interrupt */
			XPsmFw_Write32(PSMX_GLOBAL_REG_WAKEUP1_IRQ_DIS, RPUWakeupHandlerTable[Index].Mask);
		} else {
			Status = XST_SUCCESS;
		}
	}

	return Status;
}

/**
 * XPsmFw_DispatchPwrCtlHandler() - PwrCtl interrupt handler
 *
 * @PwrCtlStatus   Power Down status register value
 * @PwrCtlIntMask  Power Down interrupt mask register value
 *
 * @return         XST_SUCCESS or error code
 */
XStatus XPsmFw_DispatchPwrCtlHandler(u32 PwrCtlStatus, u32 PwrCtlIntMask)
{
	XStatus Status = XST_FAILURE;
	u32 Index;

	for (Index = 0U; Index < ARRAYSIZE(SleepHandlerTable); Index++) {
		if ((CHECK_BIT(PwrCtlStatus, SleepHandlerTable[Index].Mask)) &&
		    !(CHECK_BIT(PwrCtlIntMask, SleepHandlerTable[Index].Mask))) {
			/* Call power up handler */
			Status = SleepHandlerTable[Index].Handler();

			/* Disable direct power-down interrupt */
			XPsmFw_Write32(PSMX_GLOBAL_REG_PWR_CTRL1_IRQ_DIS, SleepHandlerTable[Index].Mask);
		} else {
			Status = XST_SUCCESS;
		}
	}

	return Status;
}

/**
 * XPsmFw_GetPsmToPlmEventAddr() - Provides address of
 * PsmToPlmEvent
 *
 * @EventAddr      Buffer pointer to store PsmToPlmEvent
 */
void XPsmFw_GetPsmToPlmEventAddr(u32 *EventAddr)
{
	*EventAddr = (u32)(&PsmToPlmEvent);
}
