/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
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
#include <tegrabl_blockdev.h>
#include <tegrabl_sdmmc_defs.h>
#include <tegrabl_sdmmc_protocol.h>
#include <tegrabl_sdmmc_bdev.h>
#include <tegrabl_sdmmc_host.h>
#include <tegrabl_clock.h>
#include <tegrabl_timer.h>
#include <tegrabl_io.h>
#include <arsdmmc.h>
#include <arapb_misc_gp.h>
#include <tegrabl_drf.h>
#include <tegrabl_addressmap.h>

/*  Defines the macro for reading from various offsets of sdmmc base controller.
 */
#define sdmmc_readl(context, reg) \
	NV_READ32(context->base_addr + SDMMC_##reg##_0);

/*  Defines the macro for writing to various offsets of sdmmc base controller.
 */
#define sdmmc_writel(context, reg, value) \
	NV_WRITE32((context->base_addr + SDMMC_##reg##_0), value);

/** @brief Wait till the internal clock is stable.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_wait_clk_stable(sdmmc_context_t *context)
{
	uint32_t reg;
	uint32_t clk_ready;
	uint32_t timeout = TIME_OUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Wait till the clock is stable. */
	while (timeout != 0U) {
		reg = sdmmc_readl(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
		clk_ready = NV_DRF_VAL(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
						INTERNAL_CLOCK_STABLE, reg);
		if (clk_ready != 0U) {
			break;
		}
		tegrabl_udelay(1);
		timeout--;
		if (!timeout) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			pr_debug("clk stable time out\n");
			goto fail;
		}
	}
fail:
	return error;
}

/** @brief Wait for the command to be completed.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param cmd_reg Command register to decide timeout.
 *  @param arg Argument to decide timeout.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_wait_command_complete(sdmmc_context_t *context,
	uint32_t cmd_reg, uint32_t arg)
{
	uint32_t cmd_cmplt;
	uint32_t int_status;
	uint32_t timeout = COMMAND_TIMEOUT_IN_US;
	sdmmc_cmd cmd_index;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Prepare the interrupt error mask. */
	uint32_t err_mask =
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_INDEX_ERR, ERR) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_END_BIT_ERR,
			END_BIT_ERR_GENERATED) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_CRC_ERR,
			CRC_ERR_GENERATED) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_TIMEOUT_ERR, TIMEOUT);

	cmd_index = NV_DRF_VAL(SDMMC, CMD_XFER_MODE, COMMAND_INDEX, cmd_reg);

	/* Change timeout for erase command. */
	if ((cmd_index == CMD_ERASE) ||
		(cmd_index == CMD_SWITCH && arg == SWITCH_SANITIZE_ARG))
		timeout = context->erase_timeout_us;

	/* Wait for command complete. */
	while (timeout != 0U) {
		int_status = sdmmc_readl(context, INTERRUPT_STATUS);

		cmd_cmplt = NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, CMD_COMPLETE,
			int_status);

		if ((int_status & err_mask) != 0U) {
			pr_debug("Error in command_complete %x int_status\n", int_status);
			error =  TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 0);
			goto fail;
		}
		if (cmd_cmplt != 0U)
			break;

		tegrabl_udelay(1);
		timeout--;

		if (!timeout) {
			error =  TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 1);
			goto fail;
		}
	}

fail:
	return error;
}

/** @brief Send the abort command with required arguments.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_abort_command(sdmmc_context_t *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t retries = 2;
	uint32_t cmd_reg;
	uint32_t int_status;
	uint32_t *sdmmc_response = &context->response[0];

	pr_debug("Sending Abort CMD%d\n",
			CMD_STOP_TRANSMISSION);

	cmd_reg =
		NV_DRF_NUM(SDMMC, CMD_XFER_MODE, COMMAND_INDEX,
			CMD_STOP_TRANSMISSION) |
		NV_DRF_DEF(SDMMC, CMD_XFER_MODE, COMMAND_TYPE, ABORT) |
		NV_DRF_DEF(SDMMC, CMD_XFER_MODE, DATA_PRESENT_SELECT,
			NO_DATA_TRANSFER) |
		NV_DRF_DEF(SDMMC, CMD_XFER_MODE, CMD_INDEX_CHECK_EN, ENABLE) |
		NV_DRF_DEF(SDMMC, CMD_XFER_MODE, CMD_CRC_CHECK_EN, ENABLE) |
		NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
			RESP_LENGTH_48BUSY) |
		NV_DRF_DEF(SDMMC, CMD_XFER_MODE, DATA_XFER_DIR_SEL, WRITE) |
		NV_DRF_DEF(SDMMC, CMD_XFER_MODE, BLOCK_COUNT_EN, DISABLE) |
		NV_DRF_DEF(SDMMC, CMD_XFER_MODE, DMA_EN, DISABLE);

	while (retries != 0U) {
		/* Clear Status bits what ever is set. */
		int_status = sdmmc_readl(context, INTERRUPT_STATUS);
		sdmmc_writel(context, INTERRUPT_STATUS, int_status);
		/* This redundant read is for debug purpose. */
		int_status = sdmmc_readl(context, INTERRUPT_STATUS);
		sdmmc_writel(context, ARGUMENT, 0);
		sdmmc_writel(context, CMD_XFER_MODE, cmd_reg);
		/* Wait for the command to be sent out.if it fails, retry. */
		if (!sdmmc_wait_command_complete(context, cmd_reg, 0))
			break;
		sdmmc_init_controller(context, context->controller_id);
		retries--;
	}
	if (retries != 0U) {
		/* Wait till response is received from card. */
		error = sdmmc_cmd_txr_ready(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		/* Wait till busy line is deasserted by card. It is for R1b response. */
		error = sdmmc_data_txr_ready(context);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			pr_debug("data not in txr mode\n");
			goto fail;
		}
		sdmmc_read_response(context, RESP_TYPE_R1B, sdmmc_response);
	}
