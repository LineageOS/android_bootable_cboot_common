/*
 * Copyright (c) 2016-2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_BOOTLOADER_UPDATE

#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <tegrabl_exit.h>
#include <tegrabl_nvblob.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_bootloader_update.h>
#include <tegrabl_brbct.h>
#include <tegrabl_nct.h>
#include <tegrabl_fuse.h>

#define GPT_PART_NAME_LEN			36

#define BR_BCT_PARTITION_NAME		"BCT"
#define MB1_BCT_PARTITION_NAME		"MB1_BCT"
#define MB2_BL_PARTITION_NAME		"mb2"
#define MB1_PARTITION_NAME			"mb1"

static const char *update_slot_suffix;

static struct tegrabl_bl_update_callbacks callbacks;
static char global_nct_spec[NCT_MAX_SPEC_LENGTH];

struct updated_partition {
	const char *name;
	struct updated_partition *next;
};

struct part_dependency {
	char partition[GPT_PART_NAME_LEN];
	char dependency[GPT_PART_NAME_LEN];
};

static struct updated_partition *list;

struct part_dependency update_sequence[] = {
	/* BCT is dependent on mb2 */
	{ BR_BCT_PARTITION_NAME,	MB2_BL_PARTITION_NAME},
#if defined(CONFIG_ENABLE_A_B_SLOT)
#if defined(CONFIG_ENABLE_A_B_UPDATE)
	/* MB1 is dependent on BRBCT */
	{ MB1_PARTITION_NAME,	BR_BCT_PARTITION_NAME },
#endif
#endif
};

/** TODO
 *  1. Add a DTB check API here to validate the DTB to be updated
 *  2. binarymagic is not set by tegraflash now. Once tegraflash takes care of
 *     that field, need to add APIs to validate all BL partitions.
 */

void tegrabl_bl_update_set_callbacks(struct tegrabl_bl_update_callbacks *cbs)
{
	if (!cbs) {
		return;
	}
	callbacks.verify_payload = cbs->verify_payload;
	callbacks.update_bct = cbs->update_bct;
	callbacks.get_slot_num = cbs->get_slot_num;
	callbacks.get_slot_via_suffix = cbs->get_slot_via_suffix;
	callbacks.get_slot_suffix = cbs->get_slot_suffix;
	callbacks.is_ratchet_update_required = cbs->is_ratchet_update_required;
	callbacks.is_always_ab_partition = cbs->is_always_ab_partition;
}

static bool is_partition_updated(const char *part_name)
{
	struct updated_partition *current = list;

	while (current != NULL) {
		if (!strncmp(current->name, part_name, GPT_PART_NAME_LEN)) {
			return true;
		}
		current = current->next;
	}

	return false;
}

static const char *get_dependency(const char *part_name)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(update_sequence); i++) {
		if (!strncmp(part_name, update_sequence[i].partition,
					 GPT_PART_NAME_LEN)) {
			return update_sequence[i].dependency;
		}
	}

	return NULL;
}

static tegrabl_error_t get_spec_info(char **spec_info)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	char *spec = global_nct_spec;
	char id[NCT_MAX_SPEC_LENGTH], config[NCT_MAX_SPEC_LENGTH];

	if (strlen(global_nct_spec))
		goto end;

	err = tegrabl_nct_get_spec(id, config);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get spec!\n");
		goto end;
	}

	tegrabl_snprintf(spec, NCT_MAX_SPEC_LENGTH, "%s.%s", id, config);
	pr_info("platform spec info: %s\n", spec);

end:
	*spec_info = (char *)global_nct_spec;
	return err;
}

static bool is_new_bup_checked;
static bool is_update_all_slots_required(void *blob)
{
	static bool is_required;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (is_new_bup_checked == true) {
		goto done;
	}

	if (!callbacks.is_ratchet_update_required) {
		is_required = false;
		is_new_bup_checked = true;
		goto done;
	}

	err = callbacks.is_ratchet_update_required(blob, &is_required);
	if (err != TEGRABL_NO_ERROR) {
		is_required = false;
	}

	is_new_bup_checked = true;

done:
	return is_required;
}

