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

tegrabl_error_t tegrabl_sparse_init_unsparse_state(
		struct tegrabl_unsparse_state *unsparse_state,
		uint64_t max_unsparse_size,
		tegrabl_error_t (*writer)(const void *buffer, uint64_t size,
			void *aux_info),
		tegrabl_error_t (*seeker)(uint64_t size, void *aux_info))
{
	if (!unsparse_state || !writer || !seeker || !max_unsparse_size) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
	}

	memset(unsparse_state, 0x0, sizeof(*unsparse_state));
	unsparse_state->state = TEGRABL_UNSPARSE_PARTIAL_IMAGE_HEADER;
	unsparse_state->remaining = sizeof(struct tegrabl_sparse_image_header);
	unsparse_state->max_unsparse_size = max_unsparse_size;
	unsparse_state->writer = writer;
	unsparse_state->seeker = seeker;

	return TEGRABL_NO_ERROR;
}

/**
 * @brief Validates header and checks if unsparsed size mentioned
 * in header is less or equal to maximum allowed size specified.
 *
 * @param sparse_image_header Header of sparse image.
 * @param max_unsparse_size Maximum unsparsed size
 *
 * @return TEGRABL_NO_ERROR if no issues or
 * TEGRABL_ERR_INVALID_VERSION if version mismatch or
 * TEGRABL_ERR_OVERFLOW if size mentioned in header is more than specified size.
 */
