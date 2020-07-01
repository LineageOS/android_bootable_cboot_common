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

tegrabl_error_t sdmmc_print_regdump(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t base;
	uint32_t i;

	base = hsdmmc->base_addr;
	TEGRABL_UNUSED(base);
	for (i = 0; i <= 0x20CU; i += 4UL) {
		pr_trace("%x = %x\n", base + i, *((uint32_t *)((uintptr_t)base + i)));
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t sdmmc_clock_init(uint32_t instance, uint32_t rate,
								 uint32_t source)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t srate;

	TEGRABL_UNUSED(source);

	err = tegrabl_car_rst_set(TEGRABL_MODULE_SDMMC, (uint8_t)instance);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_SDMMC, (uint8_t)instance, NULL);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	err = tegrabl_car_set_clk_src(TEGRABL_MODULE_SDMMC, (uint8_t)instance, (uint8_t)source);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	err = tegrabl_car_set_clk_rate(TEGRABL_MODULE_SDMMC, (uint8_t)instance, rate, &srate);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	err = tegrabl_car_rst_clear(TEGRABL_MODULE_SDMMC, (uint8_t)instance);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	return err;
}

/** @brief Sets the default parameters for the hsdmmc.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param instance Instance of the controller to be initialized.
 */
static void sdmmc_set_default_hsdmmc(struct tegrabl_sdmmc *hsdmmc,
	uint32_t instance)
{
	/* Store the default read timeout. */
	hsdmmc->read_timeout_in_us = READ_TIMEOUT_IN_US;

	/* Default boot partition size is 0. */
	hsdmmc->sdmmc_boot_partition_size = 0;

	/* Default access region is unknown. */
	hsdmmc->current_access_region = UNKNOWN_PARTITION;

	/* Instance of the id which is being initialized. */
	hsdmmc->controller_id = instance;

	/* Base address of the current controller. */
	hsdmmc->base_addr = sdmmc_base_addr[instance];

	/* Host does not support high speed mode by default. */
	hsdmmc->host_supports_high_speed_mode = 0;

	/* Erase group size is 0 sectors by default. */
	hsdmmc->erase_group_size = 0;

	/* Erase timeout is 0 by default. */
	hsdmmc->erase_timeout_us = 0;

	/* card rca */
	hsdmmc->card_rca = (2U << RCA_OFFSET);

	/* block size to 512 */
	hsdmmc->block_size_log2 = SDMMC_BLOCK_SIZE_LOG2;

	hsdmmc->is_high_capacity_card = 1;
	if (hsdmmc->device_type == DEVICE_TYPE_SD) {
		hsdmmc->data_width = 4;
	} else {
		hsdmmc->data_width = 8;
	}
}

