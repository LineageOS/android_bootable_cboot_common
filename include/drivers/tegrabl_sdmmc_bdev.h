/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef SDMMC_BDEV_H
#define SDMMC_BDEV_H

#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_error.h>
#include <tegrabl_sdmmc_param.h>

/* sdmmc modes */
#define TEGRABL_SDMMC_MODE_SDR26  0U
#define TEGRABL_SDMMC_MODE_DDR52  1U
#define TEGRABL_SDMMC_MODE_HS200  2U
#define TEGRABL_SDMMC_MODE_HS400  3U
#define TEGRABL_SDMMC_MODE_MAX    4U

/** @brief Initializes the host controller for sdmmc and card with the given
 *         instance. Registers boot & user devices in bio layer.
 *         Current configuration supported is DDR/SDR.
 *
 *  @param instance instance of the sdmmc controller.
 *  @param params Parameters to initialize sdmmc.
 *
 *  @return NO_ERROR if init is successful else appropriate error code.
 */
tegrabl_error_t sdmmc_bdev_open(uint32_t instance, struct tegrabl_sdmmc_platform_params *params);

/** @brief send CMD0, Partial CMD1
 *   This is to avoid the waiting time in QB for emmc device to warm up/reset
 *
 *  @param instance instance of the sdmmc controller.
 *  @param params Parameters to initialize sdmmc.
 *
 *  @return TEGRABL_NO_ERROR if successful, specific error if fails
 */
tegrabl_error_t sdmmc_send_cmd0_cmd1(uint32_t instance, struct tegrabl_sdmmc_platform_params *params);

#endif  /* SDMMC_BDEV_H */
