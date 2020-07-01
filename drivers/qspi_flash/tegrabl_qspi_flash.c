/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#define MODULE TEGRABL_ERR_SPI_FLASH

#include "build_config.h"
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <tegrabl_drf.h>
#include <tegrabl_malloc.h>
#include <tegrabl_clock.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_qspi.h>
#include <tegrabl_qspi_flash.h>
#include <tegrabl_qspi_flash_private.h>
#include <tegrabl_mb1_bct.h>

enum {
	AUX_INFO_INVALID_PARAMS,
	AUX_INFO_INVALID_PARAMS1,
	AUX_INFO_INVALID_PARAMS2,
	AUX_INFO_INVALID_PARAMS3,
	AUX_INFO_INVALID_PARAMS4,
	AUX_INFO_INVALID_PARAMS5,
	AUX_INFO_INVALID_PARAMS6,
	AUX_INFO_INVALID_PARAMS7,
	AUX_INFO_INVALID_PARAMS8,
	AUX_INFO_IOCTL_NOT_SUPPORTED,
	AUX_INFO_NOT_INITIALIZED,
	AUX_INFO_NO_MEMORY, /* 0xA */
	AUX_INFO_WIP_TIMEOUT,
	AUX_INFO_WEN_TIMEOUT,
	AUX_INFO_FLAG_TIMEOUT, /* 0xD */
	AUX_INFO_NOT_ALIGNED,
};

static struct tegrabl_mb1bct_qspi_params *qspi_params;


struct tegrabl_qspi_flash qspi_flash;
struct tegrabl_qspi_flash *hqspi_flash;

static tegrabl_error_t qspi_bdev_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count);
static tegrabl_error_t qspi_bdev_ioctl(
		struct tegrabl_bdev *dev, uint32_t ioctl, void *argp);
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
static tegrabl_error_t qspi_bdev_erase(tegrabl_bdev_t *dev, bnum_t block,
	bnum_t count, bool is_secure);
static tegrabl_error_t qspi_bdev_write_block(tegrabl_bdev_t *dev,
		const void *buf, bnum_t block, bnum_t count);
#endif
static tegrabl_error_t qspi_read_reg(uint32_t code, uint8_t *p_reg_val);
static tegrabl_error_t qspi_write_reg(uint32_t code, uint8_t *p_reg_val);
static tegrabl_error_t qspi_write_en(uint8_t benable);
static tegrabl_error_t qspi_writein_progress(
		uint8_t benable, uint8_t is_mdelay);
static tegrabl_error_t qspi_quad_flag_set(uint8_t bset);
static tegrabl_error_t read_device_id_info(void);
static tegrabl_error_t qspi_bdev_ioctl(
		struct tegrabl_bdev *dev, uint32_t ioctl, void *argp)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(argp);

	switch (ioctl) {
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	case TEGRABL_IOCTL_DEVICE_CACHE_FLUSH:
		break;
#endif
	default:
		pr_debug("Unknown ioctl %"PRIu32"\n", ioctl);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED,
							AUX_INFO_IOCTL_NOT_SUPPORTED);
	}

	return err;
}

tegrabl_error_t tegrabl_qspi_flash_open(
				struct tegrabl_mb1bct_qspi_params *params)
{
	static bool is_init_done;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_bdev_t *qspi_dev = NULL;

	if (!params) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS);
	}

	/* check if it's already initialized */
	if (is_init_done)
		return TEGRABL_NO_ERROR;

	/* initialize qspi hardware */
	qspi_params = params;
	err = tegrabl_qspi_open(qspi_params);
	if (err != TEGRABL_NO_ERROR) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED,
							 AUX_INFO_NOT_INITIALIZED);
	}

	/* allocate memory for qspi handle. */
	qspi_dev = tegrabl_calloc(1, sizeof(tegrabl_bdev_t));
	if (!qspi_dev) {
		pr_debug("Qspi malloc failed\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, AUX_INFO_NO_MEMORY);
	}

	hqspi_flash = &qspi_flash;

	/* read device identification info */
	err = read_device_id_info();
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	tegrabl_blockdev_initialize_bdev(qspi_dev,
					(TEGRABL_STORAGE_QSPI_FLASH << 16 | 0),
					hqspi_flash->block_size_log2,
					hqspi_flash->block_count);

	/* Fill bdev function pointers. */
	qspi_dev->read_block = qspi_bdev_read_block;
	qspi_dev->ioctl = qspi_bdev_ioctl;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	qspi_dev->write_block = qspi_bdev_write_block;
	qspi_dev->erase = qspi_bdev_erase;
#endif

	/*  Make sure flash is in X4 mode by default*/
	err = qspi_quad_flag_set(1);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	/* Register qspi boot device. */
	err = tegrabl_blockdev_register_device(qspi_dev);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("Qspi block dev registration fail (err:0x%x)\n", err);
		goto init_cleanup;
	} else {
		pr_info("Qspi initialized successfully\n");
		is_init_done = true;
		return TEGRABL_NO_ERROR;
	}

init_cleanup:
	tegrabl_free(qspi_dev);
	return err;
}