tegrabl_error_t sdmmc_send_command(sdmmc_cmd index, uint32_t arg,
	sdmmc_resp_type resp_type, uint8_t data_cmd, struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t cmd_reg;
	uint32_t *sdmmc_response = &(hsdmmc->response[0]);
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Return if the response type is out of bounds. */
	if (resp_type >= RESP_TYPE_NUM) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
		goto fail;
	}

	/* Check if ready for transferring command. */
	error = sdmmc_cmd_txr_ready(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	if (data_cmd != 0U) {
		/* Check if ready for transferring command. */
		error = sdmmc_data_txr_ready(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	/* Prepare command args. */
	error = sdmmc_prepare_cmd_reg(&cmd_reg, data_cmd, hsdmmc, index,
								  resp_type);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Try to send the commands. */
	error = sdmmc_try_send_command(cmd_reg, arg, data_cmd, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Check if ready for transferring command. */
	error  = sdmmc_cmd_txr_ready(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	if (resp_type == RESP_TYPE_R1B) {
		error = sdmmc_data_txr_ready(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
	/* Read the response. */
	error = sdmmc_read_response(hsdmmc, resp_type, sdmmc_response);

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("sdmmc send command failed, error = %x\n", error);
	}
	return error;
}

/** @brief send CMD1 to Query OCR from card
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_partial_cmd1_sequence(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t cmd1_arg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	cmd1_arg = (CARD_CAPACITY_MASK | OCR_LOW_VOLTAGE);

	/* Send SEND_OP_COND(CMD1) Command. */
	error = sdmmc_send_command(CMD_SEND_OCR, cmd1_arg,
							   RESP_TYPE_R3, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}

/** @brief Query OCR from card and fills appropriate hsdmmc.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_get_operating_conditions(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t ocr_reg = 0;
	uint32_t *sdmmc_resp = &hsdmmc->response[0];
	uint32_t cmd1_arg;
	time_t timeout = OCR_POLLING_TIMEOUT_IN_US;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	time_t start_time;
	time_t elapsed_time = 0;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	start_time = tegrabl_get_timestamp_us();
	cmd1_arg = (CARD_CAPACITY_MASK | OCR_LOW_VOLTAGE);

	while (elapsed_time < timeout) {
		/* Send SEND_OP_COND(CMD1) Command. */
		error = sdmmc_send_command(CMD_SEND_OCR, cmd1_arg,
					 RESP_TYPE_R3, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Extract OCR from Response. */
		ocr_reg = sdmmc_resp[0];

		/* Check for Card Ready. */
		if ((ocr_reg & OCR_READY_MASK) != 0UL) {
			break;
		}

		elapsed_time = tegrabl_get_timestamp_us() - start_time;
	}

	if (elapsed_time >= timeout) {
		error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 10);
		goto fail;
	}

	/* Query if the card is high cpacity card or not. */
	hsdmmc->is_high_capacity_card =
		(((ocr_reg & CARD_CAPACITY_MASK) != 0U) ? 1U : 0U);

fail:
	if (error != TEGRABL_NO_ERROR) {
		sdmmc_print_regdump(hsdmmc);
		pr_error("OCR failed, error = %x\n", error);
	}
	return error;
}

#if defined(CONFIG_ENABLE_BLOCKDEV_DEVICE_INFO)
tegrabl_error_t sdmmc_parse_cid(struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	uint32_t *sdmmc_resp = &hsdmmc->response[0];
	uint8_t dev_type;
	uint8_t device_oem;
	char prod_name[7];
	uint8_t prv;
	uint32_t psn;
	uint32_t mdt;
	uint32_t year;
	uint8_t month;

	/* Get the manufacture id */
	hsdmmc->manufacture_id =
		(uint8_t)((sdmmc_resp[3] & MANUFACTURING_ID_MASK)
						>> MANUFACTURING_ID_SHIFT);
	pr_debug("Manufacture id is %d\n", hsdmmc->manufacture_id);

	/* Get Device Type */
	dev_type = (uint8_t)((sdmmc_resp[3] & DEVICE_TYPE_MASK)
						>> DEVICE_TYPE_SHIFT);
	switch (dev_type) {
	case 0:
		pr_debug("Device Type: %s\n", DEVICE_TYPE0);
		break;
	case 1:
		pr_debug("Device Type: %s\n", DEVICE_TYPE1);
		break;
	case 2:
		pr_debug("Device Type: %s\n", DEVICE_TYPE2);
			break;
	case 3:
		pr_debug("Device Type: %s\n", DEVICE_TYPE3);
		break;
	default:
		break;
	};

	/* Get Device OEM */
	device_oem = (uint8_t)(sdmmc_resp[3] & DEVICE_OEM_MASK);
	pr_debug("Device OEM: %d\n", device_oem);

	/* Get Product name */
	/* Extracting bits 103:96 of response register */
	prod_name[0] = sdmmc_resp[2] >> 24;
	/* Extracting bits 95:88 of response register */
	prod_name[1] = (sdmmc_resp[2] >> 16) & 0xFF;
	/* Extracting bits 87:80 of response register */
	prod_name[2] = (sdmmc_resp[2] >> 8) & 0xFF;
	/* Extracting bits 79:72 of response register */
	prod_name[3] = sdmmc_resp[2] & 0xFF;
	/* Extracting bits 71:64 of response register */
	prod_name[4] = sdmmc_resp[1] >> 24;
	/* Extracting bits 63:56 of response register */
	prod_name[5] = (sdmmc_resp[1] >> 16) & 0xFF;
	prod_name[6] = '\0';
	pr_debug("Product Name: %s\n", prod_name);

	/*Get product revision */
	prv = (uint8_t)((sdmmc_resp[1] & PRV_MASK) >> PRV_SHIFT);
	pr_debug("Product revision: %d . %d\n", prv >> 4, prv & 0xF);

	/* Get product serial number */
	psn = (uint32_t)((sdmmc_resp[1] & 0xFF) << 24);
	psn = psn + (uint32_t)(sdmmc_resp[0] >> 8);
	pr_debug("Product serial number: %d\n", psn);

	/* Get product manufacturing date */
	mdt = (uint8_t)(sdmmc_resp[0] & MDT_MASK);
	year = 2013 + (mdt & 0xF);
	month = mdt >> 4;
	pr_debug("Manufacturing date (mm/yyyy): %d / %d\n", month, year);

	return ret;
}
#endif

tegrabl_error_t sdmmc_parse_csd(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t *sdmmc_resp;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t c_size;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	sdmmc_resp = &hsdmmc->response[0];

	/* Get the supported maximum block size. */
	hsdmmc->block_size_log2 =
		((CSD_READ_BL_LEN_MASK & sdmmc_resp[2]) >> CSD_READ_BL_LEN_SHIFT);

	/* Force SDMMC block-size to max supported */
	if (hsdmmc->block_size_log2 < SDMMC_BLOCK_SIZE_LOG2) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	hsdmmc->block_size_log2 = SDMMC_BLOCK_SIZE_LOG2;

	pr_trace("Block size is %lu\n", SDMMC_CONTEXT_BLOCK_SIZE(hsdmmc));

	/* Get the spec_version. */
	hsdmmc->spec_version =
		(uint8_t)((CSD_SPEC_VERS_MASK & sdmmc_resp[3]) >> CSD_SPEC_VERS_SHIFT);

	/* Get the max speed for init. */
	hsdmmc->tran_speed =
		(uint32_t)((CSD_TRAN_SPEED_MASK & sdmmc_resp[2]) >> CSD_TRAN_SPEED_SHIFT);

	/* Capacity of the device = (C_SIZE + 1) * 512 * 1024 bytes */
	c_size = (sdmmc_resp[SD_SDHC_CSIZE_WORD] & SD_SDHC_CSIZE_MASK)
					>> SD_SDHC_CSIZE_SHIFT;
	hsdmmc->user_blocks = (c_size + 1U) * SD_SDHC_CSIZE_MULTIPLIER;

	/* Enable max clock required for init. */
	/*TODO: get card clock divisor from some API. */
	if (hsdmmc->tran_speed == CSD_V4_3_TRAN_SPEED) {
		error = sdmmc_set_card_clock(hsdmmc, MODE_INIT, 8);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	} else {
		/*TODO: get card clock divisor from some API. */
		error  = sdmmc_set_card_clock(hsdmmc, MODE_INIT, 8);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
	pr_trace("sdmmc internal clock enabled\n");

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Parse CSD failed, error = %x\n", error);
	}
	return error;
}

/** @brief Check if the last command was successful or not.
 *
 *  @param index Index of the last or next command.
 *  @param after_cmd_execution Tells if the next command or last command
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_verify_response(sdmmc_cmd index,
	uint8_t after_cmd_execution, struct tegrabl_sdmmc *hsdmmc)
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

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
		goto fail;
	}

	/* Store response buffer in temporary pointer. */
	response = &hsdmmc->response[0];

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
		if (after_cmd_execution == 0U) {
			/* This is during response time. */
			if ((address_out_of_range != 0U) || (address_misaligned != 0U) ||
				(block_length_error != 0U) || (card_internal_error != 0U)) {
				error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
				goto fail;
			}
		} else if ((cmd_crc_error != 0U) || (illegal_command != 0U) ||
						(card_ecc_error != 0U)) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
			goto fail;
		} else {
			/* No Action Required */
		}
		break;
	}
	/* Check for set block length command failure. */
	case CMD_SET_BLOCK_LENGTH: {
		if ((after_cmd_execution == 0U) &&
			((block_length_error != 0U) || (card_internal_error != 0U))) {
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
		if ((after_cmd_execution == 0U) && (card_internal_error != 0U)) {
			error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 3);
			goto fail;
		}
		break;
	}

	/* Check for switch command failure. */
	case CMD_SWITCH: {
		if ((after_cmd_execution != 0U) &&
			((switch_error != 0U) || (cmd_crc_error != 0U))) {
			/* If set, the card did not switch to the expected mode as */
			/* requested by the SWITCH command. */
			error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 4);
			goto fail;
		}
		break;
	}
	/* Check for query ext-csd register command failure. */
	case CMD_SEND_EXT_CSD: {
		if ((after_cmd_execution == 0U) && (card_internal_error != 0U)) {
			error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 5);
			goto fail;
		}
		break;
	}
	/* Check for erase command failure. */
	case CMD_ERASE: {
		if ((after_cmd_execution != 0U) && (erase_error != 0U)) {
			error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 6);
			goto fail;
		}
		break;
	}
	/* Check whether card is in programming mode or not. */
	case CMD_SEND_STATUS: {
		if (after_cmd_execution != 0U) {
			if (((response[0] & CS_TRANSFER_STATE_MASK) >> CS_TRANSFER_STATE_SHIFT) == STATE_PRG) {
				error = TEGRABL_ERROR(TEGRABL_ERR_COMMAND_FAILED, 7);
			} else {
				error = TEGRABL_NO_ERROR;
			}
		}
		break;
	}
	default:
		break;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("sdmmc verify response: exit, error = %x\n", error);
	}
	return error;
}