static tegrabl_error_t tegrabl_sparse_validate_header(
		struct tegrabl_sparse_image_header *sparse_image_header,
		uint64_t max_unsparse_size)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_debug("\n=== SPARSE HEADER STARTS ===\n");
	pr_debug("MAGIC         %u\n", sparse_image_header->magic);
	pr_debug("file_hdr_sz   %u\n", sparse_image_header->file_hdr_sz);
	pr_debug("chunk_hdr_sz  %u\n", sparse_image_header->chunk_hdr_sz);
	pr_debug("blk_sz        %u\n", sparse_image_header->blk_sz);
	pr_debug("total_blks    %u\n", sparse_image_header->total_blks);
	pr_debug("total_chunks  %u\n", sparse_image_header->total_chunks);
	pr_debug("=== SPARSE HEADER ENDS ===\n");

	if (sparse_image_header->major_version !=
			TEGRABL_SPARSE_HEADER_MAJOR_VERSION) {
		pr_debug("Header major version mismatch\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID_VERSION, 0);
		goto fail;
	}

	if (max_unsparse_size < ((uint64_t)sparse_image_header->blk_sz *
					sparse_image_header->total_blks)) {
		pr_debug("Unsparse image size is more than allowed maximum size\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
	}

fail:
	return error;
}

/**
 * @brief Validates the chunk header for correct sizes.
 *
 * @param image_header Header of the sparse image.
 * @param chunk_header Chunk header to be validated.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_sparse_validate_chunk_header(
		struct tegrabl_sparse_image_header *image_header,
		struct tegrabl_sparse_chunk_header *chunk_header)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_debug("Sparse Chunk => Type: %d, Chunk Size: %d, Total Size: %d\n",
			chunk_header->chunk_type, chunk_header->chunk_sz,
			chunk_header->total_sz);

	switch (chunk_header->chunk_type) {
	case TEGRABL_SPARSE_CHUNK_TYPE_RAW:
		if (chunk_header->total_sz != (image_header->chunk_hdr_sz +
			(chunk_header->chunk_sz * image_header->blk_sz))) {
			pr_debug("Bogus chunk size for RAW chunk\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
		}
		break;
	case TEGRABL_SPARSE_CHUNK_TYPE_DONT_CARE:
		if (chunk_header->total_sz != image_header->chunk_hdr_sz) {
			pr_debug("Bogus chunk size for don't care chunk\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 1);
		}
		break;
	case TEGRABL_SPARSE_CHUNK_TYPE_FILL:
		if (chunk_header->total_sz != (image_header->chunk_hdr_sz +
					sizeof(uint32_t))) {
			pr_debug("Bogus chunk size for fill chunk\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 2);
		}
		break;
	case TEGRABL_SPARSE_CHUNK_TYPE_CRC:
		break;
	default:
		pr_debug("Invalid chunk type %d\n", chunk_header->chunk_type);
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return error;
}

tegrabl_error_t tegrabl_sparse_unsparse(
		struct tegrabl_unsparse_state *unsparse_state,
		const void *buff, uint64_t length, void *aux_info)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint64_t size = 0;
	uint64_t remaining = 0;
	uint64_t offset = 0;
	uint32_t i = 0;
	uint32_t buffer[SPARSE_MAX_LOCAL_BUFFER / sizeof(uint32_t)];
	const uint8_t *sparse_buffer = (const uint8_t *)buff;
	uint8_t *tmp_buff = NULL;
	struct tegrabl_sparse_image_header *image_header = NULL;
	struct tegrabl_sparse_chunk_header *curr_header = NULL;
	enum tegrabl_unsparse_state_type state = 0;
	uint64_t chunks_processed = 0;
#ifdef TEGRABL_CONFIG_ENABLE_SPARSE_CRC32
	uint32_t computed_crc = 0;
#endif

	if (!buff || !length || !unsparse_state) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 1);
		goto fail;
	}

	image_header = &unsparse_state->image_header;
	curr_header = &unsparse_state->chunk_header;

	if (!unsparse_state->writer || !unsparse_state->seeker) {
		pr_debug("Unsparse machine is not initialized appropriately\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
		goto fail;
	}

	remaining = unsparse_state->remaining;
	offset = unsparse_state->offset;
	state = unsparse_state->state;
	chunks_processed = unsparse_state->chunks_processed;

	pr_debug("Block @ %p of size %"PRIu64"\n", buff, length);
	pr_debug("State => Remaining %"PRIu64", offset %"PRIu64", State %d\n",
			remaining, offset, state);
#ifdef TEGRABL_CONFIG_ENABLE_SPARSE_CRC32
	computed_crc = unsparse_state->computed_crc;
#endif

	/* If length can be zero if last chunk is don't care type. If state of
	 * machine is don't care and length is zero then process don't care.
	 */
	while (length || TEGRABL_UNSPARSE_PARTIAL_CHUNK_DONT_CARE == state) {
		switch (state) {
		case TEGRABL_UNSPARSE_PARTIAL_IMAGE_HEADER:
			size = MIN(length, remaining);
			tmp_buff = (uint8_t *)image_header;
			memcpy(tmp_buff + offset, sparse_buffer, size);
			remaining -= size;
			length -= size;
			offset += size;
			sparse_buffer += size;

			if (remaining != 0) {
				continue;
			}

			/* If all header is received then process further */
			error = tegrabl_sparse_validate_header(image_header,
					unsparse_state->max_unsparse_size);
			if (error != TEGRABL_NO_ERROR) {
				pr_debug("sparse header validation failed\n");
				goto fail;
			}

			/* Header is followed by chunk header */
			memset(curr_header, 0x0, sizeof(*curr_header));
			state = TEGRABL_UNSPARSE_PARTIAL_CHUNK_HEADER;
			remaining = image_header->chunk_hdr_sz;
			offset = 0;
			break;

		case TEGRABL_UNSPARSE_PARTIAL_CHUNK_HEADER:
			size = MIN(length, remaining);
			tmp_buff = (uint8_t *)curr_header;

			if (offset < sizeof(*curr_header)) {
				memcpy(tmp_buff + offset, sparse_buffer, size);
			}

			remaining -= size;
			length -= size;
			offset += size;
			sparse_buffer += size;

			if (remaining != 0) {
				continue;
			}

			/* If entire chunk header is received then validate first
			 * and set variables for next state
			 */
			error = tegrabl_sparse_validate_chunk_header(
					image_header, curr_header);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}

			chunks_processed++;
			if (chunks_processed > image_header->total_chunks) {
				pr_debug("Received more chunks expected\n");
				error = TEGRABL_ERROR(TEGRABL_ERR_OUT_OF_RANGE, 0);
				goto fail;
			}

			state = curr_header->chunk_type - TEGRABL_SPARSE_CHUNK_TYPE_RAW +
									TEGRABL_UNSPARSE_PARTIAL_CHUNK_RAW;
			offset = 0;
			remaining = (uint64_t)curr_header->chunk_sz * image_header->blk_sz;
			break;

		case TEGRABL_UNSPARSE_PARTIAL_CHUNK_RAW:
			size = MIN(length, remaining);
			error = unsparse_state->writer(sparse_buffer, size, aux_info);
			if (error != TEGRABL_NO_ERROR) {
				pr_debug("Failed to write unsparse image\n");
				TEGRABL_SET_HIGHEST_MODULE(error);
				goto fail;
			}
#ifdef TEGRABL_CONFIG_ENABLE_SPARSE_CRC32
			computed_crc = tegrabl_utils_crc32(computed_crc,
					(void *)sparse_buffer, size);
#endif
			remaining -= size;
			length -= size;
			offset += size;
			sparse_buffer += size;

			/* If all bytes are consumed then next will be chunk header
			 * initialize state variables
			 */
			if (remaining == 0) {
				memset(curr_header, 0x0, sizeof(*curr_header));
				state = TEGRABL_UNSPARSE_PARTIAL_CHUNK_HEADER;
				remaining = sizeof(*curr_header);
				offset = 0;
			}
			break;

		case TEGRABL_UNSPARSE_PARTIAL_CHUNK_FILL:
			if (offset < sizeof(uint32_t)) {
				size = MIN(length, sizeof(uint32_t));
				tmp_buff = (uint8_t *) &unsparse_state->fill_value;
				memcpy(tmp_buff + offset, sparse_buffer, size);
				length -= size;
				offset += size;
				sparse_buffer += size;

				if (offset < sizeof(uint32_t)) {
					continue;
				}
			}

			buffer[0] = unsparse_state->fill_value;
			for (i = 1; i < ARRAY_SIZE(buffer); i++) {
				buffer[i] = buffer[0];
			}

			while (remaining) {
				size = MIN(remaining, SPARSE_MAX_LOCAL_BUFFER);
				remaining -= size;

#ifdef TEGRABL_CONFIG_ENABLE_SPARSE_CRC32
				computed_crc = tegrabl_utils_crc32(computed_crc,
						(void *)buffer, size);
#endif
				error = unsparse_state->writer(buffer, size, aux_info);
				if (error != TEGRABL_NO_ERROR) {
					pr_debug("Failed to write unsparse image\n");
					TEGRABL_SET_HIGHEST_MODULE(error);
					goto fail;
				}
			}

			memset(curr_header, 0x0, sizeof(*curr_header));
			state = TEGRABL_UNSPARSE_PARTIAL_CHUNK_HEADER;
			remaining = sizeof(*curr_header);
			offset = 0;
			break;

		case TEGRABL_UNSPARSE_PARTIAL_CHUNK_DONT_CARE:
			error = unsparse_state->seeker(remaining, aux_info);
			if (error != TEGRABL_NO_ERROR) {
				pr_debug("Failed to seek to new location while unsparsing\n");
				TEGRABL_SET_HIGHEST_MODULE(error);
				goto fail;
			}

			memset(curr_header, 0x0, sizeof(*curr_header));
			state = TEGRABL_UNSPARSE_PARTIAL_CHUNK_HEADER;
			remaining = sizeof(*curr_header);
			offset = 0;
			break;

		case TEGRABL_UNSPARSE_PARTIAL_CHUNK_CRC:
#ifdef TEGRABL_CONFIG_ENABLE_SPARSE_CRC32
			if (offset < sizeof(uint32_t)) {
				size = MIN(length, sizeof(uint32_t));
				tmp_buff = (uint8_t *) &unsparse_state->image_crc;
				memcpy(tmp_buff + offset, sparse_buffer, size);
				length -= size;
				offset += size;
				sparse_buffer += size;
				continue;
			}

			if (unsparse_state->image_crc != computed_crc) {
				pr_debug("Computed crc32 0x%08x, expected crc32 0x%08x\n",
					computed_crc, unsparse_state->image_crc);
				error = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
			}

			memset(curr_header, 0x0, sizeof(*curr_header));
			state = TEGRABL_UNSPARSE_PARTIAL_CHUNK_HEADER;
			remaining = sizeof(*curr_header);
			offset = 0;
#else
			pr_info("Crc32 for unsparse is not enabled\n");
#endif
			break;

		default:
			pr_debug("invalid state %d\n", state);
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID_STATE, 0);
			goto fail;
		}
	}

	unsparse_state->remaining = remaining;
	unsparse_state->offset = offset;
	unsparse_state->state = state;
	unsparse_state->chunks_processed = chunks_processed;
	unsparse_state->chunks_processed = chunks_processed;

#ifdef TEGRABL_CONFIG_ENABLE_SPARSE_CRC32
	unsparse_state->computed_crc = computed_crc;
#endif

fail:
	return error;
}