static tegrabl_error_t read_device_id_info(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t reg_val;
	uint8_t device_flag;

#if defined(CONFIG_ENABLE_QSPI_FLASH_READ_ID)
	struct tegrabl_qspi_transfer transfers[2];
	uint8_t command;
	uint8_t buf_id[DEVICE_ID_LEN];
	uint32_t i, device_cnt;

	command = QSPI_FLASH_CMD_RDID;
	transfers[0].tx_buf = &command;
	transfers[0].rx_buf = NULL;
	transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
	transfers[0].read_len = 0;
	transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers[0].bus_width = QSPI_BUS_WIDTH_X1;
	transfers[0].dummy_cycles = ZERO_CYCLES;
	transfers[0].op_mode = SDR_MODE;

	transfers[1].tx_buf = NULL;
	transfers[1].rx_buf = buf_id;
	transfers[1].write_len = 0;
	transfers[1].read_len = DEVICE_ID_LEN;
	transfers[1].mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers[1].bus_width = QSPI_BUS_WIDTH_X1;
	transfers[1].dummy_cycles = ZERO_CYCLES;
	transfers[1].op_mode = SDR_MODE;

	err = tegrabl_qspi_transaction(transfers, 2);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: register (0x%x) read fail (err:0x%x)\n",
				 __func__, command, err);
		return err;
	}

	device_cnt = sizeof(device_info_list)/sizeof(struct device_info);
	for (i = 0; i < device_cnt; i++) {
		if (buf_id[0] == device_info_list[i].manufacture_id &&
		buf_id[1] == device_info_list[i].memory_type &&
		buf_id[2] == device_info_list[i].density) {
			pr_info("QSPI Flash: %s\n", device_info_list[i].name);
			break;
		}
	}

	if (i >= device_cnt) {
		i = 0;
		pr_info("QSPI Flash: Defaulting to %s\n",
					device_info_list[i].name);
	}

	hqspi_flash->device_list_index = i;
	hqspi_flash->flash_size_log2 = buf_id[2];

	/* Treat < 16MBytes density as an error */
	if (hqspi_flash->flash_size_log2 < FLASH_SIZE_16MB_LOG2) {
		pr_error("QSPI Flash: Insufficient flash size (%d MB)\n",
			1 << (hqspi_flash->flash_size_log2 -
					FLASH_SIZE_1MB_LOG2));
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID,
					AUX_INFO_INVALID_PARAMS7);
	} else if (hqspi_flash->flash_size_log2 >= 0x20) {
		/* Adjust for devices with bloated flash densities */
		hqspi_flash->flash_size_log2 -= 6;
	}

	pr_debug("Memory Interface Type = %x\n", buf_id[1]);
	pr_info("QSPI Flash Size = %d MB\n",
		1 << (hqspi_flash->flash_size_log2 - FLASH_SIZE_1MB_LOG2));

	/* Sector Size */
	hqspi_flash->sector_size_log2 = device_info_list[i].sector_size;
	hqspi_flash->sector_count = 1 << (hqspi_flash->flash_size_log2 -
						hqspi_flash->sector_size_log2);
	/* Subsector (parameter sector) size         */
	/* On some Spansion devices, there're        */
	/* 8x4K parameter sectors on top or bottom   */
	/* It's different with "subsector" on Micron */
	/* We ignore "subsector" now                 */
	hqspi_flash->parameter_sector_size_log2 =
				device_info_list[i].parameter_sector_size;
	hqspi_flash->parameter_sector_count =
				device_info_list[i].parameter_sector_cnt;

	/* if size is > 16 MB (2 ^ 24), we need to address with 4 bytes */
	if (hqspi_flash->flash_size_log2 > 24) {
		hqspi_flash->address_length = 4;
	} else {
		hqspi_flash->address_length = 3;
	}

#else
	/* Default to Spansion 64MBytes */
	hqspi_flash->device_list_index = 1;
	/* 2 ^ 26 = 64 MB */
	hqspi_flash->flash_size_log2 = 26;

	/* 2 ^ 18 = 256 KB */
	hqspi_flash->sector_size_log2 = 18;
	hqspi_flash->sector_count = 1 << (hqspi_flash->flash_size_log2 -
								hqspi_flash->sector_size_log2);

	/* 2 ^ 14 = 16 KB */
	hqspi_flash->parameter_sector_size_log2 = 14;
	hqspi_flash->parameter_sector_count = 8;

	hqspi_flash->address_length = 4;
#endif /* CONFIG_ENABLE_QSPI_FLASH_READ_ID */

	device_flag = device_info_list[hqspi_flash->device_list_index].flag;

	hqspi_flash->qddr_read = 0;
#if defined(CONFIG_ENABLE_QSPI_QDDR_READ)
	if (device_flag & FLAG_DDR)
		hqspi_flash->qddr_read = 1;
#endif

	/* 2 ^ 9 = 512 Bytes */
	hqspi_flash->block_size_log2 = 9;

	hqspi_flash->block_count = 1 << (hqspi_flash->flash_size_log2 -
					hqspi_flash->block_size_log2);

	hqspi_flash->page_write_size = 256;
	hqspi_flash->qpi_bus_width = QSPI_BUS_WIDTH_X1;

	if (device_flag & FLAG_PAGE512) {
		/* Do not error out if we fail to enable 512B page */
		/* programming buffer. Rest of the functionality still works. */
		pr_debug("%s : Request to set page size to 512B.\n", __func__);

		err = qspi_read_reg(QSPI_FLASH_REG_CR3V, &reg_val);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: read CR3V cmd fail (err:0x%x)\n",
					__func__, err);
			return TEGRABL_NO_ERROR;
		}

		reg_val |= QSPI_FLASH_PAGE512_ENABLE;
		err = qspi_write_reg(QSPI_FLASH_REG_CR3V, &reg_val);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: write CR3V cmd fail (err:0x%x)\n",
					__func__, err);
			return TEGRABL_NO_ERROR;
		}

		err = qspi_writein_progress(QSPI_FLASH_WIP_WAIT_FOR_READY,
						QSPI_FLASH_WIP_WAIT_IN_US);

		if (err == TEGRABL_NO_ERROR) {
			err = qspi_read_reg(QSPI_FLASH_REG_CR3V, &reg_val);
			if (err != TEGRABL_NO_ERROR) {
				pr_debug("%s: read CR3V cmd fail (err:0x%x)\n",
						__func__, err);
				return TEGRABL_NO_ERROR;
			}

			if (reg_val & QSPI_FLASH_PAGE512_ENABLE) {
				hqspi_flash->page_write_size = 512;
				pr_info("QSPI Flash: Set 512B page size\n");
			}
		}
	}

	return err;
}

tegrabl_error_t tegrabl_qspi_flash_reinit(
					struct tegrabl_mb1bct_qspi_params *params)
{
	static bool is_reinit_done;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (!params) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS1);
	}

	if (is_reinit_done) {
		return TEGRABL_NO_ERROR;
	}

	/* initialize new params */
	qspi_params = params;

	err = tegrabl_qspi_reinit(params);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	is_reinit_done = true;

	/* re-init with mb1bct dev-params */
	pr_info("Qspi reinitialized\n");
	return err;
}

