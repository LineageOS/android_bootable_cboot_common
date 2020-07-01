/*
 * Copyright (c) 2015-2020, NVIDIA CORPORATION.  All rights reserved.
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
#define COMMAND_TIMEOUT_IN_US					100000U

/* OCR register polling timeout */
#define OCR_POLLING_TIMEOUT_IN_US				1000000U

/* Timeout from controller side for read to be completed */
#define READ_TIMEOUT_IN_US						200000U

/* Timeout in host side for misc operations */
#define TIME_OUT_IN_US							100000U

/* Timeout for reading data present on data lines */
#define DATA_TIMEOUT_IN_US						4000000U

/* Maximum number of sectors erasable in one shot */
#define MAX_ERASABLE_SECTORS					0xFA000U

/*  Define 102 Mhz Clock
 */
#define CLK_102_MHZ 102000U

/* Different modes for  card clock init */
	/* 400 KHz supplied to card. */
#define MODE_INIT 0U

	/* DIV64 clock along with oscillation enable supported. */
#define MODE_POWERON 1U

	/* Depends on the mode being supported for data transfer. */
#define MODE_DATA_TRANSFER 2U
typedef uint32_t sdmmc_mode_t;

/* Defines the VOLTAGE range supported. */
	/* Query the VOLTAGE supported. */
#define OCR_QUERY_VOLTAGE 0x00000000U

	/* High VOLTAGE only. */
#define OCR_HIGH_VOLTAGE 0x00ff8000UL

	/* Both VOLTAGEs. */
#define OCR_DUAL_VOLTAGE 0x00ff8080U

	/* Low VOLTAGE only. */
#define OCR_LOW_VOLTAGE 0x00000080U
typedef uint32_t sdmmc_ocr_volt_range;

/* Defines Emmc/Esd card states. */
	/* Card is in idle state. */
#define STATE_IDLE 0U

	/* Card is in ready state. */
#define STATE_READY 1U

	/* Not used. */
#define STATE_IDENT 2U

	/* Card is in standby state. */
#define STATE_STBY 3U

	/* Card is in transfer state. */
#define STATE_TRAN 4U

	/* Not used. */
#define STATE_DATA 5U

	/* Not used. */
#define STATE_RCV 6U

	/* Card is in programming mode. */
#define STATE_PRG 7U
typedef uint32_t sdmmc_state;

/* Defines various command being supported by EMMC. */
#define CMD_IDLE_STATE 0U
#define CMD_SEND_OCR 1U
#define CMD_ALL_SEND_CID 2U
#define CMD_SET_RELATIVE_ADDRESS 3U
#define CMD_SEND_RELATIVE_ADDRESS 3U
#define CMD_SWITCH 6U
#define CMD_SELECT_DESELECT_CARD 7U
#define CMD_SEND_EXT_CSD 8U
#define CMD_SEND_CSD 9U
#define CMD_SEND_CID 10U
#define CMD_STOP_TRANSMISSION 12U
#define CMD_SEND_STATUS 13U
#define CMD_SET_BLOCK_LENGTH 16U
#define CMD_READ_SINGLE 17U
#define CMD_READ_MULTIPLE 18U
#define CMD_SET_BLOCK_COUNT 23U
#define CMD_WRITE_SINGLE 24U
#define CMD_WRITE_MULTIPLE 25U
#define CMD_ERASE_GROUP_START 35U
#define CMD_ERASE_GROUP_END 36U
#define CMD_ERASE 38U
typedef uint32_t sdmmc_cmd;

/* Defines Command Responses of Emmc/Esd. */
#define RESP_TYPE_NO_RESP 0U
#define RESP_TYPE_R1 1U
#define RESP_TYPE_R2 2U
#define RESP_TYPE_R3 3U
#define RESP_TYPE_R4 4U
#define RESP_TYPE_R5 5U
#define RESP_TYPE_R6 6U
#define RESP_TYPE_R7 7U
#define RESP_TYPE_R1B 8U
#define RESP_TYPE_NUM 9U
typedef uint32_t sdmmc_resp_type;

/* Sd Specific Defines */
#define SD_SECTOR_SIZE			512U
#define SD_SECTOR_SZ_LOG2		9U
#define MAX_SECTORS_PER_BLOCK		512U
#define SD_HOST_VOLTAGE_RANGE		0x100U
#define SD_HOST_CHECK_PATTERN		0xAAU
#define SD_CARD_OCR_VALUE		0x00300000U
#define SD_CARD_POWERUP_STATUS_MASK	0x80000000UL
#define SD_CARD_CAPACITY_MASK		0x40000000U
#define SD_SDHC_SWITCH_BLOCK_SIZE	64U
#define SD_CSD_BLOCK_LEN_WORD		2U
#define SD_SDHC_CSIZE_MASK		0x3FFFFF00U
#define SD_SDHC_CSIZE_WORD		1U
#define SD_SDHC_CSIZE_SHIFT		8U
#define SD_SDHC_CSIZE_MULTIPLIER	1024U
#define SD_CSD_CSIZE_HIGH_WORD		2U
#define SD_CSD_CSIZE_HIGH_WORD_SHIFT	10U
#define SD_CSD_CSIZE_HIGH_WORD_MASK	0x3U
#define SD_CSD_CSIZE_LOW_WORD		1U
#define SD_CSD_CSIZE_LOW_WORD_SHIFT	22U
#define SD_CSD_CSIZE_MULT_WORD		1U
#define SD_CSD_CSIZE_MULT_SHIFT		7U
#define SD_CSD_CSIZE_MULT_MASK		0x7U
#define SD_BUS_WIDTH_1BIT		0U
#define SD_BUS_WIDTH_4BIT		2U

