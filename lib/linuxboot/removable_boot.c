/**
 * Copyright (c) 2019-2021, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#if defined(CONFIG_ENABLE_USB_SD_BOOT) || defined(CONFIG_ENABLE_NVME_BOOT)

#define MODULE  TEGRABL_ERR_LINUXBOOT

#include "build_config.h"
#include <string.h>
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
#include <linux_load.h>
#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
#include <extlinux_boot.h>
#endif
#include <removable_boot.h>
#if defined(CONFIG_ENABLE_A_B_SLOT)
#include <tegrabl_a_b_boot_control.h>
#endif

struct tegrabl_img_dtb {
	char *img_name_str;
	char *dtb_name_str;
	uint32_t img_bin_type;
	uint32_t dtb_bin_type;
} img_dtb_table[] = {
	{
		.img_name_str = "kernel",
		.dtb_name_str = "kernel-dtb",
		.img_bin_type = TEGRABL_BINARY_KERNEL,
		.dtb_bin_type = TEGRABL_BINARY_KERNEL_DTB
	},
#if defined(CONFIG_ENABLE_L4T_RECOVERY)
	{
		.img_name_str = "recovery",
		.dtb_name_str = "recovery-dtb",
		.img_bin_type = TEGRABL_BINARY_RECOVERY_IMG,
		.dtb_bin_type = TEGRABL_BINARY_RECOVERY_DTB
	},
#endif
};

static tegrabl_error_t load_from_partition(struct tegrabl_fm_handle *fm_handle,
						void **boot_img_load_addr,
						void **dtb_load_addr,
						bool is_recovery)
{
	uint32_t boot_img_size;
	uint32_t dtb_size;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_img_dtb *img_dtb = NULL;

	TEGRABL_UNUSED(is_recovery);

	/* Get load address of kernel/recovery and dtb/recovery-dtb */
	err = tegrabl_get_boot_img_load_addr(boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	*dtb_load_addr = (void *)tegrabl_get_dtb_load_addr();

	/* Fill the struct tegrabl_img_dtb based on the "is_recovery" */
#if defined(CONFIG_ENABLE_L4T_RECOVERY)
	img_dtb = is_recovery ? &img_dtb_table[1] : &img_dtb_table[0];
#else
	img_dtb = &img_dtb_table[0];
#endif

	/* Load kernel or recovery image */
	pr_info("Loading %s ...\n", img_dtb->img_name_str);
	boot_img_size = BOOT_IMAGE_MAX_SIZE;
	err = tegrabl_fm_read_partition(fm_handle->bdev, img_dtb->img_name_str,
						*boot_img_load_addr, &boot_img_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Load kernel dtb or recovery dtb */
	pr_info("Loading %s ...\n", img_dtb->dtb_name_str);
	dtb_size = DTB_MAX_SIZE;
	err = tegrabl_fm_read_partition(fm_handle->bdev, img_dtb->dtb_name_str,
						*dtb_load_addr, &dtb_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Validate both the binaries */
#if defined(CONFIG_ENABLE_SECURE_BOOT)
	err = tegrabl_validate_binary(img_dtb->img_bin_type, img_dtb->img_name_str, BOOT_IMAGE_MAX_SIZE,
					*boot_img_load_addr, &boot_img_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_validate_binary(img_dtb->dtb_bin_type, img_dtb->dtb_name_str, DTB_MAX_SIZE,
					*dtb_load_addr, NULL);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
#else
	/* When BCH is not available, then binary size cannot be known so use buffer size */
	boot_img_size = BOOT_IMAGE_MAX_SIZE;
#endif  /* CONFIG_ENABLE_SECURE_BOOT */

	err = tegrabl_verify_boot_img_hdr(*boot_img_load_addr, boot_img_size);

fail:
	return err;
}

tegrabl_error_t removable_boot_load_kernel_and_dtb(uint8_t boot_type,
												   uint8_t device_instance,
												   void **boot_img_load_addr,
												   void **dtb_load_addr,
												   void **ramdisk_load_addr,
												   uint32_t *kernel_size,
												   uint64_t *ramdisk_size)
{
	char *boot_type_str = NULL;
	uint8_t device_type = 0;
	struct tegrabl_device_config_params device_config = {0};
	struct tegrabl_bdev *bdev = NULL;
	struct tegrabl_fm_handle *fm_handle = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
#if defined(CONFIG_ENABLE_L4T_RECOVERY)
	struct tegrabl_kernel_bootctrl bootctrl;
	bool boot_to_recovery = false;
#endif

	/* Get device type */
	if (boot_type == BOOT_FROM_SD) {
		boot_type_str = "SD";
		device_type = TEGRABL_STORAGE_SDCARD;
	} else if (boot_type == BOOT_FROM_USB) {
		boot_type_str = "USB";
		device_type = TEGRABL_STORAGE_USB_MS;
	} else if (boot_type == BOOT_FROM_NVME) {
		boot_type_str = "NVME";
		device_type = TEGRABL_STORAGE_NVME;
	} else {
		pr_error("Invalid boot type %u\n", boot_type);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	pr_info("########## %s (%u) boot ##########\n", boot_type_str, device_instance);

	if ((boot_img_load_addr == NULL) || (dtb_load_addr == NULL)) {
		pr_error("Invalid args passed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	/* Initialize storage device */
	err = init_storage_device(&device_config, device_type, device_instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Failed to initialize device %u-%u\n", device_type, device_instance);
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
#if defined(CONFIG_ENABLE_L4T_RECOVERY)
	tegrabl_get_kernel_bootctrl(&bootctrl);
	if (bootctrl.mode == BOOT_TO_RECOVERY_MODE) {
		boot_to_recovery = true;
	}

#if defined(CONFIG_ENABLE_A_B_SLOT)
	/* If no rootfs available, load recovery kernel */
	if (tegrabl_a_b_rootfs_is_all_unbootable(NULL)) {
		boot_to_recovery = true;
	}
#endif

	if (boot_to_recovery) {
		/* Load recovery and recovery-dtb */
		err = load_from_partition(fm_handle, boot_img_load_addr, dtb_load_addr, true);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Load recovery image or recovery dtb failed, err: %u\n", err);
		}
		goto fail;
	}
#endif

#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
	bool kernel_from_rootfs;

	if (fm_handle->mount_path == NULL) {
		/* If mount path is null means rootfs is not found within device */
		goto fallback;
	}
	err = extlinux_boot_load_kernel_and_dtb(fm_handle, boot_img_load_addr, dtb_load_addr, kernel_size,
											&kernel_from_rootfs);
	if (err == TEGRABL_NO_ERROR) {
		/* If kernel is loaded from rootfs, we need to continue to load ramdisk.
		 * Note: if kernel is loaded from partition, it contains ramdisk already.
		 */
		if (kernel_from_rootfs == true) {
			err = extlinux_boot_load_ramdisk(fm_handle, ramdisk_load_addr, ramdisk_size);
			if (err == TEGRABL_NO_ERROR) {
				extlinux_boot_set_status(true);
			} else {
				pr_error("Failed loading ramdisk\n");
			}
		}
		goto fail;  /* There is no fallback for ramdisk, so let caller handle the error */
	}
	pr_error("Failed extlinux boot.\n");
	goto fail;
#else
	goto fallback;
#endif

fallback:
	if (fm_handle->bdev->published == false) {
		/* No partitions found */
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}
	err = load_from_partition(fm_handle, boot_img_load_addr, dtb_load_addr, false);
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
		if (boot_type == BOOT_FROM_NVME) {
			/* A problem in bdev's ref_count prevents the bdev to close gracefully, close it forcefully */
			pr_info("%s: force NVME bdev to close !!!\n", __func__);
			bdev->close(bdev);
		}
	}
	return err;
}

#endif  /* CONFIG_ENABLE_USB_SD_BOOT || CONFIG_ENABLE_NVME_BOOT */