static tegrabl_error_t qspi_read_reg(
		uint32_t reg_access_cmd,
		uint8_t *p_reg_val)
{
	struct tegrabl_qspi_transfer transfers[3];
	uint8_t cmd_addr_buf[8];
	uint8_t command;
	bool is_ext_reg_access_cmd = (reg_access_cmd & 0xFFFF00) ? 1 : 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	memset(transfers, 0, sizeof(transfers));
	cmd_addr_buf[0] = (reg_access_cmd >> 16) & 0xFF;
	cmd_addr_buf[1] = (reg_access_cmd >> 8) & 0xFF;
	cmd_addr_buf[2] = reg_access_cmd & 0xFF;
	command = QSPI_FLASH_CMD_RDAR;

	if (is_ext_reg_access_cmd == true) {
		transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[0].bus_width = hqspi_flash->qpi_bus_width;
		transfers[0].dummy_cycles = ZERO_CYCLES;
		transfers[0].op_mode = SDR_MODE;
		transfers[0].tx_buf = &command;
		transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
	}

	if (is_ext_reg_access_cmd == true) {
		transfers[1].tx_buf = cmd_addr_buf;
		transfers[1].dummy_cycles = 0;
		/* Extra dummy cycles needed in QPI mode */
		if (hqspi_flash->qpi_bus_width == QSPI_BUS_WIDTH_X4)
			transfers[1].write_len = 3 + 4;
		else
			transfers[1].write_len = 3 + 1;
	} else {
		transfers[1].tx_buf = &cmd_addr_buf[2];
		transfers[1].write_len = QSPI_FLASH_COMMAND_WIDTH;
		transfers[1].dummy_cycles = ZERO_CYCLES;
	}
	transfers[1].mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers[1].bus_width = hqspi_flash->qpi_bus_width;
	transfers[1].op_mode = SDR_MODE;

	transfers[2].rx_buf = p_reg_val;
	transfers[2].read_len = QSPI_FLASH_COMMAND_WIDTH;
	transfers[2].mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers[2].bus_width = hqspi_flash->qpi_bus_width;
	transfers[2].op_mode = SDR_MODE;

	if (is_ext_reg_access_cmd == true) {
		/* Extended command */
		err = tegrabl_qspi_transaction(transfers, 3);
	} else {
		/* 1-byte command */
		err = tegrabl_qspi_transaction(&transfers[1], 2);
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: register (0x%x) read fail (err:0x%x)\n",
				 __func__, reg_access_cmd, err);
	}
	return err;
}

static tegrabl_error_t qspi_write_reg(
		uint32_t reg_access_cmd,
		uint8_t *p_reg_val)
{
	struct tegrabl_qspi_transfer transfers[3];
	uint8_t cmd_addr_buf[3];
	uint8_t command;
	bool is_ext_reg_access_cmd = (reg_access_cmd & 0xFFFF00) ? 1 : 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = qspi_write_en(1);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: fail to enable write (err:0x%x)\n",
				 __func__, err);
		return err;
	}

	memset(transfers, 0, sizeof(transfers));
	cmd_addr_buf[0] = (reg_access_cmd >> 16) & 0xFF;
	cmd_addr_buf[1] = (reg_access_cmd >> 8) & 0xFF;
	cmd_addr_buf[2] = reg_access_cmd & 0xFF;
	command = QSPI_FLASH_CMD_WRAR;

	if (is_ext_reg_access_cmd == true) {
		transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[0].bus_width = hqspi_flash->qpi_bus_width;
		transfers[0].dummy_cycles = ZERO_CYCLES;
		transfers[0].op_mode = SDR_MODE;
		transfers[0].tx_buf = &command;
		transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
	}

	if (is_ext_reg_access_cmd == true) {
		transfers[1].tx_buf = cmd_addr_buf;
		transfers[1].write_len = 3;
	} else {
		transfers[1].tx_buf = &cmd_addr_buf[2];
		transfers[1].write_len = QSPI_FLASH_COMMAND_WIDTH;
	}
	transfers[1].mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers[1].bus_width = hqspi_flash->qpi_bus_width;
	transfers[1].dummy_cycles = ZERO_CYCLES;
	transfers[1].op_mode = SDR_MODE;

	transfers[2].tx_buf = p_reg_val;
	transfers[2].write_len = QSPI_FLASH_COMMAND_WIDTH;
	transfers[2].mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers[2].bus_width = hqspi_flash->qpi_bus_width;
	transfers[2].dummy_cycles = ZERO_CYCLES;
	transfers[2].op_mode = SDR_MODE;

	if (is_ext_reg_access_cmd == true) {
		/* Extended command */
		err = tegrabl_qspi_transaction(transfers, 3);
	} else {
		/* 1-byte command */
		err = tegrabl_qspi_transaction(&transfers[1], 2);
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: register (0x%x) read fail (err:0x%x)\n",
				 __func__, reg_access_cmd, err);
	}
	return err;
}

static tegrabl_error_t qspi_writein_progress(uint8_t benable, uint8_t is_mdelay)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t tried = 0;
	uint8_t reg_val;
	uint32_t comp;

	pr_debug("%s: waiting for WIP %s ...\n",
			 __func__, benable ? "Enable" : "Disable");

	do {
		if (tried++ == QSPI_FLASH_WIP_RETRY_COUNT) {
			pr_debug("%s :timeout for WIP %s\n",
					 __func__, benable ? "Enable" : "Disable");
			return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_WIP_TIMEOUT);
		}

		if (benable)
			comp = QSPI_FLASH_WIP_ENABLE;
		else
			comp = QSPI_FLASH_WIP_DISABLE;

		err = qspi_read_reg(QSPI_FLASH_CMD_RDSR1, &reg_val);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: read RDSR1 cmd fail (err:0x%x)\n",
					 __func__, err);
			return err;
		}

		if (is_mdelay == QSPI_FLASH_WIP_WAIT_IN_MS) {
			tegrabl_mdelay(QSPI_FLASH_WIP_DISABLE_WAIT_TIME);
		} else {
			tegrabl_udelay(QSPI_FLASH_WIP_DISABLE_WAIT_TIME);
		}

		pr_debug("%s: try::%u regval:(0x%x)\n",
				 __func__, tried, reg_val);

	} while ((reg_val & QSPI_FLASH_WIP_FIELD) != comp);

	pr_debug("%s: WIP %s is done\n", __func__, benable ? "enable" : "disable");

	return err;
}

