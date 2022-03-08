/*
 * Copyright (c) 2015-2021, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_LINUXBOOT

#include "build_config.h"
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_linuxboot_helper.h>
#include <tegrabl_binary_types.h>
#include <tegrabl_auth.h>
#include <tegrabl_bootimg.h>
#include <tegrabl_linuxboot_utils.h>

int32_t tegrabl_bom_compare(struct tegrabl_carveout_info *p_carveout, const uint32_t a, const uint32_t b)
{
	if (p_carveout[a].base < p_carveout[b].base)
		return -1;
	else if (p_carveout[a].base > p_carveout[b].base)
		return 1;
	else
		return 0;
}

void tegrabl_sort(struct tegrabl_carveout_info *p_carveout, uint32_t array[], int32_t count)
{
	uint32_t val;
	int32_t i;
	int32_t j;

	if (count < 2)
		return;

	for (i = 1; i < count; i++) {
		val = array[i];

		for (j = (i - 1);
				 (j >= 0) && (tegrabl_bom_compare(p_carveout, val, array[j]) < 0);
				 j--) {
			array[j + 1] = array[j];
		}

		array[j + 1] = val;
	}
}

#if defined(CONFIG_ENABLE_SECURE_BOOT)
tegrabl_error_t tegrabl_validate_binary(uint32_t bin_type, char *bin_name, uint32_t bin_max_size,
										void *load_addr, uint32_t *bin_len)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	TEGRABL_UNUSED(bin_max_size);

	pr_info("Validate %s ...\n", bin_name);

	if (!tegrabl_do_ratchet_check(bin_type, load_addr)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 2);
		goto fail;
	}

	if (bin_len != NULL) {
		*bin_len = tegrabl_auth_get_binary_len(load_addr);
	}

	err = tegrabl_auth_payload(bin_type, bin_name, load_addr, bin_max_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	 return err;
 }
#endif  /* CONFIG_ENABLE_SECURE_BOOT */

/* Sanity checks the kernel image extracted from Android boot image */
tegrabl_error_t tegrabl_verify_boot_img_hdr(union tegrabl_bootimg_header *hdr, uint32_t img_size)
{
	uint32_t hdr_size;
	uint64_t hdr_fields_sum;
	uint32_t known_crc = 0;
	uint32_t calculated_crc = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_info("Checking boot.img header magic ... ");
	if (memcmp(hdr->magic, ANDROID_MAGIC, ANDROID_MAGIC_SIZE)) {
		pr_error("Invalid header magic\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
		goto fail;
	}
	pr_info("[OK]\n");

	if (hdr->pagesize < sizeof(union tegrabl_bootimg_header)) {
		pr_error("Page size field (0x%08x) is less than header structure size (0x%08lx)\n",
				 hdr->pagesize, sizeof(union tegrabl_bootimg_header));
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	hdr_size = hdr->pagesize;
	hdr_fields_sum = hdr_size + hdr->kernelsize + hdr->ramdisksize + hdr->secondsize;
	pr_trace("img/buffer size : 0x%08x\n", (uint32_t)img_size);
	pr_trace("hdr fields sum  : 0x%08x\n", (uint32_t)hdr_fields_sum);
	pr_trace("kernel size     : 0x%08x\n", (uint32_t)hdr->kernelsize);
	pr_trace("ramdisk size    : 0x%08x\n", (uint32_t)hdr->ramdisksize);
	pr_trace("second size     : 0x%08x\n", (uint32_t)hdr->secondsize);
	pr_trace("page size       : 0x%08x\n", (uint32_t)hdr->pagesize);
	if (hdr_fields_sum > img_size) {
		pr_error("Header size fields (0x%016lx) is greater than actual binary or buffer size (0x%08x)\n",
				 hdr_fields_sum, img_size);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	/* Check header CRC if present */
	known_crc = hdr->word[(ANDROID_HEADER_SIZE - CRC32_SIZE) / sizeof(uint32_t)];
	if (known_crc) {
		pr_info("Checking boot.img header crc ... ");
		calculated_crc = tegrabl_utils_crc32(0, (char *)hdr, ANDROID_HEADER_SIZE);
		if (calculated_crc != known_crc) {
			pr_error("Invalid boot.img @ %p (header crc mismatch)\n", hdr);
			err = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 1);
			goto fail;
		}
		pr_info("[OK]\n");
	}

fail:
	return err;
}
