/*
 * Copyright (c) 2021, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_NVME

#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <tegrabl_nvme.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <tegrabl_nvme_priv.h>
#include <tegrabl_nvme_err.h>
#include <tegrabl_blockdev.h>

static bool init_done;

/**
 * @brief Reads number of block from specified block into buffer
 *
 * @param dev Block device from which to read
 * @param buf Buffer for saving read content
 * @param block Start block from which read to start
 * @param count Number of blocks to read
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error..
 */
static tegrabl_error_t tegrabl_nvme_bdev_read_block(struct tegrabl_bdev *dev,
		 void *buffer, bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bulk_count = 0;
	struct tegrabl_nvme_context *context;
	uint8_t *buf = buffer;
	bnum_t start_block = block;
	bnum_t total_blocks = count;

	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_ERR_NVME_BDEV_READ_BLOCK);
		pr_error("%s: BAD_PARAMETER; error=0x%x, dev=%p, buffer=%p\n", __func__, error, dev, buffer);
		goto fail;
	}

	context = (struct tegrabl_nvme_context *)dev->priv_data;
	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, TEGRABL_ERR_NVME_BDEV_READ_BLOCK);
		pr_error("%s: INVALID; error=0x%x, context=%p\n", __func__, error, context);
		goto fail;
	}

	if ((block + (uint64_t)count) > context->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, TEGRABL_ERR_NVME_BDEV_READ_BLOCK);
		pr_error("%s: OVERFLOW; error=0x%x, [%lu, %u]\n", __func__, error,
				 (block + (uint64_t)count), context->block_count);
		goto fail;
	}

	pr_debug("%s: start block = 0x%x, count = 0x%x\n", __func__, block, count);
	while (count != 0U) {
		bulk_count = MIN(count, context->max_transfer_blk);
		error = tegrabl_nvme_rw_blocks(context, buf, block, bulk_count, false);

		if (error != TEGRABL_NO_ERROR) {
			pr_error("%s: READ ERROR; error=0x%x, block=%u, count=%u\n", __func__, error, block, bulk_count);
			goto fail;
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		block += bulk_count;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: error=0x%x, start_block=%u, count=%u\n", __func__, error, start_block, total_blocks);
	}

	return error;
}

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
/**
 * @brief Writes number of blocks from specified block with content from buffer
 *
 * @param dev Bio device in which to write
 * @param buf Buffer containing data to be written
 * @param block Start block from which write should start
 * @param count Number of blocks to write
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_nvme_bdev_write_block(struct tegrabl_bdev *dev,
			 const void *buffer, bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bulk_count = 0;
	struct tegrabl_nvme_context *context;
	const uint8_t *buf = buffer;
	bnum_t start_block = block;
	bnum_t total_blocks = count;

	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_ERR_NVME_BDEV_WRITE_BLOCK);
		pr_error("%s: BAD_PARAMETER; error=0x%x, dev=%p, buffer=%p\n", __func__, error, dev, buffer);
		goto fail;
	}

	context = (struct tegrabl_nvme_context *)dev->priv_data;
	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, TEGRABL_ERR_NVME_BDEV_WRITE_BLOCK);
		pr_error("%s: INVALID; error=0x%x, context=%p\n", __func__, error, context);
		goto fail;
	}

	if ((block + (uint64_t)count) > context->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, TEGRABL_ERR_NVME_BDEV_WRITE_BLOCK);
		pr_error("%s: OVERFLOW; error=0x%x, [%lu, %u]\n", __func__, error,
				(block + (uint64_t)count), context->block_count);
		goto fail;
	}

	pr_debug("%s: start block = %d, count = %d\n", __func__, block, count);

	while (count > 0UL) {
		bulk_count = MIN(count, context->max_transfer_blk);
		error = tegrabl_nvme_rw_blocks(context, (void *)buf, block, bulk_count, true);

		if (error != TEGRABL_NO_ERROR) {
			pr_error("%s: WRITE ERROR; error=0x%x, block=%u, count=%u\n", __func__, error, block, bulk_count);
			goto fail;
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		block += bulk_count;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: error=0x%x, start_block=%u, count=%u\n", __func__, error, start_block, total_blocks);
	}

	return error;
}
#endif

/**
 * @brief Closes the Bio device instance
 *
 * @param dev Bio device which is to be closed.
 */
