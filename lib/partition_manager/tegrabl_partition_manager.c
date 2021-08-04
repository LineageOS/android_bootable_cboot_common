/*
 * Copyright (c) 2015-2020, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_PARTITION_MANAGER

#include "build_config.h"
#include <string.h>
#include <tegrabl_utils.h>
#include <tegrabl_malloc.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_debug.h>
#include <tegrabl_gpt.h>
#include <tegrabl_nvpt.h>
#include <inttypes.h>
#include <tegrabl_cbo.h>
#include <ctype.h>

#if defined(CONFIG_ENABLE_A_B_SLOT)
#include <tegrabl_a_b_partition_naming.h>
#include <tegrabl_a_b_boot_control.h>
#endif

#define AUX_INFO_PARTITION_LOOKUP_ERR		20
#define AUX_INFO_PARTITION_BOOT_LOOKUP_ERR	21
#define AUX_INFO_PARTITION_NOT_FOUND		22
#define AUX_INFO_PARTITION_NOT_INIT			23
#define AUX_INFO_PARTITION_GUID_NOT_FOUND	24

/**
 * @brief Stores the partition list for storage devices
 */
struct tegrabl_storage_info {
	struct list_node node;
	tegrabl_bdev_t *bdev;
	uint32_t num_partitions;
	struct tegrabl_partition_info *partitions;
};

/* List of storage device information */
static struct list_node *storage_list;

#if defined(CONFIG_ENABLE_RECOVERY_VERIFY_WRITE)
static uint32_t set_verify_flag;
static bool verify_all_partitions;
static struct list_node verify_list;
#endif

/**
 * @brief Storage device can have partition table in any format (GPT/PT/MBR etc.).
 * Libraries for interpreting this format should have publish API with below
 * prototype.
 *
 * @param bdev Handle to storage device.
 * @param offset Offset to look partition table in storage device.
 * @param partition_list List partitions found.
 * @param num_partitions Number of partitions found.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t (*publish_partition[])(tegrabl_bdev_t *bdev,
		off_t offset, struct tegrabl_partition_info **partition_list,
		uint32_t *num_partitions) = {
#if defined(CONFIG_ENABLE_GPT)
	tegrabl_gpt_publish,
#endif
#if defined(CONFIG_ENABLE_NVPT)
	tegrabl_nvpt_publish,
#endif
};

tegrabl_error_t tegrabl_partition_pt_has_3_levels(bool *info)
{
	if (info == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
	}

#if defined(CONFIG_ENABLE_NVPT)
	return tegrabl_nvpt_pt_has_3_levels(info);
#endif
	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
}

static inline bool match_partition_name(const char *partition_name, const char *name)
{
#if defined(CONFIG_ENABLE_A_B_SLOT)
	return tegrabl_a_b_match_part_name(partition_name, name);
#else
	return strcmp(partition_name, name) == 0;
#endif
}

tegrabl_error_t tegrabl_partition_lookup_bdev(const char *partition_name, struct tegrabl_partition *partition,
											  tegrabl_bdev_t *bdev)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition_info *partition_info = NULL;
	struct tegrabl_storage_info *entry = NULL;
	uint32_t num_partitions = 0;
	uint32_t i = 0;

	if ((partition_name == NULL) || (partition == NULL) || (bdev == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_PARTITION_LOOKUP_ERR);
		goto fail;
	}

	if (storage_list == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, AUX_INFO_PARTITION_NOT_INIT);
		pr_error("Partition manager might not be initialized.\n");
		goto fail;
	}
	pr_trace("Open partition %s\n", partition_name);


	/* find the partition with partition_name in the given block device */
	list_for_every_entry(storage_list, entry, struct tegrabl_storage_info, node) {
		if (entry->bdev != bdev) {
			continue;
		}

		num_partitions = entry->num_partitions;
		partition_info = entry->partitions;

		for (i = 0; i < num_partitions; i++) {
			if (strcmp(partition_info[i].name, partition_name) == 0) {
				break;
			}
		}

		if (i >= num_partitions) {
			partition_info = NULL;
		} else {
			break;
		}
	}

	if (partition_info == NULL) {
		pr_debug("Cannot find partition %s\n", partition_name);
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, AUX_INFO_PARTITION_NOT_FOUND);
		(void)memset(partition, 0x0, sizeof(*partition));
		goto fail;
	}

	partition->partition_info = &partition_info[i];
	partition->block_device = entry->bdev;
	partition->offset = 0;

