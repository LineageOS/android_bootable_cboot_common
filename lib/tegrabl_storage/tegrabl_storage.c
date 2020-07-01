/*
 * Copyright (c) 2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_STORAGE

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_storage.h>
#include <tegrabl_sdmmc_bdev.h>
#include <tegrabl_qspi.h>
#include <tegrabl_qspi_flash.h>
#include <tegrabl_sata.h>
#include <tegrabl_ufs_bdev.h>
#include <tegrabl_soc_misc.h>


const tegrabl_storage_type_t map_to_storage_type_from_mb1_bct_type[] = {

	[TEGRABL_MB1BCT_SDMMC_BOOT] = TEGRABL_STORAGE_SDMMC_BOOT,
	[TEGRABL_MB1BCT_SDMMC_USER] = TEGRABL_STORAGE_SDMMC_USER,
	[TEGRABL_MB1BCT_QSPI]       = TEGRABL_STORAGE_QSPI_FLASH,
	[TEGRABL_MB1BCT_SATA]       = TEGRABL_STORAGE_SATA,
	[TEGRABL_MB1BCT_UFS]        = TEGRABL_STORAGE_UFS
};


tegrabl_storage_type_t tegrabl_storage_map_to_storage_dev_from_mb1bct_dev(
									tegrabl_mb1_bct_boot_device_t mb1bct_dev)
{
	return map_to_storage_type_from_mb1_bct_type[mb1bct_dev];
}

tegrabl_error_t tegrabl_storage_init_dev(
								tegrabl_storage_type_t type,
								uint32_t instance,
								struct tegrabl_mb1bct_device_params *dev_params,
								enum sdmmc_init_flag sdmmc_init,
								bool ufs_reinit)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(ufs_reinit);

	switch (type) {

#if defined(CONFIG_ENABLE_EMMC)
	case TEGRABL_STORAGE_SDMMC_BOOT:
	case TEGRABL_STORAGE_SDMMC_USER:

		if (dev_params->emmc.clk_src == 0) {
			/* fill the params */
			dev_params->emmc.clk_src = TEGRABL_CLK_SRC_PLLP_OUT0;
			dev_params->emmc.best_mode = TEGRABL_SDMMC_MODE_DDR52;
			dev_params->emmc.tap_value = 9;
			dev_params->emmc.trim_value = 5;
			dev_params->emmc.instance = 3;
		}

		err = sdmmc_bdev_open(&dev_params->emmc, sdmmc_init);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error opening sdmmc-%d\n", instance);
		}
		break;
#endif  /* CONFIG_ENABLE_EMMC */

#if defined(CONFIG_ENABLE_UFS)
	case TEGRABL_STORAGE_UFS:
		err = tegrabl_ufs_bdev_open(ufs_reinit);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error opening ufs\n");
		}
		break;

/* WAR to tackle UFS init failure in MB2 */
#else
	case TEGRABL_STORAGE_UFS:
		break;
#endif

#if defined(CONFIG_ENABLE_QSPI)
	case TEGRABL_STORAGE_QSPI_FLASH:
		if (dev_params->qspi.clk_div == 0) {
			dev_params->qspi.clk_src = TEGRABL_CLK_SRC_CLK_M;
			dev_params->qspi.clk_div = 1;
			dev_params->qspi.width = QSPI_BUS_WIDTH_X1;
			dev_params->qspi.dma_type = DMA_GPC;
			dev_params->qspi.xfer_mode = QSPI_MODE_DMA;
			dev_params->qspi.read_dummy_cycles = EIGHT_CYCLES;
			dev_params->qspi.trimmer_val1 = 0;
			dev_params->qspi.trimmer_val2 = 0;
		}

		err = tegrabl_qspi_flash_open(&dev_params->qspi);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error opening qspi\n");
		}
		break;
#endif  /* CONFIG_ENABLE_QSPI */

