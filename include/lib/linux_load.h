/*
 * Copyright (c) 2015 - 2021, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_LINUX_LOADER_H
#define INCLUDED_TEGRABL_LINUX_LOADER_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_partition_loader.h>
#include <tegrabl_bootimg.h>

#if defined(CONFIG_ENABLE_L4T_RECOVERY)
#define KERNEL_BOOTCTRL_MAGIC_NUMBER 0x4C544342   /* "BCTL" */
#define KERNEL_BOOTCTRL_VERSION 1

enum boot_mode {
	BOOT_TO_NORMAL_MODE = 0U,
	BOOT_TO_RECOVERY_MODE,
	BOOT_MODE_MAX = BOOT_TO_RECOVERY_MODE,
};

enum recovery_cmd {
	RECOVERY_CMD_NULL = 0U,
	RECOVERY_CMD_UPDATE_PACKAGE,
	RECOVERY_CMD_FACTORY_RESET,
	RECOVERY_CMD_MAX = RECOVERY_CMD_FACTORY_RESET,
};

struct tegrabl_kernel_bootctrl {
	uint32_t magic_number;  /* "BCTL" */
	enum boot_mode mode;
	enum recovery_cmd command;
	uint32_t version;
	uint32_t crc32;
};

extern struct tegrabl_kernel_bootctrl dummy_kernel_bootctrl;
extern void tegrabl_get_kernel_bootctrl(
		struct tegrabl_kernel_bootctrl *control);
#endif

struct tegrabl_kernel_bin {
	tegrabl_binary_type_t bin_type;
	bool load_from_storage;
	struct tegrabl_binary_info binary;
};

struct tegrabl_kernel_load_callbacks {
	tegrabl_error_t (*verify_boot)(union tegrabl_bootimg_header *, void *,
								   void *);
};

/**
 * @brief Load Android boot image from storage, and extract kernel/ramdisk/DTB
 * from the same
 *
 * @param kernel Used to determine type of kernel - normal/recovery
 * @param kernel_entry_point Entry-point in kernel (output parameter)
 * @param kernel_dtb Kernel DTB memory address (output parameter)
 * @param data Kernel data address in case kernel already loaded to memory
 * @param data_size Kernel data size in case kernel already loaded to memory
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_load_kernel_and_dtb(
		struct tegrabl_kernel_bin *kernel,
		void **kernel_entry_point,
		void **kernel_dtb,
		struct tegrabl_kernel_load_callbacks *callbacks,
		void *data,
		uint32_t data_size);

#if defined(CONFIG_OS_IS_ANDROID)
/**
 * @brief Get os version in Android bootimg header
 *
 * @param os_version pointer to Android os version
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_get_os_version(union android_os_version *os_version);
#endif

#endif /* INCLUDED_TEGRABL_LINUX_LOADER_H */
