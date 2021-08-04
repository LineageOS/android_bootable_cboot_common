/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <libfdt.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_display_dtb_hdmi.h>
#include <tegrabl_display_dtb_util.h>
#include <tegrabl_malloc.h>
#include <stdlib.h> /* atoi */

tegrabl_error_t parse_hdmi_regulator_settings(const void *fdt, int32_t node_offset,
	struct tegrabl_display_pdata *pdata)
{
	const uint32_t *temp;

	temp = fdt_getprop(fdt, node_offset, "avdd_hdmi-supply", NULL);
	if (temp != NULL) {
		pdata->hdmi_dtb.avdd_hdmi_supply = fdt32_to_cpu(*temp);
		pr_debug("avdd_hdmi-supply 0x%x\n", pdata->hdmi_dtb.avdd_hdmi_supply);
	} else {
		pdata->hdmi_dtb.avdd_hdmi_supply = 0;
		pr_error("no regulator info present for avdd_hdmi-supply\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
	}

	temp = fdt_getprop(fdt, node_offset, "avdd_hdmi_pll-supply", NULL);
	if (temp != NULL) {
		pdata->hdmi_dtb.avdd_hdmi_pll_supply = fdt32_to_cpu(*temp);
		pr_debug("avdd_hdmi_pll-supply 0x%x\n", pdata->hdmi_dtb.avdd_hdmi_pll_supply);
	} else {
		pdata->hdmi_dtb.avdd_hdmi_pll_supply = 0;
		pr_error("no regulator info present for avdd_hdmi_pll-supply\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 1);
	}

	temp = fdt_getprop(fdt, node_offset, "vdd_hdmi_5v0-supply", NULL);
	if (temp != NULL) {
		pdata->hdmi_dtb.vdd_hdmi_5v0_supply = fdt32_to_cpu(*temp);
		pr_debug("vdd_hdmi_5v0-supply 0x%x\n", pdata->hdmi_dtb.vdd_hdmi_5v0_supply);
	} else {
		pdata->hdmi_dtb.vdd_hdmi_5v0_supply = 0;
		pr_error("no regulator info present for vdd_hdmi_5v0-supply\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 2);
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t parse_hpd_gpio(const void *fdt, int32_t sor_offset, struct tegrabl_display_pdata *pdata)
{
	const struct fdt_property *property;
	int32_t hdmi_prop_offset;

	property = fdt_get_property(fdt, sor_offset, "nvidia,hpd-gpio", NULL);
	if (!property) {
		pr_error("error in getting property \"nvidia,hpd-gpio\"\n");
		goto fail;
	}
	pdata->hdmi_dtb.hpd_gpio = fdt32_to_cpu(*(property->data32 + 1));
	pr_debug("hpd_gpio = %d\n", pdata->hdmi_dtb.hpd_gpio);

	hdmi_prop_offset = fdt_subnode_offset(fdt, sor_offset, "hdmi-display");
	if (hdmi_prop_offset < 0) {
		pr_error("hdmi-display subnode not found\n");
		goto fail;
	}
	hdmi_prop_offset = fdt_subnode_offset(fdt, hdmi_prop_offset, "disp-default-out");
	if (hdmi_prop_offset < 0) {
		pr_error("disp-default-out subnode not found\n");
		goto fail;
	}
	property = fdt_get_property(fdt, hdmi_prop_offset, "nvidia,out-flags", NULL);
	if (!property) {
		pr_warn("error in getting \"nvidia,out-flags\" property, set default value\n");
		pdata->hdmi_dtb.polarity = 0;
	} else {
		pdata->hdmi_dtb.polarity = fdt32_to_cpu(*(property->data32));
		pr_debug("polarity = %d\n", pdata->hdmi_dtb.polarity);
	}

	return TEGRABL_NO_ERROR;

fail:
	return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 13);
}

static tegrabl_error_t retrieve_range(const char *buf,
		uint32_t *lower_hz, uint32_t *upper_hz)
{
	uint32_t count = 0;
	uint32_t offset;
	uint32_t lower, upper;
	char *digit_list = "0123456789";
	const char *str;

	if (!buf || !lower_hz || !upper_hz) {
		pr_debug("%s(), invalid input\n", __func__); // always
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
	}

	str = buf;
	while (*str != '\0') {
		if (*str >= '0' && *str <= '9') {
			count++;
			offset = strspn(str, digit_list);
			if (count == 1)
				lower = atoi(str);
			else if (count == 2)
				upper = atoi(str);

			str += offset;
		} else {
			str++;
		}
	}

	if (count == 2) {
		*lower_hz = lower;
		*upper_hz = upper;
		return TEGRABL_NO_ERROR;
	} else {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
	}
}

static tegrabl_error_t parse_tmds_range(const void *fdt, int32_t node_offset,
		char *list_name, uint32_t *num_range_info,
		struct tmds_range_info **range_info)
{
	int subnode;
	const char **range_names;
	uint32_t range_num;
	uint32_t i, count = 0;
	uint32_t lower = 0, upper = 0;
	struct tmds_range_info *range;
	tegrabl_error_t ret = TEGRABL_NO_ERROR;

	ret = tegrabl_dt_get_prop_count_strings(fdt, node_offset, list_name, &range_num);
	if (ret != TEGRABL_NO_ERROR) {
		pr_debug("Error %d, Fail to retrieve the number of %s\n",
					ret, list_name);
		return ret;
	}

	range_names = tegrabl_malloc(range_num * sizeof(char *));
	if (!range_names) {
		pr_error("%s: Fail to alloc memory\n", __func__);
		ret = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 6);
		return ret;
	}

	ret = tegrabl_dt_get_prop_string_array(fdt, node_offset, list_name, range_names,
			NULL);
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("Error %d, Fail to retrieve the name list of %s\n",
				ret, list_name);
		goto err_get_range_names;
	}

	range = tegrabl_malloc(range_num * sizeof(struct tmds_range_info));
	if (!range) {
		pr_error("%s: Fail to alloc memory\n", __func__);
		ret = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 7);
		goto err_alloc_range;
	}

	for (i = 0; i < range_num; i++) {
		subnode = fdt_subnode_offset(fdt, node_offset, range_names[i]);
		if (subnode < 0) {
			pr_debug("HDMI prod-setting %s is not found\n",
					range_names[i]);
			continue;
		}

		if (!retrieve_range(range_names[i], &lower, &upper)) {
			if (upper < lower) {
				pr_debug("HDMI: invalid range in %s\n", range_names[i]);
				continue;
			}
		} else {
			pr_debug("HDMI: missing boundary in %s\n", range_names[i]);
			continue;
		}

		range[count].lower_hz = lower * 1000000;
		range[count].upper_hz = upper * 1000000;
		range[count].name = range_names[i];
		count++;
	}

	if (count >= 1) {
		*range_info = range;
		*num_range_info = count;
		tegrabl_free(range_names);
		pr_info("retrieved tmds range from %s\n", list_name);
		return ret;
	}

	ret = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 7);
	tegrabl_free(range);

err_alloc_range:
err_get_range_names:
	tegrabl_free(range_names);

