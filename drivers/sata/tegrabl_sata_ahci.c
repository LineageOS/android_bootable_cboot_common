/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_SATA

#include "build_config.h"
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <tegrabl_sata.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_sata_ahci.h>
#include <tegrabl_addressmap.h>
#include <arsata.h>
#include <ardev_t_fpci_sata0.h>
#include <ardev_t_ahci.h>
#include <arpmc_impl.h>
#include <tegrabl_sata_config.h>
#include <tegrabl_dmamap.h>
#include <tegrabl_drf.h>
#include <tegrabl_malloc.h>
#include <tegrabl_clock.h>
#include <tegrabl_uphy.h>

/**
 * @brief Dumps ahci registers
 */
static void tegrabl_sata_ahci_dump_registers(void)
{
	uint32_t i = 0;

	pr_debug("AHCI register space dump\n");
	for (i = 0; i <= 0x140; i += 4) {
		pr_debug("0x%08x: 0x%08x\n", NV_ADDRESS_MAP_SATA_AHCI_BASE + i,
				NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + i));
	}
}

/**
 * @brief Starts operation like read, write and identify
 * and waits till it is completed
 *
 * @return TEGRABL_NO_ERROR if transfer is successful else
 * TEGRABL_ERR_TIMEOUT.
 */
static tegrabl_error_t tegrabl_sata_start_command(time_t timeout)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t wait_time = 0;
	uint32_t reg = 0;

	pr_debug("Starting Transaction\n");
	/* Start command from slot 0 */
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXCI_0, 1);

	/* Check if command is completed */
	wait_time = timeout;
	do {
		tegrabl_udelay(1);
		wait_time--;
		if (wait_time == 0) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			pr_debug("Cannot complete command within time\n");
			tegrabl_sata_ahci_dump_registers();
			goto fail;
		}
		reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXCI_0);
	} while (reg != 0);

	/* Check if data transfer is completed */
	wait_time = timeout;
	do {
		tegrabl_udelay(1);
		wait_time--;
		if (wait_time == 0) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			pr_debug("Cannot complete data transfer within time\n");
			tegrabl_sata_ahci_dump_registers();
			goto fail;
		}
		reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXIS_0);
		reg = NV_DRF_VAL(AHCI, PORT_PXIS, DPS, reg);
	} while (!reg);

fail:
	return error;
}

