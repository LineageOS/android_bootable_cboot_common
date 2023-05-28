 /*
  * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
  *
  * NVIDIA Corporation and its licensors retain all intellectual property
  * and proprietary rights in and to this software, related documentation
  * and any modifications thereto.	Any use, reproduction, disclosure or
  * distribution of this software and related documentation without an express
  * license agreement from NVIDIA Corporation is strictly prohibited.
  */

#define MODULE TEGRABL_ERR_DP

#include <string.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_clock.h>
#include <tegrabl_malloc.h>
#include <tegrabl_timer.h>
#include <tegrabl_ar_macro.h>
#include <tegrabl_dp.h>
#include <tegrabl_sor.h>
#include <tegrabl_dpaux.h>
#include <tegrabl_drf.h>
#include <ardpaux.h>
#include <arsor1.h>

#define DP_POWER_ON_MAX_TRIES 3

static bool tegra_dp_debug = true;

/* configures sor clock for DP */
tegrabl_error_t tegrabl_dp_clock_config(struct tegrabl_nvdisp *nvdisp, int32_t instance, uint32_t clk_type)
{
	uint32_t clk_rate = 0;
	uint32_t pclk = nvdisp->mode->pclk / 1000;
	uint32_t parent_pad_clk = 0;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	pr_debug("%s: entry\n", __func__);
	if (clk_type == TEGRA_SOR_SAFE_CLK) {
		CHECK_RET(tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR_SAFE, 0, TEGRABL_CLK_SRC_PLLP_OUT0));
		CHECK_RET(tegrabl_car_clk_enable(TEGRABL_MODULE_SOR_SAFE, 0, NULL));

		CHECK_RET(tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR_OUT, instance, TEGRABL_CLK_SRC_SOR_SAFE_CLK));
		CHECK_RET(tegrabl_car_clk_enable(TEGRABL_MODULE_SOR_OUT, instance, NULL));
		tegrabl_udelay(20);

		CHECK_RET(tegrabl_car_rst_set(TEGRABL_MODULE_SOR, instance));
		CHECK_RET(tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR, instance, TEGRABL_CLK_SRC_PLLDP));
		CHECK_RET(tegrabl_car_clk_enable(TEGRABL_MODULE_SOR, instance, NULL));
		CHECK_RET(tegrabl_car_rst_clear(TEGRABL_MODULE_SOR, instance));
		tegrabl_udelay(20);
	} else if (clk_type == TEGRA_SOR_LINK_CLK) {
		CHECK_RET(tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR_PAD_CLKOUT, instance, TEGRABL_CLK_SRC_PLLDP));
		CHECK_RET(tegrabl_car_set_clk_rate(TEGRABL_MODULE_SOR_PAD_CLKOUT, instance, pclk, &clk_rate));
		CHECK_RET(tegrabl_car_clk_enable(TEGRABL_MODULE_SOR_PAD_CLKOUT, instance, NULL));

		switch (instance) {
		case 0:
			parent_pad_clk = TEGRABL_CLK_SRC_SOR0_PAD_CLKOUT;
			break;
		case 1:
			parent_pad_clk = TEGRABL_CLK_SRC_SOR1_PAD_CLKOUT;
			break;
#if !defined(IS_T186)
		case 2:
			parent_pad_clk = TEGRABL_CLK_SRC_SOR2_PAD_CLKOUT;
			break;
		case 3:
			parent_pad_clk = TEGRABL_CLK_SRC_SOR3_PAD_CLKOUT;
			break;
#endif
		default:
			pr_error("%s: invalid SOR instance %d\n", __func__, instance);
			ret = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
		}

		if (parent_pad_clk != TEGRABL_CLK_SRC_INVALID) {
			CHECK_RET(tegrabl_car_set_clk_src(TEGRABL_MODULE_SOR_OUT, instance, parent_pad_clk));
			CHECK_RET(tegrabl_car_clk_enable(TEGRABL_MODULE_SOR_OUT, instance, NULL));
			tegrabl_udelay(250);
		}
	} else {
		pr_error("%s: invalid clk type\n", __func__);
		ret = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	pr_debug("%s: exit\n", __func__);
	return ret;
}

