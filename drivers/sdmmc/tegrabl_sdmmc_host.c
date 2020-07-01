/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_SDMMC

#include <string.h>
#include <stdint.h>
#include <tegrabl_ar_macro.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_sdmmc_defs.h>
#include <tegrabl_sdmmc_protocol.h>
#include <tegrabl_sdmmc_bdev.h>
#include <tegrabl_sdmmc_host.h>
#include <tegrabl_clock.h>
#include <tegrabl_timer.h>
#include <tegrabl_io.h>
#include <arsdmmcab.h>
#include <arapb_misc_gp.h>
#include <tegrabl_drf.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_timer.h>

/*  Defines the macro for reading from various offsets of sdmmc base controller.
 */
#define sdmmc_readl(hsdmmc, reg) \
	NV_READ32((hsdmmc)->base_addr + (uint32_t)(SDMMCAB_##reg##_0));

/*  Defines the macro for writing to various offsets of sdmmc base controller.
 */
#define sdmmc_writel(hsdmmc, reg, value) \
	NV_WRITE32(((hsdmmc)->base_addr + (uint32_t)(SDMMCAB_##reg##_0)), value);

/** @brief Wait till the internal clock is stable.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_wait_clk_stable(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg;
	uint32_t clk_ready;
	uint32_t timeout = TIME_OUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Wait till the clock is stable. */
	while (timeout != 0U) {
		reg = sdmmc_readl(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
		clk_ready = NV_DRF_VAL(SDMMCAB, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
						INTERNAL_CLOCK_STABLE, reg);
		if (clk_ready != 0U) {
			break;
		}
		tegrabl_udelay(1);
		timeout--;
		if (timeout == 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			pr_error("CLK stable time out\n");
			goto fail;
		}
	}
fail:
	return error;
}

/** @brief Wait for the command to be completed.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param cmd_reg Command register to decide timeout.
 *  @param arg Argument to decide timeout.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_wait_command_complete(struct tegrabl_sdmmc *hsdmmc,
	uint32_t cmd_reg, uint32_t arg)
{
	uint32_t cmd_cmplt;
	uint32_t int_status;
	uint32_t timeout = COMMAND_TIMEOUT_IN_US;
	sdmmc_cmd cmd_index;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Prepare the interrupt error mask. */
	uint32_t err_mask =
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_INDEX_ERR, ERR) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_END_BIT_ERR,
			END_BIT_ERR_GENERATED) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_CRC_ERR,
			CRC_ERR_GENERATED) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_TIMEOUT_ERR, TIMEOUT);

	cmd_index = NV_DRF_VAL(SDMMCAB, CMD_XFER_MODE, COMMAND_INDEX, cmd_reg);

	/* Change timeout for erase command. */
	if ((cmd_index == CMD_ERASE) ||
		((cmd_index == CMD_SWITCH) && (arg == SWITCH_SANITIZE_ARG))) {
		timeout = hsdmmc->erase_timeout_us;
	}
	/* Wait for command complete. */
	while (timeout != 0U) {
		int_status = sdmmc_readl(hsdmmc, INTERRUPT_STATUS);

		cmd_cmplt = NV_DRF_VAL(SDMMCAB, INTERRUPT_STATUS, CMD_COMPLETE,
			int_status);

		if ((int_status & err_mask) != 0U) {
			pr_error("Error in command_complete %x int_status\n", int_status);
			error =  TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 0);
			goto fail;
		}
		if (cmd_cmplt != 0U) {
			break;
		}
		tegrabl_udelay(1);
		timeout--;

		if (timeout == 0U) {
			error =  TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 1);
			goto fail;
		}
	}

fail:
	return error;
}

/** @brief Send the abort command with required arguments.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_abort_command(struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t retries = 2;
	uint32_t cmd_reg;
	uint32_t int_status;
	uint32_t *sdmmc_response = &hsdmmc->response[0];

	pr_trace("Sending Abort CMD%d\n", CMD_STOP_TRANSMISSION);

	cmd_reg =
		NV_DRF_NUM(SDMMCAB, CMD_XFER_MODE, COMMAND_INDEX,
			CMD_STOP_TRANSMISSION) |
		NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, COMMAND_TYPE, ABORT) |
		NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, DATA_PRESENT_SELECT,
			NO_DATA_TRANSFER) |
		NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, CMD_INDEX_CHECK_EN, ENABLE) |
		NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, CMD_CRC_CHECK_EN, ENABLE) |
		NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, RESP_TYPE_SELECT,
			RESP_LENGTH_48BUSY) |
		NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, DATA_XFER_DIR_SEL, WRITE) |
		NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, BLOCK_COUNT_EN, DISABLE) |
		NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, DMA_EN, DISABLE);

	while (retries != 0U) {
		/* Clear Status bits what ever is set. */
		int_status = sdmmc_readl(hsdmmc, INTERRUPT_STATUS);
		sdmmc_writel(hsdmmc, INTERRUPT_STATUS, int_status);
		/* This redundant read is for debug purpose. */
		int_status = sdmmc_readl(hsdmmc, INTERRUPT_STATUS);
		sdmmc_writel(hsdmmc, ARGUMENT, 0);
		sdmmc_writel(hsdmmc, CMD_XFER_MODE, cmd_reg);
		/* Wait for the command to be sent out.if it fails, retry. */
		if (sdmmc_wait_command_complete(hsdmmc, cmd_reg, 0) == 0U) {
			break;
		}
		error = sdmmc_init_controller(hsdmmc, hsdmmc->controller_id);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		retries--;
	}
	if (retries != 0U) {
		/* Wait till response is received from card. */
		error = sdmmc_cmd_txr_ready(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		/* Wait till busy line is deasserted by card. It is for R1b response. */
		error = sdmmc_data_txr_ready(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			pr_error("Data not in txr mode\n");
			goto fail;
		}
		sdmmc_read_response(hsdmmc, RESP_TYPE_R1B, sdmmc_response);
	}
fail:
	return error;
}

/**
* @brief Enable/disable sdmmc card clock
*
* @param hsdmmc Context information to determine the base
*                 address of controller
* @param enable Gives whether to enable/disable
*/
static void sdmmc_card_clock_enable(struct tegrabl_sdmmc *hsdmmc,
	bool enable)
{
	uint32_t reg;
	uint32_t arg;

	if (enable) {
		arg = 1U;
	} else {
		arg = 0U;
	}

	reg = sdmmc_readl(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
							 SD_CLOCK_EN, arg, reg);
	sdmmc_writel(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);
	return;
}

