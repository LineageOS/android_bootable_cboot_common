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
#include <stdbool.h>
#include <tegrabl_timer.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_clock.h>
#include <tegrabl_sdmmc_defs.h>
#include <tegrabl_sdmmc_protocol.h>
#include <tegrabl_sdmmc_host.h>
#include <tegrabl_module.h>
#include <tegrabl_timer.h>
#include <tegrabl_dmamap.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_timer.h>
#include <tegrabl_malloc.h>
#include <inttypes.h>

#if defined(CONFIG_ENABLE_SDCARD)
#include <tegrabl_sd_protocol.h>
#endif

#ifndef NV_ADDRESS_MAP_SDMMC2_BASE
#define NV_ADDRESS_MAP_SDMMC2_BASE 0
#endif

/*  Base Address for each sdmmc instance
 */
static uint32_t sdmmc_base_addr[] = {
	NV_ADDRESS_MAP_SDMMC1_BASE,
	NV_ADDRESS_MAP_SDMMC2_BASE,
	NV_ADDRESS_MAP_SDMMC3_BASE,
	NV_ADDRESS_MAP_SDMMC4_BASE,
};

tegrabl_error_t sdmmc_print_regdump(sdmmc_context_t *context)
{
	uint32_t base;
	uint32_t i;

	base = context->base_addr;
	TEGRABL_UNUSED(base);
	for (i = 0; i <= 0x20C; i += 4) {
		pr_debug("%x = %x\n", base + i, *((uint32_t *)((uintptr_t)base + i)));
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t sdmmc_clock_init(uint32_t instance, uint32_t rate,
								 uint32_t source)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(rate);
	TEGRABL_UNUSED(source);

	err = tegrabl_car_rst_set(TEGRABL_MODULE_SDMMC, instance);
	if (err) {
		return err;
	}

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_SDMMC, instance, NULL);
	if (err) {
		return err;
	}

	err = tegrabl_car_set_clk_src(TEGRABL_MODULE_SDMMC, instance, source);
	if (err) {
		return err;
	}

	err = tegrabl_car_rst_clear(TEGRABL_MODULE_SDMMC, instance);
	if (err) {
		return err;
	}

	return err;
}

/** @brief Sets the default parameters for the context.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param instance Instance of the controller to be initialized.
 */
static void sdmmc_set_default_context(sdmmc_context_t *context,
	uint32_t instance)
{
	/* Store the default read timeout. */
	context->read_timeout_in_us = READ_TIMEOUT_IN_US;

	/* Default boot partition size is 0. */
	context->sdmmc_boot_partition_size = 0;

	/* Default access region is unknown. */
	context->current_access_region = UNKNOWN_PARTITION;

	/* Instance of the id which is being initialized. */
	context->controller_id = instance;

	/* Base address of the current controller. */
	context->base_addr = sdmmc_base_addr[instance];

	/* Host does not support high speed mode by default. */
	context->host_supports_high_speed_mode = 0;

	/* Erase group size is 0 sectors by default. */
	context->erase_group_size = 0;

	/* Erase timeout is 0 by default. */
	context->erase_timeout_us = 0;

	/* card rca */
	context->card_rca = (2 << RCA_OFFSET);

	/* block size to 512 */
	context->block_size_log2 = SDMMC_BLOCK_SIZE_LOG2;

	context->is_high_capacity_card = 1;
	if (context->device_type == DEVICE_TYPE_SD)
		context->data_width = 4;
	else
		context->data_width = 8;
}

