/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef TEGRABL_EXIT_H
#define TEGRABL_EXIT_H

#include <tegrabl_error.h>

/**
 * @brief Functions that provide implementation of exit path, they have to be
 *        registered in each binaries with binary specific implementation
 * @param sys_power_off API of board power off
 * @param sys_reset     API of board reboot
 * @param sys_reboot_fastboot API to set fastboot bit in pmic and reboot the
 *                      board to enter fastboot
 * @param sys_reboot_forced_recovery API to set forced recovery bit in pmic
 *                      and reboot the board to enter forced recovery mode
 */
struct tegrabl_exit_ops {
	tegrabl_error_t (*sys_power_off)(void *param);
	tegrabl_error_t (*sys_reset)(void *param);
	tegrabl_error_t (*sys_reboot_fastboot)(void *param);
	tegrabl_error_t (*sys_reboot_forced_recovery)(void *param);
};

/**
 * @brief Get the pointer of exit ops struct
 * @return pointer of exit ops
 */
struct tegrabl_exit_ops *tegrabl_exit_get_ops(void);

/**
 * @brief Power off the board
 * @return TEGRABL_NO_ERROR if success else approprite error code
 */
tegrabl_error_t tegrabl_poweroff(void);

/**
 * @brief Reset the board and enter fastboot
 * @return TEGRABL_NO_ERROR if success else approprite error code
 */
tegrabl_error_t tegrabl_reboot_fastboot(void);

/**
 * @brief Reset the board and enter recovery
 * @return TEGRABL_NO_ERROR if success else approprite error code
 */
tegrabl_error_t tegrabl_reboot_forced_recovery(void);

/**
 * @brief Close display and reset the board
 * @return TEGRABL_NO_ERROR if success else approprite error code
 */
tegrabl_error_t tegrabl_reset(void);

/**
 * @brief Init and register CPU/BPMP specific exit path (aka reboot bootloader,
 *        reboot forced recovery, power off and reboot) to tegrabl exit module
 *
 * @return TEGRABL_NO_ERROR if success, else appropriate error code
 */
tegrabl_error_t tegrabl_exit_register(void);

#endif /* __TEGRABL_EXIT_H__ */