static tegrabl_error_t qspi_write_en(uint8_t benable)
{
	struct tegrabl_qspi_transfer transfers;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t tried = 0;
	uint8_t command;
	uint8_t reg_val;
	uint32_t comp;

	pr_debug("%s: doing WEN %s\n", __func__, benable ? "enable" : "disable");

	do {
		if (tried++ == QSPI_FLASH_WE_RETRY_COUNT) {
			pr_debug("%s: timeout for WEN %s\n",
					 __func__, benable ? "enable" : "disable");
			return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_WEN_TIMEOUT);
		}

		if (benable != 0U) {
			command = QSPI_FLASH_CMD_WREN;
			comp = QSPI_FLASH_WEL_ENABLE;
		} else {
			command = QSPI_FLASH_CMD_WRDI;
			comp = QSPI_FLASH_WEL_DISABLE;
		}

		transfers.tx_buf = &command;
		transfers.rx_buf = NULL;
		transfers.write_len = QSPI_FLASH_COMMAND_WIDTH;
		transfers.read_len = 0;
		transfers.mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers.bus_width = hqspi_flash->qpi_bus_width;
		transfers.dummy_cycles = ZERO_CYCLES;
		transfers.op_mode = SDR_MODE;

		err = tegrabl_qspi_transaction(&transfers, 1);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: WEN %s fail (err:0x%x)\n",
					 __func__, benable ? "enable" : "disable", err);
			return err;
		}

		tegrabl_udelay(QSPI_FLASH_WRITE_ENABLE_WAIT_TIME);
		err = qspi_read_reg(QSPI_FLASH_CMD_RDSR1, &reg_val);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: read RDSR1 cmd fail (err:0x%x)\n",
					 __func__, err);
			return err;
		}

	} while ((reg_val & QSPI_FLASH_WEL_ENABLE) != comp);

	pr_debug("%s: WEN %s is done\n", __func__, benable ? "enable" : "disable");

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_quad_flag_set(uint8_t bset)
{
	struct tegrabl_qspi_transfer transfers[3];
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t tried = 0;
	static uint32_t bquadset;
	uint8_t command;
	uint8_t reg_val;
	uint8_t cmd_addr_buf[3];
	uint8_t input_cfg;
	uint8_t device_list_index = hqspi_flash->device_list_index;

	if ((qspi_params->width != QSPI_BUS_WIDTH_X4) ||
		(device_info_list[device_list_index].manufacture_id ==
						MANUFACTURE_ID_MICRON)) {
		return TEGRABL_NO_ERROR;
	}

	if ((bquadset && bset) || (!bquadset && !bset)) {
		pr_debug("%s: QUAD flag is already %s\n",
				 __func__, bset ? "set" : "clear");
		return TEGRABL_NO_ERROR;
	}

	pr_debug("%s: %s QUAD flag\n",
			 __func__, bset ? "setting" : "clearing");

	/* Check if QUAD bit is programmed in the H/w already.
	   This will happen when we are calling this function first time
	   and BR has already programmed the bit.
	   From next call, since bquadset is updated, we won't reach here */
	err = qspi_read_reg(QSPI_FLASH_CMD_RDCR, &reg_val);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: read RDCR cmd fail (err:0x%x)\n", __func__, err);
		return err;
	}
	if (bset != 0U)
		input_cfg = QSPI_FLASH_QUAD_ENABLE;
	else
		input_cfg = QSPI_FLASH_QUAD_DISABLE;

	if ((reg_val & QSPI_FLASH_QUAD_ENABLE) == input_cfg) {
		bquadset = bset;
		return TEGRABL_NO_ERROR;
	}

	do {
		if (tried++ == QSPI_FLASH_WRITE_ENABLE_WAIT_TIME) {
			pr_debug("%s: timeout for changing QUAD bit\n",
					 __func__);
			return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_FLAG_TIMEOUT);
		}

		memset(transfers, 0, sizeof(transfers));

		err = qspi_read_reg(QSPI_FLASH_CMD_RDSR1, &reg_val);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: read RDSR1 cmd fail (err:0x%x)\n",
					 __func__, err);
			return err;
		}

		err = qspi_write_en(1);
		if (err != TEGRABL_NO_ERROR) {
			return err;
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


		cmd_addr_buf[0] = (QSPI_FLASH_REG_CR1V >> 16) & 0xFF;
		cmd_addr_buf[1] = (QSPI_FLASH_REG_CR1V >> 8) & 0xFF;
		cmd_addr_buf[2] = QSPI_FLASH_REG_CR1V & 0xFF;

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

		err = tegrabl_qspi_transaction(transfers, 3);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: opcode WRAR fail (err:0x%x)\n",
					 __func__, err);
			return err;
		}

		pr_debug("%s: waiting for WIP to clear\n",
				 __func__);
		do {
			err = qspi_read_reg(QSPI_FLASH_CMD_RDSR1, &reg_val);
			if (err != TEGRABL_NO_ERROR) {
				pr_debug("%s: read RDSR1 cmd fail (err:0x%x)\n",
						 __func__, err);
				return err;
			}
		} while ((reg_val & QSPI_FLASH_WIP_ENABLE) == QSPI_FLASH_WIP_ENABLE);
		pr_debug("%s: WIP is cleared now\n", __func__);

		err = qspi_read_reg(QSPI_FLASH_CMD_RDCR, &reg_val);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: read RDCR cmd fail (err:0x%x)\n", __func__, err);
			return err;
		}
	} while ((reg_val & QSPI_FLASH_QUAD_ENABLE) != input_cfg);

	bquadset = bset;

	err = qspi_write_en(0);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	return TEGRABL_NO_ERROR;
}

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
static tegrabl_error_t qspi_qpi_flag_set(uint8_t bset)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	static uint32_t bqpiset;
	uint32_t read_cfg_cmd;
	uint32_t write_cfg_cmd;
	uint8_t reg_val;
	uint8_t input_cfg = 0;
	uint8_t qpi_bit_log2 = 0;
	uint8_t device_list_index = hqspi_flash->device_list_index;
	uint8_t device_flag = device_info_list[device_list_index].flag;

	if ((qspi_params->width != QSPI_BUS_WIDTH_X4) ||
					((device_flag & FLAG_QPI) == 0U)) {
		hqspi_flash->qpi_bus_width = QSPI_BUS_WIDTH_X1;
		return TEGRABL_NO_ERROR;
	}

	if (((bqpiset != 0U) && (bset != 0U)) ||
		((bqpiset == 0U) && (bset == 0U))) {
		pr_debug("%s: QPI flag is already %s\n",
				__func__, bset ? "set" : "clear");
		goto exit;
	}

	pr_debug("%s: %s QPI flag\n",
				__func__, bset ? "setting" : "clearing");

	/* QPI enable register and bit polarity are different */
	/* for different vendors and their devices */
	switch (device_info_list[device_list_index].manufacture_id) {
	case MANUFACTURE_ID_MICRON:
		read_cfg_cmd = QSPI_FLASH_CMD_RD_EVCR;
		write_cfg_cmd = QSPI_FLASH_CMD_WR_EVCR;
		qpi_bit_log2 = QSPI_FLASH_MICRON_QPI_BIT_LOG2;
		if (bset == false)
			input_cfg = QSPI_FLASH_EVCR_QPI_DISABLE;
		else
			input_cfg = QSPI_FLASH_EVCR_QPI_ENABLE;
		break;

	case MANUFACTURE_ID_SPANSION:
	default:
		read_cfg_cmd = QSPI_FLASH_REG_CR2V;
		write_cfg_cmd = QSPI_FLASH_REG_CR2V;
		qpi_bit_log2 = QSPI_FLASH_SPANSION_QPI_BIT_LOG2;
		if (bset == false)
			input_cfg = QSPI_FLASH_CR2V_QPI_DISABLE;
		else
			input_cfg = QSPI_FLASH_CR2V_QPI_ENABLE;
		break;
	}

	/* Check if QPI bit is programmed in the H/w already. */
	err = qspi_read_reg(read_cfg_cmd, &reg_val);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: read QPI cfg reg read cmd fail (err:0x%x)\n",
				__func__, err);
		return err;
	}

	if ((reg_val & (1 << qpi_bit_log2)) == input_cfg) {
		bqpiset = bset;
		pr_debug("%s: QPI flag read is already %s\n",
				__func__, bset ? "set" : "clear");
		goto exit;
	}

	reg_val &= ~(1 << qpi_bit_log2);
	reg_val |= input_cfg;

	err = qspi_write_reg(write_cfg_cmd, &reg_val);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: write QPI cfg write cmd fail (err:0x%x)\n",
				__func__, err);
		return err;
	}

	pr_debug("%s: QPI flag is %s\n",
				__func__, bset ? "set" : "cleared");

