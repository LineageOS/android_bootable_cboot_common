/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
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
#include <tegrabl_sdmmc_bdev.h>
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
#include <tegrabl_blockdev.h>

/*  The below variable is required to maintain the init status of each instance.
 */
static struct tegrabl_sdmmc *contexts[MAX_SDMMC_INSTANCES];

tegrabl_error_t sdmmc_bdev_ioctl(tegrabl_bdev_t *dev, uint32_t ioctl,
	void *args)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	sdmmc_priv_data_t *priv_data = (sdmmc_priv_data_t *)dev->priv_data;

#if defined(CONFIG_ENABLE_SDMMC_RPMB)
	uint32_t counter = 0;
	sdmmc_rpmb_context_t *rpmb_context = NULL;
#else
	TEGRABL_UNUSED(args);
	TEGRABL_UNUSED(dev);
#endif

	switch (ioctl) {
	case TEGRABL_IOCTL_SEND_STATUS:
		error = sdmmc_send_status((struct tegrabl_sdmmc *)priv_data->context);
		break;
	case TEGRABL_IOCTL_DEVICE_CACHE_FLUSH:
		break;
#if defined(CONFIG_ENABLE_SDMMC_RPMB)
	case TEGRABL_IOCTL_PROTECTED_BLOCK_KEY:
		error = sdmmc_rpmb_program_key(dev, args, (struct tegrabl_sdmmc *)priv_data->context);
		break;
	case TEGRABL_IOCTL_GET_RPMB_WRITE_COUNTER:
		error = sdmmc_rpmb_get_write_counter(dev, args,	&counter, rpmb_context,
					(struct tegrabl_sdmmc *)priv_data->context);
		break;
#endif
	default:
		pr_debug("Unknown ioctl %"PRIu32"\n", ioctl);
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 5);
		break;
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
	pr_trace("StartBlock= %d NumofBlock = %d\n", block, count);

	/* Call sdmmc_read with the given arguments. */
	error = sdmmc_io(dev, buf, block, count, 0,
				(struct tegrabl_sdmmc *)priv_data->context, priv_data->device, false);
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
		(struct tegrabl_sdmmc *)priv_data->context, priv_data->device, false);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}
#endif

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
static tegrabl_error_t sdmmc_bdev_xfer_wait(struct tegrabl_blockdev_xfer_info *xfer, time_t timeout,
		uint8_t *status)
{
	return sdmmc_xfer_wait(xfer, timeout, status);
}

static tegrabl_error_t sdmmc_bdev_xfer(struct tegrabl_blockdev_xfer_info *xfer)
{
	tegrabl_bdev_t *dev = xfer->dev;
	sdmmc_priv_data_t *priv_data = (sdmmc_priv_data_t *)dev->priv_data;
	struct tegrabl_sdmmc *hsdmmc = (struct tegrabl_sdmmc *)priv_data->context;

	return sdmmc_io(dev, (void *)xfer->buf, xfer->start_block, xfer->block_count, 1, hsdmmc,
			priv_data->device, true);
}

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
				(struct tegrabl_sdmmc *)priv_data->context, priv_data->device);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}
#endif

