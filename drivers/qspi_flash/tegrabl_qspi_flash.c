/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <tegrabl_qspi_flash_spansion.h>
#include <tegrabl_qspi_flash_macronix.h>
#include <tegrabl_qspi_flash_micron.h>

#define QSPI_TRANSFERS 3U
#define QSPI_ADDR_LENGTH 5U

static struct device_info device_info_list[] = {
	{"Spansion 16MB", 0x01, 0x20, 0x18, 0x10/* 64KB  */, 0x0c/* 4KB */, 8,
				FLAG_DDR | FLAG_QPI | FLAG_BULK | FLAG_PAGE512},
	{"Spansion 32MB", 0x01, 0x02, 0x19, 0x10/* 64KB  */, 0x0c/* 4KB */, 8,
				FLAG_DDR | FLAG_QPI | FLAG_BULK | FLAG_PAGE512_FIXED},
	{"Spansion 64MB", 0x01, 0x02, 0x20, 0x12/* 256KB */, 0x0c/* 4KB */, 8,
				FLAG_DDR | FLAG_QPI | FLAG_BULK | FLAG_PAGE512},
	{"Micron 16MB", 0x20, 0xBB, 0x18, 0x10/* 64KB */, 0x0c/* 4KB */, 0,
				FLAG_QPI | FLAG_BULK},
	{"Macronix 64MB", 0xC2, 0x95, 0x3A, 0x10/* 64KB */, 0x0c/* 4KB */, 0,
				FLAG_DDR | FLAG_QPI | FLAG_BULK}
};

static struct tegrabl_qspi_flash_driver_info *qspi_flash_driver_info[QSPI_MAX_INSTANCE];

static tegrabl_error_t qspi_bdev_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count);
static tegrabl_error_t qspi_bdev_ioctl(
		struct tegrabl_bdev *dev, uint32_t ioctl, void *argp);
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
static tegrabl_error_t qspi_bdev_erase(tegrabl_bdev_t *dev, bnum_t block,
	bnum_t count, bool is_secure);
static tegrabl_error_t qspi_bdev_write_block(tegrabl_bdev_t *dev,
		const void *buf, bnum_t block, bnum_t count);
static tegrabl_error_t tegrabl_qspi_flash_read(struct tegrabl_qspi_flash_driver_info *hqfdi,
		uint32_t start_page_num, uint32_t num_of_pages, uint8_t *p_dest, bool async);
static tegrabl_error_t tegrabl_qspi_blockdev_xfer(struct tegrabl_blockdev_xfer_info *xfer);
static tegrabl_error_t tegrabl_qspi_blockdev_xfer_wait(struct tegrabl_blockdev_xfer_info *xfer,
						time_t timeout, uint8_t *status_flag);
static tegrabl_error_t qspi_qpi_flag_set(struct tegrabl_qspi_flash_driver_info *hqfdi, bool bset);

#endif

static tegrabl_error_t qspi_quad_flag_set(
		struct tegrabl_qspi_flash_driver_info *hqfdi, uint8_t bset);
static tegrabl_error_t read_device_id_info(
		struct tegrabl_qspi_flash_driver_info *hqfdi);

static tegrabl_error_t qspi_bdev_ioctl(
		struct tegrabl_bdev *dev, uint32_t ioctl, void *argp)
{
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(argp);

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	if (ioctl == TEGRABL_IOCTL_DEVICE_CACHE_FLUSH) {
		return TEGRABL_NO_ERROR;
	}
#endif
	pr_debug("Unknown ioctl %"PRIu32"\n", ioctl);
	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, AUX_INFO_IOCTL_NOT_SUPPORTED);
}

static void qspi_init_platform_params(
				struct tegrabl_qspi_flash_platform_params *params,
				struct tegrabl_qspi_platform_params *qparam)
{
	qparam->clk_src = params->clk_src;
	qparam->clk_src_freq = params->clk_src_freq;
	qparam->interface_freq = params->interface_freq;
	qparam->clk_div = params->clk_div;
	qparam->max_bus_width = params->max_bus_width;
	qparam->dma_type = params->dma_type;
	qparam->fifo_access_mode = params->fifo_access_mode;
	qparam->trimmer1_val = params->trimmer1_val;
	qparam->trimmer2_val = params->trimmer2_val;
}

static void qspi_init_device(struct tegrabl_qspi_flash_platform_params *params,
							 struct tegrabl_qspi_device_property *qdevice)
{
	qdevice->spi_mode = QSPI_SIGNAL_MODE_0;
	qdevice->cs_active_low = true;
	qdevice->chip_select = 0;
	qdevice->speed_hz = params->interface_freq;
}

tegrabl_error_t tegrabl_qspi_flash_open(uint32_t instance,
				struct tegrabl_qspi_flash_platform_params *params)
{
	static bool is_init_done[QSPI_MAX_INSTANCE];
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_bdev_t *qspi_dev = NULL;
	struct tegrabl_qspi_platform_params qparams;
	struct tegrabl_qspi_device_property qdevice;
	struct tegrabl_qspi_handle *hqspi = NULL;
	struct tegrabl_qspi_flash_driver_info *hqfdi;

