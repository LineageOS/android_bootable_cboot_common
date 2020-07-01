/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef SDMMC_BDEV_H
#define SDMMC_BDEV_H

#include <tegrabl_blockdev.h>
#include <tegrabl_mb1_bct.h>

enum tegrabl_sdmmc_mode {
	TEGRABL_SDMMC_MODE_SDR26,
	TEGRABL_SDMMC_MODE_DDR52,
	TEGRABL_SDMMC_MODE_HS200,
	TEGRABL_SDMMC_MODE_MAX,
};

enum sdmmc_init_flag {
	SDMMC_INIT = 0,
	SDMMC_INIT_REINIT,
	SDMMC_INIT_SKIP,
	SDMMC_INIT_SET_BITS = 3, /* for the mask */
};

#define SDMMC_INIT_MASK  (SDMMC_INIT_SET_BITS << 0)

/** @brief Initializes the host controller for sdmmc and card with the given
 *         instance. Registers boot & user devices in bio layer.
 *         Current configuration supported is DDR/SDR.
 *
 *  @param emmc_params Parameters to initialize sdmmc.
 *  @param flag flag to specify full init/reinit/skip enum
 *
 *  @return NO_ERROR if init is successful else appropriate error code.
 */
tegrabl_error_t sdmmc_bdev_open(
		struct tegrabl_mb1bct_emmc_params *emmc_params, uint32_t flag);

/** @brief Deallocates the memory allocated to sdmmc context.
 *
 *  @param dev bdev_t handle to be deallocated.
 *
 *  @return Void.
 */
tegrabl_error_t sdmmc_bdev_close(tegrabl_bdev_t *dev);

/** @brief send CMD0, Partial CMD1
 *   This is to avoid the waiting time in QB for emmc device to warm up/reset
 *
 *  @param emmc_params Parameters to initialize sdmmc.
 *
 *  @return TEGRABL_NO_ERROR if successful, specific error if fails
 */
tegrabl_error_t sdmmc_send_cmd0_cmd1(
							struct tegrabl_mb1bct_emmc_params *emmc_params);

#endif  /* SDMMC_BDEV_H */
