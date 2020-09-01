/*
 * Copyright (c) 2016-2021, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_AB_BOOTCTRL

#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_utils.h>
#include <stdbool.h>
#include <inttypes.h>
#include <tegrabl_a_b_boot_control.h>
#include <tegrabl_soc_misc.h>
#include <tegrabl_malloc.h>
#include <tegrabl_partition_manager.h>
#include <arscratch.h>

typedef uint32_t smd_bin_copy_t;
#define SMD_COPY_PRIMARY 0U
#define SMD_COPY_SECONDARY 1U
#define MAX_SMD_COPY 2U

static uint32_t current_smd;
#define SMD_INVALID MAX_SMD_COPY

static void *smd_loadaddress;
static void *smd_backup;

static tegrabl_error_t
tegrabl_a_b_get_rootfs_retry_count(void *smd, uint8_t *rootfs_retry_count);

#if defined(CONFIG_ENABLE_DEBUG)
static void boot_chain_dump_slot_info(void *load_address)
{
	uint32_t i = 0;
	struct slot_meta_data *smd_info = (struct slot_meta_data *)load_address;

	pr_info("magic:0x%x, version: %d, num_slots: %d\n",
			smd_info->magic,
			smd_info->version,
			smd_info->num_slots);

	for (i = 0; i < MAX_SLOTS; i++) {
		pr_trace("slot: %d, pri: %d, suffix: %c%c, retry: %d, boot_succ: %d\n",
				 i, smd_info->slot_info[i].priority,
				 smd_info->slot_info[i].suffix[0],
				 smd_info->slot_info[i].suffix[1],
				 smd_info->slot_info[i].retry_count,
				 smd_info->slot_info[i].boot_successful);
	}
}
#endif

uint16_t tegrabl_a_b_get_version(void *smd)
{
	struct slot_meta_data *smd_info = (struct slot_meta_data *)smd;

	TEGRABL_ASSERT(smd != NULL);

	return smd_info->version;
}

void tegrabl_a_b_init(void *smd)
{
	struct slot_meta_data *smd_info = (struct slot_meta_data *)smd;

	TEGRABL_ASSERT(smd != NULL);

	/* Set default: A: bootable, B: unbootable */
	smd_info->slot_info[0].priority = 15;
	smd_info->slot_info[0].retry_count = 7;
	smd_info->slot_info[0].boot_successful = 1;
	(void) memcpy(smd_info->slot_info[0].suffix, (const char *)(BOOT_CHAIN_SUFFIX_A),
			BOOT_CHAIN_SUFFIX_LEN);

	smd_info->slot_info[1].priority = 0;
	smd_info->slot_info[1].retry_count = 0;
	smd_info->slot_info[1].boot_successful = 0;
	(void) memcpy(smd_info->slot_info[1].suffix, (const char *)(BOOT_CHAIN_SUFFIX_B),
			BOOT_CHAIN_SUFFIX_LEN);

	smd_info->magic = BOOT_CHAIN_MAGIC;
	smd_info->version = BOOT_CHAIN_VERSION;

	/*
	 * Simulate SMD but set max_slots to 1 so that device is handled as
	 * non-A/B system.
	 */
	smd_info->num_slots = 1;
}

static inline uint16_t tegrabl_a_b_get_max_num_slots(void *smd)
{
	struct slot_meta_data *smd_info = (struct slot_meta_data *)smd;

	TEGRABL_ASSERT(smd != NULL);

	return smd_info->num_slots;
}

tegrabl_error_t tegrabl_a_b_init_boot_slot_reg(void *smd)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t reg;
	uint32_t slot;

	reg = tegrabl_get_boot_slot_reg();

	/* In case not power on reset, scratch register may have been already
	 * initialized in previous boot, hence skip */
	if (BOOT_CHAIN_REG_MAGIC_GET(reg) == BOOT_CHAIN_REG_MAGIC) {
		goto done;
	}

	/* If reg magic is invalid, init reg with SMD and retain update flag */
	reg = 0;
	/* Set reg magic */
	reg = BOOT_CHAIN_REG_MAGIC_SET(reg);
	tegrabl_set_boot_slot_reg(reg);

	err = tegrabl_a_b_get_active_slot(smd, &slot);
	if (err != TEGRABL_NO_ERROR) {
		goto done;
	}

	tegrabl_a_b_save_boot_slot_reg(smd, slot);

done:
	return err;
}

tegrabl_error_t tegrabl_a_b_init_rootfs_slot_reg(void *smd)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t rf_reg;
	uint8_t retry_count = 0;
	uint16_t version;

	/* RF_SR is used when support redundancy user or rootfs A/B */
	version = tegrabl_a_b_get_version(smd);
	if (BOOTCTRL_SUPPORT_REDUNDANCY_USER(version) == 0U &&
		BOOTCTRL_SUPPORT_ROOTFS_AB(version) == 0U) {
		goto done;
	}

	rf_reg = tegrabl_get_rootfs_slot_reg();

	/* In case not power on reset, rf scratch register may have been already
	 * initialized in previous boot, hence skip */
	if (ROOTFS_AB_REG_MAGIC_GET(rf_reg) == ROOTFS_AB_REG_MAGIC) {
		goto done;
	}

	/* If rf reg magic is invalid, init rf reg with rotate_count = 0 */
	rf_reg = 0;
	/* Set reg magic */
	rf_reg = ROOTFS_AB_REG_MAGIC_SET(rf_reg);

	/* Copy rootfs retry_count if rootfs A/B without unified A/B. */
	if (BOOTCTRL_SUPPORT_ROOTFS_AB(version) &&
		BOOTCTRL_SUPPORT_UNIFIED_AB(version) == 0U) {
		/* Copy SMD rootfs retry_count to RF_SR */
		tegrabl_a_b_get_rootfs_retry_count(smd, &retry_count);
		rf_reg = ROOTFS_AB_REG_RETRY_COUNT_SET(retry_count, rf_reg);
	}

	tegrabl_set_rootfs_slot_reg(rf_reg);

done:
	return err;
}