static tegrabl_error_t sdmmc_enable_hs400(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t host_reg = 0;
	uint32_t misc_reg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t rate;
	uint8_t instance;

	pr_trace("Enable high speed for HS400 mode\n");

	if ((hsdmmc->card_support_speed & ECSD_CT_HS400_180_MASK) != ECSD_CT_HS400_180) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 3);
		goto fail;
	}

	error = sdmmc_enable_timing_hs400(hsdmmc, TEGRABL_SDMMC_MODE_DDR52);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	hsdmmc->enhanced_strobe = true;
	error = sdmmc_set_bus_width(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = sdmmc_enable_timing_hs400(hsdmmc, TEGRABL_SDMMC_MODE_HS400);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = tegrabl_car_init_pll_with_rate(TEGRABL_CLK_PLL_ID_PLLC4, 0U, NULL);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	instance = (uint8_t)hsdmmc->controller_id;

	error = tegrabl_car_clk_disable(TEGRABL_MODULE_SDMMC, instance);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = tegrabl_car_set_clk_src(TEGRABL_MODULE_SDMMC, instance, TEGRABL_CLK_SRC_PLLC4_OUT0_LJ);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = tegrabl_car_set_clk_rate(TEGRABL_MODULE_SDMMC, instance, 200000, &rate);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	tegrabl_udelay(2);

	error = tegrabl_car_clk_enable(TEGRABL_MODULE_SDMMC, instance, NULL);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = sdmmc_set_card_clock(hsdmmc, MODE_DATA_TRANSFER, 0);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* reset SD clock enable */
	sdmmc_card_clock_enable(hsdmmc, false);

	/* set HS400 UHS Mode */
	host_reg = sdmmc_readl(hsdmmc, AUTO_CMD12_ERR_STATUS);
	host_reg = NV_FLD_SET_DRF_DEF(SDMMCAB, AUTO_CMD12_ERR_STATUS, UHS_MODE_SEL, HS400, host_reg);
	sdmmc_writel(hsdmmc, AUTO_CMD12_ERR_STATUS, host_reg);

	/* set enable SD clock */
	sdmmc_card_clock_enable(hsdmmc, true);

	/* enable storbe bit */
	misc_reg = sdmmc_readl(hsdmmc, VENDOR_SYS_SW_CNTRL);
	misc_reg = NV_FLD_SET_DRF_NUM(SDMMCAB, VENDOR_SYS_SW_CNTRL, ENHANCED_STROBE_MODE, 1, misc_reg);
	sdmmc_writel(hsdmmc, VENDOR_SYS_SW_CNTRL, misc_reg);

	/* run dll caliberation */
	error = sdmmc_dll_caliberation(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* read ext csd register */
	error = sdmmc_get_ext_csd(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	pr_debug("sdmmc HS400 mode enabled\n");

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("sdmmc HS400 enable failed\n");
	}
	return error;
}

/*  @brief Enable ddr mode operation for read/write.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_enable_ddr_mode(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t host_reg = 0;
	uint32_t cap_reg = 0;
	uint32_t cap_high_reg = 0;
	uint32_t misc_reg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t ddr50;
	uint32_t voltage_support_1_8_v;
	uint32_t high_speed_support;

	if ((hsdmmc->card_support_speed & ECSD_CT_HS_DDR_52_180_300_MASK)
			== ECSD_CT_HS_DDR_52_180_300) {
		/* set HS_TIMING to 1 before setting the ddr mode data width.. */
		hsdmmc->high_speed_mode = 1;

		pr_trace("Enable high speed for DDR mode\n");

		error = sdmmc_enable_high_speed(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* When SPARE[8] is set DDR50 support is advertised in */
		/* CAPABILITIES_HIGER_0_DDR50 */

		misc_reg = sdmmc_readl(hsdmmc, VENDOR_MISC_CNTRL);

		misc_reg = NV_FLD_SET_DRF_NUM(SDMMCAB, VENDOR_MISC_CNTRL, SDMMC_SPARE0,
			0x100, misc_reg);

		sdmmc_writel(hsdmmc, VENDOR_MISC_CNTRL, misc_reg);

		pr_trace("set bus width for DDR mode\n");
		error = sdmmc_set_bus_width(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/*set the ddr mode in Host controller and other misc things.. */
		/* DDR support is available by fuse bit */
		/* check capabilities register for Ddr support */
		/* read capabilities and capabilities higher reg */
		cap_reg = sdmmc_readl(hsdmmc, CAPABILITIES);
		cap_high_reg = sdmmc_readl(hsdmmc, CAPABILITIES_HIGHER);

		ddr50 = NV_DRF_VAL(SDMMCAB, CAPABILITIES_HIGHER, DDR50, cap_high_reg);
		voltage_support_1_8_v = NV_DRF_VAL(SDMMCAB, CAPABILITIES,
										   VOLTAGE_SUPPORT_1_8_V, cap_reg);
		high_speed_support = NV_DRF_VAL(SDMMCAB, CAPABILITIES, HIGH_SPEED_SUPPORT,
										cap_reg);

		if ((ddr50 != 0U) && (voltage_support_1_8_v != 0U) &&
			(high_speed_support != 0U)) {
			/* reset SD clock enable */
			sdmmc_card_clock_enable(hsdmmc, false);

			/* set DDR50 UHS Mode */
			host_reg = sdmmc_readl(hsdmmc, AUTO_CMD12_ERR_STATUS);
			host_reg = NV_FLD_SET_DRF_DEF(SDMMCAB, AUTO_CMD12_ERR_STATUS,
				UHS_MODE_SEL, DDR50, host_reg);
			sdmmc_writel(hsdmmc, AUTO_CMD12_ERR_STATUS, host_reg);

			/* set enable SD clock */
			sdmmc_card_clock_enable(hsdmmc, true);
		}
		pr_debug("sdmmc DDR50 mode enabled\n");
	} else if ((hsdmmc->card_support_speed &
				ECSD_CT_HS_DDR_52_120_MASK) ==
			ECSD_CT_HS_DDR_52_120) {
		pr_debug("Not supported for DDR 1.2V\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		goto fail;
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 1);
		pr_error("Unknown DDR operation\n");
		goto fail;
	}

fail:
	return error;
}

/** @brief Resets all the registers of the controller.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_reset_controller(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg;
	uint32_t reset_in_progress;
	uint32_t timeout = TIME_OUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 25);
		goto fail;
	}

	/* Reset Controller's All reg's. */
	reg = NV_DRF_DEF(SDMMCAB, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
		SW_RESET_FOR_ALL, RESETED);
	sdmmc_writel(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);

	/* Wait till Reset is completed. */
	while (timeout != 0U) {
		reg = sdmmc_readl(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);

		reset_in_progress = NV_DRF_VAL(SDMMCAB,
			SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, SW_RESET_FOR_ALL, reg);

		if (reset_in_progress == 0U) {
			break;
		}

		tegrabl_udelay(1);
		timeout--;

		if (timeout == 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 2);
			goto fail;
		}
	}

