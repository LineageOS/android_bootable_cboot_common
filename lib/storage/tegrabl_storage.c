/*
 * Copyright (c) 2017-2019, NVIDIA Corporation.  All rights reserved.
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
#include <tegrabl_clock.h>
#include <tegrabl_sata.h>
#include <tegrabl_ufs_bdev.h>
#include <tegrabl_soc_misc.h>

#if defined(CONFIG_ENABLE_SDCARD)
#include <tegrabl_gpio.h>
#include <tegrabl_sd_bdev.h>
#endif

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
						tegrabl_storage_type_t type, uint32_t instance,
						struct tegrabl_mb1bct_device_params *const dev_params,
						struct tegrabl_sd_platform_params *const sd_params,
						bool sdmmc_skip_init, bool ufs_reinit)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
#if defined(CONFIG_ENABLE_EMMC)
	struct tegrabl_sdmmc_platform_params sdmmc_params;
#endif
#if defined(CONFIG_ENABLE_QSPI)
	struct tegrabl_qspi_flash_platform_params qflash_params;
#endif
#if defined(CONFIG_ENABLE_UFS)
	struct tegrabl_ufs_platform_params ufs_params;
#endif
#if defined(CONFIG_ENABLE_SDCARD)
	bool is_sd_present = 0;
#else
	TEGRABL_UNUSED(sd_params);
#endif

	TEGRABL_UNUSED(ufs_reinit);

	switch (type) {

#if defined(CONFIG_ENABLE_EMMC)
	case TEGRABL_STORAGE_SDMMC_BOOT:
	case TEGRABL_STORAGE_SDMMC_USER:

		if (dev_params->emmc.clk_src == 0U) {
			/* fill the params */
			sdmmc_params.clk_src = TEGRABL_CLK_SRC_PLLP_OUT0;
			sdmmc_params.best_mode = TEGRABL_SDMMC_MODE_DDR52;
			sdmmc_params.tap_value = 9;
			sdmmc_params.trim_value = 5;
		} else {
			/* Copy parameter from device param to qspi_flash parameters */
			sdmmc_params.clk_src = dev_params->emmc.clk_src;
			sdmmc_params.best_mode = dev_params->emmc.best_mode;
			sdmmc_params.tap_value = dev_params->emmc.tap_value;
			sdmmc_params.trim_value = dev_params->emmc.trim_value;
		}
		sdmmc_params.is_skip_init = sdmmc_skip_init;
		err = sdmmc_bdev_open(instance, &sdmmc_params);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error opening sdmmc-%d\n", instance);
		}
		break;
#endif  /* CONFIG_ENABLE_EMMC */

#if defined(CONFIG_ENABLE_UFS)
	case TEGRABL_STORAGE_UFS:
		ufs_params.max_hs_mode = UFS_NO_HS_GEAR;
		ufs_params.max_pwm_mode = UFS_PWM_GEAR_4;
		ufs_params.max_active_lanes = UFS_TWO_LANES_ACTIVE;
		ufs_params.page_align_size = UFS_DEFAULT_PAGE_ALIGN_SIZE;
		ufs_params.enable_hs_modes = false;
		ufs_params.enable_fast_auto_mode = false;
		ufs_params.enable_hs_rate_b = false;
		ufs_params.enable_hs_rate_a = false;
		ufs_params.ufs_init_done = false;
		ufs_params.skip_hs_mode_switch = false;
		err = tegrabl_ufs_bdev_open(ufs_reinit, &ufs_params);
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

		if (dev_params->qspi.clk_div == 0U) {
			qflash_params.clk_src = TEGRABL_CLK_SRC_CLK_M;
			qflash_params.clk_div = 1;
			qflash_params.clk_src_freq = 0;
			qflash_params.interface_freq = 0U;
#if defined(CONFIG_ENABLE_QSPI_QDDR_READ)
			qflash_params.enable_ddr_read = true;
#else
			qflash_params.enable_ddr_read = false;
#endif
			qflash_params.max_bus_width = QSPI_BUS_WIDTH_X1;
			qflash_params.dma_type = DMA_GPC;
			qflash_params.fifo_access_mode = QSPI_MODE_DMA;
			qflash_params.read_dummy_cycles = EIGHT_CYCLES;
			qflash_params.trimmer1_val = 0;
			qflash_params.trimmer2_val = 0;
		} else {
			/* Copy parameter from device param to qspi_flash parameters */
			qflash_params.clk_src = dev_params->qspi.clk_src;
			qflash_params.clk_div = dev_params->qspi.clk_div;
			qflash_params.clk_src_freq = 0U;
			qflash_params.interface_freq = 0U;
			qflash_params.max_bus_width = dev_params->qspi.width;
#if defined(CONFIG_ENABLE_QSPI_QDDR_READ)
			qflash_params.enable_ddr_read = true;
#else
			qflash_params.enable_ddr_read = false;
#endif
			qflash_params.dma_type = dev_params->qspi.dma_type;
			qflash_params.fifo_access_mode = dev_params->qspi.xfer_mode;
			qflash_params.read_dummy_cycles = dev_params->qspi.read_dummy_cycles;
			qflash_params.trimmer1_val = dev_params->qspi.trimmer_val1;
			qflash_params.trimmer2_val = dev_params->qspi.trimmer_val2;
		}

		err = tegrabl_qspi_flash_open(0, &qflash_params);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Error opening qspi\n");
		}
		break;
