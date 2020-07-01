/**
 * Copyright (c) 2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#if defined(CONFIG_ENABLE_USB_SD_BOOT)

#define MODULE  TEGRABL_ERR_LINUXBOOT

#include "build_config.h"
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_cbo.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_linuxboot_helper.h>
#include <config_storage.h>
#include <tegrabl_file_manager.h>
#include <tegrabl_sdram_usage.h>
#include <tegrabl_binary_types.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_linuxboot_utils.h>
#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
#include <extlinux_boot.h>
#endif
#include <usb_sd_boot.h>

static tegrabl_error_t load_from_partition(struct tegrabl_fm_handle *fm_handle,
										   void **boot_img_load_addr,
										   void **dtb_load_addr)
{
	uint32_t boot_img_size;
	uint32_t dtb_size;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* Get load address of kernel and dtb   */
	err = tegrabl_get_boot_img_load_addr(boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	*dtb_load_addr = (void *)tegrabl_get_dtb_load_addr();

	/* Load boot image */
	pr_info("Loading kernel ...\n");
	boot_img_size = BOOT_IMAGE_MAX_SIZE;
	err = tegrabl_fm_read_partition(fm_handle->bdev, "kernel", *boot_img_load_addr, &boot_img_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Load kernel dtb */
	pr_info("Loading kernel-dtb ...\n");
	dtb_size = DTB_MAX_SIZE;
	err = tegrabl_fm_read_partition(fm_handle->bdev, "kernel-dtb", *dtb_load_addr, &dtb_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Validate both the binaries */
#if defined(CONFIG_OS_IS_L4T)
	err = tegrabl_validate_binary(TEGRABL_BINARY_KERNEL, BOOT_IMAGE_MAX_SIZE, *boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_validate_binary(TEGRABL_BINARY_KERNEL_DTB, DTB_MAX_SIZE, *dtb_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
#endif

	err = tegrabl_verify_boot_img_hdr(*boot_img_load_addr, boot_img_size);

fail:
	return err;
}

tegrabl_error_t usb_sd_boot_load_kernel_and_dtb(uint8_t boot_type,
												void **boot_img_load_addr,
												void **dtb_load_addr,
												void **ramdisk_load_addr,
												uint32_t *kernel_size,
												uint64_t *ramdisk_size)
{
	char *boot_type_str = NULL;
	uint8_t device_type = 0;
	uint8_t device_instance = 0;
	struct tegrabl_device_config_params device_config = {0};
	struct tegrabl_bdev *bdev = NULL;
	struct tegrabl_fm_handle *fm_handle = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* Get device type */
	device_instance = 0;
	if (boot_type == BOOT_FROM_SD) {
		boot_type_str = "SD";
		device_type = TEGRABL_STORAGE_SDCARD;
	} else if (boot_type == BOOT_FROM_USB) {
		boot_type_str = "USB";
		device_type = TEGRABL_STORAGE_USB_MS;
	} else {
		pr_error("Invalid boot type %u\n", boot_type);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	pr_info("########## %s boot ##########\n", boot_type_str);

	if ((boot_img_load_addr == NULL) || (dtb_load_addr == NULL)) {
		pr_error("Invalid args passed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	/* Initialize storage device */
	err = init_storage_device(&device_config, device_type, device_instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to initialize device %u-%u\n", device_type, device_instance);
		goto fail;
	}

	/* Publish partitions of storage device*/
	bdev = tegrabl_blockdev_open(device_type, device_instance);
	if (bdev == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_OPEN_FAILED, 0);
		goto fail;
	}
	tegrabl_fm_publish(bdev, &fm_handle);
	if (fm_handle == NULL) {
		/* Above publish function prints the error msg, so no need to again print the error here */
		goto fail;
	}

#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
	if (fm_handle->mount_path == NULL) {
		/* If mount path is null means rootfs is not found within device */
		goto fallback;
	}
	err = extlinux_boot_load_kernel_and_dtb(fm_handle, boot_img_load_addr, dtb_load_addr, kernel_size);
	if (err == TEGRABL_NO_ERROR) {
		err = extlinux_boot_load_ramdisk(fm_handle, ramdisk_load_addr, ramdisk_size);
		if (err == TEGRABL_NO_ERROR) {
			extlinux_boot_set_status(true);
		}
		goto fail;  /* There is no fallback for ramdisk, so let caller handle the error */
	}
	pr_info("Fallback: Load binaries from partition\n");
#endif

fallback:
	if (fm_handle->bdev->published == false) {
		/* No partitions found */
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}
	err = load_from_partition(fm_handle, boot_img_load_addr, dtb_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	if (fm_handle != NULL) {
		/* Un-publish partitions */
		if (tegrabl_partitions_unpublish(bdev) != TEGRABL_NO_ERROR) {
			pr_error("Failed to unpublish partitions of storage device\n");
		}
		tegrabl_fm_close(fm_handle);
	}
	if (bdev != NULL) {
		tegrabl_blockdev_close(bdev);
	}
	return err;
}

#endif  /* CONFIG_ENABLE_USB_SD_BOOT */
