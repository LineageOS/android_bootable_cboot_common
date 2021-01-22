/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_SPI

#include <stdint.h>
#include <tegrabl_drf.h>
#include <tegrabl_ar_macro.h>
#include <tegrabl_malloc.h>
#include <tegrabl_clock.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_io.h>
#include <tegrabl_timer.h>
#include <arspi.h>
#include <tegrabl_spi.h>

/* Wrapper macros for reading/writing from/to SPI */
#define spi_readl(hspi, reg) \
		NV_READ32((((uintptr_t)(hspi->base_addr)) + (SPI_##reg##_0)))

#define spi_writel(hspi, reg, value) \
		NV_WRITE32((((uintptr_t)(hspi->base_addr)) + (SPI_##reg##_0)), (value))

/* Disable compiler optimization locally to ensure read after write */
#define spi_writel_flush(hspi, reg, value) \
do { \
	uint32_t reg32 = 0; \
	NV_WRITE32(((uintptr_t)(hspi->base_addr) + (SPI_##reg##_0)), (value)); \
	reg32 = NV_READ32(((uintptr_t)(hspi->base_addr)) + (SPI_##reg##_0)); \
	reg32 = reg32; \
} while (0)

#define SPI_MAX_BIT_LENGTH		31
#define SPI_8Bit_BIT_LENGTH		7
#define SPI_FIFO_DEPTH			64
#define BYTES_PER_WORD			4

/* Read time out of 1 second. */
#define SPI_HW_TIMEOUT			1000000

/* Flush fifo timeout, resolution = 10us*/
 #define FLUSHFIFO_TIMEOUT   10000  /* 10000 x 10us = 100ms */


struct tegrabl_spi spi[TEGRABL_SPI_INSTANCE_COUNT];

static uint32_t spi_addr_map[TEGRABL_SPI_INSTANCE_COUNT] = {
	NV_ADDRESS_MAP_SPI1_BASE,
	NV_ADDRESS_MAP_SPI2_BASE,
	NV_ADDRESS_MAP_SPI3_BASE,
#if defined NV_ADDRESS_MAP_SPI4_BASE
	NV_ADDRESS_MAP_SPI4_BASE,
#endif
};
/* macro aux info */
#define AUX_INFO_FLUSH_FIFO 0
#define AUX_INFO_CHECK_TIMEOUT 1
#define AUX_INFO_WRITE_FAIL 2 /* 0x2 */
#define AUX_INFO_ADDR_OVRFLOW 3
#define AUX_INFO_INVALID_TXFER_ARGS 4
#define AUX_INFO_INVALID_DMA 5 /* 0x5 */

/* macro fifo flush */
typedef uint32_t flush_type_t;
#define TX_FIFO_FLUSH 0
#define RX_FIFO_FLUSH 1

struct tegrabl_spi_prod_setting {
	uint32_t num_settings;
	uint32_t *settings;
};

#define DUMMY_MODES 1

static struct tegrabl_spi_prod_setting
	spi_prod_settings[TEGRABL_SPI_INSTANCE_COUNT][DUMMY_MODES];

tegrabl_error_t tegrabl_spi_register_prod_settings(uint32_t instance,
		uint32_t mode, uint32_t *settings, uint32_t num_settings)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	pr_debug("%s: entry\n", __func__);

	pr_debug("instance = %d, mode = %d, num_settings = %d, settings = %p\n",
		instance, mode, num_settings, settings);

	if (instance >= TEGRABL_SPI_INSTANCE_COUNT ||  mode != 0) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (num_settings && (settings == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	spi_prod_settings[instance][mode].num_settings = num_settings;
	pr_debug("instance = %d, mode = %d, num_settings = %d, settings = %p\n",
		instance, mode, num_settings, settings);
	spi_prod_settings[instance][mode].settings = settings;

fail:
	return error;
}

static bool spi_is_transfer_complete(struct tegrabl_spi *hspi)
{
	/* Read the Status register and findout whether the SPI is busy
	 * Should be called only if rx and/or tx was enabled else ready bit
	 * is 0 by default
	 */
	uint32_t reg_value;
	bool ret = false;

	/* (PIO) &&  !(RDY)) == 1 => ongoing  */
	/* (PIO) &&  !(RDY)) == 0 => complete */

	/* PIO bits are auto clear bits, and it becomes
	 * zero once xfer is complete
	 */

	if	(NV_DRF_VAL(SPI, COMMAND, PIO, spi_readl(hspi, COMMAND)) &&
		(NV_DRF_VAL(SPI, TRANSFER_STATUS, RDY,
		spi_readl(hspi, TRANSFER_STATUS)) == 0)) {
		ret = false;
	} else {
		ret = true;
	}

	/* Clear the Ready bit in else if it is set */
	/* Ready bit is not auto clear bit */
	reg_value = spi_readl(hspi, TRANSFER_STATUS);
	if (reg_value & NV_DRF_DEF(SPI, TRANSFER_STATUS, RDY, READY)) {
		/* Write 1 to RDY bit field to clear it. */
		spi_writel_flush(hspi, TRANSFER_STATUS,
			NV_DRF_DEF(SPI, TRANSFER_STATUS, RDY, READY));
	}
	return ret;
}

static void spi_set_chip_select(struct tegrabl_spi *hspi,
	bool is_level_high)
{
	uint32_t cmd_reg = spi_readl(hspi, COMMAND);
	cmd_reg = NV_FLD_SET_DRF_NUM(SPI, COMMAND, CS_SW_VAL,
			is_level_high, cmd_reg);
	spi_writel_flush(hspi, COMMAND, cmd_reg);
}

static tegrabl_error_t spi_flush_fifos(struct tegrabl_spi *hspi,
	flush_type_t type)
{
	uint32_t status_reg;
	uint32_t timeout_count = 0;
	uint32_t flush_field = 0;

	/* read fifo status */
	status_reg = spi_readl(hspi, FIFO_STATUS);

	switch (type) {
	case TX_FIFO_FLUSH:
		/* return if tx fifo is empty */
		if (NV_DRF_VAL(SPI, FIFO_STATUS, TX_FIFO_EMPTY, status_reg) ==
				 SPI_FIFO_STATUS_0_TX_FIFO_EMPTY_EMPTY)
			return TEGRABL_NO_ERROR;

		flush_field = NV_DRF_DEF(SPI, FIFO_STATUS, TX_FIFO_FLUSH, FLUSH);
		break;

	case RX_FIFO_FLUSH:
		/* return if rx fifo is empty */
		if (NV_DRF_VAL(SPI, FIFO_STATUS, RX_FIFO_EMPTY, status_reg) ==
				 SPI_FIFO_STATUS_0_TX_FIFO_EMPTY_EMPTY)
			return TEGRABL_NO_ERROR;

		flush_field = NV_DRF_DEF(SPI, FIFO_STATUS, RX_FIFO_FLUSH, FLUSH);
		break;

	default:
		return TEGRABL_NO_ERROR;
	}

	/* Write into Status register to clear the FIFOs */
	spi_writel_flush(hspi, FIFO_STATUS, flush_field);

	/* Wait until those bits become 0. */
	do {
		tegrabl_udelay(1);
		status_reg = spi_readl(hspi, FIFO_STATUS);
		if (!(status_reg & flush_field)) {
			return TEGRABL_NO_ERROR;
		}
		timeout_count++;
	} while (timeout_count <= FLUSHFIFO_TIMEOUT);

	return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_FLUSH_FIFO);
}

static tegrabl_error_t spi_disable_transfer(struct tegrabl_spi *hspi)
{
	uint32_t reg_value;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* Disable PIO mode */
	reg_value = spi_readl(hspi, COMMAND);
	if (NV_DRF_VAL(SPI, COMMAND, PIO, reg_value) == SPI_COMMAND_0_PIO_PIO) {
		reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, PIO, STOP,
				reg_value);
		spi_writel(hspi, COMMAND, reg_value);
	}

	/* Flush Tx fifo */
	err = spi_flush_fifos(hspi, TX_FIFO_FLUSH);

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: Flush tx fifo failed err = %x\n", __func__, err);
		return err;
	}

	/* Flush Rx fifo */
	err = spi_flush_fifos(hspi, RX_FIFO_FLUSH);

	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: Flush rx fifo failed err = %x\n", __func__, err);
		return err;
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t spi_chk_timeout(struct tegrabl_spi *hspi,
		uint32_t txfer_start_time_in_us)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (tegrabl_get_timestamp_us() - txfer_start_time_in_us > SPI_HW_TIMEOUT) {
		/* Check last time before saying timeout. */
		if (!(spi_is_transfer_complete(hspi))) {
			err = spi_disable_transfer(hspi);
			if (err != TEGRABL_NO_ERROR) {
				pr_debug("%s: Fail to disable transfers, err = %x\n",
						 __func__, err);
			}
			return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_CHECK_TIMEOUT);
		}
	}
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t spi_chk_hw_rdy_or_timeout(struct tegrabl_spi *hspi,
	uint32_t txfer_start_time_in_us)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	while (!(spi_is_transfer_complete(hspi))) {
		err = spi_chk_timeout(hspi, txfer_start_time_in_us);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("%s: Timeout err = %x\n", __func__, err);
			return err;
		}
	}
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t
spi_write_into_fifo(struct tegrabl_spi *hspi, uint8_t *p_tx_buff,
	uint32_t words_to_write, uint32_t pkt_len, uint32_t wrt_strt_tm_in_us)
{
	uint32_t reg_value;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	while (words_to_write) {

		/* Read the Status register and find if the Tx fifo is FULL. */
		/* Push data only when tx fifo is not full. */
		reg_value = spi_readl(hspi, FIFO_STATUS);

		if (reg_value & NV_DRF_DEF(SPI, FIFO_STATUS, TX_FIFO_FULL, FULL)) {
			err = spi_chk_timeout(hspi, wrt_strt_tm_in_us);
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
		 * SPI protocol expects most significant bit of a byte first
		 * i.e. (first)bit7, bit6....bit0 (last)
		 * During SPI controller initialization LSBi_FE is set to LAST and
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
		 * By proper setting of LSBi_FE and LSBy_FE bits in
		 * command register, bits can be transferred in desired manner
		 * In the example given above 0x12 byte is transmitted first and also
		 * most significant bit gets out first
		 */

		if (pkt_len == BYTES_PER_WORD)
			reg_value = (*((uint32_t *)p_tx_buff));
		 else
			reg_value = (uint32_t)(*p_tx_buff);

		spi_writel(hspi, TX_FIFO, reg_value);

		/* increment buffer pointer */
		p_tx_buff += pkt_len;

		/* decrement requested number of words */
		words_to_write--;
	}

	if (err) {
		pr_debug("%s: err = %x\n", __func__, err);
	}

	return err;
}

static tegrabl_error_t
spi_read_from_fifo(struct tegrabl_spi *hspi, uint8_t *p_rx_buff,
	uint32_t words_or_bytes_to_read, uint32_t pkt_len,
	uint32_t read_start_time_in_us)
{
	uint32_t reg_value;
	uint32_t words_count;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	while (words_or_bytes_to_read) {
		/* Read the Status register and find whether the RX fifo Empty */
		reg_value = spi_readl(hspi, FIFO_STATUS);
		if (reg_value & NV_DRF_DEF(SPI, FIFO_STATUS, RX_FIFO_EMPTY, EMPTY)) {
			err = spi_chk_timeout(hspi, read_start_time_in_us);
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
		words_count = NV_DRF_VAL(SPI, FIFO_STATUS, RX_FIFO_FULL_COUNT,
				reg_value);
		words_count = (words_count < words_or_bytes_to_read) ?
			words_count : words_or_bytes_to_read;

		while (words_count--) {
			reg_value = spi_readl(hspi, RX_FIFO);
			if (pkt_len == BYTES_PER_WORD)
				/* All 4 bytes are valid data */
				*((uint32_t *)p_rx_buff) = reg_value;
			else
				/* only 1 byte is valid data */
				(*p_rx_buff) = (uint8_t)(reg_value);

			/* increment buffer pointer */
			p_rx_buff += pkt_len;

			/* decrement requested number of words */
			words_or_bytes_to_read--;
		}
	}

	if (err) {
		pr_debug("%s: err = %x\n", __func__, err);
	}

	return err;
}

static tegrabl_error_t spi_hw_proc_write_pio(struct tegrabl_spi *hspi,
	uint8_t *p_write_buffer, uint32_t bytes_to_write)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t wrt_strt_tm_in_us;
	uint32_t pkt_len;
	uint32_t reg_value;
	uint32_t words_to_write;
	uint32_t residual_to_write;
	uint8_t *p_address = p_write_buffer;

	pr_debug("%s: p_write_buffer = %p, bytes_to_write = %d\n",
			 __func__, p_write_buffer, (uint32_t) bytes_to_write);

	residual_to_write = bytes_to_write % BYTES_PER_WORD;
	words_to_write = bytes_to_write / BYTES_PER_WORD;

	/* Tx in words */
	while (words_to_write) {
		/* Enable Tx */
		reg_value = spi_readl(hspi, COMMAND);
		reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, Tx_EN, ENABLE, reg_value);
		spi_writel(hspi, COMMAND, reg_value);

		/* Set length of a packet in 32 bits. */
		reg_value = NV_FLD_SET_DRF_NUM(SPI, COMMAND, BIT_LENGTH,
										SPI_MAX_BIT_LENGTH, reg_value);
		spi_writel_flush(hspi, COMMAND, reg_value);

		spi_writel(hspi, DMA_BLK_SIZE, (SPI_FIFO_DEPTH - 1));

		/* Packet by word. */
		pkt_len = BYTES_PER_WORD;

		/* Start to Tx */
		/* Get start time */
		wrt_strt_tm_in_us = tegrabl_get_timestamp_us();

		if (words_to_write > SPI_FIFO_DEPTH) {
			err = spi_write_into_fifo(hspi, p_address,
					SPI_FIFO_DEPTH, pkt_len, wrt_strt_tm_in_us);
			words_to_write -= SPI_FIFO_DEPTH;
			p_address += BYTES_PER_WORD * SPI_FIFO_DEPTH;
		} else {
			err = spi_write_into_fifo(hspi, p_address,
					words_to_write, pkt_len, wrt_strt_tm_in_us);
			p_address += BYTES_PER_WORD * words_to_write;
			words_to_write = 0;
		}

		if (err == TEGRABL_NO_ERROR) {
			/* Data was written successfully */
			/* Enable PIO mode to send*/
			reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, PIO, PIO,
					spi_readl(hspi, COMMAND));
			spi_writel(hspi, COMMAND, reg_value);

			/* Make sure spi hw ready at the end and no timeout */
			spi_chk_hw_rdy_or_timeout(hspi, wrt_strt_tm_in_us);
		} else {
			pr_debug("SPI: pio write failed\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_WRITE_FAILED,
				   AUX_INFO_WRITE_FAIL);
			goto done;
		}
		/* Disable Tx */
		reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, Tx_EN, DISABLE,
				spi_readl(hspi, COMMAND));
		spi_writel_flush(hspi, COMMAND, reg_value);
	}

	/* Tx in bytes */
	while (residual_to_write) {
		/* Enable Tx*/
		reg_value = spi_readl(hspi, COMMAND);
		reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, Tx_EN, ENABLE, reg_value);
		spi_writel(hspi, COMMAND, reg_value);

		/* Set length of a packet in 8 bits. */
		reg_value = NV_FLD_SET_DRF_NUM(SPI, COMMAND, BIT_LENGTH,
										SPI_8Bit_BIT_LENGTH, reg_value);
		spi_writel_flush(hspi, COMMAND, reg_value);

		spi_writel(hspi, DMA_BLK_SIZE, (residual_to_write - 1));

		/* Packet by byte. */
		pkt_len = 1;

		/* Start to Tx */
		/* Get start time */
		wrt_strt_tm_in_us = tegrabl_get_timestamp_us();

		err = spi_write_into_fifo(hspi, p_address, pkt_len,
				residual_to_write, wrt_strt_tm_in_us);
		residual_to_write = 0;

		if (err == TEGRABL_NO_ERROR) {
			/* Data was written successfully */
			/* Enable PIO mode to send */
			reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, PIO, PIO,
					spi_readl(hspi, COMMAND));
			spi_writel(hspi, COMMAND, reg_value);

			/* Make sure spi hw ready at the end and no timeout */
			err = spi_chk_hw_rdy_or_timeout(hspi, wrt_strt_tm_in_us);
			if (err != TEGRABL_NO_ERROR)
				goto done;
		} else {
			pr_debug("SPI: pio write failed\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_WRITE_FAILED,
				   AUX_INFO_WRITE_FAIL);
			goto done;
		}
	}

done:
	/* Disable Tx */
	reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, Tx_EN, DISABLE,
			spi_readl(hspi, COMMAND));
	spi_writel(hspi, COMMAND, reg_value);

	/* clear the status register */
	spi_writel_flush(hspi, FIFO_STATUS, spi_readl(hspi, FIFO_STATUS));

	return err;
}

static tegrabl_error_t spi_hw_proc_read_pio(struct tegrabl_spi *hspi,
	uint8_t *p_read_buffer,	uint32_t bytes_to_read)
{
	uint32_t read_start_time_in_us;
	uint32_t dma_blk_size;
	uint32_t pkt_len;
	uint32_t packet_count;
	uint32_t reg_value;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_debug("%s:p_read_buffer = %p, bytes_to_read = %d\n",
			 __func__, p_read_buffer, (uint32_t) bytes_to_read);

	reg_value = spi_readl(hspi, COMMAND);
	if (bytes_to_read % BYTES_PER_WORD != 0) {
		/* Number of bytes to be read is not multiple of 4 bytes */
		reg_value = NV_FLD_SET_DRF_NUM(SPI, COMMAND, BIT_LENGTH,
				SPI_8Bit_BIT_LENGTH, reg_value);
		packet_count = bytes_to_read;
		/* Number of meaningful bytes in one packet = 1 byte */
		pkt_len = 1;
	} else {
		reg_value = NV_FLD_SET_DRF_NUM(SPI, COMMAND, BIT_LENGTH,
				SPI_MAX_BIT_LENGTH, reg_value);
		packet_count = bytes_to_read / (BYTES_PER_WORD);
		/* Number of meaningful bytes in one packet = 4 bytes */
		pkt_len = BYTES_PER_WORD;
	}
	spi_writel(hspi, COMMAND, reg_value);

	while (packet_count) {
		dma_blk_size = packet_count > 65536 ? 65536 : packet_count;
		packet_count -= dma_blk_size;

		/* Enable Rx */
		reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, Rx_EN, ENABLE,
				spi_readl(hspi, COMMAND));
		spi_writel(hspi, COMMAND, reg_value);
		spi_writel(hspi, DMA_BLK_SIZE, (dma_blk_size - 1));
		/* Get start time */
		read_start_time_in_us = tegrabl_get_timestamp_us();
		/* Enable DMA to start transfer */
		reg_value = spi_readl(hspi, DMA_CTL);
		reg_value =
		    NV_FLD_SET_DRF_DEF(SPI, DMA_CTL, DMA_EN, ENABLE, reg_value);
		spi_writel(hspi, DMA_CTL, reg_value);

		/* Try reading data from fifo
		 * Dma is already enabled so keep reading if rx fifo is non empty
		 * and hw is not timed out.
		 * Assumption is that p_read_buffer is pointing to a buffer
		 * which is large enough to hold requested number of bytes.
		 */
		err = spi_read_from_fifo(hspi, p_read_buffer, dma_blk_size,
				pkt_len, read_start_time_in_us);
		if (err != TEGRABL_NO_ERROR) {
			break;
		}
		/* Make sure spi hw is ready at the end and there is no timeout */
		err = spi_chk_hw_rdy_or_timeout(hspi, read_start_time_in_us);
		if (err != TEGRABL_NO_ERROR) {
			break;
		}
		p_read_buffer += dma_blk_size * pkt_len;
	}

	/* Disable Rx */
	reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, Rx_EN, DISABLE,
			spi_readl(hspi, COMMAND));
	spi_writel(hspi, COMMAND, reg_value);
	/* Disable DMA to stop transfer */
	reg_value = spi_readl(hspi, DMA_CTL);
	reg_value =
	    NV_FLD_SET_DRF_DEF(SPI, DMA_CTL, DMA_EN, DISABLE, reg_value);
	spi_writel(hspi, DMA_CTL, reg_value);
	spi_writel_flush(hspi, FIFO_STATUS, spi_readl(hspi, FIFO_STATUS));

	return err;
}

static tegrabl_error_t spi_hw_proc_write(struct tegrabl_spi *hspi,
		uint8_t *p_write_buffer, uint32_t bytes_to_write)
{
	/* PIO mode has limitatin of reconfigure spi controller every 64 packets */
	/* Currently, spi driver only supports PIO mode */
	/* TODO: DMA mode if packets > 64 */

	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = spi_hw_proc_write_pio(hspi, p_write_buffer, bytes_to_write);
	if (err != TEGRABL_NO_ERROR)
		return err;

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t spi_hw_proc_read(struct tegrabl_spi *hspi,
	uint8_t *p_read_buffer,	uint32_t bytes_to_read)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = spi_hw_proc_read_pio(hspi, p_read_buffer, bytes_to_read);
	if (err != TEGRABL_NO_ERROR)
		return err;
	return TEGRABL_NO_ERROR;
}

struct tegrabl_spi *tegrabl_spi_open(uint8_t instance, uint32_t freq_khz)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_spi *hspi = NULL;
	uint32_t rate_set = 0;
	uint32_t reg_value = 0;
	struct tegrabl_spi_prod_setting *setting;
	uint32_t i;

	hspi = &spi[instance];
	if (hspi == NULL) {
		pr_debug("failed to allocate spi handle\n");
		goto fail;
	}

	hspi->instance = instance;
	hspi->base_addr = spi_addr_map[hspi->instance];
	hspi->freq_khz = freq_khz;

	/* Clock reset */
	err = tegrabl_car_rst_set(TEGRABL_MODULE_SPI, hspi->instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("unable to assert reset to spi instance %u\n",
				hspi->instance);
		goto fail;
	}

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_SPI, instance, NULL);

	/* Set clock source */
	err = tegrabl_car_set_clk_src(TEGRABL_MODULE_SPI, instance,
		TEGRABL_CLK_SRC_PLLP_OUT0);

	/* Set clock actual rate */
	err = tegrabl_car_set_clk_rate(TEGRABL_MODULE_SPI, instance,
		hspi->freq_khz, &rate_set);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("unable to set clk rate to spi instance %u\n",
			hspi->instance);
		goto fail;
	}

	/* Release clock reset */
	err = tegrabl_car_rst_clear(TEGRABL_MODULE_SPI, instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("unable to clear reset to spi instance %u\n",
				hspi->instance);
		goto fail;
	}

	/* Configure spi controller register */
	reg_value = spi_readl(hspi, COMMAND);
	reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, M_S, MASTER, reg_value);
	reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, MODE, Mode0, reg_value);
	reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, CS_SEL, CS0, reg_value);
	reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, CS_POL_INACTIVE0, DEFAULT,
		reg_value);
	reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, CS_SW_HW, SOFTWARE, reg_value);
	reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, CS_SW_VAL, HIGH, reg_value);
	spi_writel(hspi, COMMAND, reg_value);

	setting = &spi_prod_settings[instance][0];

	if (setting->num_settings) {
		/* Apply prod settings using <addr, mask, value> tuple */
		pr_debug("apply spi controller prod settings\n");
		for (i = 0; i < (setting->num_settings * 3); i += 3) {
			pr_debug("settings = %x\n", setting->settings[i]);
			reg_value = NV_READ32((uintptr_t)(hspi->base_addr +
				setting->settings[i]));
			reg_value &= (~setting->settings[i + 1]);
			reg_value |= (setting->settings[i + 2] & setting->settings[i + 1]);
			NV_WRITE32(((uintptr_t)(hspi->base_addr + setting->settings[i])),
				reg_value);
			pr_debug("reg_value = %x\n", reg_value);
		}
	}

	/* Flush Tx fifo */
	err = spi_flush_fifos(hspi, TX_FIFO_FLUSH);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("SPI: failed to flush tx fifo\n");
		goto fail;
	}

	/* Flush Rx fifo */
	err = spi_flush_fifos(hspi, RX_FIFO_FLUSH);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("SPI: failed to flush rx fifo\n");
		goto fail;
	}

