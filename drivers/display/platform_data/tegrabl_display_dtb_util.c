/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_DISPLAY

#include <string.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_nvdisp.h>
#include <libfdt.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_display_dtb_util.h>

void parse_nvdisp_dtb_settings(const void *fdt, int32_t node_offset,
							struct tegrabl_display_pdata *pdata)
{
	uint32_t prop_val;
	const uint32_t *temp;

	temp = fdt_getprop(fdt, node_offset, "nvidia,dc-flags", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		if (prop_val)
			pdata->flags = NVDISP_FLAG_ENABLED;
		pr_debug("dc flags %d\n", prop_val);
	} else {
		pdata->flags = 0x0;
	}

	temp = fdt_getprop(fdt, node_offset, "nvidia,cmu-enable", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		if (prop_val)
			pdata->flags |= NVDISP_FLAG_CMU_ENABLE;
		pr_debug("cmu enable %d\n", prop_val);
	}

	temp = fdt_getprop(fdt, node_offset, "nvidia,fb-win", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		if (prop_val)
			pdata->win_id = prop_val;
		pr_debug("using window %d\n", prop_val);
	}
}

tegrabl_error_t parse_prod_settings(const void *fdt, int32_t prod_offset,
	struct prod_list **prod_list, struct prod_pair *node_config,
	uint32_t num_nodes)
{
	int32_t prod_subnode;
	uint32_t prod_tuple_count;
	const struct fdt_property *property;
	uint32_t i;
	uint32_t j;
	uint32_t k;
	struct prod_tuple *prod_tuple;
	const char *temp;
	struct prod_list *prod_list_l;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	prod_list_l = tegrabl_malloc(sizeof(struct prod_list));
	if (prod_list_l == NULL) {
		pr_error("%s: memory allocation failed\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 6);
		goto fail;
	}

	prod_list_l->num = num_nodes;
	prod_list_l->prod_settings = tegrabl_malloc(prod_list_l->num *
		sizeof(struct prod_settings));

	if (prod_list_l->prod_settings == NULL) {
		pr_error("%s: memory allocation failed\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 7);
		goto fail;
	}

	for (i = 0; i < prod_list_l->num; i++) {
		prod_subnode = fdt_subnode_offset(fdt, prod_offset,
										  node_config[i].name);
		property = fdt_get_property(fdt, prod_subnode, "prod", NULL);
		if (!property) {
			pr_error("error in getting property (offset) %d\n", prod_subnode);
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
			goto fail;
		}

		err = tegrabl_dt_count_elems_of_size(fdt, prod_subnode, "prod", sizeof(uint32_t), &prod_tuple_count);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s: Failed to get number of prod tuples\n", __func__);
			goto fail;
		}
		prod_tuple_count /= 3;

		prod_list_l->prod_settings[i].prod_tuple =
			tegrabl_malloc(prod_tuple_count * sizeof(struct prod_tuple));
		if (prod_list_l->prod_settings[i].prod_tuple == NULL) {
			pr_error("%s: memory allocation failed\n", __func__);
			err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 8);
			goto fail;
		}
		prod_list_l->prod_settings[i].count = prod_tuple_count;
		prod_tuple = prod_list_l->prod_settings[i].prod_tuple;
		for (j = 0, k = 0; j < prod_tuple_count; j++) {
			temp = property->data + k;
			prod_tuple[j].addr = fdt32_to_cpu(*(uint32_t *)temp);
			k = k + 4;

			temp = property->data + k;
			prod_tuple[j].mask = fdt32_to_cpu(*(uint32_t *)temp);
			k = k + 4;

			temp = property->data + k;
			prod_tuple[j].val = fdt32_to_cpu(*(uint32_t *)temp);
			k = k + 4;
		}
	}

	*prod_list = prod_list_l;

	return err;

fail:
	pr_debug("%s, failed to parse prod settings\n", __func__);
	if (prod_list_l)
		tegrabl_free(prod_list_l);
	return err;
}

void tegrabl_display_parse_xbar(const void *fdt, int32_t sor_offset,
	struct tegrabl_display_pdata *pdata)
{
	const struct fdt_property *property;
	uint32_t i;

	memset(pdata->xbar_ctrl, 0, XBAR_CNT);

	property = fdt_get_property(fdt, sor_offset, "nvidia,xbar-ctrl", NULL);
	if (property != NULL) {
		for (i = 0; i < XBAR_CNT; i++) {
			pdata->xbar_ctrl[i] = fdt32_to_cpu(*(property->data32 + i));
		}
	} else {
		pr_warn("error in getting xbar-ctrl property offset\n");
		pr_warn("setting to default value 0 1 2 3 4\n");
		for (i = 0; i < XBAR_CNT; i++)
			pdata->xbar_ctrl[i] = i;
	}
}