tegrabl_error_t sdmmc_send_command(sdmmc_cmd index, uint32_t arg,
	sdmmc_resp_type resp_type, uint8_t data_cmd, sdmmc_context_t *context)
{
	uint32_t cmd_reg;
	uint32_t *sdmmc_response = &(context->response[0]);
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Return if the response type is out of bounds. */
	if (resp_type >= RESP_TYPE_NUM) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
		goto fail;
	}

	/* Check if ready for transferring command. */
	error = sdmmc_cmd_txr_ready(context);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	if (data_cmd) {
		/* Check if ready for transferring command. */
		error = sdmmc_data_txr_ready(context);
		if (error != TEGRABL_NO_ERROR)
			goto fail;
	}

	/* Prepare command args. */
	sdmmc_prepare_cmd_reg(&cmd_reg, data_cmd, context, index, resp_type);

	/* Try to send the commands. */
	error = sdmmc_try_send_command(cmd_reg, arg, data_cmd, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Check if ready for transferring command. */
	error  = sdmmc_cmd_txr_ready(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	if (resp_type == RESP_TYPE_R1B) {
		error = sdmmc_data_txr_ready(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
	/* Read the response. */
	sdmmc_read_response(context, resp_type, sdmmc_response);

fail:
	if (error) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

/** @brief send CMD1 to Query OCR from card
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_partial_cmd1_sequence(sdmmc_context_t *context)
{
	uint32_t ocr_reg = 0;
	uint32_t *sdmmc_resp = &context->response[0];
	uint32_t cmd1_arg = OCR_QUERY_VOLTAGE;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	/* Send SEND_OP_COND(CMD1) Command. */
	error = sdmmc_send_command(CMD_SEND_OCR, cmd1_arg,
							   RESP_TYPE_R3, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Extract OCR from Response. */
	ocr_reg = sdmmc_resp[0];

	if (ocr_reg & OCR_HIGH_VOLTAGE) {
		cmd1_arg = OCR_HIGH_VOLTAGE;
		context->is_high_voltage_range = 1;
	} else if (ocr_reg & OCR_LOW_VOLTAGE) {
		cmd1_arg = OCR_LOW_VOLTAGE;
		context->is_high_voltage_range = 0;
	} else {
		/* No Action Required */
	}
	cmd1_arg |= CARD_CAPACITY_MASK;

	/* Send SEND_OP_COND(CMD1) Command. */
	error = sdmmc_send_command(CMD_SEND_OCR, cmd1_arg,
							   RESP_TYPE_R3, 0, context);
fail:
	return error;
}

/** @brief Query OCR from card and fills appropriate context.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_get_operating_conditions(sdmmc_context_t *context)
{
	uint32_t ocr_reg = 0;
	uint32_t *sdmmc_resp = &context->response[0];
	uint32_t cmd1_arg = OCR_QUERY_VOLTAGE;
	time_t timeout = OCR_POLLING_TIMEOUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	time_t start_time;
	time_t elapsed_time = 0;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}
	start_time = tegrabl_get_timestamp_us();
	while (elapsed_time < timeout) {
		/* Send SEND_OP_COND(CMD1) Command. */
		error = sdmmc_send_command(CMD_SEND_OCR, cmd1_arg,
					 RESP_TYPE_R3, 0, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Extract OCR from Response. */
		ocr_reg = sdmmc_resp[0];

		/* Check for Card Ready. */
		if (ocr_reg & OCR_READY_MASK)
			break;

		/* Decide the voltage range supported. */
		if (cmd1_arg == OCR_QUERY_VOLTAGE) {
			if (ocr_reg & OCR_HIGH_VOLTAGE) {
				cmd1_arg = OCR_HIGH_VOLTAGE;
				context->is_high_voltage_range = 1;
			} else if (ocr_reg & OCR_LOW_VOLTAGE) {
				cmd1_arg = OCR_LOW_VOLTAGE;
				context->is_high_voltage_range = 0;
			} else {
				continue;
			}
			cmd1_arg |= CARD_CAPACITY_MASK;
			continue;
		}
		elapsed_time = tegrabl_get_timestamp_us() - start_time;
	}

	if (elapsed_time >= timeout) {
		error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 10);
		goto fail;
	}

	/* Query if the card is high cpacity card or not. */
	context->is_high_capacity_card = ((ocr_reg & CARD_CAPACITY_MASK) ? 1 : 0);

fail:
	if (error != TEGRABL_NO_ERROR) {
		sdmmc_print_regdump(context);
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

tegrabl_error_t sdmmc_parse_cid(sdmmc_context_t *context)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	uint32_t *sdmmc_resp = &context->response[0];

	/* Get the manufacture id */
	context->manufacture_id =
		(uint8_t)((sdmmc_resp[3] & MANUFACTURING_ID_MASK)
													 >> MANUFACTURING_ID_SHIFT);
	pr_debug("Manufacture id is %d\n", context->manufacture_id);

	return ret;
}

tegrabl_error_t sdmmc_parse_csd(sdmmc_context_t *context)
{
	uint32_t *sdmmc_resp;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t c_size;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	sdmmc_resp = &context->response[0];

	/* Get the supported maximum block size. */
	context->block_size_log2 =
		((CSD_READ_BL_LEN_MASK & sdmmc_resp[2]) >> CSD_READ_BL_LEN_SHIFT);

	/* Force SDMMC block-size to max supported */
	if (context->block_size_log2 < SDMMC_BLOCK_SIZE_LOG2) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	context->block_size_log2 = SDMMC_BLOCK_SIZE_LOG2;

	pr_debug("Block size is %d\n", SDMMC_CONTEXT_BLOCK_SIZE(context));

	/* Get the spec_version. */
	context->spec_version =
		(uint8_t)((CSD_SPEC_VERS_MASK & sdmmc_resp[3]) >> CSD_SPEC_VERS_SHIFT);

	/* Get the max speed for init. */
	context->tran_speed =
		(uint32_t)((CSD_TRAN_SPEED_MASK & sdmmc_resp[2]) >>
														CSD_TRAN_SPEED_SHIFT);

	/* Capacity of the device = (C_SIZE + 1) * 512 * 1024 bytes */
	c_size = (sdmmc_resp[SD_SDHC_CSIZE_WORD] & SD_SDHC_CSIZE_MASK)
					>> SD_SDHC_CSIZE_SHIFT;
	context->user_blocks = (c_size + 1) * SD_SDHC_CSIZE_MULTIPLIER;

	/* Enable max clock required for init. */
	/*TODO: get card clock divisor from some API. */
	if (context->tran_speed == CSD_V4_3_TRAN_SPEED) {
		error = sdmmc_set_card_clock(context, MODE_INIT, 8);
		if (error != TEGRABL_NO_ERROR)
			goto fail;
	} else {
		/*TODO: get card clock divisor from some API. */
		error  = sdmmc_set_card_clock(context, MODE_INIT, 8);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
	pr_debug("sdmmc internal clock enabled\n");

fail:
	if (error) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

/** @brief Check if the last command was successful or not.
 *
 *  @param index Index of the last or next command.
 *  @param after_cmd_execution Tells if the next command or last command
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_verify_response(sdmmc_cmd index,
	uint8_t after_cmd_execution, sdmmc_context_t *context)
{
	uint32_t *response;
	uint32_t address_out_of_range;
	uint32_t address_misaligned;
	uint32_t block_length_error;
	uint32_t cmd_crc_error;
	uint32_t illegal_command;
	uint32_t card_internal_error;
	uint32_t card_ecc_error;
	uint32_t switch_error;
	uint32_t erase_error;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
		goto fail;
	}

	/* Store response buffer in temporary pointer. */
	response = &context->response[0];

	/* Set error mask for out of bound addresses. */
	address_out_of_range =
		(CS_ADDRESS_OUT_OF_RANGE_MASK & response[0]) >>
												 CS_ADDRESS_OUT_OF_RANGE_SHIFT;

	/* Set error mask for unaligned addresses. */
	address_misaligned =
		(CS_ADDRESS_MISALIGN_MASK & response[0]) >> CS_ADDRESS_MISALIGN_SHIFT;

	/* Set error mask for wrong block length. */
	block_length_error =
		(CS_BLOCK_LEN_ERROR_MASK & response[0]) >> CS_BLOCK_LEN_ERROR_SHIFT;

	/* Set error mask for CRC errors. */
	cmd_crc_error =
		(CS_COM_CRC_ERROR_MASK & response[0]) >> CS_COM_CRC_ERROR_SHIFT;

	/* Check if the command is illegal after previous command or not. */
	illegal_command =
		(CS_ILLEGAL_CMD_MASK & response[0]) >> CS_ILLEGAL_CMD_SHIFT;

	/* Set error mask for internal card error. */
	card_internal_error =
		(CS_CC_ERROR_MASK & response[0]) >> CS_CC_ERROR_SHIFT;

	/* Set error mask for card ecc errors. */
	card_ecc_error =
		(CS_CARD_ECC_FAILED_MASK & response[0]) >> CS_CARD_ECC_FAILED_SHIFT;

	/* Set error mask for switch command. */
	switch_error =
		(CS_SWITCH_ERROR_MASK & response[0]) >> CS_SWITCH_ERROR_SHIFT;

	/* Set error mask for error command. */
	 erase_error = (CS_ERASE_CMD_ERROR_MASK & response[0]);

	switch (index) {
	/* Check for read/write command failure */
	case CMD_READ_MULTIPLE:
	case CMD_WRITE_MULTIPLE: {
		if (!after_cmd_execution) {
			/* This is during response time. */
			if (address_out_of_range || address_misaligned ||
				block_length_error || card_internal_error) {
				error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
				goto fail;
			}
		} else if (cmd_crc_error || illegal_command || card_ecc_error) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
			goto fail;
		} else {
			/* No Action Required */
		}
		break;
	}
	/* Check for set block length command failure. */
	case CMD_SET_BLOCK_LENGTH: {
		if ((!after_cmd_execution) &&
			(block_length_error || card_internal_error)) {
			/* Either the argument of a SET_BLOCKLEN command exceeds the */
			/* maximum value allowed for the card, or the previously defined */
			/* block length is illegal for the current command. */
			error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 2);
			goto fail;
		}
		break;
	}
	/* Check for set block count command failure. */
	case CMD_SET_BLOCK_COUNT: {
		if ((!after_cmd_execution) && card_internal_error) {
			error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 3);
			goto fail;
		}
		break;
	}

	/* Check for switch command failure. */
	case CMD_SWITCH: {
		if (after_cmd_execution && (switch_error || cmd_crc_error)) {
			/* If set, the card did not switch to the expected mode as */
			/* requested by the SWITCH command. */
			error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 4);
			goto fail;
		}
		break;
	}
	/* Check for query ext-csd register command failure. */
	case CMD_SEND_EXT_CSD: {
		if ((!after_cmd_execution) && card_internal_error) {
			error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 5);
			goto fail;
		}
		break;
	}
	/* Check for erase command failure. */
	case CMD_ERASE: {
		if (after_cmd_execution && erase_error) {
			error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 6);
			goto fail;
		}
		break;
	}
	/* Check whether card is in programming mode or not. */
	case CMD_SEND_STATUS: {
		if (after_cmd_execution) {
			if (((response[0] & CS_TRANSFER_STATE_MASK)
								>> CS_TRANSFER_STATE_SHIFT)
												== STATE_PRG)
				error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 7);
			else
				error = TEGRABL_NO_ERROR;
		}
		break;
	}
	default:
		break;
	}