fail:
	return error;
}

/**
* @brief Enables Host Controller V4 to use 64 bit addr dma
*
* @param hsdmmc Context information to determine the base
*                 address of controller.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t sdmmc_enable_hostv4(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg;

	reg = sdmmc_readl(hsdmmc, AUTO_CMD12_ERR_STATUS);
	reg = NV_FLD_SET_DRF_DEF(SDMMCAB, AUTO_CMD12_ERR_STATUS, HOST_VERSION_4_EN,
			ENABLE, reg);
	reg = NV_FLD_SET_DRF_DEF(SDMMCAB, AUTO_CMD12_ERR_STATUS,
			ADDRESSING_64BIT_EN, ENABLE, reg);
	sdmmc_writel(hsdmmc, AUTO_CMD12_ERR_STATUS, reg);

	hsdmmc->is_hostv4_enabled = true;
	return TEGRABL_NO_ERROR;
}

/**
* @brief Get the status of hostv4 whether it is enabled or not
*	and store in sdmmc_hsdmmc
*
* @param hsdmmc Context information to determine the base
*                 address of controller.
*
*/
void sdmmc_get_hostv4_status(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg_data;

	reg_data = sdmmc_readl((hsdmmc), AUTO_CMD12_ERR_STATUS);

	reg_data = NV_DRF_VAL(SDMMCAB, AUTO_CMD12_ERR_STATUS, HOST_VERSION_4_EN,
						  reg_data);

	hsdmmc->is_hostv4_enabled = (reg_data ==
		SDMMCAB_AUTO_CMD12_ERR_STATUS_0_HOST_VERSION_4_EN_ENABLE) ? true : false;
}

/** @brief sets the internal clock for the card from controller.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param mode Different mode like init, power on & data transfer.
 *  @param clk_divider Appropriate divider for generating card clock.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_set_card_clock(struct tegrabl_sdmmc *hsdmmc,
	sdmmc_mode_t mode, uint32_t clk_divider)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t reg = 0;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 26);
		goto fail;
	}

	switch (mode) {
	/* Set Clock below 400 KHz. */
	case MODE_POWERON:
		pr_trace("Clock set for power on of card\n");
		reg = NV_DRF_DEF(SDMMCAB, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
				INTERNAL_CLOCK_EN, OSCILLATE) |
			NV_DRF_DEF(SDMMCAB, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
				SDCLK_FREQUENCYSELECT, DIV256);

		sdmmc_writel(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);

		error = sdmmc_wait_clk_stable(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;

	/* Set clock as requested by user. */
	case MODE_INIT:
	case MODE_DATA_TRANSFER:
		pr_trace("Clock set for init or data_transfer\n");

		sdmmc_card_clock_enable(hsdmmc, false);
		reg = sdmmc_readl(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
		reg = NV_FLD_SET_DRF_NUM(SDMMCAB, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
			SDCLK_FREQUENCYSELECT, clk_divider, reg);
		sdmmc_writel(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);

		error = sdmmc_wait_clk_stable(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		reg = sdmmc_readl(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
		reg = NV_FLD_SET_DRF_NUM(SDMMCAB, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
				DATA_TIMEOUT_COUNTER_VALUE, 0xE, reg);
		sdmmc_writel(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);
		sdmmc_card_clock_enable(hsdmmc, true);
		break;
	default:
		break;
	}
fail:
	return error;
}

/** @brief Sets the voltage in power_control_host according to capabilities.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_enable_bus_power(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg = 0;
	uint32_t cap_reg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 27);
		goto fail;
	}

	cap_reg = sdmmc_readl(hsdmmc, CAPABILITIES);

	/* Read the voltage supported by the card. */
	pr_trace("Set the correct voltage range\n");
	if (NV_DRF_VAL(SDMMCAB, CAPABILITIES, VOLTAGE_SUPPORT_3_3_V, cap_reg) != 0U) {
		reg |=
			NV_DRF_DEF(SDMMCAB, POWER_CONTROL_HOST, SD_BUS_VOLTAGE_SELECT, V3_3);
	} else if (NV_DRF_VAL(SDMMCAB, CAPABILITIES,
				VOLTAGE_SUPPORT_3_0_V, cap_reg) != 0U) {
		reg |=
			NV_DRF_DEF(SDMMCAB, POWER_CONTROL_HOST, SD_BUS_VOLTAGE_SELECT, V3_0);
	} else {
		reg |=
			NV_DRF_DEF(SDMMCAB, POWER_CONTROL_HOST, SD_BUS_VOLTAGE_SELECT, V1_8);
	}
	/* Enable bus power. */
	reg |= NV_DRF_DEF(SDMMCAB, POWER_CONTROL_HOST, SD_BUS_POWER, POWER_ON);

	sdmmc_writel(hsdmmc, POWER_CONTROL_HOST, reg);

fail:
	return error;
}

/** @brief Sets the interrupt error mask.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return Void.
 */
tegrabl_error_t sdmmc_set_interrupt_status_reg(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 28);
		goto fail;
	}

	reg =
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, DATA_END_BIT_ERR, ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, DATA_CRC_ERR, ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, DATA_TIMEOUT_ERR, ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, COMMAND_INDEX_ERR, ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, COMMAND_END_BIT_ERR,
			ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, COMMAND_CRC_ERR, ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, COMMAND_TIMEOUT_ERR,
			ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, CARD_REMOVAL, ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, CARD_INSERTION, ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, DMA_INTERRUPT, ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, TRANSFER_COMPLETE, ENABLE) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS_ENABLE, COMMAND_COMPLETE, ENABLE);

	/* Poll for the above interrupts. */
	pr_trace("Setup error mask for interrupt\n");
	sdmmc_writel(hsdmmc, INTERRUPT_STATUS_ENABLE, reg);

