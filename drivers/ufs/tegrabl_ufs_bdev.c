/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All rights reserved.
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
#include <tegrabl_ufs_rpmb.h>
#include "tegrabl_ufs_local.h"

static bool init_done;
static bool bdev_registration;
#define UFS_BLOCK_MAX 64U
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
#if defined(CONFIG_ENABLE_UFS_RPMB)
	uint32_t counter = 0;
#else
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(ioctl);
	TEGRABL_UNUSED(argp);
#endif
	if (dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context = (struct tegrabl_ufs_context *)dev->priv_data;

	if ((context == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (ioctl) {
#if defined(CONFIG_ENABLE_UFS_RPMB)
	case TEGRABL_IOCTL_GET_RPMB_WRITE_COUNTER:
		error = ufs_rpmb_get_write_counter(dev, argp,
				&counter,
				NULL,
				(void *)context);
		break;
	case TEGRABL_IOCTL_PROTECTED_BLOCK_KEY:
		error = ufs_rpmb_program_key(dev, argp,
				context);
		break;
#endif
	case TEGRABL_IOCTL_BLOCK_DEV_SUSPEND:
		error = tegrabl_ufs_hibernate_enter();
		break;
	default:
		pr_debug("Unknown ioctl %"PRIu32"\n", ioctl);
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		break;
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


/** @brief Executes the either read/write transaction based on the given
	transaction details. Transfer info block numbers to read/write and
	contains blocking/non-blocking etc.
 *
 *  @param xfer transfer info
 *
 *  @return Returns the pointer to the async io structure, NULL if fails.
 */
static tegrabl_error_t tegrabl_ufs_blockdev_xfer(
		struct tegrabl_blockdev_xfer_info *xfer)
{

	struct tegrabl_bdev *dev = NULL;
	uint32_t block = 0;
	uint32_t count = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bulk_count = 0;
	struct ufs_priv_data *priv_data = NULL;
	struct tegrabl_ufs_context *context = NULL;
	uint8_t *buf = NULL;

	if ((xfer == NULL) || ((xfer->dev == NULL)) || (xfer->buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((xfer->xfer_type != (uint32_t)(TEGRABL_BLOCKDEV_READ)) || (!xfer->is_non_blocking)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	dev = xfer->dev;
	buf = xfer->buf;
	block = xfer->start_block;
	count = xfer->block_count;

	priv_data = (struct ufs_priv_data *)dev->priv_data;

	context = (struct tegrabl_ufs_context *)priv_data->context;

	if ((context == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((block + count) > dev->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	if (count != 0U) {
		bulk_count = MIN(count, UFS_RW_BLOCK_MAX);
		error = tegrabl_ufs_xfer(priv_data->lun_id, block, 0,
				bulk_count, (uint32_t *)buf);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		context->xfer_info.dma_in_progress = true;
		context->xfer_info.bulk_count = bulk_count;
	}

fail:
	return error;
}

/**
* @brief Checks the transaction status and waits until the completion or error.
*
* @param xfer transfer info
* @param timeout  time to wait for the transfer
* @param xfer status_flag Address of the status flag. XFER_IN_PROGRESS, XFER_COMPLETE
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
static tegrabl_error_t tegrabl_ufs_blockdev_xfer_wait(
		struct tegrabl_blockdev_xfer_info *xfer,
		time_t timeout, uint8_t *status_flag)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_bdev *dev = NULL;
	bnum_t block = 0;
	bnum_t count = 0;
	uint32_t bulk_count = 0;
	struct ufs_priv_data *priv_data = NULL;
	struct tegrabl_ufs_context *context = NULL;
	uint8_t *buf = NULL;
	time_t start_time_us;
	time_t elapsed_time_us;
	time_t timeout_us;

	if ((xfer == NULL) || (xfer->dev == NULL) || (xfer->buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((xfer->xfer_type != TEGRABL_BLOCKDEV_READ) || (!xfer->is_non_blocking)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	dev = xfer->dev;
	buf = xfer->buf;
	block = xfer->start_block;
	count = xfer->block_count;
	*status_flag = TEGRABL_BLOCKDEV_XFER_IN_PROGRESS;

	priv_data = (struct ufs_priv_data *)dev->priv_data;
	context = (struct tegrabl_ufs_context *)priv_data->context;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	elapsed_time_us = 0;
	timeout_us = timeout;
	start_time_us = tegrabl_get_timestamp_us();

	while ((count > 0UL) && (elapsed_time_us <= timeout_us)) {
		if (context->xfer_info.dma_in_progress) {
			bulk_count = context->xfer_info.bulk_count;
			error = tegrabl_ufs_rw_check_complete(bulk_count, (uint32_t *)buf);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
			count -= bulk_count;
			buf += (bulk_count << context->block_size_log2);
			block += bulk_count;
		}
		bulk_count = MIN(count, UFS_RW_BLOCK_MAX);
		if (bulk_count <= 0UL) {
			break;
		}

		error = tegrabl_ufs_xfer(priv_data->lun_id, block, 0, bulk_count, (uint32_t *)buf);

		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		elapsed_time_us = tegrabl_get_timestamp_us() - start_time_us;
	}
	if (count > 0UL) {
		xfer->start_block = block;
		xfer->block_count = count;
		xfer->buf = buf;
	} else {
		*status_flag = TEGRABL_BLOCKDEV_XFER_COMPLETE;
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
static tegrabl_error_t tegrabl_ufs_bdev_read_block(
	struct tegrabl_bdev *dev,
	void *buffer, uint32_t block, uint32_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bulk_count = 0;
	struct ufs_priv_data *priv_data = NULL;
	struct tegrabl_ufs_context *context = NULL;
	uint8_t *buf = buffer;
#if defined(CONFIG_ENABLE_UFS_KPI)
	last_read_start_time = tegrabl_get_timestamp_us();
	total_read_size += count;
#endif

	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	priv_data = (struct ufs_priv_data *)dev->priv_data;
	context = (struct tegrabl_ufs_context *)priv_data->context;

	if ((context == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((block + count) > dev->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	while (count != 0U) {
		bulk_count = MIN(count, UFS_RW_BLOCK_MAX);
		error = tegrabl_ufs_read(priv_data->lun_id, block, 0,
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
	pr_trace("Total read time is =%"PRIu64"\n", total_read_time);
	pr_trace("Total blocks is =%0x\n", total_read_size);
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
	struct ufs_priv_data *priv_data = NULL;
	struct tegrabl_ufs_context *context = NULL;
	const uint8_t *buf = buffer;

#if defined(CONFIG_ENABLE_UFS_KPI)
	last_write_start_time = tegrabl_get_timestamp_us();
	total_write_size += count;
#endif
	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	priv_data = (struct ufs_priv_data *)dev->priv_data;
	context = (struct tegrabl_ufs_context *)priv_data->context;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if ((block + count) > dev->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	while (count != 0U) {
		bulk_count = MIN(count, UFS_RW_BLOCK_MAX);
		error = tegrabl_ufs_write(priv_data->lun_id, block, 0, bulk_count, (uint32_t *)buf);

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
	pr_trace("Total write time is =%"PRIu64"\n", total_write_time);
	pr_trace("Total blocks is =%0x\n", total_write_size);
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
	struct ufs_priv_data *priv_data = NULL;

	if (dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		return error;
	}
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(is_secure);
	pr_trace("Calling erase\n");
	priv_data = (struct ufs_priv_data *)dev->priv_data;
	error = tegrabl_ufs_erase(dev, priv_data->lun_id, block, count);
	if (error != TEGRABL_NO_ERROR) {
		pr_warn("Erase failed\n");
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
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(context);
	if (dev != NULL) {
		if (dev->priv_data != NULL) {
			tegrabl_free(dev->priv_data);
		}
		error = tegrabl_ufs_deinit();
		init_done = false;
	}
	return error;
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
	uint32_t block_size_log2 = 0;
	bnum_t block_count = 0;
	struct tegrabl_bdev *user_dev = NULL;
	struct tegrabl_bdev *ufs_boot_dev = NULL;
	struct ufs_priv_data *boot_priv_data = NULL;
	struct ufs_priv_data *user_priv_data = NULL;
	uint32_t device_id = 0;
	uint8_t *buffer;
#if defined(CONFIG_ENABLE_UFS_RPMB)
	tegrabl_bdev_t *rpmb_dev = NULL;
#endif

	/*Allocate memory for ufs boot and user priv data and devices*/
	buffer = tegrabl_calloc(1, (TOTAL_UFS_LUNS * sizeof(struct ufs_priv_data)) +
					(TOTAL_UFS_LUNS * sizeof(struct tegrabl_bdev)));
	if (buffer == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	boot_priv_data = (struct ufs_priv_data *)buffer;
	boot_priv_data->lun_id = BOOT_LUN_ID;
	boot_priv_data->context = (void *)context;

	/* Initialize block driver with boot area region. */
	ufs_boot_dev = (struct tegrabl_bdev *)(buffer + (TOTAL_UFS_LUNS * sizeof(struct ufs_priv_data)));

	device_id = ((uint32_t)TEGRABL_STORAGE_UFS) << 16U;
	error =  tegrabl_ufs_get_lun_capacity(boot_priv_data->lun_id, &block_size_log2, &block_count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	pr_trace("Device id is %0x\n", device_id);
	pr_trace("block_size_log2 is %0x\n", (uint32_t)block_size_log2);
	error = tegrabl_blockdev_initialize_bdev(
			ufs_boot_dev, device_id, block_size_log2, block_count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	ufs_boot_dev->buf_align_size = TEGRABL_UFS_BUF_ALIGN_SIZE;
	/* Fill bdev function pointers. */
	ufs_boot_dev->read_block = tegrabl_ufs_bdev_read_block;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	ufs_boot_dev->write_block = tegrabl_ufs_bdev_write_block;
	ufs_boot_dev->erase = tegrabl_ufs_bdev_erase;
#endif
	ufs_boot_dev->close = tegrabl_ufs_bdev_close;
	ufs_boot_dev->ioctl = tegrabl_ufs_bdev_ioctl;
	ufs_boot_dev->xfer = tegrabl_ufs_blockdev_xfer;
	ufs_boot_dev->xfer_wait = tegrabl_ufs_blockdev_xfer_wait;
	ufs_boot_dev->priv_data = (void *)boot_priv_data;

	error = tegrabl_blockdev_register_device(ufs_boot_dev);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
	}
	pr_info("registered UFS Boot Area success\n");

	/* register USER partition area */
	user_priv_data = (struct ufs_priv_data *)(buffer + sizeof(struct ufs_priv_data));
	user_priv_data->lun_id = USER_LUN_ID;
	user_priv_data->context = (void *)context;

	user_dev = (struct tegrabl_bdev *)(buffer + (TOTAL_UFS_LUNS * sizeof(struct ufs_priv_data)) +
				(sizeof(struct tegrabl_bdev)));
	device_id = ((uint32_t)TEGRABL_STORAGE_UFS_USER) << 16U;
	block_count = 0;
	block_size_log2 = 0;
	error =  tegrabl_ufs_get_lun_capacity(boot_priv_data->lun_id, &block_size_log2, &block_count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}
	pr_trace("Device id is %0x\n", device_id);
	pr_trace("block_size_log2 is %0x\n", (uint32_t)block_size_log2);
	error = tegrabl_blockdev_initialize_bdev(
			user_dev, device_id, block_size_log2, block_count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	user_dev->buf_align_size = TEGRABL_UFS_BUF_ALIGN_SIZE;

	/* Fill bdev function pointers. */
	user_dev->read_block = tegrabl_ufs_bdev_read_block;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	user_dev->write_block = tegrabl_ufs_bdev_write_block;
	user_dev->erase = tegrabl_ufs_bdev_erase;
#endif
	user_dev->close = tegrabl_ufs_bdev_close;
	user_dev->ioctl = tegrabl_ufs_bdev_ioctl;
	user_dev->xfer = tegrabl_ufs_blockdev_xfer;
	user_dev->xfer_wait = tegrabl_ufs_blockdev_xfer_wait;
	user_dev->priv_data = (void *)user_priv_data;

	error = tegrabl_blockdev_register_device(user_dev);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
	}
	pr_debug("Registered UFS USER AREA success\n");
#if defined(CONFIG_ENABLE_UFS_RPMB)
	/* Check if RPMB is availble */
	if (tegrabl_ufs_is_rpmb_lun_supported() != 0U) {
		/* Query RPMB descriptor */
		error =  tegrabl_ufs_get_lun_capacity((uint8_t)UFS_UPIU_RPMB_WLUN, &block_size_log2,
				&block_count);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
		}

		/* Register RPMB partition area */
		rpmb_dev = tegrabl_calloc(1, sizeof(struct tegrabl_bdev));

		if (rpmb_dev == NULL) {
			pr_error("Failed to allocate memory for ufs priv data\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}

		device_id = ((uint32_t)TEGRABL_STORAGE_UFS_RPMB) << 16U;
		pr_trace("Device id is %0x\n", device_id);
		pr_trace("Block_size_log2 is %0u, block_count us %0u\n", block_size_log2, block_count);
		error = tegrabl_blockdev_initialize_bdev(
				rpmb_dev, device_id, block_size_log2, block_count);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		rpmb_dev->buf_align_size = TEGRABL_UFS_BUF_ALIGN_SIZE;
		/* Fill bdev function pointers. */
		rpmb_dev->read_block = NULL;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
		rpmb_dev->write_block = NULL;
		rpmb_dev->erase = NULL;
#endif
		rpmb_dev->close = tegrabl_ufs_bdev_close;
		rpmb_dev->ioctl = tegrabl_ufs_bdev_ioctl;
		rpmb_dev->priv_data = (void *)context;

		error = tegrabl_blockdev_register_device(rpmb_dev);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
		}
		pr_debug("Registered UFS rpmb device success\n");
	}
#endif
	bdev_registration = true;
fail:
#if defined(CONFIG_ENABLE_UFS_RPMB)
	if (rpmb_dev != NULL) {
		tegrabl_free(rpmb_dev);
	}
#endif
	return error;
}

tegrabl_error_t tegrabl_ufs_bdev_open(bool reinit, struct tegrabl_ufs_platform_params *ufs_params)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_ufs_context *context = {0};
	struct tegrabl_ufs_params *params;

	pr_trace("max_hs_mode = %d\n", ufs_params->max_hs_mode);
	pr_trace("max_pwm_mode = %d\n", ufs_params->max_pwm_mode);
	pr_trace("max_active_lanes = %d\n", ufs_params->max_active_lanes);
	pr_trace("page_align_size = %d\n", ufs_params->page_align_size);
	pr_trace("enable_hs_modes = %d\n", ufs_params->enable_hs_modes);
	pr_trace("enable_fast_auto_mode = %d\n", ufs_params->enable_fast_auto_mode);
	pr_trace("enable_hs_rate_b = %d\n", ufs_params->enable_hs_rate_b);
	pr_trace("enable_hs_rate_a = %d\n", ufs_params->enable_hs_rate_a);
	pr_trace("ufs_init_done = %d\n", ufs_params->ufs_init_done);
	pr_trace("skip_hs_mode_switch = %d\n", ufs_params->skip_hs_mode_switch);

	init_done = reinit;

	context = tegrabl_malloc(sizeof(struct tegrabl_ufs_context));

	if (context == NULL) {
		pr_error("Failed to allocate memory for ufs context\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	memset(context, 0x0, sizeof(struct tegrabl_ufs_context));
	tegrabl_ufs_get_params(0, ufs_params, &params);
	if (init_done) {
		context->init_done = 1UL;
	}

	error = tegrabl_ufs_init(params, context);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("UFS initialization failed\n");
		goto fail;
	}
	if (bdev_registration == false) {
		error = tegrabl_ufs_register_region(context);
		if ((error != TEGRABL_NO_ERROR)) {
			pr_error("Failed to register device region\n");
			goto fail;
		}
	}
	init_done = true;

fail:
	return error;
}