static tegrabl_error_t sdmmc_register_region(struct tegrabl_sdmmc *hsdmmc)
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
	hsdmmc->count_devices = 1;

	boot_priv_data = tegrabl_calloc(1, sizeof(sdmmc_priv_data_t));

	/* Check for memory allocation. */
	if (boot_priv_data == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	boot_priv_data->device = DEVICE_BOOT;
	boot_priv_data->context = (void *)hsdmmc;

	/* Initialize block driver with boot_area region. */
	pr_debug("Init boot device\n");

	device_id = TEGRABL_STORAGE_SDMMC_BOOT << 16 | hsdmmc->controller_id;

	/* Allocate memory for boot device handle. */
	pr_trace("Allocating memory for boot device\n");
	boot_dev = tegrabl_calloc(1, sizeof(tegrabl_bdev_t));

	/* Check for memory allocation. */
	if (boot_dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		goto fail;
	}

	error = tegrabl_blockdev_initialize_bdev(boot_dev, device_id,
									 hsdmmc->block_size_log2,
									 hsdmmc->boot_blocks << 1);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	boot_dev->buf_align_size = TEGRABL_SDMMC_BUF_ALIGN_SIZE;
	/* Fill bdev function pointers. */
	boot_dev->read_block = sdmmc_bdev_read_block;
#if !defined(CONFIG_DISABLE_EMMC_BLOCK_WRITE)
	boot_dev->write_block = sdmmc_bdev_write_block;
#endif
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	boot_dev->erase = sdmmc_bdev_erase;
	boot_dev->xfer = sdmmc_bdev_xfer;
	boot_dev->xfer_wait = sdmmc_bdev_xfer_wait;
#endif
	boot_dev->close = sdmmc_bdev_close;
	boot_dev->ioctl = sdmmc_bdev_ioctl;
	boot_dev->priv_data = (void *)boot_priv_data;

	/* Register sdmmc_boot device. */
	pr_trace("Registering boot device\n");
	error = tegrabl_blockdev_register_device(boot_dev);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	user_priv_data = tegrabl_calloc(1, sizeof(sdmmc_priv_data_t));

	/* Check for memory allocation. */
	if (user_priv_data == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 2);
		goto fail;
	}

	hsdmmc->count_devices += 1U;
	user_priv_data->device = DEVICE_USER;
	user_priv_data->context = (void *)hsdmmc;

	/* Initialize block driver with user area region. */
	pr_debug("Init user device\n");
	device_id = TEGRABL_STORAGE_SDMMC_USER << 16 | hsdmmc->controller_id;

	/* Allocate memory for user device handle. */
	pr_trace("Allocating memory for user device\n");
	user_dev = tegrabl_calloc(1, sizeof(tegrabl_bdev_t));

	/* Check for memory allocation. */
	if (user_dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 3);
		goto fail;
	}

	error = tegrabl_blockdev_initialize_bdev(user_dev, device_id,
									 hsdmmc->block_size_log2,
									 hsdmmc->user_blocks);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	user_dev->buf_align_size = TEGRABL_SDMMC_BUF_ALIGN_SIZE;
	/* Fill bdev function pointers. */
	user_dev->read_block = sdmmc_bdev_read_block;
#if !defined(CONFIG_DISABLE_EMMC_BLOCK_WRITE)
	user_dev->write_block = sdmmc_bdev_write_block;
#endif
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	user_dev->erase = sdmmc_bdev_erase;
	user_dev->xfer = sdmmc_bdev_xfer;
	user_dev->xfer_wait = sdmmc_bdev_xfer_wait;
#endif
	user_dev->close = sdmmc_bdev_close;
	user_dev->ioctl = sdmmc_bdev_ioctl;
	user_dev->priv_data = (void *)user_priv_data;

	/* Register sdmmc_user device. */
	pr_trace("Registering user device\n");
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

	hsdmmc->count_devices += 1;
	rpmb_priv_data->device = DEVICE_RPMB;
	rpmb_priv_data->context = (void *)hsdmmc;

	/* Initialize block driver with rpmb area region. */
	pr_debug("Init RPMB device\n");
	device_id = TEGRABL_STORAGE_SDMMC_RPMB << 16 | hsdmmc->controller_id;

	/* Allocate memory for rpmb device handle. */
	pr_trace("Allocating memory for RPMB device\n");
	rpmb_dev = tegrabl_calloc(1, sizeof(tegrabl_bdev_t));

	/* Check for memory allocation. */
	if (!rpmb_dev) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 5);
		goto fail;
	}

	error = tegrabl_blockdev_initialize_bdev(rpmb_dev, device_id,
									 RPMB_DATA_SIZE_LOG2,
									 hsdmmc->rpmb_blocks);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	rpmb_dev->buf_align_size = TEGRABL_SDMMC_BUF_ALIGN_SIZE;
	/* Fill bdev function pointers. */
	rpmb_dev->read_block = sdmmc_bdev_read_block;
#if !defined(CONFIG_DISABLE_EMMC_BLOCK_WRITE)
	rpmb_dev->write_block = sdmmc_bdev_write_block;
#endif
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	rpmb_dev->erase = sdmmc_bdev_erase;
	rpmb_dev->xfer = sdmmc_bdev_xfer;
	rpmb_dev->xfer_wait = sdmmc_bdev_xfer_wait;
#endif
	rpmb_dev->close = sdmmc_bdev_close;
	rpmb_dev->ioctl = sdmmc_bdev_ioctl;
	rpmb_dev->priv_data = (void *)rpmb_priv_data;

	/* Register sdmmc_rpmb device. */
	pr_trace("Registering RPMB device\n");
	error = tegrabl_blockdev_register_device(rpmb_dev);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
