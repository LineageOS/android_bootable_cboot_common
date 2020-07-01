/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_SPI

#include "build_config.h"
#include <stdint.h>
#include <tegrabl_drf.h>
#include <tegrabl_malloc.h>
#include <tegrabl_clock.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_gpcdma.h>
#include <arqspi.h>
#include <tegrabl_qspi.h>
#include <tegrabl_qspi_flash.h>
#include <tegrabl_qspi_private.h>
#include <inttypes.h>

/* Flush fifo timeout, resolution = 10us */
#define FLUSHFIFO_TIMEOUT	10000  /* 10000 x 10us = 100ms */
enum {
	AUX_INFO_FLUSH_FIFO,
	AUX_INFO_CHECK_TIMEOUT,
	AUX_INFO_WRITE_FAIL, /* 0x2 */
	AUX_INFO_ADDR_OVRFLOW,
	AUX_INFO_INVALID_TXFER_ARGS,
	AUX_INFO_INVALID_DMA, /* 0x5 */
};

enum flush_type {
	TX_FIFO_FLUSH,
	RX_FIFO_FLUSH,
};

static struct tegrabl_qspi_ctxt s_qspi_context;
static struct tegrabl_mb1bct_qspi_params *qspi_params;
static tegrabl_error_t configure_qspi_clk(void);
static tegrabl_error_t qspi_hw_flush_fifos(enum flush_type type);
/* dump debug registers */
/*
void qspi_dump_registers(void)
{
	uint32_t reg32 = 0;

	pr_debug("**************QSPI registers ***************\n");
	reg32 = qspi_readl(COMMAND);
	pr_debug("command = %x\n", reg32);
	reg32 = qspi_readl(COMMAND2);
	pr_debug("command2 = %x\n", reg32);
	reg32 = qspi_readl(TIMING_REG1);
	pr_debug("timingreg1 = %x\n", reg32);
	reg32 = qspi_readl(TIMING_REG2);
	pr_debug("timingreg2 = %x\n", reg32);
	reg32 = qspi_readl(TRANSFER_STATUS);
	pr_debug("transferstatus = %x\n", reg32);
	reg32 = qspi_readl(FIFO_STATUS);
	pr_debug("fifostatus = %x\n", reg32);
	reg32 = qspi_readl(DMA_CTL);
	pr_debug("dmactl = %x\n", reg32);
	reg32 = qspi_readl(DMA_BLK_SIZE);
	pr_debug("dma_blk_size = %x\n", reg32);
	reg32 = qspi_readl(INTR_MASK);
	pr_debug("intr_mask = %x\n", reg32);
	reg32 = qspi_readl(SPARE_CTLR);
	pr_debug("spare_ctlr = %x\n", reg32);
	reg32 = qspi_readl(MISC);
	pr_debug("misc = %x\n", reg32);
	reg32 = qspi_readl(TIMING3);
	pr_debug("timing3 = %x\n", reg32);
}

*/
static tegrabl_error_t tegrabl_qspi_finish_init(void)
{
	uint32_t reg;
	tegrabl_error_t err;

	/* reconfigure the clock */
	err = configure_qspi_clk();
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	/* Configure initial settings */
	reg = qspi_readl(COMMAND);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, M_S, MASTER, reg);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, MODE, Mode0, reg);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, CS_SEL, CS0, reg);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, CS_POL_INACTIVE0, DEFAULT,
			reg);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, CS_SW_HW, SOFTWARE, reg);
	/* Look at QSPI Spansion/Micron devices' data sheet.
	 * CS pin of flash device is active low.
	 * To rx/tx, transition CS from high to low, send/rx,
	 * transition CS from low to high
	 */
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, CS_SW_VAL, HIGH, reg);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, IDLE_SDA, DRIVE_LOW, reg);

	qspi_writel(COMMAND, reg);

	/* Flush Tx fifo */
	err = qspi_hw_flush_fifos(TX_FIFO_FLUSH);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("QSPI: failed to flush tx fifo\n");
		return err;
	}

	/* Flush Rx fifo */
	err = qspi_hw_flush_fifos(RX_FIFO_FLUSH);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("QSPI: failed to flush rx fifo\n");
		return err;
	}

	pr_debug("tx_clk_tap_delay : %u\n", qspi_params->trimmer_val1);
	pr_debug("rx_clk_tap_delay : %u\n", qspi_params->trimmer_val2);

	/* Program trimmer values based on params */
	reg =
	NV_DRF_NUM(QSPI, COMMAND2, Tx_Clk_TAP_DELAY, qspi_params->trimmer_val1) |
	NV_DRF_NUM(QSPI, COMMAND2, Rx_Clk_TAP_DELAY, qspi_params->trimmer_val2);
	qspi_writel_flush(COMMAND2, reg);

	return TEGRABL_NO_ERROR;
}

