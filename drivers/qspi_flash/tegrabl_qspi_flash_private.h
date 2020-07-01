/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
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

#if defined(__cplusplus)
extern "C"
{
#endif

/* QSPI Flash Commands */

/* Read Device Identification Commands */
#define QSPI_FLASH_CMD_REMS					0x90
#define QSPI_FLASH_CMD_RDID					0x9F
#define QSPI_FLASH_CMD_RES					0xAB

/* Spansion Register Access Commands */
#define QSPI_FLASH_CMD_RDSR1				0x05
#define QSPI_FLASH_CMD_RDSR2				0x07
#define QSPI_FLASH_CMD_RDCR					0x35
#define QSPI_FLASH_CMD_RDAR					0x65
#define QSPI_FLASH_CMD_WRR					0x01
#define QSPI_FLASH_CMD_WRDI					0x04
#define QSPI_FLASH_CMD_WREN					0x06
#define QSPI_FLASH_CMD_WRAR					0x71
#define QSPI_FLASH_REG_CR1V					0x800002
#define QSPI_FLASH_REG_CR2V					0x800003
#define QSPI_FLASH_REG_CR3V					0x800004

/* Micron Register access commands */
#define QSPI_FLASH_CMD_RD_EVCR					0x65
#define QSPI_FLASH_CMD_WR_EVCR					0x61

/* 3-byte addressing */
/* Read Flash Array Commands */
#define QSPI_FLASH_CMD_READ					0x03
#define QSPI_FLASH_CMD_FAST_READ			0x0B
#define QSPI_FLASH_CMD_DUAL_IO_READ			0xBB
#define QSPI_FLASH_CMD_QUAD_IO_READ			0xEB
#define QSPI_FLASH_CMD_DDR_DUAL_IO_READ		0xBD
#define QSPI_FLASH_CMD_DDR_QUAD_IO_READ		0xED

/* Program Flash Array Commands */
#define QSPI_FLASH_CMD_PAGE_PROGRAM			0x02
#define QSPI_FLASH_CMD_QUAD_PAGE_PROGRAM	0x32

/* Erase Flash Array Commands */
#define QSPI_FLASH_CMD_PARA_SECTOR_ERASE	0x20
#define QSPI_FLASH_CMD_SECTOR_ERASE			0xD8


/* 4-byte addressing */
/* Read Flash Array Commands */
#define QSPI_FLASH_CMD_4READ				0x13
#define QSPI_FLASH_CMD_4FAST_READ			0x0C
#define QSPI_FLASH_CMD_4DUAL_IO_READ		0xBC
#define QSPI_FLASH_CMD_4QUAD_IO_READ		0xEC
#define QSPI_FLASH_CMD_4DDR_DUAL_IO_READ	0xEE
#define QSPI_FLASH_CMD_4DDR_QUAD_IO_READ	0xEE

/* Program Flash Array Commands */
#define QSPI_FLASH_CMD_4PAGE_PROGRAM		0x12
#define QSPI_FLASH_CMD_4QUAD_PAGE_PROGRAM	0x34

/* Erase Flash Array Commands */
#define QSPI_FLASH_CMD_4PARA_SECTOR_ERASE	0x21
#define QSPI_FLASH_CMD_4SECTOR_ERASE		0xDC
#define QSPI_FLASH_CMD_BULK_ERASE			0xC7


/* Reset Commands */
#define QSPI_FLASH_CMD_SW_RESET_ENABLE		0x66
#define QSPI_FLASH_CMD_SW_RESET				0x99
#define QSPI_FLASH_CMD_MODE_BIT_RESET		0xFF


#define QSPI_FLASH_NUM_OF_TRANSFERS				3
#define QSPI_FLASH_COMMAND_WIDTH				1
#define QSPI_FLASH_ADDRESS_WIDTH				4
#define QSPI_FLASH_QSPI_FLASH_DATA_TRANSFER		2
#define QSPI_FLASH_MAX_TRANSFER_SIZE			(65536 * 4)
#define QSPI_FLASH_CMD_MODE_VAL					0x0
#define QSPI_FLASH_ADDR_DATA_MODE_VAL			0x0
#define QSPI_FLASH_WRITE_ENABLE_WAIT_TIME		1000
#define QSPI_FLASH_WIP_DISABLE_WAIT_TIME		1000
#define QSPI_FLASH_WIP_WAIT_FOR_READY			0
#define QSPI_FLASH_WIP_WAIT_FOR_BUSY			1
#define QSPI_FLASH_WIP_WAIT_IN_US			0
#define QSPI_FLASH_WIP_WAIT_IN_MS			1
#define QSPI_FLASH_WE_RETRY_COUNT				2000
#define QSPI_FLASH_WIP_RETRY_COUNT				2000
#define QSPI_FLASH_SINGLE_WRITE_SIZE			256
#define QSPI_FLASH_QUAD_ENABLE					0x02
#define QSPI_FLASH_QUAD_DISABLE					0x0
#define QSPI_FLASH_WEL_ENABLE					0x02
#define QSPI_FLASH_WEL_DISABLE					0x00
#define QSPI_FLASH_WIP_ENABLE					0x01
#define QSPI_FLASH_WIP_DISABLE					0x00
#define QSPI_FLASH_WIP_FIELD					0x01
#define QSPI_FLASH_PAGE512_ENABLE				0x10

/* Spansion QPI config */
#define QSPI_FLASH_CR2V_QPI_DISABLE				0x00
#define QSPI_FLASH_CR2V_QPI_ENABLE				0x40
#define QSPI_FLASH_SPANSION_QPI_BIT_LOG2			6

/* Micron QPI config - bit 7: 1 (default) Qpi disbale */
#define QSPI_FLASH_EVCR_QPI_DISABLE				0x80
#define QSPI_FLASH_EVCR_QPI_ENABLE				0x00
#define QSPI_FLASH_MICRON_QPI_BIT_LOG2				7

#define FLASH_SIZE_1MB_LOG2				0x14
#define FLASH_SIZE_16MB_LOG2				0x18
#define FLASH_SIZE_32MB_LOG2				0x19
#define FLASH_SIZE_64MB_LOG2				0x1A

#define PAGE_SIZE_256B_LOG2				0x8
#define PAGE_SIZE_512B_LOG2				0x9

/* Manufacturer ID */
#define MANUFACTURE_ID_SPANSION				0x01
#define MANUFACTURE_ID_WINBOND				0xEF
#define MANUFACTURE_ID_MICRON				0x20
#define DEVICE_ID_LEN					3

/* Bit definitions of flag in vendor_info */
#define FLAG_PAGE512							0x01
#define FLAG_QPI							0x02
#define FLAG_QUAD							0x04
#define FLAG_BULK							0x08
#define FLAG_PAGE_SIZE_512						0x10
#define FLAG_DDR							0x20
#define FLAG_UNIFORM							0x40

struct tegrabl_qspi_flash {
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
	uint8_t qpi_bus_width;
	bool qddr_read;
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

struct device_info device_info_list[] = {
	{"Spansion 16MB", 0x01, 0x20, 0x18, 0x10/* 64KB  */, 0x0c/* 4KB */, 8,
				FLAG_DDR | FLAG_QPI | FLAG_BULK | FLAG_PAGE512},
	{"Spansion 64MB", 0x01, 0x02, 0x20, 0x12/* 256KB */, 0x0c/* 4KB */, 8,
				FLAG_DDR | FLAG_QPI | FLAG_BULK | FLAG_PAGE512},
	{"Micron 16MB",   0x20, 0xBB, 0x18, 0x10/* 64KB */,  0x0c/* 4KB */, 0,
				FLAG_QPI | FLAG_BULK},
};

#if defined(__cplusplus)
}
#endif

#endif  /* ifndef INCLUDED_TEGRABL_QSPI_FLASH_PRIVATE_H */
