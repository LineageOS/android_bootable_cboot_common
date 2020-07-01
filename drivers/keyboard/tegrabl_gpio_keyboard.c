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

#include <tegrabl_utils.h>
#include <tegrabl_gpio.h>
#include <tegrabl_gpio_hw.h>
#include <tegrabl_gpio_keyboard.h>
#include <tegrabl_keyboard.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <libfdt.h>
#include <string.h>
#include <keyboard_config.h>

#define MAX_GPIO_KEYS 3
#define INVALID_HANDLE -1

struct gpio_pin_info {
	uint32_t chip_id;
	uint32_t pin;
	enum key_code key_code;
	enum key_event reported_state;
	enum key_event new_state;
};

static bool s_is_initialised;
static struct gpio_pin_info gpio_keys[MAX_GPIO_KEYS];
static uint32_t total_keys;

static char *select_key[] = {
	"power",
	"home"
};

static char *up_key[] = {
	"volume_up",
	"back"
};

static char *down_key[] = {
	"volume_down",
	"menu"
};

static tegrabl_error_t config_gpio(char **key, uint32_t key_size,
								   struct key_info *keys_info,
								   uint32_t keys_info_size, enum key_code code,
								   uint32_t *pin_count)
{
	uint32_t i, j;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	struct gpio_driver *gpio_drv;

	for (i = 0; key_size; i++) {
		for (j = 0; j < keys_info_size; j++) {
			if (!strcmp(key[i], keys_info[j].key_name)) {
				pr_debug("Key %s found in config table\n", key[i]);
				gpio_keys[*pin_count].chip_id = keys_info[j].chip_id;
				gpio_keys[*pin_count].pin = keys_info[j].gpio_pin;
				ret = tegrabl_gpio_driver_get(gpio_keys[*pin_count].chip_id,
											  &gpio_drv);
				if (ret != TEGRABL_NO_ERROR) {
					TEGRABL_SET_HIGHEST_MODULE(ret);
					return ret;
				}
				ret = gpio_config(gpio_drv, gpio_keys[*pin_count].pin,
								  GPIO_PINMODE_INPUT);
				if (ret != TEGRABL_NO_ERROR) {
					pr_error("Failed to config GPIO pin %d: chip_id %d\n",
							 gpio_keys[*pin_count].pin,
							 gpio_keys[*pin_count].chip_id);
					TEGRABL_SET_HIGHEST_MODULE(ret);
					return ret;
				}
				gpio_keys[*pin_count].key_code = code;
				*pin_count = *pin_count + 1;
				pr_debug("Key %s config successful: pin_num %d chip_id %d\n",
						 key[i], gpio_keys[*pin_count].pin,
						 gpio_keys[*pin_count].chip_id);
				return TEGRABL_NO_ERROR;
			}
		}
	}
	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
}

/* Hard code keys property configs here
 * TODO
 * Remove this hard coding and use DT to config keyboard */
static tegrabl_error_t get_hard_coded_keys(void)
{
	uint32_t pin_count = 0;
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	uint32_t i;

	/* Selection Key */
	status = config_gpio(select_key, ARRAY_SIZE(select_key), keys_info,
						 ARRAY_SIZE(keys_info), KEY_ENTER, &pin_count);
	if (status != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(status);
		return status;
	}

	/* Navigation Up Key */
	status = config_gpio(up_key, ARRAY_SIZE(up_key), keys_info,
						 ARRAY_SIZE(keys_info), KEY_UP, &pin_count);
	if (status != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(status);
		return status;
	}

	/* Navigation Down Key */
	status = config_gpio(down_key, ARRAY_SIZE(down_key), keys_info,
						 ARRAY_SIZE(keys_info), KEY_DOWN, &pin_count);
	if (status != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(status);
		return status;
	}

	total_keys = pin_count;

	for (i = 0; i < total_keys; i++) {
		gpio_keys[i].reported_state = KEY_RELEASE_FLAG;
		gpio_keys[i].new_state = KEY_RELEASE_FLAG;
	}

	/* if there is only one key available, use it for
	 * Navigation as well as selection as KEY_HOLD */
	if (total_keys == 1) {
		gpio_keys[0].key_code = KEY_HOLD;
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_gpio_keyboard_init(void)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	if (s_is_initialised) {
		return TEGRABL_NO_ERROR;
	}

	ret = get_hard_coded_keys();
	if (ret != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(ret);
		return ret;
	}

	s_is_initialised = true;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_gpio_keyboard_get_key_data(enum key_code *key_code,
												   enum key_event *key_event)
{
	uint32_t i;
	enum gpio_pin_state state;
	bool flag = false;
	struct gpio_driver *gpio_drv;
	tegrabl_error_t status = TEGRABL_NO_ERROR;

	*key_code = 0;
	for (i = 0; i < total_keys; i++) {
		status = tegrabl_gpio_driver_get(gpio_keys[i].chip_id, &gpio_drv);
		if (status != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(status);
			goto fail;
		}
		gpio_read(gpio_drv, gpio_keys[i].pin, &state);
		if (state == GPIO_PIN_STATE_LOW) {
			gpio_keys[i].new_state = KEY_PRESS_FLAG;
			if (gpio_keys[i].reported_state != gpio_keys[i].new_state) {
				tegrabl_mdelay(10);
				*key_code += gpio_keys[i].key_code;
				*key_event = KEY_PRESS_FLAG;
				flag = true;
			} else {
				*key_code += gpio_keys[i].key_code;
				flag = true;
			}
		} else {
			gpio_keys[i].new_state = KEY_RELEASE_FLAG;
			if (gpio_keys[i].reported_state != gpio_keys[i].new_state) {
				tegrabl_mdelay(10);
				*key_code += gpio_keys[i].key_code;
				*key_event = KEY_RELEASE_FLAG;
				flag = true;
			}
		}
		gpio_keys[i].reported_state = gpio_keys[i].new_state;
	}

	if (!flag) {
		*key_code = KEY_IGNORE;
		*key_event = KEY_EVENT_IGNORE;
	} else
		pr_debug("Key event %d detected: key code %d\n", (int32_t)*key_event,
														 (int32_t)*key_code);

fail:
	return status;
}