static bool is_qspi_transfer_completed(void)
{
	/* Read the Status register and findout whether the QSPI is busy
	 * Should be called only if rx and/or tx was enabled else ready bit
	 * is 0 by default
	 */
	uint32_t reg32 = 0;
	bool ret_val = false;

	/* ((DMA || PIO ) &&  !(RDY)) == 1 => ongoing  */
	/* ((DMA || PIO ) &&  !(RDY)) == 0 => complete */

	/* DMA and PIO bits are auto clear bits, and it becomes
	 * zero once xfer is complete
	 */

	if (((NV_DRF_VAL(QSPI, DMA_CTL, DMA_EN, qspi_readl(DMA_CTL))) ||
		 (NV_DRF_VAL(QSPI, COMMAND, PIO, qspi_readl(COMMAND)))) &&
			(NV_DRF_VAL(QSPI, TRANSFER_STATUS, RDY,
						qspi_readl(TRANSFER_STATUS)) == 0)) {
		ret_val = false;
	} else {
		ret_val = true;
	}

	/* Clear the Ready bit in else if it is set */
	/* Ready bit is not auto clear bit */
	reg32 = qspi_readl(TRANSFER_STATUS);
	if (reg32 & NV_DRF_DEF(QSPI, TRANSFER_STATUS, RDY, READY)) {
		/* Write 1 to RDY bit field to clear it. */
		qspi_writel_flush(TRANSFER_STATUS,
						  NV_DRF_DEF(QSPI, TRANSFER_STATUS, RDY, READY));
	}
	return ret_val;
}

static void set_qspi_chip_select_level(bool is_level_high)
{
	/* Is effective only when SW CS is being used */
	uint32_t cmd_reg = qspi_readl(COMMAND);

	cmd_reg = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, CS_SW_VAL,
			is_level_high, cmd_reg);
	qspi_writel_flush(COMMAND, cmd_reg);
}

static tegrabl_error_t qspi_hw_flush_fifos(enum flush_type type)
{
	uint32_t status_reg;
	uint32_t timeout_count = 0;
	uint32_t flush_field = 0;

	/* read fifo status */
	status_reg = qspi_readl(FIFO_STATUS);

	switch (type) {
	case TX_FIFO_FLUSH:
		/* return if tx fifo is empty */
		if (NV_DRF_VAL(QSPI, FIFO_STATUS, TX_FIFO_EMPTY, status_reg) ==
				 QSPI_FIFO_STATUS_0_TX_FIFO_EMPTY_EMPTY)
			return TEGRABL_NO_ERROR;

		flush_field = NV_DRF_DEF(QSPI, FIFO_STATUS, TX_FIFO_FLUSH, FLUSH);
		break;

	case RX_FIFO_FLUSH:
		/* return if rx fifo is empty */
		if (NV_DRF_VAL(QSPI, FIFO_STATUS, RX_FIFO_EMPTY, status_reg) ==
				 QSPI_FIFO_STATUS_0_TX_FIFO_EMPTY_EMPTY)
			return TEGRABL_NO_ERROR;

		flush_field = NV_DRF_DEF(QSPI, FIFO_STATUS, RX_FIFO_FLUSH, FLUSH);
		break;

	default:
		return TEGRABL_NO_ERROR;
	}

	/* Write in to Status register to clear the FIFOs */
	qspi_writel_flush(FIFO_STATUS, flush_field);

	/* Wait until those bits become 0. */
	do {
		tegrabl_udelay(1);
		status_reg = qspi_readl(FIFO_STATUS);
		if (!(status_reg & flush_field)) {
			return TEGRABL_NO_ERROR;
		}
		timeout_count++;
	} while (timeout_count <= FLUSHFIFO_TIMEOUT);

	return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_FLUSH_FIFO);
}

