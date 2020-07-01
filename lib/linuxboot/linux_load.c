/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_LINUXBOOT

#include "build_config.h"
#include <stdint.h>
#include <string.h>
#include <libfdt.h>
#include <inttypes.h>
#include <tegrabl_utils.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_bootimg.h>
#include <tegrabl_linuxboot.h>
#include <tegrabl_linuxboot_helper.h>
#include <tegrabl_sdram_usage.h>
#include <linux_load.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_partition_loader.h>
#include <tegrabl_decompress.h>
#include <tegrabl_malloc.h>
#include <dtb_overlay.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_cbo.h>
#include <tegrabl_usbh.h>
#include <tegrabl_cpubl_params.h>

#if defined(CONFIG_ENABLE_BOOT_DEVICE_SELECT)
#include <config_storage.h>
#endif

#if defined(CONFIG_ENABLE_ETHERNET_BOOT)
#include <net_boot.h>
#endif

static uint64_t ramdisk_load;
static uint64_t ramdisk_size;
static char *bootimg_cmdline;

#define KERNEL_DTBO_PART_SIZE	(1024 * 1024 * 1)

#define FDT_SIZE_BL_DT_NODES (4048 + 4048)

#if defined(CONFIG_ENABLE_BOOT_DEVICE_SELECT)

static int8_t g_boot_order[NUM_SECONDARY_STORAGE_DEVICES] = {
	/* Specified in the order of priority from top to bottom */
	BOOT_FROM_SD,
	BOOT_FROM_USB,
	BOOT_FROM_BUILTIN_STORAGE,
	BOOT_FROM_NETWORK,
	BOOT_DEFAULT,
};
#endif

void tegrabl_get_ramdisk_info(uint64_t *start, uint64_t *size)
{
	if (start) {
		*start = ramdisk_load;
	}
	if (size) {
		*size = ramdisk_size;
	}
}

char *tegrabl_get_bootimg_cmdline(void)
{
	return bootimg_cmdline;
}

/* Sanity checks the kernel image extracted from Android boot image */
static tegrabl_error_t validate_kernel(union tegrabl_bootimg_header *hdr,
									   uint32_t *hdr_crc)
{
	uint32_t known_crc = 0;
	uint32_t calculated_crc = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_info("Checking boot.img header magic ... ");
	/* Check header magic */
	if (memcmp(hdr->magic, ANDROID_MAGIC, ANDROID_MAGIC_SIZE)) {
		pr_error("Invalid boot.img @ %p (header magic mismatch)\n", hdr);
		err = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
		goto fail;
	}
	pr_info("[OK]\n");

	/* Check header CRC if present */
	known_crc =
		hdr->word[(ANDROID_HEADER_SIZE - CRC32_SIZE) / sizeof(uint32_t)];
	if (known_crc) {
		pr_info("Checking boot.img header crc ... ");
		calculated_crc = tegrabl_utils_crc32(0, (char *)hdr,
											 ANDROID_HEADER_SIZE);
		if (calculated_crc != known_crc) {
			pr_error("Invalid boot.img @ %p (header crc mismatch)\n", hdr);
			err = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
			goto fail;
		}
		pr_info("[OK]\n");
	}

	pr_info("Valid boot.img @ %p\n", hdr);

fail:
	*hdr_crc = known_crc;
	return err;
}

