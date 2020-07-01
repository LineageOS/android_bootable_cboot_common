/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SATA_AHCI_H
#define TEGRABL_SATA_AHCI_H

#include <stdint.h>
#include <tegrabl_error.h>

#define TEGRABL_SATA_SECTOR_SIZE_LOG2 (9)

#define TEGRABL_SATA_AHCI_RFIS_SIZE (256)
#define TEGRABL_SATA_AHCI_DEVICE_IDENTITY_BUF_SIZE (512)
#define TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE (1024)
#define TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE (1024)
#define SATA_BUFFER_ALIGNEMENTS (4096)
#define SATA_MAX_READ_WRITE_SECTORS 0xFF

#define SATA_COMINIT_TIMEOUT 200000 /* us */
#define SATA_D2H_FIS_TIMEOUT 1000000 /* us */
#define TEGRABL_SATA_FLUSH_TIMEOUT 30000000 /* us */
#define TEGRABL_SATA_ERASE_TIMEOUT 10000000 /* us */
#define TEGRABL_SATA_WRITE_TIMEOUT 1000000 /* us */
#define TEGRABL_SATA_READ_TIMEOUT 1000000 /* us */
#define TEGRABL_SATA_IDENTIFY_TIMEOUT 1000000 /* us */

#define AHCI_CMD_HEADER_PRDTL (1 << 16)
#define AHCI_CMD_HEADER_CFL 0x5
#define AHCI_CMD_HEADER_WRITE (1 << 6)

#define SATA_SUPPORTS_FLUSH 4
#define SATA_SUPPORTS_FLUSH_EXT 5
#define SATA_SUPPORTS_48_BIT_ADDRESS 2

#define CMD_HEADER_WRITE (1 << 6)

#define SATA_COMMAND_DMA_WRITE_EXTENDED 0x35
#define SATA_COMMAND_DMA_WRITE 0xCA
#define SATA_COMMAND_DMA_READ 0xC8
#define SATA_COMMAND_DMA_READ_EXTENDED 0x25
#define SATA_COMMAND_IDENTIFY 0xec
#define SATA_COMMAND_FLUSH 0xE7
#define SATA_COMMAND_FLUSH_EXTENDED 0xEA

/**
 * @brief defines the mode supported by sata device driver
 */
enum tegrabl_sata_mode {
	/* Legacy mode without dma */
	TEGRABL_SATA_MODE_LEGACY = 1,

	/* With dma */
	TEGRABL_SATA_MODE_AHCI,

	TEGRABL_SATA_MODE_MAX
};

enum tegrabl_sata_interface_speed {
	TEGRABL_SATA_INTERFACE_GEN1,
	TEGRABL_SATA_INTERFACE_GEN2,
	TEGRABL_SATA_INTERFACE_GEN3
};


/**
 * @brief Defines the structure for book keeping
 */
struct tegrabl_sata_context {
	/* Sata instance id */
	uint32_t instance;
	/* Size of a block */
	size_t block_size_log2;
	/* Number of blocks in device */
	uint64_t block_count;
	/* Mode of operation */
	enum tegrabl_sata_mode mode;
	/* Interface speed */
	enum tegrabl_sata_interface_speed speed;

	/* Buffers required by controller */
	uint32_t *rfis;
	uint8_t *indentity_buf;
	uint32_t *command_list_buf;
	uint32_t *command_table;

	/* Is flush supported */
	bool supports_flush;
	/* Is extended flush supported */
	bool supports_flush_ext;
	/* Is sata controller initialized */
	bool initialized;
	/* Are extended commands supported */
	bool support_extended_cmd;
};

/**
 * @brief Defines AHCI FIS types.
 */
