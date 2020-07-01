/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_PMIC

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_pmic.h>

/* list of pmic */
static struct list_node pmic_list;

/**
 * @brief - helps registering pmic chip specific routines with this interface
 * @return - error code
 */
tegrabl_error_t tegrabl_pmic_register(tegrabl_pmic_t *drv)
{
	tegrabl_pmic_t *entry;
	bool duplicate = false;

	if (!drv || !drv->phandle) {
		return TEGRABL_ERR_BAD_PARAMETER;
	}

	/* check if duplicate */
	list_for_every_entry(&pmic_list, entry, tegrabl_pmic_t, node) {
		pr_debug("pmic list entry: %s\n", entry->name);
		if (entry->phandle == drv->phandle) {
			pr_info("pmic \"%s\" already registered\n", drv->name);
			duplicate = true;
			tegrabl_free(drv);
			break;
		}
	}

	if (!duplicate) {
		pr_info("registered '%s' pmic\n", drv->name);
		list_add_head(&pmic_list, &drv->node);
	} else {
		return TEGRABL_ERR_ALREADY_EXISTS;
	}

	return TEGRABL_NO_ERROR;
}

/**
 * @brief - performs poweroff
 * @return - error code
 */
tegrabl_error_t tegrabl_pmic_poweroff(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_pmic_t *entry;

	list_for_every_entry(&pmic_list, entry, tegrabl_pmic_t, node) {
		if (entry->poweroff) {
			pr_debug("powering off device\n");
			err = entry->poweroff();
		} else {
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		}
	}

	/* Control reaching here => failure */
	pr_error("poweroff failed: %s\n", entry->name);
	return err;
}

/**
 * @brief - Obtain the reason for the last pmic reset
 * @buf - buffer to be filled with reset reason
 * @return - error code
 */
tegrabl_error_t tegrabl_pmic_get_reset_reason(uint32_t *buf)
{
	tegrabl_pmic_t *entry;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (!buf) {
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		goto fail;
	}

	list_for_every_entry(&pmic_list, entry, tegrabl_pmic_t, node) {
		if (entry->get_reset_reason) {
			err = entry->get_reset_reason(buf);
			if (err != TEGRABL_NO_ERROR)
				goto fail;
		} else {
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		}
	}

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("pmic get reset reason failed\n");
	}
	return err;
}

/**
 * @brief - helps initialize the list
 */
void tegrabl_pmic_init(void)
{
	list_initialize(&pmic_list);
	pr_debug("pmic framework initialized\n");
}
