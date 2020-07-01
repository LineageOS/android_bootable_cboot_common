/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
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
#include <arsor.h>
#include <ardisplay.h>

#define NV_SOR_XBAR_BYPASS_MASK (1 << 0)
#define NV_SOR_XBAR_LINK_SWAP_MASK (1 << 1)
#define NV_SOR_XBAR_LINK_XSEL_MASK (0x7)

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

/* power on/off pad calibration logic */
void sor_pad_cal_power(struct sor_data *sor, bool power_up)
{
	uint32_t val = 0;

	val = sor_readl(sor, SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum);
	if (power_up)
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PAD_CAL_PD,
								 POWERUP, val);
	else
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PAD_CAL_PD,
								 POWERDOWN, val);

	sor_writel(sor, SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum, val);
}

/* The SOR power sequencer does not work for t124 so SW has to
 * go through the power sequence manually
 * Power up steps from spec:
 * STEP	PDPORT	PDPLL	PDBG	PLLVCOD	PLLCAPD	E_DPD	PDCAL
 * 1	1		1		1		1		1		1		1
 * 2	1		1		1		1		1		0		1
 * 3	1		1		0		1		1		0		1
 * 4	1		0		0		0		0		0		1
 * 5	0		0		0		0		0		0		1 */
static void sor_dp_pad_power_up(struct sor_data *sor, bool is_lvds)
{
	uint32_t val = 0;

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
	tegrabl_udelay(100); /* sleep > 20 us */

	/* step 3 */
	sor_writel_def(SOR_PLL0, PWR, ON, val);/* PDPLL */
	sor_writel_def(SOR_PLL0, VCOPD, RESCIND, val);/* PLLVCOPD */
	sor_writel_def(SOR_PLL2, AUX8, SEQ_PLLCAPPD_ENFORCE_DISABLE, val);
	tegrabl_udelay(1000);

	/* step 4 */
	sor_writel_def(SOR_PLL2, AUX7, PORT_POWERDOWN_DISABLE, val);/* PDPORT */
}

static void sor_termination_cal(struct sor_data *sor)
{
	uint32_t termadj = 0x8;
	uint32_t cur_try = 0x8;
	uint32_t reg_val;

	sor_writel_num(SOR_PLL1, TMDS_TERMADJ, termadj, reg_val);

	while (cur_try) {
		/* binary search the right value */
		tegrabl_udelay(200);
		reg_val = sor_readl(sor, SOR_NV_PDISP_SOR_PLL1_0);

		if (reg_val & NV_DRF_DEF(SOR_NV_PDISP, SOR_PLL1, TERM_COMPOUT, HIGH))
			termadj -= cur_try;
		cur_try >>= 1;
		termadj += cur_try;

		sor_writel_num(SOR_PLL1, TMDS_TERMADJ, termadj, reg_val);
	}
}

static void sor_dp_cal(struct sor_data *sor)
{
	uint32_t val = 0;
	struct tegrabl_dp *dp = sor->nvdisp->out_data;

	sor_pad_cal_power(sor, true);

	tegrabl_dp_prod_settings(sor, dp->pdata->prod_list, dp_node, 0);

	sor_writel_def(SOR_PLL2, AUX6, BANDGAP_POWERDOWN_DISABLE, val);
	tegrabl_udelay(100);

	sor_writel_def(SOR_PLL0, PLLREG_LEVEL, V45, val);
	sor_writel_def(SOR_PLL0, PWR, ON, val);
	sor_writel_def(SOR_PLL0, VCOPD, RESCIND, val);
	sor_writel_def(SOR_PLL2, AUX1, SEQ_PLLCAPPD_OVERRIDE, val);
	sor_writel_def(SOR_PLL2, AUX9, LVDSEN_OVERRIDE, val);
	sor_writel_def(SOR_PLL2, AUX8, SEQ_PLLCAPPD_ENFORCE_DISABLE, val);
	sor_writel_def(SOR_PLL1, TERM_COMPOUT, HIGH, val);

	if (sor_poll_register(sor, SOR_NV_PDISP_SOR_PLL2_0,
			NV_DRF_DEF(SOR_NV_PDISP, SOR_PLL2, AUX8, DEFAULT_MASK),
			NV_DRF_DEF(SOR_NV_PDISP, SOR_PLL2, AUX8,
					   SEQ_PLLCAPPD_ENFORCE_DISABLE),
			100, SOR_TIMEOUT_MS)) {
		pr_error("DP failed to lock PLL\n");
	}

	sor_writel_def(SOR_PLL2, AUX2, OVERRIDE_POWERDOWN, val);
	sor_writel_def(SOR_PLL2, AUX7, PORT_POWERDOWN_DISABLE, val);

	sor_termination_cal(sor);

	sor_pad_cal_power(sor, false);
}