static tegrabl_error_t get_full_part_name(uint32_t slot, char *part_name)
{
	if (!callbacks.get_slot_suffix) {
		return TEGRABL_NO_ERROR;
	}
	return callbacks.get_slot_suffix(slot, part_name, false);
}

static tegrabl_error_t get_slot_num(uint8_t *num)
{
	if (!callbacks.get_slot_num) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return callbacks.get_slot_num(num);
}

static tegrabl_error_t get_slot_via_suffix(const char *suffix, uint32_t *slot)
{
	if (!callbacks.get_slot_suffix) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return callbacks.get_slot_via_suffix(suffix, slot);
}

static tegrabl_error_t partition_has_slot(const char *part_name, bool *has_slot)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_partition part;
	char partition_name[GPT_PART_NAME_LEN];

	if (has_slot == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	strncpy(partition_name, part_name, sizeof(partition_name) - 1);
	partition_name[sizeof(partition_name) - 1] = '\0';
	err = get_full_part_name(1, partition_name);
	if (err != TEGRABL_NO_ERROR) {
		goto done;
	}

	err = tegrabl_partition_open(partition_name, &part);
	if (err != TEGRABL_NO_ERROR) {
		*has_slot = false;
		err = TEGRABL_NO_ERROR;
		goto done;
	}

	tegrabl_partition_close(&part);
	*has_slot = true;

done:
	return err;
}

static tegrabl_error_t is_always_ab_partition(const char *part_name,
											  bool *always_ab)
{
	TEGRABL_ASSERT(always_ab);

	if (!callbacks.is_always_ab_partition) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	*always_ab = callbacks.is_always_ab_partition(part_name);
	return TEGRABL_NO_ERROR;
}


static tegrabl_error_t write_partition(const char *part_name, uint8_t *data,
									   uint32_t size)
{
	struct tegrabl_partition part;
	tegrabl_error_t status = TEGRABL_NO_ERROR;

	pr_info("Updating partition: %s...\n", part_name);
	status = tegrabl_partition_open(part_name, &part);
	if (status != TEGRABL_NO_ERROR) {
		goto end;
	}
#if defined(CONFIG_ENABLE_QSPI)
	uint32_t storage_type;

	storage_type = tegrabl_blockdev_get_storage_type(part.block_device);
	if (storage_type == TEGRABL_STORAGE_QSPI_FLASH) {
		status = tegrabl_partition_erase(&part, false);
		if (status != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(status);
			goto end;
		}
	}
#endif

	if (!strncmp(part_name, BR_BCT_PARTITION_NAME,
				 strlen(BR_BCT_PARTITION_NAME)) &&
		callbacks.update_bct != NULL) {
		pr_info("updating bCT\n");
		status = callbacks.update_bct((uintptr_t)data, size);
		goto end;
	}

	status = tegrabl_partition_write(&part, data, size);
	if (status != TEGRABL_NO_ERROR) {
		goto end;
	}
	tegrabl_partition_close(&part);
end:
	return status;
}

static tegrabl_error_t write_a_b_partition(const char *part_name, uint8_t *data,
										   uint32_t size)
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	char full_part_name[GPT_PART_NAME_LEN] = {0};
	uint8_t num_slots = 1;
	uint32_t update_slot = 0;
	bool has_slot = false;
	bool update_all_slots = false;
	bool always_ab_update = false;
	uint8_t i;

	status = get_slot_via_suffix(update_slot_suffix, &update_slot);
	if (status != TEGRABL_NO_ERROR) {
		goto done;
	}

	/* Update the only partition if it does not support multiple slots */
	status = partition_has_slot(part_name, &has_slot);
	if (status != TEGRABL_NO_ERROR) {
		goto done;
	}
	if (!has_slot) {
		status = write_partition(part_name, data, size);
		goto done;
	}

	/* Update given slot */
	strncpy(full_part_name, part_name, sizeof(full_part_name) - 1);
	full_part_name[sizeof(full_part_name) - 1] = '\0';
	status = get_full_part_name(update_slot, full_part_name);
	if (status != TEGRABL_NO_ERROR) {
		goto done;
	}
	status = write_partition(full_part_name, data, size);
	if (status != TEGRABL_NO_ERROR) {
		goto done;
	}

	/* Update non-current slots */
	status = get_slot_num(&num_slots);
	if ((status != TEGRABL_NO_ERROR) || (num_slots == 1)) {
		goto done;
	}
	status = is_always_ab_partition(part_name, &always_ab_update);
	if (status != TEGRABL_NO_ERROR) {
		goto done;
	}

	update_all_slots = is_update_all_slots_required(NULL);
	if (update_all_slots || always_ab_update) {
		for (i = 0; i < num_slots; i++) {
			if (i == update_slot) {
				continue;
			}
			strncpy(full_part_name, part_name, sizeof(full_part_name) - 1);
			full_part_name[sizeof(full_part_name) - 1] = '\0';
			status = get_full_part_name(i, full_part_name);
			if (status != TEGRABL_NO_ERROR) {
				goto done;
			}
			status = write_partition(full_part_name, data, size);
			if (status != TEGRABL_NO_ERROR) {
				goto done;
			}
		}
	}

