/**
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_SPARSE

#include <string.h>
#include <inttypes.h>
#include <tegrabl_sparse.h>
#include <tegrabl_sparse_local.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>

tegrabl_error_t tegrabl_sparse_init(
		struct tegrabl_sparse_state *sparse_state, uint32_t block_size)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t *buffer = NULL;
	uint32_t buffer_size = 0;
	struct tegrabl_sparse_image_header *header = NULL;

	if (!sparse_state || block_size % 4096) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	memset(sparse_state, 0x0, sizeof(*sparse_state));
	buffer_size = sizeof(*header);
	buffer = tegrabl_malloc(buffer_size);
	if (!buffer) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	memset(buffer, 0x0, buffer_size);

	header = (struct tegrabl_sparse_image_header *)buffer;
	header->magic = TEGRABL_SPARSE_HEADER_MAGIC;
	header->major_version = 1;
	header->file_hdr_sz = buffer_size;
	header->chunk_hdr_sz = sizeof(struct tegrabl_sparse_chunk_header);
	header->blk_sz = block_size;

	sparse_state->buffer = buffer;
	sparse_state->buffer_size = buffer_size;

fail:
	return error;
}

tegrabl_error_t tegrabl_sparse_add_chunk(
		struct tegrabl_sparse_state *sparse_state,
		tegrabl_sparse_chunk_type_t type, void *data,
		uint32_t data_size, uint32_t write_size)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_sparse_chunk_header *chunk_header = NULL;
	struct tegrabl_sparse_image_header *header = NULL;
	uint8_t *buffer = NULL;
	uint32_t buffer_size = 0;
	uint64_t total_buffer_size = 0;

	if (!sparse_state || !write_size || !sparse_state->buffer ||
			!sparse_state->buffer_size) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	buffer = sparse_state->buffer;
	header = (struct tegrabl_sparse_image_header *)buffer;
	buffer_size = header->chunk_hdr_sz;

	switch (type) {
	case TEGRABL_SPARSE_CHUNK_TYPE_RAW:
		buffer_size += ROUND_UP(write_size, header->blk_sz);
		if (data_size != write_size) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
		break;
	case TEGRABL_SPARSE_CHUNK_TYPE_FILL:
	case TEGRABL_SPARSE_CHUNK_TYPE_CRC:
		buffer_size += sizeof(uint32_t);
		if (data_size != sizeof(uint32_t)) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
		break;
	case TEGRABL_SPARSE_CHUNK_TYPE_DONT_CARE:
		if (data_size != 0) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
		break;
	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	total_buffer_size = sparse_state->buffer_size + buffer_size;

	buffer = tegrabl_realloc(buffer, total_buffer_size);
	if (!buffer) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	header = (struct tegrabl_sparse_image_header *)buffer;
	chunk_header = (struct tegrabl_sparse_chunk_header *)(buffer +
						sparse_state->buffer_size);
	memset(chunk_header, 0x0, buffer_size);

	chunk_header->chunk_type = type;
	chunk_header->chunk_sz = DIV_CEIL(write_size, header->blk_sz);
	chunk_header->total_sz = buffer_size;
	header->total_blks += chunk_header->chunk_sz;
	header->total_chunks++;

	memcpy(buffer + sparse_state->buffer_size + header->chunk_hdr_sz,
		data, data_size);

	sparse_state->buffer_size = total_buffer_size;
	sparse_state->buffer = buffer;
	sparse_state->image_size += ROUND_UP(write_size, header->blk_sz);

fail:
	return error;
}

tegrabl_error_t tegrabl_sparse_get_buffer(
		struct tegrabl_sparse_state *sparse_state,
		void **buffer, uint64_t *buffer_size)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!sparse_state || !sparse_state->buffer ||
			!sparse_state->buffer_size || !buffer || !buffer_size) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	*buffer = sparse_state->buffer;
	*buffer_size = sparse_state->buffer_size;

fail:
	return error;
}

