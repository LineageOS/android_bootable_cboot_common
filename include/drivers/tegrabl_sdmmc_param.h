/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_SDMMC_PARAM_H
#define INCLUDED_TEGRABL_SDMMC_PARAM_H

/**
* @brief structure for sdmmc platform parameters
*
* clk_src		Clock source for sdmmc controller
*				Possible clock sources are
*				PLLP_OUT0,
*				PLLC_OUT0,
*				CLK_M
* clk_freq		Clock source frequency
* best_mode		Best mode to be configured.
*				HS400,
*				HS200,
*				DDR52
* tap_value		Tap value to set in vendor specific register
* trim_value	Trim value to set in vendor specific register
* pd_offset		Pull down offset value for auto calib
* pu_offset		Pull up offset value for auto calib
* dqs_trim_hs400		dqs trim hs400
* enable_strobe_hs400	Boolean flag to determine whether to enable strobe for hs400, if the device supports
*				true = enabel strobe
*				false = don't enable strobe
* is_skip_init	Boolean flag to determine whether to do full init or skip init
*				true = skip init
*				flase = full init
*/
struct tegrabl_sdmmc_platform_params {
	uint32_t clk_src;
	uint32_t clk_freq;
	uint32_t best_mode;
	uint32_t tap_value;
	uint32_t trim_value;
	uint32_t pd_offset;
	uint32_t pu_offset;
	bool dqs_trim_hs400;
	bool enable_strobe_hs400;
	bool is_skip_init;
};

#endif /* INCLUDED_TEGRABL_SDMMC_PARAM_H */
