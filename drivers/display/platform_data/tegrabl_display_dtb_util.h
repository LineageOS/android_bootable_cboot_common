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
 *  @brief Parse dc prod settings
 *
 *  @param fdt pointer to device tree
 *  @param node_offset nvdisplay node to be parsed
 *  @param pdata pointer to display data structure
 */
void parse_nvdisp_dtb_settings(const void *fdt, int32_t node_offset,
							struct tegrabl_display_pdata *pdata);

/**
 *  @brief Parse display prod settings
 *
 *  @param fdt pointer to device tree
 *  @param prod_offset prod settings node to be parsed
 *  @param prod_list data pointer to display prod list
 *  @param node_config nodes to be parsed from dt for particular display
 *  @param num_nodes number of nodes to be parsed from dt
 *  @param prod_tuple_count number of tuples for each dt node entry
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t parse_prod_settings(const void *fdt, int32_t prod_offset,
	struct prod_list **prod_list, struct prod_pair *node_config,
	uint32_t num_nodes, uint32_t prod_tuple_count);

/**
 *  @brief Parse xbar-ctrl property from Sor node
 *
 *  @param fdt pointer to device tree
 *  @param sor_offset sor offset to be considered for xbar
 *  @param pdata pointer to display data structure
 */
void tegrabl_display_parse_xbar(const void *fdt, int32_t sor_offset,
	struct tegrabl_display_pdata *pdata);
