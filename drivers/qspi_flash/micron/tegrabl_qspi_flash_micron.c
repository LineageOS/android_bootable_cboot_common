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
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_qspi_flash.h>
#include <tegrabl_qspi_flash_private.h>
#include <tegrabl_qspi_flash_micron.h>

/* Micron Register access commands */
#define QSPI_FLASH_CMD_RD_EVCR				0x65U
#define QSPI_FLASH_CMD_WR_EVCR				0x61U

/* Micron QPI config - bit 7: 1 (default) Qpi disbale */
#define QSPI_FLASH_EVCR_QPI_DISABLE			0x80U
#define QSPI_FLASH_EVCR_QPI_ENABLE			0x00U
#define QSPI_FLASH_MICRON_QPI_BIT_LOG2		7U


#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)

tegrabl_error_t qspi_flash_qpi_mode_enable_micron(struct tegrabl_qspi_flash_driver_info *hqfdi, bool benable)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t read_cfg_cmd;
	uint32_t write_cfg_cmd;
	uint8_t reg_val;
	uint8_t input_cfg;
	uint8_t qpi_bit_log2;

	read_cfg_cmd = (uint32_t) QSPI_FLASH_CMD_RD_EVCR;
	write_cfg_cmd = (uint32_t) QSPI_FLASH_CMD_WR_EVCR;
	qpi_bit_log2 = (uint8_t) QSPI_FLASH_MICRON_QPI_BIT_LOG2;
	if (benable) {
		input_cfg = (uint8_t) QSPI_FLASH_EVCR_QPI_ENABLE;
	} else {
		input_cfg = (uint8_t) QSPI_FLASH_EVCR_QPI_DISABLE;
	}

	err = qspi_read_reg(hqfdi, read_cfg_cmd, &reg_val);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Read EVCR fail (err:0x%x)\n", err);
		goto exit;
	}

	if ((reg_val & (1U << qpi_bit_log2)) == input_cfg) {
		pr_trace("Micron QPI flag is already %s\n", bset ? "set" : "clear");
		goto exit;
	}

	reg_val &= ~(1U << qpi_bit_log2);
	reg_val |= input_cfg;

	err = qspi_write_reg(hqfdi, write_cfg_cmd, &reg_val);

	if (err != TEGRABL_NO_ERROR) {
		pr_error("Micron QPI %s failed, (err:0x%x)\n",  benable ? "enable" : "disable", err);
		goto exit;
	}

	pr_trace("Micron QPI setting %s is done\n", benable ? "enable" : "disable");

exit:
	return err;
}


#endif
