/*
 * Copyright (c) 2016-2019, NVIDIA Corporation.  All Rights Reserved.
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
#include <tegrabl_decompress.h>
#include <tegrabl_malloc.h>
#include <dtb_overlay.h>
#include <tegrabl_cbo.h>
#include <tegrabl_usbh.h>
#include <tegrabl_cpubl_params.h>
#include <tegrabl_file_manager.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_exit.h>
#include <tegrabl_linuxboot_utils.h>
#include <fixed_boot.h>
#if defined(CONFIG_ENABLE_USB_SD_BOOT)
#include <usb_sd_boot.h>
#endif
#if defined(CONFIG_ENABLE_ETHERNET_BOOT)
#include <net_boot.h>
#endif
#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
#include <extlinux_boot.h>
#endif

#define FDT_SIZE_BL_DT_NODES	(4048 + 4048)

static uint64_t ramdisk_load;
static uint64_t ramdisk_size;
static char *bootimg_cmdline;

#if defined(CONFIG_OS_IS_ANDROID)
static union tegrabl_bootimg_header *android_hdr;
#endif

#define HAS_BOOT_IMG_HDR(ptr)	\
			((memcmp((ptr)->magic, ANDROID_MAGIC, ANDROID_MAGIC_SIZE) == 0) ? true : false)

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

#ifdef CONFIG_OS_IS_ANDROID
tegrabl_error_t tegrabl_get_os_version(union android_os_version *os_version)
{
	if (!android_hdr) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	*os_version = (union android_os_version)android_hdr->os_version;
	return TEGRABL_NO_ERROR;
}
#endif

/* Extract kernel from an Android boot image, and return the address where it is installed in memory */
static tegrabl_error_t extract_kernel(void *boot_img_load_addr,
									  uint32_t kernel_bin_size,
									  void **kernel_load_addr)
{
	bool is_compressed = false;
	decompressor *decomp = NULL;
	uint32_t decomp_size = 0; /* kernel size after decompressing */
	union tegrabl_bootimg_header *hdr = NULL;
	uint64_t payload_addr;
	uint32_t kernel_size;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	hdr = (union tegrabl_bootimg_header *)boot_img_load_addr;
	if (HAS_BOOT_IMG_HDR(hdr)) {
		/* Get kernel addr and size from boot img header */
		payload_addr = (uintptr_t)hdr + hdr->pagesize;
		kernel_size = hdr->kernelsize;
	} else {  /*  In extinux boot, raw kernel image gets loaded */
		payload_addr = (uintptr_t)boot_img_load_addr;
		kernel_size = kernel_bin_size;
	}

	/* Sanity check */
	if (kernel_size > MAX_KERNEL_IMAGE_SIZE) {
		pr_error("Kernel size (0x%08x) is greater than allocated size (0x%08x)\n",
				 kernel_size, MAX_KERNEL_IMAGE_SIZE);
		err = TEGRABL_ERROR(TEGRABL_ERR_TOO_LARGE, 0);
		goto fail;
	}

	*kernel_load_addr = (void *)tegrabl_get_kernel_load_addr();
	is_compressed = is_compressed_content((uint8_t *)payload_addr, &decomp);
	if (!is_compressed) {
		pr_info("Copying kernel image (%u bytes) from %p to %p ... ",
				kernel_size, (char *)payload_addr, *kernel_load_addr);
		memmove(*kernel_load_addr, (char *)payload_addr, kernel_size);
	} else {
		pr_info("Decompressing kernel image (%u bytes) from %p to %p ... ",
				kernel_size, (char *)payload_addr, *kernel_load_addr);
		decomp_size = MAX_KERNEL_IMAGE_SIZE;
		err = do_decompress(decomp, (uint8_t *)payload_addr, kernel_size, *kernel_load_addr, &decomp_size);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("\nError %d decompress kernel\n", err);
			return err;
		}
	}

	pr_info("Done\n");

fail:
	return err;
}