enum tegrabl_ahci_fis_type {
	/* Register FIS - host to device */
	TEGRABL_AHCI_FIS_TYPE_REG_H2D	= 0x27,
	/* Register FIS - device to host */
	TEGRABL_AHCI_FIS_TYPE_REG_D2H	= 0x34,
	/* DMA activate FIS - device to host */
	TEGRABL_AHCI_FIS_TYPE_DMA_ACT	= 0x39,
	/* DMA setup FIS - bidirectional */
	TEGRABL_AHCI_FIS_TYPE_DMA_SETUP	= 0x41,
	/* Data FIS - bidirectional */
	TEGRABL_AHCI_FIS_TYPE_DATA		= 0x46,
	/* BIST activate FIS - bidirectional */
	TEGRABL_AHCI_FIS_TYPE_BIST		= 0x58,
	/* PIO setup FIS - device to host */
	TEGRABL_AHCI_FIS_TYPE_PIO_SETUP	= 0x5F,
	/* Set device bits FIS - device to host */
	TEGRABL_AHCI_FIS_TYPE_DEV_BITS	= 0xA1,
};

/**
 * @brief Defines FIS transferred between host and device
 */
struct tegrabl_ahci_fis_h2d {
	/* TEGRABL_AHCI_FIS_TYPE_REG_H2D */
	uint8_t fis_type;
	/* port_multiplier=0:0; reserved=1:2; command= 3:3 */
	uint8_t prc;
	uint8_t command;
	uint8_t featurel;
	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t device;
	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint8_t featureh;
	uint8_t countl;
	uint8_t counth;
	uint8_t iccc;
	uint8_t control;
	uint8_t reseved[4];
};

/**
 * @brief Defines the prdt entry to be filled for ATA commands.
 */
struct tegrabl_ahci_prdt_entry {
	/* Buffer address low */
	uint32_t address_low;
	/* Buffer address high */
	uint32_t address_high;
	uint32_t reserved;
	/* interrupt on completion= 31:31; reserved= 22:30; byte count= 0:21 */
	uint32_t irc;
};

/**
 * @brief Defines the command table for ATA commands.
 */
struct tegrabl_ahci_cmd_table {
	uint8_t command_fis[64];
	uint8_t atpi_command[16];
	uint8_t reserved[48];
	struct tegrabl_ahci_prdt_entry prdt_entry[1];
};

/**
 * @brief Defines the structure recieved as part of INDENTIFY
 * command.
 */
struct tegrabl_ata_dev_id {
	uint8_t not_used1[20];
	uint8_t serial_number[20];
	uint8_t not_used2[6];
	uint8_t fw_version[8];
	uint8_t model_number[40];
	uint8_t not_used3[26];
	uint8_t sectors[4];
	uint8_t not_used4[48];
	uint8_t command_supported[2];
	uint8_t not_used5[26];
	uint8_t sectors_48bit[6];
	uint8_t not_used6[302];
};

/**
 * @brief initializes the sata controller and context
 *
 * @param context sata context
 * @param Handle for uphy driver
 * @return TEGRABL_NO_ERROR if successfule else appropriate error..
 */
tegrabl_error_t tegrabl_sata_ahci_init(struct tegrabl_sata_context *context,
		struct tegrabl_uphy_handle *uphy);

/**
 * @brief Read or write number block starting from specified
 * block
 *
 * @param context Context information
 * @param buf Buffer to save read content or to write to device
 * @param block Start sector for read/write
 * @param count Number of sectors to read/write
 * @param is_write True if write operation
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_sata_ahci_io(struct tegrabl_sata_context *context,
		void *buf, bnum_t block, bnum_t count, bool is_write, time_t timeout);

/**
 * @brief Erases storage device connected to sata controller
 *
 * @param context Context information
 * @param block start sector from which erasing should start
 * @param count number of sectors to erase
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_sata_ahci_erase(struct tegrabl_sata_context *context,
		bnum_t block, bnum_t count);

/**
 * @brief Sends flush command to SATA device.
 *
 * @param context SATA context
 *
 * @return TEGRABL_NO_ERROR if successful.
 */
tegrabl_error_t tegrabl_sata_ahci_flush_device(
		struct tegrabl_sata_context *context);

/**
 * @brief Frees all buffers allocated durnig init.
 *
 * @param context SATA context.
 */
void tegrabl_sata_ahci_free_buffers(struct tegrabl_sata_context *context);

/**
 * @brief Issue flush command to flush the data from
 * device's cache.
 *
 * @param context SATA context
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_sata_ahci_flush_device(
		struct tegrabl_sata_context *context);

#endif