exit:
	if (bset != 0U)
		hqspi_flash->qpi_bus_width = QSPI_BUS_WIDTH_X4;
	else
		hqspi_flash->qpi_bus_width = QSPI_BUS_WIDTH_X1;

	if (bqpiset != bset) {
		bqpiset = bset;
		qspi_writein_progress(QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_US);
	}

	return TEGRABL_NO_ERROR;
}

#define block_num_to_sector_num(blk)		\
		DIV_FLOOR_LOG2((blk << hqspi_flash->block_size_log2), \
					   hqspi_flash->sector_size_log2)

#define block_cnt_to_sector_cnt(cnt)		\
		DIV_FLOOR_LOG2((cnt << hqspi_flash->block_size_log2), \
					  hqspi_flash->sector_size_log2)

/**
 * @brief Initiate the paramter sector erase command.
 *
 * @param start_parameter_sector_num Sector Number to do Erase
 * @param num_of_parameter_sectors  Number of parameter sectors
 * FIXME: do we really need is_top variable ?
 * @param is_top Location of parameter sectors at top/bottom part of flash,
 *               Use is_top = 0 if parameter sectors are located at sector 0
 *               and is_top = 1 if parameter sectors are at last sector
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error
 */
static tegrabl_error_t
tegrabl_qspi_flash_parameter_sector_erase(
		uint32_t start_parameter_sector_num,
		uint32_t num_of_parameter_sectors,
		uint8_t is_top)
{
	struct tegrabl_qspi_transfer transfers[3];
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t address_data[4];
	uint32_t num_of_sectors_to_erase = num_of_parameter_sectors;
	uint32_t address;
	uint8_t cmd;

	if (hqspi_flash->address_length == 4)
		cmd = QSPI_FLASH_CMD_4PARA_SECTOR_ERASE;
	else
		cmd = QSPI_FLASH_CMD_PARA_SECTOR_ERASE;

	if (start_parameter_sector_num > hqspi_flash->parameter_sector_count) {
		pr_debug("%s: incorrect parameter sector number: %u\n",
				 __func__, start_parameter_sector_num);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS2);
	}

	if ((num_of_parameter_sectors == 0) ||
		(num_of_parameter_sectors > hqspi_flash->parameter_sector_count)) {
		pr_debug("%s: incorrect number of sectors: %u\n",
				 __func__, num_of_parameter_sectors);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS3);
	}

	if ((start_parameter_sector_num + num_of_parameter_sectors) >
			hqspi_flash->parameter_sector_count) {
		pr_debug("%s: exceed total param sector count\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS4);
	}

	/*  Make sure flash is in X1 mode */
	err = qspi_quad_flag_set(0);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	if (is_top) {
		address = ((1 << hqspi_flash->flash_size_log2) -
			(hqspi_flash->parameter_sector_count <<
			hqspi_flash->parameter_sector_size_log2));
	} else {
		address = start_parameter_sector_num <<
			 hqspi_flash->parameter_sector_size_log2;
	}

	while (num_of_sectors_to_erase) {
		pr_debug("%s: erasing sub sector of addr: 0x%x\n",
				 __func__, address);
		/*  Enable Write */
		err = qspi_write_en(1);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		/*  address are sent to device with MSB first */
		if (hqspi_flash->address_length == 4) {
			address_data[0] = (address >> 24) & 0xFF;
			address_data[1] = (address >> 16) & 0xFF;
			address_data[2] = (address >> 8) & 0xFF;
			address_data[3] = (address) & 0xFF;
		} else {
			address_data[0] = (address >> 16) & 0xFF;
			address_data[1] = (address >> 8) & 0xFF;
			address_data[2] = (address) & 0xFF;
		}

		/*  Set command Parameters in First Transfer */
		/*  command must be in SDR X1 mode = 0x0 */
		/*  Set Read length is 0 for command */
		transfers[0].tx_buf = &cmd;
		transfers[0].rx_buf = NULL;
		transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
		transfers[0].read_len = 0;
		transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[0].bus_width = QSPI_BUS_WIDTH_X1;
		transfers[0].dummy_cycles = ZERO_CYCLES;
		transfers[0].op_mode = SDR_MODE;


		/*  Set address Parameters in Second Transfer */
		/*  address must be sent in SDR X1 mode */
		/*  Set Read length is 0 for address */

		transfers[1].tx_buf = address_data;
		transfers[1].rx_buf = NULL;
		transfers[1].write_len = hqspi_flash->address_length;
		transfers[1].read_len = 0;
		transfers[1].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;
		transfers[1].bus_width = QSPI_BUS_WIDTH_X1;
		transfers[1].dummy_cycles = ZERO_CYCLES;
		transfers[1].op_mode = SDR_MODE;

		err = tegrabl_qspi_transaction(&transfers[0], 2);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s:sub sector erase addr 0x%x fail(err:0x%x)\n",
					 __func__, address, err);
			/*  Disable Write En bit */
			err = qspi_write_en(0);
			if (err != TEGRABL_NO_ERROR) {
				return err;
			}
			break;
		}
		qspi_writein_progress(QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_US);

		/*  Disable Write En bit */
		err = qspi_write_en(0);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		address += (1 << hqspi_flash->parameter_sector_size_log2);
		num_of_sectors_to_erase--;
	}
	/* Put back in X4 mode */
	err = qspi_quad_flag_set(1);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	return TEGRABL_NO_ERROR;
}

