/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All Rights Reserved.
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

tegrabl_error_t tegrabl_reset(void)
{
	if (ops.sys_reset == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return ops.sys_reset(NULL);
}

tegrabl_error_t tegrabl_reboot_forced_recovery(void)
{
	if (ops.sys_reboot_forced_recovery == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return ops.sys_reboot_forced_recovery(NULL);
}

tegrabl_error_t tegrabl_reboot_fastboot(void)
{
	if (ops.sys_reboot_fastboot == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return ops.sys_reboot_fastboot(NULL);
}

tegrabl_error_t tegrabl_poweroff(void)
{
	if (ops.sys_power_off == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return ops.sys_power_off(NULL);
}