/* read DPCD from DP panel */
tegrabl_error_t tegrabl_dp_dpcd_read(struct tegrabl_dp *dp, uint32_t addr,
									 uint8_t *data_ptr)
{
	uint32_t size = 1;
	uint32_t status = 0;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	ret = tegrabl_dpaux_read(dp->hdpaux, AUX_CMD_AUXRD, addr, data_ptr, &size,
							 &status);

	if (ret != TEGRABL_NO_ERROR) {
		pr_error("dp: Failed to read DPCD data. CMD 0x%x, Status 0x%x\n", addr,
				 status);
	}

	return ret;
}

/* write DPCD to DP panel */
tegrabl_error_t tegrabl_dp_dpcd_write(struct tegrabl_dp *dp, uint32_t addr,
									  uint8_t data)
{
	uint32_t size = 1;
	uint32_t status = 0;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	ret = tegrabl_dpaux_write(dp->hdpaux, AUX_CMD_AUXWR, addr, &data, &size,
							  &status);

	if (ret != TEGRABL_NO_ERROR) {
		pr_error("dp: Failed to write DPCD data. CMD 0x%x, Status 0x%x\n", addr,
				 status);
	}
	return ret;
}

/* write DPCD to DP panel using masking */
tegrabl_error_t tegrabl_dp_dpcd_write_field(struct tegrabl_dp *dp,
	uint32_t cmd, uint8_t mask, uint8_t data)
{
	uint8_t dpcd_data;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	CHECK_RET(tegrabl_dp_dpcd_read(dp, cmd, &dpcd_data));
	dpcd_data &= ~mask;
	dpcd_data |= data;
	CHECK_RET(tegrabl_dp_dpcd_write(dp, cmd, dpcd_data));

	return ret;
}

/* 64 bit division */
static inline uint64_t tegra_div64(uint64_t dividend, uint32_t divisor)
{
	do_div(dividend, divisor);
	return dividend;
}

/* set DP panel power state */
static tegrabl_error_t dp_panel_power_state(struct tegrabl_dp *dp,
	uint8_t state)
{
	uint32_t retry = 0;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	do {
		ret = tegrabl_dp_dpcd_write(dp, DPCD_SET_POWER, state);
	} while ((state != DPCD_SET_POWER_VAL_D3_PWRDWN) &&
		(retry++ < DP_POWER_ON_MAX_TRIES) && ret);

	return ret;
}

/* dump DP link configuration */
static void dp_dump_link_cfg(struct tegrabl_dp *dp,
							 const struct tegrabl_dp_link_config *cfg)
{
	if (!tegra_dp_debug) {
		return;
	}

	pr_info("DP config: cfg_name : cfg_value\n");
	pr_info("           Lane Count             %d\n",
			cfg->max_lane_count);
	pr_info("           SupportEnhancedFraming %s\n",
			cfg->support_enhanced_framing ? "Y" : "N");
	pr_info("           SupportAltScrmbRstFffe %s\n",
			cfg->alt_scramber_reset_cap ? "Y" : "N");
	pr_info("           Bandwidth              %d\n",
			cfg->max_link_bw);
	pr_info("           bpp                    %d\n",
			cfg->bits_per_pixel);
	pr_info("           EnhancedFraming        %s\n",
			cfg->enhanced_framing ? "Y" : "N");
	pr_info("           Scramble_enabled       %s\n",
			cfg->scramble_ena ? "Y" : "N");
	pr_info("           LinkBW                 %d\n",
			cfg->link_bw);
	pr_info("           lane_count             %d\n",
			cfg->lane_count);
	pr_info("           activespolarity        %d\n",
			cfg->activepolarity);
	pr_info("           active_count           %d\n",
			cfg->active_count);
	pr_info("           tu_size                %d\n",
			cfg->tu_size);
	pr_info("           active_frac            %d\n",
			cfg->active_frac);
	pr_info("           watermark              %d\n",
			cfg->watermark);
	pr_info("           hblank_sym             %d\n",
			cfg->hblank_sym);
	pr_info("           vblank_sym             %d\n",
			cfg->vblank_sym);
};

