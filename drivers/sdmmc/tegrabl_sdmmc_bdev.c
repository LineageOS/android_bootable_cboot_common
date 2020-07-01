/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_SDMMC

#include "build_config.h"
#include <stdint.h>
#include <string.h>
#include <tegrabl_sdmmc_defs.h>
#include <tegrabl_sdmmc_bdev_local.h>
#include <tegrabl_sdmmc_rpmb.h>
#include <tegrabl_sdmmc_protocol.h>
#include <tegrabl_malloc.h>
#include <tegrabl_clock.h>
#include <tegrabl_module.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <inttypes.h>
#include <tegrabl_mb1_bct.h>

/*  The below variable is required to maintain the init status of each instance.
 */
static sdmmc_context_t *contexts[4] = {0, 0, 0, 0};

tegrabl_error_t sdmmc_bdev_ioctl(tegrabl_bdev_t *dev, uint32_t ioctl,
	void *args)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

#if !defined(CONFIG_ENABLE_SDMMC_RPMB)
	TEGRABL_UNUSED(args);
	TEGRABL_UNUSED(dev);
#else
	sdmmc_priv_data_t *priv_data = (sdmmc_priv_data_t *)dev->priv_data;
	uint32_t counter = 0;
	sdmmc_rpmb_context_t *rpmb_context = NULL;
#endif
	switch (ioctl) {
#if defined(CONFIG_ENABLE_SDMMC_RPMB)
	case TEGRABL_IOCTL_PROTECTED_BLOCK_KEY:
		error = sdmmc_rpmb_program_key(dev, args,
			 (sdmmc_context_t *)priv_data->context);
		break;
	case TEGRABL_IOCTL_GET_RPMB_WRITE_COUNTER:
		error = sdmmc_rpmb_get_write_counter(dev, args,
			&counter,
			rpmb_context,
			(sdmmc_context_t *)priv_data->context);
		break;
#endif
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	case TEGRABL_IOCTL_DEVICE_CACHE_FLUSH:
		break;
#endif
	default:
		pr_debug("Unknown ioctl %"PRIu32"\n", ioctl);
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 5);
	}

	return error;
}

tegrabl_error_t sdmmc_bdev_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	sdmmc_priv_data_t *priv_data = (sdmmc_priv_data_t *)dev->priv_data;

	/* Please note it is the responsibility of the block device layer */
	/* to validate block & count. */
	pr_debug("StartBlock= %d NumofBlock = %d\n", block, count);

	/* Call sdmmc_read with the given arguments. */
	error = sdmmc_io(dev, buf, block, count, 0,
				(sdmmc_context_t *)priv_data->context, priv_data->device);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	/* If read is successful return total bytes read. */
	return error;
}

#if !defined(CONFIG_DISABLE_EMMC_BLOCK_WRITE)
tegrabl_error_t sdmmc_bdev_write_block(tegrabl_bdev_t *dev,
	const void *buf, bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	sdmmc_priv_data_t *priv_data = (sdmmc_priv_data_t *)dev->priv_data;

	/* Please note it is the responsibility of the block device layer to */
	/* validate block & count. */

	/* Call sdmmc_write with the given arguments. */
	error = sdmmc_io(dev, (void *)buf, block, count, 1,
		(sdmmc_context_t *)priv_data->context, priv_data->device);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}
#endif

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
tegrabl_error_t sdmmc_bdev_erase(tegrabl_bdev_t *dev, bnum_t block,
	bnum_t count, bool is_secure)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	sdmmc_priv_data_t *priv_data = (sdmmc_priv_data_t *)dev->priv_data;

	TEGRABL_UNUSED(is_secure);
	/* Please note it is the responsibility of the block device layer */
	/* to validate offset & length. */
	/* This implementation interprets length & offset in terms of sectors. */

	/* Call erase functionality implemented in protocol layer. */
	error = sdmmc_erase(dev, block, count,
				(sdmmc_context_t *)priv_data->context, priv_data->device);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}
#endif

