/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SDMMC_PROTOCOL_H
#define TEGRABL_SDMMC_PROTOCOL_H

#include <tegrabl_blockdev.h>
#include <tegrabl_sdmmc_bdev.h>
#include <tegrabl_sdmmc_rpmb.h>

/* Timeout from controller side for command complete */
#define COMMAND_TIMEOUT_IN_US					100000

/* OCR register polling timeout */
#define OCR_POLLING_TIMEOUT_IN_US				500000

/* Timeout from controller side for read to be completed */
#define READ_TIMEOUT_IN_US						200000

/* Timeout in host side for misc operations */
#define TIME_OUT_IN_US							100000

/* Timeout for reading data present on data lines */
#define DATA_TIMEOUT_IN_US						4000000

/* Maximum number of sectors erasable in one shot */
#define MAX_ERASABLE_SECTORS					0xFA000

/*  Define 102 Mhz Clock
 */
#define CLK_102_MHZ 102000000

/* Different modes for  card clock init */
typedef enum sdmmc_mode {
	/* 400 KHz supplied to card. */
	MODE_INIT = 0,

	/* DIV64 clock along with oscillation enable supported. */
	MODE_POWERON,

	/* Depends on the mode being supported for data transfer. */
	MODE_DATA_TRANSFER,
} sdmmc_mode_t;

/* Defines the VOLTAGE range supported. */
typedef enum {
	/* Query the VOLTAGE supported. */
	OCR_QUERY_VOLTAGE = 0x00000000,

	/* High VOLTAGE only. */
	OCR_HIGH_VOLTAGE = 0x00ff8000,

	/* Both VOLTAGEs. */
	OCR_DUAL_VOLTAGE = 0x00ff8080,

	/* Low VOLTAGE only. */
	OCR_LOW_VOLTAGE  = 0x00000080,
} sdmmc_ocr_volt_range;

/* Defines Emmc/Esd card states. */
typedef enum {
	/* Card is in idle state. */
	STATE_IDLE = 0,

	/* Card is in ready state. */
	STATE_READY,

	/* Not used. */
	STATE_IDENT,

	/* Card is in standby state. */
	STATE_STBY,

	/* Card is in transfer state. */
	STATE_TRAN,

	/* Not used. */
	STATE_DATA,

	/* Not used. */
	STATE_RCV,

	/* Card is in programming mode. */
	STATE_PRG,
} sdmmc_state;

/* Defines various command being supported by EMMC. */
typedef enum {
	CMD_IDLE_STATE = 0,
	CMD_SEND_OCR = 1,
	CMD_ALL_SEND_CID = 2,
	CMD_SET_RELATIVE_ADDRESS = 3,
	CMD_SEND_RELATIVE_ADDRESS = 3,
	CMD_SWITCH = 6,
	CMD_SELECT_DESELECT_CARD = 7,
	CMD_SEND_EXT_CSD = 8,
	CMD_SEND_CSD = 9,
	CMD_STOP_TRANSMISSION = 12,
	CMD_SEND_STATUS = 13,
	CMD_SET_BLOCK_LENGTH = 16,
	CMD_READ_SINGLE = 17,
	CMD_READ_MULTIPLE = 18,
	CMD_SET_BLOCK_COUNT = 23,
	CMD_WRITE_SINGLE = 24,
	CMD_WRITE_MULTIPLE = 25,
	CMD_ERASE_GROUP_START = 35,
	CMD_ERASE_GROUP_END = 36,
	CMD_ERASE = 38,
} sdmmc_cmd;

/* Defines Command Responses of Emmc/Esd. */
typedef enum {
	RESP_TYPE_NO_RESP = 0,
	RESP_TYPE_R1 = 1,
	RESP_TYPE_R2 = 2,
	RESP_TYPE_R3 = 3,
	RESP_TYPE_R4 = 4,
	RESP_TYPE_R5 = 5,
	RESP_TYPE_R6 = 6,
	RESP_TYPE_R7 = 7,
	RESP_TYPE_R1B = 8,
	RESP_TYPE_NUM,
} sdmmc_resp_type;