fail:
	return error;
}

/** @brief Checks if card is stable and present or not.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_is_card_present(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t card_stable;
	uint32_t reg;
	uint32_t card_inserted = 0;
	uint32_t timeout = TIME_OUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 29);
		goto fail;
	}

	/* Check if the card is present or not */
	while (timeout != 0U) {
		reg = sdmmc_readl(hsdmmc, PRESENT_STATE);
		card_stable = NV_DRF_VAL(SDMMCAB, PRESENT_STATE, CARD_STATE_STABLE, reg);
		if (card_stable != 0U) {
			card_inserted = NV_DRF_VAL(SDMMCAB, PRESENT_STATE, CARD_INSERTED,
				reg);
			break;
		}
		tegrabl_udelay(1);
		timeout--;
	}

	if (card_stable == 0U) {
		pr_debug("Card is not stable\n");
	}
	if (card_inserted == 0U) {
		pr_debug("Card is not inserted\n");
	}
	error = (card_inserted != 0U) ? TEGRABL_NO_ERROR :
				TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
fail:
	return error;
}

/** @brief Prepare cmd register to be send in command send.
 *
 *  @param cmd_reg Command register send by command send.
 *  @param data_cmd Configure cmd_reg for data transfer
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param index Index of the command being send.
 *  @param resp_type Response type of the command.
 */
tegrabl_error_t sdmmc_prepare_cmd_reg(uint32_t *cmd_reg, uint8_t data_cmd,
	struct tegrabl_sdmmc *hsdmmc, sdmmc_cmd index, sdmmc_resp_type resp_type)
{
	uint32_t reg = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((hsdmmc == NULL) || (cmd_reg == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 30);
		goto fail;
	}

	/* Basic argument preparation. */
	reg =
		NV_DRF_NUM(SDMMCAB, CMD_XFER_MODE, COMMAND_INDEX, index) |
		NV_DRF_NUM(SDMMCAB, CMD_XFER_MODE, DATA_PRESENT_SELECT,
			((data_cmd != 0U) ? 1 : 0)) |
		NV_DRF_NUM(SDMMCAB, CMD_XFER_MODE, BLOCK_COUNT_EN,
				   ((data_cmd != 0U) ? 1 : 0)) |
		NV_DRF_NUM(SDMMCAB, CMD_XFER_MODE, DMA_EN, ((data_cmd != 0U) ? 1 : 0));

	/* Enable multiple block select. */
	if ((index == CMD_READ_MULTIPLE) || (index == CMD_WRITE_MULTIPLE)) {
		reg |= NV_DRF_NUM(SDMMCAB, CMD_XFER_MODE, MULTI_BLOCK_SELECT , 1) |
				NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, AUTO_CMD12_EN, CMD12);
	}

	/* Select data direction for write. */
	if ((index == CMD_WRITE_MULTIPLE) && (data_cmd != 0U)) {
		reg |= NV_DRF_NUM(SDMMCAB, CMD_XFER_MODE, DATA_XFER_DIR_SEL, 0);
	} else if (data_cmd != 0U) {
		reg |= NV_DRF_NUM(SDMMCAB, CMD_XFER_MODE, DATA_XFER_DIR_SEL, 1);
	} else {
		/* No Action Required */
	}

	/* Cmd index check. */
	if ((resp_type != RESP_TYPE_NO_RESP) &&
		(resp_type != RESP_TYPE_R2) &&
		(resp_type != RESP_TYPE_R3) &&
		(resp_type != RESP_TYPE_R4)) {
		reg |= NV_DRF_NUM(SDMMCAB, CMD_XFER_MODE, CMD_INDEX_CHECK_EN, 1);
	}

	/* Crc index check. */
	if ((resp_type != RESP_TYPE_NO_RESP) &&
		(resp_type != RESP_TYPE_R3) &&
		(resp_type != RESP_TYPE_R4)) {
		reg |= NV_DRF_NUM(SDMMCAB, CMD_XFER_MODE, CMD_CRC_CHECK_EN, 1);
	}

	/* Response type check. */
	if (resp_type == RESP_TYPE_NO_RESP) {
		reg |= NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, RESP_TYPE_SELECT,
				NO_RESPONSE);
	} else if (resp_type == RESP_TYPE_R2) {
		reg |= NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, RESP_TYPE_SELECT,
				RESP_LENGTH_136);
	} else if (resp_type == RESP_TYPE_R1B) {
		reg |= NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, RESP_TYPE_SELECT,
				RESP_LENGTH_48BUSY);
	} else {
		reg |= NV_DRF_DEF(SDMMCAB, CMD_XFER_MODE, RESP_TYPE_SELECT,
				RESP_LENGTH_48);
	}
	*cmd_reg = reg;
fail:
	return error;
}

/** @brief Sends command by writing cmd_reg, arg & int_status.
 *
 *  @param cmd_reg Command register send by command send.
 *  @param arg Argument for the command to be send.
 *  @param data_cmd Configure cmd_reg for data transfer
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_try_send_command(uint32_t cmd_reg, uint32_t arg,
	uint8_t data_cmd, struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t trials = 3;
	uint32_t int_status;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 31);
		goto fail;
	}

	while (trials != 0U) {
		/* Clear Status bits what ever is set. */
		int_status = sdmmc_readl(hsdmmc, INTERRUPT_STATUS);
		sdmmc_writel(hsdmmc, INTERRUPT_STATUS, int_status);

		/* This redundant read is for debug purpose. */
		int_status = sdmmc_readl(hsdmmc, INTERRUPT_STATUS);
		sdmmc_writel(hsdmmc, ARGUMENT, arg);
		sdmmc_writel(hsdmmc, CMD_XFER_MODE, cmd_reg);

		/* Wait for the command to be sent out. If it fails, retry. */
		if (sdmmc_wait_command_complete(hsdmmc, cmd_reg, arg) == 0U) {
			break;
		}
		/* Recover Controller from Errors. */
		error = sdmmc_recover_controller_error(hsdmmc, data_cmd);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		trials--;
	}

	if (trials != 0U) {
		error = TEGRABL_NO_ERROR;
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 1);
	}
