/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_utils.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_fastboot_partinfo.h>
#include <tegrabl_a_b_partition_naming.h>

const struct tegrabl_fastboot_partition_info
	fastboot_partition_map_table[] = {
	{ "recovery", "recovery_a", "recovery_b", FASTBOOT_FS_TYPE_BASIC },
	{ "boot", "boot_a", "boot_b", FASTBOOT_FS_TYPE_BASIC},
	{ "dtb", "kernel-dtb_a", "kernel-dtb_b", FASTBOOT_FS_TYPE_BASIC},
	{ "system", "system_a", "system_b", FASTBOOT_FS_TYPE_EXT4 },
	{ "userdata", "userdata", NULL, FASTBOOT_FS_TYPE_EXT4 },
	{ "vendor", "vendor_a", "vendor_b", FASTBOOT_FS_TYPE_EXT4 },
	{ "bmp", "bootlogo", "bootlogo_b", FASTBOOT_FS_TYPE_BASIC },
	{ "rpb", "RPB", NULL, FASTBOOT_FS_TYPE_BASIC },
	{ "odm", "odm_a", "odm_b", FASTBOOT_FS_TYPE_EXT4 },
	{ "avb_custom_key", "avb_custom_key", NULL, FASTBOOT_FS_TYPE_BASIC },
};

const char *tegrabl_nv_private_partition_list[] = {
	"BCT",
	"MB1_BCT",
	"mb1",
	"mts-preboot",
	"mts-bootpack",
	"sce-fw",
	"eks",
	"sc7",
	"bpmp-fw",
	"NCT",
	"primary_gpt",
	"secondary_gpt"
};

const char *fastboot_var_list[] = {
	"version-bootloader",
	"serialno",
	"product",
	"secure",
	"unlocked",
	"current-slot",
	"slot-suffixes",
};

const char *fastboot_partition_var_list[] = {
	"partition-size:",
	"partition-type:",
	"has-slot:",
};

uint32_t size_var_list = ARRAY_SIZE(fastboot_var_list);
uint32_t size_pvar_list = ARRAY_SIZE(fastboot_partition_var_list);
uint32_t size_partition_map_list = ARRAY_SIZE(fastboot_partition_map_table);

const char *tegrabl_fastboot_get_tegra_part_name(const char *suffix,
		const struct tegrabl_fastboot_partition_info *part_info) {
	if (!part_info) {
		return NULL;
	}
	if (!strcmp("_b", suffix)) {
		return part_info->tegra_part_name_b;
	}
	return part_info->tegra_part_name;
}

const struct tegrabl_fastboot_partition_info *
	tegrabl_fastboot_get_partinfo(const char *partition_name)
{
	uint32_t i;
	uint32_t num_partitions = size_partition_map_list;
	uint32_t size_nv_private_partition_list =
			ARRAY_SIZE(tegrabl_nv_private_partition_list);
	static struct tegrabl_fastboot_partition_info partinfo;
	const char *fastboot_part_name = NULL;
	const char *private_part_name = NULL;

	for (i = 0; i < num_partitions; i++) {
		fastboot_part_name = fastboot_partition_map_table[i].fastboot_part_name;
		if (tegrabl_a_b_match_part_name_with_suffix(fastboot_part_name,
													partition_name)) {
			return &fastboot_partition_map_table[i];
		}
	}

	/* Do not expose any nv private partition */
	for (i = 0; i < size_nv_private_partition_list; i++) {
		private_part_name = tegrabl_nv_private_partition_list[i];
		if (tegrabl_a_b_match_part_name_with_suffix(private_part_name,
													partition_name)) {
			return NULL;
		}
	}
	partinfo.fastboot_part_name = partition_name;
	partinfo.tegra_part_name    = partition_name;
	partinfo.tegra_part_name_b  = partition_name;
	partinfo.fastboot_part_type = FASTBOOT_FS_TYPE_BASIC;

	return (const struct tegrabl_fastboot_partition_info *)&partinfo;
}

tegrabl_error_t tegrabl_fastboot_partition_write(const void *buffer,
												 uint64_t size, void *aux_info)
{
	pr_debug("Writing %"PRIu64" bytes to partition", size);

	return tegrabl_partition_write((struct tegrabl_partition *)aux_info, buffer,
								   size);
}

tegrabl_error_t tegrabl_fastboot_partition_seek(uint64_t size, void *aux_info)
{
	pr_debug("Seeking partition by %"PRIu64" bytes\n", size);

	return tegrabl_partition_seek((struct tegrabl_partition *)aux_info, size,
								  TEGRABL_PARTITION_SEEK_CUR);
}

