/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#define MODULE TEGRABL_ERR_SPI_FLASH

#include <stdint.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_qspi.h>
#include <tegrabl_qspi_flash.h>
#include <tegrabl_qspi_flash_private.h>
#include <tegrabl_qspi_flash_macronix.h>


#define BITS_TO_BYTES_SHIFT_FACTOR		3

/* Location/address of size parameter in SFDP table */
#define QSPI_FLASH_SFDP_SIZE_LOC_MACRONIX		0x34U

/* Macronix commands */
#define QSPI_FLASH_CMD_EQIO						0x35U
#define QSPI_FLASH_CMD_RSTQIO					0xF5U


#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)

tegrabl_error_t qspi_flash_qpi_mode_enable_macronix(struct tegrabl_qspi_flash_driver_info *hqfdi,
													bool benable)
{
	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	struct tegrabl_qspi_transfer transfers;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t command;

	(void)memset(&transfers, 0, sizeof(transfers));

	if (benable) {
		command = QSPI_FLASH_CMD_EQIO;
	} else {
		command = QSPI_FLASH_CMD_RSTQIO;
	}

	transfers.tx_buf = &command;
	transfers.rx_buf = NULL;
	transfers.write_len = QSPI_FLASH_COMMAND_WIDTH;
	transfers.read_len = 0;
	transfers.mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers.bus_width = chip_info->qpi_bus_width;
	transfers.dummy_cycles = ZERO_CYCLES;
	transfers.op_mode = SDR_MODE;

	err = tegrabl_qspi_transaction(hqfdi->hqspi, &transfers, 1,
								   QSPI_XFER_TIMEOUT);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Macronix QPI %s failed, (err:0x%x)\n", benable ? "enable" : "disable", err);
		goto exit;
	}

	pr_trace("Macronix QPI %s\n", benable ? "enabled" : "disabled");

exit:
	return err;
}

#endif

tegrabl_error_t qspi_flash_get_size_macronix(struct tegrabl_qspi_flash_driver_info *hqfdi)
{
	struct tegrabl_qspi_transfer transfers[3];
	uint32_t device_size;
	uint32_t compare_val;
	uint8_t i;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t size_log2 = 0;
	uint8_t command = QSPI_FLASH_CMD_RSFDP;
	/* 3-byte address for getting size from SFDP table. MSB first */
	uint8_t addr[3] = {0, 0, QSPI_FLASH_SFDP_SIZE_LOC_MACRONIX};

	(void)memset(&transfers, 0, sizeof(transfers));

	/* Sending read SFDP parameters command */
	transfers[0].tx_buf = &command;
	transfers[0].rx_buf = NULL;
	transfers[0].write_len = 1;
	transfers[0].read_len = 0;
	transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers[0].bus_width = QSPI_BUS_WIDTH_X1;
	transfers[0].dummy_cycles = ZERO_CYCLES;
	transfers[0].op_mode = SDR_MODE;

	/* Sending address of 'device size' parameter in SFDP table */
	transfers[1].tx_buf = &addr[0];
	transfers[1].rx_buf = NULL;
	transfers[1].write_len = 3;
	transfers[1].read_len = 0;
	transfers[1].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;
	transfers[1].bus_width = QSPI_BUS_WIDTH_X1;
	transfers[1].dummy_cycles = EIGHT_CYCLES;
	transfers[1].op_mode = SDR_MODE;

	/* Read the 4-byte value of size parameter */
	transfers[2].tx_buf = NULL;
	transfers[2].rx_buf = (uint8_t *)&device_size;
	transfers[2].write_len = 0;
	transfers[2].read_len = 4;
	transfers[2].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;
	transfers[2].bus_width = QSPI_BUS_WIDTH_X1;
	transfers[2].dummy_cycles = ZERO_CYCLES;
	transfers[2].op_mode = SDR_MODE;

	err = tegrabl_qspi_transaction(hqfdi->hqspi, &transfers[0], QSPI_FLASH_NUM_OF_TRANSFERS,
									QSPI_XFER_TIMEOUT);

	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error(%x) reading SFDP parameter\n", err);
		goto exit;
	}

	/* Device size received is w.r.t 0 */
	device_size += 1u;

	/* This device size is in no. of bits. Convert to bytes by dividing by 8*/
	device_size >>= BITS_TO_BYTES_SHIFT_FACTOR;

	/* Calculate log2 for a 32 bit no. */
	for (i = 16u; i >= 1u; i /= 2u) {
		compare_val = (1UL << i);
		if (device_size >= compare_val) {
			device_size >>= i;
			size_log2 += i;
		}
	}

	hqfdi->chip_info.flash_size_log2 = size_log2;

exit:
	return err;
}