tegrabl_error_t tegrabl_sor_enable_dp(struct sor_data *sor)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_dp_clock_config(sor->nvdisp, sor->instance,
								  TEGRA_SOR_SAFE_CLK);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	sor_dp_cal(sor);
	sor_dp_pad_power_up(sor, false);

fail:
	return err;
}

void tegrabl_sor_config_xbar(struct sor_data *sor)
{
	uint32_t val = 0;
	uint32_t mask = 0;
	uint32_t shift = 0;
	uint32_t i = 0;

	mask = (NV_SOR_XBAR_BYPASS_MASK | NV_SOR_XBAR_LINK_SWAP_MASK);
	for (i = 0, shift = 2; i < (sizeof(sor->xbar_ctrl) / sizeof(uint32_t));
		 shift += 3, i++) {
		mask |= NV_SOR_XBAR_LINK_XSEL_MASK << shift;
		val |= sor->xbar_ctrl[i] << shift;
	}

	tegrabl_sor_write_field(sor, SOR_NV_PDISP_SOR_XBAR_CTRL_0, mask, val);
	sor_writel(sor, SOR_NV_PDISP_SOR_XBAR_POL_0, 0);
}

void tegrabl_sor_detach(struct sor_data *sor)
{
	/* not required in bootloader as Sor is already detached
	 * we can revisit this if required in some scenario
	 */
}

static uint32_t sor_get_pd_tx_bitmap(struct sor_data *sor, uint32_t lane_count)
{
	uint32_t i;
	uint32_t val = 0;

	pr_debug("%s() entry\n", __func__);

	for (i = 0; i < lane_count; i++) {
		uint32_t index = sor->xbar_ctrl[i];

		switch (index) {
		case 0:
			val |= NV_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PD_TXD_0, NO);
			break;
		case 1:
			val |= NV_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PD_TXD_1, NO);
			break;
		case 2:
			val |= NV_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PD_TXD_2, NO);
			break;
		case 3:
			val |= NV_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PD_TXD_3, NO);
			break;
		default:
			pr_error("dp: incorrect lane cnt\n");
		}
	}

	pr_debug("%s() exit\n", __func__);
	return val;
}

void tegrabl_sor_precharge_lanes(struct sor_data *sor)
{
	const struct tegrabl_dp_link_config *cfg = sor->link_cfg;
	uint32_t val = 0;
	pr_debug("%s() entry\n", __func__);

	val = sor_get_pd_tx_bitmap(sor, cfg->lane_count);

	/* force lanes to output common mode voltage */
	tegrabl_sor_write_field(sor, SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum,
		(0xf << SOR_NV_PDISP_SOR_DP_PADCTL0_0_COMMONMODE_TXD_0_DP_TXD_2_SHIFT),
		(val << SOR_NV_PDISP_SOR_DP_PADCTL0_0_COMMONMODE_TXD_0_DP_TXD_2_SHIFT));

	/* precharge for atleast 10us */
	tegrabl_udelay(100);

	/* fallback to normal operation */
	tegrabl_sor_write_field(sor, SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum,
		(0xf << SOR_NV_PDISP_SOR_DP_PADCTL0_0_COMMONMODE_TXD_0_DP_TXD_2_SHIFT),
		0);

	pr_debug("%s() exit\n", __func__);
}
