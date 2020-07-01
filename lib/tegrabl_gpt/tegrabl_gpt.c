/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_GPT

#include <string.h>
#include <tegrabl_utils.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <tegrabl_utils.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_gpt.h>
#include <inttypes.h>

/**
 * @brief Validate the GPT header.
 *
 * @param buf buffer containing GPT header.
 * @param total_blocks Total blocks of storage device.
 *
 * @return TEGRABL_NO_ERROR if validated successfully else
 * TEGRABL_ERR_VERIFY_FAILED if signature and crc32 mismatch,
 * TEGRABL_ERR_INVALID if invalid values for other
 * header members.
 */
static tegrabl_error_t
tegrabl_gpt_validate_header(void *buf, uint64_t total_blocks)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_gpt_header *hdr = (struct tegrabl_gpt_header *)buf;
	uint32_t hdr_crc32 = 0;

	if ((buf == NULL) || (total_blocks == 0ULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	pr_debug("Validating header. Total block %" PRIu64"\n", total_blocks);

	/* First check for the signature */
	if (strncmp(hdr->signature, "EFI PART", 8)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
		pr_debug("GPT Signature failed\n");
		goto fail;
	}

	if (hdr->hdr_size != 0x5C) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		pr_debug("GPT Header size mismatch\n");
		goto fail;
	}

	/* Now check the CRC32 */
	hdr_crc32 = hdr->hdr_crc32;

	/* Clear the CRC32 field and then compute the CRC */
	hdr->hdr_crc32 = 0;

	if (hdr_crc32 != tegrabl_utils_crc32(0, (void *)buf, hdr->hdr_size)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 1);
		pr_debug("GPT Header crc mismatch\n");
		goto fail;
	}

	hdr->hdr_crc32 = hdr_crc32;

	/* Check if the start/end are inside the device limits */
	if ((hdr->first_lba >= total_blocks) ||
		(hdr->last_lba >= total_blocks)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		pr_debug("GPT header has Out of bound lbas\n");
		goto fail;
	}

fail:
	return error;
}