tegrabl_error_t tegrabl_sata_ahci_io(
		struct tegrabl_sata_context *context, void *buf, bnum_t block,
		bnum_t count, bool is_write, time_t timeout)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t reg = 0;
	struct tegrabl_ahci_cmd_table *cmd_table;
	struct tegrabl_ahci_prdt_entry *prdt_entry;
	struct tegrabl_ahci_fis_h2d *fis;
	dma_addr_t address = 0;
	bool mapped_buf = false;
	bool mapped_cmd_list = false;
	bool mapped_cmd_table = false;
	uint32_t block_size_log2 = context->block_size_log2;

	pr_debug("Sata I/O block %d, count %d, ", block, count);
	pr_debug("%s\n", is_write ? "writingg" : "reading");

	cmd_table = (struct tegrabl_ahci_cmd_table *)&context->command_table[0];
	prdt_entry = (struct tegrabl_ahci_prdt_entry *)&cmd_table->prdt_entry[0];
	fis = (struct tegrabl_ahci_fis_h2d *)(&cmd_table->command_fis[0]);

	memset(cmd_table, 0x0, sizeof(*cmd_table));

	/* Fill Command FIS */
	fis->fis_type = TEGRABL_AHCI_FIS_TYPE_REG_H2D;
	fis->prc = (1 << 7);
	fis->device = 0x40;

	/* Use commands for 48 bit address if drive supports */
	if (context->support_extended_cmd) {
		fis->command = is_write ? SATA_COMMAND_DMA_WRITE_EXTENDED :
								 SATA_COMMAND_DMA_READ_EXTENDED;
	} else {
		fis->command = is_write ? SATA_COMMAND_DMA_WRITE :
								 SATA_COMMAND_DMA_READ;
	}

	/* Fill start sector information */
	fis->lba0 = (uint8_t)(block & 0xFF);
	fis->lba1 = (uint8_t)((block >> 8) & 0xFF);
	fis->lba2 = (uint8_t)((block >> 16) & 0xFF);
	fis->lba3 = (uint8_t)((block >> 24) & 0xFF);
	block = block >> 24;
	fis->lba4 = (uint8_t)((block >> 8) & 0xFF);
	fis->lba5 = (uint8_t)((block >> 16) & 0xFF);

	/* Fill number of sectors to be read */
	fis->countl = (uint8_t)(count & 0xFF);
	fis->counth = (uint8_t)((count >> 8) & 0xFF);

	/* Map input buffer as per read/write and get physical address */
	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_SATA, context->instance,
				buf, count << block_size_log2,
				is_write ? TEGRABL_DMA_TO_DEVICE : TEGRABL_DMA_FROM_DEVICE);

	mapped_buf = true;

	pr_debug("Buffer addresss is %p\n", buf);
	pr_debug("Dma address of buffer is %"PRIx64"\n", address);

	if (!address) {
		pr_debug("dma map returned zero address for buf.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Fill the prdt entry */
	prdt_entry->address_low = (address & 0xFFFFFFFF);
	prdt_entry->address_high = (((address >> 32) & 0xFFFFFFFF));
	prdt_entry->irc = (1 << 31);
	prdt_entry->irc |= ((count << block_size_log2) - 1);

	/* Fill the command list. Use only one prdt entry. */
	context->command_list_buf[0] = AHCI_CMD_HEADER_CFL | AHCI_CMD_HEADER_PRDTL;
	if (is_write) {
		context->command_list_buf[0] |= CMD_HEADER_WRITE;
	}

	context->command_list_buf[1] = count << block_size_log2;

	/* Flush the updated command table and get its physical address */
	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->command_table[0], TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE,
			TEGRABL_DMA_TO_DEVICE);

	mapped_cmd_table = true;

	pr_debug("Dma address of command table is %"PRIx64"\n", address);

	if (!address) {
		pr_debug("dma map returned zero address for command table.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context->command_list_buf[2] = (address & 0xFFFFFFFF);
	context->command_list_buf[3] = (((address >> 32) & 0xFFFFFFFF));

	/* Flush command list buffer */
	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_SATA, context->instance,
				&context->command_list_buf[0],
				TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE, TEGRABL_DMA_TO_DEVICE);
	mapped_cmd_list = true;

	pr_debug("Dma address of command list buffer is %"PRIx64"\n", address);

	if (!address) {
		pr_debug("dma map returned zero address for command list buf.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Enable appropriate interrupts */
	reg = 0;
	reg = NV_FLD_SET_DRF_NUM(AHCI, PORT_PXIE, DPE, 1, reg);

	if (is_write) {
		reg = NV_FLD_SET_DRF_NUM(AHCI, PORT_PXIE, DHRE, 1, reg);
	}

	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXIE_0, reg);

	/* Initiate transaction and wait for completion or timeout */
	error = tegrabl_sata_start_command(timeout);

fail:
	if (mapped_cmd_list) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->command_list_buf[0],
			TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE, TEGRABL_DMA_TO_DEVICE);
	}

	if (mapped_buf) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			buf, count << block_size_log2,
			is_write ? TEGRABL_DMA_TO_DEVICE : TEGRABL_DMA_FROM_DEVICE);
	}

	if (mapped_cmd_table) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->command_table[0], TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE,
			TEGRABL_DMA_TO_DEVICE);
	}

	return error;
}

tegrabl_error_t tegrabl_sata_ahci_erase(
		struct tegrabl_sata_context *context, bnum_t block, bnum_t count)
{
	TEGRABL_UNUSED(context);
	TEGRABL_UNUSED(block);
	TEGRABL_UNUSED(count);

	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
}

