/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
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
#include <tegrabl_ar_macro.h>
#include <tegrabl_clock.h>
#include <tegrabl_timer.h>
#include <tegrabl_malloc.h>
#include <arsor1.h>
#include <tegrabl_io.h>
#include <ardisplay.h>
#include <tegrabl_drf.h>
#include <tegrabl_sor.h>

#define SOR_PWM_DUTY_CYCLE 1024
#define SOR_PWM_DIVISOR 1024

#define NV_SOR_XBAR_BYPASS_MASK (1 << 0)
#define NV_SOR_XBAR_LINK_SWAP_MASK (1 << 1)
#define NV_SOR_XBAR_LINK_XSEL_MASK (0x7)

#if defined(IS_T186)
uint32_t sor_base_address[2] = {
	NV_ADDRESS_MAP_SOR_BASE,
	NV_ADDRESS_MAP_SOR1_BASE
};
#else
uint32_t sor_base_address[4] = {
	NV_ADDRESS_MAP_SOR_BASE,
	NV_ADDRESS_MAP_SOR1_BASE,
	NV_ADDRESS_MAP_SOR2_BASE,
	NV_ADDRESS_MAP_SOR3_BASE
};
#endif

tegrabl_error_t sor_init(struct sor_data **phsor, struct tegrabl_display_sor_dtb *sor_dtb)
{
	struct sor_data *hsor;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	hsor = tegrabl_calloc(1, sizeof(struct sor_data));
	if (hsor == NULL) {
		pr_error("%s, memory allocation failed\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	hsor->instance = sor_dtb->sor_instance;
	hsor->base = (void *)(uintptr_t)(sor_base_address[sor_dtb->sor_instance]);
	hsor->portnum = 0;

	*phsor = hsor;

fail:
	return err;
}

tegrabl_error_t sor_poll_register(struct sor_data *sor, uint32_t reg, uint32_t mask, uint32_t exp_val,
		uint32_t poll_interval_us, uint32_t timeout_ms)
{
	time_t start_time = tegrabl_get_timestamp_us();
	time_t curr_time;
	uint32_t r_val = 0;

	do {
		tegrabl_udelay(poll_interval_us);
		r_val = sor_readl(sor, reg);
		curr_time = tegrabl_get_timestamp_us();
		if ((curr_time - start_time) >= timeout_ms * 1000) {
			break;
		}
	} while (((r_val & mask) != exp_val));

	if ((r_val & mask) == exp_val) {
		return TEGRABL_NO_ERROR;	/* success */
	}

	pr_debug("sor_poll_register 0x%x: timeout\n", reg);
	return TEGRABL_ERR_TIMEOUT;
}

static tegrabl_error_t sor_enable_lane_sequencer(struct sor_data *sor, bool pu)
{
	uint32_t val = 0;

	pr_debug("%s: entry\n", __func__);

	val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_LANE_SEQ_CTL, SETTING_NEW, TRIGGER, val);
	val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_LANE_SEQ_CTL, SEQUENCE, DOWN, val);
	val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_LANE_SEQ_CTL, DELAY, 15, val);
	val = pu ? NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_LANE_SEQ_CTL, NEW_POWER_STATE, PU, val) :
		NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_LANE_SEQ_CTL, NEW_POWER_STATE, PD, val);

	if (sor_poll_register(sor, SOR_NV_PDISP_SOR_LANE_SEQ_CTL_0,
			NV_DRF_DEF(SOR_NV_PDISP, SOR_LANE_SEQ_CTL, SEQ_STATE, BUSY),
			NV_DRF_DEF(SOR_NV_PDISP, SOR_LANE_SEQ_CTL, SEQ_STATE, IDLE),
			100, SOR_SEQ_BUSY_TIMEOUT_MS)) {
		pr_error("dp: timeout, sor lane sequencer busy\n");
		return TEGRABL_ERR_BUSY;
	}

	sor_writel(sor, SOR_NV_PDISP_SOR_LANE_SEQ_CTL_0, val);

	if (sor_poll_register(sor, SOR_NV_PDISP_SOR_LANE_SEQ_CTL_0,
			NV_DRF_DEF(SOR_NV_PDISP, SOR_LANE_SEQ_CTL, SETTING_NEW, DEFAULT_MASK),
			NV_DRF_DEF(SOR_NV_PDISP, SOR_LANE_SEQ_CTL, SETTING_NEW, DONE),
			100, SOR_TIMEOUT_MS)) {
		pr_error("dp: timeout, SOR lane sequencer power up/down\n");
		return TEGRABL_ERR_TIMEOUT;
	}

	pr_debug("%s: exit\n", __func__);

	return TEGRABL_NO_ERROR;
}

