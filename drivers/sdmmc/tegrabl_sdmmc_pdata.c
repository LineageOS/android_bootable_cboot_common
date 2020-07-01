/*
 * Copyright (c) 2016, NVIDIA Corporation.  All rights reserved.
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
#include <tegrabl_sdmmc_pdata.h>

char *sdmmc_nodes[4] = {
	"sdhci0",
	"sdhci1",
	"sdhci2",
	"sdhci3",
};

tegrabl_error_t tegrabl_sd_get_instance(uint32_t *instance,
	struct gpio_info *cd_gpio)
{
	uint32_t i;
	int32_t offset;
	void *fdt = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	const char *temp;
	const uint32_t *data;
	int32_t len;
	const char *name;

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get bl-dtb handle\n");
		goto fail;
	}

	err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	for (i = 0; i < ARRAY_SIZE(sdmmc_nodes); i++) {
		name = fdt_get_alias(fdt, sdmmc_nodes[i]);
		offset = fdt_path_offset(fdt, name);
		if (offset < 0) {
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 1);
			pr_debug("%s: error while finding sdmmc node\n", __func__);
			goto fail;
		}

		/* if node does not have status=ok, skip then try next node */
		temp = fdt_getprop(fdt, offset, "status", NULL);
		if (temp != NULL) {
			pr_debug("sdmmc node status = %s\n", temp);
			if (strcmp(temp, "okay")) {
				pr_debug("sdmmc node status is disabled\n");
				continue;
			}
		}

		/* if node has property named non-removable, skip then try next node */
		temp = fdt_getprop(fdt, offset, "non-removable", NULL);
		if (temp != NULL) {
			pr_debug("sdmmc node is non-removable\n");
			continue;
		}

		*instance = i;
		pr_info("sdmmc instance for sdcard = %d\n", i);

		data = fdt_getprop(fdt, offset, "cd-gpios", &len);
		if ((data == NULL) || (len < 0))
			continue;

		if (len != 12)
			continue;

		cd_gpio->handle = fdt32_to_cpu(data[0]);
		cd_gpio->pin = fdt32_to_cpu(data[1]);
		cd_gpio->flags = fdt32_to_cpu(data[2]);
		pr_debug("sdcard cd gpio handle 0x%x\n", cd_gpio->handle);
		pr_debug("sdcard cd gpi o pin 0x%x\n", cd_gpio->pin);
		pr_debug("sdcard cd gpio flags 0x%x\n", cd_gpio->flags);
		err = TEGRABL_NO_ERROR;
		break;
	}

fail:
	return err;
}

