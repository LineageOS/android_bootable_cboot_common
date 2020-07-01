/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SDMMC_HOST_H
#define TEGRABL_SDMMC_HOST_H

#include <tegrabl_sdmmc_bdev.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_dmamap.h>

/* defines the maximum transfer allowed by sdma */
#define MAX_SDMA_TRANSFER			65535

/** @brief Resets all the registers of the controller.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if reset is successful.
 */
tegrabl_error_t sdmmc_reset_controller(sdmmc_context_t *context);

/**
* @brief Enables Host Controller V4 to use 64 bit addr dma
*
* @param context Context information to determine the base
*                 address of controller.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t sdmmc_enable_hostv4(sdmmc_context_t *context);

/**
* @brief Get the status of hostv4 whether it is enabled or not
*   and store in sdmmc_context
*
* @param context Context information to determine the base
*                 address of controller.
*
*/
void sdmmc_get_hostv4_status(sdmmc_context_t *context);

/** @brief sets the card clock for the card from controller.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param mode Different mode like init, power on & data transfer.
 *  @param clk_divider Appropriate divider for generating card clock.
 *
 *  @return NO_ERROR if card clock is enabled successfully.
 */
tegrabl_error_t sdmmc_set_card_clock(sdmmc_context_t *context,
	sdmmc_mode_t mode, uint32_t clk_divider);

/** @brief Sets the voltage in power_control_host according to capabilities.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return {tegrabl_error_t} NO_ERROR if bus power is enabled successfully.
 */
tegrabl_error_t sdmmc_enable_bus_power(sdmmc_context_t *context);

/** @brief Sets the interrupt error mask.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return Void.
 */
tegrabl_error_t sdmmc_set_interrupt_status_reg(sdmmc_context_t *context);

/** @brief Checks if card is stable and present or not.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if card is present and it is stable.
 */
tegrabl_error_t sdmmc_is_card_present(sdmmc_context_t *context);

/** @brief Prepare cmd register to be send in command send.
 *
 *  @param cmd_reg Command register send by command send.
 *  @param data_cmd Configure cmd_reg for data transfer
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param index Index of the command being send.
 *  @param resp_type Response type of the command.
 *
 *  @return Void.
 */
tegrabl_error_t sdmmc_prepare_cmd_reg(uint32_t *cmd_reg, uint8_t data_cmd,
	sdmmc_context_t *context, sdmmc_cmd index, sdmmc_resp_type resp_type);

/** @brief Sends command by writing cmd_reg, arg & int_status.
 *
 *  @param cmd_reg Command register send by command send.
 *  @param arg Argument for the command to be send.
 *  @param data_cmd Configure cmd_reg for data transfer
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if command send is successful.
 */
tegrabl_error_t sdmmc_try_send_command(uint32_t cmd_reg, uint32_t arg,
	uint8_t data_cmd, sdmmc_context_t *context);

/** @brief Check if next command can be send or not.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if command transfer can be done.
 */
tegrabl_error_t sdmmc_cmd_txr_ready(sdmmc_context_t *context);

/** @brief Check if data can be send or not over data lines.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if data lines is free.
 */
tegrabl_error_t sdmmc_data_txr_ready(sdmmc_context_t *context);

/** @brief Read response of last command in local buffer.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param resp_type Response type of the command.
 *  @param buf Buffer in which response will be read.
 *
 *  @return Void.
 */
tegrabl_error_t sdmmc_read_response(sdmmc_context_t *context,
						 sdmmc_resp_type resp_type, uint32_t *buf);

/** @brief Try to recover the controller from the error occured.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param data_cmd if data_cmd then send abort command.
 *
 *  @return Void.
 */
tegrabl_error_t sdmmc_recover_controller_error(sdmmc_context_t *context,
	uint8_t data_cmd);

/** @brief Select the mode of operation (SDR/DDR/HS200). Currently only
 *         DDR & SDR supported.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if mode is successfully switched.
 */
tegrabl_error_t sdmmc_select_mode_transfer(sdmmc_context_t *context);

/** @brief Map the logical blocks to physical blocks in boot partitions.
 *
 *  @param start_sector Starting logical sector in boot partitions.
 *  @param num_sectors Number of logical sector in boot partitions.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if logical to physical sectors mapping is
 *          successful.
 */
tegrabl_error_t sdmmc_get_correct_boot_block(bnum_t *start_sector,
	bnum_t *num_sectors, sdmmc_context_t *context);

/** @brief Select the default region of operation as user partition.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if user partition is made default access region.
 */
tegrabl_error_t sdmmc_set_default_region(sdmmc_context_t *context);

/** @brief Wait till data line is ready for transfer.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if data line is ready for transfer.
 */
tegrabl_error_t sdmmc_wait_for_data_line_ready(sdmmc_context_t *context);

/** @brief Set the number of blocks to read or write.
 *
 *  @param block_size The block size being used for transfer.
 *  @param num_blocks Numbers of block to read/write.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return Void.
 */
void sdmmc_set_num_blocks(uint32_t block_size, uint32_t num_blocks,
	sdmmc_context_t *context);

/** @brief Writes the buffer start for read/write.
 *
 *  @param buf Input buffer whose address is registered.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return Void.
 */
void sdmmc_setup_dma(dma_addr_t buf, sdmmc_context_t *context);

/** @brief checks if card is in transfer state or not and perform various
 *         operations according to the mode of operation.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return Status of the device
 */
sdmmc_device_status sdmmc_query_status(sdmmc_context_t *context);

/** @brief Set the data width for data lines
 *
 *  @param width Data width to be configured.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return Void.
 */
tegrabl_error_t sdmmc_set_data_width(sdmmc_data_width width,
	sdmmc_context_t *context);

/** @brief Use to enable/disable high speed mode according to the mode of
 *         Operation.
 *
 *  @param enable Enable High Speed mode.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return Void.
 */
tegrabl_error_t sdmmc_toggle_high_speed(uint8_t enable,
	sdmmc_context_t *context);

/**
 * @brief Update I/O Spare & trim control.
 *
 * @param context Context information to determine the base
 *                 address of controller.
 */
tegrabl_error_t sdmmc_io_spare_update(sdmmc_context_t *context);

/**
 * @brief Performs auto-calibration before accessing controller.
 *
 * @param context Context information to determine the base
 *                 address of controller.
 *
 * @return NO_ERROR if successful.
 */
tegrabl_error_t sdmmc_auto_calibrate(sdmmc_context_t *context);

/**
 * @brief Update drv settings if auto-calibration failed
 *
 * @param context Context information to determine the base
 *                 address of controller.
 *
 * @param instance Instance of the sdmmc controller
 */
void sdmmc_update_drv_settings(sdmmc_context_t *context, uint32_t instance);


#endif /* TEGRABL_SDMMC_HOST_H  */
