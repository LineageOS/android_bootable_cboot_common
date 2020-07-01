/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_DECOMPRESS

#include "tegrabl_error.h"
#include "tegrabl_utils.h"
#include "tegrabl_decompress.h"
#include "string.h"

#include "tegrabl_decompress_private.h"

#define ADD_METHOD(_name, _magic1, _magic2, _init, _decompress, _end)	\
{																		\
	.name = _name,														\
	.magic = {_magic1, _magic2},										\
	.init = _init,														\
	.decompress = _decompress,											\
	.end = _end,														\
}

static decompressor decompressor_list[] = {
#ifdef CONFIG_ENABLE_ZLIB
	ADD_METHOD("zlib", 0x1f, 0x8b, zlib_init, zlib_decompress, zlib_end),
#endif
#ifdef CONFIG_ENABLE_LZF
	ADD_METHOD("lzf", 'Z', 'V', lzf_init, do_lzf_decompress, NULL),
#endif
#ifdef CONFIG_ENABLE_LZ4
	ADD_METHOD("lz4-legacy", 0x02, 0x21, NULL, do_lz4_decompress, NULL),
	ADD_METHOD("lz4", 0x04, 0x22, NULL, do_lz4_decompress, NULL),
#endif
};

decompressor *decompress_method(uint8_t *c_magic, uint32_t len)
{
	decompressor dc;
	int32_t id;

	if (len < 2) {
		pr_error("invalid magic id\n");
		return NULL;
	}

	/* search the decompression handler, exit at first match */
	for (id = 0; id < (int32_t)ARRAY_SIZE(decompressor_list); id++) {
		dc = decompressor_list[id];
		if ((dc.magic[0] == c_magic[0]) && (dc.magic[1] == c_magic[1])) {
			pr_debug("Decompressor handler found\n");
			return &decompressor_list[id];
		}
	}

	pr_debug("Decompressor handler NOT found\n");
	return NULL;
}

bool is_compressed_content(uint8_t *head_buf, decompressor **pdecomp)
{
	bool compressed = false;
	decompressor *decomp = NULL;

	pr_debug("magic id:%02x %02x\n", *head_buf, *(head_buf + 1));

	/* get decompression handler */
	decomp = decompress_method(head_buf, 2);
	if (!decomp) {
		pr_info("decompressor handler not found\n");
	} else {
		pr_info("found decompressor handler: %s\n", decomp->name);
		compressed = true;
	}

	*pdecomp = decomp;

	return compressed;
}

tegrabl_error_t do_decompress(decompressor *decomp, uint8_t *read_buffer,
							  uint32_t read_size, uint8_t *out_buffer,
							  uint32_t *outbuf_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *context = NULL;
	uint8_t *write_buffer = out_buffer;
	uint32_t written_size = 0;

	/* initialize decompressor algo if needed */
	if (strcmp(decomp->name, "lz4") && strcmp(decomp->name, "lz4-legacy")) {
		if (!decomp->init) {
			pr_critical("Decompressor init api not found\n");
			return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		}
		context = decomp->init(read_size);
		if (!context) {
			pr_critical("Decompressor init failed\n");
			return TEGRABL_ERROR(TEGRABL_ERR_INIT_FAILED, 0);
		}
		pr_debug("decompressor init DONE\n");
	}

	pr_debug("compressed-data: 0x%p, decompressed-data: 0x%p\n", read_buffer,
			 write_buffer);

	/* decompress compressed data */
	err = decomp->decompress(context, read_buffer, read_size, write_buffer,
							 *outbuf_size, &written_size);
	if (err != TEGRABL_NO_ERROR) {
		pr_critical("Failure during decompressing (err: %d)\n", err);
		return err;
	}

	/* call decompressor cleanup */
	if (decomp->end) {
		decomp->end(context);
	}

	pr_debug("decompress kernel successfully, uncompressed kernel size: %d\n",
			 written_size);

	*outbuf_size = written_size;

	return err;
}

