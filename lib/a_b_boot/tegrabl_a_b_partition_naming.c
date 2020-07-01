/*
 * Copyright (c) 2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_AB_BOOTCTRL

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <string.h>
#include <tegrabl_a_b_boot_control.h>
#include <tegrabl_a_b_partition_naming.h>

bool tegrabl_a_b_match_part_name(const char *part_name,
								 const char *full_part_name)
{
	uint32_t len;
	bool ret = false;
	const char *suffix = NULL;

	len = strlen(part_name);
	if (strncmp(part_name, full_part_name, len) != 0) {
		ret = false;
		goto done;
	}
	/* <partition>_a and <partition> is the same partition */
	suffix = &full_part_name[len];
	if ((len == strlen(full_part_name)) ||
		(strcmp(suffix, BOOT_CHAIN_SUFFIX_A) == 0)) {
		ret = true;
	}

done:
	return ret;
}

bool tegrabl_a_b_match_part_name_with_suffix(const char *part_name,
											 const char *full_part_name) {
	const char *suffix = NULL;
	uint32_t name_length;
	bool ret = false;

	name_length = strlen(full_part_name);
	if (strncmp(part_name, full_part_name, strlen(part_name)) == 0) {
		suffix = &full_part_name[strlen(part_name)];
		if ((name_length == strlen(part_name)) ||
			(strcmp(BOOT_CHAIN_SUFFIX_A, suffix) == 0) ||
			(strcmp(BOOT_CHAIN_SUFFIX_B, suffix) == 0)) {
			ret = true;
		}
	}
	return ret;
}

const char *tegrabl_a_b_get_part_suffix(const char *part)
{
	TEGRABL_ASSERT(part != NULL);

	/* "_a" and "_b" is a/b boot specific suffix, it is not supposed to be part
	 * of <partition_name> itself. E.g. <part_b> is illegal for a single
	 * partition name */
	if (strcmp(&part[strlen(part) - BOOT_CHAIN_SUFFIX_LEN],
			   BOOT_CHAIN_SUFFIX_B) == 0) {
		return BOOT_CHAIN_SUFFIX_B;
	}

	return BOOT_CHAIN_SUFFIX_A;
}

