/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
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
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @errurn TEGRABL_NO_ERROR if card is initiliazed successfully.
 */
tegrabl_error_t sd_identify_card(struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t cmd_arg;
	uint32_t ocr_reg;
	uint32_t *sdmmc_response = &(hsdmmc->response[0]);

	/* Check if card is present and stable. */
	pr_trace("Check card present and stable\n");
	if (sdmmc_is_card_present(hsdmmc))
		return TEGRABL_ERR_INVALID;

	/* Send command 0. */
	pr_trace("Send command 0\n");
	err = sdmmc_send_command(CMD_IDLE_STATE, 0, RESP_TYPE_NO_RESP, 0, hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Sending cmd 0 failed\n");
		goto fail;
	}

	/* Send command 8, get the interface condition register */
	cmd_arg = SD_HOST_VOLTAGE_RANGE | SD_HOST_CHECK_PATTERN;
	err = sdmmc_send_command(SD_CMD_SEND_IF_COND, cmd_arg,
				RESP_TYPE_R7, 0, hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Sending CMD_SD_SEND_IF_COND failed\n");
		goto fail;
	}

	do {
		err = sdmmc_send_command(SD_CMD_APPLICATION, cmd_arg,
					RESP_TYPE_R1, 0, hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Sending CMD_SD_APPLICATION failed\n");
		goto fail;
	}

	ocr_reg = SD_CARD_OCR_VALUE | CARD_CAPACITY_MASK;
	err = sdmmc_send_command(SD_ACMD_SEND_OP_COND, ocr_reg,
				 RESP_TYPE_R3, 0, hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Sending cmd SdAppCmd_SendOcr failed\n");
		goto fail;
	}
		ocr_reg = *sdmmc_response;
		/* Indicates no card is present in the slot */
		if (ocr_reg == 0) {
			goto fail;
		}

	} while (!(ocr_reg & (uint32_t)(SD_CARD_POWERUP_STATUS_MASK)));

	if (ocr_reg & SD_CARD_CAPACITY_MASK) {
		hsdmmc->is_high_capacity_card = true;
	}

	/* Request for all the available cids. */
	err = sdmmc_send_command(CMD_ALL_SEND_CID, 0, RESP_TYPE_R2, 0, hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Sending CID failed");
		goto fail;
	}
#if defined (CONFIG_ENABLE_BLOCKDEV_DEVICE_INFO)
	/* Store the hsdmmc by parsing cid. */
	err = sdmmc_parse_cid(hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Parse CID failed\n");
		goto fail;
	}
#endif

	/* Assign the relative card address. */
	pr_trace("Send command 3\n");
	err = sdmmc_send_command(CMD_SET_RELATIVE_ADDRESS, 9, RESP_TYPE_R6, 0,
				hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Assigning RCA failed\n");
		goto fail;
	}

	/* Hard code one rca. */
	pr_trace("Set RCA for the card\n");
	hsdmmc->card_rca = *sdmmc_response;

	/* Query the csd. */
	pr_trace("Query card specific data by command 9\n");
	err = sdmmc_send_command(CMD_SEND_CSD, hsdmmc->card_rca, RESP_TYPE_R2,
				 0, hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Query CSD failed\n");
		goto fail;
	}

	/* Store the hsdmmc by parsing csd. */
	pr_trace("Parse CSD data\n");
	err = sdmmc_parse_csd(hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Parse CSD failed\n");
		goto fail;
	}

	/* Select the card for data transfer. */
	pr_trace("Send command 7\n");
	err = sdmmc_send_command(CMD_SELECT_DESELECT_CARD, hsdmmc->card_rca,
			 RESP_TYPE_R1, 0, hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Sending cmd7 failed\n");
		goto fail;
	}

	/* Check if card is in data transfer mode or not. */
	err = sdmmc_send_command(CMD_SEND_STATUS, hsdmmc->card_rca, RESP_TYPE_R1,
				0, hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Card is not in transfer mode\n");
		goto fail;
	}

	err = sdmmc_card_transfer_mode(hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Setting card to transfer mode failed\n");
		goto fail;
	}

	/* Send ACMD6 to Set bus width to Four bit wide.*/
	err = sdmmc_send_command(SD_CMD_APPLICATION, hsdmmc->card_rca,
				RESP_TYPE_R1, 0, hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Command_ApplicationCommand transfer mode\n");
		goto fail;
	}

	err = sdmmc_send_command(SD_ACMD_SET_BUS_WIDTH, SD_BUS_WIDTH_4BIT,
				RESP_TYPE_R1, 0, hsdmmc);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("SdAppCmd_SetBusWidth transfer mode\n");
		goto fail;
	}

	/* Now change the Host bus width as well */
	hsdmmc->data_width = DATA_WIDTH_4BIT;
	sdmmc_set_data_width(DATA_WIDTH_4BIT, hsdmmc);

	/* Only data region on SD card */
	hsdmmc->current_access_region = 0;