fail:
	return error;
}

/** @brief Check if next command can be send or not.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_cmd_txr_ready(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg;
	uint32_t cmd_txr_ready;
	uint32_t timeout = COMMAND_TIMEOUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 32);
		goto fail;
	}

	/* Check if sending command is allowed or not. */
	while (timeout != 0U) {
		reg = sdmmc_readl(hsdmmc, PRESENT_STATE);
		/* This bit is set to zero after response is received. So, response */
		/* registers should be read only after this bit is cleared. */
		cmd_txr_ready = NV_DRF_VAL(SDMMCAB, PRESENT_STATE, CMD_INHIBIT_CMD, reg);

		if (cmd_txr_ready == 0U) {
			break;
		}

		tegrabl_udelay(1);
		timeout--;
		if (timeout == 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 3);
			goto fail;
		}
	}
fail:
	return error;
}

/** @brief Check if data can be send or not over data lines.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if data lines is free.
 */
tegrabl_error_t sdmmc_data_txr_ready(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg;
	uint32_t data_txr_ready;
	uint32_t timeout = DATA_TIMEOUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 33);
		goto fail;
	}

	/* Check if sending data is allowed or not. */
	while (timeout != 0U) {
		reg = sdmmc_readl(hsdmmc, PRESENT_STATE);
		/* This bit is set to zero after response is received. So, response */
		/* registers should be read only after this bit is cleared. */
		data_txr_ready = NV_DRF_VAL(SDMMCAB, PRESENT_STATE, CMD_INHIBIT_DAT,
			reg);

		if (data_txr_ready == 0U) {
			break;
		}

		tegrabl_udelay(1);
		timeout--;

		if (timeout == 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 4);
			goto fail;
		}
	}

fail:
	return error;
}

/** @brief Read response of last command in local buffer.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param resp_type Response type of the command.
 *  @param buf Buffer in which response will be read.
 */
tegrabl_error_t sdmmc_read_response(struct tegrabl_sdmmc *hsdmmc,
	sdmmc_resp_type resp_type, uint32_t *buf)
{
	uint32_t *temp = buf;
	uint32_t i;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((hsdmmc == NULL) || (buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 34);
		goto fail;
	}

	/* read the response of the last command send */
	switch (resp_type) {
	case RESP_TYPE_R1:
	case RESP_TYPE_R1B:
	case RESP_TYPE_R3:
	case RESP_TYPE_R4:
	case RESP_TYPE_R5:
	case RESP_TYPE_R6:
	case RESP_TYPE_R7:
		/* bits 39:8 of response are mapped to 31:0. */
		*temp = sdmmc_readl(hsdmmc, RESPONSE_R0_R1);
		pr_trace("%08X\n", buf[0]);
		break;
	case RESP_TYPE_R2:
		/* bits 127:8 of response are mapped to 119:0. */
		*temp = sdmmc_readl(hsdmmc, RESPONSE_R0_R1);
		temp++;
		*temp = sdmmc_readl(hsdmmc, RESPONSE_R2_R3);
		temp++;
		*temp = sdmmc_readl(hsdmmc, RESPONSE_R4_R5);
		temp++;
		*temp = sdmmc_readl(hsdmmc, RESPONSE_R6_R7);
		for (i = 0; i < 4U; i++) {
			pr_trace("%08X\n", buf[i]);
		}
		break;
	case RESP_TYPE_NO_RESP:
	default:
		*temp = 0;
		break;
	}
fail:
	return error;
}

/** @brief Try to recover the controller from the error occured.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param data_cmd if data_cmd then send abort command.
 */
tegrabl_error_t sdmmc_recover_controller_error(struct tegrabl_sdmmc *hsdmmc,
	uint8_t data_cmd)
{
	uint32_t reg;
	uint32_t present_state;
	uint32_t reset_progress;
	uint32_t int_status;
	uint32_t timeout = TIME_OUT_IN_US;
	uint32_t cmd_error;
	uint32_t data_error;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 35);
		goto fail;
	}

	/* Prepare command error mask. */
	cmd_error =
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_INDEX_ERR, ERR) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_END_BIT_ERR,
			END_BIT_ERR_GENERATED) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_CRC_ERR,
			CRC_ERR_GENERATED) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_TIMEOUT_ERR, TIMEOUT);

	data_error =
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, DATA_END_BIT_ERR, ERR) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, DATA_CRC_ERR, ERR) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, DATA_TIMEOUT_ERR, TIMEOUT);

	int_status = sdmmc_readl(hsdmmc, INTERRUPT_STATUS);
	reg = sdmmc_readl(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);

	if ((int_status & cmd_error) != 0U) {
		/* Reset Command line. */
		reg |= NV_DRF_DEF(SDMMCAB, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
			SW_RESET_FOR_CMD_LINE, RESETED);
		sdmmc_writel(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);
		/* Wait till Reset is completed. */
		while (timeout != 0U) {
			reg = sdmmc_readl(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
			reset_progress = NV_DRF_VAL(SDMMCAB,
				SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, SW_RESET_FOR_CMD_LINE,
				reg);
			if (reset_progress == 0U) {
				break;
			}
			tegrabl_udelay(1);
			timeout--;
		}
		if (timeout == 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 5);
			goto fail;
		}
	}
	if ((int_status & data_error) != 0U) {
		/* Reset Data line. */
		reg |= NV_DRF_DEF(SDMMCAB, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
			SW_RESET_FOR_DAT_LINE, RESETED);
		sdmmc_writel(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);
		/* Wait till Reset is completed. */
		while (timeout != 0U) {
			reg = sdmmc_readl(hsdmmc, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
			reset_progress = NV_DRF_VAL(SDMMCAB,
				SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, SW_RESET_FOR_DAT_LINE,
				reg);
			if (reset_progress == 0U) {
				break;
			}
			tegrabl_udelay(1);
			timeout--;
		}
		if (timeout == 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 6);
			goto fail;
		}
	}
	/* Clear Interrupt Status. */
	sdmmc_writel(hsdmmc, INTERRUPT_STATUS, int_status);

	/* Issue abort command. */
	if (data_cmd != 0U) {
		(void)sdmmc_abort_command(hsdmmc);
	}

	/* Wait for 40us as per spec. */
	tegrabl_udelay(40);

	/* Read Present State register. */
	present_state = sdmmc_readl(hsdmmc, PRESENT_STATE);
	if (NV_DRF_VAL(SDMMCAB, PRESENT_STATE,
			DAT_3_0_LINE_LEVEL, present_state) != 0U) {
		/* Before give up, try full reset once. */
		sdmmc_init_controller(hsdmmc, hsdmmc->controller_id);
		present_state = sdmmc_readl(hsdmmc, PRESENT_STATE);
		if (NV_DRF_VAL(SDMMCAB, PRESENT_STATE, DAT_3_0_LINE_LEVEL,
				present_state) != 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 7);
			goto fail;
		}
	}
