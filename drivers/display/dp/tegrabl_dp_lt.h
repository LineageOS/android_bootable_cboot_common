/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TEGRABL_DP_LT_H__
#define __TEGRABL_DP_LT_H__

#include <string.h>
#include <tegrabl_sor_dp.h>
#include <tegrabl_dp_dpcd.h>

#define CR_RETRY_LIMIT 5
#define CE_RETRY_LIMIT 5
#define LT_TIMEOUT_MS 10000
#define HPD_DROP_TIMEOUT_MS 1500

#define DRIVE_CURRENT_L0 0
#define DRIVE_CURRENT_L1 1
#define DRIVE_CURRENT_L2 2
#define DRIVE_CURRENT_L3 3

#define PRE_EMPHASIS_L0 0
#define PRE_EMPHASIS_L1 1
#define PRE_EMPHASIS_L2 2
#define PRE_EMPHASIS_L3 3

#define POST_CURSOR2_L0 0
#define POST_CURSOR2_L1 1
#define POST_CURSOR2_L2 2
#define POST_CURSOR2_L3 3

/* macro state */
#define STATE_RESET 0
#define STATE_FAST_LT 1
#define STATE_CLOCK_RECOVERY 2
#define STATE_CHANNEL_EQUALIZATION 3
#define STATE_DONE_FAIL 4
#define STATE_DONE_PASS 5
#define STATE_REDUCE_BIT_RATE 6
#define STATE_COUNT 7

/**
* @brief Link Training state machine
*/
static const char * const tegra_dp_lt_state_names[] = {
	"Reset",
	"fast link training",
	"clock recovery",
	"channel equalization",
	"link training fail/disable",
	"link training pass",
	"reduce bit rate",
};

/**
* @brief structure for DP Link config data
*/
struct tegrabl_dp_lt_data {
	struct tegrabl_dp *dp;
	int32_t shutdown;
	int32_t state;
	uint32_t tps;
	int32_t pending_evt; /* pending link training request */

	uint8_t no_aux_handshake;
	uint8_t tps3_supported;
	uint8_t aux_rd_interval;

	bool lt_config_valid;
	uint32_t drive_current[4]; /* voltage swing */
	uint32_t pre_emphasis[4]; /* post cursor1 */
	uint32_t post_cursor2[4];
	uint32_t tx_pu;
	uint32_t n_lanes;
	uint32_t link_bw;

	uint32_t cr_retry;
	uint32_t ce_retry;
};

/* CTS approved list. Do not alter. */
static const uint8_t tegrabl_dp_link_config_priority[][2] = {
	/* link bandwidth, lane count */
	{SOR_LINK_SPEED_G5_4, 4}, /* 21.6Gbps */
	{SOR_LINK_SPEED_G2_7, 4}, /* 10.8Gbps */
	{SOR_LINK_SPEED_G1_62, 4}, /* 6.48Gbps */
	{SOR_LINK_SPEED_G5_4, 2}, /* 10.8Gbps */
	{SOR_LINK_SPEED_G2_7, 2}, /* 5.4Gbps */
	{SOR_LINK_SPEED_G1_62, 2}, /* 3.24Gbps */
	{SOR_LINK_SPEED_G5_4, 1}, /* 5.4Gbps */
	{SOR_LINK_SPEED_G2_7, 1}, /* 2.7Gbps */
	{SOR_LINK_SPEED_G1_62, 1}, /* 1.62Gbps */
};

static inline int32_t tegra_dp_is_max_vs(uint32_t pe, uint32_t vs)
{
	return vs >= DRIVE_CURRENT_L3;
}

static inline int32_t tegra_dp_is_max_pe(uint32_t pe, uint32_t vs)
{
	return pe >= PRE_EMPHASIS_L3;
}

static inline int32_t tegra_dp_is_max_pc(uint32_t pc)
{
	return pc >= POST_CURSOR2_L3;
}

/**
 *  @brief Initialize DP Link Training Structure
 *
 *  @param lt_data pointer to LT data structure
 *  @param dp pointer to DP structure
 */
void tegrabl_dp_lt_init(struct tegrabl_dp_lt_data *lt_data,
						struct tegrabl_dp *dp);

/**
 *  @brief Perform DP Link Training
 *
 *  @param lt_data pointer to LT data structure
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_dp_lt(struct tegrabl_dp_lt_data *lt_data);

#endif
