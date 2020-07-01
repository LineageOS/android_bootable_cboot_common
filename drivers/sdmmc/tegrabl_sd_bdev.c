/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
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
#include <tegrabl_sdmmc_protocol.h>
#include <tegrabl_malloc.h>
#include <tegrabl_clock.h>
#include <tegrabl_module.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <inttypes.h>
#include <tegrabl_sd_bdev.h>
#include <tegrabl_sdmmc_bdev.h>
#include <tegrabl_sd_card.h>
#include <tegrabl_sd_param.h>
#include <tegrabl_regulator.h>

#define TEGRABL_SD_BUF_ALIGN_SIZE	4U

static struct tegrabl_sdmmc *contexts[4] = {0, 0, 0, 0};

tegrabl_error_t sd_bdev_is_card_present(struct gpio_info *cd_gpio, bool *is_present)
{
	return tegrabl_sd_is_card_present(cd_gpio, is_present);
}

static tegrabl_error_t sd_register_region(struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_bdev_t *user_dev = NULL;
	uint32_t device_id;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	sdmmc_priv_data_t *user_priv_data = NULL;

	hsdmmc->count_devices = 1;

	user_priv_data = tegrabl_calloc(1, sizeof(sdmmc_priv_data_t));

	/* Check for memory allocation. */
	if (!user_priv_data) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	user_priv_data->device = DEVICE_USER;
	user_priv_data->context = (void *)hsdmmc;

	/* Initialize block driver with sdcard. */
	pr_trace("Init sdcard\n");

	device_id = TEGRABL_STORAGE_SDCARD << 16 | hsdmmc->controller_id;

	/* Allocate memory for boot device handle. */
	pr_trace("Allocating memory for boot device\n");
	user_dev = tegrabl_calloc(1, sizeof(tegrabl_bdev_t));

	/* Check for memory allocation. */
	if (!user_dev) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		goto fail;
	}

	error = tegrabl_blockdev_initialize_bdev(user_dev, device_id,
									 hsdmmc->block_size_log2,
									 hsdmmc->user_blocks);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	user_dev->buf_align_size = TEGRABL_SD_BUF_ALIGN_SIZE;
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
	pr_trace("Registering user device\n");
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

tegrabl_error_t sd_bdev_open(uint32_t instance, struct tegrabl_sd_platform_params *params)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_sdmmc *hsdmmc = NULL;

	if (instance >= MAX_SDMMC_INSTANCES) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 6);
		goto fail;
	}

	pr_trace("Instance: %d\n", instance);
	hsdmmc = contexts[instance];

	/* Allocate memory for context*/
	pr_trace("Allocating memory for context\n");
	hsdmmc = tegrabl_alloc(TEGRABL_HEAP_DMA, sizeof(struct tegrabl_sdmmc));

	/* Check for memory allocation. */
	if (!hsdmmc) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 6);
		goto fail;
	}

	/* Initialize the memory with zero. */
	memset(hsdmmc, 0x0, sizeof(struct tegrabl_sdmmc));

	hsdmmc->device_type = DEVICE_TYPE_SD;
	hsdmmc->clk_src = TEGRABL_CLK_SRC_PLLP_OUT0;
	hsdmmc->controller_id = instance;
	hsdmmc->best_mode = TEGRABL_SDMMC_MODE_DDR52;
	hsdmmc->tap_value = 9;
	hsdmmc->trim_value = 5;

	/* Call sdmmc_init to proceed with initialization. */
	pr_trace("sdmmc init\n");

	error = tegrabl_regulator_enable(params->vmmc_supply);
	if ((error != TEGRABL_NO_ERROR) && (TEGRABL_ERROR_REASON(error) != TEGRABL_ERR_NOT_SUPPORTED)) {
		goto fail;
	}

	error = tegrabl_regulator_set_voltage(params->vmmc_supply, 3300000, STANDARD_VOLTS);
	if ((error != TEGRABL_NO_ERROR) && (TEGRABL_ERROR_REASON(error) != TEGRABL_ERR_NOT_SUPPORTED)) {
		goto fail;
	}

	error = sdmmc_init(hsdmmc->controller_id, hsdmmc, false);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	if (!contexts[hsdmmc->controller_id]) {
		/* Fill the required function pointers and register the device. */
		pr_trace("sd device register\n");
		error = sd_register_region(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	contexts[instance] = hsdmmc;
fail:

	if ((error != TEGRABL_NO_ERROR) && hsdmmc) {
		tegrabl_dealloc(TEGRABL_HEAP_DMA, hsdmmc);
	}

	/*TODO: fix this and remove delay*/
	tegrabl_mdelay(10);
	return error;
}

tegrabl_error_t sd_bdev_close(tegrabl_bdev_t *dev)
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

	/* Close allocated hsdmmc for sdmmc. */
	if (priv_data && (hsdmmc->count_devices == 1)) {
		contexts[hsdmmc->controller_id] = NULL;
		tegrabl_dealloc(TEGRABL_HEAP_DMA, hsdmmc);
	} else if (priv_data && hsdmmc->count_devices) {
		hsdmmc->count_devices--;
	}
	if (priv_data)
		tegrabl_free(priv_data);

	return TEGRABL_NO_ERROR;
}
