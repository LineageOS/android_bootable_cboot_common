/*
 * Copyright (c) 2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_LINUXBOOT

#include <stdint.h>
#include <libfdt.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <libufdt.h>
#include <ufdt_overlay.h>

tegrabl_error_t tegrabl_dtb_overlay(void **kernel_dtb, void *kernel_dtbo)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct fdt_header *main_dt, *overlay_dt, *merged_dt;
	uint32_t main_dt_sz, overlay_dt_sz;

	if (!(*kernel_dtb) || !kernel_dtbo) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	main_dt = (struct fdt_header *)*kernel_dtb;
	main_dt_sz = fdt_totalsize(main_dt);

	overlay_dt = (struct fdt_header *)kernel_dtbo;
	overlay_dt_sz = fdt_totalsize(overlay_dt);

	pr_info("Merge kernel-dtbo into kernel-dtb\n");
	merged_dt = ufdt_apply_overlay(main_dt, main_dt_sz, overlay_dt,
								   overlay_dt_sz);
	if (!merged_dt) {
		pr_error("Failed to merge kernel-dtbo into kernel-dtb\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 0);
		goto fail;
	}

	/* merged kernel DTB address will be changed */
	*kernel_dtb = (void *)merged_dt;
	pr_info("Merged kernel-dtb @ %p\n", *kernel_dtb);

fail:
	return err;
}