static tegrabl_error_t extract_ramdisk(void *boot_img_load_addr)
{
	union tegrabl_bootimg_header *hdr = NULL;
	uint64_t ramdisk_offset = (uint64_t)NULL; /* Offset of 1st ramdisk byte in boot.img */
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	ramdisk_load = tegrabl_get_ramdisk_load_addr();
	pr_trace("%u: ramdisk load addr: 0x%"PRIx64"\n", __LINE__, ramdisk_load);

	/* Sanity check */
	hdr = (union tegrabl_bootimg_header *)boot_img_load_addr;
	if (hdr->ramdisksize > RAMDISK_MAX_SIZE) {
		pr_error("Ramdisk size (0x%08x) is greater than allocated size (0x%08x)\n",
				 hdr->ramdisksize, RAMDISK_MAX_SIZE);
		err = TEGRABL_ERROR(TEGRABL_ERR_TOO_LARGE, 1);
		goto fail;
	}

	ramdisk_offset = ROUND_UP_POW2(hdr->pagesize + hdr->kernelsize, hdr->pagesize);
	ramdisk_offset = (uintptr_t)hdr + ramdisk_offset;
	ramdisk_size = hdr->ramdisksize;

	if (ramdisk_offset != ramdisk_load) {
		pr_info("Move ramdisk (len: %"PRIu64") from 0x%"PRIx64" to 0x%"PRIx64
				"\n", ramdisk_size, ramdisk_offset, ramdisk_load);
		memmove((void *)((uintptr_t)ramdisk_load), (void *)((uintptr_t)ramdisk_offset), ramdisk_size);
	}

	bootimg_cmdline = (char *)hdr->cmdline;

fail:
	return err;
}

#if defined(CONFIG_DT_SUPPORT)
static tegrabl_error_t extract_kernel_dtb(void **kernel_dtb, void *kernel_dtbo)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	TEGRABL_UNUSED(kernel_dtbo);

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

#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
	if (extlinux_boot_get_status()) {
		err = extlinux_boot_update_bootargs(*kernel_dtb);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
#endif

	pr_trace("kernel-dtbo @ %p\n", kernel_dtbo);
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
#else  /* CONFIG_DT_SUPPORT */
static tegrabl_error_t extract_kernel_dtb(void **kernel_dtb, void *kernel_dtbo)
{
	TEGRABL_UNUSED(kernel_dtb);
	TEGRABL_UNUSED(kernel_dtbo);
	return TEGRABL_NO_ERROR;
}
#endif  /* CONFIG_DT_SUPPORT */

#if defined(CONFIG_ENABLE_BOOT_DEVICE_SELECT)
tegrabl_error_t tegrabl_load_kernel_and_dtb(struct tegrabl_kernel_bin *kernel,
											void **kernel_entry_point,
											void **kernel_dtb,
											struct tegrabl_kernel_load_callbacks *callbacks,
											void *data,
											uint32_t data_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *kernel_dtbo = NULL;
	bool is_load_done = false;
	uint32_t i = 0;
	uint8_t *boot_order;
	void *boot_img_load_addr = NULL;
	void *ramdisk_load_addr = NULL;
	uint32_t kernel_size = 0;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if (!kernel_entry_point || !kernel_dtb) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Get boot order from cbo.dtb */
	boot_order = tegrabl_get_boot_order();

	/* Try loading boot image and dtb from devices as per boot order */
	for (i = 0; (boot_order[i] != BOOT_DEFAULT) && (!is_load_done); i++) {

		switch (boot_order[i]) {

#if defined(CONFIG_ENABLE_ETHERNET_BOOT)
		case BOOT_FROM_NETWORK:
			err = net_boot_load_kernel_and_dtb(&boot_img_load_addr, kernel_dtb);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("Net boot failed, err: %u\n", err);
				continue;
			}
			is_load_done = true;
			break;
#endif

#if defined(CONFIG_ENABLE_USB_SD_BOOT)
		case BOOT_FROM_SD:
		case BOOT_FROM_USB:
			err = usb_sd_boot_load_kernel_and_dtb(boot_order[i],
												  &boot_img_load_addr,
												  kernel_dtb,
												  &ramdisk_load_addr,
												  &kernel_size,
												  &ramdisk_size);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("%s boot failed, err: %u\n", boot_order[i] == BOOT_FROM_SD ? "SD" : "USB", err);
				continue;
			}
			is_load_done = true;
			break;
#endif

		case BOOT_FROM_BUILTIN_STORAGE:
		default:
			err = fixed_boot_load_kernel_and_dtb(kernel,
												 &boot_img_load_addr,
												 kernel_dtb,
												 &kernel_dtbo,
												 &ramdisk_load_addr,
												 data,
												 data_size,
												 &kernel_size,
												 &ramdisk_size);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
			is_load_done = true;
			break;
		}
	}

	/* Boot from storage, if not already tried or if booting from all other options failed */
	if (!is_load_done) {
		err = fixed_boot_load_kernel_and_dtb(kernel,
											 &boot_img_load_addr,
											 kernel_dtb,
											 &kernel_dtbo,
											 &ramdisk_load_addr,
											 data,
											 data_size,
											 &kernel_size,
											 &ramdisk_size);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	ramdisk_load = (uintptr_t)ramdisk_load_addr;

	pr_info("Kernel hdr @%p\n", boot_img_load_addr);
	pr_info("Kernel dtb @%p\n", *kernel_dtb);

	if (callbacks != NULL && callbacks->verify_boot != NULL) {
		callbacks->verify_boot(boot_img_load_addr, *kernel_dtb, kernel_dtbo);
	}

	err = extract_kernel(boot_img_load_addr, kernel_size, kernel_entry_point);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error (%u) extracting the kernel\n", err);
		goto fail;
	}

	if (HAS_BOOT_IMG_HDR((union tegrabl_bootimg_header *)boot_img_load_addr)) {
		err = extract_ramdisk(boot_img_load_addr);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error (%u) extracting the ramdisk\n", err);
			goto fail;
		}
	}

	err = extract_kernel_dtb(kernel_dtb, kernel_dtbo);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error (%u) extracting the kernel DTB\n", err);
		goto fail;
	}

	pr_info("%s: Done\n", __func__);

