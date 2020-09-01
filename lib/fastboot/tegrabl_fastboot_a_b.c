/*
 * Copyright (c) 2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_FASTBOOT

#include <stdio.h>
#include <stdbool.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_fastboot_protocol.h>
#include <tegrabl_fastboot_a_b.h>
#include <tegrabl_fastboot_partinfo.h>
#include <tegrabl_utils.h>
#include <tegrabl_partition_manager.h>
#include <string.h>

#define MAX_PART_NAME_LEN 16
#define MAX_BOOT_CHAIN_SUFFIX_LEN 16

const char *fastboot_a_b_var_list[] = {
	"slot-successful:",
	"slot-unbootable:",
	"slot-retry-count:",
	"current-slot",
	"slot-suffixes",
	"has-slot",
	"slot-count",
};

const char *fastboot_a_b_slot_var_list[] = {
	"slot-successful:",
	"slot-unbootable:",
	"slot-retry-count:",
};

uint32_t size_a_b_slot_var_list = ARRAY_SIZE(fastboot_a_b_slot_var_list);

static struct tegrabl_fastboot_a_b_ops a_b_ops;

inline bool is_a_b_var_type(const char *arg)
{
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(fastboot_a_b_var_list); i++) {
		if (IS_VAR_TYPE(fastboot_a_b_var_list[i])) {
			return true;
		}
	}
	return false;
}

struct tegrabl_fastboot_a_b_ops *tegrabl_fastboot_get_a_b_ops(void)
{
	return &a_b_ops;
}

tegrabl_error_t tegrabl_get_slot_num(uint8_t *slot_num)
{
	if (!a_b_ops.get_slot_num) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return a_b_ops.get_slot_num(slot_num);
}

tegrabl_error_t tegrabl_get_current_slot(char *slot)
{
	if (!a_b_ops.get_current_slot) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return a_b_ops.get_current_slot(slot);
}

tegrabl_error_t tegrabl_get_slot_suffix(uint8_t slot_id, char *slot)
{
	if (!a_b_ops.get_slot_suffix) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return a_b_ops.get_slot_suffix(slot_id, slot);
}

tegrabl_error_t tegrabl_get_slot_suffixes(char *slot)
{
	uint8_t num_slots, i;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	TEGRABL_ASSERT(slot);

	if (!a_b_ops.get_slot_suffix) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		goto done;
	}

	error = tegrabl_get_slot_num(&num_slots);
	if (error != TEGRABL_NO_ERROR) {
		goto done;
	}

	for (i = 0; i < num_slots; i++) {
		error = a_b_ops.get_slot_suffix(i, slot + strlen(slot));
		if (error != TEGRABL_NO_ERROR) {
			goto done;
		}
		/* comma separate the suffixes */
		if (i < num_slots - 1) {
			strcat(slot, ",");
		}
	}

done:
	return error;
}

tegrabl_error_t tegrabl_is_slot_successful(const char *slot,
										   bool *slot_successful)
{
	if (!a_b_ops.is_slot_successful) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return a_b_ops.is_slot_successful(slot, slot_successful);
}

tegrabl_error_t tegrabl_is_slot_unbootable(const char *slot,
										   bool *slot_unbootable)
{
	if (!a_b_ops.is_slot_unbootable) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return a_b_ops.is_slot_unbootable(slot, slot_unbootable);
}

tegrabl_error_t tegrabl_get_slot_retry_count(const char *slot,
											 uint8_t *count)
{
	if (!a_b_ops.get_slot_retry_count) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return a_b_ops.get_slot_retry_count(slot, count);
}