void sor_set_lane_count(struct sor_data *sor, uint8_t lane_count)
{
	uint32_t val = 0;

	pr_debug("%s: entry\n", __func__);

	val = sor_readl(sor, SOR_NV_PDISP_SOR_DP_LINKCTL0_0 + sor->portnum);
	switch (lane_count) {
	case 0:
		pr_debug("%s: %d\n", __func__, __LINE__);
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_LINKCTL0, LANECOUNT, ZERO, val);
		break;
	case 1:
		pr_debug("%s: %d\n", __func__, __LINE__);
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_LINKCTL0, LANECOUNT, ONE, val);
		break;
	case 2:
		pr_debug("%s: %d\n", __func__, __LINE__);
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_LINKCTL0, LANECOUNT, TWO, val);
		break;
	case 4:
		pr_debug("%s: %d\n", __func__, __LINE__);
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_LINKCTL0, LANECOUNT, FOUR, val);
		break;
	default:
		pr_debug("%s: %d\n", __func__, __LINE__);
		/* 0 should be handled earlier. */
		pr_error("dp: Invalid lane count %d\n", lane_count);
		return;
	}

	sor_writel(sor, (SOR_NV_PDISP_SOR_DP_LINKCTL0_0 + sor->portnum), val);

	pr_debug("%s: exit\n", __func__);
}

static tegrabl_error_t sor_get_pd_tx_bitmap(struct sor_data *sor, uint32_t lane_count)
{
	uint32_t i;
	uint32_t val = 0;
	uint32_t index;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_debug("%s() entry\n", __func__);

	val = sor_readl(sor, (SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum));

	for (i = 0; i < lane_count; i++) {
		index = sor->xbar_ctrl[i];

		switch (index) {
		case 0:
			val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PD_TXD_0, NO, val);
			break;
		case 1:
			val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PD_TXD_1, NO, val);
			break;
		case 2:
			val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PD_TXD_2, NO, val);
			break;
		case 3:
			val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PD_TXD_3, NO, val);
			break;
		default:
			pr_error("dp: incorrect lane cnt\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
			break;
		}
	}

	sor_writel(sor, (SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum), val);

	pr_debug("%s() exit\n", __func__);

	return err;
}

tegrabl_error_t sor_power_lanes(struct sor_data *sor, uint32_t lane_count, bool pu)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_debug("%s: entry\n", __func__);

	if (pu) {
		err = sor_get_pd_tx_bitmap(sor, lane_count);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

		sor_set_lane_count(sor, lane_count);
	}

	err = sor_enable_lane_sequencer(sor, pu);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_debug("%s: exit\n", __func__);

fail:
	return err;
}

tegrabl_error_t sor_set_power_state(struct sor_data *sor, uint32_t pu_pd)
{
	uint32_t r_val = 0;
	uint32_t orig_val = 0;

	pr_debug("%s: entry\n", __func__);

	orig_val = sor_readl(sor, SOR_NV_PDISP_SOR_PWR_0);

	r_val = pu_pd ? NV_DRF_DEF(SOR_NV_PDISP, SOR_PWR, NORMAL_STATE, PU) :
		NV_DRF_DEF(SOR_NV_PDISP, SOR_PWR, NORMAL_STATE, PD);

	if (r_val == orig_val) {
		return TEGRABL_NO_ERROR; /* No update needed */
	}

	r_val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_PWR, SETTING_NEW, TRIGGER, r_val);
	sor_writel(sor, SOR_NV_PDISP_SOR_PWR_0, r_val);

	/* Poll to confirm it is done */
	if (sor_poll_register(sor, SOR_NV_PDISP_SOR_PWR_0,
			NV_DRF_DEF(SOR_NV_PDISP, SOR_PWR, SETTING_NEW, DEFAULT_MASK),
			NV_DRF_DEF(SOR_NV_PDISP, SOR_PWR, SETTING_NEW, DONE),
			100, SOR_TIMEOUT_MS)) {
		pr_error("nvdisp timeout waiting for SOR_PWR = NEW_DONE\n");
		return TEGRABL_ERR_TIMEOUT;
	}

	pr_debug("%s: exit\n", __func__);
	return TEGRABL_NO_ERROR;
}

