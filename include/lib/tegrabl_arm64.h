/*
 * Copyright (c) 2020, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_ARM64_H
#define INCLUDED_TEGRABL_ARM64_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define ARM64_MAGIC 0x644d5241
#define ARM64_HEADER_SIZE 64

/**
 * ARM64 Linux kernel image header.
 *
 * Please refer to the following Linux kernel document for more details
 * on the header format:
 *
 * https://www.kernel.org/doc/Documentation/arm64/booting.txt
 */

union tegrabl_arm64_header {
	/* this word is added to deal with aliasing rules */
	uint32_t word[ARM64_HEADER_SIZE / sizeof(uint32_t)];
	struct {
		uint32_t code0;
		uint32_t code1;
		uint64_t text_offset;
		uint64_t image_size;
		uint64_t flags;
		uint64_t res2;
		uint64_t res3;
		uint64_t res4;
		uint32_t magic;
		uint32_t res5;
	};
};

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_TEGRABL_ARM64_H */