tegrabl_error_t sdmmc_card_transfer_mode(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t *sdmmc_resp;
	uint32_t card_state;
	tegrabl_error_t error;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 7);
		goto fail;
	}

	sdmmc_resp = &hsdmmc->response[0];
	card_state =
		(CS_CURRENT_STATE_MASK & sdmmc_resp[0]) >> CS_CURRENT_STATE_SHIFT;

	/* return if card is in transfer state or not */
	if (card_state == STATE_TRAN) {
		error = TEGRABL_NO_ERROR;
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 1);
	}
fail:
	return error;
}

/** @brief Gets the power class and fill appropriate hsdmmc
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return power_class returns the power class data.
 */
static uint32_t sdmmc_get_power_class(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t power_class;

	/* Set power class for ddr mode from getextcsd data & width supported. */
	if (hsdmmc->is_ddr_mode != 0U) {
		power_class = (hsdmmc->is_high_voltage_range != 0U) ?
			hsdmmc->power_class_52MHz_ddr360V :
			hsdmmc->power_class_52MHz_ddr195V;
	} else if (hsdmmc->is_high_voltage_range != 0U) {
		power_class = (hsdmmc->high_speed_mode != 0U) ?
			hsdmmc->power_class_52MHz_360V :
			hsdmmc->power_class_26MHz_360V;
	} else {
		power_class = (hsdmmc->high_speed_mode != 0U) ?
			hsdmmc->power_class_52MHz_195V :
			hsdmmc->power_class_26MHz_195V;
	}
	/*
	 * In the above power class, lower 4 bits give power class requirement for
	 * for 4-bit data width and upper 4 bits give power class requirement for
	 * for 8-bit data width.
	 */
	if ((hsdmmc->data_width == DATA_WIDTH_4BIT) ||
		(hsdmmc->data_width == DATA_WIDTH_DDR_4BIT)) {
		power_class = (power_class >> ECSD_POWER_CLASS_4_BIT_OFFSET) &
					ECSD_POWER_CLASS_MASK;
	} else if ((hsdmmc->data_width == DATA_WIDTH_8BIT) ||
			(hsdmmc->data_width == DATA_WIDTH_DDR_8BIT)) {
		power_class = (power_class >> ECSD_POWER_CLASS_8_BIT_OFFSET) &
			ECSD_POWER_CLASS_MASK;
	} else { /*if (hsdmmc->data_width == Sdmmcdata_width_1Bit) */
		power_class = 0;
	}
	return power_class;
}

/** @brief Sends switch command with appropriate argument.
 *
 *  @param cmd_arg Argument for the switch command.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if switch command send & verify passes.
 */
