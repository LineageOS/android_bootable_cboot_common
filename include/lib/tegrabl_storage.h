/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * @file tegrabl_storage.h
 *
 * Provides interface for initializing boot and storage device.
 */

#ifndef INCLUDED_TEGRABL_STORAGE_H
#define INCLUDED_TEGRABL_STORAGE_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_mb1_bct.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_sdmmc_bdev.h>


/**
 * @brief Map mb1 bct device type enum to storage device type enum
 *
 * @param mb1bct_dev mb1 bct device type
 *
 * @return storage device type
 */
tegrabl_storage_type_t tegrabl_storage_map_to_storage_dev_from_mb1bct_dev(
									tegrabl_mb1_bct_boot_device_t mb1bct_dev);

/**
 * @brief Initialize the boot device
 *
 * @param type Device type
 * @param instance Controller instance of the device
 * @param dev_params pointer to device params
 * @param sdmmc_init SDMMC init flag
 * @param ufs_reinit UFS device reinitialization
 *
 * @return On success return TEGRABL_NO_ERROR otherwise appropriate error
 */
tegrabl_error_t tegrabl_storage_init_dev(
								tegrabl_storage_type_t type, uint32_t instance,
								struct tegrabl_mb1bct_device_params *dev_params,
								enum sdmmc_init_flag sdmmc_init,
								bool ufs_reinit);

/**
 * @brief Initialize the boot device
 *
 * @param dev_params pointer to device params
 * @param boot_dev pointer to store boot device type
 * @param sdmmc_init SDMMC init flag
 *
 * @return On success return TEGRABL_NO_ERROR otherwise appropriate error
 */
tegrabl_error_t tegrabl_storage_init_boot_dev(
								struct tegrabl_mb1bct_device_params *dev_params,
								tegrabl_storage_type_t *const boot_dev,
								enum sdmmc_init_flag sdmmc_init);

/**
 * @brief Initialize all storage devices
 *
 * @param storage_devs pointer to array of storage devices
 * @param dev_params pointer to device params
 * @param boot_dev boot device type
 * @param sdmmc_init SDMMC init flag
 * @param ufs_reinit UFS device reinitialization
 *
 * @return On success return TEGRABL_NO_ERROR otherwise appropriate error
 */
tegrabl_error_t tegrabl_storage_init_storage_devs(
								const struct tegrabl_device *storage_devs,
								struct tegrabl_mb1bct_device_params *dev_params,
								tegrabl_storage_type_t boot_dev,
								enum sdmmc_init_flag sdmmc_init,
								bool ufs_reinit);

/**
 * @brief Partially initialize sdmmc if it's only a storage device
 *
 * @param dev_params pointer to device params
 *
 * @return On success return TEGRABL_NO_ERROR otherwise appropriate error
 */
tegrabl_error_t tegrabl_storage_partial_sdmmc_init(
						struct tegrabl_mb1bct_device_params *dev_params);

/**
 * @brief Check if given dev type is set as storage device
 *
 * @param storage_devs pointer to array of storage devices
 * @param dev_type storage device type
 * @param instance storage device type instance
 *
 * @return On success return true otherwise false for other cases
 */
bool tegrabl_storage_is_storage_enabled(
						const struct tegrabl_device *storage_devs,
						const tegrabl_storage_type_t dev_type,
						const uint32_t instance);

#endif /* INCLUDED_TEGRABL_STORAGE_H */
