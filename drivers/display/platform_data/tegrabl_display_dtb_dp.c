/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
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
#include <libfdt.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_display_dtb_dp.h>

static const char * const lt_settings_name[] = {
	"lt-setting@0",
	"lt-setting@1",
	"lt-setting@2",
};

static const char * const lt_data_name[] = {
	"tegra-dp-vs-regs",
	"tegra-dp-pe-regs",
	"tegra-dp-pc-regs",
	"tegra-dp-tx-pu",
};

static const char * const lt_data_child_name[] = {
	"pc2_l0",
	"pc2_l1",
	"pc2_l2",
	"pc2_l3",
};

tegrabl_error_t parse_dp_dtb_settings(const void *fdt, int32_t offset,
									  struct tegrabl_display_dp_dtb *dp_dtb)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int32_t dp_offset = -1;
	int32_t dp_lt_data_offset = -1;
	int32_t dp_lt_setting_offset = -1;
	int32_t temp_offset = -1;
	const struct fdt_property *property;
	uint32_t temp[10];
	const uint32_t *val;
	uint32_t i, j, k, m, n;

	dp_offset = fdt_subnode_offset(fdt, offset, "dp-display");
	if (dp_offset < 0) {
		pr_debug("dp-display node not found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 8);
		goto fail;
	}

	property = fdt_get_property(fdt, dp_offset, "nvidia,is_ext_dp_panel", NULL);
	if (property != NULL) {
		dp_dtb->is_ext_dp_panel = fdt32_to_cpu(*(property->data));
	} else {
		pr_error("nvidia,is_ext_dp_panel property not found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	/******************** parsing lt-data *****************************/
	dp_lt_data_offset = fdt_subnode_offset(fdt, dp_offset, "lt-data");
	if (dp_lt_data_offset < 0) {
		pr_debug("dp-lt-data node not found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 9);
		goto fail;
	}

	dp_dtb->lt_data = tegrabl_malloc(ARRAY_SIZE(lt_data_name) *
									 sizeof(struct dp_lt_data));
	if (dp_dtb->lt_data == NULL) {
		pr_error("%s: memory allocation failed\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 3);
		goto fail;
	}

	for (i = 0; i < ARRAY_SIZE(lt_data_name); i++) {
		memset(dp_dtb->lt_data[i].name, 0, sizeof(dp_dtb->lt_data[i].name));
		strcpy(dp_dtb->lt_data[i].name, lt_data_name[i]);

		temp_offset = fdt_subnode_offset(fdt, dp_lt_data_offset,
										 lt_data_name[i]);
		if (temp_offset < 0) {
			pr_debug("%s node not found\n", lt_data_name[i]);
			err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 10);
			goto fail;
		}

		for (j = 0; j < ARRAY_SIZE(lt_data_child_name); j++) {
			k = 0;
			memset(temp, 0, sizeof(temp));
			property = fdt_get_property(fdt, temp_offset, lt_data_child_name[j],
										NULL);
			if (property != NULL) {
				for (k = 0; k < ARRAY_SIZE(temp); k++)
					temp[k] = fdt32_to_cpu(*(property->data32 + k));
			} else {
				pr_debug("dp-lt-data child node not found\n");
				err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 1);
				tegrabl_free(dp_dtb->lt_data);
				goto fail;
			}

			k = 0;
			for (m = 0; m < 4; m++) {
				for (n = 0; n < 4-m; n++) {
					dp_dtb->lt_data[i].data[j][m][n] = temp[k++];
				}
			}
		}
	}

	pr_debug("%s: DP lt-data parsed successfully\n", __func__);
	/********************* parsing lt-settings***************************/
	dp_lt_setting_offset = fdt_subnode_offset(fdt, dp_offset, "dp-lt-settings");
	if (dp_lt_setting_offset < 0) {
		pr_debug("dp-lt-settings node not found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 11);
		goto fail;
	}

	dp_dtb->lt_settings = tegrabl_malloc(ARRAY_SIZE(lt_settings_name) *
										 sizeof(struct dp_lt_settings));
	if (dp_dtb->lt_settings == NULL) {
		pr_error("%s: memory allocation failed\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 5);
		goto fail;
	}

	for (i = 0; i < ARRAY_SIZE(lt_settings_name); i++) {
		temp_offset = fdt_subnode_offset(fdt, dp_lt_setting_offset,
										 lt_settings_name[i]);
		if (temp_offset < 0) {
			pr_debug("%s node not found\n", lt_settings_name[i]);
			err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 12);
			tegrabl_free(dp_dtb->lt_data);
			tegrabl_free(dp_dtb->lt_settings);
			goto fail;
		}
		property = fdt_get_property(fdt, temp_offset,
									"nvidia,drive-current", NULL);
		if (property != NULL) {
			for (j = 0; j < VOLTAGE_SWING_COUNT; j++) {
				val = property->data32 + j;
				dp_dtb->lt_settings[i].drive_current[j] = fdt32_to_cpu(*val);
			}
		} else {
			pr_warn("error in getting drive_current property offset\n");
		}

		property = fdt_get_property(fdt, temp_offset,
									"nvidia,lane-preemphasis", NULL);
		if (property != NULL) {
			for (j = 0; j < PRE_EMPHASIS_COUNT; j++) {
				val = property->data32 + j;
				dp_dtb->lt_settings[i].pre_emphasis[j] = fdt32_to_cpu(*val);
			}
		} else {
			pr_warn("error in getting pre_emphasis property offset\n");
		}

		property = fdt_get_property(fdt, temp_offset, "nvidia,post-cursor",
									NULL);
		if (property != NULL) {
			for (j = 0; j < POST_CURSOR2_COUNT; j++) {
				val = property->data32 + j;
				dp_dtb->lt_settings[i].post_cursor2[j] = fdt32_to_cpu(*val);
			}
		} else {
			pr_warn("error in getting post_cursor2 property offset\n");
		}

		property = fdt_get_property(fdt, temp_offset, "nvidia,tx-pu", NULL);
		if (property != NULL)
			dp_dtb->lt_settings[i].tx_pu = fdt32_to_cpu(*(property->data32));
		else
			pr_warn("error in getting drive current property offset\n");

		property = fdt_get_property(fdt, temp_offset, "nvidia,load-adj", NULL);
		if (property != NULL)
			dp_dtb->lt_settings[i].load_adj = fdt32_to_cpu(*(property->data32));
		else
			pr_warn("error in getting load_adj property offset\n");
	}
	pr_debug("%s: DP lt-settings parsed successfully\n", __func__);

fail:
	return err;
}

tegrabl_error_t parse_dp_regulator_settings(const void *fdt,
	int32_t node_offset, struct tegrabl_display_pdata *pdata)
{
	uint32_t prop_val;
	const uint32_t *temp;

	temp = fdt_getprop(fdt, node_offset, "vdd-dp-pwr-supply", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		pdata->dp_dtb.vdd_dp_pwr_supply = prop_val;
		pr_debug("vdd_dp_pwr_supply %d\n", pdata->dp_dtb.vdd_dp_pwr_supply);
	} else {
		pdata->dp_dtb.vdd_dp_pwr_supply = -1;
		pr_debug("no regulator info present for vdd_dp_pwr_supply\n");
	}

	temp = fdt_getprop(fdt, node_offset, "avdd-dp-pll-supply", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		pdata->dp_dtb.avdd_dp_pll_supply = prop_val;
		pr_debug("avdd_dp_pll_supply %d\n", pdata->dp_dtb.avdd_dp_pll_supply);
	} else {
		pdata->dp_dtb.avdd_dp_pll_supply = -1;
		pr_debug("no regulator info present for avdd_dp_pll_supply\n");
	}

	temp = fdt_getprop(fdt, node_offset, "vdd-dp-pad-supply", NULL);
	if (temp != NULL) {
		prop_val = fdt32_to_cpu(*temp);
		pdata->dp_dtb.vdd_dp_pad_supply = prop_val;
		pr_debug("vdd_dp_pad_supply %d\n", pdata->dp_dtb.vdd_dp_pad_supply);
	} else {
		pdata->dp_dtb.vdd_dp_pad_supply = -1;
		pr_debug("no regulator info present for vdd_dp_pad_supply\n");
	}

	return TEGRABL_NO_ERROR;
}