/* Calcuate if given cfg can meet the mode request. */
/* Return true if mode is possible, false otherwise. */
bool tegrabl_dp_calc_config(struct tegrabl_dp *dp,
	const struct nvdisp_mode *mode, struct tegrabl_dp_link_config *cfg)
{
	const uint32_t link_rate = 27 * cfg->link_bw * 1000 * 1000;
	const uint64_t f = 100000; /* precision factor */
	uint32_t num_linkclk_line; /* Number of link clocks per line */
	uint64_t ratio_f; /* Ratio of incoming to outgoing data rate */
	uint64_t frac_f;
	uint64_t activesym_f; /* Activesym per TU */
	uint64_t activecount_f;
	uint32_t activecount;
	uint32_t activepolarity;
	uint64_t approx_value_f;
	uint32_t activefrac = 0;
	uint64_t accumulated_error_f = 0;
	uint32_t lowest_neg_activecount = 0;
	uint32_t lowest_neg_activepolarity = 0;
	uint32_t lowest_neg_tusize = 64;
	uint32_t num_symbols_per_line;
	uint64_t lowest_neg_activefrac = 0;
	uint64_t lowest_neg_error_f = 64 * f;
	uint64_t watermark_f;
	int i;
	bool neg;
	uint64_t rate;

	pr_debug("%s() ENTRY\n", __func__);

	cfg->is_valid = false;
	rate = dp->mode->pclk;

	if (!link_rate || !cfg->lane_count || !rate || !cfg->bits_per_pixel) {
		return false;
	}

	if ((uint64_t)rate * cfg->bits_per_pixel >=
		(uint64_t)link_rate * 8 * cfg->lane_count) {
		pr_debug("Requested rate calc > link_rate calc\n");
		return false;
	}

	num_linkclk_line = (uint32_t)tegra_div64(
		(uint64_t)link_rate * mode->h_active, rate);

	ratio_f = (uint64_t)rate * cfg->bits_per_pixel * f;
	ratio_f /= 8;
	ratio_f = tegra_div64(ratio_f, link_rate * cfg->lane_count);

	for (i = 64; i >= 32; --i) {
		activesym_f = ratio_f * i;
		activecount_f = tegra_div64(activesym_f, (uint32_t)f) * f;
		frac_f = activesym_f - activecount_f;
		activecount = (uint32_t)tegra_div64(activecount_f, (uint32_t)f);

		if (frac_f < (f / 2)) /* fraction < 0.5 */
			activepolarity = 0;
		else {
			activepolarity = 1;
			frac_f = f - frac_f;
		}

		if (frac_f != 0) {
			frac_f = tegra_div64((f * f),  frac_f); /* 1/fraction */
			if (frac_f > (15 * f))
				activefrac = activepolarity ? 1 : 15;
			else
				activefrac = activepolarity ?
					(uint32_t)tegra_div64(frac_f, (uint32_t)f) + 1 :
					(uint32_t)tegra_div64(frac_f, (uint32_t)f);
		}

		if (activefrac == 1) {
			activepolarity = 0;
		}

		if (activepolarity == 1)
			approx_value_f = activefrac ? tegra_div64(
				activecount_f + (activefrac * f - f) * f,
				(activefrac * f)) :	activecount_f + f;
		else
			approx_value_f = activefrac ?
				activecount_f + tegra_div64(f, activefrac) : activecount_f;

		if (activesym_f < approx_value_f) {
			accumulated_error_f = num_linkclk_line *
				tegra_div64(approx_value_f - activesym_f, i);
			neg = true;
		} else {
			accumulated_error_f = num_linkclk_line *
				tegra_div64(activesym_f - approx_value_f, i);
			neg = false;
		}

		if ((neg && (lowest_neg_error_f > accumulated_error_f)) ||
			(accumulated_error_f == 0)) {
			lowest_neg_error_f = accumulated_error_f;
			lowest_neg_tusize = i;
			lowest_neg_activecount = activecount;
			lowest_neg_activepolarity = activepolarity;
			lowest_neg_activefrac = activefrac;

			if (accumulated_error_f == 0) {
				break;
			}
		}
	}

	if (lowest_neg_activefrac == 0) {
		cfg->activepolarity = 0;
		cfg->active_count   = lowest_neg_activepolarity ?
			lowest_neg_activecount : lowest_neg_activecount - 1;
		cfg->tu_size	    = lowest_neg_tusize;
		cfg->active_frac    = 1;
	} else {
		cfg->activepolarity = lowest_neg_activepolarity;
		cfg->active_count   = (uint32_t)lowest_neg_activecount;
		cfg->tu_size	    = lowest_neg_tusize;
		cfg->active_frac    = (uint32_t)lowest_neg_activefrac;
	}

	pr_debug("%s: polarity: %d active count: %d tu size: %d, active frac: %d\n",
			 __func__, cfg->activepolarity, cfg->active_count, cfg->tu_size,
			 cfg->active_frac);

	watermark_f = tegra_div64(ratio_f * cfg->tu_size * (f - ratio_f), f);
	cfg->watermark = (uint32_t)tegra_div64(watermark_f + lowest_neg_error_f, f)
					+ cfg->bits_per_pixel / 4 - 1;
	num_symbols_per_line = (mode->h_active * cfg->bits_per_pixel) /
						(8 * cfg->lane_count);
	if (cfg->watermark > 30) {
		pr_debug("%s: unable to get a good tusize, force watermark to 30.\n",
				 __func__);
		cfg->watermark = 30;
		return false;
	} else if (cfg->watermark > num_symbols_per_line) {
		pr_debug("%s: force watermark to the number of symbols in the line.\n",
				 __func__);
		cfg->watermark = num_symbols_per_line;
		return false;
	}

	/* Refer to dev_disp.ref for more information.
	 * # symbols/hblank = ((SetRasterBlankEnd.X + SetRasterSize.Width -
	 *						SetRasterBlankStart.X - 7) * link_clk / pclk)
	 *						- 3 * enhanced_framing - Y
	 * where Y = (# lanes == 4) 3 : (# lanes == 2) ? 6 : 12 */
	cfg->hblank_sym = (int)tegra_div64((uint64_t)(mode->h_back_porch +
		mode->h_front_porch + mode->h_sync_width - 7) * link_rate, rate) - 3
		* cfg->enhanced_framing - (12 / cfg->lane_count);

	if (cfg->hblank_sym < 0) {
		cfg->hblank_sym = 0;
	}


	/* Refer to dev_disp.ref for more information.
	 * # symbols/vblank = ((SetRasterBlankStart.X -
	 *						SetRasterBlankEen.X - 25) * link_clk / pclk)
	 *						- Y - 1;
	 * where Y = (# lanes == 4) 12 : (# lanes == 2) ? 21 : 39 */
	cfg->vblank_sym = (int)tegra_div64((uint64_t)(mode->h_active - 25)
		* link_rate, rate) - (36 / cfg->lane_count) - 4;

	if (cfg->vblank_sym < 0) {
		cfg->vblank_sym = 0;
	}

	cfg->is_valid = true;

	return true;
}

