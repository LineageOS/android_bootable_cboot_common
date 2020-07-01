/**
 * Copyright (c) 2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_FIXED_BOOT_H
#define INCLUDED_FIXED_BOOT_H

/**
 * @brief Load boot.img and dtb from fixed storage
 *
 * @param kernel Ptr to kernel bin object
 * @param boot_img_load_addr Ptr to the address where boot.img/kernel is loaded (output)
 * @param dtb_load_addr Ptr to the address where dtb is loaded (output)
 * @param kernel_dtbo Ptr to the address where dtbo is loaded (output)
 * @param ramdisk_load_addr Ptr to the address where ramdisk is loaded (output)
 * @param data Ptr to the kernel image stored in memory
 * @param data_size Size of the kernel image stored in memory
 * @param kernel_size Ptr to the kernel size buffer (output)
 * @param ramdisk_size Ptr to the ramdisk size buffer (output)
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t fixed_boot_load_kernel_and_dtb(struct tegrabl_kernel_bin *kernel,
											   void **boot_img_load_addr,
											   void **dtb_load_addr,
											   void **kernel_dtbo,
											   void **ramdisk_load_addr,
											   void *data,
											   uint32_t data_size,
											   uint32_t *kernel_size,
											   uint64_t *ramdisk_size);

#endif  /* INCLUDED_FIXED_BOOT_H */