fail:
	return error;
}

/**
* @brief Enable/disable sdmmc card clock
*
* @param context Context information to determine the base
*                 address of controller
* @param enable Gives whether to enable/disable
*/
static void sdmmc_card_clock_enable(sdmmc_context_t *context,
	bool enable)
{
	uint32_t reg;

	reg = sdmmc_readl(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
	reg = NV_FLD_SET_DRF_NUM(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
		SD_CLOCK_EN, enable, reg);
	sdmmc_writel(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);
	return;
}

/*  @brief Enable ddr mode operation for read/write.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_enable_ddr_mode(sdmmc_context_t *context)
{
	uint32_t host_reg = 0;
	uint32_t cap_reg = 0;
	uint32_t cap_high_reg = 0;
	uint32_t misc_reg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((context->card_support_speed & ECSD_CT_HS_DDR_52_180_300_MASK)
			== ECSD_CT_HS_DDR_52_180_300) {
		/* set HS_TIMING to 1 before setting the ddr mode data width.. */
		context->high_speed_mode = 1;

		pr_debug("Enable high speed for ddr mode\n");

		error = sdmmc_enable_high_speed(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* When SPARE[8] is set DDR50 support is advertised in */
		/* CAPABILITIES_HIGER_0_DDR50 */

		misc_reg = sdmmc_readl(context, VENDOR_MISC_CNTRL);

		misc_reg = NV_FLD_SET_DRF_NUM(SDMMC, VENDOR_MISC_CNTRL, SDMMC_SPARE0,
			0x100, misc_reg);

		sdmmc_writel(context, VENDOR_MISC_CNTRL, misc_reg);

		pr_debug("set bus width ddr mode\n");
		error = sdmmc_set_bus_width(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/*set the ddr mode in Host controller and other misc things.. */
		/* DDR support is available by fuse bit */
		/* check capabilities register for Ddr support */
		/* read capabilities and capabilities higher reg */
		cap_reg = sdmmc_readl(context, CAPABILITIES);
		cap_high_reg = sdmmc_readl(context, CAPABILITIES_HIGHER);

		if (NV_DRF_VAL(SDMMC, CAPABILITIES_HIGHER, DDR50, cap_high_reg) &&
			NV_DRF_VAL(SDMMC, CAPABILITIES, VOLTAGE_SUPPORT_1_8_V, cap_reg) &&
			NV_DRF_VAL(SDMMC, CAPABILITIES, HIGH_SPEED_SUPPORT, cap_reg)) {
			/* reset SD clock enable */
			sdmmc_card_clock_enable(context, false);

			/* set DDR50 UHS Mode */
			host_reg = sdmmc_readl(context, AUTO_CMD12_ERR_STATUS);
			host_reg = NV_FLD_SET_DRF_DEF(SDMMC, AUTO_CMD12_ERR_STATUS,
				UHS_MODE_SEL, DDR50, host_reg);
			sdmmc_writel(context, AUTO_CMD12_ERR_STATUS, host_reg);

			/* set enable SD clock */
			sdmmc_card_clock_enable(context, true);
		}
		pr_debug("sdmmc ddr50 mode enabled\n");
	} else if ((context->card_support_speed &
				ECSD_CT_HS_DDR_52_120_MASK) ==
			ECSD_CT_HS_DDR_52_120) {
		pr_debug("not supported for ddr 1.2V\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		goto fail;
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 1);
		pr_debug("unknown ddr operation\n");
		goto fail;
	}

fail:
	return error;
}

/** @brief Resets all the registers of the controller.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_reset_controller(sdmmc_context_t *context)
{
	uint32_t reg;
	uint32_t reset_in_progress;
	uint32_t timeout = TIME_OUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 25);
		goto fail;
	}

	/* Reset Controller's All reg's. */
	reg = NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
		SW_RESET_FOR_ALL, RESETED);
	sdmmc_writel(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);

	/* Wait till Reset is completed. */
	while (timeout != 0U) {
		reg = sdmmc_readl(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);

		reset_in_progress = NV_DRF_VAL(SDMMC,
			SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, SW_RESET_FOR_ALL, reg);

		if (!reset_in_progress) {
			break;
		}

		tegrabl_udelay(1);
		timeout--;

		if (!timeout) {
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
* @param context Context information to determine the base
*                 address of controller.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t sdmmc_enable_hostv4(sdmmc_context_t *context)
{
	uint32_t reg;

	reg = sdmmc_readl(context, AUTO_CMD12_ERR_STATUS);
	reg = NV_FLD_SET_DRF_DEF(SDMMC, AUTO_CMD12_ERR_STATUS, HOST_VERSION_4_EN,
			ENABLE, reg);
	reg = NV_FLD_SET_DRF_DEF(SDMMC, AUTO_CMD12_ERR_STATUS,
			ADDRESSING_64BIT_EN, ENABLE, reg);
	sdmmc_writel(context, AUTO_CMD12_ERR_STATUS, reg);

	context->is_hostv4_enabled = true;
	return TEGRABL_NO_ERROR;
}

/**
* @brief Get the status of hostv4 whether it is enabled or not
*	and store in sdmmc_context
*
* @param context Context information to determine the base
*                 address of controller.
*
*/
void sdmmc_get_hostv4_status(sdmmc_context_t *context)
{
	uint32_t reg_data;

	reg_data = sdmmc_readl((context), AUTO_CMD12_ERR_STATUS);

	reg_data = NV_DRF_VAL(SDMMC, AUTO_CMD12_ERR_STATUS, HOST_VERSION_4_EN,
						  reg_data);

	context->is_hostv4_enabled = (reg_data ==
		SDMMC_AUTO_CMD12_ERR_STATUS_0_HOST_VERSION_4_EN_ENABLE) ? true : false;
}

/** @brief sets the internal clock for the card from controller.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param mode Different mode like init, power on & data transfer.
 *  @param clk_divider Appropriate divider for generating card clock.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_set_card_clock(sdmmc_context_t *context,
	sdmmc_mode_t mode, uint32_t clk_divider)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t reg = 0;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 26);
		goto fail;
	}

	switch (mode) {
	/* Set Clock below 400 KHz. */
	case MODE_POWERON:
		pr_debug("Clock set for power on of card\n");
		reg = NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
				INTERNAL_CLOCK_EN, OSCILLATE) |
			NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
				SDCLK_FREQUENCYSELECT, DIV256);

		sdmmc_writel(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);

		error = sdmmc_wait_clk_stable(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;

	/* Set clock as requested by user. */
	case MODE_INIT:
	case MODE_DATA_TRANSFER:
		pr_debug("clock set for init or data_transfer\n");

		sdmmc_card_clock_enable(context, false);
		reg = sdmmc_readl(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
		reg = NV_FLD_SET_DRF_NUM(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
			SDCLK_FREQUENCYSELECT, clk_divider, reg);
		sdmmc_writel(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);

		error = sdmmc_wait_clk_stable(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		reg = sdmmc_readl(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
		reg = NV_FLD_SET_DRF_NUM(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
				DATA_TIMEOUT_COUNTER_VALUE, 0xE, reg);
		sdmmc_writel(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);
		sdmmc_card_clock_enable(context, true);
		break;
	default:
		break;
	}
fail:
	return error;
}

/** @brief Sets the voltage in power_control_host according to capabilities.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_enable_bus_power(sdmmc_context_t *context)
{
	uint32_t reg = 0;
	uint32_t cap_reg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 27);
		goto fail;
	}

	cap_reg = sdmmc_readl(context, CAPABILITIES);

	/* Read the voltage supported by the card. */
	pr_debug("set the correct voltage range\n");
	if (NV_DRF_VAL(SDMMC, CAPABILITIES, VOLTAGE_SUPPORT_3_3_V, cap_reg) != 0U)
		reg |=
			NV_DRF_DEF(SDMMC, POWER_CONTROL_HOST, SD_BUS_VOLTAGE_SELECT, V3_3);
	else if (NV_DRF_VAL(SDMMC, CAPABILITIES,
				VOLTAGE_SUPPORT_3_0_V, cap_reg) != 0U)
		reg |=
			NV_DRF_DEF(SDMMC, POWER_CONTROL_HOST, SD_BUS_VOLTAGE_SELECT, V3_0);
	else
		reg |=
			NV_DRF_DEF(SDMMC, POWER_CONTROL_HOST, SD_BUS_VOLTAGE_SELECT, V1_8);

	/* Enable bus power. */
	reg |= NV_DRF_DEF(SDMMC, POWER_CONTROL_HOST, SD_BUS_POWER, POWER_ON);

	sdmmc_writel(context, POWER_CONTROL_HOST, reg);

fail:
	return error;
}

/** @brief Sets the interrupt error mask.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return Void.
 */
tegrabl_error_t sdmmc_set_interrupt_status_reg(sdmmc_context_t *context)
{
	uint32_t reg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 28);
		goto fail;
	}

	reg =
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, DATA_END_BIT_ERR, ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, DATA_CRC_ERR, ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, DATA_TIMEOUT_ERR, ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, COMMAND_INDEX_ERR, ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, COMMAND_END_BIT_ERR,
			ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, COMMAND_CRC_ERR, ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, COMMAND_TIMEOUT_ERR,
			ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, CARD_REMOVAL, ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, CARD_INSERTION, ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, DMA_INTERRUPT, ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, TRANSFER_COMPLETE, ENABLE) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS_ENABLE, COMMAND_COMPLETE, ENABLE);

	/* Poll for the above interrupts. */
	pr_debug("setup error mask for interrupt\n");
	sdmmc_writel(context, INTERRUPT_STATUS_ENABLE, reg);