tegrabl_error_t tegrabl_sata_ahci_flush_device(
		struct tegrabl_sata_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t reg = 0;
	dma_addr_t address = 0;
	struct tegrabl_ahci_cmd_table *cmd_table;
	struct tegrabl_ahci_fis_h2d *fis;
	bool mapped_cmd_list = false;
	bool mapped_cmd_table = false;

	pr_debug("Flushing the device\n");

	if (!context->supports_flush && !context->supports_flush_ext) {
		pr_info("SATA device does not support flush command.\n");
		goto fail;
	}

	cmd_table = (struct tegrabl_ahci_cmd_table *)&context->command_table[0];
	fis = (struct tegrabl_ahci_fis_h2d *)(&cmd_table->command_fis[0]);

	memset(cmd_table, 0x0, sizeof(*cmd_table));

	/* Fill Command FIS */
	fis->fis_type = TEGRABL_AHCI_FIS_TYPE_REG_H2D;
	fis->prc = (1 << 7);
	fis->device = 0x40;

	/* Use commands for 48 bit address if drive supports */
	if (context->supports_flush_ext)
		fis->command = SATA_COMMAND_FLUSH_EXTENDED;
	else
		fis->command = SATA_COMMAND_FLUSH;

	/* Fill the command list. No prdt entry. */
	context->command_list_buf[0] = AHCI_CMD_HEADER_CFL | CMD_HEADER_WRITE;

	context->command_list_buf[1] = 0;

	/* Flush the updated command table and get its physical address */
	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->command_table[0], TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE,
			TEGRABL_DMA_TO_DEVICE);

	mapped_cmd_table = true;

	pr_debug("Dma address of command table is %"PRIx64"\n", address);

	if (!address) {
		pr_debug("dma map returned zero address for command table.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context->command_list_buf[2] = (address & 0xFFFFFFFF);
	context->command_list_buf[3] = (((address >> 32) & 0xFFFFFFFF));

	/* Flush command list buffer */
	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_SATA, context->instance,
				&context->command_list_buf[0],
				TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE, TEGRABL_DMA_TO_DEVICE);
	mapped_cmd_list = true;

	pr_debug("Dma address of command list buffer is %"PRIx64"\n", address);

	if (!address) {
		pr_debug("dma map returned zero address for command list buf.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Enable appropriate interrupts */
	reg = 0;
	reg = NV_FLD_SET_DRF_NUM(AHCI, PORT_PXIE, DPE, 1, reg);
	reg = NV_FLD_SET_DRF_NUM(AHCI, PORT_PXIE, DHRE, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXIE_0, reg);

	/* Initiate transaction and wait for completion or timeout */
	error = tegrabl_sata_start_command(TEGRABL_SATA_FLUSH_TIMEOUT);

fail:
	if (mapped_cmd_list) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->command_list_buf[0],
			TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE, TEGRABL_DMA_TO_DEVICE);
	}

	if (mapped_cmd_table) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->command_table[0], TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE,
			TEGRABL_DMA_TO_DEVICE);
	}

	return error;
}