static tegrabl_error_t qspi_hw_disable_transfer(void)
{
	uint32_t reg_val;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* Disable PIO mode */
	reg_val = qspi_readl(COMMAND);
	if (NV_DRF_VAL(QSPI, COMMAND, PIO, reg_val) == QSPI_COMMAND_0_PIO_PIO) {
		reg_val = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, PIO, STOP,
				reg_val);
		qspi_writel(COMMAND, reg_val);
	}

	/* Disable DMA mode */
	reg_val = qspi_readl(DMA_CTL);
	if (NV_DRF_VAL(QSPI, DMA_CTL, DMA_EN, reg_val) ==
		QSPI_DMA_CTL_0_DMA_EN_ENABLE) {
		reg_val = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, DMA_EN, DISABLE,
				qspi_readl(DMA_CTL));
		qspi_writel(DMA_CTL, reg_val);
	}

	/* Flush Tx fifo */
	err = qspi_hw_flush_fifos(TX_FIFO_FLUSH);

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: Flush tx fifo failed err = %x\n", __func__, err);
		return err;
	}

	/* Flush Rx fifo */
	err = qspi_hw_flush_fifos(RX_FIFO_FLUSH);

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: Flush rx fifo failed err = %x\n", __func__, err);
		return err;
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_hw_check_timeout(uint32_t txfer_start_time_in_us)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (tegrabl_get_timestamp_us() - txfer_start_time_in_us > QSPI_HW_TIMEOUT) {
		/* Check last time before saying timeout. */
		if (!(is_qspi_transfer_completed())) {
			err = qspi_hw_disable_transfer();
			if (err != TEGRABL_NO_ERROR) {
				pr_debug("%s: Fail to disable transfers, err = %x\n",
						 __func__, err);
			}
			return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_CHECK_TIMEOUT);
		}
	}
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t
qspi_check_hw_rdy_or_timeout(uint32_t txfer_start_time_in_us)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	while (!is_qspi_transfer_completed()) {
		err = qspi_hw_check_timeout(txfer_start_time_in_us);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: Timeout err = %x\n", __func__, err);
			return err;
		}
	}
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t
qspi_hw_writein_transmit_fifo(
		uint8_t *p_tx_buff,
		uint32_t words_to_write,
		uint32_t dma_pkt_len,
		uint32_t wrt_strt_tm_in_us)
{
	uint32_t reg;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	while (words_to_write != 0U) {
		/* Read the Status register and find if the Tx fifo is FULL. */
		/* Push data only when tx fifo is not full. */
		reg = qspi_readl(FIFO_STATUS);

		if ((reg & NV_DRF_DEF(QSPI, FIFO_STATUS, TX_FIFO_FULL, FULL)) != 0U) {
			err = qspi_hw_check_timeout(wrt_strt_tm_in_us);
			if (err != TEGRABL_NO_ERROR) {
				/* hw timeout detected */
				break;
			} else {
				continue;
			}
		}

		/* Tx fifo is empty. Now write the data into the fifo,increment the
		 * buffer pointer and decrement the count
		 *
		 * QSPI protocol expects most significant bit of a byte first
		 * i.e. (first)bit7, bit6....bit0 (last)
		 * During QSPI controller initialization LSBi_FE is set to LAST and
		 * LSBy_FE is also set to LAST so that
		 * Data transmitted : (last)[bit24-bit31],[bit16-bit23],[bit8-bit15],
		 * [bit0-bit7] (first) [rightmost bit is transmitted first]
		 *
		 * 32 bits are read from a pointer pointing to UInt8.
		 * E.g.p_tx_buff is pointing to memory address 0x1000 and bytes stored
		 * are
		 * 0x1000 = 0x12
		 * 0x1001 = 0x34
		 * 0x1002 = 0x56
		 * 0x1003 = 0x78
		 * Reading 32 bit from location 0x1000 in little indian format would
		 * read 0x78563412 and this is the data that is being stored in tx fifo
		 * By proper setting of  LSBi_FE and LSBy_FE bits in
		 * command register, bits can be transferred in desired manner
		 * In the example given above 0x12 byte is transmitted first and also
		 * most significant bit gets out first
		 */
		if (dma_pkt_len == BYTES_PER_WORD)
			reg = (*((uint32_t *)p_tx_buff));
		 else
			reg = (uint32_t)(*p_tx_buff);

		qspi_writel(TX_FIFO, reg);

		/* increment buffer pointer */
		p_tx_buff += dma_pkt_len;

		/* decrement requested number of words */
		words_to_write--;
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: err = %x\n", __func__, err);
	}

	return err;
}