fail:
	return error;
}

/** @brief Checks if card is stable and present or not.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_is_card_present(sdmmc_context_t *context)
{
	uint32_t card_stable;
	uint32_t reg;
	uint32_t card_inserted = 0;
	uint32_t timeout = TIME_OUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 29);
		goto fail;
	}

	/* Check if the card is present or not */
	while (timeout != 0U) {
		reg = sdmmc_readl(context, PRESENT_STATE);
		card_stable = NV_DRF_VAL(SDMMC, PRESENT_STATE, CARD_STATE_STABLE, reg);
		if (card_stable != 0U) {
			card_inserted = NV_DRF_VAL(SDMMC, PRESENT_STATE, CARD_INSERTED,
				reg);
			break;
		}
		tegrabl_udelay(1);
		timeout--;
	}

	if (!card_stable)
		pr_debug("card is not stable\n");
	if (!card_inserted)
		pr_debug("card is not inserted\n");

	error = card_inserted ? TEGRABL_NO_ERROR :
				TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
fail:
	return error;
}

/** @brief Prepare cmd register to be send in command send.
 *
 *  @param cmd_reg Command register send by command send.
 *  @param data_cmd Configure cmd_reg for data transfer
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param index Index of the command being send.
 *  @param resp_type Response type of the command.
 */
