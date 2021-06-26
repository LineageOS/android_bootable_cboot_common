/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_KEYBOARD

#include <tegrabl_ar_macro.h>
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
#include <argpio_sw.h>
#include <argpio_aon_sw.h>
#include <libfdt.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_keyboard_config.h>

#define MAX_GPIO_KEYS 3
#define INVALID_HANDLE -1
#define GPIO_KEYS_COMPATIBLE "gpio-keys"

struct gpio_pin_info {
	uint32_t chip_id;
	int32_t handle;
	uint32_t pin;
	key_code_t key_code;
	key_event_t reported_state;
	key_event_t new_state;
};

static bool s_is_initialised;
static struct gpio_pin_info gpio_keys[MAX_GPIO_KEYS];
static uint32_t total_keys;

static tegrabl_error_t get_and_config_gpio(void *fdt, int32_t node_offset, const char *node_name,
										   uint32_t *pin_count)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	struct gpio_driver *gpio_drv;
	const char *property_name;
	int32_t subnode_offset;
	const struct fdt_property *property;
	uint32_t chip_id;

	subnode_offset = fdt_subnode_offset(fdt, node_offset, node_name);
	if (subnode_offset < 0) {
		pr_warn("subnode %s not found\n", node_name);
		ret = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	property_name = "gpios";
	property = fdt_get_property(fdt, subnode_offset, property_name, NULL);
	if (property == NULL) {
		pr_warn("error in getting property %s\n", property_name);
		ret = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 1);
		goto fail;
	}

	gpio_keys[*pin_count].handle = fdt32_to_cpu(*property->data32);
	gpio_keys[*pin_count].pin = fdt32_to_cpu(*(property->data32 + 1));

	ret = tegrabl_gpio_get_chipid_with_phandle(gpio_keys[*pin_count].handle, &chip_id);
	if (ret != TEGRABL_NO_ERROR) {
		goto fail;
	}
	gpio_keys[*pin_count].chip_id = chip_id;

	pr_debug("Key %s found in config table\n", node_name);
	ret = tegrabl_gpio_driver_get(chip_id, &gpio_drv);
	if (ret != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(ret);
		goto fail;
	}
	ret = gpio_config(gpio_drv, gpio_keys[*pin_count].pin, GPIO_PINMODE_INPUT);
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("Failed to config GPIO pin %d: chip_id %d\n",
				 gpio_keys[*pin_count].pin, gpio_keys[*pin_count].chip_id);
		TEGRABL_SET_HIGHEST_MODULE(ret);
		goto fail;
	}

	*pin_count = *pin_count + 1;
	pr_debug("Key %s config successful: pin_num %d chip_id %d\n",
			 node_name, gpio_keys[*pin_count].pin, gpio_keys[*pin_count].chip_id);

fail:
	return ret;
}

static tegrabl_error_t key_config(void *fdt, int32_t node_offset, char **key_array,
								  uint32_t num_key_array, uint32_t *pin_count, key_code_t key)
{
	uint32_t i;
	const char *name;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	for (i = 0; i < num_key_array; i++) {
		name = key_array[i];
		if (name == NULL) {
			break;
		}
		err = get_and_config_gpio(fdt, node_offset, name, pin_count);
		if (err == TEGRABL_NO_ERROR) {
			gpio_keys[*pin_count - 1].key_code = key;
			s_is_initialised = true;
			break;
		}
	}
	return err;
}

static tegrabl_error_t get_keys_from_dtb(void)
{
	uint32_t pin_count = 0;
	void *fdt;
	uint32_t i;
	int32_t node_offset;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	node_offset = fdt_node_offset_by_compatible(fdt, -1, GPIO_KEYS_COMPATIBLE);
	if (node_offset < 0) {
		pr_warn("'gpio-keys' compatible node not found\n");
		total_keys = 0;
		goto fail;
	}

	/* Selection Key */
	err = key_config(fdt, node_offset, select_key, ARRAY_SIZE(select_key), &pin_count, KEY_ENTER);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Selection key not successfully initialised.\n");
	}

	/* Navigation Up Key */
	err = key_config(fdt, node_offset, up_key, ARRAY_SIZE(up_key), &pin_count, KEY_UP);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Navigation Up key not successfully initialised.\n");
	}

	/* Navigation Down Key */
	err = key_config(fdt, node_offset, down_key, ARRAY_SIZE(down_key), &pin_count, KEY_DOWN);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Navigation down key not successfully initialised.\n");
	}
	err = TEGRABL_NO_ERROR;

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

fail:
	return err;
}

tegrabl_error_t tegrabl_gpio_keyboard_init(void)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	if (s_is_initialised) {
		return TEGRABL_NO_ERROR;
	}

	ret = get_keys_from_dtb();
	if (ret != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(ret);
		return ret;
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_gpio_keyboard_get_key_data(key_code_t *key_code,
												   key_event_t *key_event)
{
	uint32_t i;
	gpio_pin_state_t state;
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
