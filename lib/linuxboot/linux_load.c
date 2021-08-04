/*
 * Copyright (c) 2016-2021, NVIDIA Corporation.  All Rights Reserved.
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
#include <tegrabl_arm64.h>
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
#if defined(CONFIG_ENABLE_USB_SD_BOOT) || defined(CONFIG_ENABLE_NVME_BOOT)
#include <removable_boot.h>
#endif
#if defined(CONFIG_ENABLE_ETHERNET_BOOT)
#include <net_boot.h>
#endif
#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)
#include <extlinux_boot.h>
#endif
#if defined(CONFIG_ENABLE_NVME_BOOT)
#include <tegrabl_pcie.h>
#endif
#if defined(CONFIG_ENABLE_SECURE_BOOT)
#include <tegrabl_auth.h>
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

#if defined(CONFIG_ENABLE_L4T_RECOVERY)
struct tegrabl_kernel_bootctrl dummy_kernel_bootctrl = {
	.magic_number = KERNEL_BOOTCTRL_MAGIC_NUMBER,
	.mode = BOOT_TO_NORMAL_MODE,
	.command = RECOVERY_CMD_NULL,
	.version = KERNEL_BOOTCTRL_VERSION,
	.crc32 = 0x1cd6a971,
};

void tegrabl_get_kernel_bootctrl(struct tegrabl_kernel_bootctrl *control)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *kernel_bootctrl_load_addr;
	uint32_t binary_length;
	uint32_t crc32;
	uint32_t bootctrl_exist;

	if (!control) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		pr_error("Invalid passed-in control\n");
		return;
	}

	err = tegrabl_load_binary_copy(TEGRABL_BINARY_KERNEL_BOOTCTRL,
		&kernel_bootctrl_load_addr, &binary_length,
		TEGRABL_BINARY_COPY_PRIMARY);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("%s: failed to read primary bootctrl data\n",
			__func__);
		err = tegrabl_load_binary_copy(TEGRABL_BINARY_KERNEL_BOOTCTRL,
			&kernel_bootctrl_load_addr, &binary_length,
			TEGRABL_BINARY_COPY_RECOVERY);
		if (err != TEGRABL_NO_ERROR) {
			pr_warn("%s: failed to read recovery bootctrl data\n",
				__func__);
			bootctrl_exist = 0;
		} else
			bootctrl_exist = 1;
	} else {
		bootctrl_exist = 1;
	}

	if (bootctrl_exist) {
		memcpy(control,
			(struct tegrabl_kernel_bootctrl *)kernel_bootctrl_load_addr,
			sizeof(struct tegrabl_kernel_bootctrl));
		if (control->magic_number != KERNEL_BOOTCTRL_MAGIC_NUMBER) {
			pr_warn("%s: magic number(0x%08x) is invalid\n",
				__func__, control->magic_number);
		} else {
			/* Check crc32 if magic number is correct */
			crc32 = tegrabl_utils_crc32(0, (char *)control,
				sizeof(struct tegrabl_kernel_bootctrl) - sizeof(crc32));
			if (crc32 != control->crc32) {
				/* Use dummy boot control data if crc32 is not valid */
				pr_warn("%s: crc32(0x%08x) != (0x%08x)\n",
					__func__, crc32, control->crc32);
			} else {
				/* Check version/boot mode/recovery command if crc32 is valid */
				if (control->version != KERNEL_BOOTCTRL_VERSION) {
					pr_warn("%s: version(%u) is invalid\n",
						__func__, control->version);
				} else if (control->mode > BOOT_MODE_MAX) {
					pr_warn("%s: boot mode(%u) is invalid\n",
						__func__, control->mode);
				} else if (control->command > RECOVERY_CMD_MAX) {
					pr_warn("%s: recovery mode command(%u) is invalid\n",
						__func__, control->command);
				} else {
					/* Boot control data is valid */
					goto bootctrl_done;
				}
			}
		}
	}
	pr_warn("%s: use default dummy boot control data\n", __func__);
	memcpy(control, &dummy_kernel_bootctrl, sizeof(struct tegrabl_kernel_bootctrl));