static tegrabl_error_t qspi_hw_proc_write_pio(uint8_t *p_write_buffer,
		uint32_t bytes_to_write)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t wrt_strt_tm_in_us;
	uint32_t dma_blk_size;
	uint32_t dma_pkt_len;
	uint32_t reg_val;
	uint32_t words_to_write;

	pr_debug("%s: p_write_buffer = %p, bytes_to_write = %d\n",
			 __func__, p_write_buffer, (uint32_t) bytes_to_write);
	reg_val = qspi_readl(COMMAND);

	if (bytes_to_write % BYTES_PER_WORD != 0) {
		/* Number of bytes to be read is not multiple of 4 bytes
		 * Transfer just 8 bits out of 32 bits of data in the fifo. Rest bits
		 * are ignored in unpacked mode
		 */
		reg_val = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH,
				QSPI_8Bit_BIT_LENGTH, reg_val);
		dma_blk_size = bytes_to_write;
		/* Number of meaningful bytes in one packet = 1 byte */
		dma_pkt_len = 1;
	} else {
		reg_val = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH,
				QSPI_MAX_BIT_LENGTH, reg_val);
		dma_blk_size = bytes_to_write / (BYTES_PER_WORD);
		/* Number of meaningful bytes in one packet = 4 bytes */
		dma_pkt_len = BYTES_PER_WORD;
	}

	words_to_write = dma_blk_size;

	/* Set length of a packet in bits. */
	qspi_writel(COMMAND, reg_val);

	/* Set dma block size.  The DMA Hardware block expects to be programmed
	 * one block less than the intended blocks to be transferred.
	 */
	qspi_writel(DMA_BLK_SIZE, (dma_blk_size - 1));

	/* Enable Tx */
	reg_val = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Tx_EN, ENABLE,
			qspi_readl(COMMAND));
	qspi_writel_flush(COMMAND, reg_val);

	/* Get start time */
	wrt_strt_tm_in_us = tegrabl_get_timestamp_us();

	if (words_to_write != 0U) {
		/* something to be transmitted */
		if (words_to_write > QSPI_FIFO_DEPTH) {
			err = qspi_hw_writein_transmit_fifo(p_write_buffer, QSPI_FIFO_DEPTH,
					dma_pkt_len, wrt_strt_tm_in_us);
			words_to_write = words_to_write - QSPI_FIFO_DEPTH;
		} else {
			err = qspi_hw_writein_transmit_fifo(p_write_buffer, words_to_write,
					dma_pkt_len, wrt_strt_tm_in_us);
			words_to_write = 0;
		}

		if (err == TEGRABL_NO_ERROR) {
			/* Data was written successfully      */
			/* Enable DMA mode for >256B transfer */
			/* It works for <=256B transfer too.  */
			/* Normally, to transfer <= 256B data */
			/* only needs to enable PIO mode      */
			/* NV_FLD_SET_DRF_DEF(QSPI, COMMAND, PIO, PIO, */
			/*                       qspi_readl(COMMAND)); */
			reg_val = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL,
				DMA_EN, ENABLE, qspi_readl(DMA_CTL));
			qspi_writel(DMA_CTL, reg_val);
			if (words_to_write != 0U) {
				/* More data to be written in FIFO */
				/* Since dma is already enabled, just keep filling fifo */
				err = qspi_hw_writein_transmit_fifo(
					p_write_buffer + (QSPI_FIFO_DEPTH * BYTES_PER_WORD),
					words_to_write, dma_pkt_len,
					wrt_strt_tm_in_us);
				if (err != TEGRABL_NO_ERROR) {
					pr_debug("QSPI: pio(>256B) write failed\n");
					err = TEGRABL_ERROR(TEGRABL_ERR_WRITE_FAILED,
							AUX_INFO_WRITE_FAIL);
					goto done;
				}
			}

			/* Make sure spi hw is ready at the end and there is no timeout */
			qspi_check_hw_rdy_or_timeout(wrt_strt_tm_in_us);
		} else {
			pr_debug("QSPI: pio write failed\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_WRITE_FAILED, AUX_INFO_WRITE_FAIL);
			goto done;
		}

	} /* end if (words_to_write) */