static tegrabl_error_t tegrabl_nvme_bdev_close(struct tegrabl_bdev *dev)
{
	struct tegrabl_nvme_context *context;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev != NULL) && init_done) {
		if (dev->priv_data != NULL) {
			context = (struct tegrabl_nvme_context *)dev->priv_data;
			if (context != NULL) {
				tegrabl_nvme_free_buffers(context);
				error = tegrabl_nvme_reset_pcie(context);
				tegrabl_free(context);
				if (error != TEGRABL_NO_ERROR) {
					pr_error("%s: Failed tegrabl_nvme_reset_pcie; error=0x%x\n", __func__, error);
					error = TEGRABL_ERROR(error, TEGRABL_ERR_NVME_BDEV_CLOSE);
					goto fail;
				}
			}
		}
		init_done = false;
	}

fail:
	return error;
}

/**
 * @brief Fills member of context structure with default values
 *
 * @param context Context of NVME
 * @param instance NVME controller instance
 */
static void tegrabl_nvme_set_default_context(
		struct tegrabl_nvme_context *context, uint32_t instance)
{
	pr_trace("Default Context\n");
	context->instance = (uint8_t)instance;
	context->block_size_log2 = context->block_size_log2;
}

static tegrabl_error_t tegrabl_nvme_register_region(
		struct tegrabl_nvme_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	size_t block_size_log2 = context->block_size_log2;
	bnum_t block_count = (bnum_t)context->block_count;
	struct tegrabl_bdev *user_dev = NULL;
	uint32_t device_id = 0;

	user_dev = tegrabl_calloc(1, sizeof(struct tegrabl_bdev));

	if (user_dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, TEGRABL_ERR_NVME_REGISTER_REGION);
		pr_error("%s: NO MEMORY; error=0x%x, size=%u\n", __func__, error,
				 (uint32_t)sizeof(struct tegrabl_bdev));
		goto fail;
	}

	device_id = TEGRABL_STORAGE_NVME << 16 | context->instance;
	pr_info("nvme device id %08x\n", device_id);

	error = tegrabl_blockdev_initialize_bdev(user_dev, device_id, block_size_log2, block_count);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed BDEV INIT; error=0x%x\n", __func__, error);
		goto fail;
	}
	user_dev->buf_align_size = TEGRABL_NVME_BUF_ALIGN_SIZE;

	/* Fill bdev function pointers. */
	user_dev->read_block = tegrabl_nvme_bdev_read_block;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	user_dev->write_block = tegrabl_nvme_bdev_write_block;
#endif
	user_dev->close = tegrabl_nvme_bdev_close;
	user_dev->priv_data = (void *)context;

	error = tegrabl_blockdev_register_device(user_dev);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed REGISTER DEVICE; error=0x%x\n", __func__, error);
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_nvme_bdev_open(uint32_t instance)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_nvme_context *context = NULL;

	if (init_done) {
		goto fail;
	}

	pr_info("Initializing nvme device instance %d\n", instance);

	context = tegrabl_malloc(sizeof(struct tegrabl_nvme_context));

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, TEGRABL_ERR_NVME_BDEV_OPEN);
		pr_error("%s: NO MEMORY; error=0x%x, size=%u\n", __func__, error,
				 (uint32_t)sizeof(struct tegrabl_nvme_context));
		goto fail;
	}

	memset(context, 0x0, sizeof(struct tegrabl_nvme_context));

	context->instance = (uint8_t)instance;

	pr_info("Initializing nvme controller\n");
	error = tegrabl_nvme_init(context);
	if (error != TEGRABL_NO_ERROR) {
		pr_warn("%s: Failed NVME INIT; error=0x%x\n", __func__, error);
		goto fail;
	}

	pr_trace("Registering region\n");
	error = tegrabl_nvme_register_region(context);
	if (error != TEGRABL_NO_ERROR) {
		pr_warn("%s: Failed REGISTER REGION; error=0x%x\n", __func__, error);
		goto fail;
	}

	init_done = true;

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: Failed BDEV OPEN; error=0x%x\n", __func__, error);
		if (context)
			tegrabl_free(context);
	}

	return error;
}