static bool tegrabl_a_b_is_valid(void *smd)
{
	uint32_t reg;
	struct slot_meta_data *smd_info;

	if (smd == NULL) {
		/* check info stored in scratch register */
		reg = tegrabl_get_boot_slot_reg();

		return (bool)
				((BOOT_CHAIN_REG_MAGIC_GET(reg) == BOOT_CHAIN_REG_MAGIC) &&
				 (BOOT_CHAIN_REG_MAX_SLOTS_GET(reg) > 1U));
	}

	/* check info from smd buffer */
	smd_info = (struct slot_meta_data *)smd;
	return (bool)((smd_info->magic == BOOT_CHAIN_MAGIC) &&
				  (smd_info->num_slots > 1U));
}

static tegrabl_error_t boot_chain_get_bootslot_from_reg(uint32_t *active_slot)
{
	tegrabl_error_t err;
	uint32_t reg;

	reg = tegrabl_get_boot_slot_reg();

	if (!tegrabl_a_b_is_valid(NULL)) {
		pr_warn("No valid slot number is found in scratch register\n");
		pr_warn("Return default slot: %s\n", BOOT_CHAIN_SUFFIX_A);
		*active_slot = BOOT_SLOT_A;
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto done;
	}

	*active_slot = BOOT_CHAIN_REG_SLOT_NUM_GET(reg);
	err = TEGRABL_NO_ERROR;

done:
	return err;
}

