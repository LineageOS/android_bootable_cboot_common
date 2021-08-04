/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_REGULATOR

#include <string.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_gpio.h>
#include <libfdt.h>
#include <tegrabl_regulator.h>
#include "tegrabl_regulator_priv.h"

#define FIXED_REGULATOR "fixed-regulators"

/* global fdt handle */
static void *fdt;
/* fixed-regulators node */
static int32_t node_offset;
/* list of fixed regulators */
static struct list_node fixed_regulators;

/* fixed regulator info */
struct fixed_regulator_t {
	/* fixed regulator list */
	struct list_node node;
	/* fixed regulator phandle */
	int32_t phandle;
	/* input supply phandle */
	int32_t supply_phandle;
	/* gpio phandle and gpio num */
	uint32_t gpio_phandle;
	uint32_t gpio_pin_num;
	/* gpio active high or low */
	bool is_active_high;
};

static tegrabl_error_t fixed_regulator_lookup(int32_t phandle,
											  struct fixed_regulator_t **entry)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool found = false;

	list_for_every_entry(&fixed_regulators, *entry,
						 struct fixed_regulator_t, node) {
		if ((*entry)->phandle == phandle) {
			found = true;
			break;
		}
	}

	if (!found) {
		pr_critical("could not find fixed regulator\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	pr_debug("fixed regulator found for 0x%x with gpio phandle 0x%x\n",
			 (*entry)->phandle, (*entry)->gpio_phandle);

fail:
	return err;
}

static tegrabl_error_t fixed_regulator_enable(int32_t phandle)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct fixed_regulator_t *entry;
	bool gpio_state = true;
	bool is_fixed = false;
	struct gpio_driver *gpio_drv;
#if defined(CONFIG_ENABLE_GPIO_DT_BASED)
	uint32_t chipid;
#endif

	/* find the regulator in the list */
	err = fixed_regulator_lookup(phandle, &entry);
	if (err !=  TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* enable input supply voltage */
	if (entry->supply_phandle) {
		err = tegrabl_regulator_is_fixed(entry->supply_phandle, &is_fixed);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

		if (is_fixed) {
			tegrabl_regulator_enable(entry->supply_phandle);
		} else {
			tegrabl_regulator_set_voltage(entry->supply_phandle, 0,
										  STANDARD_VOLTS);
			tegrabl_regulator_enable(entry->supply_phandle);
		}
	}

#if defined(CONFIG_ENABLE_GPIO_DT_BASED)
	err = tegrabl_gpio_get_chipid_with_phandle(entry->gpio_phandle, &chipid);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	entry->gpio_phandle = chipid;
#endif

	err = tegrabl_gpio_driver_get(entry->gpio_phandle, &gpio_drv);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	/* configure gpio */
	gpio_config(gpio_drv, entry->gpio_pin_num, GPIO_PINMODE_OUTPUT);

	if (!entry->is_active_high) {
		gpio_state = false;
	}
	gpio_write(gpio_drv, entry->gpio_pin_num,
			   gpio_state ? GPIO_PIN_STATE_HIGH : GPIO_PIN_STATE_LOW);

fail:
	if (err !=  TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
	}

	return err;
}

static tegrabl_error_t fixed_regulator_disable(int32_t phandle)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct fixed_regulator_t *entry;
	bool gpio_state = false;
	struct gpio_driver *gpio_drv;

	/* find the regulator in the list */
	err = fixed_regulator_lookup(phandle, &entry);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}


	err = tegrabl_gpio_driver_get(entry->gpio_phandle, &gpio_drv);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	/* configure gpio */
	gpio_config(gpio_drv, entry->gpio_pin_num, GPIO_PINMODE_OUTPUT);

	if (!entry->is_active_high) {
		gpio_state = true;
	}
	gpio_write(gpio_drv, entry->gpio_pin_num,
			   gpio_state ? GPIO_PIN_STATE_HIGH : GPIO_PIN_STATE_LOW);

fail:
	if (err !=  TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
	}

	return err;
}

