/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_FASTBOOT_PARTINFO_H
#define TEGRABL_FASTBOOT_PARTINFO_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>
#include <stdbool.h>

struct tegrabl_fastboot_partition_info {
	const char *fastboot_part_name;
	const char *tegra_part_name;
	const char *tegra_part_name_b;
};

extern const char *fastboot_var_list[];
extern const char *fastboot_partition_var_list[];
extern const struct tegrabl_fastboot_partition_info
	fastboot_partition_map_table[];
extern uint32_t size_var_list;
extern uint32_t size_pvar_list;
extern uint32_t size_partition_map_list;

const struct tegrabl_fastboot_partition_info *
	tegrabl_fastboot_get_partinfo(const char *partition_name);

/**
 * @brief Get the tegra partition name of given slot
 *
 * @param suffix slot suffix
 * @param part_info handle of partition info
 *
 * @return tegra part name of given slot
 */
const char *tegrabl_fastboot_get_tegra_part_name(const char *suffix,
	const struct tegrabl_fastboot_partition_info *part_info);

/**
 * @brief Wrapper for partition write operation.
 *
 * @param buffer input buffer to be written in partition
 * @param size Bytes to seek from current location.
 * @param aux_info Handle of partition.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_fastboot_partition_write(const void *buffer,
												 uint64_t size, void *aux_info);

/**
 * @brief Wrapper for partition seek operation.
 *
 * @param size Bytes to seek from current location.
 * @param aux_info Handle of partition.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_fastboot_partition_seek(uint64_t size, void *aux_info);
#endif
