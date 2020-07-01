/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_SDMMC

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_sdmmc_protocol.h>
#include <tegrabl_sdmmc_protocol_rpmb.h>

/** @brief Issue a single read to the RPMB partition.
 *
 *  @param rpmb_buf Pointer to read buffer containing single frame.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_rpmb_block_read(sdmmc_rpmb_frame_t *frame,
											 struct tegrabl_sdmmc *hsdmmc)
{
	uint32_t arg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((hsdmmc == NULL) || (frame == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 22);
		goto fail;
	}

	/* Set block count to 1. */
	arg = 1;

	error = sdmmc_send_command(CMD_SET_BLOCK_COUNT, arg,
							   RESP_TYPE_R1, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = sdmmc_verify_response(CMD_SET_BLOCK_COUNT, 0, hsdmmc);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
	}
	/* Send read request */
	error = sdmmc_block_io(0, 1, (uint8_t *)frame, 0, hsdmmc, false);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
	}

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

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("sdmmc RPMB block read: exit, error = %x\n", error);
	}
	return error;
}

/** @brief Issue a single write to the RPMB partition.
 *
 *  @param rpmb_buf Pointer to write buffer containing single frame.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param is_reliable_write enabling the reliable write.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
static tegrabl_error_t sdmmc_rpmb_block_write(sdmmc_rpmb_frame_t *frame, struct tegrabl_sdmmc *hsdmmc,
											  uint8_t is_reliable_write)
{
	uint32_t arg;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((hsdmmc == NULL) || (frame == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 23);
		goto fail;
	}

	/* Set block count to 1 and enable reliable write */
	if (is_reliable_write != 0U) {
		arg = (1UL | (1UL << 31));
	} else {
		arg = 1UL;
	}

	error = sdmmc_send_command(CMD_SET_BLOCK_COUNT, arg,
							   RESP_TYPE_R1, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = sdmmc_verify_response(CMD_SET_BLOCK_COUNT, 0, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

    /* Send write request */
	error = sdmmc_block_io(0, 1, (uint8_t *)frame, 1, hsdmmc, false);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

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
fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("sdmmc RPMB block write: exit, error = %x\n", error);
	}
	return error;
}

/** @brief Read/write to a single sector within RPMB partition.
*
*  @param is_write Is the command is for write or not.
*  @param hsdmmc Context information to determine the base
*                 address of controller.
*  @param device Device to be accessed.
*
*  @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t sdmmc_rpmb_io(uint8_t is_write, sdmmc_rpmb_context_t *rpmb_context,
							  struct tegrabl_sdmmc *hsdmmc)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((hsdmmc == NULL) || (rpmb_context == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 24);
		goto fail;
	}

	/* Mark the device is idle. */
	hsdmmc->device_status = DEVICE_STATUS_IDLE;

	/* Switch to RPMB partition. */
	error = sdmmc_select_access_region(hsdmmc, RPMB_PARTITION);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	if (is_write != 0U) {
		/* Send request frame. */
		error = sdmmc_rpmb_block_write(&rpmb_context->req_frame, hsdmmc, 1);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}

		/* Send request-response frame. */
		error = sdmmc_rpmb_block_write(&rpmb_context->req_resp_frame, hsdmmc, 0);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	} else {
		/* Send request frame. */
		error = sdmmc_rpmb_block_write(&rpmb_context->req_frame, hsdmmc, 0);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	/* Get response frame. */
	error = sdmmc_rpmb_block_read(&rpmb_context->resp_frame, hsdmmc);

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("sdmmc RPMB IO: exit, error = %x\n", error);
	}
	return error;
}