done:
	/* Disable Tx */
	reg_val = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Tx_EN, DISABLE,
			qspi_readl(COMMAND));
	qspi_writel(COMMAND, reg_val);

	/* clear the status register */
	qspi_writel_flush(FIFO_STATUS, qspi_readl(FIFO_STATUS));

	return err;
}

static tegrabl_error_t
qspi_read_from_receive_fifo(
	uint8_t *p_rx_buff,
	uint32_t words_or_bytes_to_read,
	uint32_t dma_pkt_len,
	uint32_t read_start_time_in_us)
{
	uint32_t reg_val;
	uint32_t words_count;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	while (words_or_bytes_to_read != 0U) {
		/* Read the Status register and find whether the RX fifo Empty */
		reg_val = qspi_readl(FIFO_STATUS);
		if ((reg_val & NV_DRF_DEF(QSPI, FIFO_STATUS,
				RX_FIFO_EMPTY, EMPTY)) != 0U) {
			err = qspi_hw_check_timeout(read_start_time_in_us);
			if (err != TEGRABL_NO_ERROR)
				/* hw timeout detected */
				break;
			else
				continue;
		}

		/* Rx fifo is found non empty. Read from rx fifo, increment the buffer
		 * pointer and decrement the count
		 *
		 * QSPI protocol expects most significant bit of a byte first
		 * i.e. (first)bit7, bit6....bit0 (last)
		 * During QSPI controller initialization LSBi_FE is set to LAST and
		 * LSBy_FE is also set to LAST so that Data received :
		 * (last) [bit24-bit31], [bit16-bit23], [bit8-bit15],
		 * [bit0-bit7] (first) [rightmost bit is received first]
		 */
		words_count = NV_DRF_VAL(QSPI, FIFO_STATUS, RX_FIFO_FULL_COUNT,
				reg_val);
		words_count = (words_count < words_or_bytes_to_read) ?
			words_count : words_or_bytes_to_read;

		while (words_count != 0U) {
			reg_val = qspi_readl(RX_FIFO);
			if (dma_pkt_len == BYTES_PER_WORD)
				/* All 4 bytes are valid data */
				*((uint32_t *)p_rx_buff) = reg_val;
			else
				/* only 1 byte is valid data */
				(*p_rx_buff) = (uint8_t)(reg_val);

			/* increment buffer pointer */
			p_rx_buff += dma_pkt_len;

			/* decrement requested number of words */
			words_or_bytes_to_read--;
			words_count--;
		}
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: err = %x\n", __func__, err);
	}

	return err;
}

static tegrabl_error_t qspi_hw_proc_write(uint8_t *p_write_buffer,
		uint32_t bytes_to_write)
{
	return qspi_hw_proc_write_pio(p_write_buffer, bytes_to_write);
}