void sor_set_link_bandwidth(struct sor_data *sor, uint8_t link_bw)
{
	pr_debug("%s: entry\n", __func__);
	uint32_t r_val = 0;

	sor_writel_num(SOR_CLK_CNTRL, DP_LINK_SPEED, link_bw, r_val);

	/* It can take upto 200us for PLLs in analog macro to settle */
	tegrabl_udelay(300);

	pr_debug("%s: exit\n", __func__);
}

void sor_config_hdmi_clk(struct sor_data *sor, uint32_t pclk)
{
	uint32_t link_bw;
	uint32_t r_val = 0;

	pr_debug("%s: entry\n", __func__);

	if (sor->clk_type == TEGRA_SOR_LINK_CLK) {
		return;
	}

	sor_writel_def(SOR_CLK_CNTRL, DP_CLK_SEL, SINGLE_PCLK, r_val);

	/* VCO output has to be doubled in case of 4K@60 mode */
	if (pclk < MAX_1_4_FREQUENCY)
		link_bw = SOR_LINK_SPEED_G2_7;
	else
		link_bw = SOR_LINK_SPEED_G5_4;
	sor_set_link_bandwidth(sor, link_bw);

	sor->clk_type = TEGRA_SOR_LINK_CLK;

	pr_debug("%s: exit\n", __func__);
}

void sor_pad_cal_power(struct sor_data *sor, bool power_up)
{
	uint32_t val = 0;

	val = sor_readl(sor, SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum);
	if (power_up) {
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PAD_CAL_PD, POWERUP, val);
	} else {
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, PAD_CAL_PD, POWERDOWN, val);
	}

	sor_writel(sor, SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum, val);
}

/* hdmi uses sor sequencer for pad power up */
void sor_hdmi_pad_power_up(struct sor_data *sor)
{
	pr_debug("%s: entry\n", __func__);
	uint32_t r_val = 0;

	sor_writel_def(SOR_PLL2, AUX9, LVDSEN_OVERRIDE, r_val);
	sor_writel_def(SOR_PLL2, AUX0, SEQ_PLL_PULLDOWN_OVERRIDE, r_val);
	sor_writel_def(SOR_PLL2, CLKGEN_MODE, DP_TMDS, r_val);
	sor_writel_def(SOR_PLL2, AUX2, OVERRIDE_POWERDOWN, r_val);
	sor_writel_def(SOR_PLL2, AUX1, SEQ_PLLCAPPD_OVERRIDE, r_val);
	sor_writel_def(SOR_PLL2, AUX6, BANDGAP_POWERDOWN_ENABLE, r_val);
	sor_writel_def(SOR_PLL2, AUX7, PORT_POWERDOWN_ENABLE, r_val);
	sor_writel_def(SOR_PLL2, AUX8, SEQ_PLLCAPPD_ENFORCE_ENABLE, r_val);
	sor_writel_def(SOR_PLL0, PWR, OFF, r_val);
	sor_writel_def(SOR_PLL0, VCOPD, ASSERT, r_val);
	sor_pad_cal_power(sor, false);
	tegrabl_udelay(70);

	sor_writel_def(SOR_PLL2, AUX6, BANDGAP_POWERDOWN_DISABLE, r_val);
	tegrabl_udelay(100);

	sor_writel_def(SOR_PLL0, PWR, ON, r_val);
	sor_writel_def(SOR_PLL0, VCOPD, RESCIND, r_val);
	sor_writel_def(SOR_PLL2, AUX8, SEQ_PLLCAPPD_ENFORCE_DISABLE, r_val);
	tegrabl_udelay(300);

	sor_writel_def(SOR_PLL2, AUX7, PORT_POWERDOWN_DISABLE, r_val);

	/* TERM_ENABLE is disabled at the end of rterm calibration. Re-enable it here. */
	sor_writel_def(SOR_PLL1, TMDS_TERM, ENABLE, r_val);
	tegrabl_udelay(20);

	pr_debug("%s: exit\n", __func__);
}