/* set max link configuration for particular DP panel */
static tegrabl_error_t dp_init_max_link_cfg(struct tegrabl_dp *dp,
											struct tegrabl_dp_link_config *cfg)
{
	uint8_t dpcd_data;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	if (dp->sink_cap_valid)
		dpcd_data = dp->sink_cap[DPCD_MAX_LANE_COUNT];
	else
		CHECK_RET(tegrabl_dp_dpcd_read(dp, DPCD_MAX_LANE_COUNT, &dpcd_data));

	cfg->max_lane_count = dpcd_data & DPCD_MAX_LANE_COUNT_MASK;

	if (cfg->max_lane_count >= 4)
		cfg->max_lane_count = 4;
	else if (cfg->max_lane_count >= 2)
		cfg->max_lane_count = 2;
	else
		cfg->max_lane_count = 1;

	if (dp->pdata && dp->pdata->lanes && (dp->pdata->lanes < cfg->max_lane_count)) {
			cfg->max_lane_count = dp->pdata->lanes;
	}

	if (dpcd_data & DPCD_MAX_LANE_COUNT_TPS3_SUPPORTED_YES) {
		cfg->tps = TRAINING_PATTERN_3;
	} else {
		cfg->tps = TRAINING_PATTERN_2;
	}

	cfg->support_enhanced_framing =
		(dpcd_data & DPCD_MAX_LANE_COUNT_ENHANCED_FRAMING_YES) ?
		true : false;