static tegrabl_error_t boot_chain_get_bootslot_from_smd(void *smd,
							uint32_t *active_slot)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct slot_meta_data *bootctrl = (struct slot_meta_data *)smd;
	uint8_t max_priority;
	uint32_t i, slot;

	/* Check if the data is correct */
	if (bootctrl->magic != BOOT_CHAIN_MAGIC) {
		pr_error("SMD is corrupted!\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	/* Init priority as unbootable */
	max_priority = 0;

	/* Find slot with retry_count > 0 and highest priority */
	for (i = 0; i < bootctrl->num_slots; i++) {
		if ((bootctrl->slot_info[i].retry_count != 0U) &&
			(bootctrl->slot_info[i].priority != 0U)) {
			if (max_priority < bootctrl->slot_info[i].priority) {
				max_priority = bootctrl->slot_info[i].priority;
				slot = i;
			}
		}
	}

	/* Found a bootable slot? */
	if (max_priority == 0U) {
		pr_error("No bootable slot found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto done;
	}

	*active_slot = (uint32_t)slot;
	pr_trace("Active boot chain: %u\n", *active_slot);

	err = TEGRABL_NO_ERROR;

done:
	return err;
}

tegrabl_error_t tegrabl_a_b_get_active_slot(void *smd, uint32_t *active_slot)
{
	tegrabl_error_t err;

	/* Use slot number previous saved in scratch register */
	if (smd == NULL) {
		err = boot_chain_get_bootslot_from_reg(active_slot);
		goto done;
	}

	/* Run a/b slot selection logic to find out active slot */
	err = boot_chain_get_bootslot_from_smd(smd, active_slot);

done:
	return err;
}

void tegrabl_a_b_set_retry_count_reg(uint32_t slot, uint8_t retry_count)
{
	uint32_t reg;

	reg = tegrabl_get_boot_slot_reg();
	switch (slot) {
	case BOOT_SLOT_A:
		reg = BOOT_CHAIN_REG_A_RETRY_COUNT_SET(retry_count, reg);
		break;
	case BOOT_SLOT_B:
		reg = BOOT_CHAIN_REG_B_RETRY_COUNT_SET(retry_count, reg);
		break;
	default:
		break;
	}
	tegrabl_set_boot_slot_reg(reg);
}

static uint8_t tegrabl_a_b_get_retry_count_reg(uint32_t slot, uint32_t reg)
{
	uint8_t retry_count = 0;

	switch (slot) {
	case BOOT_SLOT_A:
		retry_count = (uint8_t)BOOT_CHAIN_REG_A_RETRY_COUNT_GET(reg);
		break;
	case BOOT_SLOT_B:
		retry_count = (uint8_t)BOOT_CHAIN_REG_B_RETRY_COUNT_GET(reg);
		break;
	default:
		TEGRABL_ASSERT(0);
		break;
	}
	return retry_count;
}

void tegrabl_a_b_copy_retry_count(void *smd, uint32_t *reg, uint32_t direct)
{
	struct slot_meta_data *bootctrl = (struct slot_meta_data *)smd;
	uint8_t retry_count;

	TEGRABL_ASSERT(smd != NULL);

	switch (direct) {
	case FROM_REG_TO_SMD:
		retry_count = (uint8_t)BOOT_CHAIN_REG_A_RETRY_COUNT_GET(*reg);
		bootctrl->slot_info[BOOT_SLOT_A].retry_count = retry_count;
		retry_count = (uint8_t)BOOT_CHAIN_REG_B_RETRY_COUNT_GET(*reg);
		bootctrl->slot_info[BOOT_SLOT_B].retry_count = retry_count;
		break;
	case FROM_SMD_TO_REG:
		retry_count = bootctrl->slot_info[BOOT_SLOT_A].retry_count;
		*reg = BOOT_CHAIN_REG_A_RETRY_COUNT_SET(retry_count, *reg);
		retry_count = bootctrl->slot_info[BOOT_SLOT_B].retry_count;
		*reg = BOOT_CHAIN_REG_B_RETRY_COUNT_SET(retry_count, *reg);
		break;
	default:
		break;
	}
}

void tegrabl_a_b_save_boot_slot_reg(void *smd, uint32_t slot)
{
	uint32_t reg;
	struct slot_meta_data *bootctrl = (struct slot_meta_data *)smd;

	TEGRABL_ASSERT(smd != NULL);

	reg = tegrabl_get_boot_slot_reg();

	/* Set slot number */
	reg = BOOT_CHAIN_REG_SLOT_NUM_SET(slot, reg);

	/* Set max slots */
	reg = BOOT_CHAIN_REG_MAX_SLOTS_SET(tegrabl_a_b_get_max_num_slots(smd), reg);

	/* Set retry counts */
	tegrabl_a_b_copy_retry_count(smd, &reg, FROM_SMD_TO_REG);

	/* Set update flag if current boot slot's boot_succ flag is 0 */
	if (bootctrl->slot_info[slot].boot_successful == 0U) {
		reg = BOOT_CHAIN_REG_UPDATE_FLAG_SET(BC_FLAG_OTA_ON, reg);
	} else {
		/* check SMD version and set REDUNDANCY flag if it is supported */
		if (BOOTCTRL_SUPPORT_REDUNDANCY(bootctrl->version) != 0U) {
			reg = BOOT_CHAIN_REG_UPDATE_FLAG_SET(BC_FLAG_REDUNDANCY_BOOT, reg);
		}
	}

	tegrabl_set_boot_slot_reg(reg);
}

tegrabl_error_t tegrabl_a_b_check_and_update_retry_count(void *smd,
														 uint32_t slot)
{
	struct slot_meta_data *bootctrl = (struct slot_meta_data *)smd;

	TEGRABL_ASSERT(smd != NULL);

	/*
	 * Decrement retry count if
	 * a. REDUNDNACY is supported, or
	 * b. Current slot state is unsuccessful
	 */

	if ((BOOTCTRL_SUPPORT_REDUNDANCY(bootctrl->version) != 0U) ||
		(bootctrl->slot_info[slot].boot_successful == 0U)) {
		TEGRABL_ASSERT(bootctrl->slot_info[slot].retry_count != 0U);
		bootctrl->slot_info[slot].retry_count--;
	}

#if defined(CONFIG_ENABLE_DEBUG)
	boot_chain_dump_slot_info(bootctrl);
#endif

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_get_bootslot_suffix(char *suffix, bool full_suffix)
{
	tegrabl_error_t err;
	uint32_t slot;

	err = tegrabl_a_b_get_active_slot(NULL, &slot);
	if (err != TEGRABL_NO_ERROR) {
		if (TEGRABL_ERROR_REASON(err) == TEGRABL_ERR_NOT_FOUND) {
			err = TEGRABL_NO_ERROR;
		} else {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			goto fail;
		}
	}

	if ((full_suffix == false) &&  (slot == BOOT_SLOT_A)) {
		*suffix = '\0';
		goto done;
	}

	if (slot == BOOT_SLOT_A) {
		strncpy(suffix, BOOT_CHAIN_SUFFIX_A, BOOT_CHAIN_SUFFIX_LEN);
	} else {
		strncpy(suffix, BOOT_CHAIN_SUFFIX_B, BOOT_CHAIN_SUFFIX_LEN);
	}
	*(suffix + BOOT_CHAIN_SUFFIX_LEN) = '\0';

done:
	pr_info("Active slot suffix: %s\n", suffix);

fail:
	return err;
}

tegrabl_error_t tegrabl_a_b_set_bootslot_suffix(uint32_t slot, char *partition,
												bool full_suffix)
{
	const char *suffix;

	/*
	 * To be compatible with legacy partition names, set slot A's suffix to
	 * none, ie, no suffix for slot A, if full_suffix is not needed
	 */
	if ((full_suffix == false) && (slot == BOOT_SLOT_A)) {
		goto done;
	}

	if (slot == BOOT_SLOT_A) {
		suffix = BOOT_CHAIN_SUFFIX_A;
	} else {
		suffix = BOOT_CHAIN_SUFFIX_B;
	}
	strcat(partition, suffix);

done:
	pr_debug("Select partition: %s\n", partition);
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_get_slot_via_suffix(const char *suffix,
												uint32_t *boot_slot_id)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((*suffix == '\0') || (*suffix == 'a') || (strcmp(suffix, BOOT_CHAIN_SUFFIX_A) == 0)) {
		*boot_slot_id = (uint32_t)BOOT_SLOT_A;
	} else if ((*suffix == 'b') || (strcmp(suffix, BOOT_CHAIN_SUFFIX_B) == 0)) {
		*boot_slot_id = (uint32_t)BOOT_SLOT_B;
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return error;
}

tegrabl_error_t tegrabl_a_b_get_slot_num(void *smd_addr, uint8_t *num_slots)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (num_slots == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	*num_slots = (uint8_t)smd->num_slots;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_get_successful(void *smd_addr, uint32_t slot,
										   uint8_t *successful)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= (uint32_t)MAX_SLOTS) || (successful == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	*successful = smd->slot_info[slot].boot_successful;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_get_retry_count(void *smd_addr, uint32_t slot,
											uint8_t *retry_count)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= (uint32_t)MAX_SLOTS) || (retry_count == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	*retry_count = smd->slot_info[slot].retry_count;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_get_priority(void *smd_addr, uint32_t slot,
										 uint8_t *priority)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS) || (priority == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	*priority = smd->slot_info[slot].priority;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_is_unbootable(void *smd_addr, uint32_t slot,
										  bool *is_unbootable)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct slot_meta_data *smd;
	uint8_t priority, retry_count;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS) || (is_unbootable == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	smd = (struct slot_meta_data *)smd_addr;

	error = tegrabl_a_b_get_priority((void *)smd, slot, &priority);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	error = tegrabl_a_b_get_retry_count((void *)smd, slot, &retry_count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	*is_unbootable = ((priority == 0U) || (retry_count == 0U)) ? true : false;

done:
	return error;
}

tegrabl_error_t tegrabl_a_b_set_successful(void *smd_addr, uint32_t slot,
										   uint8_t successful)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	smd->slot_info[slot].boot_successful = successful;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_set_retry_count(void *smd_addr, uint32_t slot,
											uint8_t retry_count)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= MAX_SLOTS)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	smd->slot_info[slot].retry_count = retry_count;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_a_b_set_priority(void *smd_addr, uint32_t slot,
										 uint8_t priority)
{
	struct slot_meta_data *smd;

	if ((smd_addr == NULL) || (slot >= (uint32_t)MAX_SLOTS)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd = (struct slot_meta_data *)smd_addr;
	smd->slot_info[slot].priority = priority;

	return TEGRABL_NO_ERROR;
}

static int tegrabl_get_max_retry_count(void *smd)
{
	struct slot_meta_data_v2 *smd_v2;
	uint16_t version;
	uint8_t max_bl_retry_count;

	version = tegrabl_a_b_get_version(smd);
	/* Set the maximum slot retry count to default if smd
	 * extension is not support
	 */
	if (BOOT_CHAIN_VERSION_GET(version) < BOOT_CHAIN_VERSION_ROOTFS_AB) {
		max_bl_retry_count = SLOT_RETRY_COUNT_DEFAULT;
	} else {
		smd_v2 = (struct slot_meta_data_v2 *)smd;
		max_bl_retry_count = smd_v2->smd_ext.max_bl_retry_count;
	}

	if (max_bl_retry_count == 0 || max_bl_retry_count > SLOT_RETRY_COUNT_DEFAULT) {
		max_bl_retry_count = SLOT_RETRY_COUNT_DEFAULT;
		pr_error("Invalid max retry count, set to the default value(%d)\n",
			max_bl_retry_count);
	}

	return max_bl_retry_count;
}

static uint8_t get_max_rotate_count(void *smd)
{
	uint8_t max_bl_retry_count;
	uint8_t max_rotate_count = ROOTFS_AB_ROTATE_COUNT;

	max_bl_retry_count = tegrabl_get_max_retry_count(smd);

	/*
	 * If max_bl_retry_count is 1, then max rotate_count shoud be 2;
	 * if max_bl_retry_count is larger than 1, then max rotate_count is 4.
	 */
	if (max_bl_retry_count * 2 < ROOTFS_AB_ROTATE_COUNT) {
		max_rotate_count = max_bl_retry_count * 2;
	}

	return max_rotate_count;
}

tegrabl_error_t tegrabl_a_b_set_active_slot(void *smd_addr, uint32_t slot_id)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct slot_meta_data *smd;
	uint32_t i;

	if ((smd_addr == NULL) || (slot_id >= MAX_SLOTS)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	smd = (struct slot_meta_data *)smd_addr;

	/* Decrease all slot's priority by 1 */
	for (i = 0; i < MAX_SLOTS; i++) {
		if (smd->slot_info[i].priority > 1U) {
			smd->slot_info[i].priority -= 1U;
		}
	}

	/* Reset slot info of given slot to default */
	error = tegrabl_a_b_set_priority((void *)smd, slot_id,
									 SLOT_PRIORITY_DEFAULT);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}
	error = tegrabl_a_b_set_retry_count((void *)smd, slot_id, tegrabl_get_max_retry_count((void *)smd));
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	error = tegrabl_a_b_set_successful((void *)smd, slot_id, 0);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}
done:
	return error;
}

tegrabl_error_t tegrabl_a_b_get_current_rootfs_id(void *smd, uint8_t *rootfs_id)
{
	struct slot_meta_data_v2 *smd_v2;
	uint8_t rootfs_select;
	uint16_t version;
	uint32_t bl_slot;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (rootfs_id == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (smd == NULL) {
		error = tegrabl_a_b_get_smd((void **)&smd);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			return error;
		}
	}

	version = tegrabl_a_b_get_version(smd);
	if (BOOTCTRL_SUPPORT_ROOTFS_AB(version) == 0U) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/*
	 * "Unified bl&rf a/b" is supported from version
	 * BOOT_CHAIN_VERSION_UNIFY_RF_BL_AB. If supported and enabled,
	 * use bootloader active slot for rootfs.
	 */
	if (BOOTCTRL_IS_UNIFIED_AB_ENABLED(version)) {
		error = tegrabl_a_b_get_active_slot(smd, &bl_slot);
		if (error != TEGRABL_NO_ERROR) {
			return error;
		}

		*rootfs_id = (uint8_t)bl_slot;
		return error;
	}
	smd_v2 = (struct slot_meta_data_v2 *)smd;
	rootfs_select = ROOTFS_SELECT(smd_v2);
	*rootfs_id = GET_ROOTFS_ACTIVE(rootfs_select);

	return TEGRABL_NO_ERROR;
}

static int tegrabl_get_rootfs_max_retry_count(void *smd)
{
	struct slot_meta_data_v2 *smd_v2;
	uint16_t version;
	uint8_t max_rf_retry_count;
	uint8_t rf_misc_info;

	version = tegrabl_a_b_get_version(smd);
	/* Set the maximum rootfs slot retry count to default if
	 * rootfs max retry count is not support in smd
	 */
	if (BOOT_CHAIN_VERSION_GET(version) < BOOT_CHAIN_VERSION_UNIFY_RF_BL_AB) {
		max_rf_retry_count = ROOTFS_RETRY_COUNT_DEFAULT;
	} else {
		smd_v2 = (struct slot_meta_data_v2 *)smd;
		rf_misc_info = smd_v2->smd_ext.features.rootfs_misc_info;
		max_rf_retry_count = GET_MAX_ROOTFS_RETRY_COUNT(rf_misc_info);
	}

	if ((max_rf_retry_count > ROOTFS_RETRY_COUNT_DEFAULT) ||
		(max_rf_retry_count == 0)) {
		max_rf_retry_count = ROOTFS_RETRY_COUNT_DEFAULT;
		pr_error("Invalid max rootfs retry count, set to the default value(%d)\n",
				 max_rf_retry_count);
	}

	return max_rf_retry_count;
}

static tegrabl_error_t
tegrabl_a_b_set_active_rootfs(void *smd, uint8_t rootfs_id)
{
	struct slot_meta_data_v2 *smd_v2;
	uint8_t *rootfs_select;
	uint8_t val;

	if (smd == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if ((rootfs_id != ROOTFS_A) && (rootfs_id != ROOTFS_B) &&
	    (rootfs_id != ROOTFS_INVALID)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd_v2 = (struct slot_meta_data_v2 *)smd;
	rootfs_select = &(ROOTFS_SELECT(smd_v2));
	val = SET_ROOTFS_ACTIVE(rootfs_id, *rootfs_select);
	*rootfs_select = val;
	val = SET_ROOTFS_RETRY_COUNT(tegrabl_get_rootfs_max_retry_count(smd),
			*rootfs_select);
	*rootfs_select = val;

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t
tegrabl_a_b_get_rootfs_retry_count(void *smd, uint8_t *rootfs_retry_count)
{
	struct slot_meta_data_v2 *smd_v2;
	uint8_t rootfs_select;

	if ((smd == NULL) || (rootfs_retry_count == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd_v2 = (struct slot_meta_data_v2 *)smd;
	rootfs_select = ROOTFS_SELECT(smd_v2);
	*rootfs_retry_count = GET_ROOTFS_RETRY_COUNT(rootfs_select);

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t
tegrabl_a_b_set_rootfs_retry_count(void *smd, uint8_t rootfs_retry_count)
{
	struct slot_meta_data_v2 *smd_v2;
	uint8_t *rootfs_select;
	uint8_t val;

	if (smd == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (rootfs_retry_count > tegrabl_get_rootfs_max_retry_count(smd)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd_v2 = (struct slot_meta_data_v2 *)smd;
	rootfs_select = &(ROOTFS_SELECT(smd_v2));
	val = SET_ROOTFS_RETRY_COUNT(rootfs_retry_count, *rootfs_select);
	*rootfs_select = val;

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t
tegrabl_a_b_get_rootfs_status(void *smd, uint32_t slot_id,
				uint8_t *rootfs_status)
{
	struct slot_meta_data_v2 *smd_v2;

	if ((smd == NULL) || (slot_id >= MAX_SLOTS) ||
	    (rootfs_status == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd_v2 = (struct slot_meta_data_v2 *)smd;
	*rootfs_status = ROOTFS_STATUS(smd_v2, slot_id);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t
tegrabl_a_b_set_rootfs_status(void *smd, uint32_t slot_id,
				uint8_t rootfs_status)
{
	struct slot_meta_data_v2 *smd_v2;

	if ((smd == NULL) || (slot_id >= MAX_SLOTS)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (rootfs_status > ROOTFS_STATUS_END) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	smd_v2 = (struct slot_meta_data_v2 *)smd;
	ROOTFS_STATUS(smd_v2, slot_id) = rootfs_status;

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t tegrabl_a_b_select_active_rootfs(void *smd)
{
	struct slot_meta_data_v2 *smd_v2;
	uint8_t rootfs_select, retry_count, rfs_id;
	uint8_t rfs_status = ROOTFS_STATUS_NORMAL;
	uint32_t rf_reg;
	uint16_t version;

	if (smd == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	/*
	 * Do not select active rootfs if rootfs A/B is disabled,
	 * or unified A/B is enabled.
	 */
	version = tegrabl_a_b_get_version(smd);
	if (BOOTCTRL_SUPPORT_ROOTFS_AB(version) == 0U ||
		BOOTCTRL_IS_UNIFIED_AB_ENABLED(version) != 0U) {
		goto done;
	}

	/*
	 * Use retry_count in RF_SR, if rootfs retry_count is decreased to 0,
	 * update the rootfs status and rfs_id in SMD buffer
	 */
	smd_v2 = (struct slot_meta_data_v2 *)smd;
	rootfs_select = ROOTFS_SELECT(smd_v2);
	rfs_id = GET_ROOTFS_ACTIVE(rootfs_select);

	rf_reg = tegrabl_get_rootfs_slot_reg();
	retry_count = ROOTFS_AB_REG_RETRY_COUNT_GET(rf_reg);

	if (retry_count == 0) {
		/* Set current slot as BOOT_FAILED and retry count to 0 */
		tegrabl_a_b_set_rootfs_status(smd, rfs_id,
					ROOTFS_STATUS_BOOT_FAILED);

		/* Check next slot rootfs status */
		tegrabl_a_b_get_rootfs_status(smd, !rfs_id, &rfs_status);
		if (rfs_status == ROOTFS_STATUS_NORMAL) {
			/* Switch active rootfs to next slot */
			tegrabl_a_b_set_active_rootfs(smd, !rfs_id);
			/* Get restored rootfs retry_count again */
			tegrabl_a_b_get_rootfs_retry_count(smd, &retry_count);
		} else {
			tegrabl_a_b_set_active_rootfs(smd, ROOTFS_INVALID);
			goto done;
		}
	}

	/*
	 * Just decrease rootfs retry_count in RF_SR, UE will clear RF_SR,
	 * if failed to reach UE, retry_count will be decreased by 1
	 */
	rf_reg = ROOTFS_AB_REG_RETRY_COUNT_SET(retry_count - 1, rf_reg);
	tegrabl_set_rootfs_slot_reg(rf_reg);

done:
	return TEGRABL_NO_ERROR;
}

bool tegrabl_a_b_rootfs_is_all_unbootable(void *smd)
{
	tegrabl_error_t err;
	uint8_t rootfs_id;
	uint16_t version;
	uint8_t rotate_count = 0;
	uint8_t max_rotate_count;
	uint32_t rf_reg;

	if (smd == NULL) {
		err = tegrabl_a_b_get_smd((void **)&smd);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			return false;
		}
	}

	version = tegrabl_a_b_get_version(smd);
	/*
	 * "Unified bl&rf a/b" is supported from version
	 * BOOT_CHAIN_VERSION_UNIFY_RF_BL_AB. If supported and enabled,
	 * use bootloader bootable status for rootfs.
	 * Since bootloader is already running, check rotate_count for
	 * u-boot/kernel. If rotate_count is no less than maximum value,
	 * boot to recovery kernel, else boot to normal kernel.
	 */
	if (BOOTCTRL_IS_UNIFIED_AB_ENABLED(version) ||
		BOOTCTRL_SUPPORT_REDUNDANCY_USER(version)) {
		rf_reg = tegrabl_get_rootfs_slot_reg();
		/*
		 * The magic field of rootfs SR must match with ROOTFS_AB_REG_MAGIC,
		 * since it has been initialized in tegrabl_a_b_init_rootfs_slot_reg()
		 * if not match.
		 */
		TEGRABL_ASSERT(ROOTFS_AB_REG_MAGIC_GET(rf_reg) == ROOTFS_AB_REG_MAGIC);

		/*
		 * If rotate_count is no less than max_rotate_count,
		 * clear the rotate_count in scratch register
		 * and return true to boot to recovery kernel.
		 */
		rotate_count = ROOTFS_AB_REG_ROTATE_COUNT_GET(rf_reg);
		max_rotate_count = get_max_rotate_count(smd);
		if (rotate_count >= max_rotate_count) {
			rotate_count = 0;
			rf_reg = ROOTFS_AB_REG_ROTATE_COUNT_SET(rotate_count, rf_reg);
			tegrabl_set_rootfs_slot_reg(rf_reg);

			return true;
		}
		return false;
	}

	err = tegrabl_a_b_get_current_rootfs_id(smd, &rootfs_id);
	if (err != TEGRABL_NO_ERROR) {
		/* rootfs AB is not enabled, return false. */
		return false;
	}

	if (rootfs_id == ROOTFS_INVALID)
		return true;
	else
		return false;
}

tegrabl_error_t tegrabl_a_b_get_rootfs_suffix(char *suffix, bool full_suffix)
{
	tegrabl_error_t err;
	uint8_t rootfs_id;

	err = tegrabl_a_b_get_current_rootfs_id(NULL, &rootfs_id);
	if (err != TEGRABL_NO_ERROR) {
		/* rootfs AB is not enabled, ROOTFS_A is the default id. */
		rootfs_id = ROOTFS_A;
		err = TEGRABL_NO_ERROR;
	}

	/*
	 * rootfs A: "_a" or "",
	 * rootfs B: "_b",
	 * no bootable rootfs: return error.
	 */
	if ((full_suffix == false) && (rootfs_id == ROOTFS_A)) {
		*suffix = '\0';
		goto done;
	}

	if (rootfs_id == ROOTFS_A) {
		strncpy(suffix, BOOT_CHAIN_SUFFIX_A, BOOT_CHAIN_SUFFIX_LEN);
	} else if (rootfs_id == ROOTFS_B) {
		strncpy(suffix, BOOT_CHAIN_SUFFIX_B, BOOT_CHAIN_SUFFIX_LEN);
	} else {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}
	*(suffix + BOOT_CHAIN_SUFFIX_LEN) = '\0';

done:
	pr_info("Active rootfs suffix: %s\n", suffix);
	return err;
}

static tegrabl_error_t load_smd_bin_copy(smd_bin_copy_t bin_copy)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct slot_meta_data *smd = NULL;
	struct slot_meta_data_v2 *smd_v2 = NULL;
	struct tegrabl_partition part;
	char *smd_part;
	uint32_t crc32;
	uint32_t smd_v2_len = sizeof(struct slot_meta_data_v2);
	uint32_t smd_len = sizeof(struct slot_meta_data);
	uint32_t smd_ext_len = sizeof(struct slot_meta_data_ext);
	uint16_t version;

	current_smd = SMD_INVALID;

	smd_part = (bin_copy == SMD_COPY_PRIMARY) ? "SMD" : "SMD_b";
	error = tegrabl_partition_open(smd_part, &part);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	/* Always move to first byte */
	error = tegrabl_partition_seek(&part, 0, TEGRABL_PARTITION_SEEK_SET);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	error = tegrabl_partition_read(&part, smd_loadaddress, smd_v2_len);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}
	tegrabl_partition_close(&part);

	smd = (struct slot_meta_data *)smd_loadaddress;

	version = tegrabl_a_b_get_version(smd);
	if (BOOT_CHAIN_VERSION_GET(version) == BOOT_CHAIN_VERSION_ONE) {
		/* VERSION_ONE: only check magic id for SMD sanity */
		if (smd->magic != BOOT_CHAIN_MAGIC) {
			error = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
			pr_error("%s corrupt with incorrect magic id\n",
				 smd_part);
			goto done;
		}
	} else {
		/* Other VERSIONs: always check crc for SMD sanity */
		crc32 = tegrabl_utils_crc32(0, smd_loadaddress,
					    smd_len - sizeof(crc32));
		if (crc32 != smd->crc32) {
			error = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
			pr_error("%s corrupt with incorrect crc\n", smd_part);
			goto done;
		}
	}

	/* Check crc for SMD extension sanity */
	if (BOOT_CHAIN_VERSION_GET(version) >= BOOT_CHAIN_VERSION_ROOTFS_AB) {
		smd_v2 = (struct slot_meta_data_v2 *)smd_loadaddress;
		crc32 = tegrabl_utils_crc32(0, &smd_v2->smd_ext.crc32_len,
					    smd_ext_len - sizeof(crc32));
		if (crc32 != smd_v2->smd_ext.crc32) {
			error = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
			pr_error("%s corrupt with incorrect crc in extension\n",
				 smd_part);
			goto done;
		}
	}

	current_smd = bin_copy;

done:
	return error;
}

tegrabl_error_t tegrabl_a_b_get_smd(void **smd)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t smd_len;

	TEGRABL_ASSERT(smd != NULL);

	/* Return smd address directly if it has already been loaded */
	if (smd_loadaddress != NULL) {
		*smd = smd_loadaddress;
		goto done;
	}

	smd_len = sizeof(struct slot_meta_data_v2);
	smd_loadaddress = tegrabl_malloc(smd_len);
	if (smd_loadaddress == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto done;
	}

	smd_backup = tegrabl_malloc(smd_len);
	if (smd_backup == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		tegrabl_free(smd_loadaddress);
		smd_loadaddress = NULL;
		goto done;
	}

	/* Load and verify SMD primary copy */
	error = load_smd_bin_copy(SMD_COPY_PRIMARY);
	if (error == TEGRABL_NO_ERROR) {
		*smd = smd_loadaddress;
		goto init;
	}

	/* If SMD primary copy has corrupted, fallback to SMD secondary copy */
	error = load_smd_bin_copy(SMD_COPY_SECONDARY);
	if (error == TEGRABL_NO_ERROR) {
		*smd = smd_loadaddress;
		goto init;
	}

	tegrabl_free(smd_loadaddress);
	smd_loadaddress = NULL;

	tegrabl_free(smd_backup);
	smd_backup = NULL;
	goto done;

init:
	/*
	 * Backup current SMD buffer to smd_backup, compare the contents
	 * of smd_backup and smd_loadaddress before leaving Cboot, if they
	 * are different, flush SMD buffer to SMD partition.
	 */
	memcpy(smd_backup, smd_loadaddress, smd_len);

	/*
	 * Initialize the rootfs scratch register
	 * if unified A/B or Redundancy user or rootfs A/B is enabled.
	 */
	tegrabl_a_b_init_rootfs_slot_reg(*smd);

	/*
	 * Select active rootfs if unified A/B is disabled,
	 * and rootfs A/B is enabled.
	 */
	tegrabl_a_b_select_active_rootfs(*smd);

done:
	return error;
}

static tegrabl_error_t flush_smd_bin_copy(void *smd, smd_bin_copy_t bin_copy)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_partition part;
	char *smd_part;

	smd_part = (bin_copy == SMD_COPY_PRIMARY) ? "SMD" : "SMD_b";

	/* Write SMD back to storage */
	error = tegrabl_partition_open(smd_part, &part);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

#if defined(CONFIG_ENABLE_QSPI)
	uint32_t storage_type;
	/* Erase SMD since QSPI storage needs to be erased before writing */
	storage_type = tegrabl_blockdev_get_storage_type(part.block_device);
	if (storage_type == TEGRABL_STORAGE_QSPI_FLASH) {
		error = tegrabl_partition_erase(&part, false);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto done;
		}
	}
#endif

	/* Always move to first byte */
	error = tegrabl_partition_seek(&part, 0, TEGRABL_PARTITION_SEEK_SET);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	error = tegrabl_partition_write(&part, smd,
					sizeof(struct slot_meta_data_v2));
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}

	tegrabl_partition_close(&part);
done:
	return error;
}

tegrabl_error_t tegrabl_a_b_flush_smd(void *smd)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct slot_meta_data *bootctrl = (struct slot_meta_data *)smd;
	struct slot_meta_data_v2 *smd_v2;
	uint32_t smd_payload_len;
	smd_bin_copy_t bin_copy;

	if (smd == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	/* Update crc field before writing */
	smd_payload_len = sizeof(struct slot_meta_data) - sizeof(uint32_t);
	bootctrl->crc32 = tegrabl_utils_crc32(0, smd, smd_payload_len);

	/* Update crc for SMD extension sanity */
	if (BOOT_CHAIN_VERSION_GET(tegrabl_a_b_get_version(smd)) >=
				BOOT_CHAIN_VERSION_ROOTFS_AB) {
		smd_v2 = (struct slot_meta_data_v2 *)smd;
		smd_payload_len = sizeof(struct slot_meta_data_ext)
				  - sizeof(uint32_t);
		smd_v2->smd_ext.crc32 = tegrabl_utils_crc32(0,
						&smd_v2->smd_ext.crc32_len,
						smd_payload_len);
	}

	/*
	 * Always flush both primary SMD and secondary SMD.
	 * However, must start with the non-current copy to prevent both
	 * copies from corrupted.
	 */
	bin_copy = (current_smd == SMD_COPY_PRIMARY) ? SMD_COPY_SECONDARY : SMD_COPY_PRIMARY;
	error = flush_smd_bin_copy(smd, bin_copy);
	if (error != TEGRABL_NO_ERROR)
		goto done;
	bin_copy = (current_smd == SMD_COPY_PRIMARY) ? SMD_COPY_PRIMARY : SMD_COPY_SECONDARY;
	error = flush_smd_bin_copy(smd, bin_copy);

done:
	return error;
}

/*
 * For REDUNDANCY BL only, there is no need to update SMD except
 *    when boot slot is switched on this boot. For such case, we need
 *    save the latest retry count from SR to SMD.
 */
static tegrabl_error_t check_non_user_redundancy_status(uint32_t reg,
		struct slot_meta_data *smd)
{
	uint8_t retry_count;
	uint32_t current_slot;

	/* Restore retry_count to max_bl_retry_count for REDUNDANCY BL only. */
	current_slot = BOOT_CHAIN_REG_SLOT_NUM_GET(reg);
	retry_count = tegrabl_get_max_retry_count(smd);
	tegrabl_a_b_set_retry_count_reg(current_slot, retry_count);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t check_rootfs_ab_status(uint32_t reg,
		struct slot_meta_data *smd)
{
	uint8_t retry_count;
	uint32_t current_slot;

	/*
	 * Restore bootloader retry_count to max_bl_retry_count
	 */
	current_slot = BOOT_CHAIN_REG_SLOT_NUM_GET(reg);
	retry_count = tegrabl_get_max_retry_count(smd);
	tegrabl_a_b_set_retry_count_reg(current_slot, retry_count);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t check_user_redundancy_status(uint32_t bl_reg,
		struct slot_meta_data *smd)
{
	tegrabl_error_t error;
	uint8_t retry_count;
	uint8_t slot1_priority, slot2_priority;
	uint32_t current_slot;
	uint8_t rotate_count = 0;
	uint8_t max_rotate_count;
	uint32_t rf_reg;

	/*
	 * Restore retry_count to max_bl_retry_count
	 */
	current_slot = BOOT_CHAIN_REG_SLOT_NUM_GET(bl_reg);
	retry_count = tegrabl_get_max_retry_count(smd);
	tegrabl_a_b_set_retry_count_reg(current_slot, retry_count);
	bl_reg = tegrabl_get_boot_slot_reg();

	/*
	 * When "unified bl&rf a/b" or Redundacny User is enabled:
	 * For any boot failure at this stage (u-boot or kernel), the policy
	 * is to try up to two times on a slot by rotate_count in RF_SR.
	 * When expired, switch slot and try again.
	 *
	 * Use rotate_count in scratch register(RF_SR) to record the try times,
	 * if control reaches kernel, UE restores the rotate_count in RF_SR;
	 * if not, rotate_count will increase by 1 on each try.
	 * If both slots have tried max_rotate_count/2 times, boot to recovery.
	 *
	 * When rotate_count reaches max_rotate_count/2 or max_rotate_count,
	 * if both slots are bootable, switch slot by changing priority;
	 *
	 * If control reaches kernel, UE restores slots priorities.
	 * If not, a/b logic at MB1 on next boot may switch boot slot based on
	 * slots priorities.
	 */
	error = tegrabl_a_b_get_priority(smd, current_slot,
				&slot1_priority);
	if (error != TEGRABL_NO_ERROR) {
		goto done;
	}
	error = tegrabl_a_b_get_priority(smd, !current_slot,
				&slot2_priority);
	if (error != TEGRABL_NO_ERROR) {
		goto done;
	}

	rf_reg = tegrabl_get_rootfs_slot_reg();
	/*
	 * The magic field of rootfs SR must match with ROOTFS_AB_REG_MAGIC,
	 * since it has been initialized in tegrabl_a_b_init_rootfs_slot_reg()
	 * if not match.
	 */
	TEGRABL_ASSERT(ROOTFS_AB_REG_MAGIC_GET(rf_reg) == ROOTFS_AB_REG_MAGIC);

	/* Increase rotate_count by 1 */
	rotate_count = ROOTFS_AB_REG_ROTATE_COUNT_GET(rf_reg);
	rotate_count++;

	/*
	 * When rotate_count reaches maximum(max_rotate_count/2) for one slot,
	 * or when rotate_count reaches maximum(max_rotate_count) for both slots,
	 * if both slots are bootable, switch bootloader slot.
	 */
	max_rotate_count = get_max_rotate_count(smd);
	if ((rotate_count == max_rotate_count / 2) ||
		(rotate_count == max_rotate_count)) {
		/* Switch bootloader slot when both slots are bootable */
		if (tegrabl_a_b_get_retry_count_reg(!current_slot, bl_reg) &&
			slot1_priority && slot2_priority) {
			/* current slot priority must be greater or equal than
			 * non-current slot */
			TEGRABL_ASSERT(slot1_priority >= slot2_priority);

			/* Switch to non-current slot by changing priority */
			tegrabl_a_b_set_priority(smd, current_slot, 14);
			tegrabl_a_b_set_priority(smd, !current_slot, 15);
		}
	}

	/* Save rotate_count to RF_SR */
	rf_reg = ROOTFS_AB_REG_ROTATE_COUNT_SET(rotate_count, rf_reg);
	tegrabl_set_rootfs_slot_reg(rf_reg);

done:
	return error;
}

static tegrabl_error_t check_redundancy_status(uint32_t reg,
		struct slot_meta_data *smd)
{
	tegrabl_error_t error;
	uint16_t version;

	/*
	 * For REDUNDANCY BL only, there is no need to update SMD except
	 *    when boot slot is switched on this boot. For such case, we need
	 *    save the latest retry count from SR to SMD.
	 *
	 * For REDUNDANCY USER, ie, redundancy is supported for cboot's payload
	 *    such as u-boot and kernel. For such case, since BL is already
	 *    successfully booted to cboot, we should restore retry count. We
	 *    use rotate_count in scratch register(RF_SR) to record the try
	 *    times. If control reaches kernel, UE restores the rotate_count
	 *    in RF_SR; if not, rotate_count will increase by 1 on each try.
	 *    If both slots have tried max_rotate_count/2 times, boot to recovery.
	 *
	 *    When rotate_count reaches max_rotate_count/2 or max_rotate_count,
	 *    if both slots are bootable, switch slot by changing priority;
	 *    By changing slot priority values, A/B logic at mb1 can switch
	 *    boot slot when current boot slot's priority is lower than the
	 *    other slot.
	 */
	version = tegrabl_a_b_get_version(smd);
	if ((BOOTCTRL_SUPPORT_REDUNDANCY_USER(version) == 0U) &&
	    (BOOTCTRL_SUPPORT_ROOTFS_AB(version) == 0U)) {
		/* REDUNDANCY is supported at bootloader only */
		error = check_non_user_redundancy_status(reg, smd);
	} else if (BOOTCTRL_SUPPORT_REDUNDANCY_USER(version) ||
	    BOOTCTRL_IS_UNIFIED_AB_ENABLED(version)) {
		/*
		 * REDUNDANCY is supported at kernel (or u-boot)
		 * or unified bl&rf a/b is enabled.
		 */
		error = check_user_redundancy_status(reg, smd);
	} else if (BOOTCTRL_SUPPORT_ROOTFS_AB(version) != 0U) {
		error = check_rootfs_ab_status(reg, smd);
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return error;
}

tegrabl_error_t tegrabl_a_b_update_smd(void)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct slot_meta_data *smd = NULL;
	uint32_t reg;
	uint8_t bc_flag;
	uint16_t version;

	reg = tegrabl_get_boot_slot_reg();
	bc_flag = (uint8_t)BOOT_CHAIN_REG_UPDATE_FLAG_GET(reg);
	error = tegrabl_a_b_get_smd((void **)&smd);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto done;
	}
	version = tegrabl_a_b_get_version(smd);
	if ((BOOT_CHAIN_REG_MAGIC_GET(reg) == BOOT_CHAIN_REG_MAGIC) &&
		((bc_flag == BC_FLAG_OTA_ON) || (bc_flag == BC_FLAG_REDUNDANCY_BOOT))) {
		/*
		 * When control reaches here, BL can claim safe.
		 *
		 * If OTA in progress, save retry count.
		 * or
		 * If REDUNDANCY enabled, check redundancy status. save retry count
		 *    based on return flag.
		 *
		 * If SMD buffer is changed, flush SMD buffer to SMD partition.
		 */
		if ((bc_flag == BC_FLAG_REDUNDANCY_BOOT) &&
		    (BOOTCTRL_SUPPORT_REDUNDANCY(version) != 0U)) {
			error = check_redundancy_status(reg, smd);
			if (error != TEGRABL_NO_ERROR) {
				goto done;
			}
		}

		/* Update SMD based on SR */
		reg = tegrabl_get_boot_slot_reg();
		tegrabl_a_b_copy_retry_count(smd, &reg, FROM_REG_TO_SMD);

		/* Flush SMD if current SMD buffer is changed */
		if (memcmp(smd, smd_backup, sizeof(struct slot_meta_data_v2))) {
			pr_info("SMD partition is updated.\n");
			error = tegrabl_a_b_flush_smd(smd);
			if (error != TEGRABL_NO_ERROR) {
				TEGRABL_SET_HIGHEST_MODULE(error);
				goto done;
			}
		}
	}

done:
	/* Clear SR before handing over to kernel */
	if (BOOT_CHAIN_REG_MAGIC_GET(reg) == BOOT_CHAIN_REG_MAGIC) {
		reg = 0;
		tegrabl_set_boot_slot_reg(reg);
	}

	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
	}
	return error;
}