	if (params == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS);
	}

	pr_trace("clk_src = %d\n", params->clk_src);
	pr_trace("clk_div = %d\n", params->clk_div);
	pr_trace("clk_src_freq = %d\n", params->clk_src_freq);
	pr_trace("interface_freq= %d\n", params->interface_freq);
	pr_trace("max_bus_width = %d\n", params->max_bus_width);
	pr_trace("enable_ddr_read = %d\n", params->enable_ddr_read);
	pr_trace("dma_type = %d\n", params->dma_type);
	pr_trace("fifo_access_mode= %d\n", params->fifo_access_mode);
	pr_trace("read_dummy_cycles = %d\n", params->read_dummy_cycles);
	pr_trace("trimmer1_val = %d\n", params->trimmer1_val);
	pr_trace("trimmer2_val = %d\n", params->trimmer2_val);

	/* check if it's already initialized */
	if (is_init_done[instance]) {
		return TEGRABL_NO_ERROR;
	}
	hqfdi = tegrabl_calloc(1, sizeof(*hqfdi));
	if (hqfdi == NULL) {
		pr_error("Failed to allocate memory for qspi_flash_driver_info\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, AUX_INFO_NO_MEMORY_1);
	}

	hqfdi->transfers = tegrabl_calloc(QSPI_TRANSFERS, sizeof(struct tegrabl_qspi_transfer));
	if (hqfdi == NULL) {
		pr_error("Failed to allocate memory for qspi transfers\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, AUX_INFO_NO_MEMORY_2);
	}

	hqfdi->address_data = tegrabl_calloc(QSPI_ADDR_LENGTH, sizeof(uint8_t));
	if (hqfdi == NULL) {
		pr_error("Failed to allocate memory for qspi address\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, AUX_INFO_NO_MEMORY_3);
	}

	hqfdi->cmd = tegrabl_calloc(1, sizeof(uint8_t));
	if (hqfdi == NULL) {
		pr_error("Failed to allocate memory for qspi cmd\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, AUX_INFO_NO_MEMORY_4);
	}
	/* initialize qspi hardware */
	memcpy(&hqfdi->plat_params, params, sizeof(*params));
	qspi_init_platform_params(&hqfdi->plat_params, &qparams);
	qspi_init_device(&hqfdi->plat_params, &qdevice);

	err = tegrabl_qspi_open(instance, &qdevice, &qparams, &hqspi);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to open QSPI Instance%d: 0x%08x\n", instance, err);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED,
							AUX_INFO_NOT_INITIALIZED);
		goto qflash_free;
	}

	hqfdi->hqspi = hqspi;

	/* allocate memory for qspi handle. */
	qspi_dev = tegrabl_calloc(1, sizeof(tegrabl_bdev_t));
	if (qspi_dev == NULL) {
		pr_error("Qspi calloc failed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, AUX_INFO_NO_MEMORY_1);
		goto qflash_free;
	}

	/* read device identification info */
	err = read_device_id_info(hqfdi);
	if (err != TEGRABL_NO_ERROR) {
		goto init_cleanup;
	}

	err = tegrabl_blockdev_initialize_bdev(qspi_dev,
					(((uint32_t)TEGRABL_STORAGE_QSPI_FLASH << 16) | instance),
					hqfdi->chip_info.block_size_log2,
					hqfdi->chip_info.block_count);
	if (err != TEGRABL_NO_ERROR) {
		goto init_cleanup;
	}

	qspi_dev->buf_align_size = TEGRABL_QSPI_BUF_ALIGN_SIZE;

	/* Fill bdev function pointers. */
	qspi_dev->read_block = qspi_bdev_read_block;
	qspi_dev->ioctl = qspi_bdev_ioctl;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	qspi_dev->write_block = qspi_bdev_write_block;
	qspi_dev->erase = qspi_bdev_erase;
	qspi_dev->xfer = tegrabl_qspi_blockdev_xfer;
	qspi_dev->xfer_wait = tegrabl_qspi_blockdev_xfer_wait;
#endif
	qspi_dev->priv_data = hqfdi;

	/*  Make sure flash is in X4 mode by default*/
	err = qspi_quad_flag_set(hqfdi, 1);
	if (err != TEGRABL_NO_ERROR) {
		goto init_cleanup;
	}

	/* Register qspi boot device. */
	err = tegrabl_blockdev_register_device(qspi_dev);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Qspi block dev registration failed (err:0x%x)\n", err);
		goto init_cleanup;
	}

	pr_info("Qspi initialized successfully\n");
	is_init_done[instance] = true;
	qspi_flash_driver_info[instance] = hqfdi;
	return TEGRABL_NO_ERROR;

init_cleanup:
	tegrabl_free(qspi_dev);

qflash_free:
	tegrabl_free(hqfdi);

	return err;
}

static tegrabl_error_t read_device_id_info(
					struct tegrabl_qspi_flash_driver_info *hqfdi)
{
	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t device_flag;
	uint8_t buf_id[DEVICE_ID_LEN];
	uint32_t i, device_cnt;

	struct tegrabl_qspi_transfer *transfers;
	uint8_t command;

	transfers = hqfdi->transfers;
	memset(transfers, 0, 2U*(sizeof(struct tegrabl_qspi_transfer)));

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

	err = tegrabl_qspi_transaction(hqfdi->hqspi, transfers, 2,
								   QSPI_XFER_TIMEOUT);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Read-device-ID: register (0x%x) read fail (err:0x%x)\n", command, err);
		return err;
	}

	device_cnt = sizeof(device_info_list)/sizeof(struct device_info);
	for (i = 0; i < device_cnt; i++) {
		if (buf_id[0] == device_info_list[i].manufacture_id &&
		buf_id[1] == device_info_list[i].memory_type &&
		buf_id[2] == device_info_list[i].density) {
			pr_trace("QSPI Flash: %s\n", device_info_list[i].name);
			break;
		}
	}

	if (i < device_cnt) {
		chip_info->device_list_index = i;
		chip_info->flash_size_log2 = buf_id[2];

		if (MANUFACTURE_ID_MACRONIX == buf_id[0]) {
			err = qspi_flash_get_size_macronix(hqfdi);

			if (err != TEGRABL_NO_ERROR) {
				return err;
			}
		}
	} else {
		pr_info("No supported QSPI flash found\n");
		chip_info->flash_size_log2 = 0;
	}

	/* Treat < 16MBytes density as an error */
	if (chip_info->flash_size_log2 < FLASH_SIZE_16MB_LOG2) {
		pr_error("QSPI Flash: Insufficient flash size (%lu MB)\n",
				 1UL << (chip_info->flash_size_log2 - FLASH_SIZE_1MB_LOG2));
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID,
					AUX_INFO_INVALID_PARAMS7);
	} else if (chip_info->flash_size_log2 >= 0x20UL) {
		/* Adjust for devices with bloated flash densities */
		chip_info->flash_size_log2 -= 6UL;
	}

	pr_trace("Memory Interface Type = %x\n", buf_id[1]);
	pr_info("QSPI Flash Size = %lu MB\n", 1UL << (chip_info->flash_size_log2 - FLASH_SIZE_1MB_LOG2));

	device_flag = device_info_list[chip_info->device_list_index].flag;

	/* Sector Size */
	chip_info->sector_size_log2 = device_info_list[i].sector_size;
	chip_info->sector_count = 1UL << (chip_info->flash_size_log2 -
						chip_info->sector_size_log2);
	/* Subsector (parameter sector) size         */
	/* On some Spansion devices, there're        */
	/* 8x4K parameter sectors on top or bottom   */
	/* It's different with "subsector" on Micron */
	/* We ignore "subsector" now                 */
	chip_info->parameter_sector_size_log2 =
				device_info_list[i].parameter_sector_size;
	chip_info->parameter_sector_count =
				device_info_list[i].parameter_sector_cnt;

	/* if size is > 16 MB (2 ^ 24), we need to address with 4 bytes */
	if (chip_info->flash_size_log2 > 24UL) {
		chip_info->address_length = 4;
	} else {
		chip_info->address_length = 3;
	}

	if ((device_flag & (uint8_t) FLAG_DDR) != 0U) {
		chip_info->qddr_read = hqfdi->plat_params.enable_ddr_read;
	} else {
		chip_info->qddr_read = false;
	}

	/* 2 ^ 9 = 512 Bytes */
	chip_info->block_size_log2 = 9;

	chip_info->block_count = 1UL << (chip_info->flash_size_log2 -
					chip_info->block_size_log2);

	chip_info->page_write_size = 256;
	chip_info->qpi_bus_width = QSPI_BUS_WIDTH_X1;

	/* This translates to "if defined(ENABLE)". But to avoid unnecessary changes in t186,
	 * macro is used as DISABLE
	 */