	if (dp->sink_cap_valid)
		dpcd_data = dp->sink_cap[DPCD_MAX_DOWNSPREAD];
	else
		CHECK_RET(tegrabl_dp_dpcd_read(dp, DPCD_MAX_DOWNSPREAD, &dpcd_data));

#if !defined(IS_T186) /*added in t19x*/
	/*
	 * The check for TPS4 should be after the check for TPS3. That helps
	 * assign a higher priority to TPS4
	 */
	if (dpcd_data & DPCD_MAX_DOWNSPREAD_TPS4_SUPPORTED_YES) {
		cfg->tps = TRAINING_PATTERN_4;
	}
#endif

	cfg->downspread = (dpcd_data & DPCD_MAX_DOWNSPREAD_VAL_0_5_PCT) ?
		true : false;
	cfg->support_fast_lt = (dpcd_data &
		DPCD_MAX_DOWNSPREAD_NO_AUX_HANDSHAKE_LT_T) ? true : false;

	CHECK_RET(tegrabl_dp_dpcd_read(dp, DPCD_TRAINING_AUX_RD_INTERVAL,
								   &dpcd_data));

	cfg->aux_rd_interval = dpcd_data & DPCD_TRAINING_AUX_RD_INTERVAL_MASK;

	if (dp->sink_cap_valid)
		cfg->max_link_bw = dp->sink_cap[DPCD_MAX_LINK_BANDWIDTH];
	else
		CHECK_RET(tegrabl_dp_dpcd_read(dp, DPCD_MAX_LINK_BANDWIDTH,
									   &cfg->max_link_bw));

	if (cfg->max_link_bw >= SOR_LINK_SPEED_G5_4)
		cfg->max_link_bw = SOR_LINK_SPEED_G5_4;
	else if (cfg->max_link_bw >= SOR_LINK_SPEED_G2_7)
		cfg->max_link_bw = SOR_LINK_SPEED_G2_7;
	else
		cfg->max_link_bw = SOR_LINK_SPEED_G1_62;

	if (dp->pdata && dp->pdata->link_bw && (dp->pdata->link_bw < cfg->max_link_bw)) {
			cfg->max_link_bw = dp->pdata->link_bw;
	}

	CHECK_RET(tegrabl_dp_dpcd_read(dp, DPCD_EDP_CONFIG_CAP, &dpcd_data));

	cfg->alt_scramber_reset_cap =
		(dpcd_data & DPCD_EDP_CONFIG_CAP_ASC_RESET_YES) ? true : false;

	cfg->only_enhanced_framing = (dpcd_data &
		DPCD_EDP_CONFIG_CAP_FRAMING_CHANGE_YES) ? true : false;

	cfg->edp_cap = (dpcd_data &
		DPCD_EDP_CONFIG_CAP_DISPLAY_CONTROL_CAP_YES) ? true : false;

	CHECK_RET(tegrabl_dp_dpcd_read(dp, DPCD_FEATURE_ENUM_LIST, &dpcd_data));

	cfg->support_vsc_ext_colorimetry = (dpcd_data &
		DPCD_FEATURE_ENUM_LIST_VSC_EXT_COLORIMETRY) ? true : false;

	cfg->bits_per_pixel = (dp->nvdisp->depth * 3) ? : 24;

	cfg->lane_count = cfg->max_lane_count;

	cfg->link_bw = cfg->max_link_bw;

	cfg->enhanced_framing = cfg->support_enhanced_framing;

	tegrabl_dp_calc_config(dp, dp->mode, cfg);

	dp->max_link_cfg = *cfg;

	return ret;
}

static tegrabl_error_t dp_set_assr(struct tegrabl_dp *dp, bool ena)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	uint8_t dpcd_data = ena ? DPCD_EDP_CONFIG_SET_ASC_RESET_ENABLE :
							DPCD_EDP_CONFIG_SET_ASC_RESET_DISABLE;

	ret = tegrabl_dp_dpcd_write(dp, DPCD_EDP_CONFIG_SET, dpcd_data);
	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Also reset the scrambler to 0xfffe */
	sor_set_internal_panel(dp->sor, ena);