/* Sd Specific Defines */
#define SD_SECTOR_SIZE   512
#define SD_SECTOR_SZ_LOG2 9
#define MAX_SECTORS_PER_BLOCK 512
#define SD_HOST_VOLTAGE_RANGE   0x100
#define SD_HOST_CHECK_PATTERN   0xAA
#define SD_CARD_OCR_VALUE   0x00300000
#define SD_CARD_POWERUP_STATUS_MASK 0x80000000
#define SD_CARD_CAPACITY_MASK   0x40000000
#define SD_SDHC_SWITCH_BLOCK_SIZE  64
#define SD_CSD_BLOCK_LEN_WORD   2
#define SD_SDHC_CSIZE_MASK  0x3FFFFF00
#define SD_SDHC_CSIZE_WORD  1
#define SD_SDHC_CSIZE_SHIFT  8
#define SD_SDHC_CSIZE_MULTIPLIER  1024
#define SD_CSD_CSIZE_HIGH_WORD   2
#define SD_CSD_CSIZE_HIGH_WORD_SHIFT 10
#define SD_CSD_CSIZE_HIGH_WORD_MASK 0x3
#define SD_CSD_CSIZE_LOW_WORD   1
#define SD_CSD_CSIZE_LOW_WORD_SHIFT   22
#define SD_CSD_CSIZE_MULT_WORD   1
#define SD_CSD_CSIZE_MULT_SHIFT  7
#define SD_CSD_CSIZE_MULT_MASK  0x7
#define SD_BUS_WIDTH_1BIT   0
#define SD_BUS_WIDTH_4BIT   2

/* Defines various Application specific Sd Commands as per spec */
enum {
	SD_ACMD_SET_BUS_WIDTH = 6,
	SD_CMD_SEND_IF_COND = 8,
	SD_ACMD_SD_STATUS = 13,
	SD_ACMD_SEND_NUM_WR_BLOCKS = 22,
	SD_ACMD_SET_WR_BLK_ERASE_COUNT = 23,
	SD_ACMD_SEND_OP_COND = 41,
	SD_ACMD_SET_CLR_CARD_DETECT = 42,
	SD_ACMD_SEND_SCR = 51,
	SD_CMD_APPLICATION = 55,
	SD_CMD_GENERAL = 56,
	SD_ACMD_FORCE32 = 0x7FFFFFFF,
} sd_cmd;

/**
* @brief  Prints the sdmmc register dump
*
* @param context Context information to determine the base
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t sdmmc_print_regdump(sdmmc_context_t *context);

tegrabl_error_t sdmmc_clock_init(uint32_t instance, uint32_t rate,
								 uint32_t source);

/** @brief  Initializes the card and the controller and select appropriate mode
 *          for card transfer like DDR or SDR.
 *
 *  @param instance Instance of the controller to be initialized.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param flag sdmmc init flag
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_init(uint32_t instance, sdmmc_context_t *context,
	uint32_t flag);

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
	sdmmc_device device);

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
	 sdmmc_context_t *context, sdmmc_device device);

/** @brief Reset the controller registers and enable internal clock at 400 KHz.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param instance Instance of the controller to be initialized.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_init_controller(sdmmc_context_t *context,
	uint32_t instance);

/** @brief Sets the data bus width for DDR/SDR mode.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_set_bus_width(sdmmc_context_t *context);

/** @brief Enables high speed mode for card version more than 4.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_enable_high_speed(sdmmc_context_t *context);

/** @brief Selects the region of access from user or boot partitions.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @param region  Select either user or boot region.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_select_access_region(sdmmc_context_t *context,
									sdmmc_access_region region);

/** @brief Performs sanitize operation over unaddressed sectors
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_sanitize(sdmmc_context_t *context);

/** @brief Read/write to a single sector within RPMB partition.
 *
 *  @param is_write Is the command is for write or not.
 *  @param context Context information for RPMB access.
 *  @param context Context information for controller.
 *  @param device Device to be accessed.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_rpmb_io(uint8_t is_write,
	sdmmc_rpmb_context_t *rpmb_context, sdmmc_context_t *context);


/** @brief Query CID register from card and fills appropriate context.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if CID query is successful.
 */
tegrabl_error_t sdmmc_parse_cid(sdmmc_context_t *context);

/** @brief Checks if the card is in transfer state or not.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_card_transfer_mode(sdmmc_context_t *context);

/** @brief Sends the command with the given index.
 *
 *  @param index Command index to be send.
 *  @param arg Argument to be send.
 *  @param resp_type Response Type of the command.
 *  @param data_cmd If the command is data type or not.
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_send_command(sdmmc_cmd index, uint32_t arg,
	sdmmc_resp_type resp_type, uint8_t data_cmd, sdmmc_context_t *context);

/** @brief Query CSD register from card and fills appropriate context.
 *
 *  @param context Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_parse_csd(sdmmc_context_t *context);

#endif /* TEGRABL_SDMMC_PROTOCOL_H */