fail:
	return error;
}

tegrabl_error_t tegrabl_partition_boot_guid_lookup_bdev(char *pt_type_guid,
														struct tegrabl_partition *partition,
														tegrabl_bdev_t *bdev)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition_info *partition_info = NULL;
	struct tegrabl_storage_info *entry = NULL;
	uint32_t num_partitions = 0;
	uint32_t i = 0;
#if defined(CONFIG_ENABLE_A_B_SLOT)
	uint8_t rootfs_id;
#endif

	if ((partition == NULL) || (bdev == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_PARTITION_BOOT_LOOKUP_ERR);
		pr_error("Invalid args passed\n");
		goto fail;
	}

	list_for_every_entry(storage_list, entry, struct tegrabl_storage_info, node) {
		if (entry->bdev != bdev) {
			continue;
		}
		num_partitions = entry->num_partitions;
		partition_info = entry->partitions;

		/* Check for boot partition by matching partition type UUID */
		for (i = 0; i < num_partitions; i++) {
			if (strncasecmp(partition_info[i].ptype_guid, pt_type_guid, GUID_STR_LEN) == 0) {
				pr_trace("Partition type GUID matched\n\n");
				goto partition_found;
			}
		}
	}

	if (partition_info == NULL) {
		pr_error("No partition found with matching boot GUID\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, AUX_INFO_PARTITION_GUID_NOT_FOUND);
		memset(partition, 0x0, sizeof(struct tegrabl_partition));
		goto fail;
	}

	/* Fall back: assume 0th partition is boot partition*/
	if (i >= num_partitions) {
		pr_info("Fallback: assuming 0th partition is boot partition\n");
		i = 0;
	} else {
		pr_info("Boot partition: %u\n", i);
	}

#if defined(CONFIG_ENABLE_A_B_SLOT)
	error = tegrabl_a_b_get_current_rootfs_id(NULL, &rootfs_id);
	if (error == TEGRABL_NO_ERROR) {
		if (rootfs_id == ROOTFS_A) {
			error = tegrabl_partition_lookup_bdev("APP",
							partition, bdev);
		} else if (rootfs_id == ROOTFS_B) {
			error = tegrabl_partition_lookup_bdev("APP_b",
							partition, bdev);
		} else {
			/* Rootfs A/B are both unbootable */
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}

		if (error == TEGRABL_NO_ERROR) {
			/* Found partition, goto fail directly */
			pr_info("Set %dth partition is boot partition\n",
				rootfs_id);
			goto fail;
		}
		/*
		 * Didn't find APP or APP_b,
		 * scan next storage to find valid rootfs.
		 */
		goto fail;

	} else {
		/* rootfs AB isn't enabled, restore error */
		error = TEGRABL_NO_ERROR;
	}
#endif

partition_found:
	partition->partition_info = &partition_info[i];
	partition->block_device = entry->bdev;
	partition->offset = 0;

fail:
	return error;
}

tegrabl_error_t tegrabl_partition_open(const char *partition_name,
									   struct tegrabl_partition *partition)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition_info *partition_info = NULL;
	struct tegrabl_storage_info *entry = NULL;
	uint32_t num_partitions = 0;
	uint32_t i = 0;

	if ((partition_name == NULL) || (partition == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 19);
		goto fail;
	}

	pr_trace("Open partition %s\n", partition_name);


	if (storage_list == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, AUX_INFO_PARTITION_NOT_INIT);
		pr_error("Partition manager might not be initialized.\n");
		goto fail;
	}

	list_for_every_entry(storage_list, entry,
									struct tegrabl_storage_info, node) {
		num_partitions = entry->num_partitions;
		partition_info = entry->partitions;

		for (i = 0; i < num_partitions; i++) {
			if (match_partition_name(partition_info[i].name, partition_name)) {
				break;
			}
		}

		if (i >= num_partitions) {
			partition_info = NULL;
		} else {
			break;
		}
	}

	if (partition_info == NULL) {
		pr_error("Cannot find partition %s\n", partition_name);
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		(void)memset(partition, 0x0, sizeof(*partition));
		goto fail;
	}

	partition->partition_info = &partition_info[i];
	partition->block_device = entry->bdev;
	partition->offset = 0;

