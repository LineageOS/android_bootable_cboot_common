/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
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