static tegrabl_error_t sdmmc_register_region(sdmmc_context_t *context)
{
	tegrabl_bdev_t *user_dev = NULL;
	tegrabl_bdev_t *boot_dev = NULL;
	uint32_t device_id;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	sdmmc_priv_data_t *boot_priv_data = NULL;
	sdmmc_priv_data_t *user_priv_data = NULL;
#if defined(CONFIG_ENABLE_SDMMC_RPMB)
	tegrabl_bdev_t *rpmb_dev = NULL;
	sdmmc_priv_data_t *rpmb_priv_data = NULL;
#endif
	context->count_devices = 1;

	boot_priv_data = tegrabl_calloc(1, sizeof(sdmmc_priv_data_t));

	/* Check for memory allocation. */
	if (!boot_priv_data) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	boot_priv_data->device = DEVICE_BOOT;
	boot_priv_data->context = (void *)context;

	/* Initialize block driver with boot_area region. */
	pr_debug("init boot device\n");

	device_id = TEGRABL_STORAGE_SDMMC_BOOT << 16 | context->controller_id;

	/* Allocate memory for boot device handle. */
	pr_debug("allocating memory for boot device\n");
	boot_dev = tegrabl_calloc(1, sizeof(tegrabl_bdev_t));

	/* Check for memory allocation. */
	if (!boot_dev) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		goto fail;
	}

	error = tegrabl_blockdev_initialize_bdev(boot_dev, device_id,
									 context->block_size_log2,
									 context->boot_blocks << 1);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	/* Fill bdev function pointers. */
	boot_dev->read_block = sdmmc_bdev_read_block;
#if !defined(CONFIG_DISABLE_EMMC_BLOCK_WRITE)
	boot_dev->write_block = sdmmc_bdev_write_block;
#endif
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	boot_dev->erase = sdmmc_bdev_erase;
#endif
	boot_dev->close = sdmmc_bdev_close;
	boot_dev->ioctl = sdmmc_bdev_ioctl;
	boot_dev->priv_data = (void *)boot_priv_data;

	/* Register sdmmc_boot device. */
	pr_debug("registering boot device\n");
	error = tegrabl_blockdev_register_device(boot_dev);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	user_priv_data = tegrabl_calloc(1, sizeof(sdmmc_priv_data_t));

	/* Check for memory allocation. */
	if (!user_priv_data) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 2);
		goto fail;
	}

	context->count_devices += 1;
	user_priv_data->device = DEVICE_USER;
	user_priv_data->context = (void *)context;

	/* Initialize block driver with user area region. */
	pr_debug("init user device\n");
	device_id = TEGRABL_STORAGE_SDMMC_USER << 16 | context->controller_id;

	/* Allocate memory for user device handle. */
	pr_debug("allocating memory for user device\n");
	user_dev = tegrabl_calloc(1, sizeof(tegrabl_bdev_t));

	/* Check for memory allocation. */
	if (!user_dev) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 3);
		goto fail;
	}

	error = tegrabl_blockdev_initialize_bdev(user_dev, device_id,
									 context->block_size_log2,
									 context->user_blocks);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	/* Fill bdev function pointers. */
	user_dev->read_block = sdmmc_bdev_read_block;
#if !defined(CONFIG_DISABLE_EMMC_BLOCK_WRITE)
	user_dev->write_block = sdmmc_bdev_write_block;
#endif
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	user_dev->erase = sdmmc_bdev_erase;
#endif
	user_dev->close = sdmmc_bdev_close;
	user_dev->ioctl = sdmmc_bdev_ioctl;
	user_dev->priv_data = (void *)user_priv_data;

	/* Register sdmmc_user device. */
	pr_debug("registering user device\n");
	error = tegrabl_blockdev_register_device(user_dev);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

#if defined(CONFIG_ENABLE_SDMMC_RPMB)
	rpmb_priv_data = tegrabl_calloc(1, sizeof(sdmmc_priv_data_t));

	/* Check for memory allocation. */
	if (!rpmb_priv_data) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 4);
		goto fail;
	}

	context->count_devices += 1;
	rpmb_priv_data->device = DEVICE_RPMB;
	rpmb_priv_data->context = (void *)context;

	/* Initialize block driver with rpmb area region. */
	pr_debug("init rpmb device\n");
	device_id = TEGRABL_STORAGE_SDMMC_RPMB << 16 | context->controller_id;

	/* Allocate memory for rpmb device handle. */
	pr_debug("allocating memory for rpmb device\n");
	rpmb_dev = tegrabl_calloc(1, sizeof(tegrabl_bdev_t));

	/* Check for memory allocation. */
	if (!rpmb_dev) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 5);
		goto fail;
	}

	error = tegrabl_blockdev_initialize_bdev(rpmb_dev, device_id,
									 RPMB_DATA_SIZE_LOG2,
									 context->rpmb_blocks);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	/* Fill bdev function pointers. */
	rpmb_dev->read_block = sdmmc_bdev_read_block;
