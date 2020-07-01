/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_UFS

#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <tegrabl_error.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_ufs_int.h>
#include <tegrabl_ufs_bdev.h>
#include <tegrabl_debug.h>
#include <tegrabl_ufs_hci.h>
#include <tegrabl_ufs_int.h>
#include <tegrabl_malloc.h>

static bool init_done;
#define UFS_BLOCK_MAX 64
#define UFS_RW_BLOCK_MAX (MAX_PRDT_LENGTH * UFS_BLOCK_MAX)
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
static tegrabl_error_t tegrabl_ufs_bdev_ioctl(
		struct tegrabl_bdev *dev, uint32_t ioctl, void *argp)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_ufs_context *context = NULL;

	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(ioctl);
	TEGRABL_UNUSED(argp);

	if (!dev) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context = (struct tegrabl_ufs_context *)dev->priv_data;

	if (!context) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (ioctl) {
	default:
		pr_debug("Unknown ioctl %"PRIu32"\n", ioctl);
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

fail:
	return error;
}

#if defined(CONFIG_ENABLE_UFS_KPI)
static time_t last_read_start_time;
static time_t last_read_end_time;
static time_t total_read_time;
static uint32_t total_read_size;
#endif

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
static tegrabl_error_t tegrabl_ufs_bdev_read_block(
	struct tegrabl_bdev *dev,
	void *buffer, uint32_t block, uint32_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bulk_count = 0;
	struct tegrabl_ufs_context *context = NULL;
	uint8_t *buf = buffer;
#if defined(CONFIG_ENABLE_UFS_KPI)
	last_read_start_time = tegrabl_get_timestamp_us();
	total_read_size += count;
#endif
	if (!dev || !buffer) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context = (struct tegrabl_ufs_context *)dev->priv_data;

	if (!context) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((block + count) > context->boot_lun_num_blocks) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}


	while (count) {
		bulk_count = MIN(count, UFS_RW_BLOCK_MAX);
		error = tegrabl_ufs_read(block, 0,
			bulk_count, (uint32_t *)buf);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		block += bulk_count;
	}
#if defined(CONFIG_ENABLE_UFS_KPI)
	last_read_end_time = tegrabl_get_timestamp_us();
	total_read_time += last_read_end_time - last_read_start_time;
	pr_info("total read time is =%"PRIu64"\n", total_read_time);
	pr_info("total blocks is =%0x\n", total_read_size);
#endif
fail:
	return error;
}

#if defined(CONFIG_ENABLE_UFS_KPI)
static time_t last_write_start_time;
static time_t last_write_end_time;
static time_t total_write_time;
static uint32_t total_write_size;
#endif

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
static tegrabl_error_t
tegrabl_ufs_bdev_write_block(struct tegrabl_bdev *dev,
			const void *buffer, uint32_t block, uint32_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bulk_count = 0;
	struct tegrabl_ufs_context *context = NULL;
	const uint8_t *buf = buffer;

#if defined(CONFIG_ENABLE_UFS_KPI)
	last_write_start_time = tegrabl_get_timestamp_us();
	total_write_size += count;
#endif
	if (!dev || !buffer) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context = (struct tegrabl_ufs_context *)dev->priv_data;

	if (!context) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((block + count) > context->boot_lun_num_blocks) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	while (count) {
		bulk_count = MIN(count, UFS_RW_BLOCK_MAX);
		error = tegrabl_ufs_write(block, 0, bulk_count, (uint32_t *)buf);

		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		block += bulk_count;
	}
#if defined(CONFIG_ENABLE_UFS_KPI)
	last_write_end_time = tegrabl_get_timestamp_us();
	total_write_time += last_write_end_time - last_write_start_time;
	pr_info("total write time is =%"PRIu64"\n", total_write_time);
	pr_info("total blocks is =%0x\n", total_write_size);
#endif
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
static tegrabl_error_t tegrabl_ufs_bdev_erase(
		struct tegrabl_bdev *dev, bnum_t block, bnum_t count, bool is_secure)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(is_secure);
	pr_info("calling erase\n");
	error = tegrabl_ufs_erase(block, count);
	if (error != TEGRABL_NO_ERROR) {
		pr_warn("erase failed\n");
	}
	return TEGRABL_NO_ERROR;
}
#endif

/**
 * @brief Closes the Bio device instance
 *
 * @param dev Bio device which is to be closed.
 */
 static tegrabl_error_t tegrabl_ufs_bdev_close(struct tegrabl_bdev *dev)
{
	struct tegrabl_ufs_context *context = NULL;

	TEGRABL_UNUSED(context);
	if (dev) {
		if (dev->priv_data) {
			tegrabl_free(dev->priv_data);
		}
		tegrabl_ufs_deinit();
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

static tegrabl_error_t tegrabl_ufs_register_region(
		struct tegrabl_ufs_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	size_t block_size_log2 = context->block_size_log2;
	bnum_t block_count = context->boot_lun_num_blocks;
	struct tegrabl_bdev *user_dev = NULL;
	uint32_t device_id = 0;

	user_dev = tegrabl_calloc(1, sizeof(struct tegrabl_bdev));

	if (user_dev == NULL) {
		pr_debug("failed to allocate memory for ufs priv data\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	device_id = TEGRABL_STORAGE_UFS << 16;
	pr_debug("device id is %0x\n", device_id);
	pr_debug("block_size_log2 is %0x\n", (uint32_t)block_size_log2);
	error = tegrabl_blockdev_initialize_bdev(
			user_dev, device_id, block_size_log2, block_count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	/* Fill bdev function pointers. */
	user_dev->read_block = tegrabl_ufs_bdev_read_block;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	user_dev->write_block = tegrabl_ufs_bdev_write_block;
	user_dev->erase = tegrabl_ufs_bdev_erase;
#endif
	user_dev->close = tegrabl_ufs_bdev_close;
	user_dev->ioctl = tegrabl_ufs_bdev_ioctl;
	user_dev->priv_data = (void *)context;

	error = tegrabl_blockdev_register_device(user_dev);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_ufs_bdev_open(bool reinit)

{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_ufs_context *context = {0};
	struct tegrabl_ufs_params *params;

	init_done = reinit;

	context = tegrabl_malloc(sizeof(struct tegrabl_ufs_context));

	if (context == NULL) {
		pr_error("failed to allocate memory for ufs context\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	memset(context, 0x0, sizeof(struct tegrabl_ufs_context));
	tegrabl_ufs_get_params(0, &params);
	if (init_done) {
		context->init_done = init_done;
	}

	error = tegrabl_ufs_init(params, context);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("failed to initialize ufs\n");
		goto fail;
	}
	error = tegrabl_ufs_register_region(context);
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("failed to register device region\n");
		goto fail;
	}
	init_done = true;

fail:
	return error;
}