fail:
	return ret;
}

/* set link bandwidth in sor register and in DP panel */
static tegrabl_error_t dp_set_link_bandwidth(struct tegrabl_dp *dp,
											 uint8_t link_bw)
{
	sor_set_link_bandwidth(dp->sor, link_bw);

	/* Sink side */
	return tegrabl_dp_dpcd_write(dp, DPCD_LINK_BANDWIDTH_SET, link_bw);
}

/* set enhanced framing enabled or not in sor register and DP panel */
static tegrabl_error_t dp_set_enhanced_framing(struct tegrabl_dp *dp,
											   bool enable)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	struct sor_data *sor = dp->sor;
	uint32_t val = 0;

	if (enable) {
		val = sor_readl(sor, SOR_NV_PDISP_SOR_DP_LINKCTL0_0 + sor->portnum);
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_LINKCTL0, ENHANCEDFRAME,
								 ENABLE, val);
		sor_writel(sor, SOR_NV_PDISP_SOR_DP_LINKCTL0_0 + sor->portnum, val);

		CHECK_RET(tegrabl_dp_dpcd_write_field(dp, DPCD_LANE_COUNT_SET,
					DPCD_LANE_COUNT_SET_ENHANCEDFRAMING_T,
					DPCD_LANE_COUNT_SET_ENHANCEDFRAMING_T));
	}

	return ret;
}

/* set Lane count in sor register and DP panel */
static tegrabl_error_t dp_set_lane_count(struct tegrabl_dp *dp,
										 uint8_t lane_cnt)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	sor_power_lanes(dp->sor, lane_cnt, true);

	CHECK_RET(tegrabl_dp_dpcd_write_field(dp, DPCD_LANE_COUNT_SET,
				DPCD_LANE_COUNT_SET_MASK, lane_cnt));

	return ret;
}

/* configure DP link speed */
static tegrabl_error_t dp_link_cal(struct tegrabl_dp *dp)
{
	struct tegrabl_dp_link_config *cfg = &dp->link_cfg;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (dp->pdata->br_prod_list == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
		goto fail;
	}

	switch (cfg->link_bw) {
	case SOR_LINK_SPEED_G1_62:  /* RBR */
		pr_debug("%s() --RBR\n", __func__);
		sor_config_prod_settings(dp->sor, dp->pdata->br_prod_list, dp_br_nodes, SOR_LINK_SPEED_G1_62);
		break;
	case SOR_LINK_SPEED_G2_7:   /* HBR */
		pr_debug("%s() --HBR\n", __func__);
		sor_config_prod_settings(dp->sor, dp->pdata->br_prod_list, dp_br_nodes, SOR_LINK_SPEED_G2_7);
		break;
	case SOR_LINK_SPEED_G5_4:  /* HBR2 */
		pr_debug("%s() --HBR2\n", __func__);
		sor_config_prod_settings(dp->sor, dp->pdata->br_prod_list, dp_br_nodes, SOR_LINK_SPEED_G5_4);
		break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 1);
	}

fail:
	return err;
}

/* DP initialization */
static tegrabl_error_t tegrabl_dp_init(struct tegrabl_nvdisp *nvdisp, struct tegrabl_display_pdata *pdata)
{
	struct tegrabl_dp *dp;
	struct sor_data *sor;
	struct tegrabl_display_sor_dtb *sor_dtb = &(pdata->sor_dtb);
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	dp = tegrabl_calloc(1, sizeof(struct tegrabl_dp));
	if (!dp) {
		pr_error("%s, memory allocation failed\n", __func__);
		ret = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	memset(dp, 0, sizeof(struct tegrabl_dp));

	ret = sor_init(&sor, sor_dtb);
	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}
	sor->nvdisp = nvdisp;
	sor->link_cfg = &(dp->link_cfg);
	memcpy(sor->xbar_ctrl, sor_dtb->xbar_ctrl, XBAR_CNT * sizeof(uint32_t));

	dp->nvdisp = nvdisp;
	dp->mode = nvdisp->mode;
	dp->sor = sor;
	dp->enabled = false;
	dp->pdata = &(pdata->dp_dtb);

	nvdisp->out_data = dp;

	tegrabl_dp_lt_init(&dp->lt_data, dp);
	pr_debug("dp init end\n");

	return tegrabl_dpaux_init_aux(sor_dtb->dpaux_instance, &(dp->hdpaux));

fail:
	if (dp) {
		tegrabl_free(dp);
	}
	return ret;
}