void sor_termination_cal(struct sor_data *sor)
{
	uint32_t termadj = 0x8;
	uint32_t cur_try = 0x8;
	uint32_t reg_val;

	sor_writel_num(SOR_PLL1, TMDS_TERMADJ, termadj, reg_val);

	while (cur_try) {
		/* binary search the right value */
		tegrabl_udelay(200);
		reg_val = sor_readl(sor, SOR_NV_PDISP_SOR_PLL1_0);

		if (reg_val & NV_DRF_DEF(SOR_NV_PDISP, SOR_PLL1, TERM_COMPOUT, HIGH)) {
			termadj -= cur_try;
		}
		cur_try >>= 1;
		termadj += cur_try;

		sor_writel_num(SOR_PLL1, TMDS_TERMADJ, termadj, reg_val);
	}
}

void sor_cal(struct sor_data *sor)
{
	uint32_t r_val = 0;

	pr_debug("%s: entry\n", __func__);

	sor_writel_def(SOR_PLL2, AUX6, BANDGAP_POWERDOWN_DISABLE, r_val);
	sor_writel_def(SOR_PLL1, TMDS_TERM, ENABLE, r_val);
	tegrabl_udelay(100);

	sor_pad_cal_power(sor, true);
	tegrabl_udelay(20);

	sor_termination_cal(sor);

	sor_pad_cal_power(sor, false);
	tegrabl_udelay(20);

	sor_writel_def(SOR_PLL2, AUX6, BANDGAP_POWERDOWN_ENABLE, r_val);
	sor_writel_def(SOR_PLL1, TMDS_TERM, DISABLE, r_val);
	tegrabl_udelay(100);
}

void sor_set_internal_panel(struct sor_data *sor, bool is_int)
{
	uint32_t r_val;

	pr_debug("%s: entry\n", __func__);

	r_val = sor_readl(sor, (SOR_NV_PDISP_SOR_DP_SPARE0_0 + sor->portnum));

	if (is_int) {
		r_val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_SPARE0, PANEL, INTERNAL, r_val);
	} else {
		r_val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_SPARE0, PANEL, EXTERNAL, r_val);
	}

	r_val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_SPARE0, SOR_CLK_SEL, MACRO_SORCLK, r_val);

	sor_writel(sor, (SOR_NV_PDISP_SOR_DP_SPARE0_0 + sor->portnum), r_val);

	if (sor->nvdisp->type == DISPLAY_OUT_HDMI) {
		r_val = sor_readl(sor, SOR_NV_PDISP_SOR_DP_SPARE0_0 + sor->portnum);
		r_val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_SPARE0, DISP_VIDEO_PREAMBLE_CYA, DISABLE, r_val);
		sor_writel(sor, SOR_NV_PDISP_SOR_DP_SPARE0_0 + sor->portnum, r_val);
	}

	pr_debug("%s: exit\n", __func__);
}

void sor_super_update(struct sor_data *sor)
{
	pr_debug("%s: entry\n", __func__);

	sor_writel(sor, SOR_NV_PDISP_SOR_SUPER_STATE0_0, 0);
	sor_writel(sor, SOR_NV_PDISP_SOR_SUPER_STATE0_0, 1);
	sor_writel(sor, SOR_NV_PDISP_SOR_SUPER_STATE0_0, 0);

	pr_debug("%s: exit\n", __func__);
}

void sor_update(struct sor_data *sor)
{
	pr_debug("%s: entry\n", __func__);

	sor_writel(sor, SOR_NV_PDISP_SOR_STATE0_0, 0);
	sor_writel(sor, SOR_NV_PDISP_SOR_STATE0_0, 1);
	sor_writel(sor, SOR_NV_PDISP_SOR_STATE0_0, 0);

	pr_debug("%s: exit\n", __func__);
}

