/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
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
#include <tegrabl_sd_bdev.h>
#include <tegrabl_sdmmc_bdev.h>
#include <tegrabl_sd_card.h>

static sdmmc_context_t *contexts[4] = {0, 0, 0, 0};

tegrabl_error_t sd_bdev_is_card_present(uint32_t *instance, bool *is_present)
{
	return tegrabl_sd_is_card_present(instance, is_present);
}

static tegrabl_error_t sd_register_region(sdmmc_context_t *context)
{
	tegrabl_bdev_t *user_dev = NULL;
	uint32_t device_id;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	sdmmc_priv_data_t *user_priv_data = NULL;

	context->count_devices = 1;

	user_priv_data = tegrabl_calloc(1, sizeof(sdmmc_priv_data_t));

	/* Check for memory allocation. */
	if (!user_priv_data) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	user_priv_data->device = DEVICE_USER;
	user_priv_data->context = (void *)context;

	/* Initialize block driver with sdcard. */
	pr_debug("init sdcard\n");

	device_id = TEGRABL_STORAGE_SDCARD << 16 | context->controller_id;

	/* Allocate memory for boot device handle. */
	pr_debug("allocating memory for boot device\n");
	user_dev = tegrabl_calloc(1, sizeof(tegrabl_bdev_t));

	/* Check for memory allocation. */
	if (!user_dev) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		goto fail;
	}

	error = tegrabl_blockdev_initialize_bdev(user_dev, device_id,
									 context->block_size_log2,
									 context->user_blocks);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	/* Fill bdev function pointers. */
	user_dev->read_block = sdmmc_bdev_read_block;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	user_dev->write_block = sdmmc_bdev_write_block;
	user_dev->erase = sdmmc_bdev_erase;
#endif
	user_dev->close = sdmmc_bdev_close;
	user_dev->ioctl = sdmmc_bdev_ioctl;
	user_dev->priv_data = (void *)user_priv_data;

	/* Register sdmmc_boot device. */
	pr_debug("registering user device\n");
	error = tegrabl_blockdev_register_device(user_dev);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	if (error && user_dev) {
		tegrabl_free(user_dev);
	}

	if (error && user_priv_data) {
		tegrabl_free(user_priv_data);
	}

	return error;
}

tegrabl_error_t sd_bdev_open(uint32_t instance)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	sdmmc_context_t *context = NULL;

	if (instance > INSTANCE_3) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 6);
		goto fail;
	}

	pr_debug("instance: %d\n", instance);
	context = contexts[instance];

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

	context->device_type = DEVICE_TYPE_SD;
	context->clk_src = TEGRABL_CLK_SRC_PLLP_OUT0;
	context->controller_id = instance;
	context->best_mode = TEGRABL_SDMMC_MODE_DDR52;
	context->tap_value = 9;
	context->trim_value = 5;

	/* Call sdmmc_init to proceed with initialization. */
	pr_debug("sdmmc init\n");

	error = sdmmc_init(context->controller_id, context, SDMMC_INIT);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	if (!contexts[context->controller_id]) {
		/* Fill the required function pointers and register the device. */
		pr_debug("sd device register\n");
		error = sd_register_region(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	contexts[instance] = context;
fail:

	if ((error != TEGRABL_NO_ERROR) && context) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, context);
	}

	/*TODO: fix this and remove delay*/
	tegrabl_mdelay(10);
	return error;
}

tegrabl_error_t sd_bdev_close(tegrabl_bdev_t *dev)
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
	}
	if (priv_data)
		tegrabl_free(priv_data);

	return TEGRABL_NO_ERROR;
}