/* DPCD initialization for DP panel */
static void dp_dpcd_init(struct tegrabl_dp *dp)
{
	struct tegrabl_dp_link_config *cfg = &dp->link_cfg;
	uint32_t size_ieee_oui = 3, auxstat;
	uint8_t data_ieee_oui_be[3] = {
		(IEEE_OUI >> 16) & 0xff,
		(IEEE_OUI >> 8) & 0xff,
		IEEE_OUI & 0xff
	};

	/* Check DP version */
	if (tegrabl_dp_dpcd_read(dp, DPCD_REV, &dp->revision)) {
		pr_error("dp: failed to read the revision number from sink\n");
	}

	if (dp_init_max_link_cfg(dp, cfg)) {
		pr_error("dp: failed to init link configuration\n");
	}

	tegrabl_dpaux_write(dp->hdpaux, AUX_CMD_AUXWR, DPCD_SOURCE_IEEE_OUI,
						data_ieee_oui_be, &size_ieee_oui, &auxstat);
}

/* Training pattern generator for DP panel */
void tegrabl_dp_tpg(struct tegrabl_dp *dp, uint32_t tp, uint32_t n_lanes)
{
	if (tp == TRAINING_PATTERN_DISABLE)
		tegrabl_dp_dpcd_write(dp, DPCD_TRAINING_PATTERN_SET,
							  (tp | DPCD_TRAINING_PATTERN_SET_SC_DISABLED_F));
	else {
		if (tp == TRAINING_PATTERN_4) {
			tegrabl_dp_dpcd_write(dp, DPCD_TRAINING_PATTERN_SET,
					(0x7 | DPCD_TRAINING_PATTERN_SET_SC_DISABLED_F));
		} else {
			tegrabl_dp_dpcd_write(dp, DPCD_TRAINING_PATTERN_SET,
					(tp | DPCD_TRAINING_PATTERN_SET_SC_DISABLED_T));
		}
	}

	tegrabl_sor_tpg(dp->sor, tp, n_lanes);
}

static void dp_tu_config(struct tegrabl_dp *dp,
						 const struct tegrabl_dp_link_config *cfg)
{
	struct sor_data *sor = dp->sor;
	uint32_t val;

	val = sor_readl(sor, SOR_NV_PDISP_SOR_DP_LINKCTL0_0 + sor->portnum);
	val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_DP_LINKCTL0, TUSIZE,
							 cfg->tu_size, val);
	sor_writel(sor, SOR_NV_PDISP_SOR_DP_LINKCTL0_0 + sor->portnum, val);

	val = sor_readl(sor, (SOR_NV_PDISP_SOR_DP_CONFIG0_0 + sor->portnum));
	val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_DP_CONFIG0, WATERMARK,
							 cfg->watermark, val);
	val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_DP_CONFIG0, ACTIVESYM_COUNT,
							 cfg->active_count, val);
	val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_DP_CONFIG0, ACTIVESYM_FRAC,
							 cfg->active_frac, val);
	val = cfg->activepolarity ?
			NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_CONFIG0, ACTIVESYM_POLARITY,
							   POSITIVE, val) :
			NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_CONFIG0, ACTIVESYM_POLARITY,
							   NEGATIVE, val);
	val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_CONFIG0, ACTIVESYM_CNTL,
							 ENABLE, val);
	val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_CONFIG0, RD_RESET_VAL,
							 NEGATIVE, val);
	sor_writel(sor, (SOR_NV_PDISP_SOR_DP_CONFIG0_0 + sor->portnum), val);
}

/* update Link configuration data on DP panel */
void tegrabl_dp_update_link_config(struct tegrabl_dp *dp)
{
	struct tegrabl_dp_link_config *cfg = &dp->link_cfg;

	dp_set_link_bandwidth(dp, cfg->link_bw);
	dp_set_lane_count(dp, cfg->lane_count);
	dp_link_cal(dp);
	dp_tu_config(dp, cfg);
}

