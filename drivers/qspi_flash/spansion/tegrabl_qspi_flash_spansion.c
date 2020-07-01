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
#include <tegrabl_qspi_flash_spansion.h>

#define QSPI_FLASH_REG_CR1V					0x800002U
#define QSPI_FLASH_REG_CR2V					0x800003U
#define QSPI_FLASH_REG_CR3V					0x800004U

/* Spansion QPI config */
#define QSPI_FLASH_CR2V_QPI_DISABLE			0x00U
#define QSPI_FLASH_CR2V_QPI_ENABLE			0x40U
#define QSPI_FLASH_SPANSION_QPI_BIT_LOG2	6U

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)

tegrabl_error_t qspi_flash_qpi_mode_enable_spansion(struct tegrabl_qspi_flash_driver_info *hqfdi,
													bool benable)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t read_cfg_cmd;
	uint32_t write_cfg_cmd;
	uint8_t reg_val;
	uint8_t input_cfg;
	uint8_t qpi_bit_log2;

	read_cfg_cmd = (uint32_t) QSPI_FLASH_REG_CR2V;
	write_cfg_cmd = (uint32_t) QSPI_FLASH_REG_CR2V;
	qpi_bit_log2 = (uint8_t) QSPI_FLASH_SPANSION_QPI_BIT_LOG2;
	if (benable) {
		input_cfg = (uint8_t) QSPI_FLASH_CR2V_QPI_ENABLE;
	} else {
		input_cfg = (uint8_t) QSPI_FLASH_CR2V_QPI_DISABLE;
	}

	err = qspi_read_reg(hqfdi, read_cfg_cmd, &reg_val);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Read CR2V fail (err:0x%x)\n", err);
		goto exit;
	}

	if ((reg_val & (1U << qpi_bit_log2)) == input_cfg) {
		pr_trace("Spansion QPI flag is already %s\n", bset ? "set" : "clear");
		goto exit;
	}

	reg_val &= ~(1U << qpi_bit_log2);
	reg_val |= input_cfg;

	err = qspi_write_reg(hqfdi, write_cfg_cmd, &reg_val);

	if (err != TEGRABL_NO_ERROR) {
		pr_error("Spansion QPI %s failed, (err:0x%x)\n",  benable ? "enable" : "disable", err);
		goto exit;
	}

	pr_trace("Spansion QPI setting %s is done\n", benable ? "enable" : "disable");

exit:
	return err;
}

#endif

