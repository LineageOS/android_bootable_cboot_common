/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __KEYBOARD_CONFIG_H_
#define __KEYBOARD_CONFIG_H_

#include <tegrabl_gpio.h>
#include <tegrabl_gpio_hw.h>
#include <tegrabl_gpio_keyboard.h>

/* Default keyboard property hard coded here */
static struct key_info keys_info[] = {
	{
		.key_name = "power",
		.chip_id = TEGRA_GPIO_AON_CHIPID,
		.gpio_pin = TEGRA_GPIO(FF, 0),
	},
	{
		.key_name = "volume_up",
		.chip_id = TEGRA_GPIO_AON_CHIPID,
		.gpio_pin = TEGRA_GPIO(FF, 1),
	},
	{
		.key_name = "volume_down",
		.chip_id = TEGRA_GPIO_AON_CHIPID,
		.gpio_pin = TEGRA_GPIO(FF, 2),
	},
};

#endif
