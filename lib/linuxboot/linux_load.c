/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All Rights Reserved.
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
#include <tegrabl_sdram_usage.h>
#include <linux_load.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_partition_loader.h>
#include <tegrabl_decompress.h>

static uint64_t ramdisk_load;
static uint64_t ramdisk_size;
static char *bootimg_cmdline;

/* maximum possible uncompressed kernel image size--60M */
#define MAX_KERNEL_IMAGE_SIZE (1024 * 1024 * 60)

#define FDT_SIZE_BL_DT_NODES (4048 + 4048)
static tegrabl_error_t fdt_create_space(void *fdt)
{
	uint32_t newlen;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int retval;

	newlen = fdt_totalsize(fdt) + FDT_SIZE_BL_DT_NODES;
	retval = fdt_open_into(fdt, fdt, newlen);
	if (retval < 0) {
		pr_error("fdt_open_into fail (%s)\n", fdt_strerror(retval));
		err = tegrabl_error_value(TEGRABL_ERR_LINUXBOOT, 0,
								  TEGRABL_ERR_DT_EXPAND_FAILED);
	}
	return err;
}

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

/* Extract kernel from an Android boot image, and return the address where it
 * is installed in memory
 */
static tegrabl_error_t extract_kernel(union tegrabl_bootimg_header *hdr,
								   void **kernel_entry_point)
{
	void *kernel_load = NULL;
	uint64_t kernel_offset = 0; /* Offset of 1st kernel byte in boot.img */
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t hdr_crc = 0;
	bool is_compressed = false;
	decompressor *decomp = NULL;
	uint32_t decomp_size = 0; /* kernel size after decompressing */

	kernel_offset = hdr->pagesize;

	err = validate_kernel(hdr, &hdr_crc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u failed to validate kernel\n", err);
		return err;
	}

	if (hdr_crc)
		kernel_load = (char *)0x80000000 + hdr->kerneladdr;
	else
		kernel_load = (char *)LINUX_LOAD_ADDRESS;

	is_compressed = is_compressed_content((uint8_t *)hdr + kernel_offset,
										  &decomp);

	if (!is_compressed) {
		pr_info("Copying kernel image (%u bytes) from %p to %p ... ",
				hdr->kernelsize, (char *)hdr + kernel_offset, kernel_load);
		memmove(kernel_load, (char *)hdr + kernel_offset, hdr->kernelsize);
	} else {
		pr_info("Decompressing kernel image (%u bytes) from %p to %p ... ",
				hdr->kernelsize, (char *)hdr + kernel_offset, kernel_load);

		decomp_size = MAX_KERNEL_IMAGE_SIZE;
		err = do_decompress(decomp, (uint8_t *)hdr + kernel_offset,
							hdr->kernelsize, kernel_load, &decomp_size);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("\nError %d decompress kernel\n", err);
			return err;
		}
	}

	pr_info("Done\n");

	*kernel_entry_point = kernel_load;

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t extract_ramdisk(union tegrabl_bootimg_header *hdr)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint64_t ramdisk_offset = (uint64_t)NULL; /* Offset of 1st ramdisk byte in boot.img */

	ramdisk_offset = ROUND_UP_POW2(hdr->pagesize + hdr->kernelsize,
								   hdr->pagesize);

	ramdisk_offset = (uintptr_t)hdr + ramdisk_offset;
	ramdisk_load = RAMDISK_ADDRESS;
	ramdisk_size = hdr->ramdisksize;
	if (ramdisk_offset != ramdisk_load) {
		pr_info("Move ramdisk (len: %"PRIu64") from 0x%"PRIx64" to 0x%"PRIx64
				"\n", ramdisk_size, ramdisk_offset, ramdisk_load);
		memmove((void *)((uintptr_t)ramdisk_load),
				(void *)((uintptr_t)ramdisk_offset), ramdisk_size);
	}
	bootimg_cmdline = (char *)hdr->cmdline;

	return err;
}

#if !defined(CONFIG_DT_SUPPORT)
static tegrabl_error_t extract_kernel_dtb(void **kernel_dtb)
{
	return TEGRABL_NO_ERROR;
}
#else
static tegrabl_error_t extract_kernel_dtb(void **kernel_dtb)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = fdt_create_space(*kernel_dtb);
	if (err != TEGRABL_NO_ERROR) {
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

fail:
	return err;
}
#endif /* end of CONFIG_DT_SUPPORT */

tegrabl_error_t tegrabl_load_kernel_and_dtb(
			struct tegrabl_kernel_bin *kernel,
			void **kernel_entry_point,
			void **kernel_dtb,
			struct tegrabl_kernel_load_callbacks *callbacks,
			void *data)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	union tegrabl_bootimg_header *hdr = (void *)((uintptr_t)0xDEADDEA0);

	if (!kernel_entry_point || !kernel_dtb) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (!kernel->load_from_storage) {
		pr_info("Loading kernel/boot.img from memory ...\n");
		if (!data) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
			pr_error("Found no kernel in memory\n");
			goto fail;
		}
		hdr = data;
		goto load_dtb;
	}

	pr_info("Loading kernel/boot.img from storage ...\n");
#if !defined(CONFIG_BACKDOOR_LOAD)
	err = tegrabl_load_binary(kernel->bin_type, (void **)&hdr, NULL);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Kernel loading failed\n");
		goto fail;
	}
#else
	hdr = (void *)((uintptr_t)BOOT_IMAGE_LOAD_ADDRESS);
#endif

load_dtb:
	/* Load kernel_dtb early since it is used in verified boot */
#if defined(CONFIG_DT_SUPPORT)
#if !defined(CONFIG_BACKDOOR_LOAD)
	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_KERNEL, kernel_dtb);
	/* Load kernel_dtb if not already loaded in memory */
	if ((err != TEGRABL_NO_ERROR) || (*kernel_dtb == NULL)) {
		err = tegrabl_load_binary(TEGRABL_BINARY_KERNEL_DTB, kernel_dtb, NULL);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Kernel-dtb loading failed\n");
			goto fail;
		}
	}
#else
	*kernel_dtb = (void *)((uintptr_t)DTB_LOAD_ADDRESS);
#endif /* CONFIG_BACKDOOR_LOAD */
#else
	*kernel_dtb = NULL;
#endif /* CONFIG_DT_SUPPORT */

	pr_info("Kernel DTB @ %p\n", *kernel_dtb);

	if (callbacks != NULL && callbacks->verify_boot != NULL)
		callbacks->verify_boot(hdr, *kernel_dtb);

	err = extract_kernel(hdr, kernel_entry_point);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the kernel\n", err);
		goto fail;
	}

	err = extract_ramdisk(hdr);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the ramdisk\n", err);
		goto fail;
	}

	err = extract_kernel_dtb(kernel_dtb);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the kernel DTB\n", err);
		goto fail;
	}

	pr_info("%s: Done\n", __func__);

fail:
	return err;
}