fail:
	return error;
}

uint64_t tegrabl_partition_size(struct tegrabl_partition *partition)
{
	struct tegrabl_partition_info *partition_info = NULL;

	if (partition == NULL) {
		goto fail;
	}

	partition_info = partition->partition_info;

	if (partition_info == NULL) {
		pr_debug("Partition handle is not initialized appropriately.\n");
		goto fail;
	}

	return partition_info->total_size;
fail:
	return 0;
}

void tegrabl_partition_close(struct tegrabl_partition *partition)
{
	if (partition != NULL) {
		(void)memset(partition, 0x0, sizeof(*partition));
	}
}

tegrabl_error_t tegrabl_partition_erase(struct tegrabl_partition *partition,
		bool secure)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition_info *partition_info = NULL;

	if (partition == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	partition_info = partition->partition_info;

	if ((partition_info == NULL) || (partition->block_device == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 2);
		pr_debug("Partition handle is not initialized appropriately.\n");
		goto fail;
	}

	pr_debug("Erasing partition %s\n", partition_info->name);

	error = tegrabl_blockdev_erase(partition->block_device,
								   (uint32_t)partition_info->start_sector,
								   (uint32_t)partition_info->num_sectors,
								   secure);

	if (TEGRABL_NO_ERROR != error) {
		error = tegrabl_err_set_highest_module(error, MODULE);
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_partition_write(struct tegrabl_partition *partition,
										const void *buf, size_t num_bytes)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition_info *partition_info = NULL;
	off_t offset = 0;

	if ((partition == NULL) || (buf == NULL) || (num_bytes == 0U)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	partition_info = partition->partition_info;

	if ((partition_info == NULL) || (partition->block_device == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 3);
		pr_debug("Partition handle is not initialized appropriately.\n");
		goto fail;
	}

	pr_debug("Writing %s from offset %"PRIu64 "num_bytes %zd\n",
			 partition_info->name, partition->offset, num_bytes);

	if (partition_info->total_size < (num_bytes + partition->offset)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		pr_debug("Cannot write beyond partition boundary for %s\n",
				 partition_info->name);
		goto fail;
	}

	offset = partition->offset + (partition_info->start_sector <<
								  partition->block_device->block_size_log2);

	error = tegrabl_blockdev_write(partition->block_device, buf,
			offset, num_bytes);

	if (TEGRABL_NO_ERROR != error) {
		error = tegrabl_err_set_highest_module(error, MODULE);
	}

	partition->offset += num_bytes;

fail:
	return error;
}

tegrabl_error_t tegrabl_partition_start_in_block(
		struct tegrabl_partition *partition,
		uint32_t *start)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition_info *partition_info = NULL;

	if ((partition == NULL) || (start == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto fail;
	}

	partition_info = partition->partition_info;

	if (partition_info == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 4);
		pr_debug("Partition handle is not initialized appropriately.\n");
		goto fail;
	}

	*start = (uint32_t)partition_info->start_sector;

fail:
	return error;
}

tegrabl_error_t tegrabl_partition_read(struct tegrabl_partition *partition,
									   void *buf, size_t num_bytes)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition_info *partition_info = NULL;
	off_t offset = 0;

	if ((partition == NULL) || (buf == NULL) || (num_bytes == 0U)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
		goto fail;
	}

	partition_info = partition->partition_info;

	if ((partition_info == NULL) || (partition->block_device == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 4);
		pr_error("Partition handle is not initialized appropriately.\n");
		goto fail;
	}

	if (partition_info->total_size < (num_bytes + partition->offset)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 1);
		pr_error("Cannot read beyond partition boundary for %s\n",
				 partition_info->name);

		goto fail;
	}

	pr_debug("Reading %s from offset %"PRIu64 "num_bytes %zd\n",
			 partition_info->name, partition->offset, num_bytes);

	offset = partition->offset + (partition_info->start_sector <<
								  partition->block_device->block_size_log2);

	error = tegrabl_blockdev_read(partition->block_device, buf,
			offset, num_bytes);

	if (TEGRABL_NO_ERROR != error) {
		error = tegrabl_err_set_highest_module(error, MODULE);
		goto fail;
	}

	partition->offset += num_bytes;