/* Extract kernel from an Android boot image, and return the address where it is installed in memory */
static tegrabl_error_t extract_kernel(void *boot_img_load_addr, void **kernel_load_addr)
{
	uint64_t kernel_offset = 0; /* Offset of 1st kernel byte in boot.img */
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t hdr_crc = 0;
	bool is_compressed = false;
	decompressor *decomp = NULL;
	uint32_t decomp_size = 0; /* kernel size after decompressing */
	union tegrabl_bootimg_header *hdr = NULL;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	hdr = (union tegrabl_bootimg_header *)boot_img_load_addr;
	kernel_offset = hdr->pagesize;

	err = validate_kernel(hdr, &hdr_crc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u failed to validate kernel\n", err);
		return err;
	}

	*kernel_load_addr = (void *)tegrabl_get_kernel_load_addr();
	pr_trace("%u: kernel load addr: %p\n", __LINE__, *kernel_load_addr);

	is_compressed = is_compressed_content((uint8_t *)hdr + kernel_offset, &decomp);

	if (!is_compressed) {
		pr_info("Copying kernel image (%u bytes) from %p to %p ... ",
				hdr->kernelsize, (char *)hdr + kernel_offset, *kernel_load_addr);
		memmove(*kernel_load_addr, (char *)hdr + kernel_offset, hdr->kernelsize);
	} else {
		pr_info("Decompressing kernel image (%u bytes) from %p to %p ... ",
				hdr->kernelsize, (char *)hdr + kernel_offset, *kernel_load_addr);

		decomp_size = MAX_KERNEL_IMAGE_SIZE;
		err = do_decompress(decomp, (uint8_t *)hdr + kernel_offset,
							hdr->kernelsize, *kernel_load_addr, &decomp_size);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("\nError %d decompress kernel\n", err);
			return err;
		}
	}

	pr_info("Done\n");

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t extract_ramdisk(void *boot_img_load_addr)
{
	union tegrabl_bootimg_header *hdr = NULL;
	uint64_t ramdisk_offset = (uint64_t)NULL; /* Offset of 1st ramdisk byte in boot.img */
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	ramdisk_load = tegrabl_get_ramdisk_load_addr();
	pr_trace("%u: ramdisk load addr: 0x%"PRIx64"\n", __LINE__, ramdisk_load);

	hdr = (union tegrabl_bootimg_header *)boot_img_load_addr;
	ramdisk_offset = ROUND_UP_POW2(hdr->pagesize + hdr->kernelsize, hdr->pagesize);
	ramdisk_offset = (uintptr_t)hdr + ramdisk_offset;
	ramdisk_size = hdr->ramdisksize;

	if (ramdisk_offset != ramdisk_load) {
		pr_info("Move ramdisk (len: %"PRIu64") from 0x%"PRIx64" to 0x%"PRIx64
				"\n", ramdisk_size, ramdisk_offset, ramdisk_load);
		memmove((void *)((uintptr_t)ramdisk_load), (void *)((uintptr_t)ramdisk_offset), ramdisk_size);
	}

	bootimg_cmdline = (char *)hdr->cmdline;

	return err;
}

#if !defined(CONFIG_DT_SUPPORT)
static tegrabl_error_t extract_kernel_dtb(void **kernel_dtb, void *kernel_dtbo)
{
	return TEGRABL_NO_ERROR;
}
#else
static tegrabl_error_t extract_kernel_dtb(void **kernel_dtb, void *kernel_dtbo)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	err = tegrabl_dt_create_space(*kernel_dtb, FDT_SIZE_BL_DT_NODES, DTB_MAX_SIZE);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* Save dtb handle */
	err = tegrabl_dt_set_fdt_handle(TEGRABL_DT_KERNEL, *kernel_dtb);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = tegrabl_linuxboot_update_dtb(*kernel_dtb);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_debug("kernel-dtbo @ %p\n", kernel_dtbo);
#if defined(CONFIG_ENABLE_DTB_OVERLAY)
	err = tegrabl_dtb_overlay(kernel_dtb, kernel_dtbo);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Booting with default kernel-dtb!!!\n");
		err = TEGRABL_NO_ERROR;
	}
#endif

fail:
	return err;
}
#endif /* end of CONFIG_DT_SUPPORT */

static tegrabl_error_t tegrabl_load_from_fixed_storage(bool load_from_storage,
													   void **boot_img_load_addr,
													   void **dtb_load_addr,
													   void **kernel_dtbo,
													   void *data)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	TEGRABL_UNUSED(kernel_dtbo);

	/* Load boot image */
	if (load_from_storage) {
		pr_info("Loading kernel/boot.img from built-in storage ...\n");
		err = tegrabl_load_binary(TEGRABL_BINARY_KERNEL, boot_img_load_addr, NULL);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

	} else {
		pr_info("Loading kernel/boot.img from memory ...\n");
		if (!data) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
			pr_error("Found no kernel in memory\n");
			goto fail;
		}
		*boot_img_load_addr = data;
	}


	/* Load kernel_dtb if not already loaded in memory */
