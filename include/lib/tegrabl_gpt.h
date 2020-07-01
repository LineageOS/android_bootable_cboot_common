/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_GPT_H
#define TEGRABL_GPT_H

#include <stddef.h>
#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_blockdev.h>

struct tegrabl_gpt_guid {
	uint8_t guid[16];
};

#define TEGRABL_GPT_MAX_PARTITION_NAME		36
#define TEGRABL_GPT_MAX_PARTITION_ENTRIES	128

/**
 * @brief Defines the header structure of GPT.
 */
struct tegrabl_gpt_header {
	char signature[8];
	uint32_t revision;
	uint32_t hdr_size;
	uint32_t hdr_crc32;
	uint32_t rsvd;
	uint64_t curr_lba;
	uint64_t backup_lba;
	uint64_t first_lba;
	uint64_t last_lba;
	struct tegrabl_gpt_guid disk_guid;
	uint64_t table_start_lba;
	uint32_t num_entries;
	uint32_t entry_size;
	uint32_t table_crc32;
};

/**
 * @brief Defines the structure of partition entry in GPT.
 */
struct tegrabl_gpt_entry {
	struct tegrabl_gpt_guid ptype_guid;
	struct tegrabl_gpt_guid unique_guid;
	uint64_t first_lba;
	uint64_t last_lba;
	uint64_t attribs;
	uint16_t pname[TEGRABL_GPT_MAX_PARTITION_NAME];
};

/**
 * @brief Tries to read GPT from the device and returns information
 * about all partitions in partition_list.
 *
 * @param dev Device to be published
 * @param offset Known offset of GPT if not primary or secondary GPT.
 * @param partition_list If GPT is successfully found then this will
 * point to memory location containing list of partitions found.
 * @param num_partitions if GPT  is successfully found then this will
 * be updated with number of partition found.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code.
 */
tegrabl_error_t tegrabl_gpt_publish(tegrabl_bdev_t *dev,
		off_t offset, struct tegrabl_partition_info **partition_list,
		uint32_t *num_partitions);

#endif
