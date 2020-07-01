/*
 * Copyright (c) 2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef _TEGRABL_DECOMPRESS_PRIVATE_H
#define _TEGRABL_DECOMPRESS_PRIVATE_H

#include "tegrabl_error.h"
#include "tegrabl_debug.h"
#include "tegrabl_malloc.h"
#include "string.h"


/* zlib decompressor APIs */
#ifdef CONFIG_ENABLE_ZLIB
/* zlib algo context initialization */
void *zlib_init(uint32_t compressed_size);

/* zlib algo decompress api */
tegrabl_error_t zlib_decompress(void *cntxt, void *in_buffer, uint32_t in_size,
								void *out_buffer, uint32_t outbuf_size,
								uint32_t *written_size);

/* zlib algo clean up api */
tegrabl_error_t zlib_end(void *cntxt);
#endif


/* lzf decompressor APIs */
#ifdef CONFIG_ENABLE_LZF
/* lzf algo context initialization */
void *lzf_init(uint32_t compressed_size);

/* lzf algo decompress api */
tegrabl_error_t do_lzf_decompress(void *cntxt, void *in_buffer,
								  uint32_t in_size, void *out_buffer,
								  uint32_t outbuf_size, uint32_t *written_size);
#endif


#ifdef CONFIG_ENABLE_LZ4
/* lz4 algo decompress api */
tegrabl_error_t do_lz4_decompress(void *cntxt, void *in_buffer,
								  uint32_t in_size, void *out_buffer,
								  uint32_t outbuf_size, uint32_t *written_size);
#endif

#endif