bootctrl_done:
	pr_debug("%s: version=%u, mode=%u, command=%u\n",
		__func__, control->version, control->mode,
		control->command);
}
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

/* Extract kernel from an Android boot image, and return the address where it is installed in memory */
static tegrabl_error_t extract_kernel(void *boot_img_load_addr,
									  uint32_t kernel_bin_size,
									  void **kernel_load_addr)
{
	bool is_compressed = false;
	decompressor *decomp = NULL;
	uint32_t decomp_size = 0; /* kernel size after decompressing */
	union tegrabl_bootimg_header *hdr = NULL;
	union tegrabl_arm64_header *ahdr = NULL;
	uint64_t payload_addr;
	uint32_t kernel_size;
	uint64_t kernel_text_offset = tegrabl_get_kernel_text_offset();
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	hdr = (union tegrabl_bootimg_header *)boot_img_load_addr;
	if (HAS_BOOT_IMG_HDR(hdr)) {
		/* Get kernel addr and size from boot img header */
		payload_addr = (uintptr_t)hdr + hdr->pagesize;
		kernel_size = hdr->kernelsize;
	} else {  /*  In extinux boot, raw kernel image gets loaded */
		ahdr = boot_img_load_addr;
		if (ahdr->magic == ARM64_MAGIC) {
			kernel_text_offset = ahdr->text_offset;
		}
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

	*kernel_load_addr = (void *)(tegrabl_get_kernel_load_addr() + kernel_text_offset);
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

#if defined(CONFIG_ENABLE_NVME_BOOT)
static bool nvme_load_kernel_and_dtb(char *boot_dev,
									 void **boot_img_load_addr,
									 void **kernel_dtb,
									 void **ramdisk_load_addr,
									 uint32_t *kernel_size,
									 uint64_t *ramdisk_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool is_load_done = false;
	int8_t *pcie_ctrl_nums;
	int8_t ctrl_num;
	int8_t i;

	pr_debug("%s: boot_dev=%s\n", __func__, boot_dev);

	/*
	 * boot_dev has the "nvme" stripped from its original boot_dev.
	 * tegrabl_get_pcie_ctrl_nums() function returns a list of pcie controller numbers based
	 * on the original boot_dev:
	 *
	 * original boot_dev		list of ctrl_nums
	 * -----------------		-----------------------------------
	 *   "nvme"					0, 1, . . , max_ctrl_supported-1 (to probe all PCIe controllers)
	 *   "nvme:C<n>"			n (to probe just PCIe controller Cn)
	 *   "nvme:pcie@<addr>"		n (to probe a PCIe controller n which has the PCI address of <addr>)
	 *
	 * The returned list is terminated with -1.
	 */
	pcie_ctrl_nums = tegrabl_get_pcie_ctrl_nums(boot_dev);

	if (pcie_ctrl_nums == NULL) {
		return false;
	}

	i = 0;
	while (!is_load_done) {
		ctrl_num = pcie_ctrl_nums[i++];
		pr_debug("%s: NVME kernel load from ctrl=%d.\n", __func__, ctrl_num);
		if (ctrl_num < 0) {
			tegrabl_free((void *)pcie_ctrl_nums);
			break;
		}

		err = removable_boot_load_kernel_and_dtb(BOOT_FROM_NVME,
												 (uint8_t)ctrl_num,
												 boot_img_load_addr,
												 kernel_dtb,
												 ramdisk_load_addr,
												 kernel_size,
												 ramdisk_size);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s (%d) boot failed, err: 0x%x\n", "NVME", ctrl_num, err);
			continue;
		}
		tegrabl_free((void *)pcie_ctrl_nums);
		return true;
	}
	return false;
}
#endif	/* CONFIG_ENABLE_NVME_BOOT */

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
	uint32_t i;
	char **boot_dev_order;
	char *boot_dev;
	uint8_t device_id;
	void *boot_img_load_addr = NULL;
	void *ramdisk_load_addr = NULL;
	uint32_t kernel_size = 0;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	if (!kernel_entry_point || !kernel_dtb) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/*
	 * Get boot dev order from cbo.dtb. boot_dev_order is the boot device string,
	 * like "sd", "usb", or "nvme:pcie@14180000", "nvme@5".
	 */
	boot_dev_order = tegrabl_get_boot_dev_order();

	/* Try loading boot image and dtb from devices as per boot order */
	i = 0;
	while (!is_load_done) {
		boot_dev = boot_dev_order[i++];
		if (boot_dev == NULL) {
			break;
		}

		/*
		 * tegrabl_cbo_map_boot_dev() function maps the boot device string
		 * to device_id, as in BOOT_FROM_USB, BOOT_FROM_NVME, or BOOT_FROM_BUILTIN_STORAGE.
		 * The function also returns a pointer pointed to boot_dev string that pass the
		 * matched base boot device name.
		 * The base boot device names are: "usb", "sd", "emmc", "net", "nvme", etc.
		 * (See g_boot_devices[] in tegrabl_cbo.c file.)
		 *
		 * Ex 1:
		 *   boot_dev -> "emmc";
		 *   after tegrabl_cbo_map_boot_dev(),
		 *   boot_dev -> '\0', device_id = BOOT_FROM_BUILTIN_STORAGE
		 *
		 * Ex 2:
		 *   boot_dev -> "nvme:pcie@14180000"
		 *   after tegrabl_cbo_map_boot_dev(),
		 *   boot_dev -> ":pcie@14180000", device_id = BOOT_FROM_NVME
		 */
		boot_dev = tegrabl_cbo_map_boot_dev(boot_dev, &device_id);
		if (boot_dev == NULL) {
			/* Invalid boot_dev */
			continue;
		}

		switch (device_id) {
#if defined(CONFIG_ENABLE_ETHERNET_BOOT)
		case BOOT_FROM_NETWORK:
			err = net_boot_load_kernel_and_dtb(&boot_img_load_addr, kernel_dtb);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("Net boot failed, err: %u\n", err);
				break;
			}
			is_load_done = true;
			break;
#endif

#if defined(CONFIG_ENABLE_USB_SD_BOOT)
		case BOOT_FROM_SD:
		case BOOT_FROM_USB:
			err = removable_boot_load_kernel_and_dtb(device_id,
													 0,
													&boot_img_load_addr,
													 kernel_dtb,
													 &ramdisk_load_addr,
													 &kernel_size,
													 &ramdisk_size);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("%s boot failed, err: %u\n", device_id == BOOT_FROM_SD ? "SD" : "USB", err);
				break;
			}
			is_load_done = true;
			break;
#endif	/* CONFIG_ENABLE_USB_SD_BOOT */

#if defined(CONFIG_ENABLE_NVME_BOOT)
		case BOOT_FROM_NVME:
			is_load_done = nvme_load_kernel_and_dtb(boot_dev,
													&boot_img_load_addr,
													kernel_dtb,
													&ramdisk_load_addr,
													&kernel_size,
													&ramdisk_size);
			break;
#endif	/* CONFIG_ENABLE_NVME_BOOT */

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
#if defined(CONFIG_ENABLE_SECURE_BOOT)
	pr_debug("%s: completing auth ...\n", __func__);
	err = tegrabl_auth_complete();
#endif

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
#if defined(CONFIG_ENABLE_SECURE_BOOT)
	pr_debug("%s: completing auth ...\n", __func__);
	err = tegrabl_auth_complete();
#endif
	tegrabl_free(kernel_dtbo);

	return err;
}
#endif  /* CONFIG_ENABLE_BOOT_DEVICE_SELECT */
