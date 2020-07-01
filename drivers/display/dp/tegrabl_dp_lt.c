/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_DP

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_timer.h>
#include <tegrabl_ar_macro.h>
#include <tegrabl_dp_lt.h>
#include <tegrabl_dp.h>
#include <tegrabl_dpaux.h>
#include <tegrabl_drf.h>
#include <arsor1.h>
#include <ardpaux.h>

static tegrabl_error_t set_lt_state(struct tegrabl_dp_lt_data *lt_data,
									int32_t target_state, int32_t delay_ms);
static tegrabl_error_t set_lt_tpg(struct tegrabl_dp_lt_data *lt_data,
								  uint32_t tp);

/* Check if post-cursor2 programming is supported */
static inline bool is_pc2_supported(struct tegrabl_dp_lt_data *lt_data)
{
	struct tegrabl_dp_link_config *cfg = &lt_data->dp->link_cfg;

	return (!(lt_data->dp->pdata->pc2_disabled) && cfg->tps == TRAINING_PATTERN_3);
}

/*
 * Wait period before reading link status.
 * If dpcd addr 0xe TRAINING_AUX_RD_INTERVAL absent or zero,
 * wait for 100us for CR status and 400us for CE status.
 * Otherwise, use values as specified.
 */
static inline uint32_t wait_aux_training(struct tegrabl_dp_lt_data *lt_data,
										 bool is_clk_recovery)
{
	if (!lt_data->aux_rd_interval)
		is_clk_recovery ? tegrabl_udelay(200) : tegrabl_udelay(500);
	else
		tegrabl_mdelay(lt_data->aux_rd_interval * 4);

	return lt_data->aux_rd_interval;
}

static int32_t get_next_lower_link_config(struct tegrabl_dp *dp,
	struct tegrabl_dp_link_config *link_cfg)
{
	uint8_t cur_n_lanes = link_cfg->lane_count;
	uint8_t cur_link_bw = link_cfg->link_bw;
	uint32_t priority_index;
	uint32_t priority_arr_size = ARRAY_SIZE(tegrabl_dp_link_config_priority);

	for (priority_index = 0; priority_index < priority_arr_size;
		priority_index++) {
		if (tegrabl_dp_link_config_priority[priority_index][0] == cur_link_bw &&
			tegrabl_dp_link_config_priority[priority_index][1] == cur_n_lanes) {
			break;
		}
	}

	/* already at lowest link config */
	if (priority_index == priority_arr_size - 1) {
		return -1;
	}

	for (priority_index++;
		priority_index < priority_arr_size; priority_index++) {
		if ((tegrabl_dp_link_config_priority[priority_index][0] <=
			link_cfg->max_link_bw) &&
			(tegrabl_dp_link_config_priority[priority_index][1] <=
			link_cfg->max_lane_count)) {
			return priority_index;
		}
	}

	/* we should never end up here */
	return -1;
}

static bool get_clock_recovery_status(struct tegrabl_dp_lt_data *lt_data)
{
	uint32_t cnt;
	uint32_t n_lanes = lt_data->n_lanes;
	uint8_t data_ptr = 0;

	/* support for 1 lane */
	uint32_t loopcnt = (n_lanes == 1) ? 1 : n_lanes >> 1;

	for (cnt = 0; cnt < loopcnt; cnt++) {
		tegrabl_dp_dpcd_read(lt_data->dp, (DPCD_LANE0_1_STATUS + cnt),
							 &data_ptr);

		if (n_lanes == 1)
			return (data_ptr & 0x1) ? true : false;
		else if (!(data_ptr & 0x1) ||
			!(data_ptr & (0x1 << DPCD_STATUS_LANEXPLUS1_CR_DONE_SHIFT)))
			return false;
	}

	return true;
}