fail:
	tegrabl_free(kernel_dtbo);
	tegrabl_usbh_close();

	return err;
}

#else  /* CONFIG_ENABLE_BOOT_DEVICE_SELECT */

tegrabl_error_t tegrabl_load_kernel_and_dtb(struct tegrabl_kernel_bin *kernel,
											void **kernel_entry_point,
											void **kernel_dtb,
											struct tegrabl_kernel_load_callbacks *callbacks,
											void *data,
											uint32_t data_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *kernel_dtbo = NULL;
	void *boot_img_load_addr = NULL;
	void *ramdisk_load_addr = NULL;
	uint32_t kernel_size = 0;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if (!kernel_entry_point || !kernel_dtb) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	err = fixed_boot_load_kernel_and_dtb(kernel,
										 &boot_img_load_addr,
										 kernel_dtb,
										 &kernel_dtbo,
										 &ramdisk_load_addr,
										 data,
										 data_size,
										 &kernel_size,
										 &ramdisk_size);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error (%u) builtin kernel/dtb load failed\n", err);
		goto fail;
	}

#if defined(CONFIG_OS_IS_ANDROID)
	android_hdr = boot_img_load_addr;
#endif

	pr_info("Kernel hdr @%p\n", boot_img_load_addr);
	pr_info("Kernel dtb @%p\n", *kernel_dtb);

	if (callbacks != NULL && callbacks->verify_boot != NULL) {
		callbacks->verify_boot(boot_img_load_addr, *kernel_dtb, kernel_dtbo);
	}

	err = extract_kernel(boot_img_load_addr, kernel_size, kernel_entry_point);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the kernel\n", err);
		goto fail;
	}

	if (HAS_BOOT_IMG_HDR((union tegrabl_bootimg_header *)boot_img_load_addr)) {
		err = extract_ramdisk(boot_img_load_addr);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error %u loading the ramdisk\n", err);
			goto fail;
		}
	}

	err = extract_kernel_dtb(kernel_dtb, kernel_dtbo);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u loading the kernel DTB\n", err);
		goto fail;
	}

	pr_info("%s: Done\n", __func__);

fail:
	tegrabl_free(kernel_dtbo);

	return err;
}
#endif  /* CONFIG_ENABLE_BOOT_DEVICE_SELECT */
