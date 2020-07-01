/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
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
#include <libfdt.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_display_dtb_hdmi.h>

tegrabl_error_t parse_hdmi_regulator_settings(const void *fdt,
	int32_t node_offset, struct tegrabl_display_pdata *pdata)
{
	uint32_t prop_val;
	const uint32_t *temp;

	temp = fdt_getprop(fdt, node_offset, "avdd_hdmi-supply", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		pdata->hdmi_dtb.avdd_hdmi_supply = prop_val;
		pr_debug("avdd_hdmi-supply %d\n", pdata->hdmi_dtb.avdd_hdmi_supply);
	} else {
		pdata->hdmi_dtb.avdd_hdmi_supply = 0;
		pr_error("no regulator info present for avdd_hdmi-supply\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
	}

	temp = fdt_getprop(fdt, node_offset, "avdd_hdmi_pll-supply", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		pdata->hdmi_dtb.avdd_hdmi_pll_supply = prop_val;
		pr_debug("avdd_hdmi_pll-supply %d\n",
				 pdata->hdmi_dtb.avdd_hdmi_pll_supply);
	} else {
		pdata->hdmi_dtb.avdd_hdmi_pll_supply = 0;
		pr_error("no regulator info present for avdd_hdmi_pll-supply\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 1);
	}

	temp = fdt_getprop(fdt, node_offset, "vdd_hdmi_5v0-supply", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		pdata->hdmi_dtb.vdd_hdmi_5v0_supply = prop_val;
		pr_debug("vdd_hdmi_5v0-supply %d\n",
				 pdata->hdmi_dtb.vdd_hdmi_5v0_supply);
	} else {
		pdata->hdmi_dtb.vdd_hdmi_5v0_supply = 0;
		pr_error("no regulator info present for vdd_hdmi_5v0-supply\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 2);
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t parse_hpd_gpio(const void *fdt, int32_t sor_offset,
							   struct tegrabl_display_pdata *pdata)
{
	const char *property_name;
	const struct fdt_property *property;
	const char *gpio;
	int32_t hdmi_prop_offset;
	const uint32_t *polarity;

	property_name = "nvidia,hpd-gpio";
	property = fdt_get_property(fdt, sor_offset, property_name, NULL);
	if (!property) {
		pr_error("error in getting property %s\n", property_name);
		goto fail;
	}
	gpio = property->data + 4;
	pdata->hdmi_dtb.hpd_gpio = fdt32_to_cpu(*(uint32_t *)gpio);

	hdmi_prop_offset = fdt_subnode_offset(fdt, sor_offset, "hdmi-display");
	if (hdmi_prop_offset < 0) {
		pr_error("hdmi-display subnode not found\n");
		goto fail;
	}
	hdmi_prop_offset = fdt_subnode_offset(fdt, hdmi_prop_offset,
										  "disp-default-out");
	if (hdmi_prop_offset < 0) {
		pr_error("disp-default-out subnode not found\n");
		goto fail;
	}
	property = fdt_get_property(fdt, hdmi_prop_offset,
								"nvidia,out-flags", NULL);
	if (!property) {
		pr_error("error in getting \"nvidia,out-flags\" property\n");
		goto fail;
	}
	polarity = property->data32;
	pdata->hdmi_dtb.polarity = fdt32_to_cpu(*(uint32_t *)polarity);

	return TEGRABL_NO_ERROR;

fail:
	return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 13);
}
