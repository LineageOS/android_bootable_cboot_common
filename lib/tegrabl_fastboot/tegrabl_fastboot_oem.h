/*
 * Copyright (c) 2016, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_FASTBOOT_OEM_H
#define TEGRABL_FASTBOOT_OEM_H

#include <tegrabl_error.h>
#include <tegrabl_fastboot_protocol.h>

/**
 * @brief Functions that provide implementation of fastboot, they have to be
 *        registered in each binaries with binary specific implementation
 * @param change_device_state API to lock/unlock bootloader
 * @param is_device_unlocked API to get device lock state
 */
struct tegrabl_fastboot_oem_ops {
	bool (*is_device_unlocked)(void);
	tegrabl_error_t (*change_device_state)(enum fastboot_lockcmd);
	tegrabl_error_t (*get_fuse_ecid)(uint32_t *ecid, uint32_t size);
};

/**
 * @brief get the ecid fuse of the device
 *
 * @param ecid ecid stored in fuse (output param)
 * @param size ecid size
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_get_fuse_ecid(uint32_t *ecid, uint32_t size);

/**
 * @brief determine if bootloader is locked or unlocked
 *
 * @param is_unlocked unlocked state (output param)
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_is_device_unlocked(bool *is_unlocked);

/**
 * @brief locks/unlocks bootloader
 * @param cmd signifies whether to lock or unlock the bootloader
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_change_device_state(enum fastboot_lockcmd cmd);

/**
 * @brief  Get the pointer of fastboot oem ops struct
 * @return pointer of fastboot ops
 */
struct tegrabl_fastboot_oem_ops *tegrabl_fastboot_get_oem_ops(void);

/**
 * @brief Handle fastboot oem cmds
 * @param arg oem cmd string
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_fastboot_oem_handler(const char *arg);

#endif /* TEGRABL_FASTBOOT_OEM_H */
