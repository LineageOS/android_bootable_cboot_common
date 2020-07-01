/*
 * Copyright (c) 2015 - 2017, NVIDIA Corporation.  All Rights Reserved.
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
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_load_kernel_and_dtb(
		struct tegrabl_kernel_bin *kernel,
		void **kernel_entry_point,
		void **kernel_dtb,
		struct tegrabl_kernel_load_callbacks *callbacks,
		void *data);

#endif /* INCLUDED_TEGRABL_LINUX_LOADER_H */
