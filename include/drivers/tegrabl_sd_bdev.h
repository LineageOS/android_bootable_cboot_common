/*
 * Copyright (c) 2016, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited
 */

#ifndef TEGRABL_SD_BDEV_H
#define TEGRABL_SD_BDEV_H

#include <tegrabl_error.h>
#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_blockdev.h>

/**
 * @brief Gives the instance of the sdcard and tells whether card is present
 *        or not
 *
 * @param instance Address in which instance has to be stored.
 * @param is_present Address in which card present status has to be stored.
 *
 * @return TEGRABL_NO_ERROR if success, otherwise error code.
 */
tegrabl_error_t sd_bdev_is_card_present(uint32_t *instance, bool *is_present);

/** @brief Initializes the host controller for sdmmc and card with the given
 *         instance
 *
 *  @param emmc_params Parameters to initialize sdmmc.
 *  @param flag flag to specify full init/reinit/skip enum
 *
 *  @return TEGRABL_NO_ERROR if success, otherwise error code.
 */
tegrabl_error_t sd_bdev_open(uint32_t instance);

/** @brief Deallocates the memory allocated to sdmmc context.
 *
 *  @param dev bdev_t handle to be deallocated.
 *
 *  @return Void.
 */
tegrabl_error_t sd_bdev_close(tegrabl_bdev_t *dev);

#endif  /* SD_BDEV_H */
