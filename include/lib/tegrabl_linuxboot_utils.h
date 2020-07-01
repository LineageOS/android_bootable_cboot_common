/*
 * Copyright (c) 2015-2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_LINUXBOOT_UTILS_H
#define INCLUDED_LINUXBOOT_UTILS_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include <tegrabl_error.h>
#include <tegrabl_cpubl_params.h>
#include <tegrabl_bootimg.h>

/**
 * @brief Helper API (with BL-specific implementation), to compare based on
 * base address as required for calculating free dram regions
 *
 * @param index-1
 * @param index-2
 *
 * @return 0 or 1 based on comparision and -1 otherwise
 */
int32_t tegrabl_bom_compare(struct tegrabl_carveout_info *p_carveout, const uint32_t a, const uint32_t b);

/**
 * @brief Helper API (with BL-specific implementation), to sort in ascending order
 * based on base addresses of permanent carveouts. This will be used to calcuate
 * free dram regions.
 *
 * @param parm_carveout array
 * @param number of parm carvouts
 *
 * @return void
 */
void tegrabl_sort(struct tegrabl_carveout_info *p_carveout, uint32_t array[], int32_t count);

/**
 * @brief Validate the binary
 *
 * @param bin_type Type of binary
 * @param bin_max_size Max size of the binary
 * @param load_addr Address where binary is loaded
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t tegrabl_validate_binary(uint32_t bin_type, uint32_t bin_max_size, void *load_addr);

/**
 * @brief Verify boot.img header
 *
 * @param hdr Address where boot.img header is loaded
 * @param img_size size of boot.img
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t tegrabl_verify_boot_img_hdr(union tegrabl_bootimg_header *hdr, uint32_t img_size);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_LINUXBOOT_UTILS_H */