fail:
	return error;
}

/** @brief Set the data width for data lines
 *
 *  @param width Data width to be configured.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 */
tegrabl_error_t sdmmc_set_data_width(sdmmc_data_width width,
	struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 36);
		goto fail;
	}
	reg = sdmmc_readl(hsdmmc, POWER_CONTROL_HOST);
	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, POWER_CONTROL_HOST, DATA_XFER_WIDTH, width,
		reg);
	/* When 8-bit data width is enabled, the bit field DATA_XFER_WIDTH */
	/* value is not valid. */
	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, POWER_CONTROL_HOST,
		EXTENDED_DATA_TRANSFER_WIDTH,
		(width == DATA_WIDTH_8BIT) || (width == DATA_WIDTH_DDR_8BIT) ? 1 : 0,
		reg);
	sdmmc_writel(hsdmmc, POWER_CONTROL_HOST, reg);

fail:
	return error;
}

/** @brief Set the number of blocks to read or write.
 *
 *  @param block_size The block size being used for transfer.
 *  @param num_blocks Numbers of block to read/write.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 */
void sdmmc_set_num_blocks(uint32_t block_size, uint32_t num_blocks,
	struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg;

	/*
	* DMA512K: This makes controller halt when ever it detects 512KB boundary.
	* When controller halts on this boundary, need to clear the
	* dma block boundary event and write SDMA base address again.
	* Writing address again triggers controller to continue.
	* We can't disable this. We have to live with it.
	*/
	reg = NV_DRF_NUM(SDMMCAB, BLOCK_SIZE_BLOCK_COUNT, BLOCKS_COUNT,
			num_blocks) |
			NV_DRF_DEF(SDMMCAB, BLOCK_SIZE_BLOCK_COUNT,
				HOST_DMA_BUFFER_SIZE, DMA512K) |
			NV_DRF_NUM(SDMMCAB, BLOCK_SIZE_BLOCK_COUNT,
				XFER_BLOCK_SIZE_11_0, block_size);

	sdmmc_writel(hsdmmc, BLOCK_SIZE_BLOCK_COUNT, reg);
}

/** @brief Writes the buffer start for read/write.
 *
 *  @param buf Input buffer whose address is registered.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 */
void sdmmc_setup_dma(dma_addr_t buf, struct tegrabl_sdmmc *hsdmmc)
{
	if (hsdmmc->is_hostv4_enabled == false) {
		sdmmc_writel(hsdmmc, SYSTEM_ADDRESS, (uintptr_t)buf);
	}
#if defined(CONFIG_ENABLE_SDMMC_64_BIT_SUPPORT)
	else {
		sdmmc_writel(hsdmmc, ADMA_SYSTEM_ADDRESS, (uint32_t) buf);
		sdmmc_writel(hsdmmc, UPPER_ADMA_SYSTEM_ADDRESS, (uint32_t)(buf >> 32));
	}
#endif
}

/** @brief checks if card is in transfer state or not and perform various
 *         operations according to the mode of operation.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return Status of the device
 */
sdmmc_device_status sdmmc_query_status(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t sdma_address;
	uint32_t transfer_done = 0;
	uint32_t intr_status;
	uint32_t error_mask;
	uint32_t dma_boundary_interrupt;
	uint32_t data_timeout_error;

	error_mask =
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, DATA_END_BIT_ERR, ERR) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, DATA_CRC_ERR, ERR) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, DATA_TIMEOUT_ERR, TIMEOUT) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_INDEX_ERR, ERR) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_END_BIT_ERR,
			END_BIT_ERR_GENERATED) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_CRC_ERR,
			CRC_ERR_GENERATED) |
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, COMMAND_TIMEOUT_ERR, TIMEOUT);

	dma_boundary_interrupt =
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, DMA_INTERRUPT, GEN_INT);

	data_timeout_error =
		NV_DRF_DEF(SDMMCAB, INTERRUPT_STATUS, DATA_TIMEOUT_ERR, TIMEOUT);

	if (hsdmmc->device_status == DEVICE_STATUS_IO_PROGRESS) {
		/* Check whether Transfer is done. */
		intr_status = sdmmc_readl(hsdmmc, INTERRUPT_STATUS);

		transfer_done =
			NV_DRF_VAL(SDMMCAB, INTERRUPT_STATUS, XFER_COMPLETE, intr_status);
		/* Check whether there are any errors. */
		if ((intr_status & error_mask) != 0U) {
			if ((intr_status & error_mask) == data_timeout_error) {
				hsdmmc->device_status = DEVICE_STATUS_DATA_TIMEOUT;
			} else {
				hsdmmc->device_status = DEVICE_STATUS_CRC_FAILURE;
			}
			/* Recover from errors here. */
			(void)sdmmc_recover_controller_error(hsdmmc, 1);
		} else if ((intr_status & dma_boundary_interrupt) != 0U) {
			/* Need to clear DMA boundary interrupt and write SDMA address */
			/* again. Otherwise controller doesn't go ahead. */
			sdmmc_writel(hsdmmc, INTERRUPT_STATUS, dma_boundary_interrupt);
			if (hsdmmc->is_hostv4_enabled == false) {
				sdma_address = sdmmc_readl(hsdmmc, SYSTEM_ADDRESS);
				sdmmc_writel(hsdmmc, SYSTEM_ADDRESS, sdma_address);
			}
#if defined(CONFIG_ENABLE_SDMMC_64_BIT_SUPPORT)
			else {
				sdma_address = sdmmc_readl(hsdmmc, UPPER_ADMA_SYSTEM_ADDRESS);
				sdmmc_writel(hsdmmc, UPPER_ADMA_SYSTEM_ADDRESS, sdma_address);
				sdma_address = sdmmc_readl(hsdmmc, ADMA_SYSTEM_ADDRESS);
				sdmmc_writel(hsdmmc, ADMA_SYSTEM_ADDRESS, sdma_address);
			}
#endif
		} else if (transfer_done != 0U) {
			hsdmmc->device_status = DEVICE_STATUS_IDLE;
			sdmmc_writel(hsdmmc, INTERRUPT_STATUS, intr_status);
		} else if ((uint32_t)(tegrabl_get_timestamp_ms() -
						(hsdmmc->read_start_time)) > DATA_TIMEOUT_IN_US) {
			hsdmmc->device_status = DEVICE_STATUS_IO_FAILURE;
		} else {
			/* No Action Required */
		}
	}
	return hsdmmc->device_status;
}

