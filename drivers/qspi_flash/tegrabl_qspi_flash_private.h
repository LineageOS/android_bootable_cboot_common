/*
 * Copyright (c) 2015-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_QSPI_FLASH_PRIVATE_H
#define INCLUDED_TEGRABL_QSPI_FLASH_PRIVATE_H

#include "build_config.h"
#include <stdint.h>
#include <tegrabl_qspi.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define TEGRABL_QSPI_BUF_ALIGN_SIZE 4U

/* QSPI Flash Commands */

/* Read Device Identification Commands */
#define QSPI_FLASH_CMD_REMS					0x90U
#define QSPI_FLASH_CMD_RDID					0x9FU
#define QSPI_FLASH_CMD_RES					0xABU
#define QSPI_FLASH_CMD_RSFDP				0x5AU


/* Register Access Commands */
#define QSPI_FLASH_CMD_RDSR1				0x05U
#define QSPI_FLASH_CMD_RDSR2				0x07U
#define QSPI_FLASH_CMD_RDCR					0x35U
#define QSPI_FLASH_CMD_RDAR					0x65U
#define QSPI_FLASH_CMD_WRR					0x01U
#define QSPI_FLASH_CMD_WRDI					0x04U
#define QSPI_FLASH_CMD_WREN					0x06U
#define QSPI_FLASH_CMD_WRAR					0x71U


/* 3-byte addressing */
/* Read Flash Array Commands */
#define QSPI_FLASH_CMD_READ					0x03U
#define QSPI_FLASH_CMD_FAST_READ			0x0BU
#define QSPI_FLASH_CMD_DUAL_IO_READ			0xBBU
#define QSPI_FLASH_CMD_QUAD_IO_READ			0xEBU
#define QSPI_FLASH_CMD_DDR_DUAL_IO_READ		0xBDU
#define QSPI_FLASH_CMD_DDR_QUAD_IO_READ		0xEDU

/* Program Flash Array Commands */
#define QSPI_FLASH_CMD_PAGE_PROGRAM			0x02U
#define QSPI_FLASH_CMD_QUAD_PAGE_PROGRAM	0x32U

/* Erase Flash Array Commands */
#define QSPI_FLASH_CMD_PARA_SECTOR_ERASE	0x20U
#define QSPI_FLASH_CMD_SECTOR_ERASE			0xD8U


/* 4-byte addressing */
/* Read Flash Array Commands */
#define QSPI_FLASH_CMD_4READ				0x13U
#define QSPI_FLASH_CMD_4FAST_READ			0x0CU
#define QSPI_FLASH_CMD_4DUAL_IO_READ		0xBCU
#define QSPI_FLASH_CMD_4QUAD_IO_READ		0xECU
#define QSPI_FLASH_CMD_4DDR_DUAL_IO_READ	0xEEU
#define QSPI_FLASH_CMD_4DDR_QUAD_IO_READ	0xEEU

/* Program Flash Array Commands */
#define QSPI_FLASH_CMD_4PAGE_PROGRAM		0x12U
#define QSPI_FLASH_CMD_4QUAD_PAGE_PROGRAM	0x34U

/* Erase Flash Array Commands */
#define QSPI_FLASH_CMD_4PARA_SECTOR_ERASE	0x21U
#define QSPI_FLASH_CMD_4SECTOR_ERASE		0xDCU
#define QSPI_FLASH_CMD_BULK_ERASE			0xC7U


/* Reset Commands */
#define QSPI_FLASH_CMD_SW_RESET_ENABLE		0x66U
#define QSPI_FLASH_CMD_SW_RESET				0x99U
#define QSPI_FLASH_CMD_MODE_BIT_RESET		0xFFU


#define QSPI_FLASH_NUM_OF_TRANSFERS				3U
#define QSPI_FLASH_COMMAND_WIDTH				1U
#define QSPI_FLASH_ADDRESS_WIDTH				4U
#define QSPI_FLASH_QSPI_FLASH_DATA_TRANSFER		2U
#define QSPI_FLASH_MAX_TRANSFER_SIZE			(65536U * 4U)
#define QSPI_FLASH_CMD_MODE_VAL					0x0U
#define QSPI_FLASH_ADDR_DATA_MODE_VAL			0x0U
#define QSPI_FLASH_WRITE_ENABLE_WAIT_TIME		1000U
#define QSPI_FLASH_WIP_DISABLE_WAIT_TIME		1000U
#define QSPI_FLASH_WIP_WAIT_FOR_READY			false
#define QSPI_FLASH_WIP_WAIT_FOR_BUSY			true
#define QSPI_FLASH_WIP_WAIT_IN_US				false
#define QSPI_FLASH_WIP_WAIT_IN_MS				true
#define QSPI_FLASH_WE_RETRY_COUNT				2000U
#define QSPI_FLASH_WIP_RETRY_COUNT				2000U
#define QSPI_FLASH_SINGLE_WRITE_SIZE			256U
#define QSPI_FLASH_QUAD_ENABLE					0x02U
#define QSPI_FLASH_QUAD_DISABLE					0x0U
#define QSPI_FLASH_WEL_ENABLE					0x02UL
#define QSPI_FLASH_WEL_DISABLE					0x00U
#define QSPI_FLASH_WIP_ENABLE					0x01U
#define QSPI_FLASH_WIP_DISABLE					0x00U
#define QSPI_FLASH_WIP_FIELD					0x01U
/* CR3V feature flags */
#define QSPI_FLASH_PAGE512_ENABLE				0x10U
#define QSPI_FLASH_BLANK_CHECK_ENABLE			0x20U