static tegrabl_error_t partition_has_slot(const char *partition_name,
										  bool *has_slot, char *response)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	const struct tegrabl_fastboot_partition_info *partinfo = NULL;
	struct tegrabl_partition partition;
	char part_name[MAX_PART_NAME_LEN] = {0};
	char slot_suffix[MAX_BOOT_CHAIN_SUFFIX_LEN] = {0};

	/* "bootloader" is a special case for all bootloader binaries.
	 * "has-slot" should return the same result as "cpu-bootloader" */
	if (!strcmp(partition_name, "bootloader")) {
		partition_name = "cpu-bootloader";
	}
	partinfo = tegrabl_fastboot_get_partinfo(partition_name);
	if (!partinfo) {
		COPY_RESPONSE("No partition present with this name.");
		goto fail;
	}
	/* Use raw partition name to check if it's a valid partition */
	error = tegrabl_partition_open(partinfo->tegra_part_name, &partition);
	if (error != TEGRABL_NO_ERROR) {
		COPY_RESPONSE("No partition present with this name.");
		goto fail;
	}

	tegrabl_partition_close(&partition);

	/* Use partition name with slot 1 suffix to check if it has slots */
	error = tegrabl_get_slot_suffix(1, slot_suffix);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	sprintf(part_name, "%s%s", partinfo->tegra_part_name, slot_suffix);
	error = tegrabl_partition_open(part_name, &partition);
	if (error == TEGRABL_NO_ERROR) {
		*has_slot = true;
		tegrabl_partition_close(&partition);
	} else {
		/* _b partition open failure means it has no slot  */
		*has_slot = false;
	}
	error = TEGRABL_NO_ERROR;

fail:
	return error;
}

tegrabl_error_t tegrabl_fastboot_a_b_var_handler(const char *arg,
												 char *response)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	char slot_suffix[MAX_BOOT_CHAIN_SUFFIX_LEN] = {0};
	const char *suffix = NULL;
	bool slot_successful;
	bool slot_unbootable;
	uint8_t slot_retry_count = 0;
	bool has_slot = false;
	uint8_t num_slots = 0;

	if (IS_VAR_TYPE("has-slot:")) {
		error = partition_has_slot(arg + strlen("has-slot:"), &has_slot,
								   response);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		if (!has_slot) {
			COPY_RESPONSE("no");
		} else {
			COPY_RESPONSE("yes");
		}
	} else if (IS_VAR_TYPE("current-slot")) {
		error = tegrabl_get_current_slot(slot_suffix);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		COPY_RESPONSE(slot_suffix);
	} else if (IS_VAR_TYPE("slot-suffixes")) {
		error = tegrabl_get_slot_suffixes(slot_suffix);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		COPY_RESPONSE(slot_suffix);
	} else if (IS_VAR_TYPE("slot-count")) {
		error = tegrabl_get_slot_num(&num_slots);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		sprintf(response + strlen(response), "%u", num_slots);
	} else if (IS_VAR_TYPE("slot-successful:")) {
		suffix = arg + strlen("slot-successful:");
		error = tegrabl_is_slot_successful(suffix, &slot_successful);
		if (error != TEGRABL_NO_ERROR) {
			COPY_RESPONSE("No slot present with this suffix.");
			goto fail;
		}
		if (slot_successful == true) {
			COPY_RESPONSE("yes");
		} else {
			COPY_RESPONSE("no");
		}
	} else if (IS_VAR_TYPE("slot-unbootable:")) {
		suffix = arg + strlen("slot-unbootable:");
		error = tegrabl_is_slot_unbootable(suffix, &slot_unbootable);
		if (error != TEGRABL_NO_ERROR) {
			COPY_RESPONSE("No slot present with this suffix.");
			goto fail;
		}
		if (slot_unbootable == true) {
			COPY_RESPONSE("yes");
		} else {
			COPY_RESPONSE("no");
		}
	} else if (IS_VAR_TYPE("slot-retry-count:")) {
		suffix = arg + strlen("slot-retry-count:");
		error = tegrabl_get_slot_retry_count(suffix, &slot_retry_count);
		if (error != TEGRABL_NO_ERROR) {
			COPY_RESPONSE("No slot present with this suffix.");
			goto fail;
		}
		sprintf(response + strlen(response), "%u", slot_retry_count);
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_set_active_slot(const char *slot)
{
	if (!a_b_ops.slot_set_active) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return a_b_ops.slot_set_active(slot);
}