#if defined(CONFIG_DT_SUPPORT)
	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_KERNEL, dtb_load_addr);
	if ((err != TEGRABL_NO_ERROR) || (*dtb_load_addr == NULL)) {

		*dtb_load_addr = (void *)tegrabl_get_dtb_load_addr();
		pr_trace("%u: dtb load addr: %p\n", __LINE__, *dtb_load_addr);

		err = tegrabl_load_binary(TEGRABL_BINARY_KERNEL_DTB, dtb_load_addr, NULL);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
#endif /* CONFIG_DT_SUPPORT */

#if defined(CONFIG_DT_SUPPORT)
#if defined(CONFIG_ENABLE_DTB_OVERLAY)
		/* kernel_dtbo should also be protected by verified boot */
		*kernel_dtbo = tegrabl_malloc(KERNEL_DTBO_PART_SIZE);
		if (!*kernel_dtbo) {
			pr_error("Failed to allocate memory\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			return err;
		}

		err = tegrabl_load_binary(TEGRABL_BINARY_KERNEL_DTBO, kernel_dtbo, NULL);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error %u loading kernel-dtbo\n", err);
			goto fail;
		}
		pr_info("kernel DTBO @ %p\n", kernel_dtbo);
#endif /* CONFIG_ENABLE_DTB_OVERLAY */

#else
		*dtb_load_addr = NULL;
#endif /* CONFIG_DT_SUPPORT */

fail:
	return err;
}

#if defined(CONFIG_ENABLE_BOOT_DEVICE_SELECT)
static tegrabl_error_t tegrabl_load_from_removable_storage(struct tegrabl_bdev *bdev,
														   void **boot_img_load_addr,
														   void **dtb_load_addr)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	err = tegrabl_load_binary_bdev(TEGRABL_BINARY_KERNEL, boot_img_load_addr, NULL, bdev);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("\nError (0x%x) loading kernel\n", err);
		goto fail;
	}

	*dtb_load_addr = (void *)tegrabl_get_dtb_load_addr();

	err = tegrabl_load_binary_bdev(TEGRABL_BINARY_KERNEL_DTB, dtb_load_addr, NULL, bdev);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("\nError (0x%x) loading kernel-dtb\n", err);
		goto fail;
	}

	err = tegrabl_dt_set_fdt_handle(TEGRABL_DT_KERNEL, *dtb_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("\nError (0x%x) initializing kernel\n", err);
		goto fail;
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_load_kernel_and_dtb(struct tegrabl_kernel_bin *kernel,
											void **kernel_entry_point,
											void **kernel_dtb,
											struct tegrabl_kernel_load_callbacks *callbacks,
											void *data)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *kernel_dtbo = NULL;
	struct tegrabl_bdev *bdev = NULL;
	bool is_load_done = false;
	bool boot_from_builtin = false;
	uint32_t i = 0;
	uint8_t device_instance = 0;
	uint8_t device_type = 0;
	struct tegrabl_device_config_params device_config = {0};
	struct tegrabl_partition kernel_partition = {0};
	struct tegrabl_partition kernel_dtb_partition = {0};
	int8_t *boot_order;
	void *boot_img_load_addr = NULL;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if (!kernel_entry_point || !kernel_dtb) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Get boot order from cbo.dtb */
	boot_order = tegrabl_get_boot_order();
	if (boot_order == NULL) {
		boot_order = g_boot_order;
		pr_debug("%s: using default boot order\n", __func__);
	} else {
		pr_debug("%s: using boot order from CBO Partition\n", __func__);
	}

	/* Try loading boot image and dtb from devices as per boot order */
	for (i = 0; (boot_order[i] != BOOT_DEFAULT) && (!is_load_done) && (!boot_from_builtin); i++) {

		if (boot_order[i] == BOOT_INVALID) {
			continue;
		}

		switch (boot_order[i]) {

#if defined(CONFIG_ENABLE_ETHERNET_BOOT)
		case BOOT_FROM_NETWORK:
			pr_info("Loading kernel & kernel-dtb from network ...\n");
			if (net_boot_stack_init() != TEGRABL_NO_ERROR) {
				pr_error("Error (%u) network stack init\n", err);
				continue;
			}
			err = net_boot_load_kernel_images(&boot_img_load_addr, kernel_dtb);
			if (err == TEGRABL_NO_ERROR) {
				is_load_done = true;
			} else {
				pr_error("Error (%u) network load failed for kernel & kernel-dtb\n", err);
			}
			break;
#endif

		case BOOT_FROM_SD:
		case BOOT_FROM_USB:
			if (boot_order[i] == BOOT_FROM_SD) {
				device_type = TEGRABL_STORAGE_SDCARD;
				device_instance = 0;
			} else if ((boot_order[i] == BOOT_FROM_USB)) {
				device_type = TEGRABL_STORAGE_USB_MS;
				device_instance = 0;
			}

			pr_info("Loading kernel & kernel-dtb from removable storage (%u)\n", device_type);
			/* Initialize device */
			err = init_storage_device(&device_config, device_type, device_instance);
			if (err != TEGRABL_NO_ERROR) {
				pr_warn("Failed to initialize device %u-%u\n", device_type, device_instance);
				continue;
			}

			/* Publish partitions only for removable storage devices */
			bdev = tegrabl_blockdev_open(device_type, device_instance);
			if (bdev == NULL) {
				continue;
			}
			tegrabl_partition_publish(bdev, 0);

			/* Find if kernel partition is present and on which device */
			err = tegrabl_partition_lookup_bdev("kernel", &kernel_partition, bdev);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("Error: kernel partition not found\n");
				tegrabl_partitions_unpublish(bdev);
				continue;
			}

			/* Find if kernel-dtb partition is present and on which device */
			err = tegrabl_partition_lookup_bdev("kernel-dtb", &kernel_dtb_partition, bdev);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("Error: kernel-dtb partition not found\n");
				tegrabl_partitions_unpublish(bdev);
				continue;
			}

			err = tegrabl_load_from_removable_storage(bdev, &boot_img_load_addr, kernel_dtb);
			if (err == TEGRABL_NO_ERROR) {
				is_load_done = true;
			} else {
				pr_error("Error (%u) removable storage load failed for kernel & kernel-dtb\n", err);
				tegrabl_partitions_unpublish(bdev);
			}

			break;

		case BOOT_FROM_BUILTIN_STORAGE:
		default:
			boot_from_builtin = true;
			break;
		}
	}

	/* try builtin, if not already done & if booting from other options failed */
	if (boot_from_builtin) {
		err = tegrabl_load_from_fixed_storage(kernel->load_from_storage, &boot_img_load_addr, kernel_dtb,
											  &kernel_dtbo, data);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error (%u) builtin load failed for kernel & kernel-dtb\n", err);
			goto fail;
		}
		is_load_done = true;
	}

	if (is_load_done == false) {
		goto fail;
	}

	pr_info("Kernel hdr @%p\n", boot_img_load_addr);
	pr_info("Kernel dtb @%p\n", *kernel_dtb);

	if (callbacks != NULL && callbacks->verify_boot != NULL) {
		callbacks->verify_boot(boot_img_load_addr, *kernel_dtb, kernel_dtbo);
	}

	err = extract_kernel(boot_img_load_addr, kernel_entry_point);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error (%u) extracting the kernel\n", err);
		goto fail;
	}

	err = extract_ramdisk(boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error (%u) extracting the ramdisk\n", err);
		goto fail;
	}

	err = extract_kernel_dtb(kernel_dtb, kernel_dtbo);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error (%u) extracting the kernel DTB\n", err);
		goto fail;
	}

	pr_debug("%s: Done\n", __func__);

