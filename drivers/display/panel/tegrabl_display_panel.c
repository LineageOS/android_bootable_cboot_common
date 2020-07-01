/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_PANEL

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_display_panel.h>
#include <tegrabl_gpio.h>
#include <tegrabl_pmic.h>
#include <tegrabl_regulator.h>
#include <tegrabl_padctl.h>

static tegrabl_error_t tegrabl_is_hdmi_connected(bool *is_connected,
	struct tegrabl_display_hdmi_dtb *hdmi_dtb)
{
	uint32_t pin;
	uint32_t chip_id = TEGRA_GPIO_MAIN_CHIPID; /*from DTB*/
	gpio_pin_state_t state;
	struct gpio_driver *gpio_drv;
	uint32_t check_state = GPIO_PIN_STATE_HIGH; /*get from dtb*/
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_debug("%s: check HDMI cable connection\n", __func__);

	pin = hdmi_dtb->hpd_gpio;

	tegrabl_padctl_config_to_gpio(pin);

	pr_debug("%s: check HDMI pin = %d\n", __func__, pin);

	err = tegrabl_gpio_driver_get(chip_id, &gpio_drv);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed to get GPIO driver struct\n", __func__);
		TEGRABL_SET_HIGHEST_MODULE(err);
		return err;
	}

	err = gpio_config(gpio_drv, pin, GPIO_PINMODE_INPUT);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed to configure GPIO pin: %d\n", __func__, pin);
		TEGRABL_SET_HIGHEST_MODULE(err);
		return err;
	}

	err = gpio_read(gpio_drv, pin, &state);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed to read GPIO pin: %d\n", __func__, pin);
		TEGRABL_SET_HIGHEST_MODULE(err);
		return err;
	}

	if (hdmi_dtb->polarity & TEGRABL_HDMI_HOTPLUG_MASK) {
		check_state = GPIO_PIN_STATE_LOW;
	}
	if (state == check_state) {
		pr_info("hdmi cable connected\n");
		*is_connected = true;
	}
	return err;
}

static tegrabl_error_t tegrabl_display_enable_regulator(int32_t phandle,
														uint32_t voltage)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_regulator_enable(phandle);
	if ((TEGRABL_ERROR_REASON(err) == TEGRABL_ERR_NOT_SUPPORTED) || err == TEGRABL_NO_ERROR) {
		err = tegrabl_regulator_set_voltage(phandle, voltage, STANDARD_VOLTS);
		if ((TEGRABL_ERROR_REASON(err) == TEGRABL_ERR_NOT_SUPPORTED) || err == TEGRABL_NO_ERROR) {
			return TEGRABL_NO_ERROR;
		}
	}
	return TEGRABL_ERROR(TEGRABL_ERR_REGULATOR, 0);
}

tegrabl_error_t tegrabl_display_init_regulator(uint32_t du_type, struct tegrabl_display_pdata *pdata)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool is_connected = false;
	struct tegrabl_display_hdmi_dtb hdmi_dtb = pdata->hdmi_dtb;
	struct tegrabl_display_dp_dtb dp_dtb = pdata->dp_dtb;

	switch (du_type) {
	case DISPLAY_OUT_HDMI:
		err = tegrabl_display_enable_regulator(hdmi_dtb.vdd_hdmi_5v0_supply, 5000000);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

		err = tegrabl_is_hdmi_connected(&is_connected, &hdmi_dtb);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s: cannot read hdmi pin status\n", __func__);
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_CONNECTED, 0);
				goto fail;
		} else if (!is_connected) {
			pr_error("%s: hdmi cable is not connected\n", __func__);
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_CONNECTED, 1);
			goto fail;
		}

		err = tegrabl_display_enable_regulator(hdmi_dtb.avdd_hdmi_supply, 1050000);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		err = tegrabl_display_enable_regulator(hdmi_dtb.avdd_hdmi_pll_supply, 1800000);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	case DISPLAY_OUT_DP:
		if (dp_dtb.vdd_dp_pwr_supply > 0) {
			err = tegrabl_display_enable_regulator(dp_dtb.vdd_dp_pwr_supply, 1000000);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}

		if (dp_dtb.avdd_dp_pll_supply > 0) {
			err = tegrabl_display_enable_regulator(dp_dtb.avdd_dp_pll_supply, 1800000);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}

		if (dp_dtb.vdd_dp_pad_supply > 0) {
			err = tegrabl_display_enable_regulator(dp_dtb.vdd_dp_pad_supply, 5000000);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}

		if (dp_dtb.dp_hdmi_5v0_supply > 0) {
			err = tegrabl_display_enable_regulator(dp_dtb.dp_hdmi_5v0_supply, 5000000);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}
		break;
	case DISPLAY_OUT_EDP:
		if (dp_dtb.dvdd_lcd_supply > 0) {
			err = tegrabl_display_enable_regulator(dp_dtb.dvdd_lcd_supply, 1800000);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}

		if (dp_dtb.avdd_lcd_supply > 0) {
			err = tegrabl_display_enable_regulator(dp_dtb.avdd_lcd_supply, 3300000);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}

		if (dp_dtb.vdd_lcd_bl_en_supply > 0) {
			err = tegrabl_display_enable_regulator(dp_dtb.vdd_lcd_bl_en_supply, 1800000);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}
		break;
	case DISPLAY_OUT_DSI:
		/*dsi support can be added as required*/
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		break;
	default:
		pr_debug("%s: no valid display\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
		break;
	}

fail:
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
	}
	return err;
}