fail:
	if (err != TEGRABL_NO_ERROR) {
		hspi = NULL;
	}

	return hspi;
}

tegrabl_error_t tegrabl_spi_transaction(struct tegrabl_spi *hspi,
	struct tegrabl_spi_transfer *p_transfers, uint8_t no_of_transfers)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_spi_transfer *p_nxt_transfer = NULL;
	uint32_t transfer_count = 0;
	uint32_t reg_value;

	if ((p_transfers != NULL) && no_of_transfers)
		p_nxt_transfer = p_transfers;
	else
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_TXFER_ARGS);

	/* Process with individual transfer */
	while (transfer_count < no_of_transfers) {
		reg_value = spi_readl(hspi, COMMAND);

		/* Set Packed and Unpacked mode */
		reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, PACKED, DISABLE,
				reg_value);
		/* Number of bits to be transmitted per packet in unpacked mode = 32 */
		reg_value = NV_FLD_SET_DRF_NUM(SPI, COMMAND, BIT_LENGTH,
				SPI_MAX_BIT_LENGTH, reg_value);

		spi_writel(hspi, COMMAND, reg_value);

		/* Set values for first transfer */
		/* set cs sw val to low */
		if (transfer_count == 0) {
			spi_set_chip_select(hspi, false);
		}

		/* Tx/Rx */
		if (p_nxt_transfer->tx_buf && p_nxt_transfer->write_len) {
			err = spi_hw_proc_write(hspi, p_nxt_transfer->tx_buf,
					p_nxt_transfer->write_len);
		}
		if ((err == TEGRABL_NO_ERROR) && p_nxt_transfer->read_len) {
			err = spi_hw_proc_read(hspi, p_nxt_transfer->rx_buf,
					p_nxt_transfer->read_len);
		}
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("SPI: transaction failed\n");
			break;
		}

		/* Next transfer */
		p_nxt_transfer++;
		transfer_count++;

		spi_writel(hspi, MISC, 0);
	}
	/* set cs sw val to high */
	spi_set_chip_select(hspi, true);

	if (err) {
		pr_debug("%s: err = %x\n", __func__, err);
		return err;
	} else {
		return TEGRABL_NO_ERROR;
	}
}

