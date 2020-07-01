/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_SOR

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_clock.h>
#include <tegrabl_timer.h>
#include <tegrabl_malloc.h>
#include <tegrabl_sor_dp.h>
#include <tegrabl_dp.h>
#include <tegrabl_drf.h>
#include <tegrabl_addressmap.h>
#include <arsor1.h>
#include <ardisplay.h>

void tegrabl_sor_tpg(struct sor_data *sor, uint32_t tp, uint32_t n_lanes)
{
	uint32_t const tbl[][2] = {
		/* ansi8b/10b encoded, scrambled */
		{1, 1}, /* no pattern, training not in progress */
		{1, 0}, /* training pattern 1 */
		{1, 0}, /* training pattern 2 */
		{1, 0}, /* training pattern 3 */
		{1, 0}, /* D102 */
		{1, 1}, /* SBLERRRATE */
		{0, 0}, /* PRBS7 */
		{0, 0}, /* CSTM */
		{1, 1}, /* HBR2_COMPLIANCE */
	};
	uint32_t cnt;
	uint32_t val = 0;

	for (cnt = 0; cnt < n_lanes; cnt++) {
		uint32_t tp_shift = NV_SOR_DP_TPG_LANE1_PATTERN_SHIFT * cnt;
		val |= tp << tp_shift |
			tbl[tp][0] << (tp_shift +
			NV_SOR_DP_TPG_LANE0_CHANNELCODING_SHIFT) |
			tbl[tp][1] << (tp_shift +
			NV_SOR_DP_TPG_LANE0_SCRAMBLEREN_SHIFT);
	}

	sor_writel(sor, SOR_NV_PDISP_SOR_DP_TPG_0, val);
}

void tegrabl_sor_port_enable(struct sor_data *sor, bool enb)
{
	uint32_t val = 0;

	val = sor_readl(sor, SOR_NV_PDISP_SOR_DP_LINKCTL0_0 + sor->portnum);
	if (enb)
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_LINKCTL0, ENABLE, YES,
								 val);
	else
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_LINKCTL0, ENABLE, NO,
								 val);
	sor_writel(sor, (SOR_NV_PDISP_SOR_DP_LINKCTL0_0 + sor->portnum), val);
}

/* The SOR power sequencer does not work for t124 so SW has to
 * go through the power sequence manually
 * Power up steps from spec:
 * STEP	PDPORT	PDPLL	PDBG	PLLVCOD	PLLCAPD	E_DPD	PDCAL
 * 1		1		1		1		1		1		1		1
 * 2		1		1		1		1		1		0		1
 * 3		1		1		0		1		1		0		1
 * 4		1		0		0		0		0		0		1
 * 5		0		0		0		0		0		0		1 */
static void sor_dp_pad_power_up(struct sor_data *sor)
{
	uint32_t val = 0;

	sor_writel_def(SOR_PLL2, AUX2, OVERRIDE_POWERDOWN, val);
	sor_writel_def(SOR_PLL2, AUX1, SEQ_PLLCAPPD_OVERRIDE, val);

	/* step 1 */
	sor_writel_def(SOR_PLL2, AUX7, PORT_POWERDOWN_ENABLE, val);/* PDPORT */
	sor_writel_def(SOR_PLL2, AUX6, BANDGAP_POWERDOWN_ENABLE, val);/* PDBG */
	sor_writel_def(SOR_PLL2, AUX8, SEQ_PLLCAPPD_ENFORCE_ENABLE, val);/*PLLCAPD*/

	sor_writel_def(SOR_PLL0, PWR, OFF, val);
	sor_writel_def(SOR_PLL0, VCOPD, ASSERT, val);

	sor_pad_cal_power(sor, false);
	tegrabl_udelay(100); /* sleep > 5us */

	/* step 2 */
	sor_writel_def(SOR_PLL2, AUX6, BANDGAP_POWERDOWN_DISABLE, val);
	tegrabl_udelay(150);

	/* step 3 */
	sor_writel_def(SOR_PLL0, PWR, ON, val);/* PDPLL */
	sor_writel_def(SOR_PLL0, VCOPD, RESCIND, val);/* PLLVCOPD */
	sor_writel_def(SOR_PLL2, AUX8, SEQ_PLLCAPPD_ENFORCE_DISABLE, val);
	tegrabl_udelay(1000);

	/* step 4 */
	sor_writel_def(SOR_PLL2, AUX7, PORT_POWERDOWN_DISABLE, val);/* PDPORT */

	/* TERM_ENABLE is disabled at the end of rterm calibration. Re-enable it here. */
	sor_writel_def(SOR_PLL1, TMDS_TERM, ENABLE, val);
	tegrabl_udelay(20);
}

void tegrabl_sor_enable_dp(struct sor_data *sor)
{
	sor_cal(sor);
	sor_dp_pad_power_up(sor);
	sor_power_lanes(sor, sor->link_cfg->lane_count, true);
}

static void sor_get_cm_tx_bitmap(struct sor_data *sor, uint32_t lane_count)
{
	uint32_t i;
	uint32_t val = 0;

	pr_debug("%s() entry\n", __func__);

	val = sor_readl(sor, (SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum));

	for (i = 0; i < lane_count; i++) {
		uint32_t index = sor->xbar_ctrl[i];

		switch (index) {
		case 0:
			val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, COMMONMODE_TXD_2_DP_TXD_0, ENABLE, val);
			break;
		case 1:
			val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, COMMONMODE_TXD_1_DP_TXD_1, ENABLE, val);
			break;
		case 2:
			val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, COMMONMODE_TXD_0_DP_TXD_2, ENABLE, val);
			break;
		case 3:
			val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, COMMONMODE_TXD_3_DP_TXD_3, ENABLE, val);
			break;
		default:
			pr_error("dp: incorrect lane cnt\n");
		}
	}

	sor_writel(sor, (SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum), val);

	pr_debug("%s() exit\n", __func__);
}

void tegrabl_sor_precharge_lanes(struct sor_data *sor)
{
	const struct tegrabl_dp_link_config *cfg = sor->link_cfg;
	uint32_t val = 0;
	pr_debug("%s() entry\n", __func__);

	/* force lanes to output common mode voltage */
	sor_get_cm_tx_bitmap(sor, cfg->lane_count);

	/* precharge for atleast 10us */
	tegrabl_udelay(100);

	/* fallback to normal operation */
	val = sor_readl(sor, (SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum));
	val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, COMMONMODE_TXD_0_DP_TXD_2, DISABLE, val);
	val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, COMMONMODE_TXD_1_DP_TXD_1, DISABLE, val);
	val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, COMMONMODE_TXD_2_DP_TXD_0, DISABLE, val);
	val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, COMMONMODE_TXD_3_DP_TXD_3, DISABLE, val);
	sor_writel(sor, (SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum), val);

	pr_debug("%s() exit\n", __func__);
}

void tegrabl_sor_detach(struct sor_data *sor)
{
	/* not required in bootloader as Sor is already detached
	 * we can revisit this if required in some scenario
	 */
}