static bool get_channel_eq_status(struct tegrabl_dp_lt_data *lt_data)
{
	uint32_t cnt;
	uint32_t n_lanes = lt_data->n_lanes;
	uint8_t data = 0;
	bool ce_done = true;

	/* support for 1 lane */
	uint32_t loopcnt = (n_lanes == 1) ? 1 : n_lanes >> 1;

	for (cnt = 0; cnt < loopcnt; cnt++) {
		tegrabl_dp_dpcd_read(lt_data->dp, (DPCD_LANE0_1_STATUS + cnt), &data);

		if (n_lanes == 1) {
			ce_done = (data & (0x1 << DPCD_STATUS_LANEX_CHN_EQ_DONE_SHIFT)) &&
						(data & (0x1 << DPCD_STATUS_LANEX_SYMBOL_LOCKED_SHIFT));
			break;
		} else if (!(data & (0x1 << DPCD_STATUS_LANEX_CHN_EQ_DONE_SHIFT)) ||
			!(data & (0x1 << DPCD_STATUS_LANEX_SYMBOL_LOCKED_SHIFT)) ||
			!(data & (0x1 << DPCD_STATUS_LANEXPLUS1_CHN_EQ_DONE_SHIFT)) ||
			!(data & (0x1 << DPCD_STATUS_LANEXPLUS1_SYMBOL_LOCKED_SHIFT))) {
			ce_done = false;
			break;
		}
	}

	if (ce_done) {
		tegrabl_dp_dpcd_read(lt_data->dp, DPCD_LANE_ALIGN_STATUS_UPDATED,
							 &data);
		if (!(data & DPCD_LANE_ALIGN_STATUS_INTERLANE_ALIGN_DONE_YES))
			ce_done = false;
	}

	return ce_done;
}

static bool get_lt_status(struct tegrabl_dp_lt_data *lt_data)
{
	bool cr_done, ce_done;

	cr_done = get_clock_recovery_status(lt_data);
	pr_debug("%s: cr_done %d\n", __func__, cr_done);
	if (!cr_done)
		return false;

	ce_done = get_channel_eq_status(lt_data);
	pr_debug("%s: ce_done %d\n", __func__, ce_done);

	return ce_done;
}

/*
 * get updated voltage swing, pre-emphasis and
 * post-cursor2 settings from panel
 */
static void get_lt_new_config(struct tegrabl_dp_lt_data *lt_data)
{
	uint32_t cnt;
	static uint8_t data_ptr;
	uint32_t n_lanes = lt_data->n_lanes;
	uint32_t *vs = lt_data->drive_current;
	uint32_t *pe = lt_data->pre_emphasis;
	uint32_t *pc = lt_data->post_cursor2;
	bool pc_supported = is_pc2_supported(lt_data);

	/* support for 1 lane */
	uint32_t loopcnt = (n_lanes == 1) ? 1 : n_lanes >> 1;

	for (cnt = 0; cnt < loopcnt; cnt++) {
		tegrabl_dp_dpcd_read(lt_data->dp, (DPCD_LANE0_1_ADJUST_REQ + cnt),
							 &data_ptr);

		pe[2 * cnt] = (data_ptr & DPCD_ADJUST_REQ_LANEX_PE_MASK) >>
						DPCD_ADJUST_REQ_LANEX_PE_SHIFT;
		vs[2 * cnt] = (data_ptr & DPCD_ADJUST_REQ_LANEX_DC_MASK) >>
						DPCD_ADJUST_REQ_LANEX_DC_SHIFT;
		pe[1 + 2 * cnt] = (data_ptr & DPCD_ADJUST_REQ_LANEXPLUS1_PE_MASK) >>
							DPCD_ADJUST_REQ_LANEXPLUS1_PE_SHIFT;
		vs[1 + 2 * cnt] = (data_ptr & DPCD_ADJUST_REQ_LANEXPLUS1_DC_MASK) >>
							DPCD_ADJUST_REQ_LANEXPLUS1_DC_SHIFT;
	}

	if (pc_supported) {
		tegrabl_dp_dpcd_read(lt_data->dp, DPCD_ADJUST_REQ_POST_CURSOR2,
							 &data_ptr);
		for (cnt = 0; cnt < n_lanes; cnt++) {
			pc[cnt] = (data_ptr >>
						DPCD_ADJUST_REQ_POST_CURSOR2_LANE_SHIFT(cnt)) &
						DPCD_ADJUST_REQ_POST_CURSOR2_LANE_MASK;
		}
	}

	for (cnt = 0; cnt < n_lanes; cnt++) {
		pr_debug("%s: lane %d: vs level: %d, pe level: %d, pc2 level: %d\n",
				 __func__, cnt, vs[cnt], pe[cnt], pc_supported ? pc[cnt] : 0);
	}
}