fail:
	return err;
}

tegrabl_error_t sd_erase(tegrabl_bdev_t *dev, bnum_t block, bnum_t count,
	struct tegrabl_sdmmc *hsdmmc, sdmmc_device device)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	bnum_t blk_start;
	bnum_t blk_end;
	bnum_t erase_blk_end;
	bnum_t dev_blk_end;
	time_t start_time;
	time_t end_time;

	TEGRABL_UNUSED(device);

	if ((dev == NULL) || (hsdmmc == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 18);
		goto fail;
	}

	/* Make sure erase won't exceed the dev's block_count */
	erase_blk_end = block + count - 1U;
	dev_blk_end = dev->block_count - 1U;
	if (erase_blk_end > dev_blk_end) {
		pr_warn("Limit sd_erase() to 0x%x (from 0x%x)\n", dev_blk_end, erase_blk_end);
		erase_blk_end = dev_blk_end;
	}

	blk_start = block;
	blk_end = blk_start + MAX_ERASABLE_SECTORS - 1U;

	while (blk_start < erase_blk_end) {
		if (blk_end > erase_blk_end)
			blk_end = erase_blk_end;

		pr_trace("Send erase block start command (start=0x%x)\n", blk_start);
		error = sdmmc_send_command(SD_CMD_ERASE_BLK_START, blk_start, RESP_TYPE_R1, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto exit;
		}

		pr_trace("Send erase block end command (end=0x%x)\n", blk_end);
		error = sdmmc_send_command(SD_CMD_ERASE_BLK_END,
								   blk_end,
								   RESP_TYPE_R1, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto exit;
		}

		pr_trace("Send erase command with arg=0\n");
		error = sdmmc_send_command(SD_CMD_ERASE, 0, RESP_TYPE_R1B, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto exit;
		}

		/* Verify the response of the erase command. */
		pr_trace("Verify the response\n");
		error = sdmmc_verify_response(CMD_ERASE, 1, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto exit;
		}

		/* Check if the card is in programming state or not. */
		pr_trace("Send status command\n");
		start_time = tegrabl_get_timestamp_ms();
		do {
			error = sdmmc_send_command(CMD_SEND_STATUS,
						 hsdmmc->card_rca, RESP_TYPE_R1, 0, hsdmmc);
			if (error != TEGRABL_NO_ERROR) {
				goto exit;
			}
			if (sdmmc_verify_response(CMD_SEND_STATUS, 1, hsdmmc) != TEGRABL_NO_ERROR) {
				continue;
			} else {
				break;
			}
			end_time = tegrabl_get_timestamp_ms();
			if ((end_time - start_time) > SD_ERASE_TIMEOUT_IN_MS) {
				error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 22);
				goto exit;
			}
			tegrabl_mdelay(1);
		} while (true);

		blk_start = blk_end + 1U;
		blk_end += MAX_ERASABLE_SECTORS;
	}

exit:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Failed to erase blocks (0x%x : 0x%x)\n", blk_start, blk_end);
	}

fail:
	return error;
}
