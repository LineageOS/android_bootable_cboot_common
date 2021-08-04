/*
 * Copyright (c) 2016-2021, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors errain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications theerro.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited
 */

#define MODULE TEGRABL_ERR_SDMMC

#include <stdint.h>
#include <libfdt.h>
#include <tegrabl_devicetree.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_sd_pdata.h>

#define SDMMC_MAX_INSTANCES 4

char *sdmmc_aliases[2] = {
	"sdhci",
	"mmc",
};

tegrabl_error_t tegrabl_sd_get_platform_params(uint32_t *instance, struct tegrabl_sd_platform_params *params)
{
	uint32_t i, j;
	int32_t offset;
	void *fdt = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	const char *temp;
	const uint32_t *data;
	int32_t len;
	const char *name;
	char alias[10];

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get bl-dtb handle\n");
		goto fail;
	}

	for (j = 0; j < ARRAY_SIZE(sdmmc_aliases); j++) {
		for (i = 0; i < SDMMC_MAX_INSTANCES; i++) {
			tegrabl_snprintf(alias, sizeof(alias), "%s%d", sdmmc_aliases[j], i);
			name = fdt_get_alias(fdt, alias);
			if (name == NULL) {
				continue;
			}
			offset = fdt_path_offset(fdt, name);
			if (offset < 0) {
				err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 1);
				pr_error("Error while finding sdmmc node\n");
				goto fail;
			}

			/* if node does not have status=ok, skip then try next node */
			temp = fdt_getprop(fdt, offset, "status", NULL);
			if (temp != NULL) {
				pr_trace("sdmmc node status = %s\n", temp);
				if (strcmp(temp, "okay")) {
					continue;
				}
			}

			/* if node has property named non-removable, skip then try next node */
			temp = fdt_getprop(fdt, offset, "non-removable", NULL);
			if (temp != NULL) {
				pr_debug("sdmmc node is non-removable\n");
				continue;
			}

			data = fdt_getprop(fdt, offset, "cd-gpios", &len);
			if ((data == NULL) || (len < 0))
				continue;

			if (len != 12)
				continue;

			pr_trace("sdcard instance = %d\n", i);
			*instance = i;
			goto found;
		}
	}

found:
	if (i == SDMMC_MAX_INSTANCES) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	params->cd_gpio.handle = fdt32_to_cpu(data[0]);
	params->cd_gpio.pin = fdt32_to_cpu(data[1]);
	params->cd_gpio.flags = fdt32_to_cpu(data[2]);

	/*
	 * CBoot interprets the GPIO as being active-low if bit 0 is 0
	 * and active-high is bit 0 is 1. However, device-tree defines
	 * active-low as 1 and active-high as 0. Therefore, if the
	 * property 'cd-inverted' is not present, toggle bit 0 so that
	 * CBoot identifies the polarity correctly.
	 */
	temp = fdt_getprop(fdt, offset, "cd-inverted", NULL);
	if (temp != NULL) {
		pr_debug("sdmmc cd-inverted\n");
	} else {
		params->cd_gpio.flags = params->cd_gpio.flags ^ 0x1;
	}

	pr_debug("sdcard gpio handle 0x%x\n", params->cd_gpio.handle);
	pr_debug("sdcard gpio pin 0x%x\n", params->cd_gpio.pin);
	pr_debug("sdcard gpio flags 0x%x\n", params->cd_gpio.flags);

	data = fdt_getprop(fdt, offset, "vmmc-supply", NULL);
	if (data != NULL) {
		params->vmmc_supply = fdt32_to_cpu(*data);
		pr_trace("vmmc-supply 0x%x\n", params->vmmc_supply);
		err = TEGRABL_NO_ERROR;
	} else {
		params->vmmc_supply = 0;
		pr_error("no regulator info present for vmmc-supply\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
	}

fail:
	return err;
}
