/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __TEGRABL_BOOTLOADER_UPDATE_H
#define __TEGRABL_BOOTLOADER_UPDATE_H

#include <tegrabl_error.h>
#include <tegrabl_nvblob.h>

struct tegrabl_bl_update_callbacks {
	tegrabl_error_t (*verify_payload)(tegrabl_blob_handle bh);
	tegrabl_error_t (*update_bct)(uintptr_t new_bct, uint32_t size);
	tegrabl_error_t (*get_slot_num)(uint8_t *num);
	tegrabl_error_t (*get_slot_via_suffix)(const char *suffix, uint32_t *slot);
	tegrabl_error_t (*get_slot_suffix)(uint32_t slot, char *slot_suffix,
									   bool full_suffix);
	tegrabl_error_t (*is_ratchet_update_required)(void *blob, bool *is_needed);
	bool (*is_always_ab_partition)(const char *part);
};

/**
 * @brief Set some chip specific callbacks for certain binary updating
 * @param cbs callbacks need to be initialised
 */
void tegrabl_bl_update_set_callbacks(struct tegrabl_bl_update_callbacks *cbs);

/**
 * @brief Check and update for payload/nvblob
 *
 * @param p_ptr payload pointer, it is present in the sdram memory
 * @param size size of the payload
 * @param suffix slot suffix to update
 *
 * @return Return TEGRABL_NO_ERROR if update success otherwise return error
 *         status value.
 */
tegrabl_error_t tegrabl_check_and_update_bl_payload(void *p_ptr, uint32_t size,
													const char *suffix);

#endif
