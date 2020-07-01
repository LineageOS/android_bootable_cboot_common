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
 * @brief Get boot details by parsing extlinux.conf file
 *
 * @param handle pointer to file_manager handle
 * @param extlinux_conf pointer to extlinux conf structure
 * @param boot_entry entry to boot from extlinux.conf file
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t get_boot_details(struct tegrabl_fm_handle * const fm_handle,
								 struct conf * const extlinux_conf,
								 uint32_t *boot_entry);

#endif