static void set_tx_pu(struct tegrabl_dp_lt_data *lt_data)
{
	struct tegrabl_dp *dp = lt_data->dp;
	struct sor_data *sor = dp->sor;
	int32_t n_lanes = lt_data->n_lanes;
	int32_t cnt = 1;
	uint32_t *vs = lt_data->drive_current;
	uint32_t *pe = lt_data->pre_emphasis;
	uint32_t *pc = lt_data->post_cursor2;
	uint32_t max_tx_pu;
	uint32_t val;

	pr_debug("%s: entry\n", __func__);

	if (!dp->pdata) {
		val = sor_readl(dp->sor, SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum);
		val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, TX_PU, DISABLE,
								 val);
		sor_writel(dp->sor, SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum, val);
		lt_data->tx_pu = 0;
		return;
	}

	max_tx_pu = dp->pdata->lt_data[DP_TX_PU].data[pc[0]][vs[0]][pe[0]];
	for (; cnt < n_lanes; cnt++) {
		val = dp->pdata->lt_data[DP_TX_PU].data[pc[cnt]][vs[cnt]][pe[cnt]];
		max_tx_pu = (max_tx_pu < val) ? val	: max_tx_pu;
	}

	lt_data->tx_pu = max_tx_pu;
	val = sor_readl(dp->sor, SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum);
	val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_DP_PADCTL0, TX_PU_VALUE,
							 max_tx_pu, val);
	val = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, SOR_DP_PADCTL0, TX_PU, ENABLE,
							 val);
	sor_writel(dp->sor, SOR_NV_PDISP_SOR_DP_PADCTL0_0 + sor->portnum, val);
}

/*
 * configure voltage swing, pre-emphasis,
 * post-cursor2 and tx_pu on host and sink
 */