static tegrabl_error_t qspi_hw_proc_read_dma(uint8_t *p_read_buffer,
		uint32_t bytes_to_read)
{
	uint32_t read_start_time_in_us;
	uint32_t dma_blk_size;
	uint32_t packet_count;
	uint32_t read_index = 0;
	uint32_t reg_val;
	uint64_t p_address = 0;
	struct tegrabl_dma_xfer_params params;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t timeout = 0;

	pr_debug("%s: p_read_buffer = %p, bytes_to_read = %d\n",
			 __func__, p_read_buffer, (uint32_t) bytes_to_read);

	reg_val = qspi_readl(COMMAND);
	reg_val = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH,
			QSPI_MAX_BIT_LENGTH, reg_val);
	qspi_writel_flush(COMMAND, reg_val);

	/* Number of meaningful bytes in one packet = 4 bytes */
	packet_count = bytes_to_read / (BYTES_PER_WORD);
	read_index = 0;

	do {
		dma_blk_size = packet_count > 65536 ? 65536 : packet_count;
		packet_count -= dma_blk_size;

		/* Set dma block size.  The DMA Hardware block expects to be programmed
		 * one block less than the intended blocks to be transferred.
		 */
		qspi_writel(DMA_BLK_SIZE, (dma_blk_size - 1));

		/* Check address overflow, DMA is capable of handling only
		 * 32bit addresses */
		p_address = (uintptr_t)p_read_buffer + read_index;

		params.dst = (uintptr_t)p_address;
		params.src = (uintptr_t)NV_ADDRESS_MAP_QSPI_BASE + QSPI_RX_FIFO_0;
		params.size = dma_blk_size * BYTES_PER_WORD;
		params.is_async_xfer = true;
		params.dir = DMA_IO_TO_MEM;
		params.io_bus_width = BUS_WIDTH_32;

		if (s_qspi_context.dma_type == DMA_GPC) {
			params.io = GPCDMA_IO_QSPI;
		} else if (s_qspi_context.dma_type == DMA_BPMP) {
			params.io = DMA_IO_QSPI;
		} else {
			/* No Action Required */
		}

		err = tegrabl_dma_transfer(s_qspi_context.dma_handle, 0, &params);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			pr_debug("QSPI: dma transfer failed\n");
			return err;
		}

		read_index += (dma_blk_size * BYTES_PER_WORD);

		/* Enable Rx */
		reg_val = qspi_readl(COMMAND);
		reg_val = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Rx_EN, ENABLE,
				reg_val);
		qspi_writel_flush(COMMAND, reg_val);

		tegrabl_udelay(2);

		/* Get start time */
		read_start_time_in_us = tegrabl_get_timestamp_us();

		/* Enable DMA mode */
		reg_val = qspi_readl(DMA_CTL);
		reg_val = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, DMA_EN, ENABLE,
				reg_val);
		/* Set  DMA trigger threshold to 8 packet length in QSPI FIFO */
		reg_val = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, RX_TRIG, TRIG8, reg_val);
		qspi_writel_flush(DMA_CTL, reg_val);

		pr_debug("QSPI: polling for dma transfer status\n");
		do {
			tegrabl_udelay(50);
			err = tegrabl_dma_transfer_status(s_qspi_context.dma_handle, 0,
											  &params);
		} while ((timeout++ <= 40000) && (err != TEGRABL_NO_ERROR));

		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			pr_debug("QSPI: dma poll timed out\n");
			return err;
		}
		pr_debug("QSPI: dma complete\n");
		/* Make sure spi hw is ready at the end and there is no timeout */
		err = qspi_check_hw_rdy_or_timeout(read_start_time_in_us);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s QSPI hw timeout! %d:\n", __func__, __LINE__);
			/* qspi_dump_registers(); */
			return err;
		}

		/* Disable Rx */
		reg_val = qspi_readl(COMMAND);
		reg_val = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Rx_EN, DISABLE,
				reg_val);
		qspi_writel(COMMAND, reg_val);

		/* DMA_EN bits get cleared by hw if all the data was tranferred */
		/* successfully, else s/w will disbale it upon */
		/* detection of h/w timeout */
		/* clear the status register */
		qspi_writel_flush(FIFO_STATUS, qspi_readl(FIFO_STATUS));
	} while (packet_count != 0);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_hw_proc_read_pio(uint8_t *p_read_buffer,
		uint32_t bytes_to_read)
{
	uint32_t read_start_time_in_us;
	uint32_t dma_blk_size;
	uint32_t packet_length;
	uint32_t packet_count;
	uint32_t reg_val;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_debug("%s:p_read_buffer = %p, bytes_to_read = %d\n",
			 __func__, p_read_buffer, (uint32_t) bytes_to_read);

	reg_val = qspi_readl(COMMAND);
	if (bytes_to_read % BYTES_PER_WORD != 0) {
		/* Number of bytes to be read is not multiple of 4 bytes */
		reg_val = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH,
				QSPI_8Bit_BIT_LENGTH, reg_val);
		packet_count = bytes_to_read;
		/* Number of meaningful bytes in one packet = 1 byte */
		packet_length = 1;
	} else {
		reg_val = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH,
				QSPI_MAX_BIT_LENGTH, reg_val);
		packet_count = bytes_to_read / (BYTES_PER_WORD);
		/* Number of meaningful bytes in one packet = 4 bytes */
		packet_length = BYTES_PER_WORD;
	}
	qspi_writel(COMMAND, reg_val);

	while (packet_count != 0U) {
		dma_blk_size = packet_count > 65536 ? 65536 : packet_count;
		packet_count -= dma_blk_size;

		/* Set dma block size. */
		qspi_writel(DMA_BLK_SIZE, (dma_blk_size - 1));

		/* Enable Rx */
		reg_val = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Rx_EN, ENABLE,
				qspi_readl(COMMAND));
		qspi_writel(COMMAND, reg_val);

		/* Get start time */
		read_start_time_in_us = tegrabl_get_timestamp_us();

		/* Enable DMA mode */
		reg_val = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, DMA_EN, ENABLE,
				qspi_readl(DMA_CTL));
		qspi_writel(DMA_CTL, reg_val);

		/* Try reading data from fifo
		 * Dma is already enabled so keep reading if rx fifo is non empty
		 * and hw is not timed out.
		 * Assumption is that p_read_buffer is pointing to a buffer
		 * which is large enough to hold requested number of bytes.
		 */
		err = qspi_read_from_receive_fifo(p_read_buffer, dma_blk_size,
				packet_length, read_start_time_in_us);
		if (err != TEGRABL_NO_ERROR) {
			break;
		}
		/* Make sure spi hw is ready at the end and there is no timeout */
		err = qspi_check_hw_rdy_or_timeout(read_start_time_in_us);
		if (err != TEGRABL_NO_ERROR) {
			break;
		}
		p_read_buffer += dma_blk_size * packet_length;
	}

	/* Disable Rx */
	reg_val = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Rx_EN, DISABLE,
			qspi_readl(COMMAND));
	qspi_writel(COMMAND, reg_val);

	/* DMA_EN bits get cleared by hw if all the data was tranferredi */
	/* successfully, else s/w will disbale it upon detection of h/w timeout */
	/* clear the status register */
	qspi_writel_flush(FIFO_STATUS, qspi_readl(FIFO_STATUS));

	return err;
}