tegrabl_error_t tegrabl_spi_ioctl(struct tegrabl_spi *hspi,
	tegrabl_spi_ioctl_t ioctl, void *args)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t reg_value;
	struct tegrabl_spi_endianess *endian;
	uint32_t mode;
	uint32_t freq_khz;
	uint32_t rate_set;

	pr_debug("%s: ioctl = %d\n", __func__, ioctl);

	switch (ioctl) {
	case SPI_IOCTL_SET_ENDIANESS:
		endian = (struct tegrabl_spi_endianess *) args;

		reg_value = spi_readl(hspi, COMMAND);
		/* LSBy_FE */
		if (endian->is_lsbyte_first == true)
			reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, En_LE_Byte, FIRST,
				reg_value);
		else
			reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, En_LE_Byte, LAST,
				reg_value);

		/* LSBi_FE */
		if (endian->is_lsbit_first == true)
			reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, En_LE_Bit, FIRST,
				reg_value);
		else
			reg_value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, En_LE_Byte, LAST,
				reg_value);
		spi_writel(hspi, COMMAND, reg_value);
		break;

	case SPI_IOCTL_SET_MODE:
		mode = *(uint32_t *) args;

		if (mode > SPI_MODE3) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			break;
		}
		reg_value = spi_readl(hspi, COMMAND);
		reg_value = NV_FLD_SET_DRF_NUM(SPI, COMMAND, MODE, mode, reg_value);
		spi_writel(hspi, COMMAND, reg_value);
		break;

	case SPI_IOCTL_SET_FREQ:
		freq_khz = *(uint32_t *) args;

		/* Set clock actual rate */
		err = tegrabl_car_set_clk_rate(TEGRABL_MODULE_SPI, hspi->instance,
			freq_khz, &rate_set);
		if (err != TEGRABL_NO_ERROR) {
			pr_debug("unable to set clk rate to spi instance %u\n",
				hspi->instance);
		}
		break;
	}
	return err;
}

tegrabl_error_t tegrabl_spi_close(struct tegrabl_spi *hspi)
{
	if (hspi == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
	}
	return TEGRABL_NO_ERROR;
}

