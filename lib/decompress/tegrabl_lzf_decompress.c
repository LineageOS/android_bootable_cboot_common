/*
 * Copyright (c) 2006      Stefan Traby <stefan@hello-penguin.com>
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 * Modifications by NVIDIA:
 * - Exhaustive modifications to fit with streamed decompression framework
 */

#include "lzf.h"
#include "string.h"
#include "stdbool.h"
#include "tegrabl_error.h"
#include "tegrabl_utils.h"
#include "tegrabl_decompress.h"
#include "tegrabl_decompress_private.h"

/* FIXME: Place the blocksize in a common place (header or config-option) */
#define BLOCKSIZE (1024 * 64 - 1)
#define MAX_BLOCKSIZE BLOCKSIZE

/*
 * Anatomy: an lzf file consists of any number of blocks in following format:
 *
 * \x00   EOF (optional)
 * "ZV\0" 2-byte-usize <uncompressed data>
 * "ZV\1" 2-byte-csize 2-byte-usize <compressed data>
 * "ZV\2" 4-byte-NvTbootComputeCrc32-0xdebb20e3 (NYI)
 */

#define TYPE0_HDR_SIZE 5
#define TYPE1_HDR_SIZE 7
#define MAX_HDR_SIZE 7
#define MIN_HDR_SIZE 5

/* The source data from storage is read into ReadBuffer and is used for
 * decompression. The decompressed data is written to the destination buffer */
/* Required size of ReadBuffer is twice the maximum size of compressed
 * image + 2 * pages */

#define DECOMP_PAGES 64

struct lzf_context {
	uint8_t *hdr;
	uint32_t h_rem;
	uint8_t *data;
	bool data_compressed;
	uint32_t data_rem_size;
	int32_t data_csize;
	int32_t data_ucsize;
	uint32_t data_read;
	uint32_t csum;
	uint32_t csum_offset;
};

static struct lzf_context _context;

void *lzf_init(uint32_t compressed_size)
{
	struct lzf_context *context = &_context;

	context->hdr = NULL;
	context->h_rem = 0;
	context->data = NULL;
	context->data_compressed = false;
	context->data_rem_size = 0;
	context->data_csize = 0;
	context->data_ucsize = 0;
	context->data_read = 0;
	context->csum = 0;
	context->csum_offset = compressed_size - 4;

	return context;
}