#if !defined(CONFIG_DISABLE_QSPI_FLASH_WRITE_512B_PAGE)
	if ((device_flag & (uint8_t) FLAG_PAGE512) != 0U) {
		/* 512 bytes page is only supported by spansion */
		err = qspi_flash_page_512bytes_enable_spansion(hqfdi);
	}
#endif

	return err;
}

tegrabl_error_t tegrabl_qspi_flash_reinit(uint32_t instance,
					struct tegrabl_qspi_flash_platform_params *params)
{
	static bool is_reinit_done;
	uint8_t device_flag;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_qspi_platform_params qparams;
	struct tegrabl_qspi_device_property qdevice;
	struct tegrabl_qspi_handle *hqspi = NULL;
	struct tegrabl_qspi_flash_chip_info *chip_info;

	if (params == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS1);
	}

	if (is_reinit_done) {
		return TEGRABL_NO_ERROR;
	}

	hqspi = qspi_flash_driver_info[instance]->hqspi;
	qspi_flash_driver_info[instance]->hqspi =  NULL;

	err = tegrabl_qspi_close(hqspi);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	/* initialize new params */
	memcpy(&qspi_flash_driver_info[instance]->plat_params, params, sizeof(*params));
	qspi_init_platform_params(&qspi_flash_driver_info[instance]->plat_params, &qparams);
	qspi_init_device(&qspi_flash_driver_info[instance]->plat_params, &qdevice);

	err = tegrabl_qspi_open(instance, &qdevice, &qparams, &hqspi);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	is_reinit_done = true;
	qspi_flash_driver_info[instance]->hqspi = hqspi;
	chip_info = &qspi_flash_driver_info[instance]->chip_info;
	device_flag = device_info_list[chip_info->device_list_index].flag;

	if ((device_flag & (uint8_t) FLAG_DDR) != 0U) {
		chip_info->qddr_read = params->enable_ddr_read;
	} else {
		chip_info->qddr_read = false;
	}

	/* re-init with mb1bct dev-params */
	pr_info("Qspi reinitialized\n");
	return err;
}

tegrabl_error_t qspi_read_reg(
		struct tegrabl_qspi_flash_driver_info *hqfdi,
		uint32_t reg_access_cmd,
		uint8_t *p_reg_val)
{
	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	uint8_t cmd_addr_buf[8];
	uint8_t command;
	bool is_ext_reg_access_cmd;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_qspi_transfer *transfers;

	is_ext_reg_access_cmd = ((reg_access_cmd & (uint32_t) 0xFFFF00) == 0U) ?
					false : true;

	transfers = hqfdi->transfers;
	memset(transfers, 0, QSPI_TRANSFERS*(sizeof(struct tegrabl_qspi_transfer)));
	cmd_addr_buf[0] = (uint8_t) ((reg_access_cmd >> 16) & (uint32_t) 0xFF);
	cmd_addr_buf[1] = (uint8_t) ((reg_access_cmd >> 8) & (uint32_t) 0xFF);
	cmd_addr_buf[2] = (uint8_t) reg_access_cmd;
	command = QSPI_FLASH_CMD_RDAR;

	if (is_ext_reg_access_cmd) {
		transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[0].bus_width = chip_info->qpi_bus_width;
		transfers[0].dummy_cycles = ZERO_CYCLES;
		transfers[0].op_mode = SDR_MODE;
		transfers[0].tx_buf = &command;
		transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
	} else {
		command = (uint8_t) reg_access_cmd;
	}

	if (is_ext_reg_access_cmd) {
		transfers[1].tx_buf = cmd_addr_buf;
		transfers[1].dummy_cycles = 0;
		/* Extra dummy cycles needed in QPI mode */
		if (chip_info->qpi_bus_width == QSPI_BUS_WIDTH_X4) {
			transfers[1].write_len = 3 + 4;
		} else {
			transfers[1].write_len = 3 + 1;
		}
	} else {
		transfers[1].tx_buf = &command;
		transfers[1].write_len = QSPI_FLASH_COMMAND_WIDTH;
		transfers[1].dummy_cycles = ZERO_CYCLES;
	}
	transfers[1].mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers[1].bus_width = chip_info->qpi_bus_width;
	transfers[1].op_mode = SDR_MODE;

	transfers[2].rx_buf = p_reg_val;
	transfers[2].read_len = QSPI_FLASH_COMMAND_WIDTH;
	transfers[2].mode = QSPI_FLASH_CMD_MODE_VAL;
	transfers[2].bus_width = chip_info->qpi_bus_width;
	transfers[2].dummy_cycles = ZERO_CYCLES;
	transfers[2].op_mode = SDR_MODE;

	if (is_ext_reg_access_cmd) {
		/* Extended command */
		err = tegrabl_qspi_transaction(hqfdi->hqspi, transfers, 3,
									   QSPI_XFER_TIMEOUT);
	} else {
		/* 1-byte command */
		err = tegrabl_qspi_transaction(hqfdi->hqspi, &transfers[1], 2,
									   QSPI_XFER_TIMEOUT);
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_error("Qspi read: register (0x%x) read fail (err:0x%x)\n",
				 reg_access_cmd, err);
	}
	return err;
}

tegrabl_error_t qspi_write_reg(
		struct tegrabl_qspi_flash_driver_info *hqfdi,
		uint32_t reg_access_cmd,
		uint8_t *p_reg_val)
{
	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	uint8_t cmd_addr_buf[3];
	uint8_t command;
	bool is_ext_reg_access_cmd;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_qspi_transfer *transfers;

	is_ext_reg_access_cmd = ((reg_access_cmd & (uint32_t) 0xFFFF00) == 0U) ?
					false : true;

	transfers = hqfdi->transfers;
	err = qspi_write_en(hqfdi, true);
	if (err == TEGRABL_NO_ERROR) {
		memset(transfers, 0, QSPI_TRANSFERS*(sizeof(struct tegrabl_qspi_transfer)));
		cmd_addr_buf[0] = (uint8_t) ((reg_access_cmd >> 16) & (uint32_t) 0xFF);
		cmd_addr_buf[1] = (uint8_t) ((reg_access_cmd >> 8) & (uint32_t) 0xFF);
		cmd_addr_buf[2] = (uint8_t) reg_access_cmd;
		command = QSPI_FLASH_CMD_WRAR;

		if (is_ext_reg_access_cmd) {
			transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
			transfers[0].bus_width = chip_info->qpi_bus_width;
			transfers[0].dummy_cycles = ZERO_CYCLES;
			transfers[0].op_mode = SDR_MODE;
			transfers[0].tx_buf = &command;
			transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
		} else {
			command = (uint8_t) reg_access_cmd;
		}

		if (is_ext_reg_access_cmd) {
			transfers[1].tx_buf = cmd_addr_buf;
			transfers[1].write_len = 3;
		} else {
			transfers[1].tx_buf = &command;
			transfers[1].write_len = QSPI_FLASH_COMMAND_WIDTH;
		}
		transfers[1].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[1].bus_width = chip_info->qpi_bus_width;
		transfers[1].dummy_cycles = ZERO_CYCLES;
		transfers[1].op_mode = SDR_MODE;

		transfers[2].tx_buf = p_reg_val;
		transfers[2].write_len = QSPI_FLASH_COMMAND_WIDTH;
		transfers[2].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[2].bus_width = chip_info->qpi_bus_width;
		transfers[2].dummy_cycles = ZERO_CYCLES;
		transfers[2].op_mode = SDR_MODE;

		if (is_ext_reg_access_cmd) {
			/* Extended command */
			err = tegrabl_qspi_transaction(hqfdi->hqspi, transfers, 3,
										   QSPI_XFER_TIMEOUT);
		} else {
			/* 1-byte command */
			err = tegrabl_qspi_transaction(hqfdi->hqspi, &transfers[1], 2,
										   QSPI_XFER_TIMEOUT);
		}
	} else {
		pr_error("Qspi write: register (0x%x) write fail (err:0x%x)\n",
				 reg_access_cmd, err);
	}

	return err;
}

