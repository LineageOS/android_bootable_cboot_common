/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include <tegrabl_error.h>
#include <tegrabl_display_dtb.h>

/**
 *  @brief Parse dp dtb settings
 *
 *  @param fdt pointer to device tree
 *  @param offset offset of sor node containing dp-display node to be parsed
 *  @param dp_dtb pointer to dp dtb data structure
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t parse_dp_dtb_settings(const void *fdt, int32_t offset,
									  struct tegrabl_display_dp_dtb *dp_dtb);

/**
*  @brief Parse dp regulator settings
*
*  @param fdt pointer to device tree
*  @param node_offset nvdisp node to be parsed
*  @param pdata pointer to dtb data structure
*
*  @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t parse_dp_regulator_settings(const void *fdt,
	int32_t node_offset, struct tegrabl_display_pdata *pdata);