static void set_lt_config(struct tegrabl_dp_lt_data *lt_data)
{
	struct tegrabl_dp *dp = lt_data->dp;
	struct sor_data *sor = dp->sor;
	int32_t n_lanes = lt_data->n_lanes;
	bool pc_supported = is_pc2_supported(lt_data);
	int32_t i, cnt;
	uint32_t val;
	uint32_t *vs = lt_data->drive_current;
	uint32_t *pe = lt_data->pre_emphasis;
	uint32_t *pc = lt_data->post_cursor2;
	uint32_t aux_stat = 0;
	uint8_t training_lanex_set[4] = {0, 0, 0, 0};
	uint32_t training_lanex_set_size = sizeof(training_lanex_set);

	/* support for 1 lane */
	int32_t loopcnt = (n_lanes == 1) ? 1 : n_lanes >> 1;

	/*
	 * apply voltage swing, preemphasis, postcursor2 and tx_pu
	 * prod settings to each lane based on levels
	 */
	for (i = 0; i < n_lanes; i++) {
		uint32_t pe_reg, vs_reg, pc_reg;
		uint32_t pe_val, vs_val, pc_val = 0;
		cnt = sor->xbar_ctrl[i];

		vs_reg = dp->pdata->lt_data[DP_VS].data[pc[i]][vs[i]][pe[i]];
		pe_reg = dp->pdata->lt_data[DP_PE].data[pc[i]][vs[i]][pe[i]];
		pc_reg = dp->pdata->lt_data[DP_PC].data[pc[i]][vs[i]][pe[i]];

		pe_val = sor_readl(sor,
					SOR_NV_PDISP_SOR_LANE_PREEMPHASIS0_0 + sor->portnum);
		vs_val = sor_readl(sor,
					SOR_NV_PDISP_SOR_LANE_DRIVE_CURRENT0_0 + sor->portnum);
		if (pc_supported) {
			pc_val = sor_readl(sor,
						SOR_NV_PDISP_SOR_POSTCURSOR0_0 + sor->portnum);
		}

		switch (cnt) {
		case 0:
			pe_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_LANE_PREEMPHASIS0,
										LANE2_DP_LANE0, pe_reg, pe_val);
			vs_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_LANE_DRIVE_CURRENT0,
										LANE2_DP_LANE0, vs_reg, vs_val);
			if (pc_supported) {
				pc_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_POSTCURSOR0,
											LANE2_DP_LANE0, pc_reg, pc_val);
			}
			break;
		case 1:
			pe_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_LANE_PREEMPHASIS0,
										LANE1_DP_LANE1, pe_reg, pe_val);
			vs_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_LANE_DRIVE_CURRENT0,
										LANE1_DP_LANE1, vs_reg, vs_val);
			if (pc_supported) {
				pc_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_POSTCURSOR0,
											LANE1_DP_LANE1, pc_reg, pc_val);
			}
			break;
		case 2:
			pe_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_LANE_PREEMPHASIS0,
										LANE0_DP_LANE2, pe_reg, pe_val);
			vs_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_LANE_DRIVE_CURRENT0,
										LANE0_DP_LANE2, vs_reg, vs_val);
			if (pc_supported) {
				pc_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_POSTCURSOR0,
											LANE0_DP_LANE2, pc_reg, pc_val);
			}
			break;
		case 3:
			pe_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_LANE_PREEMPHASIS0,
										LANE3_DP_LANE3, pe_reg, pe_val);
			vs_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_LANE_DRIVE_CURRENT0,
										LANE3_DP_LANE3, vs_reg, vs_val);
			if (pc_supported) {
				pc_val = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, SOR_POSTCURSOR0,
											LANE3_DP_LANE3, pc_reg, pc_val);
			}
			break;
		default:
			pr_error("dp: incorrect lane cnt\n");
		}


		sor_writel(sor, SOR_NV_PDISP_SOR_LANE_PREEMPHASIS0_0 + sor->portnum, pe_val);
		sor_writel(sor, SOR_NV_PDISP_SOR_LANE_DRIVE_CURRENT0_0 + sor->portnum, vs_val);
		if (pc_supported) {
			sor_writel(sor, SOR_NV_PDISP_SOR_POSTCURSOR0_0 + sor->portnum, pc_val);
		}

		pr_debug("%s: lane %d: vs level: %d, pe level: %d, pc2 level: %d\n",
				 __func__, i, vs[i], pe[i], pc_supported ? pc[i] : 0);
	}
	set_tx_pu(lt_data);
	tegrabl_udelay(20); /* HW stabilization delay */

	/* apply voltage swing and preemphasis levels to panel for each lane */
	for (cnt = n_lanes - 1; cnt >= 0; cnt--) {
		uint32_t max_vs_flag = tegra_dp_is_max_vs(pe[cnt], vs[cnt]);
		uint32_t max_pe_flag = tegra_dp_is_max_pe(pe[cnt], vs[cnt]);

		val = (vs[cnt] << DPCD_TRAINING_LANEX_SET_DC_SHIFT) |
			(max_vs_flag ? DPCD_TRAINING_LANEX_SET_DC_MAX_REACHED_T :
				DPCD_TRAINING_LANEX_SET_DC_MAX_REACHED_F) |
			(pe[cnt] << DPCD_TRAINING_LANEX_SET_PE_SHIFT) |
			(max_pe_flag ? DPCD_TRAINING_LANEX_SET_PE_MAX_REACHED_T :
				DPCD_TRAINING_LANEX_SET_PE_MAX_REACHED_F);

		training_lanex_set[cnt] = val;
	}
	tegrabl_dpaux_write(dp->hdpaux, AUX_CMD_AUXWR, DPCD_TRAINING_LANE0_SET,
						training_lanex_set, &training_lanex_set_size,
						&aux_stat);

	/* apply postcursor2 levels to panel for each lane */
	if (pc_supported) {
		for (cnt = 0; cnt < loopcnt; cnt++) {
			uint32_t max_pc_flag0 = tegra_dp_is_max_pc(pc[cnt]);
			uint32_t max_pc_flag1 = tegra_dp_is_max_pc(pc[cnt + 1]);

			val = (pc[cnt] << DPCD_LANEX_SET2_PC2_SHIFT) |
				(max_pc_flag0 ? DPCD_LANEX_SET2_PC2_MAX_REACHED_T :
					DPCD_LANEX_SET2_PC2_MAX_REACHED_F) |
				(pc[cnt + 1] << DPCD_LANEXPLUS1_SET2_PC2_SHIFT) |
				(max_pc_flag1 ? DPCD_LANEXPLUS1_SET2_PC2_MAX_REACHED_T :
					DPCD_LANEXPLUS1_SET2_PC2_MAX_REACHED_F);
			tegrabl_dp_dpcd_write(dp, (DPCD_TRAINING_LANE0_1_SET2 + cnt), val);
		}
	}
}