/** @brief Use to enable/disable high speed mode according to the mode of
 *         Operation.
 *
 *  @param enable Enable High Speed mode.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 */
void sdmmc_toggle_high_speed(uint8_t enable,
	struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg = 0;

	pr_trace("Toggle high speed bit\n");
	reg = sdmmc_readl(hsdmmc, POWER_CONTROL_HOST);
	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, POWER_CONTROL_HOST, HIGH_SPEED_EN,
			((enable == 1U) ? 1 : 0), reg);
	sdmmc_writel(hsdmmc, POWER_CONTROL_HOST, reg);
}
/** @brief Select the mode of operation (SDR/DDR/HS200). Currently only
 *         DDR & SDR supported.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_select_mode_transfer(struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 38);
		goto fail;
	}

	if (hsdmmc->best_mode == TEGRABL_SDMMC_MODE_HS400) {
		hsdmmc->data_width = DATA_WIDTH_DDR_8BIT;
		error = sdmmc_enable_hs400(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	} else if (hsdmmc->best_mode == TEGRABL_SDMMC_MODE_DDR52) {
		if ((hsdmmc->card_support_speed
				& ECSD_CT_HS_DDR_MASK) != 0U) {
			pr_info("sdmmc DDR50 mode\n");
			if (hsdmmc->device_type == DEVICE_TYPE_SD) {
				hsdmmc->data_width = DATA_WIDTH_DDR_4BIT;
			} else {
				hsdmmc->data_width = DATA_WIDTH_DDR_8BIT;
			}
			error = sdmmc_set_card_clock(hsdmmc, MODE_DATA_TRANSFER, 1);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
			error = sdmmc_enable_ddr_mode(hsdmmc);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}

			sdmmc_set_tap_trim(hsdmmc);
			goto fail;
		}
	} else if (hsdmmc->best_mode == TEGRABL_SDMMC_MODE_SDR26) {
		pr_info("sdmmc SDR mode\n");
		if (hsdmmc->device_type == DEVICE_TYPE_SD) {
			hsdmmc->data_width = DATA_WIDTH_4BIT;
		} else {
			hsdmmc->data_width = DATA_WIDTH_8BIT;
		}
		error = sdmmc_set_card_clock(hsdmmc, MODE_DATA_TRANSFER, 2);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		error = sdmmc_set_bus_width(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		goto fail;
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
	}

fail:
	return error;
}

/** @brief Select the default region of operation as user partition.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_set_default_region(struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	sdmmc_access_region boot_part;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 39);
		goto fail;
	}

	boot_part = (sdmmc_access_region)((hsdmmc->boot_config >>
			ECSD_BC_BPE_OFFSET) & ECSD_BC_BPE_MASK);

	if (boot_part == ECSD_BC_BPE_BAP1 || boot_part == ECSD_BC_BPE_BAP2) {
		error =  sdmmc_select_access_region(hsdmmc, boot_part);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
fail:
	return error;
}

/** @brief Wait till data line is ready for transfer.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_wait_for_data_line_ready(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t present_state;
	uint32_t data_line_active;
	uint32_t timeout = DATA_TIMEOUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 40);
		goto fail;
	}

	pr_trace("Wait for Dataline ready\n");
	while (timeout != 0U) {
		present_state = sdmmc_readl(hsdmmc, PRESENT_STATE);
		data_line_active = NV_DRF_VAL(SDMMCAB, PRESENT_STATE, DAT_LINE_ACTIVE,
			present_state);
		if (data_line_active == 0U) {
			break;
		}
		tegrabl_udelay(1);
		timeout--;

		if (timeout == 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 8);
			goto fail;
		}
	}
fail:
	return error;
}

/** @brief Map the logical blocks to physical blocks in boot partitions.
 *
 *  @param start_sector Starting logical sector in boot partitions.
 *  @param num_sectors Number of logical sector in boot partitions.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_get_correct_boot_block(bnum_t *start_sector,
	bnum_t *num_sectors, struct tegrabl_sdmmc *hsdmmc)
{
	sdmmc_access_region region;
	bnum_t sector_per_boot_block;
	bnum_t current_sector;
	bnum_t sector_in_current_region;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((hsdmmc == NULL) || (start_sector == NULL) || (num_sectors == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 41);
		goto fail;
	}

	sector_per_boot_block = hsdmmc->boot_blocks;
	current_sector = *start_sector;
	sector_in_current_region = *num_sectors;

	pr_trace("sector_per_boot_block = %d\n", sector_per_boot_block);

	/* If boot partition size is zero, then the card is either eSD or */
	/* eMMC version is < 4.3. */
	if (hsdmmc->boot_blocks == 0U) {
		hsdmmc->current_access_region = USER_PARTITION;
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 42);
		goto fail;
	}

	if (current_sector < sector_per_boot_block) {
		region = BOOT_PARTITION_1;
		if (sector_in_current_region > sector_per_boot_block - current_sector) {
			sector_in_current_region = sector_per_boot_block - current_sector;
		}
	} else if (current_sector < (sector_per_boot_block << 1)) {
		region = BOOT_PARTITION_2;
		current_sector = current_sector - sector_per_boot_block;
		if (sector_in_current_region > sector_per_boot_block - current_sector) {
			sector_in_current_region = sector_per_boot_block - current_sector;
		}
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 43);
		goto fail;
	}

	if (region != hsdmmc->current_access_region) {
		error = sdmmc_select_access_region(hsdmmc, region);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	*start_sector = current_sector;
	*num_sectors  = sector_in_current_region;

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("sdmmc get boot block: exit, error = %08X\n", error);
	}
	return error;
}

