/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SDMMC_DEFS_H
#define TEGRABL_SDMMC_DEFS_H

#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_sdmmc_card_reg.h>
#include <tegrabl_debug.h>
#include <tegrabl_timer.h>

enum device_type {
	DEVICE_TYPE_EMMC,
	DEVICE_TYPE_SD,
};

/* Defines various widths for data line supported. */
typedef enum {
	/* Specifies a 1 bit interface to sdmmc */
	DATA_WIDTH_1BIT = 0,

	/* Specifies a 4 bit interface to sdmmc */
	DATA_WIDTH_4BIT = 1,

	/* Specifies a 8 bit interface to sdmmc */
	DATA_WIDTH_8BIT = 2,

	/* Specifies a 4 bit Ddr interface to sdmmc */
	DATA_WIDTH_DDR_4BIT = 5,

	/* Specifies a 8 bit Ddr interface to sdmmc */
	DATA_WIDTH_DDR_8BIT = 6,

} sdmmc_data_width;

typedef enum {
	/* Card is in idle mode */
	DEVICE_STATUS_IDLE = 0,

	/* Card is under I/O operations */
	DEVICE_STATUS_IO_PROGRESS,

	/* I/O operations have failed */
	DEVICE_STATUS_IO_FAILURE,

	/* CRC error observed */
	DEVICE_STATUS_CRC_FAILURE,

	/* Data timeout happened */
	DEVICE_STATUS_DATA_TIMEOUT,
} sdmmc_device_status;

/* Defines Emmc card partitions. */
typedef enum {
	/* Access region is user partition */
	USER_PARTITION = 0,

	/* Access region is boot partition1 */
	BOOT_PARTITION_1,

	/* Access region is boot partition2 */
	BOOT_PARTITION_2,

	/* Access region is RPMB partition */
	RPMB_PARTITION,

	/* Access region reserved */
	NUM_PARTITION,

	/* Access region is unknown */
	UNKNOWN_PARTITION,
} sdmmc_access_region;

typedef struct sdmmc_context {
	/* Is Sdmmc controller initialized */
	bool initialized;

	/* Count the number of devices */
	uint8_t count_devices;

	/* Card's Relative Card Address. */
	uint32_t card_rca;

	/* Max transfer speed in init mode */
	uint32_t tran_speed;

	/* Data bus width. */
	sdmmc_data_width data_width;

	/* Response buffer. Use int for word align. */
	uint32_t response[RESPONSE_BUFFER_SIZE / sizeof(uint32_t)];

	/* Device status. */
	sdmmc_device_status device_status;

	/* Indicates whether to access the card in High speed mode. */
	uint8_t high_speed_mode;

	/* Indicates whether the card is high capacity card or not. */
	uint8_t is_high_capacity_card;

	/* Spec version. */
	uint8_t spec_version;

	/* Holds sdmmc Boot Partition size. */
	uint8_t sdmmc_boot_partition_size;

	/* Holds the current access region. */
	sdmmc_access_region current_access_region;

	/* Indicates whether host supports high speed mode. */
	uint8_t host_supports_high_speed_mode;

	/* Indicates whether card supports high speed mode. */
	uint8_t card_supports_high_speed_mode;

	/* Boot partition number of blocks. */
	uint32_t boot_blocks;

	/* Indicates whether high voltage range is used for Card identification. */
	uint8_t is_high_voltage_range;

	/* Max Power class supported by target board. */
	uint8_t max_power_class_supported;

	/* Number of blocks present in card in user partition. */
	uint32_t user_blocks;

	/* Number of blocks present in card in RPMB partition. */
	uint32_t rpmb_blocks;

	/* Log_2 of number of blocks present in card. */
	uint32_t block_size_log2;

	/* Holds read time out at current card clock frequency. */
	uint32_t read_timeout_in_us;

	/* holds the read start time */
	time_t read_start_time;

	/* holds boot config register */
	uint32_t boot_config;

	/* Flag to indicate the card speed and operating voltage level */
	uint8_t card_support_speed;

	/* store card's bus width. */
	uint8_t card_bus_width;

	/* Indicates whether Ddr mode is used for data transfer */
	uint8_t is_ddr_mode;

	/* Indicates controller in use */
	uint32_t controller_id;

	/* Indicates the current Controller Base Address */
	uint32_t base_addr;

	/* Indicate power class for 26 MHz at 3.6V */
	uint32_t power_class_26MHz_360V;

	/* Indicate power class for 52 MHz at 3.6V */
	uint32_t power_class_52MHz_360V;

	/* Indicate power class for 26 MHz at 1.95V */
	uint32_t power_class_26MHz_195V;

	/* Indicate power class for 52 MHz at 1.95V */
	uint32_t power_class_52MHz_195V;

	/* Indicate power class for 52 MHz at 3.6V for ddr mode */
	uint32_t power_class_52MHz_ddr360V;

	/* Indicate power class for 52 MHz at 1.95V for ddr mode */
	uint32_t power_class_52MHz_ddr195V;

	/* erase group size in blocks */
	uint32_t erase_group_size;

	/* erase timeout in us */
	uint32_t erase_timeout_us;

	/* is sanitize supported or not */
	uint8_t sanitize_support;

	/* device type */
	uint8_t device_type;

	uint8_t manufacture_id;

	/* buffer for extended csd register */
	uint8_t TEGRABL_ALIGN(4) ext_csd_buffer_address[ECSD_BUFFER_SIZE];

	/* clock source to use while init */
	uint32_t clk_src;

	/* Best mode of operation */
	uint32_t best_mode;

	uint32_t tap_value;

	uint32_t trim_value;

	bool is_hostv4_enabled;

} sdmmc_context_t;

#define SDMMC_BLOCK_SIZE_LOG2			9	/* 512 bytes */

#define SDMMC_CONTEXT_BLOCK_SIZE(context)	(1U << (context)->block_size_log2)

/* Defines the regions under sdmmc device to be registered in bio layer */
typedef enum {
	/* defines invalid device type */
	DEVICE_INVALID = 0x0,

	/* defines boot device for bio layer */
	DEVICE_BOOT,

	/* defines user device for bio layer */
	DEVICE_USER,

	/* defines rpmb device for bio layer */
	DEVICE_RPMB,
} sdmmc_device;

/* Defines the private data being used by sdmmc */
typedef struct sdmmc_priv_data {
	/* defines the device type */
	sdmmc_device device;

	/* store context pointer */
	void *context;

} sdmmc_priv_data_t;


#endif /* TEGRABL_SDMMC_DEFS_H */