fail:
	tegrabl_free(kernel_dtbo);
	err = tegrabl_usbh_close();

	return err;
}

#else
tegrabl_error_t tegrabl_load_kernel_and_dtb(struct tegrabl_kernel_bin *kernel,
											void **kernel_entry_point,
											void **kernel_dtb,
											struct tegrabl_kernel_load_callbacks *callbacks,
											void *data)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *kernel_dtbo = NULL;
	void *boot_img_load_addr = NULL;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if (!kernel_entry_point || !kernel_dtb) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	err = tegrabl_load_from_fixed_storage(kernel->load_from_storage, &boot_img_load_addr, kernel_dtb,
										  &kernel_dtbo, data);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error (%u) builtin load failed for kernel & kernel-dtb\n", err);
		goto fail;
	}

	pr_info("Kernel hdr @%p\n", boot_img_load_addr);
	pr_info("Kernel dtb @%p\n", *kernel_dtb);

	if (callbacks != NULL && callbacks->verify_boot != NULL) {
		callbacks->verify_boot(boot_img_load_addr, *kernel_dtb, kernel_dtbo);
	}

	err = extract_kernel(boot_img_load_addr, kernel_entry_point);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the kernel\n", err);
		goto fail;
	}

	err = extract_ramdisk(boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the ramdisk\n", err);
		goto fail;
	}

	err = extract_kernel_dtb(kernel_dtb, kernel_dtbo);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the kernel DTB\n", err);
		goto fail;
	}

	pr_debug("%s: Done\n", __func__);

fail:
	tegrabl_free(kernel_dtbo);

	return err;
}
#endif