void sdmmc_update_drv_settings(struct tegrabl_sdmmc *hsdmmc, uint32_t instance)
{
	uint32_t val;

	if (instance != 3U) {
		return;
	}

	val = sdmmc_readl(hsdmmc, SDMEMCOMPPADCTRL);
	val = NV_FLD_SET_DRF_NUM(SDMMCAB, SDMEMCOMPPADCTRL, COMP_PAD_DRVUP_OVR,
			0xA, val);
	val = NV_FLD_SET_DRF_NUM(SDMMCAB, SDMEMCOMPPADCTRL, COMP_PAD_DRVDN_OVR,
			0xA, val);
	sdmmc_writel(hsdmmc, SDMEMCOMPPADCTRL, val);

	return;
}

/**
 * @brief Performs auto-calibration before accessing controller.
 *
 * @param hsdmmc Context information to determine the base
 *                 address of controller.
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_auto_calibrate(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg = 0;
	uint32_t timeout = TIME_OUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 44);
		goto fail;
	}

	/* disable card clock before auto calib */
	sdmmc_card_clock_enable(hsdmmc, false);

	/* set E_INPUT_OR_E_PWRD bit after auto calib */
	reg = sdmmc_readl(hsdmmc, SDMEMCOMPPADCTRL);
	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, SDMEMCOMPPADCTRL, PAD_E_INPUT_OR_E_PWRD,
			1, reg);
	sdmmc_writel(hsdmmc, SDMEMCOMPPADCTRL, reg);
	tegrabl_udelay(2);

	reg = sdmmc_readl(hsdmmc, AUTO_CAL_CONFIG);

	/* set PD and PU offsets */
	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, AUTO_CAL_CONFIG,
			AUTO_CAL_PD_OFFSET, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, AUTO_CAL_CONFIG,
			AUTO_CAL_PU_OFFSET, 0, reg);

	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, AUTO_CAL_CONFIG,
			AUTO_CAL_ENABLE, 1, reg);
	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, AUTO_CAL_CONFIG,
			AUTO_CAL_START, 1, reg);

	sdmmc_writel(hsdmmc, AUTO_CAL_CONFIG, reg);

	// Wait till Auto cal active is cleared or timeout upto 100ms
	while (timeout != 0U) {
		reg = sdmmc_readl(hsdmmc, AUTO_CAL_STATUS);
		reg = NV_DRF_VAL(SDMMCAB, AUTO_CAL_STATUS, AUTO_CAL_ACTIVE, reg);
		if (reg == 0U) {
			break;
		}
		tegrabl_udelay(1);
		timeout--;
		if (timeout == 0U) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 9);
			goto fail;
		}
	}

fail:
	if (hsdmmc != NULL) {
		/* clear E_INPUT_OR_E_PWRD bit after auto calib */
		reg = sdmmc_readl(hsdmmc, SDMEMCOMPPADCTRL);
		reg = NV_FLD_SET_DRF_NUM(SDMMCAB, SDMEMCOMPPADCTRL, PAD_E_INPUT_OR_E_PWRD,
				0, reg);
		sdmmc_writel(hsdmmc, SDMEMCOMPPADCTRL, reg);

		/* enable card clock after auto calib */
		sdmmc_card_clock_enable(hsdmmc, true);
	}

	if (error != TEGRABL_NO_ERROR) {
		pr_debug("Auto calibration failed\n");
	}
	return error;
}

/**
 * @brief Update I/O Spare & trim control.
 *
 * @param hsdmmc Context information to determine the base
 *                 address of controller.
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_io_spare_update(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t val;
	uint32_t temp;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 45);
		goto fail;
	}

	/* set SPARE_OUT[3] bit */
	val = sdmmc_readl(hsdmmc, IO_SPARE);
	temp = NV_DRF_VAL(SDMMCAB, IO_SPARE, SPARE_OUT, val);
	temp |= 1U << 3;
	val = NV_FLD_SET_DRF_NUM(SDMMCAB, IO_SPARE, SPARE_OUT, temp, val);
	sdmmc_writel(hsdmmc, IO_SPARE, val);

	val = sdmmc_readl(hsdmmc, VENDOR_IO_TRIM_CNTRL);
	val = NV_FLD_SET_DRF_NUM(SDMMCAB, VENDOR_IO_TRIM_CNTRL, SEL_VREG, 0, val);
	sdmmc_writel(hsdmmc, VENDOR_IO_TRIM_CNTRL, val);

fail:
	return error;
}

void sdmmc_set_tap_trim(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t reg;

	reg = sdmmc_readl(hsdmmc, VENDOR_CLOCK_CNTRL);
	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, VENDOR_CLOCK_CNTRL, TRIM_VAL,
			hsdmmc->trim_value, reg);
	reg = NV_FLD_SET_DRF_NUM(SDMMCAB, VENDOR_CLOCK_CNTRL, TAP_VAL,
			hsdmmc->tap_value, reg);
	sdmmc_writel(hsdmmc, VENDOR_CLOCK_CNTRL, reg);
}

tegrabl_error_t sdmmc_dll_caliberation(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t val;
	time_t start_time;
	time_t end_time;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	val = sdmmc_readl(hsdmmc, VENDOR_DLLCAL_CFG);
	val = NV_FLD_SET_DRF_NUM(SDMMCAB, VENDOR_DLLCAL_CFG, CALIBRATE, 1, val);
	sdmmc_writel(hsdmmc, VENDOR_DLLCAL_CFG, val);

	start_time = tegrabl_get_timestamp_ms();

	/* wait until DLL calibration is done or timeout */
	do {
	    val = sdmmc_readl(hsdmmc, VENDOR_DLLCAL_CFG_STA);
		if (NV_DRF_VAL(SDMMCAB, VENDOR_DLLCAL_CFG_STA, DLL_CAL_ACTIVE, val) != 1UL) {
			break;
		}
		end_time = tegrabl_get_timestamp_ms();
		if ((end_time - start_time) > DLL_CALIB_TIMEOUT_IN_MS) {
			err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			goto fail;
		}
	    tegrabl_mdelay(1);
	} while (true);

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("DLL calibration failed\n");
	}
	return err;
}
