/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_SATA

#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <tegrabl_sata.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_sata_ahci.h>
#include <tegrabl_malloc.h>
#include <tegrabl_sata_err_aux.h>

static bool init_done;

/**
 * @brief Processes ioctl request.
 *
 * @param dev Block dev device registed while open
 * @param ioctl Ioctl number
 * @param argp Arguments for IOCTL call which might be updated or used
 * based on ioctl request.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_sata_bdev_ioctl(
		struct tegrabl_bdev *dev, uint32_t ioctl, void *argp)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_sata_context *context = NULL;

	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(ioctl);
	TEGRABL_UNUSED(argp);
	TEGRABL_UNUSED(context);

	if (dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_SATA_BDEV_IOCTL);
		TEGRABL_SET_ERROR_STRING(error, "dev: %p", dev);
		goto fail;
	}

	context = (struct tegrabl_sata_context *)dev->priv_data;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, TEGRABL_SATA_BDEV_IOCTL);
		TEGRABL_SET_ERROR_STRING(error, "context: %p", context);
		goto fail;
	}

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	if (ioctl == TEGRABL_IOCTL_DEVICE_CACHE_FLUSH) {
		return tegrabl_sata_ahci_flush_device(context);
	}
#endif

	error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, TEGRABL_SATA_BDEV_IOCTL);
	TEGRABL_SET_ERROR_STRING(error, "ioctl %d", ioctl);

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_COMMAND_FAILED, "ioctl %d", ioctl);
	}

	return error;
}


/**
* @brief Checks the transaction status and waits until the completion or error.
*
* @param xfer transfer info
* @param timeout  time to wait for the transfer
* @param xfer status_flag Address of the status flag. TEGRABL_BLOCKDEV_XFER_IN_PROGRESS,
* TEGRABL_BLOCKDEV_XFER_COMPLETE and TEGRABL_BLOCKDEV_XFER_FAILURE
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
static tegrabl_error_t tegrabl_sata_bdev_xfer_wait(
		struct tegrabl_blockdev_xfer_info *xfer,
		time_t timeout, uint8_t *status_flag)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_bdev *dev = NULL;
	bnum_t block = 0;
	bnum_t count = 0;
	uint32_t bulk_count = 0;
	struct tegrabl_sata_context *context = NULL;
	uint8_t *buf = NULL;
	time_t start_time_us;
	time_t elapsed_time_us;
	time_t timeout_us;

	if (xfer == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_SATA_BDEV_XFER_WAIT_1);
		TEGRABL_SET_ERROR_STRING(error, "xfer: %p", xfer);
		goto fail;
	}

	if (xfer->dev == NULL || xfer->buf == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, TEGRABL_SATA_BDEV_XFER_WAIT_1);
		TEGRABL_SET_ERROR_STRING(error, "dev: %p, buf: %p", xfer->dev, xfer->buf);
		goto fail;
	}

	dev = xfer->dev;
	context = (struct tegrabl_sata_context *)dev->priv_data;
	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, TEGRABL_SATA_BDEV_XFER_WAIT_2);
		TEGRABL_SET_ERROR_STRING(error, "context: %p", context);
		goto fail;
	}

	buf = xfer->buf;
	block = xfer->start_block;
	count = xfer->block_count;
	*status_flag = TEGRABL_BLOCKDEV_XFER_IN_PROGRESS;

	elapsed_time_us = 0;
	timeout_us = timeout;
	start_time_us = tegrabl_get_timestamp_us();

	while ((count > 0UL) && (elapsed_time_us <= timeout_us)) {
		if (context->xfer_info.dma_in_progress) {
			/* revisit this timeout */
			error = tegrabl_sata_xfer_complete(context, TEGRABL_SATA_READ_TIMEOUT);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
			bulk_count = context->xfer_info.bulk_count;
			count -= bulk_count;
			buf += (bulk_count << context->block_size_log2);
			block += bulk_count;
		}

		bulk_count = MIN(count, SATA_MAX_READ_WRITE_SECTORS);
		if (bulk_count <= 0UL) {
			break;
		}

		error = tegrabl_sata_ahci_xfer(context, buf, block, bulk_count,
				context->xfer_info.is_write,
				TEGRABL_SATA_READ_TIMEOUT, true);

		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_XFER_FAILED, "%s of %"PRIu32" blocks from block %"PRIu32,
				(context->xfer_info.is_write ? "write" : "read"), bulk_count, block);
			goto fail;
		}

		context->xfer_info.bulk_count = bulk_count;

		elapsed_time_us = tegrabl_get_timestamp_us() - start_time_us;
	}

	if (count > 0UL) {
		xfer->start_block = block;
		xfer->block_count = count;
	} else {
		*status_flag = TEGRABL_BLOCKDEV_XFER_COMPLETE;
	}

