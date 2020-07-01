/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include <string.h>
#include <tegrabl_display_dtb.h>

/**
 *  @brief Parse hdmi regulator settings
 *
 *  @param fdt pointer to device tree
 *  @param node_offset nvdisp node to be parsed
 *  @param pdata pointer to dtb data structure
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t parse_hdmi_regulator_settings(const void *fdt,
	int32_t node_offset, struct tegrabl_display_pdata *pdata);


/**
 *  @brief Parse hdmi hpd gpio
 *
 *  @param fdt pointer to device tree
 *  @param sor_offset SOR node to be parsed
 *  @param pdata pointer to display data structure
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t parse_hpd_gpio(const void *fdt, int32_t sor_offset,
							   struct tegrabl_display_pdata *pdata);
