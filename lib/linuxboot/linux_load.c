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
#include <tegrabl_auth.h>
#include <tegrabl_exit.h>
#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
#include <extlinux_boot.h>
#endif
#if defined(CONFIG_OS_IS_L4T)
#include <tegrabl_auth.h>
#endif

#if defined(CONFIG_ENABLE_BOOT_DEVICE_SELECT)
#include <config_storage.h>
#endif

#if defined(CONFIG_ENABLE_ETHERNET_BOOT)
#include <net_boot.h>
#endif

#define KERNEL_IMAGE			"boot.img"
#define KERNEL_DTB				"tegra194-p2888-0001-p2822-0000.dtb"
#define KERNEL_DTBO_PART_SIZE	(1024 * 1024 * 1)
#define FDT_SIZE_BL_DT_NODES	(4048 + 4048)

static uint64_t ramdisk_load;
static uint64_t ramdisk_size;
static char *bootimg_cmdline;

#if defined(CONFIG_OS_IS_ANDROID)
static union tegrabl_bootimg_header *android_hdr;
#endif

static uint32_t kernel_size;
#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
static uint32_t kernel_dtb_size;
static struct conf extlinux_conf;
static uint32_t boot_entry;
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

static tegrabl_error_t verify_boot_img_header(union tegrabl_bootimg_header *hdr, uint32_t img_size)
{
	uint32_t hdr_size;
	uint64_t hdr_size_fields_sum;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* Check if the binary has header */
	if (memcmp(hdr->magic, ANDROID_MAGIC, ANDROID_MAGIC_SIZE)) {
		/* This is raw kernel image, so skip header sanity checks */
		goto fail;
	}

	if (hdr->pagesize < sizeof(union tegrabl_bootimg_header)) {
		pr_error("Page size field (0x%08x) is less than header structure size (0x%08lx)\n",
				 hdr->pagesize, sizeof(union tegrabl_bootimg_header));
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	hdr_size = hdr->pagesize;
	hdr_size_fields_sum = hdr_size + hdr->kernelsize + hdr->ramdisksize + hdr->secondsize;
	if (hdr_size_fields_sum > img_size) {
		pr_error("Header size fields (0x%016lx) is greater than actual binary size (0x%08x)\n",
				 hdr_size_fields_sum, img_size);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

fail:
	return err;
}

/* Extract kernel from an Android boot image, and return the address where it is installed in memory */
static tegrabl_error_t extract_kernel(void *boot_img_load_addr, void **kernel_load_addr)
{
	uint32_t hdr_crc = 0;
	bool is_compressed = false;
	decompressor *decomp = NULL;
	uint32_t decomp_size = 0; /* kernel size after decompressing */
	union tegrabl_bootimg_header *hdr = NULL;
	uint64_t payload_addr;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	TEGRABL_UNUSED(hdr);
	TEGRABL_UNUSED(hdr_crc);

#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
	payload_addr = (uintptr_t)boot_img_load_addr;
	/* kernel_size variable gets set in tegrabl_load_from_fixed_storage() */
#else
	hdr = (union tegrabl_bootimg_header *)boot_img_load_addr;
	payload_addr = (uintptr_t)hdr + hdr->pagesize;
	kernel_size = hdr->kernelsize;
	err = validate_kernel(hdr, &hdr_crc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u failed to validate kernel\n", err);
		return err;
	}
#endif

	*kernel_load_addr = (void *)tegrabl_get_kernel_load_addr();
	pr_trace("%u: kernel load addr: %p\n", __LINE__, *kernel_load_addr);

	/* Sanity check */
	if (kernel_size > MAX_KERNEL_IMAGE_SIZE) {
		pr_error("Kernel size (0x%08x) is greater than allocated size (0x%08x)\n",
				 hdr->kernelsize, MAX_KERNEL_IMAGE_SIZE);
		err = TEGRABL_ERROR(TEGRABL_ERR_TOO_LARGE, 0);
		goto fail;
	}

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
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	ramdisk_load = tegrabl_get_ramdisk_load_addr();
	pr_trace("%u: ramdisk load addr: 0x%"PRIx64"\n", __LINE__, ramdisk_load);

#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
	struct tegrabl_fm_handle *fm_handle = tegrabl_file_manager_get_handle();
	void *load_addr = (void *)ramdisk_load;
	uint32_t file_size = RAMDISK_MAX_SIZE;

	if (extlinux_conf.section[boot_entry].initrd_path == NULL) {
		pr_warn("Ramdisk image path not found\n");
		goto fail;
	}

	pr_info("Loading ramdisk ...\n");
	err = tegrabl_fm_read(fm_handle,
						  extlinux_conf.section[boot_entry].initrd_path,
						  NULL,
						  &load_addr,
						  &file_size,
						  NULL);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	ramdisk_size = file_size;

#else
	union tegrabl_bootimg_header *hdr = NULL;
	uint64_t ramdisk_offset = (uint64_t)NULL; /* Offset of 1st ramdisk byte in boot.img */
	hdr = (union tegrabl_bootimg_header *)boot_img_load_addr;

	/* Sanity check */
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
#endif

fail:
	return err;
}

#if !defined(CONFIG_DT_SUPPORT)
static tegrabl_error_t extract_kernel_dtb(void **kernel_dtb, void *kernel_dtbo)
{
	TEGRABL_UNUSED(kernel_dtb);
	TEGRABL_UNUSED(kernel_dtbo);
	return TEGRABL_NO_ERROR;
}
#else
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
	err = tegrabl_linuxboot_update_bootargs(*kernel_dtb, extlinux_conf.section[boot_entry].boot_args);
	if (err != 0) {
		pr_warn("Failed to update DTB bootargs with extlinux.conf\n");
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
#endif /* end of CONFIG_DT_SUPPORT */

#if defined(CONFIG_ENABLE_BOOT_DEVICE_SELECT)
#if defined(CONFIG_OS_IS_L4T)
static tegrabl_error_t tegrabl_validate_binary(uint32_t bin_type, uint32_t bin_max_size, void *load_addr)
{
	char *bin_name;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if (!tegrabl_do_ratchet_check(bin_type, load_addr)) {
		goto fail;
	}

	if (bin_type == TEGRABL_BINARY_KERNEL) {
		bin_name = "kernel";
	} else if (bin_type == TEGRABL_BINARY_KERNEL_DTB) {
		bin_name = "kernel-dtb";
	} else {
		bin_name = "";
	}
	err = tegrabl_auth_payload(bin_type, bin_name, load_addr, bin_max_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return err;
}
#endif
#endif

static tegrabl_error_t tegrabl_load_from_fixed_storage(bool load_from_storage,
													   void **boot_img_load_addr,
													   void **dtb_load_addr,
													   void **kernel_dtbo,
													   void *data,
													   uint32_t data_size,
													   uint32_t *boot_img_size)
{
	uint32_t file_size;
	bool is_file_loaded_from_fs = false;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	TEGRABL_UNUSED(kernel_dtbo);
	TEGRABL_UNUSED(file_size);
	TEGRABL_UNUSED(is_file_loaded_from_fs);

	/* Load boot image from memory */
	if (!load_from_storage) {
		pr_info("Loading kernel from memory ...\n");
		if (!data) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
			pr_error("Found no kernel in memory\n");
			goto fail;
		}
		*boot_img_load_addr = data;
		*boot_img_size = data_size;
		goto boot_image_load_done;
	}

	/* Load and parse extlinux.conf */
#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
	uint8_t device_instance;
	uint8_t device_type;
	struct tegrabl_bdev *bdev = NULL;
	struct tegrabl_fm_handle *fm_handle;

	device_type = TEGRABL_STORAGE_SDMMC_USER;		/* TODO: remove hardcoding */
	device_instance = 3;							/* TODO: remove hardcoding */
	bdev = tegrabl_blockdev_open(device_type, device_instance);
	if (bdev == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_OPEN_FAILED, 0);
		goto fail;
	}
	err = tegrabl_fm_publish(bdev, &fm_handle);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u\n", err);
		goto fail;
	}

	err = get_boot_details(fm_handle, &extlinux_conf, &boot_entry);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
#endif

#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
	/* Get load address for boot image and dtb */
	err = tegrabl_get_boot_img_load_addr(boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	file_size = BOOT_IMAGE_MAX_SIZE;
	pr_info("Loading kernel from storage ...\n");
	err = tegrabl_fm_read(fm_handle,
						  extlinux_conf.section[boot_entry].linux_path,
						  NULL,
						  boot_img_load_addr,
						  &file_size,
						  &is_file_loaded_from_fs);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	kernel_size = file_size;
#else
	pr_info("Loading kernel from partition ...\n");
	err = tegrabl_load_binary(TEGRABL_BINARY_KERNEL, boot_img_load_addr, boot_img_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
#endif

#if defined(CONFIG_ENABLE_BOOT_DEVICE_SELECT)
#if defined(CONFIG_OS_IS_L4T)
	if (!is_file_loaded_from_fs) {
		err = tegrabl_validate_binary(TEGRABL_BINARY_KERNEL, BOOT_IMAGE_MAX_SIZE, *boot_img_load_addr);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
#endif /* CONFIG_OS_IS_L4T */
#endif /* CONFIG_ENABLE_BOOT_DEVICE_SELECT */

boot_image_load_done:
	/* Load kernel_dtb if not already loaded in memory */
#if defined(CONFIG_DT_SUPPORT)
	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_KERNEL, dtb_load_addr);
	if ((err != TEGRABL_NO_ERROR) || (*dtb_load_addr == NULL)) {
#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
		pr_info("Loading dtb from storage ...\n");
		*dtb_load_addr = (void *)tegrabl_get_dtb_load_addr();
		file_size = DTB_MAX_SIZE;
		err = tegrabl_fm_read(fm_handle,
							  extlinux_conf.section[boot_entry].dtb_path,
							  "kernel-dtb",
							  dtb_load_addr,
							  &file_size,
							  &is_file_loaded_from_fs);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		kernel_dtb_size = file_size;
#else
		pr_info("Loading kernel-dtb from partition ...\n");
		err = tegrabl_load_binary(TEGRABL_BINARY_KERNEL_DTB, dtb_load_addr, NULL);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
#endif /* CONFIG_ENABLE_EXTLINUX_BOOT */

#if defined(CONFIG_ENABLE_BOOT_DEVICE_SELECT)
#if defined(CONFIG_OS_IS_L4T)
		if (!is_file_loaded_from_fs) {
			err = tegrabl_validate_binary(TEGRABL_BINARY_KERNEL_DTB, DTB_MAX_SIZE, *dtb_load_addr);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}
#endif /* CONFIG_OS_IS_L4T */
#endif /* CONFIG_ENABLE_BOOT_DEVICE_SELECT */
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
														   void **dtb_load_addr,
														   uint32_t *boot_img_size)
{
	struct tegrabl_fm_handle *fm_handle;
	uint32_t file_size;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	/* Get load address for boot image and dtb */
	err = tegrabl_get_boot_img_load_addr(boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	*dtb_load_addr = (void *)tegrabl_get_dtb_load_addr();

	pr_info("Loading kernel ...\n");
	fm_handle = tegrabl_file_manager_get_handle();
	file_size = BOOT_IMAGE_MAX_SIZE;
	err = tegrabl_fm_read(fm_handle, "/"KERNEL_IMAGE, "kernel", boot_img_load_addr, &file_size, NULL);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	*boot_img_size = file_size;
#if defined(CONFIG_OS_IS_L4T)
	err = tegrabl_validate_binary(TEGRABL_BINARY_KERNEL, BOOT_IMAGE_MAX_SIZE, *boot_img_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
#endif

	pr_info("Loading dtb ...\n");
	file_size = DTB_MAX_SIZE;
	err = tegrabl_fm_read(fm_handle, "/"KERNEL_DTB, "kernel-dtb", dtb_load_addr, &file_size, NULL);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
#if defined(CONFIG_OS_IS_L4T)
	err = tegrabl_validate_binary(TEGRABL_BINARY_KERNEL_DTB, DTB_MAX_SIZE, *dtb_load_addr);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
#endif

fail:
	return err;
}

tegrabl_error_t tegrabl_load_kernel_and_dtb(struct tegrabl_kernel_bin *kernel,
											void **kernel_entry_point,
											void **kernel_dtb,
											struct tegrabl_kernel_load_callbacks *callbacks,
											void *data,
											uint32_t data_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *kernel_dtbo = NULL;
	struct tegrabl_bdev *bdev = NULL;
	bool is_load_done = false;
	bool boot_from_builtin_done = false;
	uint32_t i = 0;
	uint8_t device_instance = 0;
	uint8_t device_type = 0;
	struct tegrabl_device_config_params device_config = {0};
	uint8_t *boot_order;
	void *boot_img_load_addr = NULL;
	struct tegrabl_fm_handle *fm_handle;
	uint32_t boot_img_size = 0;

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
			pr_info("Loading kernel & kernel-dtb from network ...\n");
			if (net_boot_stack_init() != TEGRABL_NO_ERROR) {
				pr_error("Error (%u) network stack init\n", err);
				continue;
			}
			err = net_boot_load_kernel_images(&boot_img_load_addr, kernel_dtb, &boot_img_size);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("Error (%u) network load failed for kernel & kernel-dtb\n", err);
				continue;
			}
#if defined(CONFIG_OS_IS_L4T)
			err = tegrabl_validate_binary(TEGRABL_BINARY_KERNEL, BOOT_IMAGE_MAX_SIZE, boot_img_load_addr);
			if (err != TEGRABL_NO_ERROR) {
				continue;
			}
			err = tegrabl_validate_binary(TEGRABL_BINARY_KERNEL_DTB, DTB_MAX_SIZE, *kernel_dtb);
			if (err != TEGRABL_NO_ERROR) {
				continue;
			}
#endif
			is_load_done = true;
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

			tegrabl_fm_publish(bdev, &fm_handle);
			err = tegrabl_load_from_removable_storage(bdev,
													  &boot_img_load_addr,
													  kernel_dtb,
													  &boot_img_size);
			if (err == TEGRABL_NO_ERROR) {
				is_load_done = true;
			} else {
				pr_error("Error (%u) removable storage load failed for kernel & kernel-dtb\n", err);
			}
			tegrabl_fm_close(fm_handle);
			err = tegrabl_partitions_unpublish(bdev);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("Failed to unpublish partitions from removable storage\n");
			}
			tegrabl_blockdev_close(bdev);
			break;

		case BOOT_FROM_BUILTIN_STORAGE:
		default:
			err = tegrabl_load_from_fixed_storage(kernel->load_from_storage,
												  &boot_img_load_addr,
												  kernel_dtb,
												  &kernel_dtbo,
												  data,
												  data_size,
												  &boot_img_size);
			if (err == TEGRABL_NO_ERROR) {
				is_load_done = true;
			} else {
				pr_error("Error (%u) builtin kernel/dtb load failed\n", err);
#if defined(CONFIG_ENABLE_A_B_SLOT)
				pr_error("A/B loader failure\n");
				pr_trace("Trigger SoC reset\n")
				tegrabl_reset();
				goto fail;
#endif
			}
			boot_from_builtin_done = true;
			break;
		}
	}

	if (!is_load_done) {
		/* try builtin, if not already tried or if booting from all other options failed */
		if (boot_from_builtin_done) {
			goto fail;
		}
		err = tegrabl_load_from_fixed_storage(kernel->load_from_storage,
											  &boot_img_load_addr,
											  kernel_dtb,
											  &kernel_dtbo,
											  data,
											  data_size,
											  &boot_img_size);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error (%u) builtin kernel/dtb load failed\n", err);
#if defined(CONFIG_ENABLE_A_B_SLOT)
			pr_error("A/B loader failure\n");
			pr_trace("Trigger SoC reset\n")
			tegrabl_reset();
#endif
			goto fail;
		}
	}

	pr_info("Kernel hdr @%p\n", boot_img_load_addr);
	pr_info("Kernel dtb @%p\n", *kernel_dtb);

	if (callbacks != NULL && callbacks->verify_boot != NULL) {
		callbacks->verify_boot(boot_img_load_addr, *kernel_dtb, kernel_dtbo);
	}

	err = verify_boot_img_header(boot_img_load_addr, boot_img_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
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

	pr_info("%s: Done\n", __func__);

fail:
	tegrabl_free(kernel_dtbo);
	tegrabl_usbh_close();

	return err;
}

#else
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
	uint32_t boot_img_size = 0;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if (!kernel_entry_point || !kernel_dtb) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	err = tegrabl_load_from_fixed_storage(kernel->load_from_storage,
										  &boot_img_load_addr,
										  kernel_dtb,
										  &kernel_dtbo,
										  data,
										  data_size,
										  &boot_img_size);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error (%u) builtin kernel/dtb load failed\n", err);
		goto fail;
	}

#if defined(CONFIG_OS_IS_ANDROID)
	android_hdr = boot_img_load_addr;
#endif

	pr_info("Kernel hdr @%p\n", boot_img_load_addr);
	pr_info("Kernel dtb @%p\n", *kernel_dtb);

#if defined(CONFIG_OS_IS_L4T)
	if (!tegrabl_do_ratchet_check(TEGRABL_BINARY_KERNEL, boot_img_load_addr)) {
		goto fail;
	}
	if (!tegrabl_do_ratchet_check(TEGRABL_BINARY_KERNEL_DTB, *kernel_dtb)) {
		goto fail;
	}

	err = tegrabl_auth_payload(TEGRABL_BINARY_KERNEL, KERNEL_IMAGE, boot_img_load_addr, BOOT_IMAGE_MAX_SIZE);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	err = tegrabl_auth_payload(TEGRABL_BINARY_KERNEL_DTB, KERNEL_DTB, *kernel_dtb, DTB_MAX_SIZE);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Kernel-dtb: authentication failed\n");
		goto fail;
	}
#endif

	if (callbacks != NULL && callbacks->verify_boot != NULL) {
		callbacks->verify_boot(boot_img_load_addr, *kernel_dtb, kernel_dtbo);
	}

	err = verify_boot_img_header(boot_img_load_addr, boot_img_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
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

	pr_info("%s: Done\n", __func__);

fail:
	tegrabl_free(kernel_dtbo);

	return err;
}
#endif