fail:
	return error;
}

tegrabl_error_t tegrabl_partition_seek(struct tegrabl_partition *partition,
				int64_t offset, tegrabl_partition_seek_t origin)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition_info *partition_info = NULL;
	int64_t new_offset = 0;

	if (partition == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
		goto fail;
	}

	partition_info = partition->partition_info;

	if ((partition_info == NULL) || (partition->block_device == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 5);
		pr_debug("Partition handle is not initialized appropriately.\n");
		goto fail;
	}

	switch (origin) {
	case TEGRABL_PARTITION_SEEK_SET:
		new_offset = 0;
		break;
	case TEGRABL_PARTITION_SEEK_CUR:
		new_offset = (int64_t)partition->offset;
		break;
	case TEGRABL_PARTITION_SEEK_END:
		new_offset = (int64_t)partition_info->total_size;
		break;
	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
		break;
	}

	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	new_offset += offset;

	if (new_offset < 0) {
		error = TEGRABL_ERROR(TEGRABL_ERR_UNDERFLOW, 0);
		goto fail;
	}

	if (partition_info->total_size < (uint64_t)new_offset) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 2);
		goto fail;
	}

	pr_debug("Seeking %s from offset %"PRIu64 "to %"PRIu64"\n",
			 partition_info->name, partition->offset, new_offset);

	partition->offset = (uint64_t)new_offset;

fail:
	return error;
}

int64_t tegrabl_partition_tell(struct tegrabl_partition *partition)
{
	if ((partition == NULL) || (partition->partition_info == NULL) ||
		(partition->block_device == NULL)) {
		return -1;
	}

	return (int64_t)partition->offset;
}

tegrabl_error_t tegrabl_partition_async_read(
	struct tegrabl_partition *partition, void *buf,
	uint64_t start_sector, uint64_t num_sectors,
	struct tegrabl_blockdev_xfer_info **p_xfer)
{
	struct tegrabl_partition_info *partition_info = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_blockdev_xfer_info *xfer;

	if ((partition == NULL) || (buf == NULL) || (num_sectors == 0ULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 7);
		goto fail;
	}

	partition_info = partition->partition_info;

	if ((partition_info == NULL) || (partition->block_device == NULL)) {
		pr_debug("Partition handle is not initialized appropriately.\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 8);
		goto fail;
	}

	if (partition_info->num_sectors < (start_sector + num_sectors)) {
		pr_debug("Cannot read beyond partition boundary for %s\n",
				 partition_info->name);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 9);
		goto fail;
	}

	pr_debug("Async read %s %"PRIu64" sectors from %"PRIu64" sector\n",
			 partition_info->name, num_sectors, start_sector);

	xfer = tegrabl_malloc(sizeof(struct tegrabl_blockdev_xfer_info));
	if (xfer == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 10);
		goto fail;
	}

	xfer->dev = partition->block_device;
	xfer->buf = buf;
	xfer->xfer_type = TEGRABL_BLOCKDEV_READ;
	xfer->is_non_blocking = true;
	xfer->start_block = (uint32_t)(partition_info->start_sector + start_sector);
	xfer->block_count = (uint32_t)num_sectors;
	err = tegrabl_blockdev_xfer(xfer);
	*p_xfer = xfer;

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: exit error\n", __func__);
	}
	return err;
}