static tegrabl_error_t fixed_regulator_probe(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int32_t offset;
	int32_t root_depth = 0;
	int32_t child_depth = 0;
	int32_t node_depth = 0;
	const uint32_t *prop_p;
	int32_t phandle;
	const char *name = NULL;
	tegrabl_regulator_t *r = NULL;
	struct fixed_regulator_t *f = NULL;
	struct fixed_regulator_t *entry = NULL;
	bool duplicate = false;

	if (node_offset < 0)
		offset = fdt_next_node(fdt, -1, NULL);
	else
		offset = node_offset;

	if (offset < 0) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	root_depth = fdt_node_depth(fdt, offset);
	/* parse nodes only at this depth */
	child_depth = root_depth + 1;

	do { /* find next child */
		do {
			offset = fdt_next_node(fdt, offset, NULL);
			node_depth = fdt_node_depth(fdt, offset);
		} while (node_depth > child_depth && offset);

		if (!offset || (node_depth < child_depth)) {
			break;
		}

		/*
		 * For upstream device-tree there is no 'fixed-regulator'
		 * node and so if this node is not found then just search
		 * for any nodes with the 'regulator-fixed' compatible
		 * string at the root level.
		 */
		if (node_offset < 0) {
			if (fdt_node_check_compatible(fdt, offset,
						      "regulator-fixed"))
				continue;
		}

		phandle = fdt_get_phandle(fdt, offset);
		/* skip if phandle is missing */
		if (0 == phandle) {
			continue;
		}

		prop_p = fdt_getprop(fdt, offset,
				     "gpio", NULL);

		r = (tegrabl_regulator_t *)
				tegrabl_calloc(1, sizeof(tegrabl_regulator_t));
		if (!r) {
			err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}

		/* initialize regulator properties */
		r->phandle = phandle;
		name = fdt_getprop(fdt, offset, "regulator-name", NULL);
		if (name) {
			strlcpy(r->name, name, sizeof(r->name));
		}

		if (prop_p != NULL) {
			r->is_gpio_available = true;
		}

		/* since its fixed regulator */
		r->is_fixed = true;
		/* If gpio property is not found, regulator is already enabled, otherwise disabled by default */
		r->is_enabled = !r->is_gpio_available;
		r->enable = fixed_regulator_enable;
		r->disable = fixed_regulator_disable;
		/* register regulator */
		tegrabl_regulator_register(r);

		f = (struct fixed_regulator_t *)
				tegrabl_calloc(1, sizeof(struct fixed_regulator_t));
		if (!f) {
			err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			goto fail;
		}

		/* initialize fixed regulator properties */
		f->phandle = phandle;
		/* default active high */
		f->is_active_high = true;
		if (!fdt_get_property(fdt, offset, "enable-active-high", NULL)) {
			f->is_active_high = false;
		}

		/* TODO Retrieve the gpio_driver handle here itself, and cache it in
		 * local regulator data structure */
		if (r->is_gpio_available) {
			pr_debug("0x%x 0x%x 0x%x\n",
					fdt32_to_cpu(*((uint32_t *)prop_p)),
					fdt32_to_cpu(*((uint32_t *)(prop_p + 1))),
					fdt32_to_cpu(*((uint32_t *)(prop_p + 2))));

			f->gpio_phandle = fdt32_to_cpu(*((uint32_t *)prop_p));
			f->gpio_pin_num = fdt32_to_cpu(*((uint32_t *)(prop_p + 1)));
		}

		/* check if there is any input supply */
		if (fdt_get_property(fdt, offset, "vin-supply", NULL)) {
			prop_p = fdt_getprop(fdt, offset, "vin-supply", NULL);
			f->supply_phandle = fdt32_to_cpu(*((uint32_t *)prop_p));
		}

		/* check if duplicate */
		list_for_every_entry(&fixed_regulators, entry,
							 struct fixed_regulator_t, node) {
			if (entry->phandle == f->phandle) {
				duplicate = true;
				tegrabl_free(f);
				break;
			}
		}

		/* add fixed regulator to internal list */
		if (!duplicate) {
			list_add_head(&fixed_regulators, &f->node);
		}
	} while (offset);

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_info("fixed-regulator register failed");
	}
	return err;
}

tegrabl_error_t tegrabl_fixed_regulator_init(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	const char *str;

	if (NULL == fdt) {
		err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
		if (TEGRABL_NO_ERROR != err) {
			goto fail;
		}
	}

	/* get fixed regulators node offset */
	node_offset = fdt_node_offset_by_compatible(fdt, -1, "simple-bus");

	while (node_offset != -FDT_ERR_NOTFOUND) {
		str = (const char *)fdt_getprop(fdt, node_offset, "device_type", NULL);
		if (str == NULL) {
			node_offset = fdt_node_offset_by_compatible(fdt, node_offset,
														"simple-bus");
			continue;
		}
		if (!strncmp(str, FIXED_REGULATOR, strlen(FIXED_REGULATOR))) {
			break;
		}
		node_offset = fdt_node_offset_by_compatible(fdt, node_offset,
													"simple-bus");
	}

	/* initialize regulators list */
	list_initialize(&fixed_regulators);
	pr_info("fixed regulator driver initialized\n");
	/* iterate through all the regulators and register if their phandle is
	   present */
	if (fixed_regulator_probe() != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_info("fixed-regulator register failed");
		TEGRABL_SET_HIGHEST_MODULE(err);
	}
	return err;
}