/**
 * @brief Identifies the device connected to SATA controller
 * and retrieves the information about storage device
 *
 * @param context SATA context
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_sata_ahci_indentify_device(
		struct tegrabl_sata_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t reg = 0;
	dma_addr_t address = 0;
	struct tegrabl_ahci_cmd_table *cmd_table;
	struct tegrabl_ahci_prdt_entry *prdt_entry;
	struct tegrabl_ahci_fis_h2d *fis;
	struct tegrabl_ata_dev_id *dev_id;
	bool mapped_id_buf = false;
	bool mapped_cmd_list = false;
	bool mapped_cmd_table = false;

	cmd_table = (struct tegrabl_ahci_cmd_table *)&context->command_table[0];
	prdt_entry = (struct tegrabl_ahci_prdt_entry *)&cmd_table->prdt_entry[0];
	fis = (struct tegrabl_ahci_fis_h2d *)(&cmd_table->command_fis[0]);

	memset(cmd_table, 0x0, sizeof(*cmd_table));

	/* Fill command fis */
	fis->fis_type = TEGRABL_AHCI_FIS_TYPE_REG_H2D;
	fis->prc = (1 << 7);
	fis->command = SATA_COMMAND_IDENTIFY;
	fis->featurel = 144;

	/* Map input buffer as per read/write and get physical address */
	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->indentity_buf[0], TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE,
			TEGRABL_DMA_FROM_DEVICE);

	mapped_id_buf = true;

	pr_debug("Dma address of identity buffer is %"PRIx64"\n", address);

	if (!address) {
		pr_debug("dma map returned zero address for identity buf.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Fill the prdt entry */
	prdt_entry->address_low = (address & 0xFFFFFFFF);
	prdt_entry->address_high = (((address >> 32) & 0xFFFFFFFF));
	prdt_entry->irc = (1 << 31) | ((1 << context->block_size_log2) - 1);

	/* Fill the command list. Use only one prdt entry. */
	context->command_list_buf[0] = AHCI_CMD_HEADER_CFL | AHCI_CMD_HEADER_PRDTL;
	context->command_list_buf[1] = (1 << context->block_size_log2);

	/* Flush the updated command table and get its physical address */
	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->command_table[0], TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE,
			TEGRABL_DMA_TO_DEVICE);

	mapped_cmd_table = true;

	pr_debug("Dma address of command table is %"PRIx64"\n", address);

	if (!address) {
		pr_debug("dma map returned zero address for command table.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context->command_list_buf[2] = (address & 0xFFFFFFFF);
	context->command_list_buf[3] = (((address >> 32) & 0xFFFFFFFF));

	/* Flush command list buffer */
	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_SATA, context->instance,
				&context->command_list_buf[0],
				TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE, TEGRABL_DMA_TO_DEVICE);

	mapped_cmd_list = true;

	pr_debug("Dma address of command list buffer is %"PRIx64"\n", address);

	if (!address) {
		pr_debug("dma map returned zero address for command list buf.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Enable appropriate interrupts */
	reg = 0;
	reg = NV_FLD_SET_DRF_NUM(AHCI, PORT_PXIE, DPE, 1, reg);
	reg = NV_FLD_SET_DRF_NUM(AHCI, PORT_PXIE, PSE, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXIE_0, reg);

	/* Initiate transaction and wait for completion or timeout */
	error = tegrabl_sata_start_command(TEGRABL_SATA_IDENTIFY_TIMEOUT);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	/* Unmap identity buffer before accessing */
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->indentity_buf[0], TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE,
			TEGRABL_DMA_FROM_DEVICE);
	mapped_id_buf = false;

	/* Retrieve frequently used information */
	dev_id = (struct tegrabl_ata_dev_id *)&context->indentity_buf[0];

	/* typecast to uint64_t to avoid promotion to int and then
	 * implicit sign extension by gcc.
	 */
	context->block_count = (uint64_t)dev_id->sectors[0] |
						   (uint64_t)dev_id->sectors[1] << 8  |
						   (uint64_t)dev_id->sectors[2] << 16 |
						   (uint64_t)dev_id->sectors[3] << 24;

	if (context->block_count == 0x0FFFFFFF) {
		context->block_count = (uint64_t)dev_id->sectors_48bit[0] |
							   (uint64_t)dev_id->sectors_48bit[1] << 8  |
							   (uint64_t)dev_id->sectors_48bit[2] << 16 |
							   (uint64_t)dev_id->sectors_48bit[3] << 24;
	}

	context->supports_flush = dev_id->command_supported[1] &
								(1 << SATA_SUPPORTS_FLUSH);
	context->supports_flush_ext = dev_id->command_supported[1] &
								(1 << SATA_SUPPORTS_FLUSH_EXT);

	context->support_extended_cmd = dev_id->command_supported[1] &
								(1 << SATA_SUPPORTS_48_BIT_ADDRESS);

	pr_debug("%s extended command.",
			context->support_extended_cmd ? "Supports" : "Does not support");
	pr_debug("Total sectors %d\n", (uint32_t)context->block_count);
	pr_debug("%s flush command.",
			context->supports_flush ? "Supports" : "Does not support");

fail:
	if (mapped_cmd_list) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->command_list_buf[0],
			TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE, TEGRABL_DMA_TO_DEVICE);
	}

	if (mapped_id_buf) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->indentity_buf[0], TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE,
			TEGRABL_DMA_FROM_DEVICE);
	}

	if (mapped_cmd_table) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->command_table[0],
			TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE, TEGRABL_DMA_TO_DEVICE);
	}

	return error;
}


/**
 * @brief Enables clocks required for SATA. Also configures with
 * appropriate divisor and clock source.
 *
 * returns TEGRABL_NO_ERROR if successful else appropriate error
 */