tegrabl_error_t tegrabl_partition_async_write(
		struct tegrabl_partition *partition, void *buf, uint64_t start_sector,
		uint64_t num_sectors,
		struct tegrabl_blockdev_xfer_info **p_xfer)
{
	struct tegrabl_partition_info *partition_info = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_blockdev_xfer_info *xfer;

	if ((partition == NULL) || (buf == NULL) || (num_sectors == 0ULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 11);
		goto fail;
	}

	partition_info = partition->partition_info;

	if ((partition_info == NULL) || (partition->block_device == NULL)) {
		pr_debug("Partition handle is not initialized appropriately.\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 12);
		goto fail;
	}

	if (partition_info->num_sectors < (start_sector + num_sectors)) {
		pr_debug("Cannot write beyond partition boundary for %s\n",
				 partition_info->name);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 13);
		goto fail;
	}

	pr_debug("Async write %s %"PRIu64" sectors from %"PRIu64" sector\n",
			 partition_info->name, num_sectors, start_sector);

	xfer = tegrabl_malloc(sizeof(struct tegrabl_blockdev_xfer_info));
	if (xfer == NULL) {
	    err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 14);
	    goto fail;
	}

	xfer->dev = partition->block_device;
	xfer->buf = buf;
	xfer->xfer_type = TEGRABL_BLOCKDEV_READ;
	xfer->is_non_blocking = true;
	xfer->start_block = (uint32_t)(partition_info->start_sector + start_sector);
	xfer->block_count = (uint32_t)num_sectors;
	err = tegrabl_blockdev_xfer(xfer);
	*p_xfer = xfer;

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: exit error\n", __func__);
	}
	return err;
}

tegrabl_error_t tegrabl_partition_publish(tegrabl_bdev_t *dev, off_t offset)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	int32_t i = 0;
	uint32_t num = 0;
	struct tegrabl_storage_info  *storage_info = NULL;
	struct tegrabl_partition_info *partitions = NULL;
	size_t array_size;
	array_size = ARRAY_SIZE(publish_partition);
	if (dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 15);
		goto fail;
	}

	if (dev->published == true) {
		pr_info("Already published: %08x\n", dev->device_id);
	} else {
		/* Try publishing partition from registered publishers one at a time,
		 * stop as soon as one of them is successful */
		for (i = 0; i < (int32_t)array_size; i++) {
			if (publish_partition[i] != NULL) {
				error = publish_partition[i](dev, offset, &partitions, &num);
				if (TEGRABL_NO_ERROR != error) {
					continue;
				}
				pr_info("Found %d partitions in %s (instance %x)\n",
						num, tegrabl_blockdev_get_name((dev->device_id >> 16) & 0xfU),
						dev->device_id & 0xfU);

				storage_info = tegrabl_malloc(sizeof(*storage_info));
				if (storage_info == NULL) {
					error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
					pr_debug("Failed to allocate memory for storage info\n");
					goto fail;
				}
				storage_info->partitions = partitions;
				storage_info->num_partitions = num;
				storage_info->bdev = dev;

				list_add_head(storage_list, &storage_info->node);

				break;
			}
		}

		/* None of the partition-managers were able to publish this device */
		if (i >= (int32_t)array_size) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
			pr_warn("Cannot find any partition table for %08x\n",
					dev->device_id);
		}

		/* Don't set the published flag unless at least 1 partition is found */
		dev->published = (error == TEGRABL_NO_ERROR);

		if (TEGRABL_NO_ERROR != error) {
			pr_debug("Failed to publish %08x\n", dev->device_id);
		}
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_partitions_unpublish(tegrabl_bdev_t *dev)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_storage_info  *entry = NULL;

	if (dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 16);
		goto fail;
	}

	if (dev->published == true) {
		/* Try to unpublish only if it has been published */

		if (storage_list == NULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
			pr_debug("Partition manager might not be initialized.\n");
			goto fail;
		}

		list_for_every_entry(storage_list, entry,
											struct tegrabl_storage_info, node) {
			if (entry->bdev->device_id == dev->device_id) {
				tegrabl_free(entry->partitions);
				list_delete(&entry->node);
				tegrabl_free(entry);
				break;
			}
		}

		dev->published = false;
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_partition_manager_init(void)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	tegrabl_bdev_t *dev = NULL;

	pr_trace("Partition Manager init started\n");

	storage_list = tegrabl_malloc(sizeof(*storage_list));

	if (storage_list == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		pr_debug("Failed to allocate memory for storage info list.\n");
		goto fail;
	}

	list_initialize(storage_list);

	while ((dev = tegrabl_blockdev_next_device(dev)) != NULL) {
		if (tegrabl_blockdev_get_storage_type(dev) != TEGRABL_STORAGE_SDMMC_RPMB) {
			pr_trace("Finding partitions in %s (instance %x)\n",
					 tegrabl_blockdev_get_name((dev->device_id >> 16) & 0xfU), dev->device_id & 0xfU);
			tegrabl_partition_publish(dev, 0);
		}
	}

#if defined(CONFIG_ENABLE_RECOVERY_VERIFY_WRITE)
	list_initialize(&verify_list);
#endif

fail:
	return error;
}

