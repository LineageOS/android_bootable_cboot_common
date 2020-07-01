/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_GPIO

#include <tegrabl_gpio.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <sys/types.h>
#include <list.h>

static bool s_is_gpio_framework_initialised;
static struct list_node gpio_drivers; /* List of registered GPIO drivers */

/**
 * NOTE: This is not a thread safe driver. We should avoid register driver
 *       in different threads in any case
 */
static struct gpio_driver *gpio_locate_driver_in_list(uint32_t chip_id)
{
	struct gpio_driver *entry;

	list_for_every_entry(&gpio_drivers, entry, struct gpio_driver, node) {
		pr_debug("Found gpio driver '%s' in list\n", entry->name);
		if (entry->chip_id == chip_id) {
			return entry;
		}
	}

	return NULL;
}

tegrabl_error_t tegrabl_gpio_driver_register(struct gpio_driver *drv)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	if (!drv || !drv->ops || !drv->ops->read || !drv->ops->write ||
							!drv->ops->config) {
		pr_critical("Failed NULL checks in gpio_driver_register\n");
		ret = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto out;
	}

	if (gpio_locate_driver_in_list(drv->chip_id)) {
		pr_critical("gpio-driver '%s' already registered\n", drv->name);
		ret = TEGRABL_ERROR(TEGRABL_ERR_ALREADY_EXISTS, 0);
		goto out;
	}

	pr_info("%s: register '%s' driver\n", __func__, drv->name);
	list_add_head(&gpio_drivers, &drv->node);

out:
	return ret;
}

tegrabl_error_t tegrabl_gpio_driver_get(uint32_t chip_id,
										struct gpio_driver **out)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	if (out == NULL) {
		pr_critical("%s: Null argument\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	*out = gpio_locate_driver_in_list(chip_id);
	if (*out == NULL) {
		pr_critical("GPIO driver for chip_id %#x could not be found\n",
					chip_id);
		ret = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	}

	return ret;
}

#if defined(CONFIG_ENABLE_GPIO_DT_BASED)
tegrabl_error_t tegrabl_gpio_get_chipid_with_phandle(int32_t phandle, uint32_t *chip_id)
{
	struct gpio_driver *entry;

	TEGRABL_ASSERT(chip_id);

	list_for_every_entry(&gpio_drivers, entry, struct gpio_driver, node) {
		if (entry->phandle == phandle) {
			*chip_id = entry->chip_id;
			return TEGRABL_NO_ERROR;
		}
	}
	return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
}
#endif

void gpio_framework_init()
{
	if (s_is_gpio_framework_initialised) {
		return;
	}

	/* initialize the gpio driver list */
	list_initialize(&gpio_drivers);
	s_is_gpio_framework_initialised = true;
	pr_info("gpio framework initialized\n");
}