static tegrabl_error_t tegrabl_sata_ahci_enable_clock(void)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t temp = 0;

	if (tegrabl_car_clk_is_enabled(TEGRABL_MODULE_SATA, 0) &&
			tegrabl_car_clk_is_enabled(TEGRABL_MODULE_SATA_OOB, 0)) {
		goto done;
	}

	error = tegrabl_car_rst_set(TEGRABL_MODULE_SATA, 0);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_car_rst_set(TEGRABL_MODULE_SATACOLD, 0);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	tegrabl_udelay(1);

	error = tegrabl_car_clk_enable(TEGRABL_MODULE_SATA, 0, NULL);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_car_clk_enable(TEGRABL_MODULE_SATA_OOB, 0, NULL);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_car_set_clk_src(TEGRABL_MODULE_SATA, 0,
			TEGRABL_CLK_SRC_PLLP_OUT0);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_car_set_clk_src(TEGRABL_MODULE_SATA_OOB, 0,
			TEGRABL_CLK_SRC_PLLP_OUT0);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_car_set_clk_rate(TEGRABL_MODULE_SATA, 0,
			SATA_CLK_FREQUENCY_VAL, &temp);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_car_set_clk_rate(TEGRABL_MODULE_SATA_OOB, 0,
			SATA_OOB_CLK_FREQUENCY_VAL, &temp);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	tegrabl_udelay(1);

	/* Removing Clamps for SAX Partition, this is required just after power
	 * on SAX Partition
	 */
	NV_WRITE32(NV_ADDRESS_MAP_PMC_IMPL_BASE + PMC_IMPL_PART_SAX_CLAMP_CONTROL_0,
			0x00000000);

done:
	(void) tegrabl_car_rst_clear(TEGRABL_MODULE_SATA, 0);

	(void)tegrabl_car_rst_clear(TEGRABL_MODULE_SATACOLD, 0);

fail:
	return error;
}

/**
 * @brief Resets and initializes the SATA controller.
 *
 */
static void tegrabl_sata_ahci_init_controller(void)
{
	uint32_t reg = 0;

	/* SATA_FPCI_BAR5_0[FPCI_BAR5_START] to ‘0x0040020
	 * SATA_FPCI_BAR5_0[FPCI_BAR5_ACCESS_TYPE] to ‘1’
	 */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_BASE + SATA_FPCI_BAR5_0);
	reg = NV_FLD_SET_DRF_NUM(SATA, FPCI_BAR5,
			FPCI_BAR5_START, FPCI_BAR5_START_VAL, reg);
	reg = NV_FLD_SET_DRF_NUM(SATA, FPCI_BAR5, FPCI_BAR5_ACCESS_TYPE, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + SATA_FPCI_BAR5_0, reg);

	/* SATA_CONFIGURATION_0[EN_FPCI] to '1' */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_BASE + SATA_CONFIGURATION_0);
	reg = NV_FLD_SET_DRF_NUM(SATA, CONFIGURATION, EN_FPCI, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + SATA_CONFIGURATION_0, reg);

	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CHX_PHY_CTRL17_0,
			0x55010000);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CHX_PHY_CTRL18_0,
			0x55010000);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CHX_PHY_CTRL20_0, 0x1);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CHX_PHY_CTRL21_0, 0x1);

	/* SATA_CFG_PHY_0_0[MASK_SQUELCH] to ‘1’ */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_PHY_0_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, CFG_PHY_0, MASK_SQUELCH, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_PHY_0_0, reg);

	/* SATA_NVOOB_0[COMMA_CNT] to value as per the chip */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_NVOOB_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, NVOOB, COMMA_CNT, NVOOB_COMMA_CNT_VAL, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_NVOOB_0, reg);

	/* SATA_NVOOB_0[SQUELCH_FILTER_LENGTH] as per the chip */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_NVOOB_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, NVOOB, SQUELCH_FILTER_LENGTH,
		NVOOB_SQUELCH_FILTER_LENGTH_VAL, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_NVOOB_0, reg);

	/* SATA_NVOOB_0[SQUELCH_FILTER_MODE] to '2’b01' */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_NVOOB_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, NVOOB, SQUELCH_FILTER_MODE,
		NVOOB_SQUELCH_FILTER_MODE_VAL, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_NVOOB_0, reg);

	/* Bug 200144927 */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG2NVOOB_2_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, CFG2NVOOB_2, COMWAKE_IDLE_CNT_LOW,
			CFG2NVOOB_2_COMWAKE_IDLE_CNT_LOW_VAL, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG2NVOOB_2_0, reg);

	/* War for GEN3 drives getting detected as GEN1 issue
	 * SATA_CFG_PHY_0_0[USE_7BIT_ALIGN_DET_FOR_SPD] to ‘1’b0
	 */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_PHY_0_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, CFG_PHY_0,
			USE_7BIT_ALIGN_DET_FOR_SPD, 0, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_PHY_0_0, reg);

	/* NV_PROJ__SATA_CFG_1_BUS_MASTER to ‘1’
	 * NV_PROJ__SATA_CFG_1_MEMORY_SPACE to ‘1’
	 */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_1_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, CFG_1, BUS_MASTER, 1, reg);
	reg = NV_FLD_SET_DRF_NUM(SATA0, CFG_1, MEMORY_SPACE, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_1_0, reg);

	/* NV_PROJ__SATA_CFG_9_BASE_ADDRESS to ‘0x40020000’ */
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_9_0,
			CFG9_BASE_ADDRESS_VAL);

	/* NV_PROJ__SATA_CFG_SATA_0[BACKDOOR_PROG_IF_EN] to 1’b1 */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_SATA_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, CFG_SATA, BACKDOOR_PROG_IF_EN, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_SATA_0, reg);

	/* NV_PROJ__SATA_BKDOOR_CC_0[CLASS_CODE] to 0x0106 */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_BKDOOR_CC_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, BKDOOR_CC, CLASS_CODE,
			BKDOOR_CC_CLASS_CODE_VAL, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_BKDOOR_CC_0, reg);

	/* NV_PROJ__SATA_BKDOOR_CC_0[PROG_IF] to 0x01 */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_BKDOOR_CC_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, BKDOOR_CC, PROG_IF,
			BKDOOR_CC_PROG_IF_VAL, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_BKDOOR_CC_0, reg);

	/* NV_PROJ__SATA_CFG_SATA_0[BACKDOOR_PROG_IF_EN] to 1’b0 */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_SATA_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, CFG_SATA, BACKDOOR_PROG_IF_EN, 0, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_CFG_SATA_0, reg);

	/* Disable low power mode. */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_AHCI_HBA_CAP_BKDR_0);
	reg = NV_FLD_SET_DRF_NUM(SATA0, AHCI_HBA_CAP_BKDR, PARTIAL_ST_CAP, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(SATA0, AHCI_HBA_CAP_BKDR, SLUMBER_ST_CAP, 0, reg);
	reg = NV_FLD_SET_DRF_NUM(SATA0, AHCI_HBA_CAP_BKDR, SALP, 0, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_CFG_BASE + SATA0_AHCI_HBA_CAP_BKDR_0, reg);
}

