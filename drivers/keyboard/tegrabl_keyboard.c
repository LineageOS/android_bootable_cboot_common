/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_KEYBOARD

#include <tegrabl_keyboard.h>
#include <tegrabl_gpio_keyboard.h>
#include <tegrabl_debug.h>

static bool s_is_keyboard_initialised;

tegrabl_error_t tegrabl_keyboard_init(void)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	if (s_is_keyboard_initialised) {
		return TEGRABL_NO_ERROR;
	}

	ret = tegrabl_gpio_keyboard_init();
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("Gpio keyboard init failed\n");
		return ret;
	}

	pr_info("Gpio keyboard init success\n");
	s_is_keyboard_initialised = true;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_keyboard_get_key_data(key_code_t *code,
											  key_event_t *event)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	if (!s_is_keyboard_initialised) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
	}

	ret = tegrabl_gpio_keyboard_get_key_data(code, event);

	return ret;
}