tegrabl_error_t qspi_writein_progress(struct tegrabl_qspi_flash_driver_info *hqfdi, bool benable,
										bool is_mdelay)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint16_t tried = 0;
	uint8_t reg_val;
	uint8_t comp;

	pr_trace("Waiting for WIP %s ...\n", benable ? "Enable" : "Disable");

	do {
		if (tried == (uint16_t) QSPI_FLASH_WIP_RETRY_COUNT) {
			pr_trace("Timeout for WIP %s\n", benable ? "Enable" : "Disable");
			err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_WIP_TIMEOUT);
			break;
		}

		if (benable) {
			comp = (uint8_t) QSPI_FLASH_WIP_ENABLE;
		} else {
			comp = (uint8_t) QSPI_FLASH_WIP_DISABLE;
		}

		err = qspi_read_reg(hqfdi, QSPI_FLASH_CMD_RDSR1, &reg_val);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Read RDSR1 cmd fail (err:0x%x)\n", err);
			break;
		}

		if (is_mdelay) {
			tegrabl_mdelay(QSPI_FLASH_WIP_DISABLE_WAIT_TIME);
		} else {
			tegrabl_udelay(QSPI_FLASH_WIP_DISABLE_WAIT_TIME);
		}

		pr_trace("Try::%u regval:(0x%x)\n", tried, reg_val);

		tried++;
	} while ((reg_val & (uint8_t) QSPI_FLASH_WIP_FIELD) != comp);

	if (err == TEGRABL_NO_ERROR) {
		pr_trace("WIP %s is done\n", benable ? "enable" : "disable");
	}

	return err;
}

tegrabl_error_t qspi_write_en(
					struct tegrabl_qspi_flash_driver_info *hqfdi, bool benable)
{
	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	struct tegrabl_qspi_transfer *transfers;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t tried = 0;
	uint8_t command;
	uint8_t reg_val;
	uint32_t comp;

	pr_trace("Doing WEN %s\n", benable ? "enable" : "disable");

	transfers = hqfdi->transfers;
	memset(transfers, 0, sizeof(struct tegrabl_qspi_transfer));
	do {
		if (tried == QSPI_FLASH_WE_RETRY_COUNT) {
			pr_error("QSPI-WriteEN: timeout for WEN %s\n",
					 benable ? "enable" : "disable");
			return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_WEN_TIMEOUT);
		}

		if (benable) {
			command = QSPI_FLASH_CMD_WREN;
			comp = QSPI_FLASH_WEL_ENABLE;
		} else {
			command = QSPI_FLASH_CMD_WRDI;
			comp = QSPI_FLASH_WEL_DISABLE;
		}

		transfers[0].tx_buf = &command;
		transfers[0].rx_buf = NULL;
		transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
		transfers[0].read_len = 0;
		transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[0].bus_width = chip_info->qpi_bus_width;
		transfers[0].dummy_cycles = ZERO_CYCLES;
		transfers[0].op_mode = SDR_MODE;

		err = tegrabl_qspi_transaction(hqfdi->hqspi, &transfers[0], 1,
									   QSPI_XFER_TIMEOUT);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("QSPI-WriteEN: WEN %s fail (err:0x%x)\n",
					 benable ? "enable" : "disable", err);
			return err;
		}

		tegrabl_udelay(QSPI_FLASH_WRITE_ENABLE_WAIT_TIME);
		err = qspi_read_reg(hqfdi, QSPI_FLASH_CMD_RDSR1, &reg_val);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("QSPI-WriteEN: read RDSR1 cmd fail (err:0x%x)\n", err);
			return err;
		}

		tried++;
	} while ((reg_val & QSPI_FLASH_WEL_ENABLE) != comp);

	pr_trace("WEN %s is done\n", benable ? "enable" : "disable");

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_quad_flag_set(
				struct tegrabl_qspi_flash_driver_info *hqfdi, uint8_t bset)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (hqfdi->plat_params.max_bus_width != QSPI_BUS_WIDTH_X4) {
		return TEGRABL_NO_ERROR;
	}

	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	uint32_t device_list_index = chip_info->device_list_index;
	switch (device_info_list[device_list_index].manufacture_id) {
	case MANUFACTURE_ID_MICRON:
	case MANUFACTURE_ID_MACRONIX:
		break;
	case MANUFACTURE_ID_SPANSION:
		pr_trace("%s: %s QUAD flag\n", __func__, bset ? "setting" : "clearing");

		err = qspi_flash_x4_enable_spansion(hqfdi, bset);
	}

	return err;
}

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)

static tegrabl_error_t qspi_qpi_flag_set(struct tegrabl_qspi_flash_driver_info *hqfdi, bool bset)
{
	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	static bool bqpiset;
	uint32_t device_list_index = chip_info->device_list_index;

	uint8_t device_flag = device_info_list[device_list_index].flag;

	if ((hqfdi->plat_params.max_bus_width != QSPI_BUS_WIDTH_X4) ||
				((device_flag & (uint8_t) FLAG_QPI) == 0U)) {
		chip_info->qpi_bus_width = (uint32_t) QSPI_BUS_WIDTH_X1;
		return TEGRABL_NO_ERROR;
	}

	if (bqpiset == bset) {
		pr_trace("QPI flag is already %s\n", bset ? "set" : "clear");
		goto exit;
	}

	pr_trace("%s QPI flag\n", bset ? "setting" : "clearing");

	/* QPI enable register and bit polarity are different */
	/* for different vendors and their devices */
	switch (device_info_list[device_list_index].manufacture_id) {
	case MANUFACTURE_ID_MICRON:
		err = qspi_flash_qpi_mode_enable_micron(hqfdi, bset);
		break;

	case MANUFACTURE_ID_MACRONIX:
		err = qspi_flash_qpi_mode_enable_macronix(hqfdi, bset);
		break;

	case MANUFACTURE_ID_SPANSION:
		err = qspi_flash_qpi_mode_enable_spansion(hqfdi, bset);
		break;
	}

	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	pr_trace("QPI flag is %s\n", bset ? "set" : "cleared");

exit:
	if (bset) {
		chip_info->qpi_bus_width = QSPI_BUS_WIDTH_X4;
	} else {
		chip_info->qpi_bus_width = QSPI_BUS_WIDTH_X1;
	}
	if (bqpiset != bset) {
		bqpiset = bset;
		qspi_writein_progress(hqfdi, QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_US);
	}

	return TEGRABL_NO_ERROR;
}