static void lt_data_sw_reset(struct tegrabl_dp_lt_data *lt_data)
{
	struct tegrabl_dp *dp =  lt_data->dp;

	lt_data->lt_config_valid = false;
	lt_data->cr_retry = 0;
	lt_data->ce_retry = 0;
	lt_data->tx_pu = 0;
	lt_data->n_lanes = dp->link_cfg.lane_count;
	lt_data->link_bw = dp->link_cfg.link_bw;
	lt_data->no_aux_handshake = dp->link_cfg.support_fast_lt;
	lt_data->aux_rd_interval = dp->link_cfg.aux_rd_interval;

	memset(lt_data->pre_emphasis, PRE_EMPHASIS_L0,
		   sizeof(lt_data->pre_emphasis));
	memset(lt_data->drive_current, DRIVE_CURRENT_L0,
		   sizeof(lt_data->drive_current));
	memset(lt_data->post_cursor2, POST_CURSOR2_L0,
		   sizeof(lt_data->post_cursor2));
}

static tegrabl_error_t lt_data_reset(struct tegrabl_dp_lt_data *lt_data)
{
	struct tegrabl_dp *dp =  lt_data->dp;
	bool hpd_status;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	lt_data_sw_reset(lt_data);

	CHECK_RET(tegrabl_dpaux_hpd_status(dp->hdpaux, &hpd_status));
	/* reset LT data on controller and panel only if hpd is asserted */
	if (hpd_status) {
		/*
		 * Training pattern is disabled here. Do not HW reset
		 * lt config i.e. vs, pe, pc2. CTS mandates modifying these
		 * only when training pattern is enabled.
		 */
		tegrabl_dp_update_link_config(dp);
	}

	return ret;
}

static tegrabl_error_t set_lt_tpg(struct tegrabl_dp_lt_data *lt_data,
								  uint32_t tp)
{
	struct tegrabl_dp *dp = lt_data->dp;
	bool hpd_status;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	CHECK_RET(tegrabl_dpaux_hpd_status(dp->hdpaux, &hpd_status));

	if (lt_data->tps == tp) {
		return ret;
	}

	if (hpd_status) {
		tegrabl_dp_tpg(dp, tp, lt_data->n_lanes);
	} else {
		/*
		 * hpd deasserted. Just set training sequence from host side and exit
		 */
		tegrabl_sor_tpg(dp->sor, tp, lt_data->n_lanes);
	}
	lt_data->tps = tp;

	return ret;
}

static tegrabl_error_t lt_failed(struct tegrabl_dp_lt_data *lt_data)
{
	struct tegrabl_dp *dp = lt_data->dp;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	tegrabl_sor_detach(dp->sor);
	CHECK_RET(set_lt_tpg(lt_data, TRAINING_PATTERN_DISABLE));
	CHECK_RET(lt_data_reset(lt_data));

	return ret;
}

static tegrabl_error_t lt_passed(struct tegrabl_dp_lt_data *lt_data)
{
	struct tegrabl_dp *dp = lt_data->dp;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	pr_debug("%s: ENTER\n", __func__);

	lt_data->lt_config_valid = true;
	CHECK_RET(set_lt_tpg(lt_data, TRAINING_PATTERN_DISABLE));
	sor_attach(dp->sor);

	pr_debug("%s: EXIT\n", __func__);
	return ret;
}

static tegrabl_error_t lt_reset_state(struct tegrabl_dp_lt_data *lt_data)
{
	struct tegrabl_dp *dp;
	struct sor_data *sor;
	bool hpd_status;
	int32_t tgt_state;
	int32_t timeout;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	pr_debug("%s: ENTER\n", __func__);

	dp = lt_data->dp;
	sor = dp->sor;

	CHECK_RET(tegrabl_dpaux_hpd_status(dp->hdpaux, &hpd_status));

	if (!hpd_status || !dp->link_cfg.is_valid) {
		pr_debug("dp lt: cur_hpd: %d, link cfg valid: %d\n", !!hpd_status,
				 !!dp->link_cfg.is_valid);
		lt_failed(lt_data);
		tgt_state = STATE_DONE_FAIL;
		timeout = 0;
		goto done;
	}

	if (lt_data->lt_config_valid &&	get_lt_status(lt_data)) {
		pr_debug("dp_lt: link stable, do nothing\n");
		lt_passed(lt_data);
		tgt_state = STATE_DONE_PASS;
		timeout = 0;
		goto done;
	}

	/*
	 * detach SOR early.
	 * DP lane count can be changed only
	 * when SOR is asleep.
	 * DP link bandwidth can be changed only
	 * when SOR is in safe mode.
	 */
	tegrabl_sor_detach(sor);

	lt_data_reset(lt_data);

	tgt_state = STATE_CLOCK_RECOVERY;
	timeout = 0;

	/*
	 * pre-charge main link for at
	 * least 10us before initiating
	 * link training
	 */
	tegrabl_sor_precharge_lanes(sor);
done:
	return set_lt_state(lt_data, tgt_state, timeout);
}

