/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_SDMMC

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_gpio.h>
#include <tegrabl_sdmmc_pdata.h>

static tegrabl_error_t sd_read_pin_status(uint32_t cd_gpio_pin,
	enum gpio_pin_state *pin_state)
{
	uint32_t chip_id = TEGRA_GPIO_MAIN_CHIPID;
	struct gpio_driver *gpio_drv;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_debug("%s: entry, cd_gpio_pin = %d\n", __func__, cd_gpio_pin);

	err = tegrabl_gpio_driver_get(chip_id, &gpio_drv);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = gpio_config(gpio_drv, cd_gpio_pin, GPIO_PINMODE_INPUT);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = gpio_read(gpio_drv, cd_gpio_pin, pin_state);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: sd gpio pin status read failed\n", __func__);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}
	return err;
}

tegrabl_error_t tegrabl_sd_is_card_present(uint32_t *instance, bool *is_present)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct gpio_info cd_gpio;
	enum gpio_pin_state pin_state;
	bool is_active_low;

	err = tegrabl_sd_get_instance(instance, &cd_gpio);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = sd_read_pin_status(cd_gpio.pin, &pin_state);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	is_active_low = !(cd_gpio.flags & 0x1);

	*is_present = (pin_state == GPIO_PIN_STATE_HIGH) ^ is_active_low;

	if (*is_present)
		pr_info("Found sdcard\n");
	else
		pr_info("No sdcard\n");

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s:sd card present check failed\n", __func__);
		goto fail;
	}
	return err;
}