	return ret;
}

static tegrabl_error_t parse_prod_list(const void *fdt, int32_t prod_offset,
	char *list_name, struct prod_pair **node_config, uint32_t *num_nodes)
{
	uint32_t i;
	uint32_t lowest;
	uint32_t num_range_info = 0;
	uint32_t sort_idx;
	struct prod_pair *tmds_config = NULL;
	struct tmds_range_info *range_info = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = parse_tmds_range(fdt, prod_offset, list_name, &num_range_info, &range_info);
	if (err != TEGRABL_NO_ERROR)
		goto fail;

	tmds_config = tegrabl_malloc(num_range_info * sizeof(struct prod_pair));
	if (!tmds_config) {
		pr_debug("%s: Fail to alloc memory\n", __func__);
		tegrabl_free(range_info);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 8);
		goto fail;
	}

	sort_idx = 0;
	/* Construct the tmds config in sort */
	while (sort_idx < num_range_info) {
		lowest = 0;
		for (i = 1; i < num_range_info; i++) {
			if (range_info[lowest].upper_hz > range_info[i].upper_hz)
				lowest = i;
		}

		tmds_config[sort_idx].clk = range_info[lowest].upper_hz;
		tmds_config[sort_idx].name = range_info[lowest].name;
		sort_idx++;
		/* Clear the lowest one */
		range_info[lowest].upper_hz = 0xffffffff;
	}

	*node_config = tmds_config;
	*num_nodes = num_range_info;
	tegrabl_free(range_info);
fail:
	return err;
}

tegrabl_error_t parse_hdmi_prod_settings(const void *fdt, int32_t prod_offset,
	struct prod_list **prod_list, struct prod_pair *legacy_node_config,
	uint32_t legacy_num_nodes)
{
	struct prod_pair *tmds_config = NULL;
	uint32_t tmds_num = 0;
	bool is_tmds_config_legacy_mode = true;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (fdt_subnode_offset(fdt, prod_offset, PROD_FORCE_LEGACY) < 0) {
		err = parse_prod_list(fdt, prod_offset, PROD_LIST_BOARD,
				&tmds_config, &tmds_num);
		if (err != TEGRABL_NO_ERROR)
			err = parse_prod_list(fdt, prod_offset, PROD_LIST_SOC,
					&tmds_config, &tmds_num);
		if (err == TEGRABL_NO_ERROR)
			is_tmds_config_legacy_mode = false;
	}

	if (is_tmds_config_legacy_mode) {
		pr_info("HDMI prod-setting fall back to legacy mode\n");
		tmds_num = legacy_num_nodes;
		tmds_config = legacy_node_config;
	}

	err = parse_prod_settings(fdt, prod_offset, prod_list, tmds_config, tmds_num);
	return err;
}