static tegrabl_error_t tegrabl_sata_ahci_reset(
		struct tegrabl_sata_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t reg = 0;
	dma_addr_t address = 0;
	uint32_t timeout = 0;
	bool mapped_cmd_list = false;
	bool mapped_rfis = false;

	/* NOTE: Corsair requires posedge PxSCTL.DET. See Bug 200139714.
	 * Whenever code for programming PxSCTL.DET is added, please add
	 * war for Bug 200139714.
	 */

	/* Enable AHCI Mode */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_HBA_GHC_0);
	reg = NV_FLD_SET_DRF_NUM(AHCI, HBA_GHC, AE, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_HBA_GHC_0, reg);

	/* Enable AHCI Mode */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_HBA_GHC_0);
	reg = NV_FLD_SET_DRF_NUM(AHCI, HBA_GHC, HR, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_HBA_GHC_0, reg);

	/* Enable ahci interrupt */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_HBA_GHC_0);
	reg = NV_FLD_SET_DRF_NUM(AHCI, HBA_GHC, AE, 1, reg);
	reg = NV_FLD_SET_DRF_NUM(AHCI, HBA_GHC, IE, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_HBA_GHC_0, reg);

	if (context->speed != TEGRABL_SATA_INTERFACE_GEN2) {
		reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_HBA_CAP_BKDR_0);
		reg = NV_FLD_SET_DRF_DEF(AHCI, HBA_CAP_BKDR, INTF_SPD_SUPP, GEN1, reg);
		NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_HBA_CAP_BKDR_0, reg);

		reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXSCTL_0);
		reg = NV_FLD_SET_DRF_DEF(AHCI, PORT_PXSCTL, SPD, GEN1, reg);
		NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXSCTL_0, reg);
	}

	/* SATA_INTR_MASK_0[IP_INT_MASK] to 1’b1 */
	reg = 0;
	reg = NV_FLD_SET_DRF_NUM(SATA, INTR_MASK, IP_INT_MASK, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_BASE + SATA_INTR_MASK_0, reg);

	/* Get physical address of command list buffer */
	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_SATA, context->instance,
				&context->command_list_buf[0],
				TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE, TEGRABL_DMA_TO_DEVICE);

	mapped_cmd_list = true;

	pr_debug("Dma address of command list buffer is %"PRIx64"\n", address);

	if (!address) {
		pr_debug("dma map returned zero address for command list buffer.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Write lower address bits of command list buffer */
	reg = (uint32_t)(address & 0xFFFFFFFF);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXCLB_0, reg);

	/* Write upper address bits of command list buffer */
	reg = (uint32_t)((address >> 32) & 0xFFFFFFFF);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXCLBU_0, reg);

	/* Get the physical address of rfis */
	address = tegrabl_dma_map_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->rfis[0], TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE,
			TEGRABL_DMA_TO_DEVICE);

	mapped_rfis = true;

	pr_debug("Dma address of rfis buffer is %"PRIx64"\n", address);

	if (!address) {
		pr_debug("dma map returned zero address for rfis buffer.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Write lower address bits of FIS buffer */
	reg = (uint32_t)(address & 0xFFFFFFFF);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXFB_0, reg);

	/* Write upper address bits of command list buffer */
	reg = (uint32_t)((address >> 32) & 0xFFFFFFFF);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXFBU_0, reg);

	/* Enable appropriate interrupts */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXIE_0);
	reg = NV_FLD_SET_DRF_NUM(AHCI, PORT_PXIE, PCE, 1, reg);
	reg = NV_FLD_SET_DRF_NUM(AHCI, PORT_PXIE, PRCE, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXIE_0, reg);

	/* Wait for phy ready and connection status */
	timeout = SATA_COMINIT_TIMEOUT;
	while (timeout != 0U) {
		tegrabl_udelay(1);
		timeout--;
		reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXIS_0);
		reg = NV_DRF_VAL(AHCI, PORT_PXIS, PCS, reg) &&
			  NV_DRF_VAL(AHCI, PORT_PXIS, PRCS, reg);
		if (reg != 0U)
			break;
	}

	if (!timeout) {
		pr_debug("Failed to complete com init\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
		tegrabl_sata_ahci_dump_registers();
		goto fail;
	}

	pr_debug("PXSSTS: 0x%08x\n",
			NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXSSTS_0));

	/* Clear any error bit set */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXSERR_0);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXSERR_0, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXIS_0, 0);

	/* Receive FIS from device */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXCMD_0);
	reg = NV_FLD_SET_DRF_NUM(AHCI, PORT_PXCMD, FRE, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXCMD_0, reg);

	timeout = SATA_D2H_FIS_TIMEOUT;
	while (timeout != 0U) {
		tegrabl_udelay(1);
		timeout--;
		reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXTFD_0);
		reg = NV_DRF_VAL(AHCI, PORT_PXTFD, STS_ERR, reg) ||
			  NV_DRF_VAL(AHCI, PORT_PXTFD, STS_DRQ, reg) ||
			  NV_DRF_VAL(AHCI, PORT_PXTFD, STS_BSY, reg);

		if (!reg) {
			break;
		}
	}

	if (!timeout) {
		pr_debug("Failed to receive FIS from device.\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
		goto fail;
	}

	/* Clear any error bit set */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXSERR_0);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXSERR_0, reg);

	/* Start processing commands */
	reg = NV_READ32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXCMD_0);
	reg = NV_FLD_SET_DRF_NUM(AHCI, PORT_PXCMD, ST, 1, reg);
	NV_WRITE32(NV_ADDRESS_MAP_SATA_AHCI_BASE + AHCI_PORT_PXCMD_0, reg);