static tegrabl_error_t sdmmc_send_switch_command(uint32_t cmd_arg,
	struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_trace("Send switch command\n");

	/* Sends the switch command. */
	error = sdmmc_send_command(CMD_SWITCH,
				 cmd_arg, RESP_TYPE_R1B, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Send status to the controller. */
	error = sdmmc_send_command(CMD_SEND_STATUS,
				 hsdmmc->card_rca, RESP_TYPE_R1, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Verify the response of the switch command. */
	error = sdmmc_verify_response(CMD_SWITCH, 1, hsdmmc);
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
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 */
static void sdmmc_get_trim_sectors(bnum_t *start_sector, bnum_t *num_sector,
	uint32_t *arg, struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t unalign_start = *start_sector % hsdmmc->erase_group_size;
	uint32_t unalign_count = *num_sector % hsdmmc->erase_group_size;
	uint32_t temp;

	*arg = 0x0;

	if (unalign_start != 0U) {
		temp = hsdmmc->erase_group_size - unalign_start;
		if (*num_sector > temp) {
			*num_sector = temp;
		}
		*arg = 0x1;
	}

	if ((unalign_count != 0U) && (unalign_start == 0U)) {
		if (*num_sector < hsdmmc->erase_group_size) {
			*arg = 0x1;
		} else {
			*num_sector = (*num_sector / hsdmmc->erase_group_size) *
							hsdmmc->erase_group_size;
		}
	}
}

/** @brief Sets the power class and fill appropriate hsdmmc
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_set_power_class(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t cmd_arg;
	uint32_t power_class;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Return if card version is less than 4. */
	if (hsdmmc->spec_version < 4U) {
		return TEGRABL_NO_ERROR;
	}
	/* Queries the power class. */
	power_class = sdmmc_get_power_class(hsdmmc);

	/* Select best possible configuration here. */
	while (power_class > hsdmmc->max_power_class_supported) {
		if ((hsdmmc->data_width == DATA_WIDTH_8BIT) ||
				(hsdmmc->data_width == DATA_WIDTH_DDR_8BIT)) {
			hsdmmc->data_width = DATA_WIDTH_4BIT;
		} else if ((hsdmmc->data_width == DATA_WIDTH_4BIT) ||
				(hsdmmc->data_width == DATA_WIDTH_DDR_4BIT)) {
			hsdmmc->data_width = DATA_WIDTH_1BIT;
		} else {
			/* No Action Required */
		}
		power_class = sdmmc_get_power_class(hsdmmc);
	}

	if (power_class != 0U) {
		pr_trace("Set Power Class to %d\n", power_class);
		cmd_arg = SWITCH_SELECT_POWER_CLASS_ARG |
			(power_class << SWITCH_SELECT_POWER_CLASS_OFFSET);
		error = sdmmc_send_switch_command(cmd_arg, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

fail:
	return error;
}

/** @brief Queries extended csd register and fill appropriate hsdmmc
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_get_ext_csd(struct tegrabl_sdmmc *hsdmmc)
{
	sdmmc_device_status dev_status;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t *buf = hsdmmc->ext_csd_buffer_address;
	uint32_t user_blocks;
	dma_addr_t dma_addr;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 8);
		goto fail;
	}

	/* Set the number of blocks to be read as 1. */
	pr_trace("Setting block to read as 1\n");
	sdmmc_set_num_blocks(SDMMC_CONTEXT_BLOCK_SIZE(hsdmmc), 1, hsdmmc);

	dma_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SDMMC,
									  (uint8_t)(hsdmmc->controller_id), buf,
									  sizeof(hsdmmc->ext_csd_buffer_address),
									  TEGRABL_DMA_FROM_DEVICE);

	/* Write the dma address. */
	pr_trace("SDMA buffer address\n");
	sdmmc_setup_dma(dma_addr, hsdmmc);

	/* Send extended csd command. */
	pr_trace("send ext CSD command\n");
	error = sdmmc_send_command(CMD_SEND_EXT_CSD,
				 0, RESP_TYPE_R1, 1, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Verify the input response. */
	pr_trace("Verify the input response\n");

	error = sdmmc_verify_response(CMD_SEND_EXT_CSD, 0,  hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Register as I/O in progress. */
	hsdmmc->device_status = DEVICE_STATUS_IO_PROGRESS;
	hsdmmc->read_start_time = tegrabl_get_timestamp_ms();

	/* Loop till I/O is in progress. */
	do {
		dev_status = sdmmc_query_status(hsdmmc);
	} while ((dev_status == DEVICE_STATUS_IO_PROGRESS));

	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SDMMC,
							 (uint8_t)(hsdmmc->controller_id), buf,
							 sizeof(hsdmmc->ext_csd_buffer_address),
							 TEGRABL_DMA_FROM_DEVICE);

	/* Check if device is in idle mode or not. */
	pr_trace("Device check for idle %d\n", dev_status);
	if (dev_status != DEVICE_STATUS_IDLE) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BUSY, 0);
		goto fail;
	}

	/* Number of sectors in each boot partition. */
	hsdmmc->boot_blocks =
		((uint32_t)buf[ECSD_BOOT_PARTITION_SIZE_OFFSET] << 17) /
		SDMMC_CONTEXT_BLOCK_SIZE(hsdmmc);
	pr_trace("Boot blocks=%d\n", hsdmmc->boot_blocks);

	/* Number of 256byte blocks in rpmb partition. */
	hsdmmc->rpmb_blocks =
		((uint32_t)buf[ECSD_RPMB_SIZE_OFFSET] << 17) / RPMB_DATA_SIZE;

	pr_trace("RPMB blocks=%d\n", hsdmmc->rpmb_blocks);

	/* Store the number of user partition sectors. */
	if (hsdmmc->is_high_capacity_card != 0U) {
		user_blocks =
			(buf[ECSD_SECTOR_COUNT_0_OFFSET] |
			(((uint32_t)buf[ECSD_SECTOR_COUNT_1_OFFSET]) << 8) |
			(((uint32_t)buf[ECSD_SECTOR_COUNT_2_OFFSET]) << 16) |
			(((uint32_t)buf[ECSD_SECTOR_COUNT_3_OFFSET]) << 24));
		hsdmmc->user_blocks = user_blocks;
		pr_trace("user blocks=%d\n", hsdmmc->user_blocks);
	}

	/* Store the power class. */
	hsdmmc->power_class_26MHz_360V =
		buf[ECSD_POWER_CL_26_360_OFFSET];
	hsdmmc->power_class_52MHz_360V =
		buf[ECSD_POWER_CL_52_360_OFFSET];
	hsdmmc->power_class_26MHz_195V =
		buf[ECSD_POWER_CL_26_195_OFFSET];
	hsdmmc->power_class_52MHz_195V =
		buf[ECSD_POWER_CL_52_195_OFFSET];
	hsdmmc->power_class_52MHz_ddr360V =
		buf[ECSD_POWER_CL_DDR_52_360_OFFSET];
	hsdmmc->power_class_52MHz_ddr195V =
		buf[ECSD_POWER_CL_DDR_52_195_OFFSET];

	/* Store extended csd revision */
	hsdmmc->ext_csd_rev = buf[ECSD_REV];

	/* Store the current speed supported by card */
	hsdmmc->card_support_speed = buf[ECSD_CARD_TYPE_OFFSET];

	/* Store the boot configs. */
	hsdmmc->boot_config = buf[ECSD_BOOT_CONFIG_OFFSET];

	/* Store if sanitize command is supported or not. */
	hsdmmc->sanitize_support =
		(buf[ECSD_SEC_FEATURE_OFFSET] &
			ECSD_SEC_SANITIZE_MASK) >> ECSD_SEC_SANITIZE_SHIFT;

	/* Store the high capacity erase group size. */
	hsdmmc->erase_group_size = (uint32_t)buf[ECSD_ERASE_GRP_SIZE] << 10;

	/* Store the high capacity erase timeout for max erase. */
	hsdmmc->erase_timeout_us =
			300000U * buf[ECSD_ERASE_TIMEOUT_OFFSET] *
						(MAX_ERASABLE_SECTORS / hsdmmc->erase_group_size);

	pr_trace("Timeout is 0x%x erase group is 0x%x\n", hsdmmc->erase_timeout_us, hsdmmc->erase_group_size);

	pr_trace("card_support_speed = %d\n", hsdmmc->card_support_speed);

	/* Store the current bus width. */
	hsdmmc->card_bus_width = buf[ECSD_BUS_WIDTH];

	pr_trace("card_bus_width = %d\n", hsdmmc->card_bus_width);

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("get ext CSD failed, error = %x\n", error);
	}
	return error;
}