/**
 * @brief Reads the GPT from the device from specified offset and
 * validates for correctness.
 *
 * @param dev Device from which GPT to be read.
 * @param offset Offset of GPT in device.
 * @param buf Buffer for GPT. If *buf points to NULL then function will
 * allocate buffer enough for GPT and updates *buf to point memory location
 * containing GPT.
 * @param buf_size If *buf is NULL then *buf_size will be updated with size
 * of GPT read. Else *buf_size should specify the size of buffer passed.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_gpt_read_validate(tegrabl_bdev_t *dev,
												 off_t offset,
												 void **buf,
												 uint32_t *buf_size)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t *buffer = NULL;
	uint32_t size = 0;
	struct tegrabl_gpt_header *hdr = NULL;
	struct tegrabl_gpt_entry *entries = NULL;
	uint32_t table_size = 0;
	uint32_t crc32 = 0;

	if ((dev == NULL) || (buf == NULL) || (buf_size == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto fail;
	}

	pr_info("Reading GPT from %"PRIu64" for device %08x\n", offset,
			 dev->device_id);

	size = sizeof(struct tegrabl_gpt_entry) * TEGRABL_GPT_MAX_PARTITION_ENTRIES;
	size += TEGRABL_BLOCKDEV_BLOCK_SIZE(dev);

	if ((*buf) == NULL) {
		buffer = (uint8_t *)tegrabl_alloc_align(TEGRABL_HEAP_DMA, 4096, size);
		if (buffer == NULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			pr_debug("Failed to allocate memory for reading GPT\n");
			goto fail;
		}

		memset(buffer, 0x0, size);
		if (buf_size)
			*buf_size = size;
	} else {
		buffer = *buf;
	}

	pr_debug("Buffer size %d, required buffer size %d\n", *buf_size, size);
	if (*buf_size < size) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, __LINE__);
		goto fail;
	}

	error = tegrabl_blockdev_read_block(dev, buffer,
			DIV_CEIL_LOG2(offset, dev->block_size_log2), 1);
	if (TEGRABL_NO_ERROR != error) {
		error = tegrabl_err_set_highest_module(error, MODULE);
		pr_debug("Failed to read header\n");
		goto fail;
	}

	error = tegrabl_gpt_validate_header(buffer, dev->block_count);
	if (TEGRABL_NO_ERROR != error) {
		pr_debug("Failed to validate GPT header\n");
		goto fail;
	};

	hdr = (struct tegrabl_gpt_header *)buffer;

	pr_debug("Table start lba %"PRIu64"\n", hdr->table_start_lba);

	pr_debug("Num entries %d, Entry Size %d\n", hdr->num_entries,
			 hdr->entry_size);

	table_size = (hdr->num_entries * hdr->entry_size);
	entries =
		(struct tegrabl_gpt_entry *)(buffer + TEGRABL_BLOCKDEV_BLOCK_SIZE(dev));

	error =	tegrabl_blockdev_read_block(dev, entries,
		hdr->table_start_lba, DIV_CEIL_LOG2(table_size, dev->block_size_log2));
	if (TEGRABL_NO_ERROR != error) {
		error = tegrabl_err_set_highest_module(error, MODULE);
		pr_debug("Failed to read table entries\n");
		goto fail;
	}

	crc32 = tegrabl_utils_crc32(0, entries, table_size);
	if (hdr->table_crc32 != crc32) {
		error = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 2);
		pr_debug("Table entry CRC mismatch from header %d, computed %d\n",
				 hdr->table_crc32, crc32);
		goto fail;
	}

	if (buf && !(*buf)) {
		*buf = buffer;
	}

	return error;
fail:
	tegrabl_free(buffer);
	return error;
}

/**
 * @brief Tries to read GPT from the device (secondary, primary and from
 * specified offset). Allocate buffer for storing GPT if input buffer points
 * to NULL. If buffer is passed then buffer should have sufficient space to
 * store GPT.
 *
 * @param dev Device from which GPT to read
 * @param offset Known offset of GPT other than primary or secondary GPT.
 * @param buf Buffer to store the GPT.
 * @param buf_size Size of the buffer.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_gpt_read(tegrabl_bdev_t *dev, off_t offset,
										void **buf, uint32_t *buf_size)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	off_t offsets[3];
	uint32_t count = 0;
	uint32_t i = 0;

	if ((dev == NULL) || (buf == NULL) || (buf_size == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
		goto fail;
	}

	pr_debug("Reading gpt for %08x, device size %" PRIu64" block size %lu\n",
			 dev->device_id, dev->size, TEGRABL_BLOCKDEV_BLOCK_SIZE(dev));

	/* If offset is not zero then read GPT from this location */
	if (offset) {
		offsets[0] = offset;
		count = 1;
	} else {
		/* Read GPT from primary and secondary location */
		/* Set offset for primary GPT. */
		offsets[0] = TEGRABL_BLOCKDEV_BLOCK_SIZE(dev);

		/* Set offset for secondary GPT. */
		offsets[1] = dev->size - TEGRABL_BLOCKDEV_BLOCK_SIZE(dev);

		count = 2;
	}

	for (i = 0; i < count; i++) {
		error = tegrabl_gpt_read_validate(dev, offsets[i], buf, buf_size);
		if (TEGRABL_NO_ERROR == error) {
			pr_debug("Read gpt successfully from %" PRIu64"\n", offsets[i]);
			break;
		}
	}

	if (i >= count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		pr_debug("Could not find GPT\n");
		goto fail;
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_gpt_publish(tegrabl_bdev_t *dev,
		off_t offset, struct tegrabl_partition_info **partition_list,
		uint32_t *num_partitions)
{
	uint32_t num_entries = 0;
	uint8_t *buffer = NULL;
	uint32_t size = 0;
	struct tegrabl_gpt_entry *entries = NULL;
	struct tegrabl_gpt_header *hdr = NULL;
	uint32_t i = 0;
	uint32_t j = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition_info *partitions = NULL;
	uint64_t start_sector;
	uint64_t num_sectors;

	if ((partition_list == NULL) || (dev == NULL) || (num_partitions == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
		goto fail;
	}

	pr_debug("Publishing device %08x\n", dev->device_id);

	error = tegrabl_gpt_read(dev, offset, (void **)&buffer, &size);
	if (TEGRABL_NO_ERROR != error)
		goto fail;

	entries =
		(struct tegrabl_gpt_entry *)(buffer + TEGRABL_BLOCKDEV_BLOCK_SIZE(dev));
	hdr = (struct tegrabl_gpt_header *)buffer;
	num_entries = hdr->num_entries;

	pr_debug("Found %d partitions\n", num_entries);

	partitions = tegrabl_malloc(sizeof(struct tegrabl_partition_info) * num_entries);
	if (partitions == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		pr_debug("Failed to allocate memory for partition array\n");
		goto fail;
	}

	for (j = 0; j < num_entries; j++) {
		for (i = 0; i < TEGRABL_GPT_MAX_PARTITION_NAME; i++) {
			partitions[j].name[i] = entries[j].pname[i] & 0xFF;
		}

		partitions[j].name[i] = '\0';
		start_sector = entries[j].first_lba;
		num_sectors = entries[j].last_lba - start_sector + 1;
		partitions[j].start_sector = start_sector;
		partitions[j].num_sectors = num_sectors;
		partitions[j].total_size = num_sectors << dev->block_size_log2;

		pr_debug("%02d] Name %s\n", j + 1, partitions[j].name);
		pr_debug("Start sector: %"PRIu64"\n", start_sector);
		pr_debug("Num sectors : %"PRIu64"\n", num_sectors);
		pr_debug("Size        : %"PRIu64"\n\n", partitions[j].total_size);
	}

	*partition_list = partitions;
	*num_partitions = num_entries;

fail:
	tegrabl_free(buffer);
	return error;
}