tegrabl_error_t sdmmc_prepare_cmd_reg(uint32_t *cmd_reg, uint8_t data_cmd,
	sdmmc_context_t *context, sdmmc_cmd index, sdmmc_resp_type resp_type)
{
	uint32_t reg = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((context == NULL) || (cmd_reg == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 30);
		goto fail;
	}

	/* Basic argument preparation. */
	reg =
		NV_DRF_NUM(SDMMC, CMD_XFER_MODE, COMMAND_INDEX, index) |
		NV_DRF_NUM(SDMMC, CMD_XFER_MODE, DATA_PRESENT_SELECT,
			(data_cmd ? 1 : 0)) |
		NV_DRF_NUM(SDMMC, CMD_XFER_MODE, BLOCK_COUNT_EN, (data_cmd ? 1 : 0)) |
		NV_DRF_NUM(SDMMC, CMD_XFER_MODE, DMA_EN, (data_cmd ? 1 : 0));

	/* Enable multiple block select. */
	if ((index == CMD_READ_MULTIPLE) || (index == CMD_WRITE_MULTIPLE)) {
		reg |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, MULTI_BLOCK_SELECT , 1) |
				NV_DRF_DEF(SDMMC, CMD_XFER_MODE, AUTO_CMD12_EN, CMD12);
	}

	/* Select data direction for write. */
	if ((index == CMD_WRITE_MULTIPLE) && data_cmd) {
		reg |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, DATA_XFER_DIR_SEL, 0);
	} else if (data_cmd != 0U) {
		reg |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, DATA_XFER_DIR_SEL, 1);
	} else {
		/* No Action Required */
	}

	/* Cmd index check. */
	if ((resp_type != RESP_TYPE_NO_RESP) &&
		(resp_type != RESP_TYPE_R2) &&
		(resp_type != RESP_TYPE_R3) &&
		(resp_type != RESP_TYPE_R4)) {
		reg |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, CMD_INDEX_CHECK_EN, 1);
	}

	/* Crc index check. */
	if ((resp_type != RESP_TYPE_NO_RESP) &&
		(resp_type != RESP_TYPE_R3) &&
		(resp_type != RESP_TYPE_R4)) {
		reg |= NV_DRF_NUM(SDMMC, CMD_XFER_MODE, CMD_CRC_CHECK_EN, 1);
	}

	/* Response type check. */
	if (resp_type == RESP_TYPE_NO_RESP) {
		reg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
				NO_RESPONSE);
	} else if (resp_type == RESP_TYPE_R2) {
		reg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
				RESP_LENGTH_136);
	} else if (resp_type == RESP_TYPE_R1B) {
		reg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
				RESP_LENGTH_48BUSY);
	} else {
		reg |= NV_DRF_DEF(SDMMC, CMD_XFER_MODE, RESP_TYPE_SELECT,
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
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_try_send_command(uint32_t cmd_reg, uint32_t arg,
	uint8_t data_cmd, sdmmc_context_t *context)
{
	uint32_t trials = 3;
	uint32_t int_status;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 31);
		goto fail;
	}

	while (trials != 0U) {
		/* Clear Status bits what ever is set. */
		int_status = sdmmc_readl(context, INTERRUPT_STATUS);
		sdmmc_writel(context, INTERRUPT_STATUS, int_status);

		/* This redundant read is for debug purpose. */
		int_status = sdmmc_readl(context, INTERRUPT_STATUS);
		sdmmc_writel(context, ARGUMENT, arg);
		sdmmc_writel(context, CMD_XFER_MODE, cmd_reg);

		/* Wait for the command to be sent out. If it fails, retry. */
		if (!sdmmc_wait_command_complete(context, cmd_reg, arg))
			break;

		/* Recover Controller from Errors. */
		sdmmc_recover_controller_error(context, data_cmd);
		trials--;
	}

	if (trials != 0U)
		error = TEGRABL_NO_ERROR;
	else
		error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 1);
fail:
	return error;
}

/** @brief Check if next command can be send or not.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_cmd_txr_ready(sdmmc_context_t *context)
{
	uint32_t reg;
	uint32_t cmd_txr_ready;
	uint32_t timeout = COMMAND_TIMEOUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 32);
		goto fail;
	}

	/* Check if sending command is allowed or not. */
	while (timeout != 0U) {
		reg = sdmmc_readl(context, PRESENT_STATE);
		/* This bit is set to zero after response is received. So, response */
		/* registers should be read only after this bit is cleared. */
		cmd_txr_ready = NV_DRF_VAL(SDMMC, PRESENT_STATE, CMD_INHIBIT_CMD, reg);

		if (!cmd_txr_ready) {
			break;
		}

		tegrabl_udelay(1);
		timeout--;
		if (!timeout) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 3);
			goto fail;
		}
	}
fail:
	return error;
}

