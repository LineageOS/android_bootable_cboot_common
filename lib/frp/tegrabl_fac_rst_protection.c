/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_FRP

#include <string.h>
#include <stdbool.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>

/* check if factory reset protection is enabled or not
 * last byte of FRP partition: 0(enabled), or non-zero(disabled) */
tegrabl_error_t tegrabl_is_frp_enabled(char *frp_part_name, bool *enabled)
{
	struct tegrabl_partition partition;
	uint64_t part_size;
	uint8_t last_byte;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	/* open FRP partition */
	ret = tegrabl_partition_open(frp_part_name, &partition);
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("failed to open partition %s\n", frp_part_name);
		return ret;
	}

	/* get partition size */
	part_size = tegrabl_partition_size(&partition);
	if (part_size == 0) {
		pr_error("failed to get partition(%s) size\n", frp_part_name);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/* move to the last byte */
	ret = tegrabl_partition_seek(&partition, part_size - 1,
								TEGRABL_PARTITION_SEEK_SET);
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("failed to seek to %lu\n", part_size - 1);
		return ret;
	}

	/* read last one byte */
	ret = tegrabl_partition_read(&partition, &last_byte, 1);
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("failed to read partition %s\n", frp_part_name);
		return ret;
	}

	/* if last byte is non-zero, FRP is disabled. Or enabled */
	*enabled = last_byte ? false : true;

	return TEGRABL_NO_ERROR;
}