fail:
	return error;
}


/** @brief Executes the either read/write transaction based on the given
	transaction details. Transfer info block numbers to read/write and
	contains blocking/non-blocking etc.
 *
 *  @param xfer transfer info
 *
 *  @return Returns the pointer to the async io structure, NULL if fails.
 */
static tegrabl_error_t tegrabl_sata_bdev_xfer(struct tegrabl_blockdev_xfer_info *xfer)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_bdev *dev = NULL;
	struct tegrabl_sata_context *context = NULL;
	uint32_t bulk_count = 0;
	bnum_t block = 0;
	bnum_t count = 0;
	bool is_write;

	if (xfer == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_SATA_BDEV_XFER_1);
		TEGRABL_SET_ERROR_STRING(error, "xfer: %p", xfer);
		goto fail;
	}

	if (xfer->dev == NULL || xfer->buf == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, TEGRABL_SATA_BDEV_XFER_1);
		TEGRABL_SET_ERROR_STRING(error, "dev: %p, buf: %p", xfer->dev, xfer->buf);
		goto fail;
	}

	dev = xfer->dev;
	context = (struct tegrabl_sata_context *)dev->priv_data;
	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, TEGRABL_SATA_BDEV_XFER_2);
		TEGRABL_SET_ERROR_STRING(error, "context: %p", context);
		goto fail;
	}

	block = xfer->start_block;
	count = xfer->block_count;
	if ((block + (uint64_t)count) > (uint64_t)context->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, TEGRABL_SATA_BDEV_XFER_1);
		TEGRABL_SET_ERROR_STRING(error, "block %"PRIu64, "block %"PRIu64,
				block + (uint64_t)count, context->block_count);
		goto fail;
	}

	bulk_count = MIN(count, SATA_MAX_READ_WRITE_SECTORS);
	if (xfer->xfer_type == TEGRABL_BLOCKDEV_READ) {
		is_write = false;
	} else {
		is_write = true;
	}

	error = tegrabl_sata_ahci_xfer(context, xfer->buf, block, bulk_count, is_write,
			TEGRABL_SATA_READ_TIMEOUT, true);

	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_XFER_FAILED, "%s of %"PRIu32" blocks from block %"PRIu32,
				(is_write ? "write" : "read"), bulk_count, block);
		goto fail;
	}

	context->xfer_info.bdev_xfer_info = xfer;
	context->xfer_info.dma_in_progress = true;
	context->xfer_info.bulk_count = bulk_count;
	context->xfer_info.is_write = is_write;

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_XFER_FAILED, "sata");
	}

	return error;
}

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
static tegrabl_error_t tegrabl_sata_bdev_read_block(struct tegrabl_bdev *dev,
		 void *buffer, bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bulk_count = 0;
	struct tegrabl_sata_context *context = NULL;
	uint8_t *buf = buffer;
	bnum_t start_block = block;
	bnum_t total_blocks = count;

	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, TEGRABL_SATA_BDEV_READ_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "dev: %p, buffer: %p", dev, buffer);
		goto fail;
	}

	context = (struct tegrabl_sata_context *)dev->priv_data;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, TEGRABL_SATA_BDEV_READ_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "context: %p", context);
		goto fail;
	}

	if ((block + (uint64_t)count) > context->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, TEGRABL_SATA_BDEV_READ_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "block %"PRIu64, "block %"PRIu64,
				(block + (uint64_t)count), context->block_count);
		goto fail;
	}

	pr_trace("%s: start block = %d, count = %d\n", __func__, block, count);
	while (count != 0U) {
		bulk_count = MIN(count, SATA_MAX_READ_WRITE_SECTORS);
		error = tegrabl_sata_ahci_io(context, buf, block, bulk_count, false,
				TEGRABL_SATA_READ_TIMEOUT);

		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_READ_FAILED, "sector %"PRIu32" count %"PRIu32,
					block, bulk_count);
			goto fail;
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		block += bulk_count;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_READ_FAILED, "sector %"PRIu32" count %"PRIu32,
				start_block, total_blocks);
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
static tegrabl_error_t tegrabl_sata_bdev_write_block(struct tegrabl_bdev *dev,
			 const void *buffer, bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bulk_count = 0;
	struct tegrabl_sata_context *context = NULL;
	const uint8_t *buf = buffer;
	bnum_t start_block = block;
	bnum_t total_blocks = count;

	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				TEGRABL_SATA_BDEV_WRITE_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "dev: %p, buffer: %p", dev, buffer);
		goto fail;
	}

	context = (struct tegrabl_sata_context *)dev->priv_data;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, TEGRABL_SATA_BDEV_WRITE_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "context: %p", context);
		goto fail;
	}

	if ((block + (uint64_t)count) > context->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, TEGRABL_SATA_BDEV_WRITE_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "block %"PRIu64, "block %"PRIu64,
				(block + (uint64_t)count), context->block_count);
		goto fail;
	}

	pr_trace("%s: start block = %d, count = %d\n", __func__, block, count);

	while (count > 0UL) {
		bulk_count = MIN(count, SATA_MAX_READ_WRITE_SECTORS);

		error = tegrabl_sata_ahci_io(context, (void *)buf, block, bulk_count,
				true, TEGRABL_SATA_WRITE_TIMEOUT);

		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_WRITE_FAILED, "sector %"PRIu32" count %"PRIu32,
					block, bulk_count);
			goto fail;
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		block += bulk_count;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_READ_FAILED, "sector %"PRIu32" count %"PRIu32,
				start_block, total_blocks);
	}

	return error;
}