static tegrabl_error_t fast_lt_state(struct tegrabl_dp_lt_data *lt_data)
{
	/*dummy func*/
	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
}

static tegrabl_error_t lt_reduce_bit_rate_state(
	struct tegrabl_dp_lt_data *lt_data)
{
	struct tegrabl_dp *dp = lt_data->dp;
	struct tegrabl_dp_link_config tmp_cfg;
	int32_t next_link_index;
	bool cur_hpd;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	CHECK_RET(tegrabl_dpaux_hpd_status(dp->hdpaux, &cur_hpd));

	if (!cur_hpd) {
		pr_info("lt: hpd deasserted, wait for sometime, then reset\n");

		lt_failed(lt_data);
		return set_lt_state(lt_data, STATE_RESET, HPD_DROP_TIMEOUT_MS);
	}

	dp->link_cfg.is_valid = false;
	tmp_cfg = dp->link_cfg;

	next_link_index = get_next_lower_link_config(dp, &tmp_cfg);
	if (next_link_index < 0) {
		goto fail;
	}

	tmp_cfg.link_bw = tegrabl_dp_link_config_priority[next_link_index][0];
	tmp_cfg.lane_count = tegrabl_dp_link_config_priority[next_link_index][1];

	if (!tegrabl_dp_calc_config(dp, dp->mode, &tmp_cfg)) {
		goto fail;
	}

	tmp_cfg.is_valid = true;
	dp->link_cfg = tmp_cfg;

	tegrabl_dp_update_link_config(dp);

	lt_data->n_lanes = tmp_cfg.lane_count;
	lt_data->link_bw = tmp_cfg.link_bw;

	pr_debug("dp lt: retry CR, lanes: %d, link_bw: 0x%x\n",
			lt_data->n_lanes, lt_data->link_bw);

	return set_lt_state(lt_data, STATE_CLOCK_RECOVERY, 0);

fail:
	pr_debug("dp lt: bit rate already lowest\n");
	lt_failed(lt_data);
	return set_lt_state(lt_data, STATE_DONE_FAIL, -1);
}

static tegrabl_error_t lt_channel_equalization_state(
	struct tegrabl_dp_lt_data *lt_data)
{
	int32_t tgt_state;
	int32_t timeout;
	bool cr_done = true;
	bool ce_done = true;
	bool cur_hpd;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	CHECK_RET(tegrabl_dpaux_hpd_status(lt_data->dp->hdpaux, &cur_hpd));

	if (!cur_hpd) {
		pr_info("lt: hpd deasserted, wait for sometime, then reset\n");

		lt_failed(lt_data);
		tgt_state = STATE_RESET;
		timeout = HPD_DROP_TIMEOUT_MS;
		goto done;
	}

	CHECK_RET(set_lt_tpg(lt_data, lt_data->dp->link_cfg.tps));

	wait_aux_training(lt_data, false);

	cr_done = get_clock_recovery_status(lt_data);
	if (!cr_done) {
		/*
		 * No HW reset here. CTS waits on write to
		 * reduced(where applicable) link BW dpcd offset.
		 */
		lt_data_sw_reset(lt_data);
		tgt_state = STATE_REDUCE_BIT_RATE;
		timeout = 0;
		pr_info("dp lt: CR lost\n");
		goto done;
	}

	ce_done = get_channel_eq_status(lt_data);
	if (ce_done) {
		lt_passed(lt_data);
		tgt_state = STATE_DONE_PASS;
		timeout = -1;
		pr_info("dp lt: CE done\n");
		goto done;
	}
	pr_debug("dp lt: CE not done\n");

