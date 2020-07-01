/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
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

	if (!dev) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context = (struct tegrabl_sata_context *)dev->priv_data;

	if (!context) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (ioctl) {
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	case TEGRABL_IOCTL_DEVICE_CACHE_FLUSH:
		error = tegrabl_sata_ahci_flush_device(context);
		break;
#endif
	default:
		pr_debug("Unknown ioctl %"PRIu32"\n", ioctl);
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

fail:
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

	if (!dev || !buffer) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context = (struct tegrabl_sata_context *)dev->priv_data;

	if (!context) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((block + count) > context->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	pr_debug("%s: start block = %d, count = %d\n", __func__, block, count);
	while (count != 0U) {
		bulk_count = MIN(count, SATA_MAX_READ_WRITE_SECTORS);
		error = tegrabl_sata_ahci_io(context, buf, block, bulk_count, false,
				TEGRABL_SATA_READ_TIMEOUT);

		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		block += bulk_count;
	}

fail:
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

	if (!dev || !buffer) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context = (struct tegrabl_sata_context *)dev->priv_data;

	if (!context) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((block + count) > context->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	pr_debug("%s: start block = %d, count = %d\n", __func__, block, count);

	while (count) {
		bulk_count = MIN(count, SATA_MAX_READ_WRITE_SECTORS);

		error = tegrabl_sata_ahci_io(context, (void *)buf, block, bulk_count,
				true, TEGRABL_SATA_WRITE_TIMEOUT);

		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		block += bulk_count;
	}

fail:
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
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(block);
	TEGRABL_UNUSED(count);
	TEGRABL_UNUSED(is_secure);

	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
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
	pr_debug("Default Context\n");
	context->mode = TEGRABL_SATA_MODE_AHCI;
	context->instance = instance;
	context->block_size_log2 = TEGRABL_SATA_SECTOR_SIZE_LOG2;
	context->speed = TEGRABL_SATA_INTERFACE_GEN2;
}

static tegrabl_error_t tegrabl_sata_register_region(
		struct tegrabl_sata_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	size_t block_size_log2 = context->block_size_log2;
	bnum_t block_count = context->block_count;
	struct tegrabl_bdev *user_dev = NULL;
	uint32_t device_id = 0;

	user_dev = tegrabl_calloc(1, sizeof(struct tegrabl_bdev));

	if (user_dev == NULL) {
		pr_debug("failed to allocate memory for sata priv data\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	device_id = TEGRABL_STORAGE_SATA << 16 | context->instance;
	pr_debug("sata device id %08x", device_id);

	tegrabl_blockdev_initialize_bdev(
			user_dev, device_id, block_size_log2, block_count);

	/* Fill bdev function pointers. */
	user_dev->read_block = tegrabl_sata_bdev_read_block;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	user_dev->write_block = tegrabl_sata_bdev_write_block;
	user_dev->erase = tegrabl_sata_bdev_erase;
#endif
	user_dev->close = tegrabl_sata_bdev_close;
	user_dev->ioctl = tegrabl_sata_bdev_ioctl;
	user_dev->priv_data = (void *)context;

	error = tegrabl_blockdev_register_device(user_dev);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_sata_bdev_open(uint32_t instance,
		struct tegrabl_uphy_handle *uphy)

{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_sata_context *context = NULL;

	if (init_done) {
		goto fail;
	}

	pr_debug("Initializing sata device instance %d\n", instance);

	context = tegrabl_malloc(sizeof(struct tegrabl_sata_context));

	if (context == NULL) {
		pr_debug("failed to allocate memory for sata context\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	memset(context, 0x0, sizeof(struct tegrabl_sata_context));

	pr_debug("Setting default context\n");
	tegrabl_sata_set_default_context(context, instance);

	pr_debug("Initializing sata controller\n");
	error = tegrabl_sata_ahci_init(context, uphy);
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("failed to initialize sata\n");
		goto fail;
	}

	pr_debug("Registering region\n");
	error = tegrabl_sata_register_region(context);
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("failed to register device region\n");
		goto fail;
	}

	init_done = true;

fail:
	return error;
}