static tegrabl_error_t qspi_hw_proc_read(uint8_t *p_read_buffer,
		uint32_t bytes_to_read)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t bytes_to_read_tmp = 0;
	uint32_t residual_bytes = 0;

	if (qspi_params->xfer_mode == QSPI_MODE_DMA) {
		if ((bytes_to_read  >= BYTES_PER_WORD)) {
			if ((bytes_to_read  % BYTES_PER_WORD) != 0) {
				residual_bytes = (bytes_to_read  % BYTES_PER_WORD);
			}
			bytes_to_read_tmp = bytes_to_read  - residual_bytes;
			err = qspi_hw_proc_read_dma(p_read_buffer, bytes_to_read_tmp);
			if (err != TEGRABL_NO_ERROR)
				return err;
		} else {
			residual_bytes = bytes_to_read;
			bytes_to_read_tmp = 0;
		}

		if (residual_bytes  > 0) {
			err = qspi_hw_proc_read_pio((p_read_buffer + bytes_to_read_tmp),
					residual_bytes);
			if (err != TEGRABL_NO_ERROR) {
				return err;
			}
		}
	} else {
		err = qspi_hw_proc_read_pio(p_read_buffer, bytes_to_read);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_qspi_transaction(
		struct tegrabl_qspi_transfer *p_transfers,
		uint8_t no_of_transfers)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_qspi_transfer *p_nxt_transfer = NULL;
	uint32_t transfer_count = 0;
	uint32_t reg;

	if ((p_transfers != NULL) && no_of_transfers)
		p_nxt_transfer = p_transfers;
	else
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_TXFER_ARGS);

	/* Process with individual transfer */
	while (transfer_count < no_of_transfers) {
#if defined(CONFIG_ENABLE_QSPI_QDDR_READ)
		tegrabl_qspi_clk_div_mode((uint32_t)(p_nxt_transfer->op_mode));
#endif
		reg = qspi_readl(COMMAND);

		reg = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, SDR_DDR_SEL,
				p_nxt_transfer->op_mode, reg);

		reg = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, INTERFACE_WIDTH,
				p_nxt_transfer->bus_width, reg);
		/* Set Packed and Unpacked mode */
		reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, PACKED, DISABLE,
				reg);
		/* Number of bits to be transmitted per packet in unpacked mode = 32 */
		reg = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH,
				QSPI_MAX_BIT_LENGTH, reg);

		qspi_writel(COMMAND, reg);

		/* Program Dummy Cycles for TX */
		if (p_nxt_transfer->tx_buf &&
			(p_nxt_transfer->write_len < 6 && transfer_count == 1)) {
			reg = qspi_readl(MISC);
			reg = NV_FLD_SET_DRF_NUM(QSPI, MISC, NUM_OF_DUMMY_CLK_CYCLES,
					p_nxt_transfer->dummy_cycles, reg);
			qspi_writel(MISC, reg);
		}

		/* Set values for first transfer */
		/* set cs sw val to low */
		if (transfer_count == 0) {
			set_qspi_chip_select_level(false);
		}

		/* Send/Rx */
		if (p_nxt_transfer->tx_buf && p_nxt_transfer->write_len) {
			err = qspi_hw_proc_write(p_nxt_transfer->tx_buf,
					p_nxt_transfer->write_len);
		}

		if ((err == TEGRABL_NO_ERROR) && p_nxt_transfer->read_len) {
			err = qspi_hw_proc_read(p_nxt_transfer->rx_buf,
					p_nxt_transfer->read_len);
		}

		if (err != TEGRABL_NO_ERROR) {
			pr_debug("QSPI: transaction failed\n");
			break;
		}
		/* Next transfer */
		p_nxt_transfer++;
		transfer_count++;

		qspi_writel(MISC, 0);
	}
	/* set cs sw val to high */
	set_qspi_chip_select_level(true);

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: err = %x\n", __func__, err);
		return err;
	} else {
		return TEGRABL_NO_ERROR;
	}
}

