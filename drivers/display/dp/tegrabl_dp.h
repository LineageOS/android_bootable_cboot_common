/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TEGRABL_DP_H__
#define __TEGRABL_DP_H__

#include <tegrabl_sor.h>
#include <tegrabl_sor_dp.h>
#include <tegrabl_dp_lt.h>
#include <tegrabl_display_dtb.h>

#define DP_DPCD_SINK_CAP_SIZE (0xc)

#define do_div(n, base) ({			  \
	uint32_t __base = (base);		  \
	uint32_t __rem;					  \
	__rem = ((uint64_t)(n)) % __base; \
	(n) = ((uint64_t)(n)) / __base;	  \
	__rem;							  \
})

#define CHECK_RET(x)				 \
	do {							 \
		ret = (x);					 \
		if (ret != TEGRABL_NO_ERROR) \
			return ret;				 \
	} while (0)

/* macro DP */
#define DP_VS 0
#define DP_PE 1
#define DP_PC 2
#define DP_TX_PU 3

/**
* @brief structure for DP configuration
*/
struct tegrabl_dp {
	int32_t sor_instance;
	struct tegrabl_dpaux *hdpaux;

	struct sor_data *sor;
	struct tegrabl_nvdisp *nvdisp;
	struct nvdisp_mode *mode;

	struct tegrabl_dp_link_config link_cfg;
	struct tegrabl_dp_link_config max_link_cfg;

	bool enabled;
	uint8_t revision;
	uint8_t edid_src;

	struct tegrabl_display_dp_dtb *pdata;
	struct tegrabl_dp_lt_data lt_data;

	uint8_t sink_cap[DP_DPCD_SINK_CAP_SIZE];
	bool sink_cap_valid;
	uint8_t sink_cnt_cp_ready;
};

/**
 *  @brief DP Clock Configuration
 *
 *  @param nvdisp pointer to nvdisp structure for nvdisp_mode
 *  @param instance sor instance
 *  @param clk_type sor clock type (safe or link clock)
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_dp_clock_config(struct tegrabl_nvdisp *nvdisp,
										int32_t instance, uint32_t clk_type);

/**
 *  @brief DPCD Read function
 *
 *  @param dp pointer to DP structure containing dpaux handle
 *  @param addr address to be read
 *  @param data_ptr data value read from DPCD
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_dp_dpcd_read(struct tegrabl_dp *dp, uint32_t addr,
									 uint8_t *data_ptr);

/**
 *  @brief DPCD Write function
 *
 *  @param dp pointer to DP structure containing dpaux handle
 *  @param addr address to be written
 *  @param data_ptr data value written to DPCD
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_dp_dpcd_write(struct tegrabl_dp *dp, uint32_t addr,
									  uint8_t data);

/**
 *  @brief DP training pattern
 *
 *  @param dp pointer to DP structure
 *  @param tp training pattern
 *  @param n_lanes number of DP lanes
 */
void tegrabl_dp_tpg(struct tegrabl_dp *dp, uint32_t tp, uint32_t n_lanes);

/**
 *  @brief Program prod settings for DP in Sor register
 *
 *  @param sor SOR Handle
 *  @param prod_list pointer to prod_list to be programmed
 *  @param node pointer to prod pair containing specific prod_lists
 *  @param clk tells which prod_list to be programmed from prod_pair
 */
void tegrabl_dp_prod_settings(struct sor_data *sor, struct prod_list *prod_list,
	struct prod_pair *node, uint32_t clk);

/**
 *  @brief DP config calculation
 *
 *  @param dp pointer to DP structure containing LT_data handle
 *  @param mode pointer to nvdisp mode structure (timing parameters)
 *  @param cfg pointer to Link config structure
 *
 * @return TURE if success, FALSE if fails.
 */
bool tegrabl_dp_calc_config(struct tegrabl_dp *dp,
	const struct nvdisp_mode *mode, struct tegrabl_dp_link_config *cfg);

/**
 *  @brief update Link configuration
 *
 *  @param dp pointer to DP structure containing LT_data handle
 */
void tegrabl_dp_update_link_config(struct tegrabl_dp *dp);

#endif