static void sor_config_pwm(struct sor_data *sor,
						   uint32_t pwm_div, uint32_t pwm_dutycycle)
{
	pr_debug("%s: entry\n", __func__);
	uint32_t r_val = 0;

	r_val = NV_DRF_NUM(SOR_NV_PDISP, SOR_PWM_DIV, DIVIDE, pwm_div);
	sor_writel(sor, SOR_NV_PDISP_SOR_PWM_DIV_0, r_val);

	r_val = sor_readl(sor, SOR_NV_PDISP_SOR_PWM_CTL_0);
	r_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_PWM_CTL, DUTY_CYCLE, pwm_dutycycle, r_val);
	r_val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_PWM_CTL, SETTING_NEW, TRIGGER, r_val);
	sor_writel(sor, SOR_NV_PDISP_SOR_PWM_CTL_0, r_val);

	if (sor_poll_register(sor, SOR_NV_PDISP_SOR_PWM_CTL_0,
			NV_DRF_DEF(SOR_NV_PDISP, SOR_PWM_CTL, SETTING_NEW, DEFAULT_MASK),
			NV_DRF_DEF(SOR_NV_PDISP, SOR_PWM_CTL, SETTING_NEW, DONE),
			100, SOR_TIMEOUT_MS)) {
		pr_debug("dp: timeout while waiting for SOR PWM setting\n");
	}

	pr_debug("%s: exit\n", __func__);
}

static void sor_config_panel(struct sor_data *sor, bool is_lvds)
{
	const struct nvdisp_mode *nvdisp_mode = sor->nvdisp->mode;
	uint32_t r_val;
	uint32_t vtotal, htotal;
	uint32_t vsync_end, hsync_end;
	uint32_t vblank_end, hblank_end;
	uint32_t vblank_start, hblank_start;

	pr_debug("%s: entry\n", __func__);

	r_val = NV_DRF_NUM(SOR_NV_PDISP, SOR_STATE1, ASY_OWNER, sor->nvdisp->instance + 1);

	if (sor->nvdisp->type == DISPLAY_OUT_HDMI) {
		r_val |= NV_DRF_DEF(SOR_NV_PDISP, SOR_STATE1, ASY_PROTOCOL, SINGLE_TMDS_A);
	} else {
		r_val |= is_lvds ? NV_DRF_DEF(SOR_NV_PDISP, SOR_STATE1, ASY_PROTOCOL, LVDS_CUSTOM) :
			NV_DRF_DEF(SOR_NV_PDISP, SOR_STATE1, ASY_PROTOCOL, DP_A);
	}

	r_val |= NV_DRF_DEF(SOR_NV_PDISP, SOR_STATE1, ASY_SUBOWNER, NONE) |
		NV_DRF_DEF(SOR_NV_PDISP, SOR_STATE1, ASY_CRCMODE, COMPLETE_RASTER);

	r_val |= (NV_DRF_DEF(SOR_NV_PDISP, SOR_STATE1, ASY_DEPOL, POSITIVE_TRUE) |
		NV_DRF_DEF(SOR_NV_PDISP, SOR_STATE1, ASY_HSYNCPOL, POSITIVE_TRUE) |
		NV_DRF_DEF(SOR_NV_PDISP, SOR_STATE1, ASY_VSYNCPOL, POSITIVE_TRUE));

	r_val |= ((sor->nvdisp->depth * 3) > 18) ?
		NV_DRF_DEF(SOR_NV_PDISP, SOR_STATE1, ASY_PIXELDEPTH, BPP_24_444) :
		NV_DRF_DEF(SOR_NV_PDISP, SOR_STATE1, ASY_PIXELDEPTH, BPP_18_444);

	sor_writel(sor, SOR_NV_PDISP_SOR_STATE1_0, r_val);

	/* Skipping programming NV_HEAD_STATE0, assuming:
	   interlacing: PROGRESSIVE, dynamic range: VESA, colorspace: RGB */

	vtotal = nvdisp_mode->v_sync_width + nvdisp_mode->v_back_porch +
		nvdisp_mode->v_active + nvdisp_mode->v_front_porch;
	htotal = nvdisp_mode->h_sync_width + nvdisp_mode->h_back_porch +
		nvdisp_mode->h_active + nvdisp_mode->h_front_porch;
	r_val = NV_DRF_NUM(SOR_NV_PDISP, HEAD_STATE1, VTOTAL, vtotal) |
		NV_DRF_NUM(SOR_NV_PDISP, HEAD_STATE1, HTOTAL, htotal);
	sor_writel(sor, SOR_NV_PDISP_HEAD_STATE1_0 + sor->nvdisp->instance, r_val);

	vsync_end = nvdisp_mode->v_sync_width - 1;
	hsync_end = nvdisp_mode->h_sync_width - 1;
	r_val = NV_DRF_NUM(SOR_NV_PDISP, HEAD_STATE2, VSYNC_END, vsync_end) |
		NV_DRF_NUM(SOR_NV_PDISP, HEAD_STATE2, HSYNC_END, hsync_end);
	sor_writel(sor, SOR_NV_PDISP_HEAD_STATE2_0 + sor->nvdisp->instance, r_val);

	vblank_end = vsync_end + nvdisp_mode->v_back_porch;
	hblank_end = hsync_end + nvdisp_mode->h_back_porch;
	r_val = NV_DRF_NUM(SOR_NV_PDISP, HEAD_STATE3, VBLANK_END, vblank_end) |
		NV_DRF_NUM(SOR_NV_PDISP, HEAD_STATE3, HBLANK_END, hblank_end);
	sor_writel(sor, SOR_NV_PDISP_HEAD_STATE3_0 + sor->nvdisp->instance, r_val);

	vblank_start = vblank_end + nvdisp_mode->v_active;
	hblank_start = hblank_end + nvdisp_mode->h_active;
	r_val = NV_DRF_NUM(SOR_NV_PDISP, HEAD_STATE4, VBLANK_START, vblank_start) |
		NV_DRF_NUM(SOR_NV_PDISP, HEAD_STATE4, HBLANK_START, hblank_start);
	sor_writel(sor, SOR_NV_PDISP_HEAD_STATE4_0 + sor->nvdisp->instance, r_val);

	sor_writel(sor, SOR_NV_PDISP_HEAD_STATE5_0 + sor->nvdisp->instance, 0x1);

	r_val = sor_readl(sor, SOR_NV_PDISP_SOR_CSTM_0);
	r_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_CSTM, ROTCLK, 2, r_val);
	r_val = is_lvds ? NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_CSTM, LVDS_EN, ENABLE, r_val) :
		NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_CSTM, LVDS_EN, DISABLE, r_val);

	sor_config_pwm(sor, SOR_PWM_DIVISOR, SOR_PWM_DUTY_CYCLE);

	sor_update(sor);

	pr_debug("%s: exit\n", __func__);
}

