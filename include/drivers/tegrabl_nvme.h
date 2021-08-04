/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_NVME_H
#define TEGRABL_NVME_H

#include <tegrabl_blockdev.h>
#include <tegrabl_error.h>

/** @brief Initializes the host controller for nvme with the given
 *         instance.
 *
 *  @param instance NVME instance to be initialize.
 *
 *  @return TEGRABL_NO_ERROR if init is successful else appropriate error.
 */
tegrabl_error_t tegrabl_nvme_bdev_open(uint32_t instance);

#endif /* TEGRABL_NVME_H */
