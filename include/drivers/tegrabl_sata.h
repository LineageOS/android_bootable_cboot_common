/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SATA_H
#define TEGRABL_SATA_H

#include <tegrabl_blockdev.h>
#include <tegrabl_error.h>
#include <tegrabl_uphy.h>
#include <tegrabl_sata_param.h>

/** @brief Initializes the host controller for sata and card with the given
 *         instance.
 *
 *  @param instance SATA instance to be initialize.
 *  @param Handle for uphy driver
 *
 *  @return TEGRABL_NO_ERROR if init is successful else appropriate error.
 */
tegrabl_error_t tegrabl_sata_bdev_open(uint32_t instance,
		struct tegrabl_uphy_handle *uphy,
		struct tegrabl_sata_platform_params *device_params);

/** @brief Deallocates the memory allocated to sata context.
 *
 *  @param dev bdev_t handle to be deallocated.
 *
 *  @return Void.
 */
void sata_bdev_close(tegrabl_bdev_t *dev);

#endif /* TEGRABL_SATA_H */