void sor_enable_sor(struct sor_data *sor, bool enable)
{
	struct tegrabl_nvdisp *nvdisp = sor->nvdisp;
	uint32_t r_val;
	uint32_t enb = 0;
	pr_debug("%s: entry\n", __func__);

	r_val = nvdisp_readl(sor->nvdisp, DISP_DISP_WIN_OPTIONS);
	switch (sor->instance) {
	case 0:
		enb = NV_FLD_SET_DRF_DEF(DC, DISP_DISP_WIN_OPTIONS, SOR_ENABLE, ENABLE, r_val);
		break;
	case 1:
		enb = NV_FLD_SET_DRF_DEF(DC, DISP_DISP_WIN_OPTIONS, SOR1_ENABLE, ENABLE, r_val);
		break;
#if !defined(IS_T186)
	case 2:
		enb = NV_FLD_SET_DRF_DEF(DC, DISP_DISP_WIN_OPTIONS, SOR2_ENABLE, ENABLE, r_val);
		break;
	case 3:
		enb = NV_FLD_SET_DRF_DEF(DC, DISP_DISP_WIN_OPTIONS, SOR3_ENABLE, ENABLE, r_val);
		break;
#endif
	default:
		pr_warn("%s: invalid sor_num:%d\n", __func__, sor->instance);
	}

	if (nvdisp->type == DISPLAY_OUT_HDMI) {
		enb |= NV_DRF_DEF(DC, DISP_DISP_WIN_OPTIONS, SOR1_TIMING_CYA, HDMI);
	}

	r_val = enable ? r_val | enb : r_val & ~enb;
	nvdisp_writel(nvdisp, DISP_DISP_WIN_OPTIONS, r_val);

	pr_debug("%s: exit\n", __func__);
}

static void sor_enable_dc(struct sor_data *sor)
{
	struct tegrabl_nvdisp *nvdisp = sor->nvdisp;
	uint32_t r_val;

	pr_debug("%s: entry\n", __func__);

	/* Enable NVDISP */
	r_val = nvdisp_readl(nvdisp, CMD_DISPLAY_COMMAND);
	r_val = NV_FLD_SET_DRF_DEF(DC, CMD_DISPLAY_COMMAND, DISPLAY_CTRL_MODE, C_DISPLAY, r_val);
	nvdisp_writel(nvdisp, CMD_DISPLAY_COMMAND, r_val);

	pr_debug("%s: exit\n", __func__);
}

