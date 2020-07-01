/*
 * Copyright (c) 2016-2018,NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_UFS_DEFS_H
#define TEGRABL_UFS_DEFS_H

#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_error.h>

#define MAX_TRD_NUM    12U
#define MAX_TMD_NUM    8U
#define UFS_BLOCK_SIZE_LOG2  12U
#define UFS_PAGE_SIZE_LOG2   12U

#define UFS_FUSE_PARAMS_0_NUM_LANES_RANGE       (0) : (0)
#define UFS_FUSE_PARAMS_0_ACTIVE_LANES_RANGE    (1) : (1)
#define UFS_FUSE_PARAMS_0_BOOT_ENABLE_RANGE     (2) : (2)
#define UFS_FUSE_PARAMS_0_LUN_RANGE             (5) : (3)
#define UFS_FUSE_PARAMS_0_PAGE_SIZE_RANGE       (7) : (6)
#define UFS_FUSE_PARAMS_0_SPEED_RANGE           (9) : (8)

struct tegrabl_ufs_params {
	uint32_t pwm_gear;
	uint32_t active_lanes;
	uint32_t page_align_size;
	uint8_t max_hs_mode;
	uint8_t max_pwm_mode;
	uint8_t max_active_lanes;
	bool enable_hs_modes;
	bool enable_fast_auto_mode;
	bool enable_hs_rate_b;
	bool enable_hs_rate_a;
	bool ufs_init_done;
	bool skip_hs_mode_switch;
};

void tegrabl_ufs_default_state(void);
tegrabl_error_t tegrabl_ufs_deinit(void);
tegrabl_error_t tegrabl_ufs_change_gear(uint32_t gear);
#endif
