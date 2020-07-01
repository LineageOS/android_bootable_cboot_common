/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include <tegrabl_blockdev.h>

#ifndef TEGRABL_BLOCKDEV_PROFILING_H
#define TEGRABL_BLOCKDEV_PROFILING_H

/**
* @brief Records the read start timestamp
*
* @param dev Block device handle.
*/
void profile_read_start(tegrabl_bdev_t *dev);

/**
* @brief Records the read end timestamp and calculates the total read time
*
* @param dev Block device handle
* @param size Bytes read
*/
void profile_read_end(tegrabl_bdev_t *dev, uint64_t size);

/**
* @brief Records the write start timestamp
*
* @param dev Block device handle
*/
void profile_write_start(tegrabl_bdev_t *dev);

/**
* @brief Records the write end timestamp
*
* @param dev Block device handle
* @param size Bytes written.
*/
void profile_write_end(tegrabl_bdev_t *dev, uint64_t size);

/**
* @brief Prints the kpi info total read and write time and speed
*
* @param bdevs Pointer to the Block device handles
*/
void list_kpi(struct tegrabl_bdev_struct *bdevs);

#endif