#define block_num_to_sector_num(blk)		\
		DIV_FLOOR_LOG2(((blk) << chip_info->block_size_log2), \
					   chip_info->sector_size_log2)

#define block_cnt_to_sector_cnt(cnt)		\
		DIV_CEIL_LOG2(((cnt) << chip_info->block_size_log2), \
					  chip_info->sector_size_log2)

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
		struct tegrabl_qspi_flash_driver_info *hqfdi,
		uint32_t start_parameter_sector_num,
		uint32_t num_of_parameter_sectors,
		uint8_t is_top)
{
	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	struct tegrabl_qspi_transfer *transfers;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t address_data[4];
	uint32_t num_of_sectors_to_erase = num_of_parameter_sectors;
	uint32_t address;
	uint8_t *cmd = hqfdi->cmd;

	transfers = hqfdi->transfers;
	memset(transfers, 0, 2U*(sizeof(struct tegrabl_qspi_transfer)));
	if (chip_info->address_length == 4UL) {
		*cmd = QSPI_FLASH_CMD_4PARA_SECTOR_ERASE;
	} else {
		*cmd = QSPI_FLASH_CMD_PARA_SECTOR_ERASE;
	}

	if (start_parameter_sector_num > chip_info->parameter_sector_count) {
		pr_error("Qspi param sector erase: Incorrect sector number: %u\n", start_parameter_sector_num);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS2);
	}

	if ((num_of_parameter_sectors == 0UL) ||
		(num_of_parameter_sectors > chip_info->parameter_sector_count)) {
		pr_error("Qspi param sector erase: Incorrect number of sectors: %u\n", num_of_parameter_sectors);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS3);
	}

	if ((start_parameter_sector_num + num_of_parameter_sectors) >
			chip_info->parameter_sector_count) {
		pr_error("Qspi param sector erase: Exceed total sector count\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS4);
	}

	/*  Make sure flash is in X1 mode */
	err = qspi_quad_flag_set(hqfdi, 0);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	if (is_top != 0U) {
		address = ((1UL << chip_info->flash_size_log2) -
			(chip_info->parameter_sector_count <<
			chip_info->parameter_sector_size_log2));
	} else {
		address = start_parameter_sector_num <<
			 chip_info->parameter_sector_size_log2;
	}

	while (num_of_sectors_to_erase != 0UL) {
		pr_trace("Erasing sub sector of addr: 0x%x\n", address);
		/*  Enable Write */
		err = qspi_write_en(hqfdi, true);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		/*  address are sent to device with MSB first */
		if (chip_info->address_length == 4UL) {
			address_data[0] = (uint8_t)(uint32_t)((address >> 24) & 0xFFU);
			address_data[1] = (uint8_t)(uint32_t)((address >> 16) & 0xFFU);
			address_data[2] = (uint8_t)(uint32_t)((address >> 8) & 0xFFU);
			address_data[3] = (uint8_t)(uint32_t)((address) & 0xFFU);
		} else {
			address_data[0] = (uint8_t)(uint32_t)((address >> 16) & 0xFFU);
			address_data[1] = (uint8_t)(uint32_t)((address >> 8) & 0xFFU);
			address_data[2] = (uint8_t)(uint32_t)((address) & 0xFFU);
		}

		/*  Set command Parameters in First Transfer */
		/*  command must be in SDR X1 mode = 0x0 */
		/*  Set Read length is 0 for command */
		transfers[0].tx_buf = cmd;
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
		transfers[1].write_len = chip_info->address_length;
		transfers[1].read_len = 0;
		transfers[1].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;
		transfers[1].bus_width = QSPI_BUS_WIDTH_X1;
		transfers[1].dummy_cycles = ZERO_CYCLES;
		transfers[1].op_mode = SDR_MODE;

		err = tegrabl_qspi_transaction(hqfdi->hqspi, &transfers[0], 2,
									   QSPI_XFER_TIMEOUT);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Qspi param sector erase: Sub sector erase failed(err:0x%x)\n", err);
			pr_trace("addr = 0x%x\n", address);
			/*  Disable Write En bit */
			err = qspi_write_en(hqfdi, false);
			if (err != TEGRABL_NO_ERROR) {
				return err;
			}
			break;
		}

		qspi_writein_progress(hqfdi, QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_US);

		/*  Disable Write En bit */
		err = qspi_write_en(hqfdi, false);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		address += (1UL << chip_info->parameter_sector_size_log2);
		num_of_sectors_to_erase--;
	}
	/* Put back in X4 mode */
	err = qspi_quad_flag_set(hqfdi, 1);
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
		struct tegrabl_qspi_flash_driver_info *hqfdi,
		uint32_t start_sector_num,
		uint32_t num_of_sectors)
{
	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	struct tegrabl_qspi_transfer *transfers;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t address_data[4];
	uint32_t num_of_sectors_to_erase = num_of_sectors;
	uint32_t address;
	uint8_t *cmd = hqfdi->cmd;

	transfers = hqfdi->transfers;
	memset(transfers, 0, 2U*(sizeof(struct tegrabl_qspi_transfer)));
	if (chip_info->address_length == 4U) {
		*cmd = QSPI_FLASH_CMD_4SECTOR_ERASE;
	} else {
		*cmd = QSPI_FLASH_CMD_SECTOR_ERASE;
	}
	if (num_of_sectors == 0UL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS5);
	}

	TEGRABL_ASSERT((start_sector_num + num_of_sectors_to_erase) <=
		chip_info->sector_count);

	/* Make sure flash is in X1 mode */
	err = qspi_quad_flag_set(hqfdi, 0);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	address = start_sector_num << chip_info->sector_size_log2;
	while (num_of_sectors_to_erase != 0UL) {
		pr_trace("Erase sector num: %u\n",
				 num_of_sectors - num_of_sectors_to_erase + start_sector_num);
		/* Enable Write */
		err = qspi_write_en(hqfdi, true);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		/* address are sent to device with MSB first */
		if (chip_info->address_length == 4UL) {
			address_data[0] = (uint8_t)(uint32_t)((address >> 24) & 0xFFU);
			address_data[1] = (uint8_t)(uint32_t)((address >> 16) & 0xFFU);
			address_data[2] = (uint8_t)(uint32_t)((address >> 8) & 0xFFU);
			address_data[3] = (uint8_t)(uint32_t)((address) & 0xFFU);
		} else {
			address_data[0] = (uint8_t)(uint32_t)((address >> 16) & 0xFFU);
			address_data[1] = (uint8_t)(uint32_t)((address >> 8) & 0xFFU);
			address_data[2] = (uint8_t)(uint32_t)((address) & 0xFFU);
		}

		/* Set command Parameters in First Transfer */
		/* command must be in SDR X1 mode = 0x0 */
		/* Set Read length is 0 for command */
		transfers[0].tx_buf = cmd;
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
		transfers[1].write_len = chip_info->address_length;
		transfers[1].read_len = 0;
		transfers[1].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;
		transfers[1].bus_width = QSPI_BUS_WIDTH_X1;
		transfers[1].dummy_cycles = ZERO_CYCLES;
		transfers[1].op_mode = SDR_MODE;


		err = tegrabl_qspi_transaction(hqfdi->hqspi, &transfers[0], 2,
									   QSPI_XFER_TIMEOUT);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Qspi sector erase: Sector erase failed (err:0x%x)\n", err);
			pr_trace("addr:0x%x\n", address);
			/* Disable Write En bit */
			err = qspi_write_en(hqfdi, false);
			if (err != TEGRABL_NO_ERROR) {
				return err;
			}

			break;
		}

		qspi_writein_progress(hqfdi, QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_US);

		/* Disable Write En bit */
		err = qspi_write_en(hqfdi, false);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		address += (1UL << chip_info->sector_size_log2);
		num_of_sectors_to_erase--;
	}

	/* Put back in X4 mode */
	err = qspi_quad_flag_set(hqfdi, 1);
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
tegrabl_qspi_flash_bulk_erase(struct tegrabl_qspi_flash_driver_info *hqfdi)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_qspi_transfer *transfers;
	uint8_t *cmd = hqfdi->cmd;

	/* Enable Write */
	err = qspi_write_en(hqfdi, true);

	if (err == TEGRABL_NO_ERROR) {
		transfers = hqfdi->transfers;
		memset(transfers, 0, sizeof(struct tegrabl_qspi_transfer));

		*cmd = QSPI_FLASH_CMD_BULK_ERASE;
		transfers[0].tx_buf = cmd;
		transfers[0].write_len = QSPI_FLASH_COMMAND_WIDTH;
		transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[0].bus_width = QSPI_BUS_WIDTH_X1;
		transfers[0].dummy_cycles = ZERO_CYCLES;
		transfers[0].op_mode = SDR_MODE;

		err = tegrabl_qspi_transaction(hqfdi->hqspi, &transfers[0], 1,
									   QSPI_XFER_TIMEOUT);
		if (err == TEGRABL_NO_ERROR) {
			/* Wait in mdelays */
			err = qspi_writein_progress(hqfdi,
					QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_MS);
		}
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_error("Qspi bulk erase: Bulk erase fail (err:0x%x)\n", err);
	}

	return err;
}