/**
 * @brief Initiate the sector erase for given sectors
 *
 * @param start_sector_num Start sector to be erased
 * @param num_of_sectors Number of sectors to be erased following
 *                       the start sector
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error
 */
static tegrabl_error_t
tegrabl_qspi_flash_sector_erase(
		uint32_t start_sector_num,
		uint32_t num_of_sectors)
{
	struct tegrabl_qspi_transfer transfers[3];
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t address_data[4];
	uint32_t num_of_sectors_to_erase = num_of_sectors;
	uint32_t address;
	uint8_t cmd;

	if (hqspi_flash->address_length == 4)
		cmd = QSPI_FLASH_CMD_4SECTOR_ERASE;
	else
		cmd = QSPI_FLASH_CMD_SECTOR_ERASE;

	if (!num_of_sectors) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS5);
	}

	TEGRABL_ASSERT((start_sector_num + num_of_sectors_to_erase) <=
		hqspi_flash->sector_count);

	/* Make sure flash is in X1 mode */
	err = qspi_quad_flag_set(0);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	address = start_sector_num << hqspi_flash->sector_size_log2;
	while (num_of_sectors_to_erase) {
		pr_debug("erase sector num: %u\n",
				 num_of_sectors - num_of_sectors_to_erase + start_sector_num);
		/* Enable Write */
		err = qspi_write_en(1);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		/* address are sent to device with MSB first */
		if (hqspi_flash->address_length == 4) {
			address_data[0] = (address >> 24) & 0xFF;
			address_data[1] = (address >> 16) & 0xFF;
			address_data[2] = (address >> 8) & 0xFF;
			address_data[3] = (address) & 0xFF;
		} else {
			address_data[0] = (address >> 16) & 0xFF;
			address_data[1] = (address >> 8) & 0xFF;
			address_data[2] = (address) & 0xFF;
		}

		/* Set command Parameters in First Transfer */
		/* command must be in SDR X1 mode = 0x0 */
		/* Set Read length is 0 for command */
		transfers[0].tx_buf = &cmd;
		transfers[0].rx_buf = NULL;
		transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
		transfers[0].read_len = 0;
		transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[0].bus_width = QSPI_BUS_WIDTH_X1;
		transfers[0].dummy_cycles = ZERO_CYCLES;
		transfers[0].op_mode = SDR_MODE;


		/* Set address Parameters in Second Transfer */
		/* address must be sent in SDR X1 mode */
		/* Set Read length is 0 for address */

		transfers[1].tx_buf = address_data;
		transfers[1].rx_buf = NULL;
		transfers[1].write_len = hqspi_flash->address_length;
		transfers[1].read_len = 0;
		transfers[1].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;
		transfers[1].bus_width = QSPI_BUS_WIDTH_X1;
		transfers[1].dummy_cycles = ZERO_CYCLES;
		transfers[1].op_mode = SDR_MODE;


		err = tegrabl_qspi_transaction(&transfers[0], 2);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: sector erase fail addr:0x%x (err:0x%x)\n",
					 __func__, address, err);
			/* Disable Write En bit */
			err = qspi_write_en(0);
			if (err != TEGRABL_NO_ERROR) {
				return err;
			}

			break;
		}
		qspi_writein_progress(QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_US);

		/* Disable Write En bit */
		err = qspi_write_en(0);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		address += (1 << hqspi_flash->sector_size_log2);
		num_of_sectors_to_erase--;
	}

	/* Put back in X4 mode */
	err = qspi_quad_flag_set(1);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	return TEGRABL_NO_ERROR;
}

/**
 * @brief Initiate the bulk erase for whole chip
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error
 */
static tegrabl_error_t
tegrabl_qspi_flash_bulk_erase(void)
{
	struct tegrabl_qspi_transfer transfers;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t cmd;

	/* Enable Write */
	err = qspi_write_en(1);
	if (err != TEGRABL_NO_ERROR)
		return err;

	memset(&transfers, 0, sizeof(struct tegrabl_qspi_transfer));

	cmd = QSPI_FLASH_CMD_BULK_ERASE;
	transfers.tx_buf = &cmd;
	transfers.write_len = QSPI_FLASH_COMMAND_WIDTH;
	transfers.mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers.bus_width = QSPI_BUS_WIDTH_X1;
	transfers.dummy_cycles = ZERO_CYCLES;
	transfers.op_mode = SDR_MODE;

	err = tegrabl_qspi_transaction(&transfers, 1);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: bulk erase fail (err:0x%x)\n", __func__, err);
		return err;
	}

	/* Wait in mdelays */
	err = qspi_writein_progress(QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_MS);
	if (err != TEGRABL_NO_ERROR) {
		pr_info("%s: WIP fail (err:0x%x)\n", __func__, err);
	}
	return err;
}