	if (++(lt_data->ce_retry) > (CE_RETRY_LIMIT + 1)) {
		pr_debug("dp lt: CE retry limit %d reached\n", lt_data->ce_retry - 2);
		/*
		 * Just do LT SW reset here. CTS mandates that
		 * LT config should be reduced only after
		 * training pattern 1 is set. Reduce bitrate
		 * state would update new lane count and
		 * bandwidth. Proceeding clock recovery/fail/reset
		 * state would reset voltage swing, pre-emphasis
		 * and post-cursor2 after setting tps 1/0.
		 */
		lt_data_sw_reset(lt_data);
		tgt_state = STATE_REDUCE_BIT_RATE;
		timeout = 0;
		goto done;
	}

	get_lt_new_config(lt_data);
	set_lt_config(lt_data);

	tgt_state = STATE_CHANNEL_EQUALIZATION;
	timeout = 0;
	pr_debug("dp lt: CE retry\n");
done:
	return set_lt_state(lt_data, tgt_state, timeout);
}

static inline bool is_vs_already_max(struct tegrabl_dp_lt_data *lt_data,
									 uint32_t old_vs[4], uint32_t new_vs[4])
{
	uint32_t n_lanes = lt_data->n_lanes;
	uint32_t cnt;

	for (cnt = 0; cnt < n_lanes; cnt++) {
		if (old_vs[cnt] == DRIVE_CURRENT_L3 &&
			new_vs[cnt] == DRIVE_CURRENT_L3) {
			continue;
		}

		return false;
	}

	return true;
}

static tegrabl_error_t lt_clock_recovery_state(
	struct tegrabl_dp_lt_data *lt_data)
{
	struct tegrabl_dp *dp;
	int32_t tgt_state;
	int32_t timeout;
	uint32_t *vs = lt_data->drive_current;
	bool cr_done;
	uint32_t vs_temp[4];
	bool cur_hpd;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	dp = lt_data->dp;

	CHECK_RET(tegrabl_dpaux_hpd_status(dp->hdpaux, &cur_hpd));

	if (!cur_hpd) {
		pr_info("lt: hpd deasserted, wait for sometime, then reset\n");

		lt_failed(lt_data);
		tgt_state = STATE_RESET;
		timeout = HPD_DROP_TIMEOUT_MS;
		goto done;
	}

	CHECK_RET(set_lt_tpg(lt_data, TRAINING_PATTERN_1));

	set_lt_config(lt_data);
	wait_aux_training(lt_data, true);
	cr_done = get_clock_recovery_status(lt_data);
	if (cr_done) {
		lt_data->cr_retry = 0;
		tgt_state = STATE_CHANNEL_EQUALIZATION;
		timeout = 0;
		pr_info("dp lt: CR done\n");
		goto done;
	}
	pr_debug("dp lt: CR not done\n");

	memcpy(vs_temp, vs, sizeof(vs_temp));
	get_lt_new_config(lt_data);

	if (!memcmp(vs_temp, vs, sizeof(vs_temp))) {
		/*
		 * Reduce bit rate if voltage swing already max or
		 * CR retry limit of 5 reached.
		 */
		if (is_vs_already_max(lt_data, vs_temp, vs) ||
			(lt_data->cr_retry)++ >= (CR_RETRY_LIMIT - 1)) {
			pr_debug("dp lt: CR retry limit %d %s reached\n", lt_data->cr_retry,
					is_vs_already_max(lt_data, vs_temp, vs) ?
					"for max vs" : "");
			/*
			 * Just do LT SW reset here. CTS mandates that
			 * LT config should be reduced only after
			 * bit rate has been reduced. Reduce bitrate
			 * state would update new lane count and
			 * bandwidth. Proceeding clock recovery state
			 * would reset voltage swing, pre-emphasis
			 * and post-cursor2.
			 */
			lt_data_sw_reset(lt_data);
			tgt_state = STATE_REDUCE_BIT_RATE;
			timeout = 0;
			goto done;
		}
	} else {
		lt_data->cr_retry = 1;
	}

	tgt_state = STATE_CLOCK_RECOVERY;
	timeout = 0;
	pr_debug("dp lt: CR retry\n");
done:
	return set_lt_state(lt_data, tgt_state, timeout);
}