done:
	return status;
}

static tegrabl_error_t update_partition(tegrabl_blob_handle bh,
										const char *part_name,
										uint32_t num_entries)
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	struct tegrabl_image_entry *entry = NULL;
	uint32_t i = 0;
	uint32_t size = 0;
	uint8_t *data = NULL;
	uint8_t *aligned_data = NULL;
	char *spec_info = NULL;
	bool op_mode = false;

	if (get_spec_info(&spec_info) != TEGRABL_NO_ERROR)
		goto end;

	/* check if device is production-fused mode */
	op_mode = fuse_is_nv_production_mode();

	for (i = 0; i < num_entries; i++) {
		status = tegrabl_blob_get_entry(bh, i, (void *)&entry);

		if (status != TEGRABL_NO_ERROR) {
			goto end;
		}

		if (strncmp(entry->partname, part_name, GPT_PART_NAME_LEN)) {
			continue;
		}

		/* if a binary has valid spec info, try choose as per spec */
		if (strlen(entry->spec_info) &&
				strncmp(entry->spec_info, spec_info, NCT_MAX_SPEC_LENGTH))
			continue;

		/*
		 * if a binary non-zero op_mode, try choose as per device's op_mode:
		 *     if device is preproduction mode, binary op_mode should be 1
		 *     if device is production mode, binary op_mode should be 2
		 */
		if (entry->op_mode && ((entry->op_mode - 1) != (uint32_t)op_mode))
			continue;

		pr_debug("partition:%s size:%d version:%d, op_mode:%d spec_info:%s\n",
				 entry->partname, entry->image_size, entry->version,
				 entry->op_mode, entry->spec_info);

		/* Get partition data address and size in blob */
		status = tegrabl_blob_get_entry_data(bh, i, &data, &size);

		if (status != TEGRABL_NO_ERROR) {
			goto end;
		}

#if defined(CONFIG_ENABLE_QSPI)
	uint32_t storage_type;
	struct tegrabl_partition part;
	status = tegrabl_partition_open(entry->partname, &part);
	if (status != TEGRABL_NO_ERROR) {
		goto end;
	}
	storage_type = tegrabl_blockdev_get_storage_type(part.block_device);
	if (storage_type == TEGRABL_STORAGE_QSPI_FLASH) {
		status = tegrabl_partition_erase(&part, false);
		if (status != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(status);
			goto end;
		}
	}
#endif

		/* Align data to BLKDEV_MEM_ALIGN byte */
		if ((uintptr_t)data & (TEGRABL_BLOCKDEV_MEM_ALIGN_SIZE - 1U)) {
			aligned_data = (uint8_t *)tegrabl_memalign(TEGRABL_BLOCKDEV_MEM_ALIGN_SIZE, size);
			if (aligned_data == NULL) {
				status = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
				pr_error("Update partition: not enough memory\n");
				goto end;
			}
			memcpy(aligned_data, data, size);
			data = aligned_data;
		}

		/* update data to partition */
#if defined(CONFIG_ENABLE_A_B_SLOT) && defined(CONFIG_ENABLE_A_B_UPDATE)
		status = write_a_b_partition(entry->partname, data, entry->image_size);
#else
		status = write_partition(entry->partname, data, entry->image_size);
#endif

		goto end;
	}
