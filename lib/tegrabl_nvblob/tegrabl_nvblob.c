/*
 * Copyright (c) 2014-2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_NVBLOB

#include <stdio.h>
#include <string.h>
#include <tegrabl_nvblob.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <tegrabl_decompress.h>

/* Definitions and Structures below this point should be synchronized with
 * host side nvblob generator :
 *          ($TEGRA_TOP/bootloader/nvbootloader/nvbootutils/nvblob/nvblob_v2)
 */
#define UPDATE_MAGIC "NVIDIA__BLOB__V2"
#define SIGNED_UPDATE_MAGIC       "SIGNED-BY-TEGRASIGN-"
#define SIGNED_UPDATE_MAGIC_SIZE  20
#define PRINT_BLOB_CRC 0

/**
 * @brief blob signed header
 *
 * @magic magic of signed header
 * @actual_blob_size size of raw blob
 * @signature_size size of blob signature
 */
struct signed_header {
	uint8_t magic[SIGNED_UPDATE_MAGIC_SIZE];
	uint32_t actual_blob_size;
	uint32_t signature_size;
};

/**
 * @brief blob related info
 *
 * @start blob start address
 * @offset data offset of the blob
 * @data_mem_size blob data memory size
 * @info_mem_size blob info memory size
 */
struct blob_info {
	uint8_t *start;
	uint32_t offset;
	uint32_t data_mem_size;
	uint32_t info_mem_size;
};

/* blob entry descriptor */
union blob_entry {
	struct tegrabl_image_entry uentry;
	struct tegrabl_bmp_entry bentry;
};

static void print_entry(const char *func, enum tegrabl_blob_type t,
						union blob_entry *e)
{
	switch(t) {
		case BLOB_UPDATE:
			pr_debug("%s: partition=%s len=%d version=%d op_mode=%d spec=%s\n",
				func, e->uentry.partname, e->uentry.image_size,
				e->uentry.version, e->uentry.op_mode, e->uentry.spec_info);
			break;
		case BLOB_BMP:
			pr_debug("%s: bmptype=%d length=%d resolutiontype=%d\n",
				func, e->bentry.bmp_type,
				e->bentry.bmp_size, e->bentry.bmp_res);
			break;
		default:
			pr_error("%s: blobtype %d is not supported\n", func, t);
			break;
	}
}

static uint8_t parse_entry(enum tegrabl_blob_type t, union blob_entry *e,
						   uint32_t *offset, uint32_t *length)
{
	pr_debug("%s: blobtype= %d\n", __func__, t);
	switch(t) {
		case BLOB_UPDATE:
			*offset = e->uentry.image_offset;
			*length = e->uentry.image_size;
			break;
		case BLOB_BMP:
			*offset = e->bentry.bmp_offset;
			*length = e->bentry.bmp_size;
			break;
		default:
			pr_error("%s: BlobType not supported.\n", __func__);
			return 0;
	}
	return 1;
}

static tegrabl_error_t blob_decompress(void **blob_buf, uint32_t *blob_asize,
									   void *header, uint32_t hdr_size,
									   uint32_t data_size, decompressor *decomp)
{
	void *outbuf = NULL;
	uint32_t outbuf_size = 0;
	uint32_t decomp_size = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t *inbuf = (uint8_t *)header;
	struct blob_header *blobheader;

	if (!header) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	blobheader = (struct blob_header *)header;
	outbuf_size = blobheader->uncomp_size;
	outbuf = tegrabl_malloc(outbuf_size);
	if (NULL == outbuf) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		pr_error("%s: Not enough memory\n", __func__);
		goto fail;
	}

	/* header is uncompressed, copy directly */
	memmove(outbuf, inbuf, hdr_size);

	decomp_size = outbuf_size;
	err = do_decompress(decomp, (uint8_t *)inbuf + hdr_size,
						data_size - hdr_size, (uint8_t *)outbuf + hdr_size,
						&decomp_size);

	if (TEGRABL_NO_ERROR != err) {
		pr_error("decompression failed (err=%d)\n", err);
		goto fail;
	}

	if (decomp_size != outbuf_size - hdr_size) {
		pr_error("decompress failed, expect:%dB, decompress:%dB\n",
				 outbuf_size - hdr_size, decomp_size);
		goto fail;
	}

	/* update blob size */
	blobheader = (struct blob_header *)outbuf;
	blobheader->size = outbuf_size;

	pr_debug("blob is decompressed successfully, size:%dB\n",
			 outbuf_size);

	*blob_buf = outbuf;
	*blob_asize = outbuf_size;
	return err;

