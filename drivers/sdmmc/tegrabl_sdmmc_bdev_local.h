/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SDMM_BDEV_LOCAL_H
#define TEGRABL_SDMM_BDEV_LOCAL_H

#include "build_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_blockdev.h>

#define TEGRABL_SDMMC_BUF_ALIGN_SIZE 4U

/** @brief Performs the requested ioctl.
 *
 *  @param dev tegrabl_bdev_t handle for the ioctl.
 *  @param request Ioctl type to be executed.
 *  @param argp Argument to be passed to the ioctl.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_bdev_ioctl(tegrabl_bdev_t *dev, uint32_t ioctl,
	void *args);

/** @brief Reads the number of sectors starting from block argument till the
 *         number of sectors is equal to count. On successful read it returns
 *         the number of bytes read and 0 in failure. The current
 *         implementation supports only ddr50.
 *
 *  @param dev The registered bio device from which read is required.
 *  @param buf Input buffer in which data will be read.
 *  @param block Starting sector from which data will be read.
 *  @param count Total number of sectors from which data will be read.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_bdev_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count);

#if !defined(CONFIG_DISABLE_EMMC_BLOCK_WRITE)
/** @brief Writes the number of sectors starting from block argument till the
 *         number of sectors is equal to count. On successful read it returns
 *         the number of bytes written and 0 in failure.
 *
 *  @param dev The registered bio device in which write is required.
 *  @param buf Input buffer from which data will be written.
 *  @param block Starting sector to which data will be written.
 *  @param count Total number of sectors to which data will be written.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_bdev_write_block(tegrabl_bdev_t *dev,
	const void *buf, bnum_t block, bnum_t count);
#endif

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
/** @brief Erase the number of sectors starting from offset argument till the
 *         number of sectors is equal to len. On successful read it returns the
 *         number of sectors erased and 0 in failure. The current
 *         implementation uses high erase for sectors aligned with high
 *         capacity erase group size. If the sector is unaligned then trim
 *         command is used for erasing residual sectors.
 *
 *  @param dev The registered bio device in which erase is required.
 *  @param block Starting sector which will be erased.
 *  @param count Total number of sectors which will be erased.
 *  @param is_secure Normal erase or secure erase.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_bdev_erase(tegrabl_bdev_t *dev, bnum_t block,
	bnum_t count, bool is_secure);

/**
 *  @brief Initiates the given xfer
 *
 *  @param xfer Address of the xfer info structure
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_bdev_xfer(struct tegrabl_blockdev_xfer_info *xfer);

/**
 *  @brief Checks the transfer status and triggers the next
 *
 *  @param xfer Address of the xfer info structure
 *  @param timeout Maxmimum timeout to wait
 *  @param status Address of the status flag to keep
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_bdev_xfer_wait(struct tegrabl_blockdev_xfer_info *xfer, time_t timeout,
		uint8_t *status);
#endif

/** @brief Deallocates the memory allocated to sdmmc context.
 *
 *  @param dev bdev_t handle to be deallocated.
 *
 *  @return Void.
 */
tegrabl_error_t sdmmc_bdev_close(tegrabl_bdev_t *dev);
#endif /* TEGRABL_SDMM_BDEV_LOCAL_H */
