/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "tegrabl_error.h"
#include "tegrabl_utils.h"
#include "lz4.h"
#include "tegrabl_decompress_private.h"

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

	(void)cntxt;

	pr_debug("inbuf=0x%p (size:%d), outbuf=0x%p\n", cbuf, in_size, dbuf);

	/* MAGIC NUMBER: 4B */
	cbuf += 4;

	while (cbuf < cbuf_end) {
		/* block size: 4B */
		c_size = *((uint32_t *)cbuf);
		cbuf += 4;
		if (cbuf >= cbuf_end) {
			break;
		}

		d_size = dbuf_end - dbuf;
		pr_debug("compressed_size:%d max_write_size:%d\n", c_size, d_size);
		err = LZ4_decompress_safe((char *)cbuf, (char *)dbuf, c_size, d_size);

		if (err < 0) {
			pr_critical("failed to decompress, err=%d\n", err);
			ret = TEGRABL_ERR_INVALID;
			goto fail;
		}

		cbuf += c_size;
		dbuf += err;
		pr_debug("cbuf:%p dbuf:%p processed_size:%d written_size:%d\n", cbuf,
				 dbuf, c_size, (uint32_t)err);
	}

	*written_size = (uint32_t)(dbuf - (uint8_t *)out_buffer);

fail:
	pr_debug("total_processed_size:%d, total_written_size:%d\n",
			 (uint32_t)(cbuf - (uint8_t *)in_buffer),
			 (uint32_t)(dbuf - (uint8_t *)out_buffer));

	return ret;
}
