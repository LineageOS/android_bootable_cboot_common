/**
 * Copyright (c) 2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_USB_SD_BOOT_H
#define INCLUDED_USB_SD_BOOT_H

#if defined(CONFIG_ENABLE_USB_SD_BOOT)
/**
 * @brief Load boot.img and dtb from USB or SD card
 *
 * @param boot_type Type of the storage device
 * @param boot_img_load_addr Ptr to the address where boot.img is loaded (output)
 * @param dtb_load_addr Ptr to the address where dtb is loaded (output)
 * @param ramdisk_load_addr Ptr to the address where ramdisk is loaded (output)
 * @param kernel_size Ptr to the kernel size buffer (output)
 * @param ramdisk_size Ptr to the ramdisk size buffer (output)
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t usb_sd_boot_load_kernel_and_dtb(uint8_t boot_type,
												void **boot_img_load_addr,
												void **dtb_load_addr,
												void **ramdisk_load_addr,
												uint32_t *kernel_size,
												uint64_t *ramdisk_size);
#endif  /* CONFIG_ENABLE_USB_SD_BOOT */

#endif  /* INCLUDED_USB_SD_BOOT_H */