/* Enable DP
 * Enable Sor Macro Clock
 * Perform Link Training
 */
static tegrabl_error_t tegrabl_dp_enable(struct tegrabl_nvdisp *nvdisp)
{
	struct tegrabl_dp *dp = nvdisp->out_data;
	struct tegrabl_dp_link_config *cfg = &dp->link_cfg;
	struct sor_data *sor = dp->sor;
	int ret;
	uint32_t val;

	if (dp->enabled) {
		return TEGRABL_NO_ERROR;
	}

	pr_debug("DP enable enter\n");

	ret = dp_panel_power_state(dp, DPCD_SET_POWER_VAL_D0_NORMAL);
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("dp: failed to exit panel power save mode (0x%x)\n", ret);
		return TEGRABL_ERROR(TEGRABL_ERR_POWER, 0);
	}

	dp_dpcd_init(dp);

	/*dp_prepare_pad*/
	CHECK_RET(tegrabl_dp_clock_config(sor->nvdisp, sor->instance, TEGRA_SOR_SAFE_CLK));
	sor_writel_def(SOR_CLK_CNTRL, DP_CLK_SEL, DIFF_DPCLK, val);
	sor_set_link_bandwidth(sor, cfg->link_bw);
	sor_config_prod_settings(sor, dp->pdata->prod_list, dp_node, 0);
	dp_link_cal(dp);

	tegrabl_sor_enable_dp(sor);

	if (cfg->alt_scramber_reset_cap) {
		dp_set_assr(dp, true);
	} else {
		sor_set_internal_panel(sor, false);
	}

	tegrabl_dp_dpcd_write(dp, DPCD_MAIN_LINK_CHANNEL_CODING_SET,
						  DPCD_MAIN_LINK_CHANNEL_CODING_SET_ANSI_8B10B);

	val = sor_readl(sor, (SOR_NV_PDISP_SOR_DP_CONFIG0_0 + sor->portnum));
	val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_CONFIG0, IDLE_BEFORE_ATTACH, ENABLE, val);
	sor_writel(sor, (SOR_NV_PDISP_SOR_DP_CONFIG0_0 + sor->portnum), val);

	tegrabl_dp_dpcd_write(dp, DPCD_DOWNSPREAD_CTRL, DPCD_DOWNSPREAD_CTRL_SPREAD_AMP_LT_0_5);

	tegrabl_dp_dpcd_write(dp, DPCD_LINK_BANDWIDTH_SET, cfg->link_bw);

	/*
	 * enhanced framing enable field shares DPCD offset
	 * with lane count set field. Make sure lane count is set
	 * before enhanced framing enable. CTS waits on first
	 * write to this offset to check for lane count set.
	 */
	tegrabl_dp_dpcd_write_field(dp, DPCD_LANE_COUNT_SET, DPCD_LANE_COUNT_SET_MASK, cfg->lane_count);
	dp_set_enhanced_framing(dp, cfg->enhanced_framing);

	dp_tu_config(dp, cfg);

	tegrabl_sor_port_enable(sor, true);
	sor_config_xbar(sor);

	/* switch to macro feedback clock */
	CHECK_RET(tegrabl_dp_clock_config(nvdisp, sor->instance, TEGRA_SOR_LINK_CLK));

	/* Host is ready. Start link training. */
	ret = tegrabl_dp_lt(&dp->lt_data);

	if (ret != TEGRABL_NO_ERROR) {
		pr_error("dp: link training failed\n");
		pr_error("DP enable unsuccessful\n");
	} else {
		dp->enabled = true;
		pr_debug("DP enable successful\n");
	}

	return ret;
}

/* Setup clock for Nvdisp and DP */
uint64_t tegrabl_dp_setup_clk(struct tegrabl_nvdisp *nvdisp, uint32_t clk_id)
{
	/*dummy func*/
	return 0;
}

/* exported DP ops */
struct nvdisp_out_ops dp_ops = {
	.init	   = tegrabl_dp_init,
	.enable	   = tegrabl_dp_enable,
	.setup_clk = tegrabl_dp_setup_clk,
};