#if !defined(CONFIG_DISABLE_EMMC_BLOCK_WRITE)
	rpmb_dev->write_block = sdmmc_bdev_write_block;
#endif
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	rpmb_dev->erase = sdmmc_bdev_erase;
#endif
	rpmb_dev->close = sdmmc_bdev_close;
	rpmb_dev->ioctl = sdmmc_bdev_ioctl;
	rpmb_dev->priv_data = (void *)rpmb_priv_data;

	/* Register sdmmc_rpmb device. */
	pr_debug("registering rpmb device\n");
	error = tegrabl_blockdev_register_device(rpmb_dev);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
#endif
fail:
	if (error && boot_dev) {
		tegrabl_free(boot_dev);
	}

	if (error && boot_priv_data) {
		tegrabl_free(boot_priv_data);
	}

	if (error && user_dev) {
		tegrabl_free(user_dev);
	}

	if (error && user_priv_data) {
		tegrabl_free(user_priv_data);
	}

#if defined(CONFIG_ENABLE_SDMMC_RPMB)
	if (error && rpmb_dev) {
		tegrabl_free(rpmb_dev);
	}

	if (error && rpmb_priv_data) {
		tegrabl_free(rpmb_priv_data);
	}
#endif

	return error;
}

tegrabl_error_t sdmmc_bdev_open(
		struct tegrabl_mb1bct_emmc_params *emmc_params, uint32_t flag)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	sdmmc_context_t *context = NULL;

	if (!emmc_params) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (emmc_params->instance > INSTANCE_3) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 6);
		goto fail;
	}

	pr_debug("instance: %d\n", emmc_params->instance);
	context = contexts[emmc_params->instance];

	/* Check if the handle is NULL or not. */
	if (context != NULL) {
		if (context->clk_src == emmc_params->clk_src &&
			context->best_mode == emmc_params->best_mode &&
			context->tap_value == emmc_params->tap_value &&
			context->trim_value == emmc_params->trim_value) {
			pr_info("sdmmc bdev is already initialized\n");
			goto fail;
		}
	} else {
		/* Allocate memory for context. */
		pr_debug("allocating memory for context\n");
		context = tegrabl_alloc(TEGRABL_HEAP_DMA, sizeof(sdmmc_context_t));

		/* Check for memory allocation. */
		if (!context) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 6);
			goto fail;
		}

		/* Initialize the memory with zero. */
		memset(context, 0x0, sizeof(sdmmc_context_t));
	}

	context->clk_src = emmc_params->clk_src;
	context->controller_id = emmc_params->instance;
	context->best_mode = emmc_params->best_mode;
	context->tap_value = emmc_params->tap_value;
	context->trim_value = emmc_params->trim_value;

	/* Call sdmmc_init to proceed with initialization. */
	pr_debug("sdmmc init\n");

	error = sdmmc_init(context->controller_id, context, flag);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	if (!contexts[context->controller_id]) {
		/* Fill the required function pointers and register the device. */
		pr_debug("sdmmc device register\n");
		error = sdmmc_register_region(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	contexts[emmc_params->instance] = context;
fail:

	if ((error != TEGRABL_NO_ERROR) && context) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, context);
	}

	return error;
}

tegrabl_error_t sdmmc_bdev_close(tegrabl_bdev_t *dev)
{
	sdmmc_priv_data_t *priv_data;
	sdmmc_context_t *context;

	if (dev == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 46);
	}

	if (dev->priv_data == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 47);
	}

	priv_data = (sdmmc_priv_data_t *)dev->priv_data;
	context = (sdmmc_context_t *)priv_data->context;

	/* Close allocated context for sdmmc. */
	if (priv_data && (context->count_devices == 1)) {
		contexts[context->controller_id] = NULL;
		tegrabl_dealloc(TEGRABL_HEAP_DMA, context);
	} else if (priv_data && context->count_devices) {
		context->count_devices--;
	} else {
		/* No Action Required */
	}
	if (priv_data != NULL) {
		tegrabl_free(priv_data);
	}

	return TEGRABL_NO_ERROR;
}
