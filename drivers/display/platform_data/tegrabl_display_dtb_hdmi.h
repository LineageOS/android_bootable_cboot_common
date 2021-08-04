/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include <string.h>
#include <tegrabl_display_dtb.h>

#define PROD_LIST_SOC "prod_list_hdmi_soc"
#define PROD_LIST_BOARD "prod_list_hdmi_board"
#define PROD_FORCE_LEGACY "bootloader-use-legacy-prods"

struct tmds_range_info {
	uint32_t lower_hz;
	uint32_t upper_hz;
	const char *name;
};

/**
 *  @brief Parse hdmi regulator settings
 *
 *  @param fdt pointer to device tree
 *  @param node_offset nvdisp node to be parsed
 *  @param pdata pointer to dtb data structure
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t parse_hdmi_regulator_settings(const void *fdt, int32_t node_offset,
	struct tegrabl_display_pdata *pdata);


/**
 *  @brief Parse hdmi hpd gpio
 *
 *  @param fdt pointer to device tree
 *  @param sor_offset SOR node to be parsed
 *  @param pdata pointer to display data structure
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t parse_hpd_gpio(const void *fdt, int32_t sor_offset, struct tegrabl_display_pdata *pdata);


/**
 *  @brief Parse hdmi prod settings
 *
 *  @param fdt pointer to device tree
 *  @param prod_offset prod settings node to be parsed
 *  @param prod_list data pointer to display prod list
 *  @param legacy_node_config nodes to be parsed from dt for hdmi display
 *  @param legacy_num_nodes number of nodes to be parsed from dt
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t parse_hdmi_prod_settings(const void *fdt, int32_t prod_offset, struct prod_list **prod_list,
	struct prod_pair *legacy_node_config, uint32_t legacy_num_nodes);
