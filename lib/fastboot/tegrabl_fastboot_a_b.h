/*
 * Copyright (c) 2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_FASTBOOT_A_B_H
#define TEGRABL_FASTBOOT_A_B_H

#include <tegrabl_error.h>
#include <tegrabl_fastboot_protocol.h>

extern const char *fastboot_a_b_slot_var_list[];
extern uint32_t size_a_b_slot_var_list;

/**
 * @brief Functions that provide implementation of fastboot, they have to be
 *        registered in each binaries with binary specific implementation
 *
 * @param get_current_slot API to get current boot slot
 * @param get_slot_numm API to get total slot number
 * @param get_slot_suffix API to get slot suffix
 * @param is_slot_successful API to get the successful flag of given slot
 * @param is_slot_unbootable API to check if given slot is unbootable
 * @param get_slot_retry_count API to get remaining retry count of given slot
 * @param slot_set_active API to set the given slot successful
 */
struct tegrabl_fastboot_a_b_ops {
	tegrabl_error_t (*get_current_slot)(char *slot);
	tegrabl_error_t (*get_slot_num)(uint8_t *num);
	tegrabl_error_t (*get_slot_suffix)(uint8_t slot, char *slot_suffixes);
	tegrabl_error_t (*is_slot_successful)(const char *slot_suffix, bool *state);
	tegrabl_error_t (*is_slot_unbootable)(const char *slot_suffix, bool *state);
	tegrabl_error_t (*get_slot_retry_count)(const char *slot_suffix,
											uint8_t *count);
	tegrabl_error_t (*slot_set_active)(const char *slot_suffix);
};

/*
 * @brief Check if var type is in fastboot_a_b_var_list
 *
 * @param arg var args get from fastboot protocol
 *
 * @return true if arg is a_b var type
 */
bool is_a_b_var_type(const char *arg);

/**
 * @brief Handle fastboot a_b_update vars
 *
 * @param arg var args get from fastboot protocol
 * @param response fastboot response buffer
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_fastboot_a_b_var_handler(const char *arg,
												 char *response);

/**
 * @brief Get the active boot slot and return the suffix
 *
 * @param slot_suffix slot_suffix string within double quotations (output param)
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_get_current_slot(char *slot_suffix);

/**
 * @brief Get the slot number
 *
 * @param slot_number number of slots (output param)
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_get_slot_num(uint8_t *slot_num);

/**
 * @brief Get slot suffixes base on slot index
 *
 * @param slot_id slot index
 * @param slot_suffix slot suffix string (output param)
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_get_slot_suffix(uint8_t slot_id, char *slot_suffix);

/**
 * @brief Get a commma-separated list of slot suffixes supported by the device
 *
 * @param slot_suffixes a commma-separated list of slot suffixes (output param)
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_get_slot_suffixes(char *slot_suffixes);

/**
 * @brief Check if given slot is verified to boot successful
 *
 * @param slot given slot suffix
 * @param state slot boot successful flag (output param)
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_is_slot_successful(const char *slot, bool *state);

/**
 * @brief Check if given slot is unable to boot
 *
 * @param slot given slot suffix
 * @param state if slot is unbootable (output param)
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_is_slot_unbootable(const char *slot, bool *state);

/**
 * @brief Check if given slot is successful to boot
 *
 * @param slot given slot suffix
 * @param count retry count remained (output param)
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_get_slot_retry_count(const char *slot, uint8_t *count);

/**
 * @brief Mark the given slot successful as well as clear unbootable flag and
 *        reset the retry count
 *
 * @param slot given slot suffix
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_set_active_slot(const char *slot);

/**
 * @brief  Get the pointer of fastboot a_b ops struct
 * @return pointer of fastboot ops
 */
struct tegrabl_fastboot_a_b_ops *tegrabl_fastboot_get_a_b_ops(void);

#endif /* TEGRABL_FASTBOOT_A_B_H */
