/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef __TEGRABL_SOR_DP_H__
#define __TEGRABL_SOR_DP_H__

#include <tegrabl_nvdisp.h>
#include <tegrabl_sor.h>

#define NV_SOR_DP_TPG_LANE1_PATTERN_SHIFT (8)
#define NV_SOR_DP_TPG_LANE0_CHANNELCODING_SHIFT (6)
#define NV_SOR_DP_TPG_LANE0_SCRAMBLEREN_SHIFT (4)

#define SOR_LINK_SPEED_G1_62 6
#define SOR_LINK_SPEED_G2_7 10
#define SOR_LINK_SPEED_G5_4 20
#define SOR_LINK_SPEED_LVDS 7

#define NV_SOR_PR_LANE2_DP_LANE0_SHIFT (16)
#define NV_SOR_PR_LANE2_DP_LANE0_MASK (0xff << 16)
#define NV_SOR_PR_LANE1_DP_LANE1_SHIFT (8)
#define NV_SOR_PR_LANE1_DP_LANE1_MASK (0xff << 8)
#define NV_SOR_PR_LANE0_DP_LANE2_SHIFT (0)
#define NV_SOR_PR_LANE0_DP_LANE2_MASK (0xff)
#define NV_SOR_PR_LANE3_DP_LANE3_SHIFT (24)
#define NV_SOR_PR_LANE3_DP_LANE3_MASK (0xff << 24)

enum {
	TRAINING_PATTERN_DISABLE = 0,
	TRAINING_PATTERN_1 = 1,
	TRAINING_PATTERN_2 = 2,
	TRAINING_PATTERN_3 = 3,
	TRAINING_PATTERN_D102 = 4,
	TRAINING_PATTERN_SBLERRRATE = 5,
	TRAINING_PATTERN_PRBS7 = 6,
	TRAINING_PATTERN_CSTM = 7,
	TRAINING_PATTERN_HBR2_COMPLIANCE = 8,
};

static inline void tegrabl_sor_write_field(struct sor_data *sor,
	uint32_t reg, uint32_t mask, uint32_t val)
{
	uint32_t reg_val = sor_readl(sor, reg);
	reg_val &= ~mask;
	reg_val |= val;
	sor_writel(sor, reg, reg_val);
}

/**
 *  @brief Program training pattern in Sor register
 *
 *  @param sor SOR Handle
 *  @param tp training pattern
 *  @param n_lanes number of DP lanes
 */
void tegrabl_sor_tpg(struct sor_data *sor, uint32_t tp, uint32_t n_lanes);

/**
 *  @brief Enable Sor port
 *
 *  @param sor SOR Handle
 *  @param enb True/False
 */
void tegrabl_sor_port_enable(struct sor_data *sor, bool enb);

/**
 *  @brief Enabling Sor for DP by pad power up
 *
 *  @param sor SOR Handle
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_sor_enable_dp(struct sor_data *sor);

/**
 *  @brief Configures Sor Xbar (lane sequence)
 *
 *  @param sor SOR Handle
 */
void tegrabl_sor_config_xbar(struct sor_data *sor);

/**
 *  @brief Program lanes to output specific mode voltage
 *
 *  @param sor SOR Handle
 */
void tegrabl_sor_precharge_lanes(struct sor_data *sor);

/**
 *  @brief Detach Sor
 *
 *  @param sor SOR Handle
 */
void tegrabl_sor_detach(struct sor_data *sor);

#endif