static tegrabl_error_t configure_qspi_clk(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct qspi_clk_data clk_data;

	switch (qspi_params->clk_src) {
	case 0:
		 /* FIXME: this will removed mb1-bct change is reflected */
		qspi_params->clk_src = TEGRABL_CLK_SRC_PLLP_OUT0;
		break;
	case TEGRABL_CLK_SRC_PLLP_OUT0:
		break;
	case TEGRABL_CLK_SRC_PLLC4_MUXED:
		/* Enable PLLC if it is not enabled already */
		tegrabl_car_init_pll_with_rate(TEGRABL_CLK_PLL_ID_PLLC4, 800000, NULL);
		/* Setting PLLC4_MUXED to 160MHz */
		tegrabl_car_set_clk_src_rate(TEGRABL_CLK_SRC_PLLC4_MUXED,
									 160000, NULL);
		break;
	case TEGRABL_CLK_SRC_CLK_M:
		break;
	default:
		pr_error("Invalid/Not supported src for qspi clk: %"PRIu32"\n",
				  qspi_params->clk_src);
		return TEGRABL_ERR_BAD_PARAMETER;
	}

	clk_data.clk_src = qspi_params->clk_src;
	/* -1 since bct always comes with +1 */
	clk_data.clk_divisor = qspi_params->clk_div - 1;

	err = tegrabl_car_rst_set(TEGRABL_MODULE_QSPI, 0);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_QSPI, 0, &clk_data);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	err = tegrabl_car_rst_clear(TEGRABL_MODULE_QSPI, 0);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	return err;
}

tegrabl_error_t tegrabl_qspi_open(struct tegrabl_mb1bct_qspi_params *params)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* initialize the params */
	qspi_params = params;

	/* check for dma validity */
	if ((qspi_params->dma_type != DMA_BPMP) &&
		(qspi_params->dma_type != DMA_GPC))
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_DMA);

	pr_info("Qspi using %s\n",
			(qspi_params->dma_type == DMA_BPMP) ? "bpmp-dma" : "gpc-dma");
	s_qspi_context.dma_type = qspi_params->dma_type;
	s_qspi_context.dma_handle = tegrabl_dma_request(qspi_params->dma_type);

	err = tegrabl_qspi_finish_init();
	if (err != TEGRABL_NO_ERROR)
		return err;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_qspi_reinit(struct tegrabl_mb1bct_qspi_params *params)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_error_t clk_err = TEGRABL_NO_ERROR;
	/* reinit the structure */
	qspi_params = params;

	/* change dma_type if different */
	if (params->dma_type != s_qspi_context.dma_type) {
		s_qspi_context.dma_type = params->dma_type;
		s_qspi_context.dma_handle = tegrabl_dma_request(params->dma_type);
	}

	err = tegrabl_qspi_finish_init();

	/* Reset QSPI div mode to unknown state after reset module
	 * This must be called after tegrabl_qspi_finish_init */
	clk_err = tegrabl_qspi_clk_div_mode(0xFFFFU);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	} else {
		return clk_err;
	}
}

