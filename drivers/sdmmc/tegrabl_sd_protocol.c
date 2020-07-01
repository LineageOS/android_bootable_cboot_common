/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors errain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications theerro.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */
#define MODULE TEGRABL_ERR_SDMMC

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_sdmmc_bdev.h>
#include <tegrabl_sdmmc_defs.h>
#include <tegrabl_sdmmc_card_reg.h>
#include <tegrabl_sd_protocol.h>
#include <tegrabl_sdmmc_protocol.h>
#include <tegrabl_sdmmc_host.h>


/** @brief Initializes the card by following SDMMC protocol.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @errurn TEGRABL_NO_ERROR if card is initiliazed successfully.
 */
tegrabl_error_t sd_identify_card(sdmmc_context_t *context)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t cmd_arg;
	uint32_t ocr_reg;
	uint32_t *sdmmc_response = &(context->response[0]);

	/* Check if card is present and stable. */
	pr_debug("check card present and stablet\\n");
	if (sdmmc_is_card_present(context))
		return TEGRABL_ERR_INVALID;

	/* Send command 0. */
	pr_debug("send command 0\n");
	err = sdmmc_send_command(CMD_IDLE_STATE, 0, RESP_TYPE_NO_RESP, 0, context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("sending cmd 0 failed");
		goto fail;
	}

	/* Send command 8, get the interface condition register */
	cmd_arg = SD_HOST_VOLTAGE_RANGE | SD_HOST_CHECK_PATTERN;
	err = sdmmc_send_command(SD_CMD_SEND_IF_COND, cmd_arg,
				RESP_TYPE_R7, 0, context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("sending CMD_SD_SEND_IF_COND failed");
		goto fail;
	}

	do {
		err = sdmmc_send_command(SD_CMD_APPLICATION, cmd_arg,
					RESP_TYPE_R1, 0, context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("sending CMD_SD_APPLICATION failed");
		goto fail;
	}

	ocr_reg = SD_CARD_OCR_VALUE | CARD_CAPACITY_MASK;
	err = sdmmc_send_command(SD_ACMD_SEND_OP_COND, ocr_reg,
				 RESP_TYPE_R3, 0, context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("sending cmd SdAppCmd_SendOcr failed");
		goto fail;
	}
		ocr_reg = *sdmmc_response;
		/* Indicates no card is present in the slot */
		if (ocr_reg == 0) {
			goto fail;
		}

	} while (!(ocr_reg & (uint32_t)(SD_CARD_POWERUP_STATUS_MASK)));

	if (ocr_reg & SD_CARD_CAPACITY_MASK) {
		context->is_high_capacity_card = true;
	}

	/* Request for all the available cids. */
	err = sdmmc_send_command(CMD_ALL_SEND_CID, 0, RESP_TYPE_R2, 0, context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("sending cid failed");
		goto fail;
	}

	/* Store the context by parsing cid. */
	err = sdmmc_parse_cid(context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("parse cid failed");
		goto fail;
	}

	/* Assign the relative card address. */
	pr_debug("send command 3\n");
	err = sdmmc_send_command(CMD_SET_RELATIVE_ADDRESS, 9, RESP_TYPE_R6, 0,
				context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("assigning rca failed");
		goto fail;
	}

	/* Hard code one rca. */
	pr_debug("set rca for the card\n");
	context->card_rca = *sdmmc_response;

	/* Query the csd. */
	pr_debug("query card specific data by command 9\n");
	err = sdmmc_send_command(CMD_SEND_CSD, context->card_rca, RESP_TYPE_R2,
				 0, context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("query csd failed");
		goto fail;
	}

	/* Store the context by parsing csd. */
	pr_debug("parse csd data\n");
	err = sdmmc_parse_csd(context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("parse csd failed");
		goto fail;
	}

	/* Select the card for data transfer. */
	pr_debug("send command 7\n");
	err = sdmmc_send_command(CMD_SELECT_DESELECT_CARD, context->card_rca,
			 RESP_TYPE_R1, 0, context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("sending cmd7 failed");
		goto fail;
	}

	/* Check if card is in data transfer mode or not. */
	err = sdmmc_send_command(CMD_SEND_STATUS, context->card_rca, RESP_TYPE_R1,
				0, context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("card is not in transfer mode");
		goto fail;
	}

	err = sdmmc_card_transfer_mode(context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("setting card to transfer mode failed");
		goto fail;
	}

	/* Send ACMD6 to Set bus width to Four bit wide.*/
	err = sdmmc_send_command(SD_CMD_APPLICATION, context->card_rca,
				RESP_TYPE_R1, 0, context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Command_ApplicationCommand transfer mode\n");
		goto fail;
	}

	err = sdmmc_send_command(SD_ACMD_SET_BUS_WIDTH, SD_BUS_WIDTH_4BIT,
				RESP_TYPE_R1, 0, context);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("SdAppCmd_SetBusWidth transfer mode\n");
		goto fail;
	}

	/* Now change the Host bus width as well */
	context->data_width = DATA_WIDTH_4BIT;
	sdmmc_set_data_width(DATA_WIDTH_4BIT, context);

	/* Only data region on SD card */
	context->current_access_region = 0;

fail:
	return err;
}