void sor_config_xbar(struct sor_data *sor)
{
	uint32_t val = 0;
	uint32_t mask = 0;
	uint32_t shift = 0;
	uint32_t i = 0;

	pr_debug("%s: entry\n", __func__);

	mask = (NV_SOR_XBAR_BYPASS_MASK | NV_SOR_XBAR_LINK_SWAP_MASK);
	for (i = 0, shift = 2; i < (sizeof(sor->xbar_ctrl) / sizeof(uint32_t));
		 shift += 3, i++) {
		mask |= NV_SOR_XBAR_LINK_XSEL_MASK << shift;
		val |= sor->xbar_ctrl[i] << shift;
	}
	tegrabl_sor_write_field(sor, SOR_NV_PDISP_SOR_XBAR_CTRL_0, mask, val);
	sor_writel(sor, SOR_NV_PDISP_SOR_XBAR_POL_0, 0);
}

void sor_attach(struct sor_data *sor)
{
	pr_debug("%s: entry\n", __func__);

	sor_config_panel(sor, false);

	sor_update(sor);

	/* Awake request */
	sor_writel(sor, SOR_NV_PDISP_SOR_SUPER_STATE1_0,
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_SUPER_STATE1, ASY_HEAD_OPMODE, SLEEP) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_SUPER_STATE1, ASY_ORMODE, SAFE) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_SUPER_STATE1, ATTACHED, YES));

	sor_super_update(sor);

	if (sor_poll_register(sor, SOR_NV_PDISP_SOR_TEST_0, SOR_NV_PDISP_SOR_TEST_0_ATTACHED_FIELD,
						  NV_DRF_DEF(SOR_NV_PDISP, SOR_TEST, ATTACHED, TRUE),
						  100, SOR_ATTACH_TIMEOUT_MS)) {
		pr_info("nvdisp timeout waiting for SOR ATTACHED\n");
	}

	sor_writel(sor, SOR_NV_PDISP_SOR_SUPER_STATE1_0,
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_SUPER_STATE1, ASY_HEAD_OPMODE, SLEEP) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_SUPER_STATE1, ASY_ORMODE, NORMAL) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_SUPER_STATE1, ATTACHED, YES));

	sor_super_update(sor);

	sor_enable_dc(sor);

	sor_enable_sor(sor, true);

	sor_writel(sor, SOR_NV_PDISP_SOR_SUPER_STATE1_0,
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_SUPER_STATE1, ASY_HEAD_OPMODE, AWAKE) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_SUPER_STATE1, ASY_ORMODE, NORMAL) |
	   NV_DRF_DEF(SOR_NV_PDISP, SOR_SUPER_STATE1, ATTACHED, YES));

	sor_super_update(sor);

	pr_debug("%s: exit\n", __func__);
}

void sor_config_prod_settings(struct sor_data *sor, struct prod_list *prod_list,
	struct prod_pair *node, uint32_t clk)
{
	uint32_t i, j;
	struct prod_settings *prod_settings;
	struct prod_tuple prod_tuple;
	uint32_t val;

	if (!prod_list) {
		pr_error("prod settings not found!\n");
		return;
	}

	prod_settings = prod_list->prod_settings;
	for (i = 0; i < prod_list->num; i++) {
		if (node[i].clk == 0) { /*single entry in prod_list*/
			break;
		}
		if (clk <= node[i].clk) {
			break;
		}
	}

	if (i < prod_list->num) {
		for (j = 0; j < prod_list->prod_settings[i].count; j++) {
			prod_tuple = prod_settings[i].prod_tuple[j];
			val = sor_readl(sor, prod_tuple.addr / 4);
			val = ((val & ~prod_tuple.mask) | (prod_tuple.val & prod_tuple.mask));
				/*the above logic is based on 1-style masking*/
			sor_writel(sor, prod_tuple.addr / 4, val);
		}
	}
}

void sor_dump_registers(struct sor_data *sor)
{
	uint32_t *addr;

	pr_debug("sor register dump\n");
	for (addr = sor->base; (void *)addr <= sor->base + 0x5E8; addr += 4) {
		pr_debug("%p: %08x %08x %08x %08x\n", addr, *(addr), *(addr + 1), *(addr + 2), *(addr + 3));
	}
	pr_debug("\n");
}