fail:
	if (error) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

tegrabl_error_t sdmmc_card_transfer_mode(sdmmc_context_t *context)
{
	uint32_t *sdmmc_resp;
	uint32_t card_state;
	tegrabl_error_t error;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 7);
		goto fail;
	}

	sdmmc_resp = &context->response[0];
	card_state =
		(CS_CURRENT_STATE_MASK & sdmmc_resp[0]) >> CS_CURRENT_STATE_SHIFT;

	/* return if card is in transfer state or not */
	if (card_state == STATE_TRAN)
		error = TEGRABL_NO_ERROR;
	else
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 1);
fail:
	return error;
}

/** @brief Gets the power class and fill appropriate context
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return power_class returns the power class data.
 */
static uint32_t sdmmc_get_power_class(sdmmc_context_t *context)
{
	uint32_t power_class;

	/* Set power class for ddr mode from getextcsd data & width supported. */
	if (context->is_ddr_mode)
		power_class = context->is_high_voltage_range ?
			context->power_class_52MHz_ddr360V :
			context->power_class_52MHz_ddr195V;
	else if (context->is_high_voltage_range)
		power_class = context->high_speed_mode ?
			context->power_class_52MHz_360V :
			context->power_class_26MHz_360V;
	else
		power_class = context->high_speed_mode ?
			context->power_class_52MHz_195V :
			context->power_class_26MHz_195V;
	/*
	 * In the above power class, lower 4 bits give power class requirement for
	 * for 4-bit data width and upper 4 bits give power class requirement for
	 * for 8-bit data width.
	 */
	if ((context->data_width == DATA_WIDTH_4BIT) ||
		(context->data_width == DATA_WIDTH_DDR_4BIT))
		power_class = (power_class >> ECSD_POWER_CLASS_4_BIT_OFFSET) &
					ECSD_POWER_CLASS_MASK;
	else if ((context->data_width == DATA_WIDTH_8BIT) ||
			(context->data_width == DATA_WIDTH_DDR_8BIT))
		power_class = (power_class >> ECSD_POWER_CLASS_8_BIT_OFFSET) &
			ECSD_POWER_CLASS_MASK;
	else /*if (context->data_width == Sdmmcdata_width_1Bit) */
		power_class = 0;

	return power_class;
}

/** @brief Sends switch command with appropriate argument.
 *
 *  @param cmd_arg Argument for the switch command.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if switch command send & verify passes.
 */