#if defined(CONFIG_ENABLE_SATA)
	case TEGRABL_STORAGE_SATA:
		err = tegrabl_sata_bdev_open(instance, NULL);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Failed to open sata-%d\n", instance);
		}
		break;
#endif  /* CONFIG_ENABLE_SATA */

	default:
		pr_error("device: %u is not supported\n", type);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		break;
	}

	return err;
}

tegrabl_error_t tegrabl_storage_init_boot_dev(
								struct tegrabl_mb1bct_device_params *dev_params,
								tegrabl_storage_type_t *const boot_dev,
								enum sdmmc_init_flag sdmmc_init)
{
	uint32_t instance;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (dev_params == NULL) {

		pr_error("Invalid device params passed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	err = tegrabl_soc_get_bootdev(boot_dev, &instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get boot device information\n");
		goto fail;
	}

	err = tegrabl_storage_init_dev(*boot_dev, instance, dev_params, sdmmc_init,
								   false);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_storage_init_storage_devs(
								const struct tegrabl_device *storage_devs,
								struct tegrabl_mb1bct_device_params *dev_params,
								tegrabl_storage_type_t boot_dev,
								enum sdmmc_init_flag sdmmc_init,
								bool ufs_reinit)
{
	tegrabl_storage_type_t dev_type;
	uint32_t instance;
	uint8_t i;
	tegrabl_error_t err;

	if ((storage_devs == NULL) || (dev_params == NULL)) {

		pr_error("Invalid storage devices or device params passed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	for (i = 0;
		 (storage_devs[i].type != TEGRABL_MB1BCT_NONE) &&
			(i < TEGRABL_MAX_STORAGE_DEVICES);
		 i++) {

		dev_type = map_to_storage_type_from_mb1_bct_type[storage_devs[i].type];
		instance = storage_devs[i].instance;

		/* Skip boot device initialization */
		if (dev_type == boot_dev) {
			continue;
		}

		/* Skip UFS initialization as storage dev if odm data isn't available */
		if ((dev_type == TEGRABL_STORAGE_UFS) &&
			(tegrabl_is_ufs_enable() == false)) {

			pr_debug("Not initializing UFS because odm data isn't available\n");
			continue;
		}

		err = tegrabl_storage_init_dev(dev_type, instance, dev_params,
									   sdmmc_init, ufs_reinit);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_storage_partial_sdmmc_init(
								struct tegrabl_mb1bct_device_params *dev_params)
{
	tegrabl_error_t err;

	if (dev_params == NULL) {

		pr_error("Invalid device params passed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	if (dev_params->emmc.clk_src == 0) {

		dev_params->emmc.clk_src = TEGRABL_CLK_SRC_PLLP_OUT0;
		dev_params->emmc.instance = 3;
		dev_params->emmc.best_mode = TEGRABL_SDMMC_MODE_DDR52;
		dev_params->emmc.tap_value = 9;
		dev_params->emmc.trim_value = 5;
	}

	err = sdmmc_send_cmd0_cmd1(&dev_params->emmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error sending cmd");
	}

fail:
	return err;
}

bool tegrabl_storage_is_storage_enabled(
							const struct tegrabl_device *storage_devs,
							const tegrabl_storage_type_t dev_type,
							const uint32_t instance)
{
	uint8_t i;
	bool is_dev_found = false;
	tegrabl_storage_type_t s_dev_type;
	uint32_t s_instance = 0;

	if (storage_devs == NULL) {
		pr_error("Invalid storage devices passed\n");
		goto fail;
	}

	for (i = 0;
		(storage_devs[i].type != TEGRABL_MB1BCT_NONE) &&
			(i < TEGRABL_MAX_STORAGE_DEVICES);
		i++) {

		s_dev_type = map_to_storage_type_from_mb1_bct_type[storage_devs[i].type];
		s_instance = storage_devs[i].instance;

		if ((dev_type == s_dev_type) && (instance == s_instance)) {
			is_dev_found = true;
			break;
		}
	}
fail:

	return is_dev_found;
}