static tegrabl_error_t qspi_bdev_erase(tegrabl_bdev_t *dev, bnum_t block,
	bnum_t count, bool is_secure)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	TEGRABL_UNUSED(is_secure);
	uint32_t sector_num = 0;
	uint32_t sector_cnt = 0;
	uint8_t *head_backup = NULL;
	uint8_t *tail_backup = NULL;
	bnum_t head_start, head_count;
	bnum_t tail_start, tail_count;
	uint8_t device_info_flag =
			 device_info_list[hqspi_flash->device_list_index].flag;

	if (!dev || !count) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS6);
	}

	if (block == 0 && count == hqspi_flash->block_count &&
						device_info_flag & FLAG_BULK) {
		/* Erase whole device and return */
		pr_info("QSPI: Erasing entire device\n");
		error = tegrabl_qspi_flash_bulk_erase();
		return error;
	}

	head_count = tail_count = 0;
	sector_num = block_num_to_sector_num(block);
	sector_cnt = block_cnt_to_sector_cnt(count);

	pr_info("QSPI: erasing sectors from %u - %u\n",
			sector_num, (sector_cnt - 1) + sector_num);

	if (count != hqspi_flash->block_count) {
		/* Need to preserve the partitions that are overlapping */
		/* on the sector which needs to be erased.              */
		/* head : former overlapping partition                  */
		/* tail : latter overlapping partition                  */
#define PAGES_IN_SECTOR_LOG2 \
	(hqspi_flash->sector_size_log2 - hqspi_flash->block_size_log2)
		head_start = sector_num << PAGES_IN_SECTOR_LOG2;
		head_count = block & ((1 << PAGES_IN_SECTOR_LOG2) - 1);
		tail_start = block + count;
		sector_cnt = tail_start + ((1 << PAGES_IN_SECTOR_LOG2) - 1);
		sector_cnt = block_num_to_sector_num(sector_cnt);
		tail_count = (sector_cnt << PAGES_IN_SECTOR_LOG2) - tail_start;
		sector_cnt -= sector_num;
#undef PAGES_IN_SECTOR_LOG2

		if (head_count != 0) {
			head_backup = tegrabl_calloc(1,
					head_count << hqspi_flash->block_size_log2);
			if (head_backup == NULL) {
				error = TEGRABL_ERROR(
					TEGRABL_ERR_NO_MEMORY, AUX_INFO_NO_MEMORY);
				goto fail;
			}
			error = qspi_bdev_read_block(dev,
					(void *)head_backup, head_start, head_count);
			if (error != TEGRABL_NO_ERROR)
				goto fail;
		}
		if (tail_count != 0) {
			tail_backup = tegrabl_calloc(1,
					tail_count << hqspi_flash->block_size_log2);
			if (tail_backup == NULL) {
				error = TEGRABL_ERROR(
					TEGRABL_ERR_NO_MEMORY, AUX_INFO_NO_MEMORY);
				goto fail;
			}
			error = qspi_bdev_read_block(dev,
					(void *)tail_backup, tail_start, tail_count);
			if (error != TEGRABL_NO_ERROR)
				goto fail;
		}
	}

	/* handling sector-0 erase differently */
	if (sector_num == 0)
		tegrabl_qspi_flash_parameter_sector_erase(0, 8, 0);
	error = tegrabl_qspi_flash_sector_erase(sector_num, sector_cnt);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	if (head_count != 0) {
		error = qspi_bdev_write_block(dev,
			(void *)head_backup, head_start, head_count);
		if (error != TEGRABL_NO_ERROR)
			goto fail;
	}
	if (tail_count != 0) {
		error = qspi_bdev_write_block(dev,
			(void *)tail_backup, tail_start, tail_count);
	}
fail:
	if (head_backup != NULL)
		tegrabl_free(head_backup);
	if (tail_backup != NULL)
		tegrabl_free(tail_backup);

	return error;
}

/**
 * @brief Initiate the writing of multiple pages of data from buffer.
 *
 * @param start_page_num Start page number for which data has to be written
 * @param num_of_pages Number of pages to be written
 * @param p_source_buffer Storage buffer of the data to be written to the device
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error
 */
static tegrabl_error_t
tegrabl_qspi_flash_write(
		uint32_t start_page_num,
		uint32_t num_of_pages,
		uint8_t *p_source_buffer)
{
	struct tegrabl_qspi_transfer transfers[2];
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t cmd_address_info[5];
	uint32_t bytes_to_write;
	uint32_t address;
	uint8_t *p_source = (uint8_t *)p_source_buffer;

	/* Use combined command address buffer */
	if (hqspi_flash->address_length == 4)
		cmd_address_info[0] = QSPI_FLASH_CMD_4PAGE_PROGRAM;
	else
		cmd_address_info[0] = QSPI_FLASH_CMD_PAGE_PROGRAM;

	if (!num_of_pages) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS7);
	}

	/* Setup QPI mode based on device info list */
	/* Switch to X1 if QPI setup fails */
	err = qspi_qpi_flag_set(1);
	if (err != TEGRABL_NO_ERROR) {
		hqspi_flash->qpi_bus_width = QSPI_BUS_WIDTH_X1;
		pr_debug("QPI setup failed err(:0x%x)\n", err);
		return err;
	}

	address = start_page_num << hqspi_flash->block_size_log2;
	bytes_to_write = num_of_pages << hqspi_flash->block_size_log2;

	pr_debug("%s: strt_pg_num(%u) num_of_pgs(%u) write_buf(%p)\n",
			 __func__, start_page_num, num_of_pages, p_source_buffer);

	while (bytes_to_write) {
		pr_debug("%s: sector write addr 0x%x\n", __func__, address);
		/* Enable Write */
		err = qspi_write_en(1);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		/* address are sent to device with MSB first */
		/* Command and address are combined to save transaction time */
		if (hqspi_flash->address_length == 4) {
			cmd_address_info[1] = (address >> 24) & 0xFF;
			cmd_address_info[2] = (address >> 16) & 0xFF;
			cmd_address_info[3] = (address >> 8) & 0xFF;
			cmd_address_info[4] = (address) & 0xFF;
		} else {
			cmd_address_info[1] = (address >> 16) & 0xFF;
			cmd_address_info[2] = (address >> 8) & 0xFF;
			cmd_address_info[3] = (address) & 0xFF;
		}

		/* Make sure the Dest is 4-byte aligned */
		if (((uintptr_t)p_source & 0x3)) {
			return TEGRABL_ERROR(TEGRABL_ERR_BAD_ADDRESS, AUX_INFO_NOT_ALIGNED);
		}

		/* Set command and address Parameters in First Transfer */
		/* address width depends on whether QPI mode is enabled */
		/* Set Read length is 0 for address */

		transfers[0].tx_buf = cmd_address_info;
		transfers[0].rx_buf = NULL;
		transfers[0].write_len = hqspi_flash->address_length + 1;
		transfers[0].read_len = 0;
		transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[0].bus_width = hqspi_flash->qpi_bus_width;
		transfers[0].dummy_cycles = ZERO_CYCLES;
		transfers[0].op_mode = SDR_MODE;

		/* Set WriteData Parameters in Second Transfer */

		transfers[1].tx_buf = p_source;
		transfers[1].rx_buf = NULL;
		transfers[1].write_len =
			MIN(bytes_to_write, hqspi_flash->page_write_size);
		transfers[1].read_len = 0;
		transfers[1].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;
		transfers[1].bus_width = hqspi_flash->qpi_bus_width;
		transfers[1].dummy_cycles = ZERO_CYCLES;
		transfers[1].op_mode = SDR_MODE;

		err = tegrabl_qspi_transaction(&transfers[0], 2);

		if (err != TEGRABL_NO_ERROR) {
			pr_debug("QSPI Flash Write at address:0x%x failed: x%x\n",
					 address, err);
			/* Disable Write En bit */
			err = qspi_write_en(0);
			if (err != TEGRABL_NO_ERROR) {
				return err;
			}

			break;
		}

		if (bytes_to_write > hqspi_flash->page_write_size) {
			bytes_to_write -= hqspi_flash->page_write_size;
			address += hqspi_flash->page_write_size;
			p_source += hqspi_flash->page_write_size;
		} else {
			bytes_to_write = 0;
		}

		qspi_writein_progress(QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_US);
	}

	/* Switch to X1 mode */
	err = qspi_qpi_flag_set(0);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("QPI disable failed err(:0x%x)\n", err);
		return err;
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_bdev_write_block(tegrabl_bdev_t *dev,
		const void *buf, bnum_t block, bnum_t count)
{
	if (!dev || !buf)
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
	else
		return tegrabl_qspi_flash_write(block, count, (uint8_t *)buf);
}
#endif