tegrabl_error_t qspi_flash_x4_enable_spansion(struct tegrabl_qspi_flash_driver_info *hqfdi, uint8_t bset)
{
	struct tegrabl_qspi_transfer transfers[3];
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t tried = 0;
	static uint32_t bquadset;
	uint8_t command;
	uint8_t reg_val;
	uint8_t cmd_addr_buf[3];
	uint8_t input_cfg;

	if (((bquadset != 0U) && (bset != 0U)) ||
		((bquadset == 0U) && (bset == 0U))) {
		pr_trace("%s: QUAD flag is already %s\n", _func__, bset ? "set" : "clear");
		goto exit;
	}

	/* Check if QUAD bit is programmed in the H/w already.
	   This will happen when we are calling this function first time
	   and BR has already programmed the bit.
	   From next call, since bquadset is updated, we won't reach here */
	err = qspi_read_reg(hqfdi, QSPI_FLASH_CMD_RDCR, &reg_val);

	if (err != TEGRABL_NO_ERROR) {
		pr_error("RDCR cmd failed, (err:0x%x)\n", err);
		goto exit;
	}

	if (bset != 0U) {
		input_cfg = QSPI_FLASH_QUAD_ENABLE;
	} else {
		input_cfg = QSPI_FLASH_QUAD_DISABLE;
	}

	if ((reg_val & QSPI_FLASH_QUAD_ENABLE) == input_cfg) {
		bquadset = bset;
		goto exit;
	}

	do {
		if (tried++ == QSPI_FLASH_WRITE_ENABLE_WAIT_TIME) {
			pr_error("Timeout for changing QE it\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_FLAG_TIMEOUT);
			goto exit;
		}

		(void)memset(transfers, 0, sizeof(transfers));

		err = qspi_read_reg(hqfdi, QSPI_FLASH_CMD_RDSR1, &reg_val);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("RDSR1 cmd failed, (err:0x%x)\n", err);
			goto exit;
		}

		err = qspi_write_en(hqfdi, true);
		if (err != TEGRABL_NO_ERROR) {
			goto exit;
		}

		command = QSPI_FLASH_CMD_WRAR;
		transfers[0].tx_buf = &command;
		transfers[0].rx_buf = NULL;
		transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
		transfers[0].read_len = 0;
		transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[0].bus_width = QSPI_BUS_WIDTH_X1;
		transfers[0].dummy_cycles = ZERO_CYCLES;
		transfers[0].op_mode = SDR_MODE;

		cmd_addr_buf[0] = (uint8_t) (QSPI_FLASH_REG_CR1V >> 16) & 0xFFU;
		cmd_addr_buf[1] = (uint8_t) (QSPI_FLASH_REG_CR1V >> 8) & 0xFFU;
		cmd_addr_buf[2] = (uint8_t) QSPI_FLASH_REG_CR1V & 0xFFU;

		transfers[1].tx_buf = &cmd_addr_buf[0];
		transfers[1].rx_buf = NULL;
		transfers[1].write_len = 3;
		transfers[1].read_len = 0;
		transfers[1].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[1].bus_width = QSPI_BUS_WIDTH_X1;
		transfers[1].dummy_cycles = ZERO_CYCLES;
		transfers[1].op_mode = SDR_MODE;

		transfers[2].tx_buf = &input_cfg;
		transfers[2].rx_buf = NULL;
		transfers[2].write_len = QSPI_FLASH_COMMAND_WIDTH;
		transfers[2].read_len = 0;
		transfers[2].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[2].bus_width = QSPI_BUS_WIDTH_X1;
		transfers[2].dummy_cycles = ZERO_CYCLES;
		transfers[2].op_mode = SDR_MODE;

		err = tegrabl_qspi_transaction(hqfdi->hqspi, transfers, 3,
									   QSPI_XFER_TIMEOUT);

		if (err != TEGRABL_NO_ERROR) {
			pr_error("X4 enable Spansion: WRAR failed, (err:0x%x)\n", err);
			goto exit;
		}
		pr_debug("X4 enable Spansion: Waiting for WIP to clear\n");
		do {
			err = qspi_read_reg(hqfdi, QSPI_FLASH_CMD_RDSR1, &reg_val);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("X4 enable Spansion: RDSR1 cmd failed, (err:0x%x)\n", err);
				goto exit;
			}
		} while ((reg_val & QSPI_FLASH_WIP_ENABLE) == QSPI_FLASH_WIP_ENABLE);
		pr_debug("X4 enable Spansion: WIP is cleared\n");

		err = qspi_read_reg(hqfdi, QSPI_FLASH_CMD_RDCR, &reg_val);

		if (err != TEGRABL_NO_ERROR) {
			pr_error("RDCR cmd fail (err:0x%x)\n", err);
			goto exit;
		}
	} while ((reg_val & QSPI_FLASH_QUAD_ENABLE) != input_cfg);

	bquadset = bset;

	err = qspi_write_en(hqfdi, false);

exit:
	return err;
}

#if !defined(CONFIG_DISABLE_QSPI_FLASH_WRITE_512B_PAGE)
tegrabl_error_t qspi_flash_page_512bytes_enable_spansion(struct tegrabl_qspi_flash_driver_info *hqfdi)
{
	tegrabl_error_t err;
	uint8_t reg_val;
	/* Do not error out if we fail to enable 512B page */
	/* programming buffer. Rest of the functionality still works. */
	pr_trace("Spansion: Request to set page size to 512B.\n");

	err = qspi_read_reg(hqfdi, QSPI_FLASH_REG_CR3V, &reg_val);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Read CR3V cmd failed, (err:0x%x)\n", err);
		return TEGRABL_NO_ERROR;
	}

	reg_val |= (uint8_t) QSPI_FLASH_PAGE512_ENABLE;
	err = qspi_write_reg(hqfdi, QSPI_FLASH_REG_CR3V, &reg_val);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Write CR3V cmd failed, (err:0x%x)\n", err);
		return TEGRABL_NO_ERROR;
	}

	err = qspi_writein_progress(hqfdi, QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_US);

	if (err == TEGRABL_NO_ERROR) {
		err = qspi_read_reg(hqfdi, QSPI_FLASH_REG_CR3V, &reg_val);
		if ((err == TEGRABL_NO_ERROR) &&
			((reg_val & QSPI_FLASH_PAGE512_ENABLE) != 0U)) {
			hqfdi->chip_info.page_write_size = 512;
			pr_trace("QSPI Flash: Set 512B page size\n");
		} else {
			pr_error("CR3V cmd failed, (err:0x%x)\n", err);
		}
	}

	return err;
}
#endif

