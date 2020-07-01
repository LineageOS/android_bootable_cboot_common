/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "build_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <string.h>

/**
 * @brief Initialize a buffer with an incremental pattern for mismatches
 *
 * @param buf Buffer to be checked
 * @param ch Starting 8-byte data of the pattern
 * @param incr Increment
 * @param len Size of the buffer in bytes
 *
 * @return 0 if successful, otherwise a non-zero value
 */
static void _clib_test_bytewise_init(uint8_t *buf, uint8_t ch,
									 uint8_t incr, size_t len)
{
	while (len != 0U) {
		*(buf++) = ch;
		ch += incr;
		len--;
	}
}

/**
 * @brief Check a buffer against an incremental pattern for mismatches
 *
 * @param buf Buffer to be checked
 * @param ch Starting 8-byte data of the pattern
 * @param incr Increment
 * @param len Size of the buffer in bytes
 *
 * @return 0 if successful, otherwise a non-zero value
 */
static int _clib_test_bytewise_verify(const uint8_t *buf, uint8_t ch,
									  uint8_t incr, size_t len)
{
	while (len != 0U) {
		if (*(buf++) != ch) {
			tegrabl_printf("Mismatch @ %p: (expected: 0x%2x, found: 0x%02x)\n",
						   (buf - 1), ch, *(buf - 1));
			return -1;
		}
		ch += incr;
		len--;
	}

	return 0;
}

static void dump32(const void *buf_start, const void *buf_end)
{
	uint32_t *ptr;

	for (ptr = (uint32_t *)buf_start; ptr < (uint32_t *)buf_end; ptr += 4) {
		pr_debug("%p: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				 ptr, ptr[0], ptr[1], ptr[2], ptr[3]);
	}
}

#define SRC_STRIDE	4
#define DST_STRIDE	4
#define LEN_STRIDE	32

int tegrabl_clib_test_memcpy(size_t maxsize, bool alloc, void *testbuf)
{
	uint8_t *buf, *src, *dst;
	size_t len;
	int result = 0;
	int iter_result;
	size_t ls, lo, ss, so, dss, dso;

	buf = (alloc) ? tegrabl_malloc(maxsize) : testbuf;

	tegrabl_printf("%s: (buf: %p, maxsize: 0x%lx)\n", __func__, buf,
				   (long int)maxsize);

	ss = 0;
	so = 0;
	for (src = buf; src < (buf + maxsize);
		 src = buf + (((ss) ? (SRC_STRIDE << (ss - 1)) : 0) + so)) {

		dss = 0;
		dso = 0;
		for (dst = buf; dst <= src;
			 dst = buf + (((dss) ? (DST_STRIDE << (dss - 1)) : 0) + dso)) {

			lo = 0;
			ls = 0;
			for (len = 0;
				 len < (unsigned long)((buf + maxsize) - src);
				 len = ((ls) ? (LEN_STRIDE << ls) : 0) + lo) {

				/* Initialize the memory with known pattern */
				_clib_test_bytewise_init(buf, 0xff, 0, src - buf);
				_clib_test_bytewise_init(src, 0xaa, 1, len);
				_clib_test_bytewise_init(src + len, 0xff, 0,
										 (buf + maxsize) - (src + len));

				/* call memcpy */
				memcpy(dst, src, len);

				/* verify entire buffer */
				iter_result = 0;
				if ((_clib_test_bytewise_verify(buf, 0xff, 0,
						dst - buf) != 0) ||
					(_clib_test_bytewise_verify(dst, 0xaa,
						1, len) != 0) ||
					(_clib_test_bytewise_verify(src + len,
						0xff, 0, (buf + maxsize) -
						(src + len)) != 0)) {
					iter_result = -1;
				}

				if ((dst + len) < src) {
					if (_clib_test_bytewise_verify(dst + len, 0xff, 0,
												   src - (dst + len)) ||
						_clib_test_bytewise_verify(src, 0xaa, 1, len)) {
						iter_result = -1;
					}
				} else {
					if (_clib_test_bytewise_verify(
							dst + len, 0xaa + ((dst + len) - src), 1,
							(src + len) - (dst + len)) != 0) {
						iter_result = -1;
					}
				}

				if (iter_result == -1) {
					tegrabl_printf("FAILED: memcpy(%p, %p, 0x%lx)\n",
								   dst, src, (long int)len);
					{
						pr_debug("After memcpy\n");
						dump32(buf, src + len + 128);

						_clib_test_bytewise_init(buf, 0xff, 0, src - buf);
						_clib_test_bytewise_init(src, 0xaa, 1, len);
						_clib_test_bytewise_init(src + len, 0xff, 0,
												 (buf + maxsize) - (src + len));

						pr_debug("Before memcpy\n");
						dump32(buf, src + len + 128);
					}
					result = -1;
					break;
				}

				if (lo == (LEN_STRIDE - 1)) {
					ls++;
				}
				lo = (lo + 1) % LEN_STRIDE;
			}

			tegrabl_printf("[dest=%p, src=%p] : DONE\n", dst, src);

			if (dso == (DST_STRIDE - 1)) {
				dss++;
			}
			dso = (dso + 1) % DST_STRIDE;
		}


		if (so == (SRC_STRIDE - 1)) {
			ss++;
		}
		so = (so + 1) % SRC_STRIDE;
	}

	if (result == 0) {
		tegrabl_printf("%s: PASSED\n", __func__);
	}

	return result;
}

int tegrabl_clib_test_memset(size_t maxsize, bool alloc, void *testbuf)
{
	uint8_t *buf;
	size_t len;
	const uint8_t testch = 0x5a;
	size_t offset;
	int result = 0;
	size_t offset_limit = (maxsize < 64) ? maxsize : 64;

	buf = (alloc) ? tegrabl_malloc(maxsize) : testbuf;

	tegrabl_printf("%s: (buf: %p, maxsize: 0x%lx)\n", __func__, buf,
				   (long int)maxsize);
	for (offset = 0; offset < offset_limit; offset++) {
		for (len = 0; len <= (maxsize - offset); len++) {
			/* Initialize the memory with known pattern */
			_clib_test_bytewise_init(buf, 0xff, 0, maxsize);

			/* call memset */
			memset(buf + offset, testch, len);

			/* verify entire buffer */
			if ((_clib_test_bytewise_verify(buf, 0xff,
					0, offset) != 0) ||
				(_clib_test_bytewise_verify(buf + offset,
					testch, 0, len) != 0) ||
				 (_clib_test_bytewise_verify(buf + offset +
					len, 0xff, 0, maxsize -
						offset - len) != 0)) {
				tegrabl_printf("FAILED: memset(%p, 0x%02x, 0x%lx)\n",
							   buf + offset, testch, (long int)len);
				result = -1;
				break;
			}
		}
		tegrabl_printf("dest=%p : DONE\n", buf + offset);
	}

	if (result == 0) {
		tegrabl_printf("%s: PASSED\n", __func__);
	}

	return result;
}
