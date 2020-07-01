/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_USBMSD_H
#define TEGRABL_USBMSD_H

#include <tegrabl_blockdev.h>
#include <tegrabl_error.h>

/** @brief Initializes the host controller for usbmsd with the given
 *         instance.
 *
 *  @param instance usbmsd instance to be initialize.
 *
 *  @return TEGRABL_NO_ERROR if init is successful else appropriate error.
 */
tegrabl_error_t tegrabl_usbmsd_bdev_open(uint32_t instance);

/** @brief Deallocates the memory allocated to usbmsd context.
 *
 *  @param dev bdev_t handle to be deallocated.
 *
 *  @return Void.
 */
void usbmsd_bdev_close(tegrabl_bdev_t *dev);

#endif /* TEGRABL_USBMSD_H */
