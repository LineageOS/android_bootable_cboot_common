/*
 * Copyright (c) 2014-2019, NVIDIA Corporation.  All Rights Reserved.
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
#include "bootimg.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define ANDROID_HEADER_SIZE sizeof(struct boot_img_hdr_v0)

typedef struct boot_img_hdr_v0 tegrabl_bootimg_header;

/*
 * operating system version and security patch level.
 * for version "A.B.C" and patch level "Y-M-D":
 *     ver = A << 14 | B << 7 | C         (7 bits for each of A, B, C)
 *     lvl = ((Y - 2000) & 127) << 4 | M  (7 bits for Y, 4 bits for M)
 *     os_version = ver << 11 | lvl
 */
union android_os_version {
	uint32_t data;
	struct {
		uint32_t security_month:4; /* bits[3:0] */
		uint32_t security_year:7; /* bits[10:4] */
		uint32_t subminor_version:7; /* bits[17:11] */
		uint32_t minor_version:7; /* bits[24:18] */
		uint32_t major_version:7; /* bits[31:25] */
	};
};

#if defined(CONFIG_DYNAMIC_LOAD_ADDRESS)
/*
 * U-Boot binary header
 * @param b_instr Holds instruction that branches to kernel code
 * @reserved
 * @kernel_offset Holds the relative offset of kernel
 * @kernel_size	Holds binary size including BSS
 * @kernel_flags Holds informative flags
 * @reserved_64bit[3]
 * @magic[5] Holds string to identify the binary
 * @reserved
 */
struct tegrabl_uboot_header {
	uint32_t b_instr;
	uint32_t reserved;
	uint64_t kernel_offset;
	uint64_t kernel_size;
	uint64_t kernel_flags;
	uint64_t reserved2[3];
	char magic[5];
	uint32_t reserved3;
};
#endif

#define CRC32_SIZE  (sizeof(uint32_t))

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_TEGRABL_BOOTIMAGE_H */

