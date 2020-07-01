/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_UTILS_H
#define TEGRABL_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_compiler.h>

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

/* Compute ceil(n/d) */
#define DIV_CEIL(n, d) (((n) + (d) - 1) / (d))

/* Compute ceil(n/(2^logd)) */
#define DIV_CEIL_LOG2(n, logd) (((n) + (1 << (logd)) - 1) >> (logd))

/* Compute floor(n/d) */
#define DIV_FLOOR(n, d) (((n) - ((n) % (d))) / (d))

/* Compute floor(n/(2^logd)) */
#define DIV_FLOOR_LOG2(n, logd) ((n) >> (logd))

/* Round-up n to next multiple of w */
#define ROUND_UP(n, w) (DIV_CEIL(n, w) * (w))

/* Round-up n to next multiple of w, where w = (2^x) */
#define ROUND_UP_POW2(n, w) ((((n) - 1) & ~((w) - 1)) + (w))

/* Round-down n to lower multiple of w */
#define ROUND_DOWN(n, w) ((n) - ((n) % (w)))

/* Round-down n to lower multiple of w, where w = (2^x) */
#define ROUND_DOWN_POW2(n, w) ((n) & ~((w) - 1))

/* Highest power of 2 <= n */
#define HIGHEST_POW2_32(n) (1 << (31 - clz((n))))

/* Compute (n % d) */
#define MOD_POW2(n, d) ((n) & ((d) - 1))

/* Compute (n % (2^logd)) */
#define MOD_LOG2(n, logd) ((n) & ((1 << (logd)) - 1))

#define BITFIELD_ONES(width) ((1 << (width)) - 1)

#define BITFIELD_MASK(width, shift)  (BITFIELD_ONES(width) << (shift))

#define BITFIELD_SHIFT(x) ((0 ? x) % 32)

#define BITFIELD_WIDTH(x) ((1 ? x) - (0 ? x))

#define BITFIELD_SET(var, value, width, shift)							\
	do {																\
		(var) = (var) & ~(BITFIELD_MASK(width, shift));					\
		(var) = (var) | (((value) & BITFIELD_ONES(width)) << (shift));	\
	} while (0)

#define BITFIELD_GET(value, width, shift) \
	(((value) & BITFIELD_MASK(width, shift)) >> (shift))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#define U64_TO_U32_LO(addr64) ((uint32_t)(uint64_t)(addr64))
#define U64_TO_U32_HI(addr64) ((uint32_t)((uint64_t)(addr64) >> 32))

#define U64_FROM_U32(addrlo, addrhi) \
	(((uint64_t)(addrlo)) | ((uint64_t)(addrhi) << 32))

#define BITS_PER_BYTE 8

#define SWAP(a, b) { \
	(a) ^= (b); \
	(b) ^= (a); \
	(a) ^= (b); \
}

#define ALIGN(X, A)	 (((X) + ((A)-1)) & ~((A)-1))

/**
 * @brief Computes the crc32 of buffer.
 *
 * @param val			Initial value.
 * @param buffer		Data for which crc32 to be computed.
 * @param buffer_size	size of buffer.
 *
 * @return Final crc32 of buffer.
 */
uint32_t tegrabl_utils_crc32(uint32_t val, void *buffer, size_t buffer_size);

/**
 * @brief Computes the checksum of buffer.
 *
 * @param buffer		Data for which checksum to be computed.
 * @param buffer_size	size of buffer.
 *
 * @return Checksum of buffer.
 */
uint32_t tegrabl_utils_checksum(void *buffer, size_t buffer_size);

/**
 * @brief Calculates CRC8 checksum
 *
 * @param buffer		Buffer for crc calculation
 * @param len			Length of the buffer
 *
 * @return crc8 checksum for the buffer
 */
uint8_t tegrabl_utils_crc8(uint8_t *buffer, uint32_t len);

/**
 * @brief Returns the binary representation of a byte data
 *
 * @param byte_ptr The location of the target byte
 *
 * @return binary_num Number with binary representation of the byte
 */
uint32_t tegrabl_utils_convert_to_binary(void *byte_ptr);


#endif // TEGRABL_UTILS_H

