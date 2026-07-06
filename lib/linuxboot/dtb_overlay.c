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
#include <stdio.h>
#include <libfdt.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_board_info.h>
#include <libufdt.h>
#include <ufdt_overlay.h>
#include <dt_table_core.h>

tegrabl_error_t tegrabl_dtb_overlay(void **kernel_dtb, void *kernel_dtbo)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct fdt_header *main_dt, *overlay_dt, *merged_dt;
	uint32_t main_dt_sz, overlay_dt_sz;
	struct board_id_info id_info;
	uint16_t i;
	uint32_t board_id, board_sku, board_fab;
	struct dt_table_header *dtimage_header = kernel_dtbo;
	struct dt_table_entry *entry0;
	struct dt_table_entry_v1 *entry1;
	struct dt_table_entry_v2 *entry2;
	char dtbo_idx[3] = {'\0'};
	int fdt_err, node;

	if (!(*kernel_dtb) || !kernel_dtbo) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	main_dt = (struct fdt_header *)*kernel_dtb;
	main_dt_sz = fdt_totalsize(main_dt);

	overlay_dt = (struct fdt_header *)kernel_dtbo;
	overlay_dt_sz = fdt_totalsize(overlay_dt);

	if (fdt32_to_cpu(dtimage_header->magic) == DT_TABLE_MAGIC) {
		if (fdt32_to_cpu(dtimage_header->dt_entry_count) > 0) {
			if (tegrabl_get_board_ids((void *)&id_info) != TEGRABL_NO_ERROR) {
				err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
				goto fail;
			}

			for (i = 0; i < id_info.count; ++i) {
				if (0 == strcmp(id_info.part[i].name, "module"))
					break;
			}
			if (i == id_info.count) {
				err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
				goto fail;
			}
			board_id = tegrabl_utils_strtoul((char *)id_info.part[i].part_no, NULL, BASE_10);
			board_sku = tegrabl_utils_strtoul((char *)id_info.part[i].part_no + 5, NULL, BASE_10);
			board_fab = tegrabl_utils_strtoul((char *)id_info.part[i].part_no + 10, NULL, BASE_16);

			switch (fdt32_to_cpu(dtimage_header->version)) {
			case 0:
				entry0 = (struct dt_table_entry *) (kernel_dtbo + fdt32_to_cpu(dtimage_header->dt_entries_offset));
				for (i = 0; i < fdt32_to_cpu(dtimage_header->dt_entry_count); i++) {
					if (fdt32_to_cpu(entry0[i].id) == board_id &&
					    fdt32_to_cpu(entry0[i].rev) == board_fab &&
					    fdt32_to_cpu(entry0[i].custom[0]) == board_sku) {
						overlay_dt = (struct fdt_header *) (kernel_dtbo + fdt32_to_cpu(entry0[i].dt_offset));
						overlay_dt_sz = fdt32_to_cpu(entry0[i].dt_size);
						break;
					}
				}
				if (i == fdt32_to_cpu(dtimage_header->dt_entry_count)) {
					err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
					goto fail;
				}
				break;
			case 1:
				entry1 = (struct dt_table_entry_v1 *) (kernel_dtbo + fdt32_to_cpu(dtimage_header->dt_entries_offset));
				for (i = 0; i < fdt32_to_cpu(dtimage_header->dt_entry_count); i++) {
					if (fdt32_to_cpu(entry1[i].id) == board_id &&
					    fdt32_to_cpu(entry1[i].rev) == board_fab &&
					    fdt32_to_cpu(entry1[i].custom[0]) == board_sku) {
						overlay_dt = (struct fdt_header *) (kernel_dtbo + fdt32_to_cpu(entry1[i].dt_offset));
						overlay_dt_sz = fdt32_to_cpu(entry1[i].dt_size);
						break;
					}
				}
				if (i == fdt32_to_cpu(dtimage_header->dt_entry_count)) {
					err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
					goto fail;
				}
				break;
			case 2:
				entry2 = (struct dt_table_entry_v2 *) (kernel_dtbo + fdt32_to_cpu(dtimage_header->dt_entries_offset));
				for (i = 0; i < fdt32_to_cpu(dtimage_header->dt_entry_count); i++) {
					if (fdt32_to_cpu(entry2[i].id) == board_id &&
					    fdt32_to_cpu(entry2[i].rev) == board_fab &&
					    fdt32_to_cpu(entry2[i].custom[0]) == board_sku) {
						overlay_dt = (struct fdt_header *) (kernel_dtbo + fdt32_to_cpu(entry2[i].dt_offset));
						overlay_dt_sz = fdt32_to_cpu(entry2[i].dt_size);
						break;
					}
				}
				if (i == fdt32_to_cpu(dtimage_header->dt_entry_count)) {
					err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
					goto fail;
				}
				break;
			default:
				err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
				goto fail;
				break;
			}

			sprintf(dtbo_idx, "%u", i);
		} else {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
			goto fail;
		}
	} else if (fdt_check_header(kernel_dtbo) < 0) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 7);
		goto fail;
	}

	pr_info("Merge kernel-dtbo into kernel-dtb\n");
	merged_dt = ufdt_apply_overlay(main_dt, main_dt_sz, overlay_dt,
								   overlay_dt_sz);
	if (!merged_dt) {
		pr_error("Failed to merge kernel-dtbo into kernel-dtb\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 0);
		goto fail;
	}

	if (dtbo_idx[0] != '\0') {
		node = fdt_path_offset(merged_dt, "/firmware/android");
		if (node > 0) {
			fdt_err = fdt_setprop_string(merged_dt, node, "dtbo_idx", dtbo_idx);
			if (fdt_err < 0) {
				pr_error("Failed to add dtbo idx in /firmware/android\n");
				return TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 1);
			}
		}
	}

	/* merged kernel DTB address will be changed */
	*kernel_dtb = (void *)merged_dt;
	pr_info("Merged kernel-dtb @ %p\n", *kernel_dtb);

fail:
	return err;
}