#define TIMING_INTERFACE_HIGH_SPEED	1U
#define TIMING_INTERFACE_HS200		2U
#define TIMING_INTERFACE_HS400		3U

/* Defines various Application specific Sd Commands as per spec */
#define SD_ACMD_SET_BUS_WIDTH		6U
#define SD_CMD_SEND_IF_COND		8U
#define SD_ACMD_SD_STATUS		13U
#define SD_ACMD_SEND_NUM_WR_BLOCKS	22U
#define SD_ACMD_SET_WR_BLK_ERASE_COUNT	23U
#define SD_CMD_ERASE_BLK_START		32U
#define SD_CMD_ERASE_BLK_END		33U
#define SD_CMD_ERASE			38U
#define SD_ACMD_SEND_OP_COND		41U
#define SD_ACMD_SET_CLR_CARD_DETECT	42U
#define SD_ACMD_SEND_SCR		51U
#define SD_CMD_APPLICATION		55U
#define SD_CMD_GENERAL			56U
#define SD_ACMD_FORCE32		0x7FFFFFFFU
#define SD_ERASE_TIMEOUT_IN_MS		100U
typedef uint32_t sd_cmd;

#define SKIP_NONE					0U
#define SKIP_INIT					1U
#define SKIP_INIT_UPDATE_CONFIG		2U

/**
* @brief  Prints the sdmmc register dump
*
* @param hsdmmc Context information to determine the base
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t sdmmc_print_regdump(struct tegrabl_sdmmc *hsdmmc);

tegrabl_error_t sdmmc_clock_init(uint32_t instance, uint32_t rate,
								 uint32_t source);

/** @brief  Initializes the card and the controller and select appropriate mode
 *          for card transfer like DDR or SDR.
 *
 *  @param instance Instance of the controller to be initialized.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param flag sdmmc init flag
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_init(uint32_t instance, struct tegrabl_sdmmc *hsdmmc, uint8_t flag);

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
	sdmmc_device device, bool is_non_blocking);

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
	 struct tegrabl_sdmmc *hsdmmc, sdmmc_device device);

/** @brief Reset the controller registers and enable internal clock at 400 KHz.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param instance Instance of the controller to be initialized.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_init_controller(struct tegrabl_sdmmc *hsdmmc,
	uint32_t instance);

/** @brief Sets the data bus width for DDR/SDR mode.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_set_bus_width(struct tegrabl_sdmmc *hsdmmc);

/** @brief Enables high speed mode for card version more than 4.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_enable_high_speed(struct tegrabl_sdmmc *hsdmmc);
/**
 *  @brief Sets the timing interface register in the ext csd register
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param mode bus speed mode to set
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t sdmmc_enable_timing_hs400(struct tegrabl_sdmmc *hsdmmc, uint8_t mode);


/** @brief Selects the region of access from user or boot partitions.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @param region  Select either user or boot region.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_select_access_region(struct tegrabl_sdmmc *hsdmmc,
									sdmmc_access_region region);

/** @brief Performs sanitize operation over unaddressed sectors
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_sanitize(struct tegrabl_sdmmc *hsdmmc);

/** @brief wait fot the transfer and initiates the next
 *
 *  @param xfer Address of the xfer info structure
 *  @param timeout Maxmimum timeout to wait
 *  @param status Address of the status flag to keep
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_xfer_wait(struct tegrabl_blockdev_xfer_info *xfer, time_t timeout, uint8_t *status);

/** @brief Read/write to a single sector within RPMB partition.
 *
 *  @param is_write Is the command is for write or not.
 *  @param hsdmmc Context information for RPMB access.
 *  @param hsdmmc Context information for controller.
 *  @param device Device to be accessed.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_rpmb_io(uint8_t is_write,
	sdmmc_rpmb_context_t *rpmb_context, struct tegrabl_sdmmc *hsdmmc);


/** @brief Query CID register from card and fills appropriate context.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return NO_ERROR if CID query is successful.
 */
tegrabl_error_t sdmmc_parse_cid(struct tegrabl_sdmmc *hsdmmc);

/** @brief Checks if the card is in transfer state or not.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_card_transfer_mode(struct tegrabl_sdmmc *hsdmmc);

/** @brief Sends the command with the given index.
 *
 *  @param index Command index to be send.
 *  @param arg Argument to be send.
 *  @param resp_type Response Type of the command.
 *  @param data_cmd If the command is data type or not.
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_send_command(sdmmc_cmd index, uint32_t arg,
	sdmmc_resp_type resp_type, uint8_t data_cmd, struct tegrabl_sdmmc *hsdmmc);

/** @brief Query CSD register from card and fills appropriate context.
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_parse_csd(struct tegrabl_sdmmc *hsdmmc);

/**
 *  @brief Reads the ext csd register contents
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_get_ext_csd(struct tegrabl_sdmmc *hsdmmc);

/** @brief Check if the last command was successful or not.
 *
 *  @param index Index of the last or next command.
 *  @param after_cmd_execution Tells if the next command or last command
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_verify_response(sdmmc_cmd index,
	uint8_t after_cmd_execution, struct tegrabl_sdmmc *hsdmmc);

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
	uint8_t is_write, struct tegrabl_sdmmc *hsdmmc, bool is_non_blocking);

/**
 *  @brief Sends the status command
 *
 *  @param hsdmmc Context information to determine the base
 *                 address of controller.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sdmmc_send_status(struct tegrabl_sdmmc *hsdmmc);

#endif /* TEGRABL_SDMMC_PROTOCOL_H */