/** @brief Check if data can be send or not over data lines.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if data lines is free.
 */
tegrabl_error_t sdmmc_data_txr_ready(sdmmc_context_t *context)
{
	uint32_t reg;
	uint32_t data_txr_ready;
	uint32_t timeout = DATA_TIMEOUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 33);
		goto fail;
	}

	/* Check if sending data is allowed or not. */
	while (timeout != 0U) {
		reg = sdmmc_readl(context, PRESENT_STATE);
		/* This bit is set to zero after response is received. So, response */
		/* registers should be read only after this bit is cleared. */
		data_txr_ready = NV_DRF_VAL(SDMMC, PRESENT_STATE, CMD_INHIBIT_DAT,
			reg);

		if (!data_txr_ready) {
			break;
		}

		tegrabl_udelay(1);
		timeout--;

		if (!timeout) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 4);
			goto fail;
		}
	}

fail:
	return error;
}

/** @brief Read response of last command in local buffer.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param resp_type Response type of the command.
 *  @param buf Buffer in which response will be read.
 */
tegrabl_error_t sdmmc_read_response(sdmmc_context_t *context,
	sdmmc_resp_type resp_type, uint32_t *buf)
{
	uint32_t *temp = buf;
	uint32_t i;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((context == NULL) || (buf == NULL)) {
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
		*temp = sdmmc_readl(context, RESPONSE_R0_R1);
		pr_debug("%08X\n", buf[0]);
		break;
	case RESP_TYPE_R2:
		/* bits 127:8 of response are mapped to 119:0. */
		*temp = sdmmc_readl(context, RESPONSE_R0_R1);
		temp++;
		*temp = sdmmc_readl(context, RESPONSE_R2_R3);
		temp++;
		*temp = sdmmc_readl(context, RESPONSE_R4_R5);
		temp++;
		*temp = sdmmc_readl(context, RESPONSE_R6_R7);
		for (i = 0; i < 4; i++) {
			pr_debug("%08X\n", buf[i]);
		}
		break;
	case RESP_TYPE_NO_RESP:
	default:
		*temp = 0;
	}
fail:
	return error;
}

/** @brief Try to recover the controller from the error occured.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param data_cmd if data_cmd then send abort command.
 */
