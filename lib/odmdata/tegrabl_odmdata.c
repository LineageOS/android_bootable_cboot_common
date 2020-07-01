/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_ODM_DATA
#define NVBOOT_TARGET_FPGA 0

#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_odmdata_lib.h>
#include <tegrabl_brbct.h>
#include <tegrabl_odmdata_soc.h>
#include <tegrabl_bct_aux_internal_data.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_malloc.h>
#include <nvboot_bct.h>
#include <string.h>

static uint32_t odmdata;

extern struct odmdata_params odmdata_array[ODMDATA_PROP_TYPE_MAX];

static uint32_t odmdata_get_offset(void)
{
	return (uint32_t)NVBOOT_BCT_CUSTOMER_DATA_SIZE - sizeof(NvBctAuxInternalData) +
		   offsetof(NvBctAuxInternalData, CustomerOption);
}

uint32_t tegrabl_odmdata_get(void)
{
	NvBootConfigTable *pbct;

	pbct = (NvBootConfigTable *)tegrabl_brbct_get();
	if (pbct == NULL) {
		pr_trace("brbct is not initialised!!, odmdata will be 0\n");
	} else {
		odmdata = NV_READ32((uintptr_t)&(pbct->CustomerData) + odmdata_get_offset());
	}
	return odmdata;
}

/**
 * @brief flush odmdata to storage
 * @param val odmdata value
 * @return TEGRABL_NO_ERROR if success, else appropriate code
 */
static tegrabl_error_t flush_odmdata_to_storage(uint32_t val)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_partition part;
	NvBootConfigTable *brbct = NULL;
	uint32_t bct_size = 0;
	uint64_t part_size = 0;

	/* Need to read br-bct from storage since the sdram copy may be decrypted */
	err = tegrabl_partition_open("BCT", &part);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	bct_size = sizeof(NvBootConfigTable);
	brbct = tegrabl_malloc(bct_size);
	if (brbct == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto done;
	}

	err = tegrabl_partition_read(&part, (void *)brbct, bct_size);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	NV_WRITE32(((uintptr_t)&(brbct->CustomerData)) + odmdata_get_offset(), val);

	err = tegrabl_partition_seek(&part, 0, TEGRABL_PARTITION_SEEK_SET);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	part_size = tegrabl_partition_size(&part);
	err = tegrabl_brbct_write_multiple((void *)brbct,
									   (void *)&part, part_size, bct_size,
									   (uint32_t)bct_size);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	pr_info("ODMDATA successfully written to storage.\n");

done:
	if (brbct != NULL) {
		tegrabl_free(brbct);
	}

	tegrabl_partition_close(&part);

	return err;
}

tegrabl_error_t tegrabl_odmdata_set(uint32_t val)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	NvBootConfigTable * pbct;

	pbct = (NvBootConfigTable *)tegrabl_brbct_get();
	if (pbct == NULL) {
		pr_error("brbct is not initialised!!\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	NV_WRITE32((uintptr_t)&(pbct->CustomerData) + odmdata_get_offset(), val);

	/* Update odmdata to BCT partition on storage */
	err = flush_odmdata_to_storage(val);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to flush odmdata to storage\n");
		return err;
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_odmdata_params_get(
	struct odmdata_params **podmdata_list,
	uint32_t *odmdata_array_size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((odmdata_array_size == NULL) || (podmdata_list == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		pr_error("Error: %d, Invalid args in 'tegrabl_odmdata_list_get'\n", err);
		return err;
	}

	*podmdata_list = odmdata_array;
	*odmdata_array_size = ARRAY_SIZE(odmdata_array);

	return err;
}

bool tegrabl_odmdata_is_device_unlocked(void)
{
	bool locked = false;

	locked = tegrabl_odmdata_get_config_by_name("bootloader_locked");
	return !locked;
}

bool tegrabl_odmdata_get_config_by_name(const char *name)
{
	uint32_t result;
	uint32_t odm_data, i;

	for (i = 0; i < ARRAY_SIZE(odmdata_array); i++) {
		if (strcmp(name, odmdata_array[i].name) == 0) {
			odm_data = tegrabl_odmdata_get();
			result = odm_data & odmdata_array[i].mask;
			result ^= odmdata_array[i].val;
			return (result != 0U) ? false : true;
		}
	}

	return false;
}
