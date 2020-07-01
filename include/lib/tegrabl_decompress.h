/*
 * Copyright (c) 2014-2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_LIB_DECOMPRESS_H
#define INCLUDED_LIB_DECOMPRESS_H

#include "tegrabl_error.h"
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define DECOMP_PAGESIZE (1024 * 32)

typedef struct {
	/* magic ID for compression algorithm */
	uint8_t magic[2];

	/* name string for compression algorithm */
	const char *name;

	/**
	 * @brief: initialization handler for decompressor
	 *
	 * @param compressed_size: compressed data size
	 *
	 * @return valid (implementation-specific) context pointer (in case of
	 *         success), returns NULL in case of failure
	 */
	void* (*init)(uint32_t compressed_size);

	/**
	 * @brief: decompression handler
	 *
	 * @param cntxt: the context returned by init
	 * @param in_buffer: pointer to input compressed data buffer
	 * @param in_size: input data size
	 * @param out_buffer: pointer to output decompressed data buffer
	 * @param outbuf_size: MAX decompressed data size supported
	 * @param written_size: decompressed data size
	 *
	 * @return SUCCESS or FAILURE
	 */
	tegrabl_error_t (*decompress)(void *cntxt, void *in_buffer,
								  uint32_t in_size, void *out_buffer,
								  uint32_t outbuf_size, uint32_t *written_size);

	/**
	 * @brief: function to cleanup and free up resources/context
	 *
	 * @param context: context returned by init
	 *
	 * @return SUCCESS or FAILURE
	 */
	tegrabl_error_t (*end)(void *context);
} decompressor;

/**
 * @brief: get the decompression handle as per magic ID
 *
 * @param c_magic: magic id from compressed file header
 * @param len: length of magic id
 *
 * @return decompressor: decompression handle pointer, return
 *         NULL if not found any magic matched
 */
decompressor *decompress_method(uint8_t *c_magic, uint32_t len);

/**
 * @brief: judge whether content is compressed by magic id
 *
 * @param head_buf: pointer to buffer saving head of content
 * @param pdecomp: pointer to decompressor handler
 *
 * @return true if content is compressed; false otherwise
 */
bool is_compressed_content(uint8_t *head_buf, decompressor **pdecomp);

/**
 * @brief: decompress compressed content to out_buffer
 *
 * @param decomp: decompression handler
 * @param read_buffer: pointer to compressed data
 * @param read_size: compressed data size (in byte)
 * @param out_buffer: pointer to uncompressed data buffer
 * @param outbuf_size: when used as input, pointer to size of out_buffer;
 *                     when used as output, pointer to actual size of data
 *                     decompressed to out_buffer
 *
 * @return error status of decompression
 */
tegrabl_error_t do_decompress(decompressor *decomp, uint8_t *read_buffer,
							  uint32_t read_size, uint8_t *out_buffer,
							  uint32_t *outbuf_size);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_LIB_DECOMPRESS_H */