tegrabl_error_t do_lzf_decompress(void *cntxt, void *in_buffer,
								  uint32_t in_size, void *out_buffer,
								  uint32_t outbuf_size, uint32_t *written_size)
{
	uint32_t processed_size = 0;
	uint32_t unprocessed_size = in_size;
	struct lzf_context *context = (struct lzf_context *)cntxt;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t *out = out_buffer;
	uint32_t ret;

	*written_size = 0;

	pr_debug(": cksum on in_buffer: %u\n",
			 tegrabl_utils_crc32(0, in_buffer, in_size));

	while ((processed_size < in_size) &&
		   (context->data_read < context->csum_offset)) {
		pr_debug("Inbuf:0x%p, Pgsize:%d, OutBuf:0x%p\n",
				 in_buffer, in_size, out);

		unprocessed_size = (in_size - processed_size);

		if (context->data == NULL) {
			pr_debug("%s: data_read: %d ; csum_offset: %d\n", __func__,
					 context->data_read, context->csum_offset);
			if (context->hdr == NULL) {
				context->hdr = in_buffer;
				pr_debug("%s: header @ 0x%p\n", __func__, context->hdr);

				if (unprocessed_size < MIN_HDR_SIZE) {
					pr_error("%s: entire type-0/1 header unavailable\n",
							 __func__);
					context->h_rem = MIN_HDR_SIZE - unprocessed_size;
					break;
				} else if (context->hdr[0] == 0) {
					pr_debug("%s: EOF encountered\n", __func__);
					break;
				} else if ((context->hdr[0] != 'Z') ||
						   (context->hdr[1] != 'V')) {
					pr_error("%s: invalid header\n", __func__);
					err = TEGRABL_ERR_INVALID;
					goto fail;
				}
			}

			switch (context->hdr[2]) {
			case 0:
				context->data_csize = -1;
				context->data_ucsize = (context->hdr[3] << 8) | context->hdr[4];
				processed_size += TYPE0_HDR_SIZE;
				context->data_read += TYPE0_HDR_SIZE;
				context->data = &context->hdr[TYPE0_HDR_SIZE];
				break;
			case 1:
				if (unprocessed_size < TYPE1_HDR_SIZE) {
					context->h_rem = TYPE1_HDR_SIZE - unprocessed_size;
					pr_info("%s: entire type-1 header unavailable\n", __func__);
					break;
				}
				context->data_csize = (context->hdr[3] << 8) | context->hdr[4];
				context->data_ucsize = (context->hdr[5] << 8) | context->hdr[6];
				processed_size += TYPE1_HDR_SIZE;
				context->data_read += TYPE1_HDR_SIZE;
				context->data = &context->hdr[TYPE1_HDR_SIZE];
				break;
			default:
				pr_error("%s: unknown blocktype\n", __func__);
				err = TEGRABL_ERR_INVALID;
				goto fail;
			}

			context->data_compressed = (context->data_csize != -1);
			context->data_rem_size = (context->data_csize == -1) ?
									 context->data_ucsize : context->data_csize;
			pr_debug("  Compressed: %d\n", context->data_compressed);
			pr_debug("    C-size  : %d bytes\n", context->data_csize);
			pr_debug("   UC-size  : %d bytes\n", context->data_ucsize);
		}

		pr_debug("%s: processed_size: %u\n", __func__, processed_size);

		if (unprocessed_size < context->data_rem_size) {
			context->data_rem_size -= unprocessed_size;
			pr_debug("%s: entire payload unavailable (%d bytes left)\n",
					 __func__, context->data_rem_size);
			break;
		}

		if ((uint8_t *)out_buffer + outbuf_size - out < context->data_ucsize) {
			pr_critical("%s: output buffer is too small\n", __func__);
			return TEGRABL_ERR_OVERFLOW;
		}

		if (!context->data_compressed) {
			pr_debug("%s: copying %d bytes from 0x%p => 0x%p\n", __func__,
					 context->data_ucsize, context->data, out);
			memcpy(out, context->data, context->data_ucsize);
			in_buffer = context->data + context->data_ucsize;
			context->data_read += context->data_ucsize;
			context->csum = tegrabl_utils_crc32(context->csum, context->data,
												context->data_ucsize);
		} else {
			pr_debug("%s: decompressing [%d bytes @ 0x%p] => [%d bytes @ 0x%p]\n",
					 __func__, context->data_csize, context->data,
					 context->data_ucsize, out);

			ret = lzf_decompress(context->data, context->data_csize,
								 out, context->data_ucsize);
			if (ret != (uint32_t)context->data_ucsize) {
				pr_error("%s: error while decompressing (ret=%u)n",
						 __func__, ret);
				err = TEGRABL_ERR_BAD_PARAMETER;
				goto fail;
			}
			in_buffer = context->data + context->data_csize;
			context->data_read += context->data_csize;
			context->csum = tegrabl_utils_crc32(context->csum, out,
												context->data_ucsize);
		}

		processed_size += context->data_rem_size;
		pr_debug("%s: processed_size: %u\n", __func__, processed_size);

		out += context->data_ucsize;
		*written_size += context->data_ucsize;
		context->data = NULL;
		context->hdr = NULL;
	}

	if (context->data_read >= context->csum_offset) {
		if (memcmp(in_buffer, &(context->csum), 4) != 0) {
			pr_error("%s: decompression crc-check failed\n", __func__);
			err = TEGRABL_ERR_INVALID;
		} else {
			pr_info("%s: decompression done successfully\n", __func__);
		}
	}

	pr_debug("%s: processed_size: %d\n", __func__, processed_size);
	pr_debug("%s: %d bytes written to output buffer\n",
			 __func__, *written_size);

fail:
	return err;
}

