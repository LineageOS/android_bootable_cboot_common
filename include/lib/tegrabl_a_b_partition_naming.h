/*
 * Copyright (c) 2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __TEGRABL_A_B_PARTITION_NAMING_H__
#define __TEGRABL_A_B_PARTITION_NAMING_H__

/**
 * @brief Check if full_part_name match the given part_name. NULL suffix will be
 *        treated as _a partition as legacy.
 *
 * @param part_name standard partition name
 * @param fullpart_name given partition name to be checked
 *
 * @return true if given partition is _a partition
 */
bool tegrabl_a_b_match_part_name(const char *part_name,
								 const char *full_part_name);

/**
 * @brief Check if full_part_name match the given part_name. A slot or B slot
 *        suffix is acceptable
 *
 * @param part_name standard partition name
 * @param fullpart_name given partition name to be checked
 *
 * @return true if given partition is _a or _b partition
 */
bool tegrabl_a_b_match_part_name_with_suffix(const char *part_name,
											 const char *full_part_name);

/**
 * @brief Get the suffix of given partition_name
 *
 * @param part_name given partition name to be checked
 *
 * @return suffix of the partition name
 */
const char *tegrabl_a_b_get_part_suffix(const char *part);

#endif /* __TEGRABL_A_B_PARTITION_NAMING_H__ */
