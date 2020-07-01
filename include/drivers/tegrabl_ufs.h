/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_UFS_H
#define INCLUDED_TEGRABL_UFS_H

#define UFS_NO_HS_GEAR	0
#define UFS_HS_GEAR_1	1
#define UFS_HS_GEAR_2	2
#define UFS_HS_GEAR_3	3

#define UFS_HS_RATE_A	1
#define UFS_HS_RATE_B	2

#define UFS_PWM_GEAR_1	1
#define UFS_PWM_GEAR_2	2
#define UFS_PWM_GEAR_3	3
#define UFS_PWM_GEAR_4	4

#define UFS_ONE_LANE_ACTIVE	1
#define UFS_TWO_LANES_ACTIVE	2

#define UFS_DEFAULT_PAGE_ALIGN_SIZE	4096

/** Macros to convert endianness
 */
#define BYTE_SWAP32(a)   \
		((((a) & 0xffUL) << 24) | (((a) & 0xff00UL) << 8) | \
		(((a) & 0xff0000UL) >> 8) | (((a) & 0xff000000UL) >> 24))
#define BYTE_SWAP16(a)   \
		((((a) & 0xffU) << 8) | (((a) & 0xff00U) >> 8))

/*
 * @brief struct tegrabl_ufs_platform_params - UFS platform specific structure
 * containing platform specific properties required for UFS device and
 * controller configuration.
 *
 * @max_hs_mode: Select the HS mode to be set when UFS HS modes are enabled.
 *			1 = HS_G1,
 *			2 = HS_G2,
 *			3 = HS_G3,
 * @max_pwm_mode: Select the PWM mode to be set when UFS device is configured
 * 			in PWM mode.
 * 			1 = PWM_G1,
 *			2 = PWM_G2,
 *			3 = PWM_G3,
 *			4 = PWM_G4
 * @max_active_lanes: Select the number of active UFS lanes to be used for
 *			data transfers.
 *			1 = 1 active lane,
 *			2 = 2 active lanes,
 * @page_align_size: Select the page alignment size to be used for UFS data
 *			structures. Should be a multiple of 16 bytes.
 * @enable_hs_modes: Select whether UFS HS modes should be enabled.
 *			true = Enable HS mode,
 *			false = Disable HS mode,
 * @enable_fast_auto_mode: Select whether UFS Fast Auto mode should be enabled.
 *			true = Enable Fast Auto mode,
 *			false = Disable Fast Auto mode,
 * @enable_hs_rate_b: Select whether UFS HS Rate B mode should be enabled.
 *			true = Enable HS Rate B mode,
 *			false = Disable HS Rate B mode,
 * @enable_hs_rate_a: Select whether UFS HS Rate A mode should be enabled.
 *			true = HS Rate A mode enabled,
 *			false = HS Rate A mode disabled,
 * @ufs_init_done: Boolean flag to determine whether UFS initialization is
 * 			done. This is used to determine if UFS partial init
 *			is required or init can be skipped and bootrom
 * 			configuration can be used.
 * 			true = UFS initialization already done,
 * 			false = UFS initialization not done,
 * @skip_hs_mode_switch: Boolean flag to determine if HS mode switch should be
 *			executed or not.
 *			true = Skip HS mode switching,
 *			false = Switch to HS mode if supported,
 */
struct tegrabl_ufs_platform_params {
	uint8_t max_hs_mode;
	uint8_t max_pwm_mode;
	uint8_t max_active_lanes;
	uint32_t page_align_size;
	bool enable_hs_modes;
	bool enable_fast_auto_mode;
	bool enable_hs_rate_b;
	bool enable_hs_rate_a;
	bool ufs_init_done;
	bool skip_hs_mode_switch;
};

uint32_t tegrabl_ufs_get_attribute(uint32_t *pufsattrb, uint32_t attrbidn, uint8_t attrbindex);
uint32_t tegrabl_ufs_get_descriptor(uint8_t *pufsdesc, uint8_t descidn, uint8_t desc_index);
uint32_t tegrabl_ufs_set_attribute(uint32_t *pufsattrb, uint32_t attrbidn, uint8_t attrbindex);
uint32_t tegrabl_ufs_set_descriptor(uint8_t *pufsdesc, uint8_t descidn, uint8_t desc_index);
#endif