tegrabl_error_t sdmmc_recover_controller_error(sdmmc_context_t *context,
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

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 35);
		goto fail;
	}

	/* Prepare command error mask. */
	cmd_error =
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_INDEX_ERR, ERR) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_END_BIT_ERR,
			END_BIT_ERR_GENERATED) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_CRC_ERR,
			CRC_ERR_GENERATED) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_TIMEOUT_ERR, TIMEOUT);

	data_error =
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_END_BIT_ERR, ERR) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_CRC_ERR, ERR) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_TIMEOUT_ERR, TIMEOUT);

	int_status = sdmmc_readl(context, INTERRUPT_STATUS);
	reg = sdmmc_readl(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);

	if ((int_status & cmd_error) != 0U) {
		/* Reset Command line. */
		reg |= NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
			SW_RESET_FOR_CMD_LINE, RESETED);
		sdmmc_writel(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);
		/* Wait till Reset is completed. */
		while (timeout != 0U) {
			reg = sdmmc_readl(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
			reset_progress = NV_DRF_VAL(SDMMC,
				SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, SW_RESET_FOR_CMD_LINE,
				reg);
			if (!reset_progress) {
				break;
			}
			tegrabl_udelay(1);
			timeout--;
		}
		if (!timeout) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 5);
			goto fail;
		}
	}
	if ((int_status & data_error) != 0U) {
		/* Reset Data line. */
		reg |= NV_DRF_DEF(SDMMC, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL,
			SW_RESET_FOR_DAT_LINE, RESETED);
		sdmmc_writel(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, reg);
		/* Wait till Reset is completed. */
		while (timeout != 0U) {
			reg = sdmmc_readl(context, SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL);
			reset_progress = NV_DRF_VAL(SDMMC,
				SW_RESET_TIMEOUT_CTRL_CLOCK_CONTROL, SW_RESET_FOR_DAT_LINE,
				reg);
			if (!reset_progress) {
				break;
			}
			tegrabl_udelay(1);
			timeout--;
		}
		if (!timeout) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 6);
			goto fail;
		}
	}
	/* Clear Interrupt Status. */
	sdmmc_writel(context, INTERRUPT_STATUS, int_status);

	/* Issue abort command. */
	if (data_cmd != 0U) {
		(void)sdmmc_abort_command(context);
	}

	/* Wait for 40us as per spec. */
	tegrabl_udelay(40);

	/* Read Present State register. */
	present_state = sdmmc_readl(context, PRESENT_STATE);
	if (NV_DRF_VAL(SDMMC, PRESENT_STATE,
			DAT_3_0_LINE_LEVEL, present_state) != 0U) {
		/* Before give up, try full reset once. */
		sdmmc_init_controller(context, context->controller_id);
		present_state = sdmmc_readl(context, PRESENT_STATE);
		if (NV_DRF_VAL(SDMMC, PRESENT_STATE, DAT_3_0_LINE_LEVEL,
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
 *  @param context Context information to determine the base
 *                 address of controller.
 */
tegrabl_error_t sdmmc_set_data_width(sdmmc_data_width width,
	sdmmc_context_t *context)
{
	uint32_t reg = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 36);
		goto fail;
	}
	reg = sdmmc_readl(context, POWER_CONTROL_HOST);
	reg = NV_FLD_SET_DRF_NUM(SDMMC, POWER_CONTROL_HOST, DATA_XFER_WIDTH, width,
		reg);
	/* When 8-bit data width is enabled, the bit field DATA_XFER_WIDTH */
	/* value is not valid. */
	reg = NV_FLD_SET_DRF_NUM(SDMMC, POWER_CONTROL_HOST,
		EXTENDED_DATA_TRANSFER_WIDTH,
		(width == DATA_WIDTH_8BIT) || (width == DATA_WIDTH_DDR_8BIT) ? 1 : 0,
		reg);
	sdmmc_writel(context, POWER_CONTROL_HOST, reg);

fail:
	return error;
}

/** @brief Set the number of blocks to read or write.
 *
 *  @param block_size The block size being used for transfer.
 *  @param num_blocks Numbers of block to read/write.
 *  @param context Context information to determine the base
 *                 address of controller.
 */
void sdmmc_set_num_blocks(uint32_t block_size, uint32_t num_blocks,
	sdmmc_context_t *context)
{
	uint32_t reg;

	reg = NV_DRF_NUM(SDMMC, BLOCK_SIZE_BLOCK_COUNT, BLOCKS_COUNT,
			num_blocks) |
			NV_DRF_DEF(SDMMC, BLOCK_SIZE_BLOCK_COUNT,
		/*
		* This makes controller halt when ever it detects 512KB boundary.
		* When controller halts on this boundary, need to clear the
		* dma block boundary event and write SDMA base address again.
		* Writing address again triggers controller to continue.
		* We can't disable this. We have to live with it.
		*/
				HOST_DMA_BUFFER_SIZE, DMA512K) |
			NV_DRF_NUM(SDMMC, BLOCK_SIZE_BLOCK_COUNT,
				XFER_BLOCK_SIZE_11_0, block_size);

	sdmmc_writel(context, BLOCK_SIZE_BLOCK_COUNT, reg);
}

/** @brief Writes the buffer start for read/write.
 *
 *  @param buf Input buffer whose address is registered.
 *  @param context Context information to determine the base
 *                 address of controller.
 */
void sdmmc_setup_dma(dma_addr_t buf, sdmmc_context_t *context)
{
	if (context->is_hostv4_enabled == false) {
		sdmmc_writel(context, SYSTEM_ADDRESS, (uintptr_t)buf);
	}
#if defined(CONFIG_ENABLE_SDMMC_64_BIT_SUPPORT)
	else {
		sdmmc_writel(context, ADMA_SYSTEM_ADDRESS, (uint32_t) buf);
		sdmmc_writel(context, UPPER_ADMA_SYSTEM_ADDRESS, (uint32_t)(buf >> 32));
	}
#endif
}

/** @brief checks if card is in transfer state or not and perform various
 *         operations according to the mode of operation.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return Status of the device
 */
sdmmc_device_status sdmmc_query_status(sdmmc_context_t *context)
{
	uint32_t sdma_address;
	uint32_t transfer_done = 0;
	uint32_t intr_status;
	uint32_t error_mask;
	uint32_t dma_boundary_interrupt;
	uint32_t data_timeout_error;

	error_mask =
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_END_BIT_ERR, ERR) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_CRC_ERR, ERR) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_TIMEOUT_ERR, TIMEOUT) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_INDEX_ERR, ERR) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_END_BIT_ERR,
			END_BIT_ERR_GENERATED) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_CRC_ERR,
			CRC_ERR_GENERATED) |
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, COMMAND_TIMEOUT_ERR, TIMEOUT);

	dma_boundary_interrupt =
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DMA_INTERRUPT, GEN_INT);

	data_timeout_error =
		NV_DRF_DEF(SDMMC, INTERRUPT_STATUS, DATA_TIMEOUT_ERR, TIMEOUT);

	if (context->device_status == DEVICE_STATUS_IO_PROGRESS) {
		/* Check whether Transfer is done. */
		intr_status = sdmmc_readl(context, INTERRUPT_STATUS);

		transfer_done =
			NV_DRF_VAL(SDMMC, INTERRUPT_STATUS, XFER_COMPLETE, intr_status);
		/* Check whether there are any errors. */
		if ((intr_status & error_mask) != 0U) {
			if ((intr_status & error_mask) == data_timeout_error)
				context->device_status = DEVICE_STATUS_DATA_TIMEOUT;
			else
				context->device_status = DEVICE_STATUS_CRC_FAILURE;

			/* Recover from errors here. */
			(void)sdmmc_recover_controller_error(context, 1);
		} else if ((intr_status & dma_boundary_interrupt) != 0U) {
			/* Need to clear DMA boundary interrupt and write SDMA address */
			/* again. Otherwise controller doesn't go ahead. */
			sdmmc_writel(context, INTERRUPT_STATUS, dma_boundary_interrupt);
			if (context->is_hostv4_enabled == false) {
				sdma_address = sdmmc_readl(context, SYSTEM_ADDRESS);
				sdmmc_writel(context, SYSTEM_ADDRESS, sdma_address);
			}
#if defined(CONFIG_ENABLE_SDMMC_64_BIT_SUPPORT)
			else {
				sdma_address = sdmmc_readl(context, UPPER_ADMA_SYSTEM_ADDRESS);
				sdmmc_writel(context, UPPER_ADMA_SYSTEM_ADDRESS, sdma_address);
				sdma_address = sdmmc_readl(context, ADMA_SYSTEM_ADDRESS);
				sdmmc_writel(context, ADMA_SYSTEM_ADDRESS, sdma_address);
			}
#endif
		} else if (transfer_done != 0U) {
			context->device_status = DEVICE_STATUS_IDLE;
			sdmmc_writel(context, INTERRUPT_STATUS, intr_status);
		} else if ((uint32_t)(tegrabl_get_timestamp_ms() -
						(context->read_start_time)) > DATA_TIMEOUT_IN_US) {
			context->device_status = DEVICE_STATUS_IO_FAILURE;
		} else {
			/* No Action Required */
		}
	}
	return context->device_status;
}