#endif  /* CONFIG_ENABLE_QSPI */

#if defined(CONFIG_ENABLE_SATA)
	case TEGRABL_STORAGE_SATA:
		err = tegrabl_sata_bdev_open(instance, NULL, NULL);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Failed to open sata-%d\n", instance);
		}
		break;
#endif  /* CONFIG_ENABLE_SATA */

#if defined(CONFIG_ENABLE_SDCARD)
	case TEGRABL_STORAGE_SDCARD:
		if (sd_params == NULL) {
			pr_warn("SD param is NULL, missing SD card boot params.\n");
			break;
		}

		gpio_framework_init();
		err = tegrabl_gpio_driver_init();
		if (err != TEGRABL_NO_ERROR) {
			pr_error("GPIO driver init failed\n");
		} else {
			err = sd_bdev_is_card_present(&sd_params->cd_gpio, &is_sd_present);
			if (err != TEGRABL_NO_ERROR) {
				pr_warn("SD card detection error: %d.\n", err);
			} else if(!is_sd_present) {
				pr_warn("No SD-card is present!\n");
				err = TEGRABL_ERR_EMPTY;
			} else {
				err = sd_bdev_open(sd_params->sd_instance, sd_params);
				if (err != TEGRABL_NO_ERROR)
					pr_warn("Initialzing SD card failed: %d\n", err);
			}
		}
		break;
#endif

	default:
		pr_error("device: %u is not supported\n", type);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		break;
	}

	return err;
}

tegrabl_error_t tegrabl_storage_init_boot_dev(
						struct tegrabl_mb1bct_device_params *const dev_params,
						tegrabl_storage_type_t *const boot_dev,
						bool sdmmc_skip_init)
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

	err = tegrabl_storage_init_dev(*boot_dev, instance,
			dev_params, NULL, sdmmc_skip_init, false);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_storage_init_storage_devs(
						const struct tegrabl_device *const storage_devs,
						struct tegrabl_mb1bct_device_params *const dev_params,
						tegrabl_storage_type_t boot_dev,
						bool sdmmc_skip_init,
						bool ufs_reinit)
{
	tegrabl_storage_type_t dev_type;
	uint32_t instance;
	uint8_t i;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((storage_devs == NULL) || (dev_params == NULL)) {

		pr_error("Invalid storage devices or device params passed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	for (i = 0;
		 (storage_devs[i].type != (uint8_t)TEGRABL_MB1BCT_NONE) &&
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

		err = tegrabl_storage_init_dev(dev_type, instance, dev_params, NULL,
									   sdmmc_skip_init, ufs_reinit);
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
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_sdmmc_platform_params sdmmc_params;

	if (dev_params == NULL) {

		pr_error("Invalid device params passed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	if (dev_params->emmc.clk_src == 0U) {
		/* fill the params */
		sdmmc_params.clk_src = TEGRABL_CLK_SRC_PLLP_OUT0;
		sdmmc_params.best_mode = TEGRABL_SDMMC_MODE_DDR52;
		sdmmc_params.tap_value = 9;
		sdmmc_params.trim_value = 5;
	} else {
		/* Copy parameter from device param to qspi_flash parameters */
		sdmmc_params.clk_src = dev_params->emmc.clk_src;
		sdmmc_params.best_mode = dev_params->emmc.best_mode;
		sdmmc_params.tap_value = dev_params->emmc.tap_value;
		sdmmc_params.trim_value = dev_params->emmc.trim_value;
	}
	err = sdmmc_send_cmd0_cmd1(3, &sdmmc_params);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error sending cmd");
	}

fail:
	return err;
}

bool tegrabl_storage_is_storage_enabled(
							const struct tegrabl_device *storage_devs,
							tegrabl_storage_type_t dev_type,
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