static tegrabl_error_t qspi_bdev_erase(tegrabl_bdev_t *dev, bnum_t block,
	bnum_t count, bool is_secure)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_qspi_flash_driver_info *hqfdi;
	struct tegrabl_qspi_flash_chip_info *chip_info;
	uint32_t sector_num = 0;
	uint32_t sector_cnt = 0;
	uint8_t *head_backup = NULL;
	uint8_t *tail_backup = NULL;
	bnum_t head_start, head_count;
	bnum_t tail_start, tail_count;
	uint8_t device_info_flag;

	TEGRABL_UNUSED(is_secure);

	if ((dev == NULL) || (count == 0U) || (dev->priv_data == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS6);
	}

	hqfdi = dev->priv_data;
	chip_info = &hqfdi->chip_info;
	device_info_flag = device_info_list[chip_info->device_list_index].flag;

	if ((block == 0UL) && (count == chip_info->block_count) &&
					((device_info_flag & FLAG_BULK) != 0U)) {
		/* Erase whole device and return */
		pr_info("QSPI: Erasing entire device\n");
		error = tegrabl_qspi_flash_bulk_erase(hqfdi);
		return error;
	}

	head_count = tail_count = 0;
	head_start = tail_start = 0;
	sector_num = block_num_to_sector_num(block);
	sector_cnt = block_cnt_to_sector_cnt(count);

	pr_trace("QSPI: erasing sectors from %u - %u\n", sector_num, (sector_cnt - 1) + sector_num);

	if (count != chip_info->block_count) {
		/* Need to preserve the partitions that are overlapping */
		/* on the sector which needs to be erased.              */
		/* head : former overlapping partition                  */
		/* tail : latter overlapping partition                  */
#define PAGES_IN_SECTOR_LOG2 \
	(chip_info->sector_size_log2 - chip_info->block_size_log2)
		head_start = sector_num << PAGES_IN_SECTOR_LOG2;
		head_count = block & ((1UL << PAGES_IN_SECTOR_LOG2) - 1UL);
		tail_start = block + count;
		sector_cnt = tail_start + ((1UL << PAGES_IN_SECTOR_LOG2) - 1UL);
		sector_cnt = block_num_to_sector_num(sector_cnt);
		tail_count = (sector_cnt << PAGES_IN_SECTOR_LOG2) - tail_start;
		sector_cnt -= sector_num;
#undef PAGES_IN_SECTOR_LOG2

		pr_info("QSPI: erasing sectors from %u - %u\n",
				sector_num, sector_cnt - 1U + sector_num);

		if (head_count != 0UL) {
			pr_info("QSPI: recoverying head blocks from %u - %u\n",
					head_start, head_start + head_count);
			head_backup = tegrabl_calloc(1,
					head_count << chip_info->block_size_log2);
			if (head_backup == NULL) {
				error = TEGRABL_ERROR(
					TEGRABL_ERR_NO_MEMORY, AUX_INFO_NO_MEMORY_1);
				goto fail;
			}
			error = qspi_bdev_read_block(dev,
					(void *)head_backup, head_start, head_count);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}
		if (tail_count != 0UL) {
			pr_info("QSPI: recoverying tail blocks from %u - %u\n",
					tail_start, tail_start + tail_count);
			tail_backup = tegrabl_calloc(1,
					tail_count << chip_info->block_size_log2);
			if (tail_backup == NULL) {
				error = TEGRABL_ERROR(
					TEGRABL_ERR_NO_MEMORY, AUX_INFO_NO_MEMORY_2);
				goto fail;
			}
			error = qspi_bdev_read_block(dev,
					(void *)tail_backup, tail_start, tail_count);
			if (error != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}
	}

	/* handling sector-0 erase differently */
	if (sector_num == 0UL) {
		tegrabl_qspi_flash_parameter_sector_erase(hqfdi, 0, 8, 0);
	}
	error = tegrabl_qspi_flash_sector_erase(hqfdi, sector_num,
											sector_cnt);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	if (head_count != 0UL) {
		error = qspi_bdev_write_block(dev,
			(void *)head_backup, head_start, head_count);
		if (error != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
	if (tail_count != 0UL) {
		error = qspi_bdev_write_block(dev,
			(void *)tail_backup, tail_start, tail_count);
	}
fail:
	if (head_backup != NULL) {
		tegrabl_free(head_backup);
	}
	if (tail_backup != NULL) {
		tegrabl_free(tail_backup);
	}

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
		struct tegrabl_qspi_flash_driver_info *hqfdi,
		uint32_t start_page_num,
		uint32_t num_of_pages,
		uint8_t *p_source_buffer)
{
	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	struct tegrabl_qspi_transfer *transfers;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t cmd_address_info[5];
	uint32_t bytes_to_write;
	uint32_t address;
	uint8_t *p_source = (uint8_t *)p_source_buffer;

	transfers = hqfdi->transfers;
	memset(transfers, 0, 2U*(sizeof(struct tegrabl_qspi_transfer)));

	/* Use combined command address buffer */
	if (chip_info->address_length == 4UL) {
		cmd_address_info[0] = QSPI_FLASH_CMD_4PAGE_PROGRAM;
	} else {
		cmd_address_info[0] = QSPI_FLASH_CMD_PAGE_PROGRAM;
	}
	if (num_of_pages == 0UL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_PARAMS7);
	}

	/* Setup QPI mode based on device info list */
	/* Switch to X1 if QPI setup fails */
	err = qspi_qpi_flag_set(hqfdi, true);
	if (err != TEGRABL_NO_ERROR) {
		chip_info->qpi_bus_width = QSPI_BUS_WIDTH_X1;
		pr_error("QPI setup failed err(:0x%x)\n", err);
		return err;
	}

	address = start_page_num << chip_info->block_size_log2;
	bytes_to_write = num_of_pages << chip_info->block_size_log2;

	pr_trace("Qspi flash write: strt_pg_num(%u) num_of_pgs(%u) write_buf(%p)\n",
			 start_page_num, num_of_pages, p_source_buffer);

	while (bytes_to_write != 0UL) {
		pr_trace("Sector write addr 0x%x\n", address);
		/* Enable Write */
		err = qspi_write_en(hqfdi, true);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}

		/* address are sent to device with MSB first */
		/* Command and address are combined to save transaction time */
		if (chip_info->address_length == 4UL) {
			cmd_address_info[1] = (uint8_t)(uint32_t)((address >> 24) & 0xFFU);
			cmd_address_info[2] = (uint8_t)(uint32_t)((address >> 16) & 0xFFU);
			cmd_address_info[3] = (uint8_t)(uint32_t)((address >> 8) & 0xFFU);
			cmd_address_info[4] = (uint8_t)(uint32_t)((address) & 0xFFU);
		} else {
			cmd_address_info[1] = (uint8_t)(uint32_t)((address >> 16) & 0xFFU);
			cmd_address_info[2] = (uint8_t)(uint32_t)((address >> 8) & 0xFFU);
			cmd_address_info[3] = (uint8_t)(uint32_t)((address) & 0xFFU);
		}

		/* Make sure the Dest is 4-byte aligned */
		if (((uintptr_t)p_source & 0x3U) != 0U) {
			return TEGRABL_ERROR(TEGRABL_ERR_BAD_ADDRESS, AUX_INFO_NOT_ALIGNED);
		}

		/* Set command and address Parameters in First Transfer */
		/* address width depends on whether QPI mode is enabled */
		/* Set Read length is 0 for address */

		transfers[0].tx_buf = cmd_address_info;
		transfers[0].rx_buf = NULL;
		transfers[0].write_len = chip_info->address_length + 1UL;
		transfers[0].read_len = 0;
		transfers[0].mode = QSPI_FLASH_CMD_MODE_VAL;
		transfers[0].bus_width = chip_info->qpi_bus_width;
		transfers[0].dummy_cycles = ZERO_CYCLES;
		transfers[0].op_mode = SDR_MODE;

		/* Set WriteData Parameters in Second Transfer */

		transfers[1].tx_buf = p_source;
		transfers[1].rx_buf = NULL;
		transfers[1].write_len =
			MIN(bytes_to_write, chip_info->page_write_size);
		transfers[1].read_len = 0;
		transfers[1].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;
		transfers[1].bus_width = chip_info->qpi_bus_width;
		transfers[1].dummy_cycles = ZERO_CYCLES;
		transfers[1].op_mode = SDR_MODE;

		err = tegrabl_qspi_transaction(hqfdi->hqspi, &transfers[0], 2,
									   QSPI_XFER_TIMEOUT);

		if (err != TEGRABL_NO_ERROR) {
			pr_error("QSPI Flash Write failed: x%x\n", err);
			pr_trace("address = 0x%x\n", address);
			/* Disable Write En bit */
			err = qspi_write_en(hqfdi, false);
			if (err != TEGRABL_NO_ERROR) {
				return err;
			}

			break;
		}

		if (bytes_to_write > chip_info->page_write_size) {
			bytes_to_write -= chip_info->page_write_size;
			address += chip_info->page_write_size;
			p_source += chip_info->page_write_size;
		} else {
			bytes_to_write = 0;
		}

		qspi_writein_progress(hqfdi, QSPI_FLASH_WIP_WAIT_FOR_READY,
					QSPI_FLASH_WIP_WAIT_IN_US);
	}

	/* Switch to X1 mode */
	err = qspi_qpi_flag_set(hqfdi, false);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("QPI disable failed err(:0x%x)\n", err);
		return err;
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_bdev_write_block(tegrabl_bdev_t *dev,
		const void *buf, bnum_t block, bnum_t count)
{
	struct tegrabl_qspi_flash_driver_info *hqfdi;

	if ((dev == NULL) || (dev->priv_data == NULL) || (buf == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
	}

	hqfdi = dev->priv_data;
	return tegrabl_qspi_flash_write(hqfdi, block, count, (uint8_t *)buf);
}
#endif

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
static tegrabl_error_t tegrabl_qspi_blockdev_xfer(struct tegrabl_blockdev_xfer_info *xfer)
{
	struct tegrabl_bdev *dev;
	uint32_t block;
	uint32_t count;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t *buf;
	struct tegrabl_qspi_flash_driver_info *hqfdi;

	if ((xfer == NULL) || (xfer->dev == NULL) || (xfer->buf == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
		goto fail;
	}

	dev = xfer->dev;
	buf = xfer->buf;
	block = xfer->start_block;
	count = xfer->block_count;
	hqfdi = dev->priv_data;

	if (count != 0U) {
		err = tegrabl_qspi_flash_read(hqfdi, block, count, (uint8_t *)buf, true);
	}

fail:
	return err;
}

static tegrabl_error_t tegrabl_qspi_blockdev_xfer_wait(struct tegrabl_blockdev_xfer_info *xfer,
						time_t timeout, uint8_t *status_flag)
{
	tegrabl_error_t err;
	tegrabl_err_reason_t err_reason;
	struct tegrabl_qspi_flash_driver_info *hqfdi;
	struct tegrabl_qspi_handle *qspi_handle;

	hqfdi = xfer->dev->priv_data;
	qspi_handle = hqfdi->hqspi;

	err = tegrabl_qspi_xfer_wait(qspi_handle, (uint32_t)timeout, true);
	err_reason = (unsigned int)TEGRABL_ERROR_REASON(err);

	if (err_reason == TEGRABL_ERR_XFER_IN_PROGRESS) {
		*status_flag = TEGRABL_BLOCKDEV_XFER_IN_PROGRESS;
	} else if (err == TEGRABL_NO_ERROR) {
		*status_flag = TEGRABL_BLOCKDEV_XFER_COMPLETE;
		hqfdi->hqspi->is_async = false;
	} else {
		*status_flag = TEGRABL_BLOCKDEV_XFER_FAILURE;
	}

	return err;
}
#endif

/**
 * @brief Initiate the reading of multiple pages of data into buffer.
 *
 * @param start_page_num Start Page Number from which to read
 * @param num_of_pages Number of pages to read
 * @param p_dest Storage for the data read from the device.
 * @param async which represents sync and async transactions
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error
 */
static tegrabl_error_t
tegrabl_qspi_flash_read(
		struct tegrabl_qspi_flash_driver_info *hqfdi,
		uint32_t start_page_num,
		uint32_t num_of_pages,
		uint8_t *p_dest,
		bool async)
{
	struct tegrabl_qspi_flash_chip_info *chip_info = &hqfdi->chip_info;
	struct tegrabl_qspi_transfer *transfers;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t bytes_to_read;
	uint32_t address;
	uint8_t *p_destination = (uint8_t *)p_dest;
	uint8_t *cmd = hqfdi->cmd;

	if (num_of_pages == 0U) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	transfers = hqfdi->transfers;
	memset(transfers, 0, sizeof(struct tegrabl_qspi_transfer));

	if (chip_info->address_length == 4UL) {
		if (hqfdi->plat_params.max_bus_width == QSPI_BUS_WIDTH_X4) {
			if (chip_info->qddr_read == true) {
				*cmd = QSPI_FLASH_CMD_4DDR_QUAD_IO_READ;
			} else {
				*cmd = QSPI_FLASH_CMD_4QUAD_IO_READ;
			}
		} else {
			*cmd = QSPI_FLASH_CMD_4READ;
		}
	} else {
		if (hqfdi->plat_params.max_bus_width == QSPI_BUS_WIDTH_X4) {
			if (chip_info->qddr_read == true) {
				*cmd = QSPI_FLASH_CMD_DDR_QUAD_IO_READ;
			} else {
				*cmd = QSPI_FLASH_CMD_QUAD_IO_READ;
			}
		} else {
			*cmd = QSPI_FLASH_CMD_READ;
		}
	}

	address = start_page_num << chip_info->block_size_log2;
	bytes_to_read = num_of_pages << chip_info->block_size_log2;
	pr_trace("strt_pg_num(%u) num_of_pgs(%u) read_buf(%p)\n",
			 start_page_num, num_of_pages, p_dest);

	/* address are sent to device with MSB first */
	if (chip_info->address_length == 4UL) {
		hqfdi->address_data[0] = (uint8_t)(address >> 24) & 0xFFU;
		hqfdi->address_data[1] = (uint8_t)(address >> 16) & 0xFFU;
		hqfdi->address_data[2] = (uint8_t)(address >> 8) & 0xFFU;
		hqfdi->address_data[3] = (uint8_t)(address) & 0xFFU;
		hqfdi->address_data[4] = 0; /* mode bits */
	} else {
		hqfdi->address_data[0] = (uint8_t)(address >> 16) & 0xFFU;
		hqfdi->address_data[1] = (uint8_t)(address >> 8) & 0xFFU;
		hqfdi->address_data[2] = (uint8_t)(address) & 0xFFU;
		hqfdi->address_data[3] = 0; /* mode bits */
	}

	/* Make sure the Dest is 4-byte aligned */
	if (((uintptr_t)p_dest & 0x3UL) != 0UL) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_ADDRESS, 0);
	}

	/* Set command Parameters in First Transfer */
	/* command must be in SDR X1 mode = 0x0 */
	/* Set Read length is 0 for command */
	transfers[0].tx_buf = cmd;
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

	transfers[1].tx_buf = hqfdi->address_data;
	transfers[1].rx_buf = NULL;
	transfers[1].read_len = 0;
	transfers[1].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;
	transfers[1].bus_width = hqfdi->plat_params.max_bus_width;

	if (hqfdi->plat_params.max_bus_width == QSPI_BUS_WIDTH_X4) {
		/* 1 byte for mode bits in write_len */
		transfers[1].write_len = chip_info->address_length + 1UL;
		transfers[1].dummy_cycles = hqfdi->plat_params.read_dummy_cycles;
		if (chip_info->qddr_read == true) {
			transfers[1].op_mode = DDR_MODE;
			/* Macronics expects dummy clock cycles inclusive of clock cycles used for mode bits.
			 * In SDR mode, mobe bits will require 2 clock cycles but in DDR only 1. Hence add one
			 * more dummy cycle */
			if (MANUFACTURE_ID_MACRONIX == device_info_list[chip_info->device_list_index].manufacture_id) {
				transfers[1].dummy_cycles += 1U;
			}
		} else {
			transfers[1].op_mode = SDR_MODE;
		}

	} else {
		transfers[1].write_len = chip_info->address_length;
		transfers[1].dummy_cycles = ZERO_CYCLES;
		transfers[1].op_mode = SDR_MODE;
	};

	/* Set Readback Parameters in Third Transfer */

	transfers[2].tx_buf = NULL;
	transfers[2].rx_buf = p_destination;
	transfers[2].write_len = 0;
	transfers[2].read_len = bytes_to_read;

	transfers[2].bus_width = hqfdi->plat_params.max_bus_width;
	transfers[2].mode = QSPI_FLASH_ADDR_DATA_MODE_VAL;

	if (hqfdi->plat_params.max_bus_width == QSPI_BUS_WIDTH_X4) {
		if (chip_info->qddr_read == true) {
			transfers[2].op_mode = DDR_MODE;
		} else {
			transfers[2].op_mode = SDR_MODE;
		}
	} else {
		transfers[2].op_mode = SDR_MODE;
    }

	if (async == true) {
		hqfdi->hqspi->is_async = true;
	} else {
		hqfdi->hqspi->is_async = false;
	}

	err = tegrabl_qspi_transaction(hqfdi->hqspi, &transfers[0], QSPI_FLASH_NUM_OF_TRANSFERS,
											QSPI_XFER_TIMEOUT);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("error in qspi transactions\n");
	}

	return err;
}

static tegrabl_error_t qspi_bdev_read_block(tegrabl_bdev_t *dev, void *buf,
	bnum_t block, bnum_t count)
{
	struct tegrabl_qspi_flash_driver_info *hqfdi;
	if ((dev == NULL) || (dev->priv_data == NULL) || (buf == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
	}
	hqfdi = dev->priv_data;
	return tegrabl_qspi_flash_read(hqfdi, block, count, (uint8_t *)buf, false);
}