/**
 * @brief Erases specified number of blocks from specified block
 *
 * @param dev Bio device which is to be erased
 * @param block Start block from which erase should start
 * @param count Number of blocks to be erased
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error..
 */
static tegrabl_error_t tegrabl_sata_bdev_erase(
		struct tegrabl_bdev *dev, bnum_t block, bnum_t count, bool is_secure)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(block);
	TEGRABL_UNUSED(count);
	TEGRABL_UNUSED(is_secure);

	err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, TEGRABL_SATA_BDEV_ERASE);
	TEGRABL_SET_ERROR_STRING(err, "erase");
	return err;
}
#endif

/**
 * @brief Closes the Bio device instance
 *
 * @param dev Bio device which is to be closed.
 */
static tegrabl_error_t tegrabl_sata_bdev_close(struct tegrabl_bdev *dev)
{
	struct tegrabl_sata_context *context = NULL;

	if (dev != NULL) {
		if (dev->priv_data != NULL) {
			context = (struct tegrabl_sata_context *)dev->priv_data;
			tegrabl_sata_ahci_free_buffers(context);
			tegrabl_free(dev->priv_data);
		}
		init_done = false;
	}

	return TEGRABL_NO_ERROR;
}

/**
 * @brief Fills member of context structure with default values
 *
 * @param context Context of SATA
 * @param instance SATA controller instance
 */
