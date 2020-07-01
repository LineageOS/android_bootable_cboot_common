/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_DISPLAY_PDATA

#include <string.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_nvdisp.h>
#include <libfdt.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_display_dtb_util.h>

/*Number of prod settings in 1 prod tuple in dtb node*/
#if defined(IS_T186)
#define PROD_TUPLE_DIV 3	/*addr, mask, value*/
#else
#define PROD_TUPLE_DIV 4	/*controller offset, addr, mask, val*/
#endif

void parse_nvdisp_dtb_settings(const void *fdt, int32_t offset, struct tegrabl_display_pdata *pdata)
{
	uint32_t prop_val;
	const uint32_t *temp;

	temp = fdt_getprop(fdt, offset, "nvidia,dc-ctrlnum", NULL);
	if (temp != NULL) {
		pdata->nvdisp_instance = fdt32_to_cpu(*temp);
		pr_debug("nvdisp_instance = %d\n", pdata->nvdisp_instance);
	}

	temp = fdt_getprop(fdt, offset, "nvidia,dc-flags", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		if (prop_val) {
			pdata->flags = NVDISP_FLAG_ENABLED;
		}
		pr_debug("dc flags %d\n", prop_val);
	} else {
		pdata->flags = 0x0;
	}

	temp = fdt_getprop(fdt, offset, "nvidia,cmu-enable", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		if (prop_val) {
			pdata->flags |= NVDISP_FLAG_CMU_ENABLE;
		}
		pr_debug("cmu enable %d\n", prop_val);
	}

	temp = fdt_getprop(fdt, offset, "nvidia,fb-win", NULL);
	if (temp != NULL) {
		pdata->win_id = fdt32_to_cpu(*temp);
		pr_debug("using window %d\n", pdata->win_id);
	}
}

tegrabl_error_t parse_prod_settings(const void *fdt, int32_t prod_offset, struct prod_list **prod_list,
	struct prod_pair *node_config, uint32_t num_nodes)
{
	int32_t prod_subnode = -1;
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
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 5);
		goto fail;
	}

	prod_list_l->num = num_nodes;
	prod_list_l->prod_settings = tegrabl_malloc(prod_list_l->num * sizeof(struct prod_settings));

	if (prod_list_l->prod_settings == NULL) {
		pr_error("%s: memory allocation failed\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 6);
		goto fail;
	}

	for (i = 0; i < prod_list_l->num; i++) {
		prod_subnode = fdt_subnode_offset(fdt, prod_offset, node_config[i].name);
		if (prod_subnode < 0) {
			pr_debug("prod setting subnode \"%s\" not found\n", node_config[i].name);
			continue;
		}
		property = fdt_get_property(fdt, prod_subnode, "prod", NULL);
		if (!property) {
			pr_error("error in getting property (offset) %d\n", prod_subnode);
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 3);
			goto fail;
		}

		err = tegrabl_dt_count_elems_of_size(fdt, prod_subnode, "prod", sizeof(uint32_t), &prod_tuple_count);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s: Failed to get number of prod tuples\n", __func__);
			goto fail;
		}
		prod_tuple_count /= PROD_TUPLE_DIV;

		prod_list_l->prod_settings[i].prod_tuple =
			tegrabl_malloc(prod_tuple_count * sizeof(struct prod_tuple));
		if (prod_list_l->prod_settings[i].prod_tuple == NULL) {
			pr_error("%s: memory allocation failed\n", __func__);
			err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 7);
			goto fail;
		}
		prod_list_l->prod_settings[i].count = prod_tuple_count;
		prod_tuple = prod_list_l->prod_settings[i].prod_tuple;
		for (j = 0, k = 0; j < prod_tuple_count; j++) {
#if !defined(IS_T186) /*introduced new in t19x*/
			temp = property->data + k;
			prod_tuple[j].index = fdt32_to_cpu(*(uint32_t *)temp);
			k = k + 4;
#endif
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
	if (prod_list_l) {
		tegrabl_free(prod_list_l);
	}
	return err;
}

void tegrabl_display_parse_xbar(const void *fdt, int32_t sor_offset, struct tegrabl_display_sor_dtb *sor_dtb)
{
	uint32_t i;

	memset(sor_dtb->xbar_ctrl, 0, XBAR_CNT);

	if (tegrabl_dt_get_prop_u32_array(fdt, sor_offset, "nvidia,xbar-ctrl", XBAR_CNT,
									  sor_dtb->xbar_ctrl, NULL) == TEGRABL_NO_ERROR) {
		for (i = 0; i < XBAR_CNT; i++) {
			pr_debug("nvidia,xbar-ctrl = %d\n", sor_dtb->xbar_ctrl[i]);
		}
	} else {
		pr_warn("error in getting xbar-ctrl property offset\n");
		pr_warn("setting to default values 0 1 2 3 4\n");
		for (i = 0; i < XBAR_CNT; i++) {
			sor_dtb->xbar_ctrl[i] = i;
		}
	}
}

struct nvdisp_mode *parse_display_timings(const void *fdt, int32_t disp_off)
{
	int32_t timing_node = -1;
	int32_t res_node = -1;
	struct nvdisp_mode *mode = NULL;

	mode = tegrabl_malloc(sizeof(struct nvdisp_mode));
	if (mode == NULL) {
		pr_error("memory allocation failed\n");
		goto fail;
	}

	memset(mode, 0, sizeof(struct nvdisp_mode));

	timing_node = fdt_subnode_offset(fdt, disp_off, "display-timings");
	if (timing_node < 0) {
		pr_debug("timing_node \"%s\" not found\n", "display-timing");
		goto fail;
	}

	res_node = fdt_first_subnode(fdt, timing_node);
	if (res_node < 0) {
		pr_debug("no resolution node found\n");
		goto fail;
	}

	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "clock-frequency", &mode->pclk);
	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "hactive", &mode->h_active);
	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "vactive", &mode->v_active);
	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "hfront-porch", &mode->h_front_porch);
	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "hback-porch", &mode->h_back_porch);
	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "vfront-porch", &mode->v_front_porch);
	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "vback-porch", &mode->v_back_porch);
	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "hsync-len", &mode->h_sync_width);
	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "vsync-len", &mode->v_sync_width);
	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "nvidia,h-ref-to-sync", &mode->h_ref_to_sync);
	(void)tegrabl_dt_get_prop_u32(fdt, res_node, "nvidia,v-ref-to-sync", &mode->v_ref_to_sync);

	pr_debug("nvdisp->pclk = %d\n", mode->pclk);
	pr_debug("nvdisp->h_active = %d\n", mode->h_active);
	pr_debug("nvdisp->vactive = %d\n", mode->v_active);
	pr_debug("nvdisp->h_front_porch = %d\n", mode->h_front_porch);
	pr_debug("nvdisp->h_back_porch = %d\n", mode->h_back_porch);
	pr_debug("nvdisp->v_front_porch = %d\n", mode->v_front_porch);
	pr_debug("nvdisp->v_back_porch = %d\n", mode->v_back_porch);
	pr_debug("nvdisp->h_sync_width = %d\n", mode->h_sync_width);
	pr_debug("nvdisp->v_sync_width = %d\n", mode->v_sync_width);
	pr_debug("nvdisp->h_ref_to_sync = %d\n", mode->h_ref_to_sync);
	pr_debug("nvdisp->v_ref_to_sync = %d\n", mode->v_ref_to_sync);

	pr_debug("display timings parsed from DT successfully\n");

	return mode;

fail:
	return NULL;
}