/** @brief Use to enable/disable high speed mode according to the mode of
 *         Operation.
 *
 *  @param enable Enable High Speed mode.
 *  @param context Context information to determine the base
 *                 address of controller.
 */
tegrabl_error_t sdmmc_toggle_high_speed(uint8_t enable,
	sdmmc_context_t *context)
{
	uint32_t reg = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 37);
		goto fail;
	}

	pr_debug("toggle high speed bit\n");
	reg = sdmmc_readl(context, POWER_CONTROL_HOST);
	reg = NV_FLD_SET_DRF_NUM(SDMMC, POWER_CONTROL_HOST, HIGH_SPEED_EN,
			(enable == 1) ? 1 : 0, reg);
	sdmmc_writel(context, POWER_CONTROL_HOST, reg);

fail:
	return error;
}

/** @brief Select the mode of operation (SDR/DDR/HS200). Currently only
 *         DDR & SDR supported.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_select_mode_transfer(sdmmc_context_t *context)
{
	uint32_t tap_value;
	uint32_t trim_value;
	uint32_t reg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 38);
		goto fail;
	}

	switch (context->best_mode) {
	case TEGRABL_SDMMC_MODE_DDR52:
		if ((context->card_support_speed
				& ECSD_CT_HS_DDR_MASK) != 0U) {
			pr_info("sdmmc ddr50 mode\n");
			if (context->device_type == DEVICE_TYPE_SD)
				context->data_width = DATA_WIDTH_DDR_4BIT;
			else
				context->data_width = DATA_WIDTH_DDR_8BIT;
			error = sdmmc_set_card_clock(context, MODE_DATA_TRANSFER, 1);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
			error = sdmmc_enable_ddr_mode(context);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}

			tap_value = context->tap_value;
			trim_value = context->trim_value;

			reg = sdmmc_readl(context, VENDOR_CLOCK_CNTRL);
			reg |= NV_DRF_NUM(SDMMC, VENDOR_CLOCK_CNTRL, TRIM_VAL, trim_value);
			reg |= NV_DRF_NUM(SDMMC, VENDOR_CLOCK_CNTRL, TAP_VAL, tap_value);
			sdmmc_writel(context, VENDOR_CLOCK_CNTRL, reg);
			break;
		}
		/* Fall through to lower speed mode */
	case TEGRABL_SDMMC_MODE_SDR26:
		pr_info("sdmmc sdr mode\n");
		if (context->device_type == DEVICE_TYPE_SD)
			context->data_width = DATA_WIDTH_4BIT;
		else
			context->data_width = DATA_WIDTH_8BIT;
		error = sdmmc_set_card_clock(context, MODE_DATA_TRANSFER, 2);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		error = sdmmc_set_bus_width(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		break;
	default:
		pr_debug("SDMMC Mode %d is not supported\n", context->best_mode);
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
	}

fail:
	return error;
}

/** @brief Select the default region of operation as user partition.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_set_default_region(sdmmc_context_t *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	sdmmc_access_region boot_part;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 39);
		goto fail;
	}

	boot_part = (sdmmc_access_region)((context->boot_config >>
			ECSD_BC_BPE_OFFSET) & ECSD_BC_BPE_MASK);

	if (boot_part == ECSD_BC_BPE_BAP1 || boot_part == ECSD_BC_BPE_BAP2) {
		error =  sdmmc_select_access_region(context, boot_part);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
fail:
	return error;
}

/** @brief Wait till data line is ready for transfer.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_wait_for_data_line_ready(sdmmc_context_t *context)
{
	uint32_t present_state;
	uint32_t data_line_active;
	uint32_t timeout = DATA_TIMEOUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 40);
		goto fail;
	}

	pr_debug("Wait for Dataline ready\n");
	while (timeout != 0U) {
		present_state = sdmmc_readl(context, PRESENT_STATE);
		data_line_active = NV_DRF_VAL(SDMMC, PRESENT_STATE, DAT_LINE_ACTIVE,
			present_state);
		if (!data_line_active) {
			break;
		}
		tegrabl_udelay(1);
		timeout--;

		if (!timeout) {
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
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_get_correct_boot_block(bnum_t *start_sector,
	bnum_t *num_sectors, sdmmc_context_t *context)
{
	sdmmc_access_region region;
	bnum_t sector_per_boot_block;
	bnum_t current_sector;
	bnum_t sector_in_current_region;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((context == NULL) || (start_sector == NULL) || (num_sectors == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 41);
		goto fail;
	}

	sector_per_boot_block = context->boot_blocks;
	current_sector = *start_sector;
	sector_in_current_region = *num_sectors;

	pr_debug("sector_per_boot_block = %d\n", sector_per_boot_block);

	/* If boot partition size is zero, then the card is either eSD or */
	/* eMMC version is < 4.3. */
	if (context->boot_blocks == 0) {
		context->current_access_region = USER_PARTITION;
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 42);
		goto fail;
	}

	if (current_sector < sector_per_boot_block) {
		region = BOOT_PARTITION_1;
		if (sector_in_current_region > sector_per_boot_block - current_sector)
			sector_in_current_region = sector_per_boot_block - current_sector;
	} else if (current_sector < (sector_per_boot_block << 1)) {
		region = BOOT_PARTITION_2;
		current_sector = current_sector - sector_per_boot_block;
		if (sector_in_current_region > sector_per_boot_block - current_sector)
			sector_in_current_region = sector_per_boot_block - current_sector;
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 43);
		goto fail;
	}

	if (region != context->current_access_region) {
		error = sdmmc_select_access_region(context, region);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	*start_sector = current_sector;
	*num_sectors  = sector_in_current_region;

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: exit error = %08X\n", __func__, error);
	}
	return error;
}

