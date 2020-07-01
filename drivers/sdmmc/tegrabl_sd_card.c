/*
 * Copyright (c) 2016-2019, NVIDIA Corporation.  All rights reserved.
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
#include <tegrabl_sd_card.h>
#include <tegrabl_sd_pdata.h>
#include <tegrabl_devicetree.h>

static tegrabl_error_t sd_read_pin_status(struct gpio_info *cd_gpio,
	gpio_pin_state_t *pin_state)
{
	struct gpio_driver *gpio_drv;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t chip_id = TEGRA_GPIO_MAIN_CHIPID;
#if defined(CONFIG_ENABLE_GPIO_DT_BASED)
	void *fdt = NULL;
#endif

	pr_trace("cd_gpio_pin = %d\n", cd_gpio_pin);

#if defined(CONFIG_ENABLE_GPIO_DT_BASED)
	if (tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt) == TEGRABL_NO_ERROR) {
		err = tegrabl_gpio_get_chipid_with_phandle(cd_gpio->handle, &chip_id);
		if (err != TEGRABL_NO_ERROR)
			goto fail;
	}
#endif

	err = tegrabl_gpio_driver_get(chip_id, &gpio_drv);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = gpio_config(gpio_drv, cd_gpio->pin, GPIO_PINMODE_INPUT);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = gpio_read(gpio_drv, cd_gpio->pin, pin_state);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("sd gpio pin status read failed\n");
		TEGRABL_SET_HIGHEST_MODULE(err);
	}
	return err;
}

tegrabl_error_t tegrabl_sd_is_card_present(struct gpio_info *cd_gpio, bool *is_present)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	gpio_pin_state_t pin_state;
	bool is_active_low;

	err = sd_read_pin_status(cd_gpio, &pin_state);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_trace("pin_state = %d\n", pin_state);
	is_active_low = !(cd_gpio->flags & 0x1);

	*is_present = (pin_state == GPIO_PIN_STATE_HIGH) ^ is_active_low;
	if (*is_present)
		pr_info("Found sdcard\n");
	else
		pr_info("No sdcard\n");

fail:
	if (err != TEGRABL_NO_ERROR)
		pr_error("sd card detection failed\n");
	return err;
}
