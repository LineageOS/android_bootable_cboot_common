/*
 * Copyright (c) 2014-2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_BOOTIMAGE_H
#define INCLUDED_TEGRABL_BOOTIMAGE_H

#include <stdint.h>
#include <tegrabl_error.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define ANDROID_MAGIC "ANDROID!"
#define ANDROID_MAGIC_SIZE 8
#define ANDROID_BOOT_NAME_SIZE 16
#define ANDROID_BOOT_CMDLINE_SIZE 512
#define ANDROID_HEADER_SIZE 2048

/**
 * Holds the boot.img (kernel + ramdisk) header.
 *
 * @param magic Holds the magic value used as a signature.
 * @param kernel_size Holds the size of kernel image.
 * @param kernel_addr Holds the load address of kernel.
 * @param ramdisk_size Holds the RAM disk (initrd) image size.
 * @param ramdisk_addr Holds the RAM disk (initrd) load address.
 * @param second_size Holds the secondary kernel image size.
 * @param second_addr Holds the secondary image address
 * @param tags_addr Holds the address for ATAGS.
 * @param page_size Holds the page size of the storage medium.
 * @param unused Unused field.
 * @param name Holds the project name, currently unused,
 * @param cmdline Holds the kernel command line to be appended to default
 *                command line.
 * @param id Holds the identifier, currently unused.
 * @param compression_algo Holds the decompression algorithm:
 * <pre>
 *                          0 = disable decompression
 *                          1 = ZLIB
 *                          2 = LZF
 * </pre>
 * @param crc_kernel Holds the store kernel checksum.
 * @param crc_ramdisk Holds the store RAM disk checksum.
 */

union tegrabl_bootimg_header {
	/* this word is added to deal with aliasing rules */
	uint32_t word[ANDROID_HEADER_SIZE / sizeof(uint32_t)];
	struct {
		uint8_t  magic[ANDROID_MAGIC_SIZE];
		uint32_t kernelsize;
		uint32_t kerneladdr;

		uint32_t ramdisksize;
		uint32_t ramdiskaddr;

		uint32_t secondsize;
		uint32_t secondaddr;

		uint32_t tagsaddr;
		uint32_t pagesize;
		uint32_t unused[2];

		uint8_t  name[ANDROID_BOOT_NAME_SIZE];
		uint8_t  cmdline[ANDROID_BOOT_CMDLINE_SIZE];

		uint32_t id[8];
		uint32_t compressionalgo;

		uint32_t kernelcrc;
		uint32_t ramdiskcrc;
	};
};

#define CRC32_SIZE  (sizeof(uint32_t))

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_TEGRABL_BOOTIMAGE_H */