#define FLASH_SIZE_1MB_LOG2					0x14U
#define FLASH_SIZE_16MB_LOG2				0x18U
#define FLASH_SIZE_32MB_LOG2				0x19U
#define FLASH_SIZE_64MB_LOG2				0x1AU

#define PAGE_SIZE_256B_LOG2					0x8U
#define PAGE_SIZE_512B_LOG2					0x9U

/* Manufacturer ID */
#define MANUFACTURE_ID_SPANSION				0x01U
#define MANUFACTURE_ID_WINBOND				0xEFU
#define MANUFACTURE_ID_MICRON				0x20U
#define MANUFACTURE_ID_MACRONIX				0xC2U

#define DEVICE_ID_LEN						3U

/* Bit definitions of flag in vendor_info */
#define FLAG_PAGE512					0x01U
#define FLAG_QPI					0x02U
#define FLAG_QUAD					0x04U
#define FLAG_BULK					0x08U
#define FLAG_BLANK_CHK					0x10U
#define FLAG_DDR					0x20U
#define FLAG_UNIFORM					0x40U
#define FLAG_PAGE512_FIXED				0x80U

/* QSPI Transfer timeout 5sec */
#define QSPI_XFER_TIMEOUT 5000000U

/* Error definiitions */
#define AUX_INFO_INVALID_PARAMS 0
#define AUX_INFO_INVALID_PARAMS1 1
#define AUX_INFO_INVALID_PARAMS2 2
#define AUX_INFO_INVALID_PARAMS3 3
#define AUX_INFO_INVALID_PARAMS4 4
#define AUX_INFO_INVALID_PARAMS5 5
#define AUX_INFO_INVALID_PARAMS6 6
#define AUX_INFO_INVALID_PARAMS7 7
#define AUX_INFO_INVALID_PARAMS8 8
#define AUX_INFO_IOCTL_NOT_SUPPORTED 9
#define AUX_INFO_NOT_INITIALIZED 10
#define AUX_INFO_NO_MEMORY_1 11 /* 0xB */
#define AUX_INFO_NO_MEMORY_2 12
#define AUX_INFO_NO_MEMORY_3 13
#define AUX_INFO_NO_MEMORY_4 14
#define AUX_INFO_WIP_TIMEOUT 15 /* 0xF*/
#define AUX_INFO_WEN_TIMEOUT 16
#define AUX_INFO_FLAG_TIMEOUT 17 /* 0x11 */
#define AUX_INFO_NOT_ALIGNED 18

struct tegrabl_qspi_flash_chip_info {
	uint32_t flash_size_log2;
	uint32_t block_size_log2;
	uint32_t block_count;
	uint32_t sector_size_log2;
	uint32_t sector_count;
	uint32_t parameter_sector_size_log2;
	uint32_t parameter_sector_count;
	uint32_t address_length;
	uint32_t device_list_index;
	uint32_t page_write_size;
	uint32_t qpi_bus_width;
	bool qddr_read;
};

struct tegrabl_qspi_flash_driver_info {
	struct tegrabl_qspi_flash_platform_params plat_params;
	struct tegrabl_qspi_flash_chip_info chip_info;
	struct tegrabl_qspi_handle *hqspi;
	struct tegrabl_qspi_transfer *transfers;
	uint8_t *address_data;
	uint8_t *cmd;
};

struct device_info {
	char    name[32];
	uint8_t manufacture_id;
	uint8_t memory_type;
	uint8_t density;
	uint8_t sector_size;
	uint8_t parameter_sector_size;
	uint8_t parameter_sector_cnt;
	uint8_t flag;
};

tegrabl_error_t qspi_read_reg(struct tegrabl_qspi_flash_driver_info *hqfdi,
							uint32_t reg_access_cmd, uint8_t *p_reg_val);

tegrabl_error_t qspi_write_reg(struct tegrabl_qspi_flash_driver_info *hqfdi,
							uint32_t reg_access_cmd, uint8_t *p_reg_val);

tegrabl_error_t qspi_write_en(struct tegrabl_qspi_flash_driver_info *hqfdi, bool benable);


tegrabl_error_t qspi_writein_progress(struct tegrabl_qspi_flash_driver_info *hqfdi, bool benable,
										bool is_mdelay);

#if defined(__cplusplus)
}
#endif

#endif  /* ifndef INCLUDED_TEGRABL_QSPI_FLASH_PRIVATE_H */