typedef tegrabl_error_t (*dispatch_func_t)(struct tegrabl_dp_lt_data *lt_data);
static const dispatch_func_t state_machine_dispatch[] = {
	lt_reset_state,			/* STATE_RESET */
	fast_lt_state,			/* STATE_FAST_LT */
	lt_clock_recovery_state,	/* STATE_CLOCK_RECOVERY */
	lt_channel_equalization_state,	/* STATE_CHANNEL_EQUALIZATION */
	NULL,				/* STATE_DONE_FAIL */
	NULL,				/* STATE_DONE_PASS */
	lt_reduce_bit_rate_state,	/* STATE_REDUCE_BIT_RATE */
};

static tegrabl_error_t perform_lt(struct tegrabl_dp_lt_data *lt_data)
{
	int32_t pending_lt_evt;
	bool cur_hpd;
	bool done = false;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	/*
	 * Observe and clear pending flag
	 * and latch the current HPD state.
	 */
	while (!done) {
		pending_lt_evt = lt_data->pending_evt;
		lt_data->pending_evt = 0;

		CHECK_RET(tegrabl_dpaux_hpd_status(lt_data->dp->hdpaux, &cur_hpd));

		pr_debug("dp lt: state %d (%s), hpd %d, pending_lt_evt %d\n",
				 lt_data->state, tegra_dp_lt_state_names[lt_data->state],
				 cur_hpd, pending_lt_evt);

		if (!cur_hpd) {
			return TEGRABL_ERR_INIT_FAILED;
		}

		if (pending_lt_evt) {
			ret = set_lt_state(lt_data, STATE_RESET, 0);
		} else if (lt_data->state < (int32_t)ARRAY_SIZE(state_machine_dispatch)) {
			dispatch_func_t func = state_machine_dispatch[lt_data->state];

			if (!func) {
				pr_warn("dp lt: NULL state handler in state %d\n", lt_data->state);
				ret = TEGRABL_ERR_INVALID;
			} else {
				pr_debug("dp lt: handler in state %d\n", lt_data->state);
				ret = func(lt_data);
			}
		} else {
			pr_warn("dp lt: unexpected state scheduled %d", lt_data->state);
			ret = TEGRABL_ERROR(TEGRABL_ERR_INVALID_STATE, 1);
		}

		switch (lt_data->state) {
		case STATE_DONE_PASS:
			if (!get_lt_status(lt_data)) {
				pr_debug("dp_lt: sink reset the link!\n");
				ret = set_lt_state(lt_data, STATE_RESET, 0);
			} else {
				done = true;
				pr_debug("dp_lt: Link Training complete PASS!\n");
			}
			break;
		case STATE_DONE_FAIL:
			pr_debug("dp_lt: Link Training complete FAIL!\n");
			ret = TEGRABL_ERROR(TEGRABL_ERR_INIT_FAILED, 0);
			break;
		default:
			break;
		}
		if (ret != TEGRABL_NO_ERROR) {
			done = true;
			pr_debug("dp_lt link training intermediate state error!\n");
		}
	}
	return ret;
}

static tegrabl_error_t set_lt_state(struct tegrabl_dp_lt_data *lt_data,
									int32_t target_state, int32_t delay_ms)
{
	pr_debug("dp lt: switching from state %d (%s) to state %d (%s)\n",
			lt_data->state, tegra_dp_lt_state_names[lt_data->state],
			target_state, tegra_dp_lt_state_names[target_state]);

	lt_data->state = target_state;

	/* we have reached final state. notify others. */
	if (target_state == STATE_DONE_PASS)
		return TEGRABL_NO_ERROR;
	else if (target_state == STATE_DONE_FAIL)
		return TEGRABL_ERR_INVALID_STATE;

	/*
	 * If the pending_hpd_evt flag is already set, don't bother to
	 * reschedule the state machine worker. We should be able to assert
	 * that there is a worker callback already scheduled, and that it is
	 * scheduled to run immediately
	 */
	if (!lt_data->pending_evt)
		return TEGRABL_NO_ERROR;

	return TEGRABL_ERROR(TEGRABL_ERR_INVALID_STATE, 0);
}

tegrabl_error_t tegrabl_dp_lt(struct tegrabl_dp_lt_data *lt_data)
{
	lt_data->pending_evt = 1;

	return perform_lt(lt_data);
}

void tegrabl_dp_lt_init(struct tegrabl_dp_lt_data *lt_data,
						struct tegrabl_dp *dp)
{
	lt_data->dp = dp;
	lt_data->state = STATE_RESET;
	lt_data->pending_evt = 0;
	lt_data->shutdown = 0;

	lt_data_sw_reset(lt_data);
}