static void tegrabl_sata_set_default_context(
		struct tegrabl_sata_context *context, uint32_t instance)
{
	pr_trace("Default Context\n");
	context->mode = TEGRABL_SATA_MODE_AHCI;
	context->instance = (uint8_t)instance;
	context->block_size_log2 = TEGRABL_SATA_SECTOR_SIZE_LOG2;
	context->speed = TEGRABL_SATA_INTERFACE_GEN2;
}

static tegrabl_error_t tegrabl_sata_register_region(
		struct tegrabl_sata_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	size_t block_size_log2 = context->block_size_log2;
	bnum_t block_count = (bnum_t)context->block_count;
	struct tegrabl_bdev *user_dev = NULL;
	uint32_t device_id = 0;

	user_dev = tegrabl_calloc(1, sizeof(struct tegrabl_bdev));

	if (user_dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, TEGRABL_SATA_REGISTER_REGION);
		TEGRABL_SET_ERROR_STRING(error, "%d", "user dev", (uint32_t)sizeof(struct tegrabl_bdev));
		goto fail;
	}

	device_id = TEGRABL_STORAGE_SATA << 16 | context->instance;
	pr_trace("sata device id %08x", device_id);

	error = tegrabl_blockdev_initialize_bdev(user_dev, device_id, block_size_log2, block_count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INIT_FAILED, "blockdev bdev");
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}
	user_dev->buf_align_size = TEGRABL_SATA_BUF_ALIGN_SIZE;

	/* Fill bdev function pointers. */
	user_dev->read_block = tegrabl_sata_bdev_read_block;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	user_dev->write_block = tegrabl_sata_bdev_write_block;
	user_dev->erase = tegrabl_sata_bdev_erase;
#endif
	user_dev->close = tegrabl_sata_bdev_close;
	user_dev->ioctl = tegrabl_sata_bdev_ioctl;
	user_dev->priv_data = (void *)context;
	user_dev->xfer = tegrabl_sata_bdev_xfer;
	user_dev->xfer_wait = tegrabl_sata_bdev_xfer_wait;

	error = tegrabl_blockdev_register_device(user_dev);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_REGISTER_FAILED, "instance %d", "blockdev", context->instance);
		TEGRABL_SET_HIGHEST_MODULE(error);
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_sata_bdev_open(uint32_t instance,
		struct tegrabl_uphy_handle *uphy,
		struct tegrabl_sata_platform_params *device_params)

{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_sata_context *context = NULL;

	pr_trace("transfer_speed = %d\n", device_params->transfer_speed);
	pr_trace("is_skip_init = %d\n", device_params->is_skip_init);

	if (init_done) {
		goto fail;
	}

	pr_trace("Initializing sata device instance %d\n", instance);

	context = tegrabl_malloc(sizeof(struct tegrabl_sata_context));

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, TEGRABL_SATA_BDEV_OPEN);
		TEGRABL_SET_ERROR_STRING(error, "%d", "context", (uint32_t)sizeof(struct tegrabl_sata_context));
		goto fail;
	}

	memset(context, 0x0, sizeof(struct tegrabl_sata_context));

	pr_trace("Setting default context\n");
	tegrabl_sata_set_default_context(context, instance);

	if (device_params != NULL) {
		pr_trace("Initializing the link in Gen %d speed\n", device_params->transfer_speed);
		context->speed = device_params->transfer_speed;
	}

	pr_trace("Initializing sata controller\n");
	error = tegrabl_sata_ahci_init(context, uphy);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INIT_FAILED, "ahci link");
		goto fail;
	}

	pr_trace("Registering region\n");
	error = tegrabl_sata_register_region(context);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_REGISTER_FAILED, "regions", "context");
		goto fail;
	}

	init_done = true;

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_OPEN_FAILED, "sata %d", instance);
	}

	return error;
}