#endif
fail:
	if (error != TEGRABL_NO_ERROR) {
		if (boot_dev != NULL) {
			tegrabl_free(boot_dev);
		}

		if (boot_priv_data != NULL) {
			tegrabl_free(boot_priv_data);
		}

		if (user_dev != NULL) {
			tegrabl_free(user_dev);
		}

		if (user_priv_data != NULL) {
			tegrabl_free(user_priv_data);
		}

#if defined(CONFIG_ENABLE_SDMMC_RPMB)
		if (rpmb_dev != NULL) {
			tegrabl_free(rpmb_dev);
		}

		if (rpmb_priv_data != NULL) {
			tegrabl_free(rpmb_priv_data);
		}
#endif
	}

	return error;
}

tegrabl_error_t sdmmc_bdev_open(uint32_t instance, struct tegrabl_sdmmc_platform_params *params)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_sdmmc *hsdmmc = NULL;
	uint8_t flag = SKIP_NONE;

	if (params == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (instance >= MAX_SDMMC_INSTANCES) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 6);
		goto fail;
	}

	pr_debug("Instance: %d\n", instance);

	pr_trace("clk_src = %d\n", params->clk_src);
	pr_trace("clk_freq = %d\n", params->clk_freq);
	pr_trace("best_mode = %d\n", params->best_mode);
	pr_trace("tap_value = %d\n", params->tap_value);
	pr_trace("trim_value = %d\n", params->trim_value);
	pr_trace("pd_offset = %d\n", params->pd_offset);
	pr_trace("pu_offset = %d\n", params->pu_offset);
	pr_trace("dqs_trim_hs400 = %d\n", params->dqs_trim_hs400);
	pr_trace("enable_strobe_hs400 = %d\n", params->enable_strobe_hs400);
	pr_trace("is_skip_init = %d\n", params->is_skip_init);

	hsdmmc = contexts[instance];
	if (params->is_skip_init) {
		flag = SKIP_INIT;
	}

	/* Check if the handle is NULL or not. */
	if (hsdmmc != NULL) {
		if (hsdmmc->clk_src == params->clk_src &&
			hsdmmc->best_mode == params->best_mode &&
			hsdmmc->tap_value == params->tap_value &&
			hsdmmc->trim_value == params->trim_value) {
			pr_info("sdmmc bdev is already initialized\n");
			goto fail;
		} else {
			flag = SKIP_INIT_UPDATE_CONFIG;
		}
	} else {
		/* Allocate memory for context */
		pr_trace("Allocating memory for context\n");
		hsdmmc = tegrabl_alloc(TEGRABL_HEAP_DMA, sizeof(struct tegrabl_sdmmc));

		/* Check for memory allocation. */
		if (hsdmmc == NULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 6);
			goto fail;
		}

		/* Initialize the memory with zero. */
		memset(hsdmmc, 0x0, sizeof(struct tegrabl_sdmmc));
	}

	hsdmmc->clk_src = params->clk_src;
	hsdmmc->controller_id = instance;
	hsdmmc->best_mode = params->best_mode;
	hsdmmc->tap_value = params->tap_value;
	hsdmmc->trim_value = params->trim_value;

	/* Call sdmmc_init to proceed with initialization. */
	pr_debug("sdmmc init\n");

	error = sdmmc_init(hsdmmc->controller_id, hsdmmc, flag);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	if (contexts[hsdmmc->controller_id] == NULL) {
		/* Fill the required function pointers and register the device. */
		pr_trace("sdmmc device register\n");
		error = sdmmc_register_region(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	contexts[instance] = hsdmmc;
fail:

	if ((error != TEGRABL_NO_ERROR) && (hsdmmc != NULL)) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, hsdmmc);
	}

	return error;
}

tegrabl_error_t sdmmc_bdev_close(tegrabl_bdev_t *dev)
{
	sdmmc_priv_data_t *priv_data;
	struct tegrabl_sdmmc *hsdmmc;

	if (dev == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 46);
	}

	if (dev->priv_data == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 47);
	}

	priv_data = (sdmmc_priv_data_t *)dev->priv_data;
	hsdmmc = (struct tegrabl_sdmmc *)priv_data->context;

	/* Close allocated context for sdmmc. */
	if ((priv_data != NULL) && (hsdmmc->count_devices == 1U)) {
		contexts[hsdmmc->controller_id] = NULL;
		tegrabl_dealloc(TEGRABL_HEAP_DMA, hsdmmc);
	} else if ((priv_data != NULL) && (hsdmmc->count_devices != 0U)) {
		hsdmmc->count_devices--;
	} else {
		/* No Action Required */
	}
	if (priv_data != NULL) {
		tegrabl_free(priv_data);
	}

	return TEGRABL_NO_ERROR;
}
