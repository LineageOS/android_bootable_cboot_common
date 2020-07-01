/**
 * Copyright (c) 2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_EXTLINUX_BOOT_H
#define INCLUDED_EXTLINUX_BOOT_H

#if defined(CONFIG_ENABLE_EXTLINUX_BOOT)

#define MAX_BOOT_SECTION            5UL

struct boot_section {
	char *label;
	char *menu_label;
	char *linux_path;
	char *dtb_path;
	char *initrd_path;
	char *boot_args;
};

struct conf {
	uint32_t default_boot_entry;
	char *menu_title;
	struct boot_section section[MAX_BOOT_SECTION];
	uint32_t num_boot_entries;
};

/**
 * @brief Load kernel and dtb through extlinux.conf
 *
 * @param fm_handle pointer to file manager handle
 * @param boot_img_load_addr Ptr to the address where boot.img is loaded (output)
 * @param dtb_load_addr Ptr to the address where dtb is loaded (output)
 * @param kernel_size Ptr to the kernel size buffer (output)
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t extlinux_boot_load_kernel_and_dtb(struct tegrabl_fm_handle *fm_handle,
												  void **boot_img_load_addr,
												  void **dtb_load_addr,
												  uint32_t *kernel_size);

/**
 * @brief Load ramdisk through extlinux.conf
 *
 * @param fm_handle pointer to file manager handle
 * @param ramdisk_load_addr Ptr to the address where ramdisk is loaded (output)
 * @param ramdisk_size Ptr to the ramdisk size bufer (output)
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t extlinux_boot_load_ramdisk(struct tegrabl_fm_handle *fm_handle,
										   void **ramdisk_load_addr,
										   uint64_t *ramdisk_size);

/**
 * @brief Update kernel dtb with boot args from extlinux.conf
 *
 * @param kernel_dtb Address where kernel dtb is loaded
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t extlinux_boot_update_bootargs(void *kernel_dtb);

/**
 * @brief Set extlinux boot status
 *
 * @param status Pass status as true if extlinux boot is success otherwise false
 */
void extlinux_boot_set_status(bool status);

/**
 * @brief Get extlinux boot status
 *
 * @return Return true if extlinux boot is success otherwise false
 */
bool extlinux_boot_get_status(void);

#endif /* CONFIG_ENABLE_EXTLINUX_BOOT */

#endif /* INCLUDED_EXTLINUX_BOOT_H */