fail:
	if (mapped_cmd_list) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->command_list_buf[0],
			TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE, TEGRABL_DMA_TO_DEVICE);
	}

	if (mapped_rfis) {
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_SATA, context->instance,
			&context->rfis[0], TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE,
			TEGRABL_DMA_TO_DEVICE);
	}

	return error;
}

void tegrabl_sata_ahci_free_buffers(struct tegrabl_sata_context *context)
{
	tegrabl_dealloc(TEGRABL_HEAP_DMA, context->rfis);
	tegrabl_dealloc(TEGRABL_HEAP_DMA, context->indentity_buf);
	tegrabl_dealloc(TEGRABL_HEAP_DMA, context->command_list_buf);
	tegrabl_dealloc(TEGRABL_HEAP_DMA, context->command_table);
}

/**
 * @brief Initializes the context with default values.
 *
 * @param context Context information.
 */
static tegrabl_error_t tegrabl_sata_ahci_init_memory_regions(
		struct tegrabl_sata_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* NOTE: All buffers should be accessible to controller. So allocate
	 * from dma heap only.
	 */
	pr_debug("Setting memory regions\n");

	/* Buffer to receive FIS should be aligned to 1KB or 4KB */
	context->rfis = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
			4096, TEGRABL_SATA_AHCI_RFIS_SIZE);
	if (!context->rfis) {
		pr_debug("Failed to allocate memory for RFIS buffer\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	memset(context->rfis, 0x0, TEGRABL_SATA_AHCI_RFIS_SIZE);

	/* Allocate memory for identity buffer */
	context->indentity_buf = tegrabl_alloc(TEGRABL_HEAP_DMA,
			TEGRABL_SATA_AHCI_DEVICE_IDENTITY_BUF_SIZE);
	if (!context->indentity_buf) {
		pr_debug("Failed to allocate memory for identity buffer\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	memset(context->indentity_buf, 0x0,
			TEGRABL_SATA_AHCI_DEVICE_IDENTITY_BUF_SIZE);

	/* Command list buffer should be aligned to 1024 */
	context->command_list_buf = tegrabl_alloc_align(TEGRABL_HEAP_DMA,
			1024, TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE);
	if (!context->command_list_buf) {
		pr_debug("Failed to allocate memory for command list buffer\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	memset(context->command_list_buf, 0x0,
			TEGRABL_SATA_AHCI_COMMAND_LIST_BUF_SIZE);

	/* Command table should be aligned to 256 */
	context->command_table = tegrabl_alloc_align(TEGRABL_HEAP_DMA, 256,
			TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE);
	if (context->command_table == NULL) {
		pr_debug("Failed to allocate memory for command table\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	memset(context->command_table, 0x0, TEGRABL_SATA_AHCI_COMMAND_TABLE_SIZE);

	pr_debug("RFIS buffer @ %p\n", context->rfis);
	pr_debug("Command list buffer @ %p\n", context->command_list_buf);
	pr_debug("Command table buffer @ %p\n", context->command_table);
	pr_debug("Identity buffer @ %p\n", context->indentity_buf);

fail:
	return error;
}

#if defined(CONFIG_ENABLE_SATA_PWERDOWN)
/**
 * @brief Disables SATA clock.
 */
static void tegrabl_sata_car_disable(void)
{
	pr_debug("Disabling sata clocks\n");

	(void) tegrabl_car_rst_set(TEGRABL_MODULE_SATA, 0);

	(void) tegrabl_car_rst_set(TEGRABL_MODULE_SATACOLD, 0);

	(void) tegrabl_car_clk_disable(TEGRABL_MODULE_SATA_OOB, 0);

	(void) tegrabl_car_clk_disable(TEGRABL_MODULE_SATA, 0);
}
#endif

tegrabl_error_t tegrabl_sata_ahci_init(struct tegrabl_sata_context *context,
		struct tegrabl_uphy_handle *uphy)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (context->initialized) {
		pr_info("sata is already initialized");
		goto fail;
	}

#if defined(CONFIG_ENABLE_SATA_PWERDOWN)
	tegrabl_sata_car_disable();
	if (uphy && uphy->power_down) {
		uphy->power_down(TEGRABL_UPHY_SATA);
	}
#endif

	if (uphy && uphy->init) {
		pr_debug("Initializing uphy\n");
		error = uphy->init(TEGRABL_UPHY_SATA);
		if (error != TEGRABL_NO_ERROR) {
			pr_debug("Failed to initialize uphy\n");
			goto fail;
		}
	}
	pr_debug("Initializing sata clocks\n");
	error = tegrabl_sata_ahci_enable_clock();
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_debug("Allocating memory regions for SATA");
	error = tegrabl_sata_ahci_init_memory_regions(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_debug("Initializing sata controller\n");
	tegrabl_sata_ahci_init_controller();

	pr_debug("Reseting sata controller\n");
	error = tegrabl_sata_ahci_reset(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_debug("Retrieving SATA device information\n");
	error = tegrabl_sata_ahci_indentify_device(context);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	return error;
}