void tegrabl_partition_manager_flush_cache(void)
{
	tegrabl_bdev_t *dev = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_debug("Flushing Caches of all storage devices\n");

	while ((dev = tegrabl_blockdev_next_device(dev)) != NULL) {
		pr_debug("Flushing cache of %08x....", dev->device_id);
		error = tegrabl_blockdev_ioctl(dev, TEGRABL_IOCTL_DEVICE_CACHE_FLUSH,
				dev);
		if (error != TEGRABL_NO_ERROR) {
			pr_warn("failed to flush cache of storage device\n");
		}
	}
}

#if defined(CONFIG_ENABLE_RECOVERY_VERIFY_WRITE)
#define READ_BUFFER_SIZE (10 * 1024 * 1024)
static tegrabl_error_t read_verify_partition(
		struct tegrabl_partition partition,
		struct verify_list_info *verify_info,
		uint8_t *buffer)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	uint64_t chunk_size = 0;
	uint64_t size = 0;
	uint64_t offset = 0;
	uint32_t read_partition_crc32 = 0;

	/* Set offset and size appropriately. */
	offset = partition.offset;
	size = verify_info->size;

	pr_debug("Seeking partition to %"PRIu64" bytes\n", offset);

	error = tegrabl_partition_seek(&partition, offset,
			TEGRABL_PARTITION_SEEK_SET);

	if (TEGRABL_NO_ERROR != error) {
		pr_error("Failed to seek partition to offset");
		goto fail;
	}

	while (size) {
		chunk_size = MIN(READ_BUFFER_SIZE, size);
		pr_debug("Reading %"PRIu64" bytes from partition\n", chunk_size);
		error = tegrabl_partition_read(&partition, buffer, chunk_size);
		if (TEGRABL_NO_ERROR != error) {
			pr_error("Failed to read partition");
			goto fail;
		}
		read_partition_crc32 = tegrabl_utils_crc32(
				read_partition_crc32, buffer, chunk_size);
		size -= chunk_size;
	}
	pr_debug("crc32: Actual=0x%08x | Calculated=0x%08x\n",
			read_partition_crc32, verify_info->crc32);

	error = (verify_info->crc32 == read_partition_crc32) ?
		TEGRABL_NO_ERROR : TEGRABL_ERR_VERIFY_FAILED;

fail:
	return error;
}