static tegrabl_error_t sdmmc_send_switch_command(uint32_t cmd_arg,
	sdmmc_context_t *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_debug("send switch command\n");

	/* Sends the switch command. */
	error = sdmmc_send_command(CMD_SWITCH,
				 cmd_arg, RESP_TYPE_R1B, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	/* Send status to the controller. */
	error = sdmmc_send_command(CMD_SEND_STATUS,
				 context->card_rca, RESP_TYPE_R1, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Verify the response of the switch command. */
	error = sdmmc_verify_response(CMD_SWITCH, 1, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}

/** @brief Determine the start and num sectors for trim operation.
 *
 *  @param start_sector Start physical sector for trim.
 *  @param num_sector Number of physical sectors for trim.
 *  @param Argument for erase command.
 *  @param context Context information to determine the base
 *                 address of controller.
 */
static void sdmmc_get_trim_sectors(bnum_t *start_sector, bnum_t *num_sector,
	uint32_t *arg, sdmmc_context_t *context)
{
	uint32_t unalign_start = *start_sector % context->erase_group_size;
	uint32_t unalign_count = *num_sector % context->erase_group_size;
	uint32_t temp;

	*arg = 0x0;

	if (unalign_start) {
		temp = context->erase_group_size - unalign_start;
		if (*num_sector > temp) {
			*num_sector = temp;
		}
		*arg = 0x1;
	}

	if (unalign_count && !unalign_start) {
		if (*num_sector < context->erase_group_size)
			*arg = 0x1;
		else
			*num_sector = (*num_sector / context->erase_group_size) *
							context->erase_group_size;
	}
}

/** @brief Sets the power class and fill appropriate context
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_set_power_class(sdmmc_context_t *context)
{
	uint32_t cmd_arg;
	uint32_t power_class;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Return if card version is less than 4. */
	if (context->spec_version < 4)
		return TEGRABL_NO_ERROR;

	/* Queries the power class. */
	power_class = sdmmc_get_power_class(context);

	/* Select best possible configuration here. */
	while (power_class > context->max_power_class_supported) {
		if ((context->data_width == DATA_WIDTH_8BIT) ||
				(context->data_width == DATA_WIDTH_DDR_8BIT)) {
			context->data_width = DATA_WIDTH_4BIT;
		} else if ((context->data_width == DATA_WIDTH_4BIT) ||
				(context->data_width == DATA_WIDTH_DDR_4BIT)) {
			context->data_width = DATA_WIDTH_1BIT;
		} else {
			/* No Action Required */
		}
		power_class = sdmmc_get_power_class(context);
	}

	if (power_class) {
		pr_debug("Set Power Class to %d\n", power_class);
		cmd_arg = SWITCH_SELECT_POWER_CLASS_ARG |
			(power_class << SWITCH_SELECT_POWER_CLASS_OFFSET);
		error = sdmmc_send_switch_command(cmd_arg, context);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
	}

fail:
	return error;
}

/** @brief Queries extended csd register and fill appropriate context
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_get_ext_csd(sdmmc_context_t *context)
{
	sdmmc_device_status dev_status;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t *buf = context->ext_csd_buffer_address;
	dma_addr_t dma_addr;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 8);
		goto fail;
	}

	/* Set the number of blocks to be read as 1. */
	pr_debug("setting block to read as 1\n");
	sdmmc_set_num_blocks(SDMMC_CONTEXT_BLOCK_SIZE(context), 1, context);

	dma_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SDMMC,
									  (uint8_t)(context->controller_id), buf,
									  sizeof(context->ext_csd_buffer_address),
									  TEGRABL_DMA_FROM_DEVICE);

	/* Write the dma address. */
	pr_debug("sdma buffer address\n");
	sdmmc_setup_dma(dma_addr, context);

	/* Send extended csd command. */
	pr_debug("send ext csd command\n");
	error = sdmmc_send_command(CMD_SEND_EXT_CSD,
				 0, RESP_TYPE_R1, 1, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Verify the input response. */
	pr_debug("verify the input response\n");

	error = sdmmc_verify_response(CMD_SEND_EXT_CSD, 0,  context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Register as I/O in progress. */
	context->device_status = DEVICE_STATUS_IO_PROGRESS;
	context->read_start_time = tegrabl_get_timestamp_ms();

	/* Loop till I/O is in progress. */
	do {
		dev_status = sdmmc_query_status(context);
	} while ((dev_status == DEVICE_STATUS_IO_PROGRESS));

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SDMMC,
							 (uint8_t)(context->controller_id), buf,
							 sizeof(context->ext_csd_buffer_address),
							 TEGRABL_DMA_FROM_DEVICE);

	/* Check if device is in idle mode or not. */
	pr_debug("device check for idle %d\n", dev_status);
	if (dev_status != DEVICE_STATUS_IDLE) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BUSY, 0);
		goto fail;
	}

	/* Number of sectors in each boot partition. */
	context->boot_blocks =
		(buf[ECSD_BOOT_PARTITION_SIZE_OFFSET] << 17) /
		SDMMC_CONTEXT_BLOCK_SIZE(context);
	pr_debug("boot blocks=%d\n", context->boot_blocks);

	/* Number of 256byte blocks in rpmb partition. */
	context->rpmb_blocks =
		(buf[ECSD_RPMB_SIZE_OFFSET] << 17) / RPMB_DATA_SIZE;

	pr_debug("rpmb blocks=%d\n", context->rpmb_blocks);

	/* Store the number of user partition sectors. */
	if (context->is_high_capacity_card) {
		context->user_blocks =
			(buf[ECSD_SECTOR_COUNT_0_OFFSET] |
			 (buf[ECSD_SECTOR_COUNT_1_OFFSET] << 8) |
			 (buf[ECSD_SECTOR_COUNT_2_OFFSET] << 16) |
			 (buf[ECSD_SECTOR_COUNT_3_OFFSET] << 24));
		pr_debug("user blocks=%d\n", context->user_blocks);
	}

	/* Store the power class. */
	context->power_class_26MHz_360V =
		buf[ECSD_POWER_CL_26_360_OFFSET];
	context->power_class_52MHz_360V =
		buf[ECSD_POWER_CL_52_360_OFFSET];
	context->power_class_26MHz_195V =
		buf[ECSD_POWER_CL_26_195_OFFSET];
	context->power_class_52MHz_195V =
		buf[ECSD_POWER_CL_52_195_OFFSET];
	context->power_class_52MHz_ddr360V =
		buf[ECSD_POWER_CL_DDR_52_360_OFFSET];
	context->power_class_52MHz_ddr195V =
		buf[ECSD_POWER_CL_DDR_52_195_OFFSET];

	/* Store the current speed supported by card */
	context->card_support_speed = buf[ECSD_CARD_TYPE_OFFSET];

	/* Store the boot configs. */
	context->boot_config = buf[ECSD_BOOT_CONFIG_OFFSET];

	/* Store if sanitize command is supported or not. */
	context->sanitize_support =
		(buf[ECSD_SEC_FEATURE_OFFSET] &
			ECSD_SEC_SANITIZE_MASK) >> ECSD_SEC_SANITIZE_SHIFT;

	/* Store the high capacity erase group size. */
	context->erase_group_size = buf[ECSD_ERASE_GRP_SIZE] << 10;

	/* Store the high capacity erase timeout for max erase. */
	context->erase_timeout_us =
			300000 * buf[ECSD_ERASE_TIMEOUT_OFFSET] *
						(MAX_ERASABLE_SECTORS / context->erase_group_size);

	pr_debug("time out is 0x%x erase group is 0x%x\n",
			context->erase_timeout_us, context->erase_group_size);

	pr_debug("card_support_speed %d\n", context->card_support_speed);

	/* Store the current bus width. */
	context->card_bus_width = buf[ECSD_BUS_WIDTH];

	pr_debug("card_bus_width %d %d\n", context->card_bus_width,
		error);

fail:
	if (error) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

