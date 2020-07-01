/*
 * Copyright (c) 2018-2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_FILE_MANAGER

#include "build_config.h"
#include <string.h>
#include <tegrabl_utils.h>
#include <tegrabl_malloc.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_file_manager.h>
#include <tegrabl_debug.h>
#include <inttypes.h>
#include <tegrabl_error.h>
#include <fs.h>
#include <tegrabl_cbo.h>

static struct tegrabl_fm_handle *fm_handle;

static char *usb_prefix = "/usb";
static char *sdcard_prefix = "/sd";
static char *sdmmc_user_prefix = "/sdmmc_user";
static char *sdmmc_boot_prefix = "/sdmmc_boot";

static char *get_prefix(uint32_t device_id)
{
	char *prefix = NULL;
	uint32_t bdev_id = BITFIELD_GET(device_id, 16, 16);

	switch (bdev_id) {
	case TEGRABL_STORAGE_USB_MS:
		prefix = usb_prefix;
		break;
	case TEGRABL_STORAGE_SDCARD:
		prefix = sdcard_prefix;
		break;
	case TEGRABL_STORAGE_SDMMC_USER:
		prefix = sdmmc_user_prefix;
		break;
	case TEGRABL_STORAGE_SDMMC_BOOT:
		prefix = sdmmc_boot_prefix;
		break;
	default:
		; /* Do nothing */
		break;
	}

	return prefix;
}

/**
* @brief Get file manager handle
*
* @return File manager handle
*/
struct tegrabl_fm_handle *tegrabl_file_manager_get_handle(void)
{
	return fm_handle;
}

/**
* @brief Publish the partitions available in the GPT and try to mount the FS in "BOOT" partition.
* If GPT itself is not available, try to detect FS from sector 0x0 and mount it.
*
* @param bdev storage device pointer
*
* @return File manager handle
*/
tegrabl_error_t tegrabl_fm_publish(tegrabl_bdev_t *bdev, struct tegrabl_fm_handle **handle)
{
	struct tegrabl_partition boot_partition = {0};
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	char *fs_type;
	char *prefix = NULL;
	int32_t status = 0x0;
	uint32_t detect_fs_sector;
	char *nvidia_boot_pt_guid = NULL;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	/* Allocate memory for handle */
	fm_handle = tegrabl_malloc(sizeof(struct tegrabl_fm_handle));
	if (fm_handle == NULL) {
		pr_error("Failed to allocate memory for fm handle!!\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0x0);
		goto fail;
	}
	memset(fm_handle, 0x0, sizeof(struct tegrabl_fm_handle));
	fm_handle->bdev = bdev;

	/* Publish the partitons available in the block device */
	err = tegrabl_partition_publish(bdev, 0);
	if (err != TEGRABL_NO_ERROR) {
		/* GPT does not exist, detect FS from start sector of the device */
		detect_fs_sector = 0x0;
		goto detect_fs;
	}
	/* GPT exists and partitions are published */

	pr_info("Look for boot partition\n");
	nvidia_boot_pt_guid = tegrabl_get_boot_pt_guid();
	err = tegrabl_partition_boot_guid_lookup_bdev(nvidia_boot_pt_guid, &boot_partition, bdev);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* BOOT partition exists! */
	pr_info("Detect filesystem in boot partition\n");
	detect_fs_sector = boot_partition.partition_info->start_sector;

detect_fs:
	fs_type = fs_detect(bdev, detect_fs_sector);
	if (fs_type == NULL) {
		/* No supported FS detected */
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0x0);
		goto fail;
	}

	prefix = get_prefix(bdev->device_id);
	if (prefix == NULL) {
		/* Unsupported device */
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0x1);
		pr_error("Unsupported storage device for File system!!\n");
		goto fail;
	}

	/* Mount fs */
	status = fs_mount(prefix, fs_type, bdev, detect_fs_sector);
	if (status != 0x0) {
		/* Mount failed */
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0x2);
		pr_error("Failed to mount file system!!\n");
		goto fail;
	}

	fm_handle->fs_type = fs_type;
	fm_handle->start_sector = detect_fs_sector;
	fm_handle->mount_path = prefix;
	if (handle) {
		*handle = fm_handle;
	}

fail:
	return err;
}

/**
* @brief Read the file from the filesystem if possible, otherwise read form the partiton.
*
* @param handle pointer to filemanager handle
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
								bool *is_file_loaded_from_fs)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_partition partition;
	uint32_t partition_size;
	char path[200];
	filehandle *fh;
	struct file_stat stat;
	int32_t status = 0x0;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if (is_file_loaded_from_fs != NULL) {
		*is_file_loaded_from_fs = false;
	}

	if (handle == NULL) {
		pr_error("Null handle passed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0x3);
		goto fail;
	}

	memset(path, ARRAY_SIZE(path), '\0');

	if ((file_path == NULL) || (handle->mount_path == NULL)) {
		goto load_from_partition;
	}

	/* Load file from FS */
	strcpy(path, handle->mount_path);
	strcat(path, file_path);

	status = fs_open_file(path, &fh);
	if (status != 0x0) {
		pr_error("file %s open failed!!\n", path);
		err = TEGRABL_ERROR(TEGRABL_ERR_OPEN_FAILED, 0x0);
		goto fail;
	}

	status = fs_stat_file(fh, &stat);
	if (status != 0x0) {
		pr_error("file %s stat failed!!\n", path);
		err = TEGRABL_ERROR(TEGRABL_ERR_OPEN_FAILED, 0x1);
		goto fail;
	}

	/* Check for file overflow */
	if (*size < stat.size) {
		err = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0x0);
		goto fail;
	}

	status = fs_read_file(fh, *load_address, 0x0, stat.size);
	if (status < 0) {
		pr_error("file %s read failed!!\n", path);
		err = TEGRABL_ERROR(TEGRABL_ERR_READ_FAILED, 0x1);
		goto fail;
	}

	*size = stat.size;
	if (is_file_loaded_from_fs) {
		*is_file_loaded_from_fs = true;
	}

	fs_close_file(fh);
	goto fail;

load_from_partition:
	if (partition_name != NULL) {
		/* Get partition info */
		err = tegrabl_partition_lookup_bdev(partition_name, &partition, handle->bdev);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Cannot open partition %s\n", partition_name);
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}

		/* Get partition size */
		partition_size = tegrabl_partition_size(&partition);
		pr_debug("Size of partition: %u\n", partition_size);
		if (!partition_size) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0x4);
			goto fail;
		}

		/* Check if the partition load may over flow */
		if (size != NULL) {
			if (*size < partition_size) {
				err = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0x1);
				goto fail;
			}
		}

		/* Read the partition from storage */
		err = tegrabl_partition_read(&partition, *load_address, partition_size);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error reading partition %s\n", partition_name);
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}

		/* Return size */
		if (size) {
			*size = partition_size;
		}
	}

fail:
	return err;
}

/**
* @brief Unmount the filesystem and freeup memory.
*
* @param handle filemanager handle to unmount and free the space.
*/
void tegrabl_fm_close(struct tegrabl_fm_handle *handle)
{
	if (handle == NULL) {
		goto fail;
	}

	if (handle->mount_path != NULL) {
		fs_unmount(handle->mount_path);
	}

	tegrabl_free(handle);
	fm_handle = NULL;

fail:
	return;
}
