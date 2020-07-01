/*
 * Copyright (c) 2014-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __NVBLOB_PARSE_H
#define __NVBLOB_PARSE_H

#include "sys/types.h"
#include <tegrabl_error.h>

#define PARTITION_NAME_LENGTH 40
#define IMG_SPEC_INFO_LENGTH 64
#define UPDATE_MAGIC_SIZE  16
#define BLOB_HEADER_ACCESSORY_SIZE 8

/* macro tegrabl blob type */
typedef uint32_t tegrabl_blob_type_t;
#define BLOB_UPDATE 0
#define BLOB_BMP 1
#define BLOB_NONE 2
#define BLOB_FORCE32 0x7FFFFFFF

/* elements need to be 32-bit aligned */
struct tegrabl_image_entry {
	char partname[PARTITION_NAME_LENGTH];
	uint32_t image_offset;
	uint32_t image_size;
	uint32_t version;
	uint32_t op_mode;
	char spec_info[IMG_SPEC_INFO_LENGTH];
};

/* macro tegrabl image type */
typedef uint32_t tegrabl_image_type_t;
#define IMAGE_NVIDIA 0
#define IMAGE_LOWBATTERY 1
#define IMAGE_CHARGING 2
#define IMAGE_CHARGED 3
#define IMAGE_FULLYCHARGED 4
#define IMAGE_SATA_FW_OTA 5
#define IMAGE_VERITY_YELLOW_PAUSE 6
#define IMAGE_VERITY_YELLOW_CONTINUE 7
#define IMAGE_VERITY_ORANGE_PAUSE 8
#define IMAGE_VERITY_ORANGE_CONTINUE 9
#define IMAGE_VERITY_RED_PAUSE 10
#define IMAGE_VERITY_RED_CONTINUE 11
#define IMAGE_USER_DEFINED 12
#define IMAGE_NUM 13
#define IMAGE_FORCE32 0x7FFFFFFF

/* macro tegrabl bmp resolution */
typedef uint32_t tegrabl_bmp_resolution_t;
#define BMPRES_480P 0
#define BMPRES_720P 1
#define BMPRES_810P 2
#define BMPRES_1080P 3
#define BMPRES_4K 4
#define BMPRES_1200P_P 5
#define BMPRES_NUM 6
#define BMPRES_FORCE32 0x7FFFFFFF

struct tegrabl_bmp_entry {
	tegrabl_image_type_t bmp_type;
	uint32_t bmp_offset;
	uint32_t bmp_size;
	tegrabl_bmp_resolution_t bmp_res;
	/* keep entry format identical with blob generator */
	char reserved[36];
};

typedef uintptr_t tegrabl_blob_handle;

/**
 * @brief ratchet info structure
 *
 * @mb1_ratchet_level
 * @mts_ratchet_level
 * @rollback_ratchet_level
 * @reserved[5] reserved bytes
 */
struct ratchet_info {
	uint8_t mb1_ratchet_level;
	uint8_t mts_ratchet_level;
	uint8_t rollback_ratchet_level;
	uint8_t reserved[5];
};

/**
 * @brief blob header accessory union
 *
 * @rb_info rollback info
 */
union header_accessory {
	struct ratchet_info ratchet_info;
	uint8_t data[BLOB_HEADER_ACCESSORY_SIZE];
};

/**
 * @brief header of blob descriptor
 *
 * @magic magic of the header
 * @version version of blob
 * @size size of the blob
 * @entries_offset offset of binary entries in blob
 * @num_entries num of binary entries in blob
 * @type blob type
 * @uncomp_size original uncompressed size if blob is compressed
 * @rb_info rollback info if ota blob embeds it
 */
struct blob_header {
	unsigned char magic[UPDATE_MAGIC_SIZE];
	uint32_t version;

	uint32_t size;

	uint32_t entries_offset;
	uint32_t num_entries;
	tegrabl_blob_type_t type;

	uint32_t uncomp_size;

	union header_accessory accessory;
};

/**
 * @brief Create a blob handle to access blob info.
 *
 * If blob is already read, pass address of the blob in "bptr". If blob
 * pointer is NULL, reads the blob from the "part_name" partition.
 *
 * Once blob is no more needed, call blob_close to free
 * all allocated memory.
 *
 * @param part_name Name of the partition which contains NvBlob
 * @param bptr Pointer to the blob if it is already read to memory
 * @param bh Blob-handle, to be filled
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_blob_init(char *part_name, uint8_t *bptr,
								  tegrabl_blob_handle *bh);

/**
 * @brief Get the size of the blob
 *
 * @bh Blob-handle
 *
 * @return size of blob
 */
uint32_t tegrabl_blob_get_size(tegrabl_blob_handle bh);

/**
 * @brief Determine if blob is signed or not
 *
 * @param bh Blob-handle
 *
 * @return false if not signed or blob represented by bh is not valid,
 *         true if signed
 */
bool tegrabl_blob_is_signed(tegrabl_blob_handle bh);

/**
 * @brief Determine type of the blob (Update, Bmp, etc...)
 *
 * @param bh Blob-handle
 * @param type Type of the blob, to be filled
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_blob_get_type(tegrabl_blob_handle bh,
									  tegrabl_blob_type_t *type);

/**
 * @brief Get memory-location and length of the signed section (actual Blob)
 *        from NvBlob partition
 *
 * @param bh Blob-handle
 * @param blob Memory-address of the signed blob, to be filled,
 *        memory is allocated inside this function, needs to be freed
 *        using blob_close
 * @param bloblen length of signed blob, to be filled
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_blob_get_details(tegrabl_blob_handle bh, uint8_t **blob,
										 uint32_t *bloblen);

/**
 * @brief Get signature of the signed blob
 *
 * @param bh Blob-handle
 * @param size Length of signature data, to be filled
 * @param signature Pointer to the Signature data in blob_handle.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_blob_get_signature(tegrabl_blob_handle bh,
										   uint32_t *size, uint8_t **signature);

/**
 * @brief Get number of entries in NvBlob
 *
 * @param bh Blob-handle
 * @param num_entries number of entries, to be filled
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_blob_get_num_entries(tegrabl_blob_handle bh,
											 uint32_t *num_entries);

/**
 * @brief Get entry header by index from NvBlob
 *
 * @param bh Blob-handle
 * @param index index of the entry to be retrieved
 * @param entry Memory-address of the index-th entry, to be filled,
 *        memory is allocated inside this function, needs to be freed
 *        using blob_close
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_blob_get_entry(tegrabl_blob_handle bh, uint32_t index,
									   void **entry);

/**
 * @brief Get data corresponding to index-th entry
 *
 * @param bh Blob-handle
 * @param index index of the desired entry
 * @param data memory-address of the data corresponding to index-th entry,
 *        to be filled,
 *        memory is allocated inside this function, needs to be freed
 *        using blob_close
 * @param size size of the data, to be filled
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_blob_get_entry_data(tegrabl_blob_handle bh,
											uint32_t index, uint8_t **data,
											uint32_t *size);

/**
 * @brief Free the memory occupied by blob
 *
 * @param bh Blob-handle
 */
void tegrabl_blob_close(tegrabl_blob_handle bh);

#endif
