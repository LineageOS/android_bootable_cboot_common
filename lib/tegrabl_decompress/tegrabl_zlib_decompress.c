/*
 * Copyright (c) 2014-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "tegrabl_error.h"
#include "tegrabl_utils.h"
#include "stdint.h"
#include "stdbool.h"
#include "zlib.h"
#include "tegrabl_decompress.h"
#include "tegrabl_decompress_private.h"

#define CHUNKSIZE (2048*32*16)

#define MORE_STATS 0

/* NOTE This makes this not thread-safe,
 * but don't think the library is anyway */
struct zlib_context {
	z_stream strm;
	bool done;
};

static struct zlib_context _context;

void *zlib_init(uint32_t compressed_size)
{
	int ret;
	struct zlib_context *context = &_context;

	/* allocate inflate state */
	context->strm.zalloc = Z_NULL;
	context->strm.zfree = Z_NULL;
	context->strm.opaque = Z_NULL;
	context->strm.avail_in = 0;
	context->strm.next_in = Z_NULL;

	/* add 32 to detect header type automatically */
	ret = inflateInit2(&(context->strm), 32 + MAX_WBITS);
	if (ret != Z_OK) {
		return NULL;
	}

	return context;
}

tegrabl_error_t zlib_decompress(void *cntxt, void *in_buffer, uint32_t in_size,
								void *out_buffer, uint32_t outbuf_size,
								uint32_t *written_size)
{
	int32_t ret;
	uint32_t have;
	uint8_t *output = out_buffer;
	struct zlib_context *context = (struct zlib_context *)cntxt;

	context->strm.avail_in = in_size;
	context->strm.next_in = in_buffer;
	context->done = false;
	*written_size = 0;

	pr_debug("inbuf=0x%p (size:0x%x), outbuf=0x%p\n",
			 in_buffer, in_size, out_buffer);

	do {
		if ((uint8_t *)out_buffer + outbuf_size - output < CHUNKSIZE) {
			pr_critical("%s: output buffer is too small!\n", __func__);
			return TEGRABL_ERR_OVERFLOW;
		}
		context->strm.avail_out = CHUNKSIZE;
		context->strm.next_out = output;
		ret = inflate(&(context->strm), Z_NO_FLUSH);

		if (ret != Z_OK && ret != Z_STREAM_END) {
			pr_critical("zlib::inflate() returns %s (%d)\n",
						context->strm.msg, ret);
			return TEGRABL_ERR_BAD_PARAMETER;
		}

		have = CHUNKSIZE - context->strm.avail_out;
		*written_size += have;
		output += have;
	} while (context->strm.avail_out == 0);

	pr_debug("%s: decompressed data-size: %d\n", __func__, *written_size);
	if (ret == Z_STREAM_END) {
		context->done = true;
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t zlib_end(void *cntxt)
{
	struct zlib_context *context = (struct zlib_context *)cntxt;

	inflateEnd(&(context->strm));

	return TEGRABL_NO_ERROR;
}

