/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SPARSE_H
#define TEGRABL_SPARSE_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>
#include <stdbool.h>

#define TEGRABL_SPARSE_HEADER_MAGIC 0xED26FF3AUL

/* Defines sparse header fields. */
struct tegrabl_sparse_image_header {
	/* SPARSE_HEADER_MAGIC 0xed26ff3a */
	uint32_t magic;
	/* (0x1) - reject images with higher major versions */
	uint16_t major_version;
	/* (0x0) - allow images with higer minor versions */
	uint16_t minor_version;
	/* 28 bytes for first revision of the file format */
	uint16_t file_hdr_sz;
	/* 12 bytes for first revision of the file format */
	uint16_t chunk_hdr_sz;
	/* block size in bytes, must be a multiple of 4 (4096) */
	uint32_t blk_sz;
	/* total blocks in the non-sparse output image */
	uint32_t total_blks;
	/* total chunks in the sparse input image */
	uint32_t total_chunks;
	/* CRC32 checksum of the original data */
	uint32_t image_checksum;
};

/* Defines chunk header fields. */
struct tegrabl_sparse_chunk_header {
	uint16_t chunk_type;
	uint16_t reserved;
	/* in blocks in output image */
	uint32_t chunk_sz;
	/* in bytes chunk header and data before next chunk hdr*/
	uint32_t total_sz;
};

/**
 * @brief Defines chunk types in sparse image.
 */
/* macro tegrabl sparse chunk type */
typedef uint32_t tegrabl_sparse_chunk_type_t;
#define TEGRABL_SPARSE_CHUNK_TYPE_RAW 0xCAC1
#define TEGRABL_SPARSE_CHUNK_TYPE_FILL 0xCAC2
#define TEGRABL_SPARSE_CHUNK_TYPE_DONT_CARE 0xCAC3
#define TEGRABL_SPARSE_CHUNK_TYPE_CRC 0xCAC4

/**
 * @brief Defines different states of unsparse machine.
 */
/* macro tegrabl unsparse state type */
typedef uint32_t tegrabl_unsparse_state_type_t;
#define TEGRABL_UNSPARSE_PARTIAL_IMAGE_HEADER 1
#define TEGRABL_UNSPARSE_PARTIAL_CHUNK_HEADER 2
#define TEGRABL_UNSPARSE_PARTIAL_CHUNK_RAW 3
#define TEGRABL_UNSPARSE_PARTIAL_CHUNK_FILL 4
#define TEGRABL_UNSPARSE_PARTIAL_CHUNK_DONT_CARE 5
#define TEGRABL_UNSPARSE_PARTIAL_CHUNK_CRC 6

/* Defines information maintained by unsparse machine while unsparsing
 * buffers.
 */
struct tegrabl_unsparse_state {
	/* Current state of unsparsing */
	tegrabl_unsparse_state_type_t state;
	/* Image header of sparse image */
	struct tegrabl_sparse_image_header image_header;
	/* Header of chunk which is being processed */
	struct tegrabl_sparse_chunk_header chunk_header;
	/* Variable to keep track of offset */
	uint64_t offset;
	/* Variable to keep track of remaining data */
	uint64_t remaining;
	/* Maximum unsparse size allowed. */
	uint64_t max_unsparse_size;
#ifdef TEGRABL_CONFIG_ENABLE_SPARSE_CRC32
	/* Crc32 from image */
	uint32_t image_crc;
	/* Crc32 computed while unsparsing */
	uint32_t computed_crc;
#endif
	/* Value to be filled for chunk of type fill. */
	uint32_t fill_value;
	/* Total chunks processed */
	uint64_t chunks_processed;
	/**
	 * @brief Handle of function which will write to correct location while
	 * unsparsing.
	 * @param buffer Buffer to be written
	 * @param size Size of the buffer
	 * @param aux_info Auxiliary information passed.
	 *
	 * @return should return TEGRABL_NO_ERROR if successful else
	 * appropriate error.
	 */
	tegrabl_error_t (*writer)(const void *buffer, uint64_t size,
			void *aux_info);

	/**
	 * @brief Handle of function which will do seek to new offset
	 * for don't care chunk.
	 *
	 * @param size Bytes to seek from current location.
	 * @param aux_info Auxiliary information passed.
	 *
	 * @return should return TEGRABL_NO_ERROR if successful else
	 * appropriate error.
	 */
	tegrabl_error_t (*seeker)(uint64_t size, void *aux_info);
};

struct tegrabl_sparse_state {
	void *buffer;
	uint64_t buffer_size;
	uint64_t image_size;
};

/**
 * @brief Checks if buffer passed has sparse image header.
 *
 * @param buffer Input buffer.
 * @param size Size of buffer
 *
 * @return true if input buffer has sparse header.
 */
static TEGRABL_INLINE bool tegrabl_sparse_image_check(void *buffer,
		uint64_t size)
{
	if (size < sizeof(uint32_t)) {
		return false;
	}
	if (*(uint32_t *)buffer == TEGRABL_SPARSE_HEADER_MAGIC) {
		return true;
	}
	return false;
}

/**
 * @brief Initializes state of unsparse machine.
 *
 * @param unsparse_state State information maintained by unsparse machine while
 * unsparsing data.
 * @param max_unsparse_size Maximum size allowed after complete unsparse.
 * @param writer Handle of function which writes to destination.
 * @param seeker Handle of function which updates the offset from current
 * location.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code.
 */
tegrabl_error_t tegrabl_sparse_init_unsparse_state(
		struct tegrabl_unsparse_state *unsparse_state,
		uint64_t max_unsparse_size,
		tegrabl_error_t (*writer)(const void *buffer, uint64_t size,
			void *aux_info),
		tegrabl_error_t (*seeker)(uint64_t size, void *aux_info));

/**
 * @brief Unsparses the current buffer based on state.
 *
 * @param unsparse_state Handle of state maintained by unsparse machine.
 * @param buff Buffer to be unsparsed.
 * @param length Size of the buffer.
 * @param aux_info Auxiliary information passed to writer and seeker functions
 * registered in init.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_sparse_unsparse(
		struct tegrabl_unsparse_state *unsparse_state, const void *buff,
		uint64_t length, void *aux_info);

/**
 * @brief Initializes context for creating sparse image.
 *
 * @param sparse_state Sparsing state
 * @param block_size Size of the block
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_sparse_init(
		struct tegrabl_sparse_state *sparse_state, uint32_t block_size);

/**
 * @brief Adds a sparse chunk to current image
 *
 * @param sparse_state Sparsing state
 * @param type Type of chunk to be added
 * @param data Data for chunk
 * @param data_size Size of the data
 * @param write_size Total size pointed by chunk
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_sparse_add_chunk(
		struct tegrabl_sparse_state *sparse_state,
		tegrabl_sparse_chunk_type_t type, void *data,
		uint32_t data_size, uint32_t write_size);

/**
 * @brief Retrieve the buffer and size of sparse image created
 * so far.
 *
 * @param sparse_state Sparsing state
 * @param buffer Updated with location of image
 * @param buffer_size Size of the image
 *
 * @return TEGRABL_NO_ERROR if successful else TEGRABL_ERR_INVALID.
 */
tegrabl_error_t tegrabl_sparse_get_buffer(
		struct tegrabl_sparse_state *sparse_state,
		void **buffer, uint64_t *buffer_size);

#endif
