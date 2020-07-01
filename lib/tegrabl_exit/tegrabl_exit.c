/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_EXIT

#include <tegrabl_error.h>
#include <tegrabl_exit.h>

static struct tegrabl_exit_ops ops;

struct tegrabl_exit_ops *tegrabl_exit_get_ops(void)
{
	return &ops;
}

inline tegrabl_error_t tegrabl_reset(void)
{
	if (!ops.sys_reset) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return ops.sys_reset(NULL);
}

inline tegrabl_error_t tegrabl_reboot_forced_recovery(void)
{
	if (!ops.sys_reboot_forced_recovery) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return ops.sys_reboot_forced_recovery(NULL);
}

inline tegrabl_error_t tegrabl_reboot_fastboot(void)
{
	if (!ops.sys_reboot_fastboot) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return ops.sys_reboot_fastboot(NULL);
}

inline tegrabl_error_t tegrabl_poweroff(void)
{
	if (!ops.sys_power_off) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return ops.sys_power_off(NULL);
}