end:
	if (aligned_data) {
		tegrabl_free(aligned_data);
		aligned_data = NULL;
	}

	if (i >= num_entries)
		pr_error("Binary for %s not found in the blob\n", part_name);
	else if (status == TEGRABL_NO_ERROR)
		pr_info("Update partition %s Success\n", entry->partname);
	else
		pr_error("Update partition %s Failed\n", entry->partname);

	return status;
}

static tegrabl_error_t update_partition_recursive(tegrabl_blob_handle bh,
												  const char *part_name,
												  uint32_t num_entries)
{
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	const char *part = NULL;
	struct updated_partition *new = NULL;

	if (is_partition_updated(part_name) == true) {
		goto fail;
	}

	part = get_dependency(part_name);

	if (part) {
		status = update_partition_recursive(bh, part, num_entries);
		if (status != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	status = update_partition(bh, part_name, num_entries);
	if (status != TEGRABL_NO_ERROR) {
		goto fail;
	}

	new = (struct updated_partition *)
		  tegrabl_malloc(sizeof(struct updated_partition));
	if (new == NULL) {
		pr_error("Update part recursive: Not enough memory\n");
		status = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	memset((void *)new, 0, sizeof(struct updated_partition));

	new->name = part_name;
	new->next = list;
	list = new;
	pr_info("add %s to the list\n", part_name);
fail:
	return status;
}

tegrabl_error_t tegrabl_check_and_update_bl_payload(void *buff, uint32_t size,
													const char *suffix)
{
	tegrabl_blob_handle bh;
	tegrabl_blob_type_t btype = BLOB_UPDATE;
	uint32_t num_entries;
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	struct tegrabl_image_entry *entry;
	uint8_t i = 0;
	struct updated_partition *current = NULL;

	if (buff == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	status = tegrabl_blob_init(NULL, buff, &bh);
	if (TEGRABL_ERROR_REASON(status) == TEGRABL_ERR_INVALID) {
		return TEGRABL_NO_ERROR;
	}

	if (!bh) {
		return status;
	}

	/* Check if the payload is incomplete */
	if (size < tegrabl_blob_get_size(bh)) {
		pr_info("Payload is incomplete\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

#if defined CONFIG_ENABLE_BLOB_SIGNING
	if (callbacks.verify_payload)
		status = callbacks.verify_payload(bh);
	if (status != TEGRABL_NO_ERROR)
		goto end;
#endif

	status = tegrabl_blob_get_type(bh, &btype);
	if (btype != BLOB_UPDATE) {
		pr_error("Payload is not of UPDATE type\n");
		goto end;
	}

#if defined(CONFIG_ENABLE_A_B_SLOT) && defined(CONFIG_ENABLE_A_B_UPDATE)
	bool update_both_slots;
	update_both_slots = is_update_all_slots_required(buff);
	if (update_both_slots) {
		pr_info("Ratcheting update detected, updating all slots..\n");
	}
	/* Save slot suffix to be updated */
	update_slot_suffix = suffix;
#endif

	status = tegrabl_blob_get_num_entries(bh, &num_entries);
	if (!num_entries) {
		status = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		pr_error("No entries in payload\n");
		goto end;
	}

	for (i = 0; i < num_entries; i++) {
		status = tegrabl_blob_get_entry(bh, i, (void *)&entry);
		if (status != TEGRABL_NO_ERROR)
			continue;

		status = update_partition_recursive(bh, entry->partname, num_entries);
		if (status != TEGRABL_NO_ERROR) {
			pr_error("Error in update %s\n", entry->partname);
			goto end;
		}
	}

	if (status != TEGRABL_NO_ERROR) {
		pr_error("Payload update failed\n");
		tegrabl_mdelay(5000);
		goto end;
	}

	/* Flush devices in case if it caches inside */
	tegrabl_partition_manager_flush_cache();

#if defined(CONFIG_ENABLE_A_B_SLOT) && defined(CONFIG_ENABLE_A_B_UPDATE)
	/* Set false to this flag since it is the end of one BUP update */
	is_new_bup_checked = false;
#endif

	pr_info("Successfully updated partitions from payload\n");

end:
	pr_info("clear list\n");
	current = list;
	while (current) {
		list = list->next;
		pr_info("list name:%s, next=%p\n", current->name, current->next);
		tegrabl_free(current);
		current = list;
	}

	return status;
}
