/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All Rights Reserved.
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
#include <stdbool.h>
#include <tegrabl_compiler.h>

#ifndef MIN
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#endif

/* Compute ceil(n/d) */
#define DIV_CEIL(n, d) (((n) + (d) - 1U) / (d))

/* Compute ceil(n/(2^logd)) */
#define DIV_CEIL_LOG2(n, logd) (((n) + (1UL << (logd)) - 1UL) >> (logd))

/* Compute floor(n/d) */
#define DIV_FLOOR(n, d) (((n) - ((n) % (d))) / (d))

/* Compute floor(n/(2^logd)) */
#define DIV_FLOOR_LOG2(n, logd) ((n) >> (logd))

/* Round-up n to next multiple of w */
#define ROUND_UP(n, w) (DIV_CEIL(n, w) * (w))

/* Round-up n to next multiple of w, where w = (2^x) */
#define ROUND_UP_POW2(n, w) ((((n) - 1U) & ~((w) - 1U)) + (w))

/* Round-down n to lower multiple of w */
#define ROUND_DOWN(n, w) ((n) - ((n) % (w)))

/* Round-down n to lower multiple of w, where w = (2^x) */
#define ROUND_DOWN_POW2(n, w) ((n) & ~((w) - 1U))

/* Highest power of 2 <= n */
#define HIGHEST_POW2_32(n) (1UL << (31U - clz((n))))

/* Compute (n % d) */
#define MOD_POW2(n, d) ((n) & ((d) - 1U))

/* Compute (n % (2^logd)) */
#define MOD_LOG2(n, logd) ((n) & ((1UL << (logd)) - 1UL))

#define BITFIELD_ONES(width) ((1UL << (width)) - 1UL)

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

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

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

#define IS_SPACE(c) \
((' ' == (c)) || ('\f' == (c)) || ('\n' == (c)) || ('\r' == (c)) || \
												 ('\t' == (c)) || ('\v' == (c)))
#define IS_DIGITAL(c) (((c) >= '0') && ((c) <= '9'))
#define IS_UPPER(c) (((c) >= 'A') && ((c) <= 'Z'))
#define IS_LOWER(c) (((c) >= 'a') && ((c) <= 'z'))
#define IS_ALPHA(c) ((IS_UPPER(c) || (IS_LOWER(c))))

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

/**
 * @brief Convert a string to an unsigned long integer
 *
 * @param nptr start number of the string
 * @param endptr end address of the string converted
 * @param base of the integer string
 *
 * @return unsigned long integer converted from the string
 */
unsigned long tegrabl_utils_strtoul(const char *nptr, char **endptr, int base);

/**
 * @brief Dump the memory contents of a region specified in parameters
 *		  in chunks of 32 bytes
 *
 * @param addr Start (virtual) address of the region to be dumped
 * @param size Size of the region to be dumped
 */
void tegrabl_utils_dump_mem(uintptr_t addr, uint32_t size);

/**
 * @brief Check if a value lies within the given range
 *
 * @param start start of the range
 * @param size Size of the range
 * @param value Value to be tested
 *
 * @return true if value lies within the range false if it doesn't
 */
static TEGRABL_INLINE bool tegrabl_utils_range_check(uint64_t start, uint64_t size, uint64_t value)
{
	return ((start <= value) && ((start + size) > value));
}

/* Swap an unsigned int (uint32_t) from be32 to le32 */
uint32_t be32tole32(uint32_t data);

/* Swap an unsigned int (uint32_t) from le32 to be32 */
uint32_t le32tobe32(uint32_t data);

#endif // TEGRABL_UTILS_H

