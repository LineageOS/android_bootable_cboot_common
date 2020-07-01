/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
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
#include <libfdt.h>
#include <tegrabl_regulator.h>
#include "tegrabl_regulator_priv.h"

/* FIXME - Temp fix to share regulator name with fixed-regulator
 * to obtain gpio chip_id */
extern volatile char temp_name[20];

/* list of regulators */
static struct list_node regulators;

/* global fdt handle */
static void *fdt;

static inline bool is_fixed_batt_connection(int32_t phandle)
{
	const char *reg_name;
	int32_t reg_node_offset = 0;
	bool result = false;

	reg_node_offset = fdt_node_offset_by_phandle(fdt, phandle);

	/* Check if the node points to ac-bat */
	reg_name = fdt_getprop(fdt, reg_node_offset, "regulator-name", NULL);
	if (strcmp(reg_name, "vdd-ac-bat") == TEGRABL_NO_ERROR) {
		pr_debug("%s(%x) = TRUE\n", __func__, phandle);
		result = true;
	}

	return result;
}

static tegrabl_error_t regulator_lookup(int32_t phandle,
										tegrabl_regulator_t **entry)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool found = false;

	pr_debug("regulator lookup with phandle 0x%x\n", phandle);
	list_for_every_entry(&regulators, *entry, tegrabl_regulator_t, node) {
		if ((*entry)->phandle == phandle) {
			found = true;
			break;
		}
	}

	if (!found) {
		pr_error("could not find regulator\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	pr_debug("regulator found '%s'\n", (*entry)->name);

fail:
	return err;
}

/**
 * @brief api to find if the regulator has fixed voltage
 *
 * @phandle handle of the dt entry
 * @is_fixed ptr to return true/false
 */
tegrabl_error_t tegrabl_regulator_is_fixed(int32_t phandle, bool *is_fixed)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_regulator_t *entry;

	if (!phandle) {
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		goto fail;
	}

	/* find the regulator in the list */
	if (regulator_lookup(phandle, &entry) != TEGRABL_NO_ERROR) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	if (entry->is_fixed)
		*is_fixed = true;
	else
		*is_fixed = false;

fail:
	return err;
}

/**
 * @brief api to enable selected regulator
 *
 * @phandle handle of the dt entry
 */
tegrabl_error_t tegrabl_regulator_enable(int32_t phandle)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_regulator_t *entry;

	if (!phandle) {
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		goto fail;
	}

	if (is_fixed_batt_connection(phandle) == true) {
		pr_debug("%s not needed for requested rail\n", __func__);
		goto fail;
	}

	/* find the regulator in the list */
	if (regulator_lookup(phandle, &entry) != TEGRABL_NO_ERROR) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	/* FIXME - Temp fix to share regulator name with fixed-regulator
	 * to obtain gpio chip_id */
	memset((void *)temp_name, '\0', sizeof(temp_name));
	strncpy((char *)temp_name, entry->name, strlen(entry->name));

	/* check if its already enabled */
	if (entry->is_enabled) {
		pr_info("regulator '%s' already enabled\n", entry->name);
		goto fail;
	}

	/* enable the regulator */
	if (entry->enable) {
		pr_info("enabling '%s' regulator\n", entry->name);
		err = entry->enable(entry->phandle);
		if (err == TEGRABL_NO_ERROR)
			entry->is_enabled = true;
	} else {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

fail:
	return err;
}

/**
 * @brief api to disable selected regulator
 *
 * @phandle handle of the dt entry
 */
tegrabl_error_t tegrabl_regulator_disable(int32_t phandle)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_regulator_t *entry;

	if (!phandle) {
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		goto fail;
	}

	if (is_fixed_batt_connection(phandle) == true) {
		pr_debug("%s not needed for requested rail\n", __func__);
		goto fail;
	}

	/* find the regulator in the list */
	if (regulator_lookup(phandle, &entry) != TEGRABL_NO_ERROR) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	if (!entry->is_enabled) {
		pr_info("regulator '%s' is not enabled\n", entry->name);
		goto fail;
	}

	/* disable the regulator */
	if (entry->disable) {
		pr_info("disabling '%s' regulator\n", entry->name);
		err = entry->disable(entry->phandle);
		if (err == TEGRABL_NO_ERROR)
			entry->is_enabled = false;
	} else {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

fail:
	return err;
}

/**
 * @brief api to set voltage for a selected rail
 *
 * @phandle handle of the dt entry
 * @volts desired voltage in microvolts
 * @volt_type - refer 'regulator_volt_type'
 */
tegrabl_error_t tegrabl_regulator_set_voltage(int32_t phandle, uint32_t volts,
					regulator_volt_type volt_type)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_regulator_t *entry;

	if (!phandle) {
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		goto fail;
	}

	if (is_fixed_batt_connection(phandle) == true) {
		pr_debug("%s not needed for requested rail\n", __func__);
		goto fail;
	}

	/* find the regulator in the list */
	if (regulator_lookup(phandle, &entry) != TEGRABL_NO_ERROR) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	/* check if its a fixed regulator */
	if (entry->is_fixed) {
		pr_debug("Cannot fine tune volt for fixed-regulator;");
		pr_debug("Enabling '%s'\n", entry->name);
		err = tegrabl_regulator_enable(phandle);
		goto fail; /* Skip rest */
	}

	/* check if volts requested by user is same as voltage defined for
	   a regulator */
	if (volt_type == STANDARD_VOLTS) {
		if (entry->set_volts) {
			volts = entry->set_volts;
		} else {
			pr_error("set volts not configured for '%s'\n", entry->name);
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
			goto fail;
		}
	}

	/* set voltage for regulator */
	if (entry->set_voltage) {
		pr_info("setting '%s' regulator to %u micro volts\n",
				entry->name, volts);
		err = entry->set_voltage(entry->phandle, volts);
		if (err != TEGRABL_NO_ERROR)
			goto fail;
	} else {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

fail:
	return err;
}

/**
 * @brief api to register regulator for pmic driver or
 *        fixed regulator driver.
 *
 * @param regulator pointer to regulator_t.
 * @return NO_ERROR on success otherwise error.
 */
tegrabl_error_t tegrabl_regulator_register(tegrabl_regulator_t *regulator)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_regulator_t *entry;
	bool duplicate = false;

	if (regulator == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		goto fail;
	}

	/* check if duplicate */
	list_for_every_entry(&regulators, entry, tegrabl_regulator_t, node) {
		if (entry->phandle == regulator->phandle) {
			pr_info("regulator \"%s\" already registered\n",
					regulator->name);
			duplicate = true;
			tegrabl_free(regulator);
			break;
		}
	}

	if (!duplicate) {
		pr_debug("register '%s' regulator\n", regulator->name);
		list_add_head(&regulators, &regulator->node);
	} else {
		err = TEGRABL_ERROR(TEGRABL_ERR_ALREADY_EXISTS, 0);
	}

fail:
	if (err != TEGRABL_NO_ERROR)
		pr_info("regulator register failed");
	return err;
}

/**
 * @brief api to init the regulator list
 */
tegrabl_error_t tegrabl_regulator_init(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* initialize regulators list */
	list_initialize(&regulators);
	pr_debug("regulator framework initialized\n");
	err = tegrabl_fixed_regulator_init();

fail:
	return err;
}
