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

tegrabl_error_t parse_dp_dtb_settings(const void *fdt, int32_t offset, struct tegrabl_display_dp_dtb *dp_dtb)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int32_t dp_lt_data_offset = -1;
	int32_t dp_lt_setting_offset = -1;
	int32_t temp_offset = -1;
	const struct fdt_property *property;
	uint32_t temp[10];
	const uint32_t *val;
	uint32_t i, j, k, m, n;

	property = fdt_get_property(fdt, offset, "nvidia,lanes", NULL);
	if (property != NULL) {
		dp_dtb->lanes = fdt32_to_cpu(*(property->data32));
		pr_debug("dp_dtb->lanes = %d\n", dp_dtb->lanes);
	} else {
		dp_dtb->lanes = 0;
		pr_warn("nvidia,lanes property not found\n");
	}

	property = fdt_get_property(fdt, offset, "nvidia,link_bw", NULL);
	if (property != NULL) {
		dp_dtb->link_bw = fdt32_to_cpu(*(property->data32));
		pr_debug("dp_dtb->link_bw = %d\n", dp_dtb->link_bw);
	} else {
		dp_dtb->link_bw = 0;
		pr_warn("nvidia,link_bw property not found\n");
	}

	property = fdt_get_property(fdt, offset, "nvidia,pc2-disabled", NULL);
	if (property != NULL) {
		dp_dtb->pc2_disabled = true;
		pr_debug("dp_dtb->pc2_disabled = %d\n", dp_dtb->pc2_disabled);
	} else {
		dp_dtb->pc2_disabled = false;
		pr_warn("nvidia,pc2-disabled property not found\n");
	}

	/********************* parsing lt-settings***************************/
	dp_lt_setting_offset = fdt_subnode_offset(fdt, offset, "dp-lt-settings");
	if (dp_lt_setting_offset < 0) {
		pr_debug("dp-lt-settings node not found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 11);
		goto lt_data;
	}

	dp_dtb->lt_settings = tegrabl_malloc(ARRAY_SIZE(lt_settings_name) * sizeof(struct dp_lt_settings));
	if (dp_dtb->lt_settings == NULL) {
		pr_warn("%s: memory allocation failed\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 4);
		goto lt_data;
	}

	for (i = 0; i < ARRAY_SIZE(lt_settings_name); i++) {
		temp_offset = fdt_subnode_offset(fdt, dp_lt_setting_offset, lt_settings_name[i]);
		if (temp_offset < 0) {
			pr_debug("%s node not found\n", lt_settings_name[i]);
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 12);
			tegrabl_free(dp_dtb->lt_settings);
			goto lt_data;
		}
		property = fdt_get_property(fdt, temp_offset, "nvidia,drive-current", NULL);
		if (property != NULL) {
			for (j = 0; j < VOLTAGE_SWING_COUNT; j++) {
				val = property->data32 + j;
				dp_dtb->lt_settings[i].drive_current[j] = fdt32_to_cpu(*val);
			}
		} else {
			pr_warn("error in getting drive_current property offset\n");
		}

		property = fdt_get_property(fdt, temp_offset, "nvidia,lane-preemphasis", NULL);
		if (property != NULL) {
			for (j = 0; j < PRE_EMPHASIS_COUNT; j++) {
				val = property->data32 + j;
				dp_dtb->lt_settings[i].pre_emphasis[j] = fdt32_to_cpu(*val);
			}
		} else {
			pr_warn("error in getting pre_emphasis property offset\n");
		}

		property = fdt_get_property(fdt, temp_offset, "nvidia,post-cursor", NULL);
		if (property != NULL) {
			for (j = 0; j < POST_CURSOR2_COUNT; j++) {
				val = property->data32 + j;
				dp_dtb->lt_settings[i].post_cursor2[j] = fdt32_to_cpu(*val);
			}
		} else {
			pr_warn("error in getting post_cursor2 property offset\n");
		}

		property = fdt_get_property(fdt, temp_offset, "nvidia,tx-pu", NULL);
		if (property != NULL) {
			dp_dtb->lt_settings[i].tx_pu = fdt32_to_cpu(*(property->data32));
		} else {
			pr_warn("error in getting drive current property offset\n");
		}
		property = fdt_get_property(fdt, temp_offset, "nvidia,load-adj", NULL);
		if (property != NULL) {
			dp_dtb->lt_settings[i].load_adj = fdt32_to_cpu(*(property->data32));
		} else {
			pr_warn("error in getting load_adj property offset\n");
		}
	}
	pr_debug("%s: DP lt-settings parsed successfully\n", __func__);

lt_data:
	/******************** parsing lt-data *****************************/
	dp_lt_data_offset = fdt_subnode_offset(fdt, offset, "lt-data");
	if (dp_lt_data_offset < 0) {
		pr_debug("dp-lt-data node not found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 9);
		goto fail;
	}

	dp_dtb->lt_data = tegrabl_malloc(ARRAY_SIZE(lt_data_name) * sizeof(struct dp_lt_data));
	if (dp_dtb->lt_data == NULL) {
		pr_warn("%s: memory allocation failed\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 3);
		goto fail;
	}

	for (i = 0; i < ARRAY_SIZE(lt_data_name); i++) {
		strncpy(dp_dtb->lt_data[i].name, lt_data_name[i], sizeof(dp_dtb->lt_data[i].name));

		temp_offset = fdt_subnode_offset(fdt, dp_lt_data_offset, lt_data_name[i]);
		if (temp_offset < 0) {
			pr_debug("%s node not found\n", lt_data_name[i]);
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 10);
			tegrabl_free(dp_dtb->lt_data);
			goto fail;
		}

		for (j = 0; j < ARRAY_SIZE(lt_data_child_name); j++) {
			k = 0;
			memset(temp, 0, sizeof(temp));
			property = fdt_get_property(fdt, temp_offset, lt_data_child_name[j], NULL);
			if (property != NULL) {
				for (k = 0; k < ARRAY_SIZE(temp); k++)
					temp[k] = fdt32_to_cpu(*(property->data32 + k));
			} else {
				pr_debug("dp-lt-data child node not found\n");
				err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 2);
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

fail:
	return err;
}

tegrabl_error_t parse_dp_regulator_settings(const void *fdt, int32_t offset,
	struct tegrabl_display_dp_dtb *pdata_dp)
{
	const uint32_t *temp;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	temp = fdt_getprop(fdt, offset, "vdd-dp-pwr-supply", NULL);
	if (temp != NULL) {
		pdata_dp->vdd_dp_pwr_supply = fdt32_to_cpu(*temp);
		pr_debug("vdd_dp_pwr_supply 0x%x\n", pdata_dp->vdd_dp_pwr_supply);
	} else {
		pdata_dp->vdd_dp_pwr_supply = -1;
		pr_debug("no regulator info present for vdd_dp_pwr_supply\n");
	}

	temp = fdt_getprop(fdt, offset, "avdd-dp-pll-supply", NULL);
	if (temp != NULL) {
		pdata_dp->avdd_dp_pll_supply = fdt32_to_cpu(*temp);
		pr_debug("avdd_dp_pll_supply 0x%x\n", pdata_dp->avdd_dp_pll_supply);
	} else {
		pdata_dp->avdd_dp_pll_supply = -1;
		pr_debug("no regulator info present for avdd_dp_pll_supply\n");
	}

	temp = fdt_getprop(fdt, offset, "vdd-dp-pad-supply", NULL);
	if (temp != NULL) {
		pdata_dp->vdd_dp_pad_supply = fdt32_to_cpu(*temp);
		pr_debug("vdd_dp_pad_supply 0x%x\n", pdata_dp->vdd_dp_pad_supply);
	} else {
		pdata_dp->vdd_dp_pad_supply = -1;
		pr_debug("no regulator info present for vdd_dp_pad_supply\n");
	}

	temp = fdt_getprop(fdt, offset, "vdd_hdmi_5v0-supply", NULL);
	if (temp != NULL) {
		pdata_dp->dp_hdmi_5v0_supply = fdt32_to_cpu(*temp);
		pr_debug("dp_hdmi_5v0-supply 0x%x\n", pdata_dp->dp_hdmi_5v0_supply);
	} else {
		pdata_dp->dp_hdmi_5v0_supply = -1;
		pr_error("no regulator info present for vdd_hdmi_5v0-supply\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 14);
	}

	return err;
}

tegrabl_error_t parse_edp_regulator_settings(const void *fdt, int32_t offset,
	struct tegrabl_display_dp_dtb *pdata_dp)
{
	const uint32_t *temp;

	temp = fdt_getprop(fdt, offset, "dvdd_lcd-supply", NULL);
	if (temp != NULL) {
		pdata_dp->dvdd_lcd_supply = fdt32_to_cpu(*temp);
		pr_debug("dvdd_lcd_supply 0x%x\n", pdata_dp->dvdd_lcd_supply);
	} else {
		pdata_dp->dvdd_lcd_supply = -1;
		pr_debug("no regulator info present for vdd_dp_pwr_supply\n");
	}

	temp = fdt_getprop(fdt, offset, "avdd_lcd-supply", NULL);
	if (temp != NULL) {
		pdata_dp->avdd_lcd_supply = fdt32_to_cpu(*temp);
		pr_debug("avdd_lcd_supply 0x%x\n", pdata_dp->avdd_lcd_supply);
	} else {
		pdata_dp->avdd_lcd_supply = -1;
		pr_debug("no regulator info present for avdd_lcd_supply\n");
	}

	temp = fdt_getprop(fdt, offset, "vdd_lcd_bl_en-supply", NULL);
	if (temp != NULL) {
		pdata_dp->vdd_lcd_bl_en_supply = fdt32_to_cpu(*temp);
		pr_debug("vdd_lcd_bl_en_supply 0x%x\n", pdata_dp->vdd_lcd_bl_en_supply);
	} else {
		pdata_dp->vdd_lcd_bl_en_supply = -1;
		pr_debug("no regulator info present for vdd_lcd_bl_en_supply\n");
	}

	return TEGRABL_NO_ERROR;
}