void sdmmc_update_drv_settings(sdmmc_context_t *context, uint32_t instance)
{
	uint32_t val;

	if (instance != 3) {
		return;
	}

	val = sdmmc_readl(context, SDMEMCOMPPADCTRL);
	val = NV_FLD_SET_DRF_NUM(SDMMC, SDMEMCOMPPADCTRL, COMP_PAD_DRVUP_OVR,
			0xA, val);
	val = NV_FLD_SET_DRF_NUM(SDMMC, SDMEMCOMPPADCTRL, COMP_PAD_DRVDN_OVR,
			0xA, val);
	sdmmc_writel(context, SDMEMCOMPPADCTRL, val);

	return;
}

/**
 * @brief Performs auto-calibration before accessing controller.
 *
 * @param context Context information to determine the base
 *                 address of controller.
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_auto_calibrate(sdmmc_context_t *context)
{
	uint32_t reg = 0;
	uint32_t timeout = TIME_OUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 44);
		goto fail;
	}

	/* disable card clock before auto calib */
	sdmmc_card_clock_enable(context, false);

	/* set E_INPUT_OR_E_PWRD bit after auto calib */
	reg = sdmmc_readl(context, SDMEMCOMPPADCTRL);
	reg = NV_FLD_SET_DRF_NUM(SDMMC, SDMEMCOMPPADCTRL, PAD_E_INPUT_OR_E_PWRD,
			1, reg);
	sdmmc_writel(context, SDMEMCOMPPADCTRL, reg);
	tegrabl_udelay(2);

	reg = sdmmc_readl(context, AUTO_CAL_CONFIG);

	/* set PD and PU offsets */
	reg = NV_FLD_SET_DRF_NUM(SDMMC, AUTO_CAL_CONFIG,
			AUTO_CAL_PD_OFFSET, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(SDMMC, AUTO_CAL_CONFIG,
			AUTO_CAL_PU_OFFSET, 0, reg);

	reg = NV_FLD_SET_DRF_NUM(SDMMC, AUTO_CAL_CONFIG,
			AUTO_CAL_ENABLE, 1, reg);
	reg = NV_FLD_SET_DRF_NUM(SDMMC, AUTO_CAL_CONFIG,
			AUTO_CAL_START, 1, reg);

	sdmmc_writel(context, AUTO_CAL_CONFIG, reg);

	// Wait till Auto cal active is cleared or timeout upto 100ms
	while (timeout != 0U) {
		reg = sdmmc_readl(context, AUTO_CAL_STATUS);
		reg = NV_DRF_VAL(SDMMC, AUTO_CAL_STATUS, AUTO_CAL_ACTIVE, reg);
		if (!reg)
			break;
		tegrabl_udelay(1);
		timeout--;
		if (!timeout) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 9);
			goto fail;
		}
	}

fail:
	if (context != NULL) {
		/* clear E_INPUT_OR_E_PWRD bit after auto calib */
		reg = sdmmc_readl(context, SDMEMCOMPPADCTRL);
		reg = NV_FLD_SET_DRF_NUM(SDMMC, SDMEMCOMPPADCTRL, PAD_E_INPUT_OR_E_PWRD,
				0, reg);
		sdmmc_writel(context, SDMEMCOMPPADCTRL, reg);

		/* enable card clock after auto calib */
		sdmmc_card_clock_enable(context, true);
	}

	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: exit error = %08X\n", __func__, error);
	}
	return error;
}

/**
 * @brief Update I/O Spare & trim control.
 *
 * @param context Context information to determine the base
 *                 address of controller.
 * @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_io_spare_update(sdmmc_context_t *context)
{
	uint32_t val;
	uint32_t temp;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 45);
		goto fail;
	}

	/* set SPARE_OUT[3] bit */
	val = sdmmc_readl(context, IO_SPARE);
	temp = NV_DRF_VAL(SDMMC, IO_SPARE, SPARE_OUT, val);
	temp |= 1 << 3;
	val = NV_FLD_SET_DRF_NUM(SDMMC, IO_SPARE, SPARE_OUT, temp, val);
	sdmmc_writel(context, IO_SPARE, val);

	val = sdmmc_readl(context, VENDOR_IO_TRIM_CNTRL);
	val = NV_FLD_SET_DRF_NUM(SDMMC, VENDOR_IO_TRIM_CNTRL, SEL_VREG, 0, val);
	sdmmc_writel(context, VENDOR_IO_TRIM_CNTRL, val);

fail:
	return error;
}
