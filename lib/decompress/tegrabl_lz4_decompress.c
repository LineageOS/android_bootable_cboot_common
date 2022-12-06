/*
 * Copyright (c) 2016-2022, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_DECOMPRESS

#include "tegrabl_error.h"
#include "tegrabl_utils.h"
#include "lz4.h"
#include "tegrabl_decompress_private.h"

#define LZ4_LEGACY_MAGIC_NUMBER		(0x184C2102)
#define LZ4_CURRENT_MAGIC_NUMBER	(0x184D2204)

#define CONTENT_CSUM_FALG_MASK		(0x1<<2)
#define CONTENT_SIZE_FALG_MASK		(0x1<<3)
#define BLOCK_CHECKSUM_FLAG_MASK	(0x1<<4)
#define BLOCK_INDEP_FLAG_MASK		(0x1<<5)

#define MAGIC_NUMBER_SZ				(4)
#define ORIGINAL_CONTENT_SZ			(8)
#define BLOCK_SIZE_SZ				(4)
#define BLOCK_CHECKSUM_SZ			(4)

#define BLOCK_MAX_SIZE_MASK			(0x7<<4)
#define BLOCK_MAX_SIZE_SHIFT		(4)

tegrabl_error_t do_lz4_decompress(void *cntxt, void *in_buffer,
								  uint32_t in_size, void *out_buffer,
								  uint32_t outbuf_size, uint32_t *written_size)
{
	int32_t err = 0;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	uint8_t *cbuf = (uint8_t *)in_buffer;
	uint8_t *dbuf = (uint8_t *)out_buffer;
	uint8_t *cbuf_end = cbuf + in_size;
	uint8_t *dbuf_end = dbuf + outbuf_size;
	uint32_t c_size, d_size;
	uint32_t magic_number;
	uint8_t frame_flag, block_descriptor, header_csum;
	uint64_t content_size = 0;
	bool block_has_csum = false;

	(void)cntxt;

	pr_debug("inbuf=0x%p (size:%d), outbuf=0x%p\n", cbuf, in_size, dbuf);

	/* MAGIC NUMBER: 4B */
	magic_number = *(uint32_t *)cbuf;
	cbuf += MAGIC_NUMBER_SZ;

	switch (magic_number) {
	case LZ4_LEGACY_MAGIC_NUMBER:
		pr_debug("Content in legacy frame format\n");
		break;

	case LZ4_CURRENT_MAGIC_NUMBER:
		frame_flag = *cbuf++;
		if (frame_flag & BLOCK_CHECKSUM_FLAG_MASK) {
			block_has_csum = true;
		}
		block_descriptor = *cbuf++;
		if (frame_flag & CONTENT_SIZE_FALG_MASK) {
			memcpy((uint8_t *)&content_size, cbuf, sizeof(uint64_t));
			cbuf += ORIGINAL_CONTENT_SZ;
		}
		header_csum = *cbuf++;
		pr_debug("Frame header: flag:0x%x b_d:0x%x h_csum:0x%x\n", frame_flag,
				 block_descriptor, header_csum);
		break;

	default:
		pr_error("Magic(0x%08x) not supported\n", magic_number);
		ret = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	while (cbuf < cbuf_end) {
		/* block size: 4B */
		c_size = *((uint32_t *)cbuf);
		cbuf += BLOCK_SIZE_SZ;
		if (!c_size || cbuf >= cbuf_end) {
			break;
		}

		d_size = dbuf_end - dbuf;
		pr_debug("compressed_size:%d max_write_size:%d\n", c_size, d_size);
		err = LZ4_decompress_safe((char *)cbuf, (char *)dbuf, c_size, d_size);

		if (err < 0) {
			pr_critical("failed to decompress, err=%d\n", err);
			ret = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
			goto fail;
		}

		cbuf += c_size;
		dbuf += err;
		pr_debug("cbuf:%p dbuf:%p processed_size:%d written_size:%d\n", cbuf,
				 dbuf, c_size, (uint32_t)err);

		if (block_has_csum) {
			/* add block checksum if needed */
			cbuf += BLOCK_CHECKSUM_SZ;
		}
	}

	*written_size = (uint32_t)(dbuf - (uint8_t *)out_buffer);
	if (content_size && (content_size != *written_size)) {
		pr_error("Decompressed size doesn't match target\n");
		ret = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

fail:
	pr_debug("total_processed_size:%d, total_written_size:%d\n",
			 (uint32_t)(cbuf - (uint8_t *)in_buffer),
			 (uint32_t)(dbuf - (uint8_t *)out_buffer));

	return ret;
}