/** @brief Erase operation from start sector till the count of sector.
 *
 *  @param dev Bio layer handle for device to be erased.
 *  @param block Start of physical sector to be erased.
 *  @param count Number of physical sectors to be erased.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_erase_block(bnum_t block, bnum_t count,
	struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	bnum_t num_sectors = count;
	bnum_t start_sector = block;
	bnum_t end_sector;
	bnum_t temp_start_sector;
	bnum_t temp_num_sector;
	uint32_t arg = 0x0;

	/* Send switch command to enable high capacity erase. */
	pr_trace("Configure for high capacity erase\n");
	error = sdmmc_send_switch_command(SWITCH_HIGH_CAPACITY_ERASE_ARG, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	while (num_sectors > 0U) {
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

		pr_trace("Pseudo start sector %d count %d\n", start_sector, end_sector);

		/* For boot partitions determine correct physical sectors. */
		if (hsdmmc->current_access_region ==
					BOOT_PARTITION_1 ||
						hsdmmc->current_access_region ==
								BOOT_PARTITION_2) {
			error = sdmmc_get_correct_boot_block(&temp_start_sector,
					   &temp_num_sector, hsdmmc);
			if (error != TEGRABL_NO_ERROR) {
				TEGRABL_SET_HIGHEST_MODULE(error);
				pr_trace("Query correct boot blocks failed\n");
				goto fail;
			}
		}
		/* Get the sectors for performing trim. */
		sdmmc_get_trim_sectors(&temp_start_sector,
							   &temp_num_sector, &arg, hsdmmc);

		pr_trace("Actual start %d actual count %d for region %d\n",
				 temp_start_sector, temp_num_sector, hsdmmc->current_access_region);

		/* Send erase group start command. */
		pr_trace("Send erase group start command\n");
		error = sdmmc_send_command(CMD_ERASE_GROUP_START,
											 temp_start_sector,
											 RESP_TYPE_R1, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Send erase group end command. */
		pr_trace("Send erase group end command\n");
		error = sdmmc_send_command(CMD_ERASE_GROUP_END,
					temp_start_sector + temp_num_sector - 1U,
					RESP_TYPE_R1, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Send erase command with trim or high capacity erase arg. */
		pr_trace("Send erase command with %d arg\n", arg);
		error = sdmmc_send_command(CMD_ERASE,
					 arg, RESP_TYPE_R1B, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Verify the response of the erase command. */
		pr_trace("Verify the response\n");
		error = sdmmc_verify_response(CMD_ERASE, 1, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Check if the card is in programming state or not. */
		pr_trace("Send status command\n");
		do {
			error = sdmmc_send_command(CMD_SEND_STATUS,
						 hsdmmc->card_rca, RESP_TYPE_R1, 0, hsdmmc);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
			if (sdmmc_verify_response(CMD_SEND_STATUS, 1, hsdmmc) != TEGRABL_NO_ERROR) {
				continue;
			} else {
				break;
			}
		} while (true);

		/* Update the sector start & number. */
		num_sectors -= temp_num_sector;
		start_sector += temp_num_sector;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("Erase block failed, error = %x\n", error);
	}
	return error;
}

/** @brief Initializes the card by following SDMMC protocol.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_identify_card(struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Check if card is present and stable. */
	pr_trace("Check card present and stable\n");
	if (sdmmc_is_card_present(hsdmmc) != TEGRABL_NO_ERROR) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 2);
	}

	/* Send command 0. */
	pr_trace("Send command 0\n");
	error = sdmmc_send_command(CMD_IDLE_STATE, 0,
				 RESP_TYPE_NO_RESP, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Get the operating conditions. */
	pr_trace("Send command 1\n");
	error = sdmmc_get_operating_conditions(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Request for all the available cids. */
	pr_trace("Send command 2\n");
	error = sdmmc_send_command(CMD_ALL_SEND_CID, 0,
										 RESP_TYPE_R2, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Assign the relative card address. */
	pr_trace("Send command 3\n");
	error = sdmmc_send_command(CMD_SET_RELATIVE_ADDRESS,
				 hsdmmc->card_rca, RESP_TYPE_R1, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Query the csd. */
	pr_trace("Query card specific data by command 9\n");
	error = sdmmc_send_command(CMD_SEND_CSD,
				 hsdmmc->card_rca, RESP_TYPE_R2, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Store the hsdmmc by parsing csd. */
	pr_trace("Parse CSD data\n");
	error = sdmmc_parse_csd(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

#if defined(CONFIG_ENABLE_BLOCKDEV_DEVICE_INFO)
	/* Query the cid. */
	pr_trace("Query CID by command 10\n");
	error = sdmmc_send_command(CMD_SEND_CID,
				hsdmmc->card_rca, RESP_TYPE_R2, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Store the hsdmmc by parsing cid. */
	pr_trace("Parse CID data\n");
	error = sdmmc_parse_cid(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
#endif

	/* Select the card for data transfer. */
	pr_trace("Send command 7\n");
	error = sdmmc_send_command(CMD_SELECT_DESELECT_CARD,
				 hsdmmc->card_rca, RESP_TYPE_R1, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Check if card is in data transfer mode or not. */
	pr_trace("Check if card is in transfer mode\n");
	error = sdmmc_send_command(CMD_SEND_STATUS,
				 hsdmmc->card_rca, RESP_TYPE_R1, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	error = sdmmc_card_transfer_mode(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Query the extended csd registers. */
	pr_trace("ext CSD register read\n");
	error = sdmmc_get_ext_csd(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Set the power class. */
	pr_trace("Set power class\n");
	error = sdmmc_set_power_class(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Enable high speed if supported. */
	pr_trace("Enable high speed\n");
	error = sdmmc_enable_high_speed(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
fail:
	return error;
}

/** @brief Read/write from the input block till the count of blocks.
 *
 *  @param block Start sector for read/write.
 *  @param count Number of sectors to be read/write.
 *  @param buf Input buffer for read/write.
 *  @param is_write Is the command is for write or not.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_block_io(bnum_t block, bnum_t count, uint8_t *buf,
	uint8_t is_write, struct tegrabl_sdmmc *hsdmmc, bool is_non_blocking)
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

	if ((hsdmmc == NULL) || (buf == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 9);
		goto fail;
	}

	/* Decide which command is to be send. */
	if (is_write != 0U) {
		cmd = CMD_WRITE_MULTIPLE;
	} else {
		cmd = CMD_READ_MULTIPLE;
	}

	/* Enable block length setting if not DDR mode. */
	if ((hsdmmc->data_width == DATA_WIDTH_4BIT) ||
		(hsdmmc->data_width == DATA_WIDTH_8BIT)) {
		/* Send SET_BLOCKLEN(CMD16) Command. */
		error = sdmmc_send_command(CMD_SET_BLOCK_LENGTH,
								   SDMMC_CONTEXT_BLOCK_SIZE(hsdmmc),
								   RESP_TYPE_R1, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			pr_error("Setting block length failed\n");
			goto fail;
		}

		error = sdmmc_verify_response(CMD_SET_BLOCK_LENGTH, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
	/* Store start and end sectors in temporary variable. */
	residue_num_sectors = count;
	residue_start_sector = block;

	while (residue_num_sectors > 0U) {
		current_start_sector = residue_start_sector;
		current_num_sectors = residue_num_sectors;

		/* Check if data line is ready for transfer. */
		if (sdmmc_wait_for_data_line_ready(hsdmmc) != TEGRABL_NO_ERROR) {
			error = sdmmc_recover_controller_error(hsdmmc, 1);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}
		pr_trace("residue_start_sector = %d, residue_num_sectors = %d\n",
				 residue_start_sector, residue_num_sectors);

		/* Select access region. This will change start & num sector */
		/* based on the region the request falls in. */

		if ((hsdmmc->current_access_region ==
					BOOT_PARTITION_1) ||
				(hsdmmc->current_access_region ==
					BOOT_PARTITION_2)) {
			error = sdmmc_get_correct_boot_block(
						&current_start_sector,
						&current_num_sectors, hsdmmc);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}
		pr_trace("current_access_region = %d\n", hsdmmc->current_access_region);

		pr_trace("actual_start_sector = %d, actual_num_sectors = %d\n",
				 current_start_sector, current_num_sectors);

		/* Set number of blocks to read or write. */
		sdmmc_set_num_blocks(SDMMC_CONTEXT_BLOCK_SIZE(hsdmmc),
							 current_num_sectors, hsdmmc);

		/* Set up command arg. */
		cmd_arg = (uint32_t) current_start_sector;

		pr_trace("cur_Start_Sector = %d, cur_num_sectors = %d,cmd_arg = %d\n", current_start_sector,
				 current_num_sectors, cmd_arg);

		dma_dir = (is_write != 0U) ? TEGRABL_DMA_TO_DEVICE : TEGRABL_DMA_FROM_DEVICE;
		dma_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_SDMMC,
			(uint8_t)(hsdmmc->controller_id), buf,
			current_num_sectors << hsdmmc->block_size_log2, dma_dir);

		/* Setup Dma. */
		pr_trace("SDMA buffer address\n");
		sdmmc_setup_dma(dma_addr, hsdmmc);

		/* Send command to Card. */
		error = sdmmc_send_command(cmd, cmd_arg, RESP_TYPE_R1, 1, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* If response fails, return error. Nothing to clean up. */
		error = sdmmc_verify_response(cmd, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		hsdmmc->device_status = DEVICE_STATUS_IO_PROGRESS;
		hsdmmc->read_start_time = tegrabl_get_timestamp_ms();

		/* Update the start sectors and num sectors accordingly. */
		residue_num_sectors -= current_num_sectors;
		residue_start_sector += current_num_sectors;

		if ((residue_num_sectors == 0U) && (is_non_blocking == true)) {
			hsdmmc->last_io_buf = buf;
			hsdmmc->last_io_num_sectors = current_num_sectors;
			hsdmmc->last_io_dma_dir = dma_dir;
			break;
		}

		/* Wait for idle condition. */
		while (sdmmc_query_status(hsdmmc) == DEVICE_STATUS_IO_PROGRESS) {
			;
		}
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SDMMC,
							(uint8_t)(hsdmmc->controller_id), buf,
							current_num_sectors << hsdmmc->block_size_log2,
							dma_dir);

		/* Error out if device is not idle. */
		if (sdmmc_query_status(hsdmmc) != DEVICE_STATUS_IDLE) {
			error = TEGRABL_ERROR(TEGRABL_ERR_BUSY, 1);
			pr_info("Device is not idle\n");
			goto fail;
		}

		buf += (current_num_sectors << hsdmmc->block_size_log2);
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("Block IO: exit, error = %x\n", error);
	}
	return error;
}

tegrabl_error_t sdmmc_xfer_wait(struct tegrabl_blockdev_xfer_info *xfer, time_t timeout, uint8_t *status)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	sdmmc_device_status dev_status;
	struct tegrabl_sdmmc *hsdmmc;
	sdmmc_priv_data_t *priv_data;
	tegrabl_bdev_t *dev;
	uint8_t *pbuf;
	bnum_t i, current_sectors;
	time_t start_time, end_time;
	uint8_t is_write;

	dev = xfer->dev;

	priv_data = (sdmmc_priv_data_t *)dev->priv_data;
	hsdmmc = (struct tegrabl_sdmmc *) priv_data->context;

	i = hsdmmc->last_xfer_blocks;
	pbuf = hsdmmc->last_xfer_buf;

	start_time = tegrabl_get_timestamp_us();
	while (true) {
		do {
			dev_status = sdmmc_query_status(hsdmmc);
			end_time = tegrabl_get_timestamp_us();
			if ((end_time - start_time) > timeout) {
				*status = TEGRABL_BLOCKDEV_XFER_IN_PROGRESS;
				hsdmmc->last_xfer_blocks = i;
				hsdmmc->last_xfer_buf = pbuf;
				goto fail;
			}
		}  while ((dev_status == DEVICE_STATUS_IO_PROGRESS));

		if (dev_status != DEVICE_STATUS_IDLE) {
			error = TEGRABL_ERROR(TEGRABL_ERR_BUSY, 2);
			goto fail;
		}

		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SDMMC, (uint8_t)hsdmmc->controller_id, hsdmmc->last_io_buf,
			hsdmmc->last_io_num_sectors << hsdmmc->block_size_log2, hsdmmc->last_io_dma_dir);

		if ((i < xfer->block_count) != true) {
			break;
		}

		current_sectors = ((xfer->block_count - i) > MAX_SDMA_TRANSFER) ?
			 MAX_SDMA_TRANSFER : (xfer->block_count - i);

		is_write = (xfer->xfer_type == TEGRABL_BLOCKDEV_WRITE) ? 1U : 0U;

		error = sdmmc_block_io(xfer->start_block + i, current_sectors, pbuf, is_write, hsdmmc, true);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		i += current_sectors;
		pbuf += (current_sectors << hsdmmc->block_size_log2);
	}
	*status = TEGRABL_BLOCKDEV_XFER_COMPLETE;

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Data transfer failed\n");
	}
	return error;
}

/** @brief Reset the controller registers and enable internal clock at 400 KHz.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param instance Instance of the controller to be initialized.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_init_controller(struct tegrabl_sdmmc *hsdmmc, uint32_t instance)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Reset the registers of the controller. */
	pr_trace("Reset controller at base\n");
	error = sdmmc_reset_controller(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Reset controller registers failed\n");
		goto fail;
	}

#if defined(CONFIG_ENABLE_SDMMC_64_BIT_SUPPORT)
	/* Enable host controller v4 */
	error = sdmmc_enable_hostv4(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		pr_error("Enable hostv4 failed\n");
		goto fail;
	}
#endif

	/* Enable IO spare registers */
	pr_trace("Update IO spare registers\n");
	error = sdmmc_io_spare_update(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	sdmmc_set_tap_trim(hsdmmc);

	/* Enable Auto-Calibration */
	pr_trace("Perform auto-calibration\n");
	error = sdmmc_auto_calibrate(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		sdmmc_update_drv_settings(hsdmmc, instance);
	}

	/* Enable the clock oscillator with DIV64 divider. */
	pr_trace("Enable internal clock\n");
	error = sdmmc_set_card_clock(hsdmmc, MODE_POWERON, 128);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Enable the bus power. */
	pr_trace("Enable bus power\n");
	error = sdmmc_enable_bus_power(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Set the error interrupt mask. */
	pr_trace("Set interrupt status reg\n");
	error = sdmmc_set_interrupt_status_reg(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Set the clock below 400 KHz for initialization of card. */
	pr_trace("Init clk for card initialization\n");
	error = sdmmc_set_card_clock(hsdmmc, MODE_INIT, 128);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Set the data width to 1. */
	pr_trace("Setting data width to 1\n");
	error = sdmmc_set_data_width(DATA_WIDTH_1BIT, hsdmmc);

fail:
	return error;
}

static tegrabl_error_t sdmmc_check_is_trans_mode(struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t instance;

	instance = (uint8_t)hsdmmc->controller_id;
	error = tegrabl_car_clk_enable(TEGRABL_MODULE_SDMMC, instance, NULL);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	error = tegrabl_car_rst_clear(TEGRABL_MODULE_SDMMC, instance);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	hsdmmc->card_rca = (2U << RCA_OFFSET);

	/* Send status to the controller. */
	error = sdmmc_send_command(CMD_SEND_STATUS,
				 hsdmmc->card_rca, RESP_TYPE_R1, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Verify the response of the switch command. */
	error = sdmmc_verify_response(CMD_SWITCH, 1, hsdmmc);
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
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param flag sdmmc init flag
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_init(uint32_t instance, struct tegrabl_sdmmc *hsdmmc, uint8_t flag)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 11);
		goto fail;
	}

	/* Check if the device is already initialized. */
	if ((hsdmmc->initialized == true) && (flag != SKIP_INIT_UPDATE_CONFIG)) {
		pr_info("sdmmc is already initialised\n");
		return TEGRABL_NO_ERROR;
	}

	/* Store the default init parameters. */
	pr_trace("Set default hsdmmc\n");
	sdmmc_set_default_hsdmmc(hsdmmc, instance);

	/* TODO: Handle the case if sdmmc is initialized with different
	 * mode than what hsdmmc is configured.
	 */
	if (((flag == SKIP_INIT) || (flag == SKIP_INIT_UPDATE_CONFIG)) &&
			(sdmmc_check_is_trans_mode(hsdmmc) == TEGRABL_NO_ERROR)) {
		sdmmc_get_hostv4_status(hsdmmc);
#if !defined(CONFIG_ENABLE_SDMMC_64_BIT_SUPPORT)
		if (hsdmmc->is_hostv4_enabled == true) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 8);
			pr_error("Unsupported sdmmc state, 64-bit not supported\n");
			goto fail;
		}
#endif
		pr_trace("ext CSD register read\n");
		error = sdmmc_get_ext_csd(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		if (flag == SKIP_INIT_UPDATE_CONFIG) {
			/* Setup card for data transfer. */
			pr_trace("Set card for data transfer\n");
			error = sdmmc_select_mode_transfer(hsdmmc);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
			/* Setup the default region of operation as user partition. */
			pr_trace("Set default region as user partition\n");
			if (hsdmmc->device_type != DEVICE_TYPE_SD) {
				error = sdmmc_set_default_region(hsdmmc);
				if (error != TEGRABL_NO_ERROR) {
					goto fail;
				}
			}
		}
		/* Mark as device is initialized. */
		hsdmmc->initialized = true;
		return TEGRABL_NO_ERROR;
	}

	/* Enable clocks for input sdmmc instance. */
	pr_trace("Enabling clock\n");
	error = sdmmc_clock_init(hsdmmc->controller_id, CLK_102_MHZ,
								hsdmmc->clk_src);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Initiliaze controller. */
	pr_trace("Initialize controller\n");
	error = sdmmc_init_controller(hsdmmc, instance);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Identify card. */
	pr_trace("Identify card\n");

#if defined(CONFIG_ENABLE_SDCARD)
	if (hsdmmc->device_type == DEVICE_TYPE_SD)
		error = sd_identify_card(hsdmmc);
	else
#endif
		error = sdmmc_identify_card(hsdmmc);

	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Setup card for data transfer. */
	pr_trace("Set card for data transfer\n");
	error = sdmmc_select_mode_transfer(hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Setup the default region of operation as user partition. */
	pr_trace("Set default region as user partition\n");
	if (hsdmmc->device_type != DEVICE_TYPE_SD) {
		error = sdmmc_set_default_region(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
	/* Mark as device is initialized. */
	hsdmmc->initialized = true;

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("sdmmc init failed, error = %x\n", error);
	}
	return error;
}

tegrabl_error_t sdmmc_send_cmd0_cmd1(uint32_t instance,
		struct tegrabl_sdmmc_platform_params *emmc_params)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_sdmmc local_hsdmmc = { 0 };
	struct tegrabl_sdmmc *hsdmmc = &local_hsdmmc;

	if (emmc_params == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 7);
		goto fail;
	}

	if (instance >= MAX_SDMMC_INSTANCES) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 6);
		goto fail;
	}

	hsdmmc->clk_src = emmc_params->clk_src;
	hsdmmc->best_mode = emmc_params->best_mode;
	hsdmmc->tap_value = emmc_params->tap_value;
	hsdmmc->trim_value = emmc_params->trim_value;
	hsdmmc->controller_id = instance;

	/* Enable clocks for input sdmmc instance. */
	pr_trace("Enabling clock\n");

	error = sdmmc_clock_init(instance, CLK_102_MHZ,
								hsdmmc->clk_src);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Store the default init parameters. */
	pr_trace("Set default params for hsdmmc\n");
	sdmmc_set_default_hsdmmc(hsdmmc, instance);

	/* Initiliaze controller. */
	pr_trace("Initialize controller\n");
	error = sdmmc_init_controller(hsdmmc, instance);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	/* Check if card is present and stable. */
	pr_trace("Check card present and is stable\n");
	if (sdmmc_is_card_present(hsdmmc) != TEGRABL_NO_ERROR) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 2);
	}
	/* Send command 0. */
	pr_trace("Send command 0\n");
	error = sdmmc_send_command(CMD_IDLE_STATE, 0,
		RESP_TYPE_NO_RESP, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = sdmmc_partial_cmd1_sequence(hsdmmc);
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
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param device User or Boot device to be accessed.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_io(tegrabl_bdev_t *dev, void *buf, bnum_t block,
	bnum_t count, uint8_t is_write, struct tegrabl_sdmmc *hsdmmc,
	sdmmc_device device, bool is_non_blocking)
{
	uint32_t i = 0;
	bnum_t current_sectors = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t *pbuf = buf;

	if ((dev == NULL) || (buf == NULL) || (hsdmmc == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 12);
		goto fail;
	}

	/* Mark the device is idle. */
	hsdmmc->device_status = DEVICE_STATUS_IDLE;
	pr_trace("StartBlock= %d NumofBlock = %d\n", block, count);
	if (hsdmmc->device_type != DEVICE_TYPE_SD) {
		/* Check for the correct region. */
		if (device == DEVICE_BOOT) {
			pr_trace("Looking in boot partitions\n");
			error = sdmmc_select_access_region(hsdmmc, BOOT_PARTITION_1);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
		} else if (device == DEVICE_USER) {
			pr_trace("Looking in user partition\n");
			error = sdmmc_select_access_region(hsdmmc, USER_PARTITION);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
		} else {
			pr_debug("Wrong block to look for\n");
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 13);
			goto fail;
		}
	}

	/* Error check the boundary condition for input sectors. */
	if ((block > (dev->block_count - 1U)) ||
		((block + count) > (dev->block_count))) {
		pr_trace("Block %d outside range with count %u\n", block, count);
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}
	/* Sdma supports maximum of 32 MB of transfer. */
	while (i < count) {
		current_sectors =
			(count - i) > MAX_SDMA_TRANSFER ? MAX_SDMA_TRANSFER : (count - i);

		error = sdmmc_block_io(block + i, current_sectors, pbuf, is_write,
					hsdmmc, is_non_blocking);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		i += current_sectors;
		pbuf += (current_sectors << hsdmmc->block_size_log2);

		if (is_non_blocking == true) {
			hsdmmc->last_xfer_blocks = i;
			hsdmmc->last_xfer_buf = pbuf;
			break;
		}
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("SDMMC IO: exit error = %x\n", error);
	}
	return error;
}

/** @brief Enables high speed mode for card version more than 4.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_enable_high_speed(struct tegrabl_sdmmc *hsdmmc)
{
	uint8_t *buf;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 14);
		goto fail;
	}

	buf = hsdmmc->ext_csd_buffer_address;

	/* Return if card version is less than 4. */
	if (hsdmmc->spec_version < 4U) {
		return TEGRABL_NO_ERROR;
	} else {
		hsdmmc->high_speed_mode = 1;
	}
	/* Clear controller's high speed bit. */
	pr_trace("Clear High Speed bit\n");
	sdmmc_toggle_high_speed(0, hsdmmc);


	/* Enable the High Speed Mode, if required. */
	if (hsdmmc->high_speed_mode != 0U) {
		pr_trace("Set High speed to %d\n",
				hsdmmc->high_speed_mode);

		error = sdmmc_send_switch_command(SWITCH_HIGH_SPEED_ENABLE_ARG,
					hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		/* Set the clock for data transfer. */
		error = sdmmc_set_card_clock(hsdmmc, MODE_DATA_TRANSFER, 1);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		/* Validate high speed mode bit from card here. */
		error = sdmmc_get_ext_csd(hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		if (buf[ECSD_HS_TIMING_OFFSET] != 0U) {
			return TEGRABL_NO_ERROR;
		}
	}
	error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 3);
fail:
	return error;
}

tegrabl_error_t sdmmc_enable_timing_hs400(struct tegrabl_sdmmc *hsdmmc, uint8_t mode)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t cmd_arg;
	uint8_t timing_interface = 0;

	if (mode == TEGRABL_SDMMC_MODE_HS200) {
		timing_interface = TIMING_INTERFACE_HS200;
	} else if (mode == TEGRABL_SDMMC_MODE_HS400) {
		timing_interface = TIMING_INTERFACE_HS400;
	} else {
		timing_interface = TIMING_INTERFACE_HIGH_SPEED;
	}

	cmd_arg = ((uint32_t)WRITE_BYTE << 24) | ((uint32_t)ECSD_HS_TIMING_OFFSET << 16) |
				((uint32_t)timing_interface << 8);
	error = sdmmc_send_switch_command(cmd_arg, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Set the clock for data transfer. */
	error = sdmmc_set_card_clock(hsdmmc, MODE_DATA_TRANSFER, 0);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}

/** @brief Selects the region of access from user or boot partitions.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param region  Select either user or boot region.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_select_access_region(struct tegrabl_sdmmc *hsdmmc,
	sdmmc_access_region region)
{
	uint32_t cmd_arg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t partition_mask;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 15);
		goto fail;
	}

	/* Check access region argument range. */
	if (region >= NUM_PARTITION) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 16);
		goto fail;
	}

	/* Prepare switch command arg for partition switching. */
	partition_mask = SWITCH_SELECT_PARTITION_MASK;
	cmd_arg = hsdmmc->boot_config & (~partition_mask);
	cmd_arg |= (uint32_t)region;
	cmd_arg <<= SWITCH_SELECT_PARTITION_OFFSET;
	cmd_arg |= SWITCH_SELECT_PARTITION_ARG;

	pr_trace("Trying to select the region\n");

	/* Send the switch command  to change the current partitions access. */
	error = sdmmc_send_switch_command(cmd_arg, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Store the access region in hsdmmc. */
	hsdmmc->current_access_region = region;

	pr_debug("Selected access_region = %d\n", region);

fail:
	return error;
}

/** @brief Sets the data bus width for DDR/SDR mode.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_set_bus_width(struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t cmd_arg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t bus_width;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 17);
		goto fail;
	}

	/* Send SWITCH(CMD6) Command to select bus width. */
	pr_debug("DDR Data width = %d,", hsdmmc->data_width);

	/* Prepare argument for switch command to change bus width. */
	bus_width = hsdmmc->data_width | (hsdmmc->enhanced_strobe ? 1UL : 0UL) << 7;
	cmd_arg = SWITCH_BUS_WIDTH_ARG | (bus_width << SWITCH_BUS_WIDTH_OFFSET);

	/* Send the switch command. */
	error = sdmmc_send_switch_command(cmd_arg, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Set the controller register corresponding to bus width. */
	error = sdmmc_set_data_width(hsdmmc->data_width, hsdmmc);

fail:
	return error;
}

/** @brief Performs erase from given offset till the length of sectors.
 *
 *  @param dev Bio device handle in which erase is required.
 *  @param block Starting sector which will be erased.
 *  @param count Total number of sectors which will be erased.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param device User or Boot device to be accessed.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_erase(tegrabl_bdev_t *dev, bnum_t block, bnum_t count,
	struct tegrabl_sdmmc *hsdmmc, sdmmc_device device)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((dev == NULL) || (hsdmmc == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 18);
		goto fail;
	}

	/* check for the correct region */
	if (device == DEVICE_BOOT) {
		pr_trace("looking in boot partitions\n");
		error = sdmmc_select_access_region(hsdmmc, BOOT_PARTITION_1);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	} else if (device == DEVICE_USER) {
		pr_trace("looking in user partitions\n");
		error = sdmmc_select_access_region(hsdmmc, USER_PARTITION);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	} else {
		/* Device is not in user or boot region. */
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 19);
		goto fail;
	}

	/* Check for boundary condition od the input bio device. */
	if ((block > (dev->block_count - 1U)) ||
		((block + count) > (dev->block_count))) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 20);
		goto fail;
	}

	/* Perform erase over the given sectors. */
	error = sdmmc_erase_block(block, count, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Erase command failed, error = %x\n", error);
	}
	return error;
}

/** @brief Performs sanitize operation over unaddressed sectors
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_sanitize(struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 21);
		goto fail;
	}

	/* Perform sanitize if supported. */
	if (hsdmmc->sanitize_support != 0U) {
		pr_trace("Perform sanitizing\n");

		error = sdmmc_send_switch_command(SWITCH_SANITIZE_ARG, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
		/* Check if the card is in programming mode or not. */
		pr_trace("Send status command\n");
		do {
			error = sdmmc_send_command(CMD_SEND_STATUS, hsdmmc->card_rca,
						 RESP_TYPE_R1, 0, hsdmmc);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
			if (sdmmc_verify_response(CMD_SEND_STATUS, 1, hsdmmc) != TEGRABL_NO_ERROR) {
				continue;
			} else {
				break;
			}
		} while (true);
	} else {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 4);
	}
fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_trace("Sanitize command failed, error = %x\n", error);
	}
	return error;
}

tegrabl_error_t sdmmc_send_status(struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (hsdmmc == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 22);
		goto fail;
	}

	/* Send status to the controller. */
	error = sdmmc_send_command(CMD_SEND_STATUS, hsdmmc->card_rca, RESP_TYPE_R1, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Verify the response of the switch command. */
	error = sdmmc_verify_response(CMD_SWITCH, 1, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}