tegrabl_error_t tegrabl_partition_verify_write(void)
{
	struct verify_list_info *entry = NULL, *temp;
	struct tegrabl_partition partition;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t *buffer = NULL;
	bool verify_status = false;

	/* Check & return if verify list empty */
	if (list_is_empty(&verify_list))
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 17);

	buffer = (uint8_t *) tegrabl_memalign(4096, READ_BUFFER_SIZE);
	if (!buffer) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		pr_info("Failed to allocate nv3p buffer");
		goto fail;
	}

	list_for_every_entry_safe(&verify_list, entry,
		temp, struct verify_list_info, node) {
		error = tegrabl_partition_open(entry->name, &partition);
		if (TEGRABL_NO_ERROR != error) {
			pr_error("Failed to open partition");
			break;
		}
		/* Call verify only if crc for the partition is received */
		if (entry->crc32 != 0) {
			error = read_verify_partition(partition, entry, buffer);
		} else {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 18);
		}

		tegrabl_partition_close(&partition);

		if (error == TEGRABL_ERR_VERIFY_FAILED) {
			pr_error("Verifying %s Partition [Failed]\n", entry->name);
			verify_status = true;
		} else {
			pr_info("Verifying %s Partition [Successful]\n", entry->name);
		}
		list_delete(&entry->node);
		tegrabl_free(entry);
	}

fail:
	if (buffer)
		tegrabl_free(buffer);

	return verify_status ? TEGRABL_ERR_VERIFY_FAILED : error;
}

static tegrabl_error_t tegrabl_partition_add_verify_node(char *partition_name)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct verify_list_info *verify_handle;

	/* Check for duplicate node */
	list_for_every_entry(&verify_list, verify_handle,
			struct verify_list_info, node) {
		if (strcmp(verify_handle->name, partition_name) == 0) {
			pr_info("Partition already enabled for verification\n");
			goto fail;
		}
	}

	verify_handle = tegrabl_malloc(sizeof(struct verify_list_info));
	if (verify_handle == NULL) {
		pr_error("Failed to alloc memory for Verify list\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	verify_handle->size = 0;
	verify_handle->crc32 = 0;
	strcpy(verify_handle->name, partition_name);
	list_add_head(&verify_list, &verify_handle->node);
	pr_debug("Added %s Partition to verify list\n",	verify_handle->name);

fail:
	return error;
}

tegrabl_error_t tegrabl_partition_set_crc(struct tegrabl_partition *partition,
		uint32_t verify_crc, uint32_t verify_size, bool is_sparse)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	char *partition_name = partition->partition_info->name;
	struct verify_list_info *entry = NULL;

	list_for_every_entry(&verify_list, entry,
			struct verify_list_info, node) {
		if (strcmp(entry->name, partition_name) == 0) {
			break;
		}
	}
	/*
	 *  Currently sparse partition verification is not supported.
	 *  So delete this node from verify list.
	 */
	if (is_sparse) {
		list_delete(&entry->node);
		goto fail;
	}

	if (entry == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}
	entry->size = verify_size;
	entry->crc32 = verify_crc;

fail:
	return error;
}

bool tegrabl_partition_is_verify_enabled(struct tegrabl_partition *partition)
{
	bool status = false;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	char *partition_name = partition->partition_info->name;
	struct verify_list_info *entry = NULL;

	if (set_verify_flag) {
		if (verify_all_partitions) {
			/*
			 * For verify all case, create verify_list_info
			 * when partition write is called.
			 */
			error = tegrabl_partition_add_verify_node(partition_name);
			if (error == TEGRABL_NO_ERROR)
				status = true;
		} else {
			/* find in verify list, if verify all is not set */
			list_for_every_entry(&verify_list, entry,
				struct verify_list_info, node) {
				if (!strcmp(entry->name, partition_name)) {
					status = true;
					break;
				}
			}
		}
	}
	return status;
}

tegrabl_error_t tegrabl_partition_verify_enable(char *partition_name,
		uint32_t device_type, uint32_t instance)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct verify_list_info *part_entry = NULL;

	if (!strcmp(partition_name, "all")) {
		verify_all_partitions = true;
		set_verify_flag = true;
		goto fail;
	}

	error = tegrabl_partition_add_verify_node(partition_name);
	if (error == TEGRABL_NO_ERROR) {
		set_verify_flag = true;
		pr_debug("Added %s partition to verify list\n",
				part_entry->name);
	}

	TEGRABL_UNUSED(device_type);
	TEGRABL_UNUSED(instance);
fail:
	return error;
}
#endif