fail:
	if (outbuf) {
		tegrabl_free(outbuf);
	}
	return err;
}

tegrabl_error_t tegrabl_blob_init(char *part_name, uint8_t *bptr,
								  tegrabl_blob_handle *bhdl)
{
	struct blob_info *bh = NULL;
	void *header = NULL;
	uint32_t hdr_size;
	uint32_t data_size = 0;
	size_t bh_size = 0;
	size_t header_asize = 0;
	struct signed_header *shdr = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition partition;
	decompressor *decomp = NULL;
	void *blob_buf = NULL;
	uint32_t blob_asize = 0;

	hdr_size =	sizeof(struct signed_header) > sizeof(struct blob_header) ?
			   sizeof(struct signed_header) : sizeof(struct blob_header);

	bh_size = sizeof(struct blob_info);
	bh = (struct blob_info *)tegrabl_malloc(bh_size);
	if (bh == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		pr_error("%s:%d not enough memory\n", __func__, __LINE__);
		goto fail;
	}

	memset(bh, 0, sizeof(struct blob_info));
	bh->info_mem_size = bh_size;

	if (bptr)
		header = (void *)bptr;
	else {
		error = tegrabl_partition_open(part_name, &partition);
		if (TEGRABL_NO_ERROR != error) {
			pr_error("Failed to open partition");
			goto fail;
		}

		header = tegrabl_malloc(hdr_size);
		if (header == NULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			pr_error("%s: Not enough memory\n", __func__);
			goto fail;
		}
		header_asize = hdr_size;

		pr_debug("%s: reading header from %s\n", __func__, part_name);
		error = tegrabl_partition_read(&partition, (void *)header, hdr_size);
		if (TEGRABL_NO_ERROR != error) {
			pr_error("Failed to read partition");
			goto fail;
		}
	}

	shdr = (struct signed_header *)header;
	if (!strncmp((char *)shdr->magic, SIGNED_UPDATE_MAGIC,
				 SIGNED_UPDATE_MAGIC_SIZE)) {
		bh->offset = sizeof(struct signed_header);
		data_size = shdr->actual_blob_size +
						shdr->signature_size +
						sizeof(struct signed_header);
	} else {
		struct blob_header *blobheader = (struct blob_header *)header;
		if (!strncmp((char *)blobheader->magic, UPDATE_MAGIC,
					UPDATE_MAGIC_SIZE)) {
			bh->offset = 0;
			data_size = blobheader->size;
		} else {
			pr_error("%s: %s partition does not have valid Blob\n",
								__func__, part_name);
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
	}

	if (!bptr) {
		tegrabl_free(header);
		header = tegrabl_malloc(data_size);
		if (header == NULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			pr_error("%s: Not enough memory\n", __func__);
			goto fail;
		}
		header_asize = data_size;

		/* Seek back to origin */
		error = tegrabl_partition_seek(&partition, 0,
					TEGRABL_PARTITION_SEEK_SET);
		if (TEGRABL_NO_ERROR != error) {
			pr_error("Failed to seek partition");
			goto fail;
		}

		pr_debug("%s: reading blob from %s\n", __func__, part_name);
		error = tegrabl_partition_read(&partition, (void *)header, data_size);
		if (TEGRABL_NO_ERROR != error) {
			pr_error("Failed to read partition");
			goto fail;
		}

		struct blob_header *blobheader = (struct blob_header *)header;
		if (is_compressed_content(
				(uint8_t *)header + blobheader->entries_offset, &decomp)) {
			pr_info("decompressing %s blob ...\n", part_name);
			error = blob_decompress(&blob_buf, (uint32_t *)&blob_asize, header,
									blobheader->entries_offset, data_size,
									decomp);
			if (TEGRABL_NO_ERROR != error) {
				pr_error("failed to decompress blob, err=%d\n", error);
				goto fail;
			}

			/* replace blob header with decompressed one */
			tegrabl_free(header);
			header = blob_buf;
			header_asize = blob_asize;

#if PRINT_BLOB_CRC
			uint32_t blob_crc = 0;
			blob_crc = tegrabl_utils_crc32(0, header, blob_asize);
			pr_info("after decompressed %s blob crc32: 0x%08x\n", part_name,
					blob_crc);
#endif
		}
	}

	bh->start = (uint8_t *)header;
	bh->data_mem_size = (uint32_t)data_size;
fail:
	if (error != TEGRABL_NO_ERROR) {
		if (header && header_asize) {
			tegrabl_free(header);
		}
		if (bh) {
			tegrabl_free(bh);
		}
		bh = NULL;
	}

	*bhdl = (tegrabl_blob_handle)bh;
	return error;
}

inline uint32_t tegrabl_blob_get_size(tegrabl_blob_handle bh)
{
	struct blob_header *header = (struct blob_header *)bh;
	return header->size;
}

bool tegrabl_blob_is_signed(tegrabl_blob_handle b)
{
	struct blob_info *bh = (struct blob_info *)b;
	if (bh == NULL) {
		return false;
	}

	if (bh->start && bh->offset > 0) {
		return true;
	}

	return false;
}

tegrabl_error_t tegrabl_blob_get_type(tegrabl_blob_handle b,
									  enum tegrabl_blob_type *type)
{
	struct blob_info *bh = (struct blob_info *)b;
	struct blob_header *blobheader = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!(bh && bh->start)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	blobheader = (struct blob_header *)(bh->start + bh->offset);

	pr_debug("%s: blob-type is %d\n", __func__, blobheader->type);
	if (type) {
		*type = blobheader->type;
	}
fail:
	return error;
}

tegrabl_error_t tegrabl_blob_get_details(tegrabl_blob_handle b, uint8_t **blob,
										 uint32_t *bloblen)
{
	struct blob_info *bh = (struct blob_info *)b;
	struct blob_header *blobheader = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!(bh && bh->start)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	blobheader = (struct blob_header *)(bh->start + bh->offset);

	if (blob) {
		*blob = (uint8_t *)blobheader;
	}
	if (bloblen) {
		*bloblen = blobheader->size;
	}
fail:
	return error;
}

tegrabl_error_t tegrabl_blob_get_signature(tegrabl_blob_handle b,
										   uint32_t *size,
										   uint8_t **signature)
{
	struct blob_info *bh = (struct blob_info *)b;
	struct signed_header *shdr = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!(bh && bh->start && bh->offset)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (tegrabl_blob_is_signed(b) == false) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	shdr = (struct signed_header *)bh->start;

	if (signature) {
		*signature = bh->start +
					 sizeof(struct signed_header) +
					 shdr->actual_blob_size;
	}
	if (size) {
		*size = shdr->signature_size;
	}
fail:
	return error;
}

tegrabl_error_t tegrabl_blob_get_num_entries(tegrabl_blob_handle b,
											 uint32_t *num_entries)
{
	struct blob_info *bh = (struct blob_info *)b;
	struct blob_header *blobheader = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!(bh && bh->start)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	blobheader = (struct blob_header *)(bh->start + bh->offset);

	if (num_entries) {
		*num_entries = blobheader->num_entries;
	}
	pr_debug("%s: number of entries is %d\n", __func__, *num_entries);
fail:
	return error;
}

static tegrabl_error_t get_casted_entry(enum tegrabl_blob_type t,
	union blob_entry *in_entry, void **out_entry)
{
	struct tegrabl_image_entry *e = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_warn("WARNING: %s:Trying to read old entry structure\n",
					__func__);
	switch(t) {
		case BLOB_UPDATE:
			e = (struct tegrabl_image_entry *)tegrabl_calloc(1,
							sizeof(struct tegrabl_image_entry));
			if (!e) {
				pr_error("%s: Not enough memory\n", __func__);
				error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
				goto fail;
			}
			memset(e, 0, sizeof(struct tegrabl_image_entry));
			memcpy(e->partname, in_entry->uentry.partname,
				PARTITION_NAME_LENGTH);
			e->image_offset = in_entry->uentry.image_offset;
			e->image_size = in_entry->uentry.image_size;
			e->version = in_entry->uentry.version;
			e->op_mode = in_entry->uentry.op_mode;
			memcpy(e->spec_info, in_entry->uentry.spec_info,
				   IMG_SPEC_INFO_LENGTH);
			if (out_entry) {
				*out_entry = e;
			}
			break;
		case BLOB_BMP:
			if (out_entry) {
				*out_entry = in_entry;
			}
			break;
		default:
			pr_error("%s: blobtype %d is not valid\n", __func__, t);
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
	}
fail:
	return error;
}

tegrabl_error_t tegrabl_blob_get_entry(tegrabl_blob_handle b, uint32_t index,
									   void **entry)
{
	struct blob_info *bh = (struct blob_info *)b;
	struct blob_header *blobheader = NULL;
	union blob_entry *blobentry = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!(bh && bh->start)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	blobheader = (struct blob_header *)(bh->start + bh->offset);

	blobentry = (union blob_entry *)((uint8_t *)blobheader +
									blobheader->entries_offset);

	switch (blobheader->type) {
	case BLOB_UPDATE:
		blobentry = (union blob_entry *)((struct tegrabl_image_entry *)blobentry
										 + index);
		break;
	case BLOB_BMP:
		blobentry = (union blob_entry *)((struct tegrabl_bmp_entry *)blobentry
										 + index);
		break;
	default:
		pr_error("%s: blobtype %d is not valid\n", __func__, blobheader->type);
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	print_entry(__func__, blobheader->type, blobentry);
	if (entry) {
		*entry = blobentry;
	}
fail:
	return error;
}

tegrabl_error_t tegrabl_blob_get_entry_data(tegrabl_blob_handle b,
	uint32_t index, uint8_t **data, uint32_t *size)
{
	struct blob_info *bh = (struct blob_info *)b;
	uint32_t offset = 0;
	uint32_t length = 0;
	enum tegrabl_blob_type type = BLOB_NONE;
	struct blob_header *blob_header = NULL;
	union blob_entry *blob_entry = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!(bh && bh->start)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	blob_header = (struct blob_header *)(bh->start + bh->offset);

	blob_entry = (union blob_entry *)((uint8_t *)blob_header +
									blob_header->entries_offset);

	switch (blob_header->type) {
	case BLOB_UPDATE:
		blob_entry = (union blob_entry *)((struct tegrabl_image_entry *)
										  blob_entry + index);
		break;
	case BLOB_BMP:
		blob_entry = (union blob_entry *)((struct tegrabl_bmp_entry *)
										  blob_entry + index);
		break;
	default:
		pr_error("%s: blobtype %d is not valid\n", __func__, blob_header->type);
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	tegrabl_blob_get_type((tegrabl_blob_handle)bh, &type);

	print_entry(__func__, type, blob_entry);

	if (parse_entry(type, blob_entry, &offset, &length) == 0) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (data) {
		*data = bh->start + bh->offset + offset;
	}
	if (size) {
		*size = length;
	}
fail:
	return error;
}

void tegrabl_blob_close(tegrabl_blob_handle b)
{
	struct blob_info *bh = (struct blob_info *)b;
	if (!bh) {
		return;
	}

	if (bh->start && bh->data_mem_size) {
		tegrabl_free(bh->start);
	}

	tegrabl_free(bh);
}