/**
 * @brief Initiate the reading of multiple pages of data into buffer.
 *
 * @param start_page_num Start Page Number from which to read
 * @param num_of_pages Number of pages to read
 * @param p_dest Storage for the data read from the device.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error
 */
static tegrabl_error_t
tegrabl_qspi_flash_read(
		uint32_t start_page_num,
		uint32_t num_of_pages,
		uint8_t *p_dest)
{
	struct tegrabl_qspi_transfer transfers[3];
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t address_data[5];
	uint32_t bytes_to_read;
	uint32_t address;
	uint8_t *p_destination = (uint8_t *)p_dest;
	uint8_t cmd;

	if (!num_of_pages) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (hqspi_flash->address_length == 4) {
		if (qspi_params->width == QSPI_BUS_WIDTH_X4)
			if (hqspi_flash->qddr_read == 1)
				cmd = QSPI_FLASH_CMD_4DDR_QUAD_IO_READ;
			else
				cmd = QSPI_FLASH_CMD_4QUAD_IO_READ;
		else
			cmd = QSPI_FLASH_CMD_4READ;
	} else {
		if (qspi_params->width == QSPI_BUS_WIDTH_X4)
			if (hqspi_flash->qddr_read == 1)
				cmd = QSPI_FLASH_CMD_DDR_QUAD_IO_READ;
			else
				cmd = QSPI_FLASH_CMD_QUAD_IO_READ;
		else
			cmd = QSPI_FLASH_CMD_READ;
	}

	address = start_page_num << hqspi_flash->block_size_log2;
	bytes_to_read = num_of_pages << hqspi_flash->block_size_log2;
	pr_debug("%s: strt_pg_num(%u) num_of_pgs(%u) read_buf(%p)\n",
			__func__, start_page_num, num_of_pages, p_dest);

	/* address are sent to device with MSB first */
	if (hqspi_flash->address_length == 4) {
		address_data[0] = (address >> 24) & 0xFF;
		address_data[1] = (address >> 16) & 0xFF;
		address_data[2] = (address >> 8) & 0xFF;
		address_data[3] = (address) & 0xFF;
		address_data[4] = 0; /* mode bits */
	} else {
		address_data[0] = (address >> 16) & 0xFF;
		address_data[1] = (address >> 8) & 0xFF;
		address_data[2] = (address) & 0xFF;
		address_data[3] = 0; /* mode bits */
	}

	/* Make sure the Dest is 4-byte aligned */
	if (((uintptr_t)p_dest & 0x3) != 0U) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_ADDRESS, 0);
	}

	/* Set command Parameters in First Transfer */
	/* command must be in SDR X1 mode = 0x0 */
	/* Set Read length is 0 for command */
	transfers[0].tx_buf = &cmd;
	transfers[0].rx_buf = NULL;
	transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
	transfers[0].read_len = 0;
	transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers[0].bus_width = QSPI_BUS_WIDTH_X1;
	transfers[0].dummy_cycles = ZERO_CYCLES;
	transfers[0].op_mode = SDR_MODE;

	/* Set address Parameters in Second Transfer */
	/* address must be sent in DDR Quad IO mode */
	/* Setup Dummy cycles before reading data */
	/* Set Read length is 0 for address */

	transfers[1].tx_buf = address_data;
	transfers[1].rx_buf = NULL;
	transfers[1].read_len = 0;
	transfers[1].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;
	transfers[1].bus_width = qspi_params->width;

	if (qspi_params->width == QSPI_BUS_WIDTH_X4) {
		/* 1 byte for mode bits in write_len */
		transfers[1].write_len = hqspi_flash->address_length + 1;
		transfers[1].dummy_cycles = qspi_params->read_dummy_cycles;
		if (hqspi_flash->qddr_read == 1) {
			transfers[1].op_mode = DDR_MODE;
		} else {
			transfers[1].op_mode = SDR_MODE;
		}

	} else {
		transfers[1].write_len = hqspi_flash->address_length;
		transfers[1].dummy_cycles = ZERO_CYCLES;
		transfers[1].op_mode = SDR_MODE;
	};

	/* Set Readback Parameters in Third Transfer */

	transfers[2].tx_buf = NULL;
	transfers[2].rx_buf = p_destination;
	transfers[2].write_len = 0;
	transfers[2].read_len = bytes_to_read;

	transfers[2].bus_width = qspi_params->width;
	transfers[2].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;

	if (qspi_params->width == QSPI_BUS_WIDTH_X4)
		if (hqspi_flash->qddr_read == 1)
			transfers[2].op_mode = DDR_MODE;
		else
			transfers[2].op_mode = SDR_MODE;
	else
		transfers[2].op_mode = SDR_MODE;

	err = tegrabl_qspi_transaction(&transfers[0],
			QSPI_FLASH_NUM_OF_TRANSFERS);

	return err;
}

static tegrabl_error_t qspi_bdev_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count)
{
	if (!dev || !buf)
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
	else
		return tegrabl_qspi_flash_read(block, count, (uint8_t *)buf);
}
