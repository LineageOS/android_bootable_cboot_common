/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_FASTBOOT

#include <tegrabl_error.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_fastboot_oem.h>
#include <tegrabl_debug.h>
#include <tegrabl_fastboot_protocol.h>
#include <string.h>

#define ECID_SIZE 4

static struct tegrabl_fastboot_oem_ops oem_ops;

struct tegrabl_fastboot_oem_ops *tegrabl_fastboot_get_oem_ops(void)
{
	return &oem_ops;
}

tegrabl_error_t tegrabl_change_device_state(enum fastboot_lockcmd cmd)
{
	if (!oem_ops.change_device_state) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	return oem_ops.change_device_state(cmd);
}

tegrabl_error_t tegrabl_is_device_unlocked(bool *is_unlocked)
{
	if (!oem_ops.is_device_unlocked) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	*is_unlocked = oem_ops.is_device_unlocked();
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_get_fuse_ecid(uint32_t *ecid, uint32_t size)
{
	if (!oem_ops.get_fuse_ecid)
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	return oem_ops.get_fuse_ecid(ecid, size);
}

static const char *get_dts_basename(const char *path)
{
	const char *fname = strrchr(path, '/');
	return fname ? (fname + 1) : path;
}

tegrabl_error_t tegrabl_fastboot_oem_handler(const char *arg)
{
	uint8_t response[MAX_RESPONSE_SIZE];
	char dtb_fname[MAX_RESPONSE_SIZE];
	uint32_t ecid[ECID_SIZE];
	char ecid_str[MAX_RESPONSE_SIZE] = {'\0'};
	uint32_t *ecid_ptr = NULL;
	const char *dts_fname = NULL;
	void *fdt;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	memset(response, 0, MAX_RESPONSE_SIZE);

	if (IS_VAR_TYPE("lock"))
		ret = tegrabl_change_device_state(FASTBOOT_LOCK);
	else if (IS_VAR_TYPE("unlock"))
		ret = tegrabl_change_device_state(FASTBOOT_UNLOCK);
	else if (IS_VAR_TYPE("dtbname")) {
		ret = tegrabl_dt_get_fdt_handle(TEGRABL_DT_KERNEL, &fdt);
		if (ret != TEGRABL_NO_ERROR) {
			fastboot_fail("DTB name not found!");
			return ret;
		}
		ret = tegrabl_dt_get_prop_string(fdt, 0, "nvidia,dtsfilename",
										 &dts_fname);
		if (ret != TEGRABL_NO_ERROR) {
			fastboot_fail("DTB name not found!");
			return ret;
		}

		/* Extract only the dts filename from the path */
		dts_fname = get_dts_basename(dts_fname);

		/* Rename *.dts to *.dtb */
		memset(dtb_fname, 0, MAX_RESPONSE_SIZE);
		strncpy(dtb_fname, dts_fname, MAX_RESPONSE_SIZE - 1);
		dtb_fname[strlen(dtb_fname) - 1] = 'b';

		/* Print the dtb filename */
		fastboot_ack("INFO", dtb_fname);
	} else if (IS_VAR_TYPE("ecid")) {
		ret = tegrabl_get_fuse_ecid(ecid, sizeof(ecid));
		if (ret != TEGRABL_NO_ERROR) {
			fastboot_fail("Get ecid failed!");
			return ret;
		}
		/* Transfer ECID to string */
		for (ecid_ptr = ecid + sizeof(ecid) / sizeof(uint32_t) - 1;
			 ecid_ptr >= ecid; ecid_ptr--)
			tegrabl_snprintf(ecid_str, sizeof(ecid_str), "%s%08x", ecid_str,
							 *ecid_ptr);
		fastboot_ack("INFO", ecid_str);
	}

	return ret;
}