/** @brief Erase operation from start sector till the count of sector.
 *
 *  @param dev Bio layer handle for device to be erased.
 *  @param block Start of physical sector to be erased.
 *  @param count Number of physical sectors to be erased.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_erase_block(bnum_t block, bnum_t count,
	sdmmc_context_t *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	bnum_t num_sectors = count;
	bnum_t start_sector = block;
	bnum_t end_sector;
	bnum_t temp_start_sector;
	bnum_t temp_num_sector;
	uint32_t arg = 0x0;

	/* Send switch command to enable high capacity erase. */
	pr_debug("configure for high capacity erase\n");
	error = sdmmc_send_switch_command(SWITCH_HIGH_CAPACITY_ERASE_ARG, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	while (num_sectors) {
		/* Make the start aligned to MAX_ERASABLE_SECTORS for big erase. */
		if (num_sectors >= MAX_ERASABLE_SECTORS) {
			end_sector = MAX_ERASABLE_SECTORS;
		} else {
			/* Trying erase less than MAX_ERASABLE_SECTORS blocks. */
			end_sector = num_sectors;
		}

		/* Store the sectors in temporary variable. */
		temp_start_sector = start_sector;
		temp_num_sector = end_sector;

		pr_debug("pseudo start sector %d  count %d\n",
				start_sector, end_sector);

		/* For boot partitions determine correct physical sectors. */
		if (context->current_access_region ==
					BOOT_PARTITION_1 ||
						context->current_access_region ==
								BOOT_PARTITION_2) {
			error = sdmmc_get_correct_boot_block(&temp_start_sector,
					   &temp_num_sector, context);
			if (error != TEGRABL_NO_ERROR) {
				TEGRABL_SET_HIGHEST_MODULE(error);
				pr_debug("query correct boot blocks failed\n");
				goto fail;
			}
		}
		/* Get the sectors for performing trim. */
		sdmmc_get_trim_sectors(&temp_start_sector,
							   &temp_num_sector, &arg, context);

		pr_debug(" actual start %d actual count %d for region %d\n",
				temp_start_sector, temp_num_sector,
						context->current_access_region);

		/* Send erase group start command. */
		pr_debug("Send erase group start command\n");
		error = sdmmc_send_command(CMD_ERASE_GROUP_START,
											 temp_start_sector,
											 RESP_TYPE_R1, 0, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Send erase group end command. */
		pr_debug("Send erase group end command\n");
		error = sdmmc_send_command(CMD_ERASE_GROUP_END,
					temp_start_sector + temp_num_sector - 1,
					RESP_TYPE_R1, 0, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Send erase command with trim or high capacity erase arg. */
		pr_debug("Send erase command with %d arg\n", arg);
		error = sdmmc_send_command(CMD_ERASE,
					 arg, RESP_TYPE_R1B, 0, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Verify the response of the erase command. */
		pr_debug("verify the response\n");
		error = sdmmc_verify_response(CMD_ERASE, 1, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Check if the card is in programming state or not. */
		pr_debug("Send status command\n");
		do {
			error = sdmmc_send_command(CMD_SEND_STATUS,
						 context->card_rca, RESP_TYPE_R1, 0, context);
			if (error != TEGRABL_NO_ERROR)
				goto fail;

			if (sdmmc_verify_response(CMD_SEND_STATUS, 1, context))
				continue;
			else
				break;
		} while (1);

		/* Update the sector start & number. */
		num_sectors -= temp_num_sector;
		start_sector += temp_num_sector;
	}

fail:
	if (error) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

/** @brief Initializes the card by following SDMMC protocol.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_identify_card(sdmmc_context_t *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Check if card is present and stable. */
	pr_debug("check card present and stable\n");
	if (sdmmc_is_card_present(context))
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 2);

	/* Send command 0. */
	pr_debug("send command 0\n");
	error = sdmmc_send_command(CMD_IDLE_STATE, 0,
				 RESP_TYPE_NO_RESP, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Get the operating conditions. */
	pr_debug("send command 1\n");
	error = sdmmc_get_operating_conditions(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Request for all the available cids. */
	pr_debug("send command 2\n");
	error = sdmmc_send_command(CMD_ALL_SEND_CID, 0,
										 RESP_TYPE_R2, 0, context);
	if (error != TEGRABL_NO_ERROR)
		goto fail;


	/* Assign the relative card address. */
	pr_debug("send command 3\n");
	error = sdmmc_send_command(CMD_SET_RELATIVE_ADDRESS,
				 context->card_rca, RESP_TYPE_R1, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Query the csd. */
	pr_debug("query card specific data by command 9\n");
	error = sdmmc_send_command(CMD_SEND_CSD,
				 context->card_rca, RESP_TYPE_R2, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Store the context by parsing csd. */
	pr_debug("parse csd data\n");
	error = sdmmc_parse_csd(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Select the card for data transfer. */
	pr_debug("send command 7\n");
	error = sdmmc_send_command(CMD_SELECT_DESELECT_CARD,
				 context->card_rca, RESP_TYPE_R1, 0, context);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	/* Check if card is in data transfer mode or not. */
	pr_debug("check if card is in transfer mode\n");
	error = sdmmc_send_command(CMD_SEND_STATUS,
				 context->card_rca, RESP_TYPE_R1, 0, context);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	error = sdmmc_card_transfer_mode(context);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	/* Query the extended csd registers. */
	pr_debug("ext csd register read\n");
	error = sdmmc_get_ext_csd(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Set the power class. */
	pr_debug("set power class\n");
	error = sdmmc_set_power_class(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Enable high speed if supported. */
	pr_debug("enable high speed\n");
	error = sdmmc_enable_high_speed(context);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

fail:
	return error;
}

/** @brief Read/write from the input block till the count of blocks.
 *
 *  @param block Start sector for read/write.
 *  @param count Number of sectors to be read/write.
 *  @param buf Input buffer for read/write.
 *  @param is_write Is the command is for write or not.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_block_io(bnum_t block, bnum_t count, uint8_t *buf,
	uint8_t is_write, sdmmc_context_t *context)
{
	uint32_t cmd_arg;
	bnum_t current_start_sector;
	bnum_t residue_start_sector;
	bnum_t current_num_sectors;
	bnum_t residue_num_sectors;
	sdmmc_cmd cmd;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	dma_addr_t dma_addr;
	tegrabl_dma_data_direction dma_dir;

	if ((context == NULL) || (buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 9);
		goto fail;
	}

	/* Decide which command is to be send. */
	if (is_write)
		cmd = CMD_WRITE_MULTIPLE;
	else
		cmd = CMD_READ_MULTIPLE;


	/* Enable block length setting if not DDR mode. */
	if ((context->data_width == DATA_WIDTH_4BIT) ||
		(context->data_width == DATA_WIDTH_8BIT)) {
		/* Send SET_BLOCKLEN(CMD16) Command. */
		error = sdmmc_send_command(CMD_SET_BLOCK_LENGTH,
								   SDMMC_CONTEXT_BLOCK_SIZE(context),
								   RESP_TYPE_R1, 0, context);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			pr_debug("setting block length failed\n");
			goto fail;
		}

		error = sdmmc_verify_response(CMD_SET_BLOCK_LENGTH, 0, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
	/* Store start and end sectors in temporary variable. */
	residue_num_sectors = count;
	residue_start_sector = block;

	while (residue_num_sectors) {
		current_start_sector = residue_start_sector;
		current_num_sectors = residue_num_sectors;

		/* Check if data line is ready for transfer. */
		if (sdmmc_wait_for_data_line_ready(context)) {
			error = sdmmc_recover_controller_error(context, 1);
			if (error != TEGRABL_NO_ERROR)
				goto fail;
		}
		pr_debug("residue_start_sector = %d, residue_num_sectors = %d\n",
			residue_start_sector, residue_num_sectors);

		/* Select access region. This will change start & num sector */
		/* based on the region the request falls in. */

		if ((context->current_access_region ==
					BOOT_PARTITION_1) ||
				(context->current_access_region ==
					BOOT_PARTITION_2)) {
			error = sdmmc_get_correct_boot_block(
						&current_start_sector,
						&current_num_sectors, context);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}
		pr_debug("Region = %d (1->BP1, 2->BP2, 0->UP)\n",
				context->current_access_region);

		pr_debug("actual_start_sector = %d, actual_num_sectors = %d\n",
				current_start_sector, current_num_sectors);

		/* Set number of blocks to read or write. */
		sdmmc_set_num_blocks(SDMMC_CONTEXT_BLOCK_SIZE(context),
							 current_num_sectors, context);

		/* Set up command arg. */
		cmd_arg = (uint32_t) current_start_sector;

		pr_debug(
				"cur_Start_Sector = %d, cur_num_sectors = %d,cmd_arg = %d\n",
				current_start_sector,
				current_num_sectors, cmd_arg);

		dma_dir = is_write ? TEGRABL_DMA_TO_DEVICE : TEGRABL_DMA_FROM_DEVICE;
		dma_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SDMMC,
			context->controller_id, buf,
			current_num_sectors << context->block_size_log2, dma_dir);

		/* Setup Dma. */
		pr_debug("sdma buffer address\n");
		sdmmc_setup_dma(dma_addr, context);

		/* Send command to Card. */
		error = sdmmc_send_command(cmd, cmd_arg, RESP_TYPE_R1, 1, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* If response fails, return error. Nothing to clean up. */
		error = sdmmc_verify_response(cmd, 0, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		context->device_status = DEVICE_STATUS_IO_PROGRESS;
		context->read_start_time = tegrabl_get_timestamp_ms();

		/* Wait for idle condition. */
		while (sdmmc_query_status(context) == DEVICE_STATUS_IO_PROGRESS)
			;

		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SDMMC,
							(uint8_t)(context->controller_id), buf,
							current_num_sectors << context->block_size_log2,
							dma_dir);

		/* Error out if device is not idle. */
		if (sdmmc_query_status(context) != DEVICE_STATUS_IDLE) {
			error = TEGRABL_ERROR(TEGRABL_ERR_BUSY, 1);
			pr_info("device is not idle\n");
			goto fail;
		}

		/* Update the start sectos and num sectors accordingly. */
		residue_num_sectors -= current_num_sectors;
		residue_start_sector += current_num_sectors;
		buf += (current_num_sectors << context->block_size_log2);
	}

fail:
	if (error) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

/** @brief Reset the controller registers and enable internal clock at 400 KHz.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param instance Instance of the controller to be initialized.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_init_controller(sdmmc_context_t *context, uint32_t instance)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 10);
		goto fail;
	}

	/* Reset the registers of the controller. */
	pr_debug("reset controller at base\n");
	error = sdmmc_reset_controller(context);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		pr_debug("reset controller registers\n");
		goto fail;
	}

#if defined(CONFIG_ENABLE_SDMMC_64_BIT_SUPPORT)
	/* Enable host controller v4 */
	error = sdmmc_enable_hostv4(context);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		pr_error("enable hostv4 failed\n");
		goto fail;
	}
#endif

	/* Enable IO spare registers */
	pr_debug("update io spare registers\n");
	error = sdmmc_io_spare_update(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Enable Auto-Calibration */
	pr_debug("perform auto-calibration\n");
	error = sdmmc_auto_calibrate(context);
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("Auto-Calibration failed\n");
		sdmmc_update_drv_settings(context, instance);
	}

	/* Enable the clock oscillator with DIV64 divider. */
	pr_debug("enable internal clock\n");
	error = sdmmc_set_card_clock(context, MODE_POWERON, 128);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Enable the bus power. */
	pr_debug("enable bus power\n");
	error = sdmmc_enable_bus_power(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Set the error interrupt mask. */
	pr_debug("set interrupt status reg\n");
	sdmmc_set_interrupt_status_reg(context);

   /* Set the clock below 400 KHz for initialization of card. */
   pr_debug("init clk set\n");
   error = sdmmc_set_card_clock(context, MODE_INIT, 128);
   if (error != TEGRABL_NO_ERROR)
       goto fail;

	/* Set the data width to 1. */
	pr_debug("setting data width to 1\n");
	sdmmc_set_data_width(DATA_WIDTH_1BIT, context);

fail:
	return error;
}

static tegrabl_error_t sdmmc_check_is_trans_mode(sdmmc_context_t *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t instance;

	instance = context->controller_id;
	error = tegrabl_car_clk_enable(TEGRABL_MODULE_SDMMC, instance, NULL);
	if (error) {
		return error;
	}

	error = tegrabl_car_rst_clear(TEGRABL_MODULE_SDMMC, instance);
	if (error) {
		return error;
	}

	context->card_rca = (2 << RCA_OFFSET);

	/* Send status to the controller. */
	error = sdmmc_send_command(CMD_SEND_STATUS,
				 context->card_rca, RESP_TYPE_R1, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Verify the response of the switch command. */
	error = sdmmc_verify_response(CMD_SWITCH, 1, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}

/** @brief  Initializes the card and the controller and select appropriate mode
 *          for card transfer like DDR or SDR.
 *
 *  @param instance Instance of the controller to be initialized.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_init(uint32_t instance, sdmmc_context_t *context,
	uint32_t flag)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 11);
		goto fail;
	}

	/* Check if the device is already initialized. */
	if (context->initialized) {
		pr_info("sdmmc is already initialised\n");
		return TEGRABL_NO_ERROR;
	}

	/* Store the default init parameters. */
	pr_debug("set default context\n");
	sdmmc_set_default_context(context, instance);

	/* TODO: Handle the case if sdmmc is initialized with different
	 * mode than what context is configured.
	 */
	if ((flag == SDMMC_INIT_REINIT) &&
			(sdmmc_check_is_trans_mode(context) == TEGRABL_NO_ERROR)) {
		sdmmc_get_hostv4_status(context);
#if !defined(CONFIG_ENABLE_SDMMC_64_BIT_SUPPORT)
		if (context->is_hostv4_enabled == true) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 8);
			pr_error("Unsupported sdmmc state: 64-bit support\n");
			goto fail;
		}
#endif
		pr_debug("ext csd register read\n");
		error = sdmmc_get_ext_csd(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		return TEGRABL_NO_ERROR;
	}

	/* Enable clocks for input sdmmc instance. */
	pr_debug("enabling clock\n");
	error = sdmmc_clock_init(context->controller_id, CLK_102_MHZ,
								context->clk_src);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	/* Initiliaze controller. */
	pr_debug("initialize controller\n");
	error = sdmmc_init_controller(context, instance);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Identify card. */
	pr_debug("identify card\n");

#if defined(CONFIG_ENABLE_SDCARD)
	if (context->device_type == DEVICE_TYPE_SD)
		error = sd_identify_card(context);
	else
#endif
		error = sdmmc_identify_card(context);

	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Setup card for data transfer. */
	pr_debug("set card for data transfer\n");
	error = sdmmc_select_mode_transfer(context);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	/* Setup the default region of operation as user partition. */
	pr_debug("set default region as user partition\n");
	if (context->device_type != DEVICE_TYPE_SD) {
		error = sdmmc_set_default_region(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
	/* Mark as device is initialized. */
	context->initialized = true;

fail:
	if (error) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

tegrabl_error_t sdmmc_send_cmd0_cmd1(
		struct tegrabl_mb1bct_emmc_params *emmc_params)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t instance;
	sdmmc_context_t local_context = { 0 };
	sdmmc_context_t *context = &local_context;

	if (!emmc_params) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 7);
		goto fail;
	}

	instance = emmc_params->instance;
	if (instance > INSTANCE_3) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 6);
		goto fail;
	}

	context->clk_src = emmc_params->clk_src;
	context->best_mode = emmc_params->best_mode;
	context->tap_value = emmc_params->tap_value;
	context->trim_value = emmc_params->trim_value;
	context->controller_id = instance;

	/* Enable clocks for input sdmmc instance. */
	pr_debug("enabling clock\n");

	error = sdmmc_clock_init(instance, CLK_102_MHZ,
								context->clk_src);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	/* Store the default init parameters. */
	pr_debug("set default context\n");
	sdmmc_set_default_context(context, instance);

	/* Initiliaze controller. */
	pr_debug("initialize controller\n");
	error = sdmmc_init_controller(context, instance);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Check if card is present and stable. */
	pr_debug("check card present and stable\n");
	if (sdmmc_is_card_present(context))
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 2);

	/* Send command 0. */
	pr_debug("send command 0\n");
	error = sdmmc_send_command(CMD_IDLE_STATE, 0,
		RESP_TYPE_NO_RESP, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = sdmmc_partial_cmd1_sequence(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}

/** @brief Read/write from the input block till the count of blocks.
 *
 *  @param dev Bio device from which read/write is done.
 *  @param buf Input buffer for read/write.
 *  @param block Start sector for read/write.
 *  @param count Number of sectors to be read/write.
 *  @param is_write Is the command is for write or not.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param device User or Boot device to be accessed.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_io(tegrabl_bdev_t *dev, void *buf, bnum_t block,
	bnum_t count, uint8_t is_write, sdmmc_context_t *context,
	sdmmc_device device)
{
	uint32_t i = 0;
	bnum_t current_sectors = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t *pbuf = buf;

	if ((dev == NULL) || (buf == NULL) || (context == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 12);
		goto fail;
	}

	/* Mark the device is idle. */
	context->device_status = DEVICE_STATUS_IDLE;
	pr_debug("StartBlock= %d NumofBlock = %d\n", block, count);
	if (context->device_type != DEVICE_TYPE_SD) {
		/* Check for the correct region. */
		if (device == DEVICE_BOOT) {
			pr_debug("looking in boot partitions\n");
			sdmmc_select_access_region(context, BOOT_PARTITION_1);
		} else if (device == DEVICE_USER) {
			pr_debug("looking in user partition\n");
			sdmmc_select_access_region(context, USER_PARTITION);
		} else {
			pr_debug("wrong block to look for\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 13);
			goto fail;
		}
	}

	/* Error check the boundary condition for input sectors. */
	if ((block > (dev->block_count - 1)) ||
		((block + count) > (dev->block_count))) {
		pr_debug("block %d outside range with count %u\n", block, count);
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}
	/* Sdma supports maximum of 32 MB of transfer. */
	while (i < count) {
		current_sectors =
			(count - i) > MAX_SDMA_TRANSFER ? MAX_SDMA_TRANSFER : (count - i);

		error = sdmmc_block_io(block + i, current_sectors, pbuf, is_write,
					context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		i += current_sectors;
		pbuf += (current_sectors << context->block_size_log2);
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

/** @brief Enables high speed mode for card version more than 4.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_enable_high_speed(sdmmc_context_t *context)
{
	uint8_t *buf;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 14);
		goto fail;
	}

	buf = context->ext_csd_buffer_address;

	/* Return if card version is less than 4. */
	if (context->spec_version < 4)
		return TEGRABL_NO_ERROR;
	else
		context->high_speed_mode = 1;

	/* Clear controller's high speed bit. */
	pr_debug("Clear High Speed bit\n");
	sdmmc_toggle_high_speed(0, context);


	/* Enable the High Speed Mode, if required. */
	if (context->high_speed_mode) {
		pr_debug("Set High speed to %d\n",
				context->high_speed_mode);

		error = sdmmc_send_switch_command(SWITCH_HIGH_SPEED_ENABLE_ARG,
					context);
		if (error != TEGRABL_NO_ERROR)
			goto fail;

		/* Set the clock for data transfer. */
		error = sdmmc_set_card_clock(context, MODE_DATA_TRANSFER, 1);
		if (error != TEGRABL_NO_ERROR)
			goto fail;

		/* Validate high speed mode bit from card here. */
		error = sdmmc_get_ext_csd(context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		if (buf[ECSD_HS_TIMING_OFFSET]) {
			return TEGRABL_NO_ERROR;
		}
	}
	error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 3);
fail:
	return error;
}

/** @brief Selects the region of access from user or boot partitions.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param region  Select either user or boot region.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_select_access_region(sdmmc_context_t *context,
	sdmmc_access_region region)
{
	uint32_t cmd_arg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 15);
		goto fail;
	}

	/* Check access region argument range. */
	if (region >= NUM_PARTITION) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 16);
		goto fail;
	}

	/* Prepare switch command arg for partition switching. */
	cmd_arg = context->boot_config & (~SWITCH_SELECT_PARTITION_MASK);
	cmd_arg |= region;
	cmd_arg <<= SWITCH_SELECT_PARTITION_OFFSET;
	cmd_arg |= SWITCH_SELECT_PARTITION_ARG;

	pr_debug("trying to select the region\n");

	/* Send the switch command  to change the current partitions access. */
	error = sdmmc_send_switch_command(cmd_arg, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Store the access region in context. */
	context->current_access_region = region;

	pr_debug("Selected Region=%d(1->BP1, 2->BP2, 0->User)\n", region);

fail:
	return error;
}

/** @brief Sets the data bus width for DDR/SDR mode.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_set_bus_width(sdmmc_context_t *context)
{
	uint32_t cmd_arg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 17);
		goto fail;
	}

	/* Send SWITCH(CMD6) Command to select bus width. */
	pr_debug("Data width to %d (0->1bit, 1->4bit,", context->data_width);
	pr_debug(" 2->8-bit, 5->4-bit DDR, 6->8-bit DDR)\n");

	/* Prepare argument for switch command to change bus width. */
	cmd_arg = SWITCH_BUS_WIDTH_ARG |
		(context->data_width << SWITCH_BUS_WIDTH_OFFSET);

	/* Send the switch command. */
	error = sdmmc_send_switch_command(cmd_arg, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Set the controller register corresponding to bus width. */
	sdmmc_set_data_width(context->data_width, context);

fail:
	return error;
}

/** @brief Performs erase from given offset till the length of sectors.
 *
 *  @param dev Bio device handle in which erase is required.
 *  @param block Starting sector which will be erased.
 *  @param count Total number of sectors which will be erased.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param device User or Boot device to be accessed.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_erase(tegrabl_bdev_t *dev, bnum_t block, bnum_t count,
	sdmmc_context_t *context, sdmmc_device device)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (context == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 18);
		goto fail;
	}

	/* check for the correct region */
	if (device == DEVICE_BOOT) {
		pr_debug("looking in boot partitions\n");
		sdmmc_select_access_region(
			context, BOOT_PARTITION_1);
	} else if (device == DEVICE_USER) {
		pr_debug("looking in user partitions\n");
		sdmmc_select_access_region(context, USER_PARTITION);
	} else {
		/* Device is not in user or boot region. */
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 19);
		goto fail;
	}

	/* Check for boundary condition od the input bio device. */
	if ((block > (dev->block_count - 1)) ||
		((block + count) > (dev->block_count))) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 20);
		goto fail;
	}

	/* Perform erase over the given sectors. */
	error = sdmmc_erase_block(block, count, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

/** @brief Performs sanitize operation over unaddressed sectors
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_sanitize(sdmmc_context_t *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 21);
		goto fail;
	}

	/* Perform sanitize if supported. */
	if (context->sanitize_support) {
		pr_debug("perform sanitizing\n");

		error = sdmmc_send_switch_command(SWITCH_SANITIZE_ARG, context);
		if (error != TEGRABL_NO_ERROR)
			goto fail;

		/* Check if the card is in programming mode or not. */
		pr_debug("Send status command\n");
		do {
			error = sdmmc_send_command(CMD_SEND_STATUS, context->card_rca,
						 RESP_TYPE_R1, 0, context);
			if (error != TEGRABL_NO_ERROR)
				goto fail;
			if (sdmmc_verify_response(CMD_SEND_STATUS, 1, context))
				continue;
			else
				break;
		} while (1);
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 4);
	}
fail:
	if (error != TEGRABL_NO_ERROR)
		pr_debug("%s: exit error = %x\n", __func__, error);
	return error;
}

/** @brief Issue a single read to the RPMB partition.
 *
 *  @param rpmb_buf Pointer to read buffer containing single frame.
 *  @param context Context information to determine the base
 *                 address of controller.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_rpmb_block_read(sdmmc_rpmb_frame_t *frame,
											 sdmmc_context_t *context)
{
	uint32_t arg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((context == NULL) || (frame == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 22);
		goto fail;
	}

	/* Set block count to 1. */
	arg = 1;

	error = sdmmc_send_command(CMD_SET_BLOCK_COUNT, arg,
				RESP_TYPE_R1, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = sdmmc_verify_response(CMD_SET_BLOCK_COUNT, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Send read request */
	error = sdmmc_block_io(0, 1, (uint8_t *)frame, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	do {
		error = sdmmc_send_command(CMD_SEND_STATUS, context->card_rca,
					RESP_TYPE_R1, 0, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		if (sdmmc_verify_response(CMD_SEND_STATUS, 1, context))
			continue;
		else
			break;
	} while (1);

fail:
	if (error) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

/** @brief Issue a single write to the RPMB partition.
 *
 *  @param rpmb_buf Pointer to write buffer containing single frame.
 *  @param context Context information to determine the base
 *                 address of controller.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_rpmb_block_write(sdmmc_rpmb_frame_t *frame,
											  sdmmc_context_t *context)
{
	uint32_t arg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((context == NULL) || (frame == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 23);
		goto fail;
	}

	/* Set block count to 1 and enable reliable write */
	arg = (1 | (1 << 31));

	error = sdmmc_send_command(CMD_SET_BLOCK_COUNT, arg,
				RESP_TYPE_R1, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = sdmmc_verify_response(CMD_SET_BLOCK_COUNT, 0, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Send write request */
	error = sdmmc_block_io(0, 1, (uint8_t *)frame, 1, context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	do {
		error = sdmmc_send_command(CMD_SEND_STATUS, context->card_rca,
					RESP_TYPE_R1, 0, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		if (sdmmc_verify_response(CMD_SEND_STATUS, 1, context))
			continue;
		else
			break;
	} while (1);
fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}

/** @brief Read/write to a single sector within RPMB partition.
 *
 *  @param is_write Is the command is for write or not.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param device Device to be accessed.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_rpmb_io(uint8_t is_write,
	sdmmc_rpmb_context_t *rpmb_context, sdmmc_context_t *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((context == NULL) || (rpmb_context == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 24);
		goto fail;
	}

	/* Mark the device is idle. */
	context->device_status = DEVICE_STATUS_IDLE;

	/* Switch to RPMB partition. */
	sdmmc_select_access_region(context, RPMB_PARTITION);

	if (is_write) {
		/* Send request frame. */
		error = sdmmc_rpmb_block_write(&rpmb_context->req_frame, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Send request-response frame. */
		error = sdmmc_rpmb_block_write(&rpmb_context->req_resp_frame,
									   context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Get response frame. */
		error = sdmmc_rpmb_block_read(&rpmb_context->resp_frame, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	} else {
		/* Send request frame. */
		error = sdmmc_rpmb_block_write(&rpmb_context->req_frame, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Get response frame. */
		error = sdmmc_rpmb_block_read(&rpmb_context->resp_frame, context);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("%s: exit error = %x\n", __func__, error);
	}
	return error;
}
