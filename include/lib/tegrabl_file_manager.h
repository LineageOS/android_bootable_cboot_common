/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_FILE_MANAGER_H
#define TEGRABL_FILE_MANAGER_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_blockdev.h>

/**
 * @brief file manager handle which contains the mounted path of the filesystem in the given storage device.
 */
struct tegrabl_fm_handle {
	tegrabl_bdev_t *bdev;
	char *fs_type;
	char *mount_path;
	uint32_t start_sector;
};

/**
 * @brief Publish the partitions available in the GPT and try to mount the FS in "BOOT" partition.
 * If GPT itself is not available, try to detect FS from sector 0x0 and mount it.
 *
 * @param bdev storage device pointer
 * @param handle double pointer to file manager handle
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails.
 */
tegrabl_error_t tegrabl_fm_publish(tegrabl_bdev_t *bdev, struct tegrabl_fm_handle **handle);

/**
 * @brief Read the file from the filesystem if available, otherwise read form the partiton.
 *
 * @param handle pointer to file manager handle
 * @param file_path file name along with the path
 * @param partition_name partition to read from in case if file read fails from filesystem.
 * @param load_address address into which the file/partition needs to be loaded.
 * @param size max size of the file expected by the caller.
 * @param is_file_loaded_from_fs specify whether file is loaded from filesystem or partition.
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails.
 */
tegrabl_error_t tegrabl_fm_read(struct tegrabl_fm_handle *handle,
								char *file_path,
								char *partition_name,
								void **load_address,
								uint32_t *size,
								bool *is_file_loaded_from_fs);

/**
 * @brief get file manager handle
 *
 * @return file manager handle
 */
struct tegrabl_fm_handle *tegrabl_file_manager_get_handle(void);

/**
 * @brief unmount the filesystem and freeup memory.
 *
 * @param handle file manager handle to unmount and free the space.
 */
void tegrabl_fm_close(struct tegrabl_fm_handle *handle);

#endif
