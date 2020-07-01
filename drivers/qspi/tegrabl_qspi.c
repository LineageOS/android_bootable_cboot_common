/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <string.h>
#include <tegrabl_ar_macro.h>
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
#include <tegrabl_qspi_soc_common.h>
#include <tegrabl_io.h>

#ifndef NV_ADDRESS_MAP_QSPI_BASE
#define NV_ADDRESS_MAP_QSPI_BASE NV_ADDRESS_MAP_QSPI0_BASE
#endif

#ifndef NV_ADDRESS_MAP_QSPI0_BASE
#define NV_ADDRESS_MAP_QSPI0_BASE NV_ADDRESS_MAP_QSPI_BASE
#endif

#ifndef NV_ADDRESS_MAP_QSPI1_BASE
#define NV_ADDRESS_MAP_QSPI1_BASE NV_ADDRESS_MAP_QSPI_BASE
#endif

/* Flush fifo timeout, resolution = 10us */
#define FLUSHFIFO_TIMEOUT	10000U  /* 10000 x 10us = 100ms */

#define QSPI_DMA_BLK_SIZE_MAX	65536UL

#define QSPI_GLOBAL_CONFIG_0_CMB_SEQ_EN_ENABLE		1
#define QSPI_GLOBAL_CONFIG_0_CMB_SEQ_EN_DISABLE		0

#define QSPI_DMA_THRESOLD				16UL

/* Maximum Instance supported */
#define AUX_INFO_FLUSH_FIFO 0
#define AUX_INFO_FILL_TX_FIFO 1
#define AUX_INFO_FILL_RX_FIFO 2 /* 0x2 */
#define AUX_INFO_XFER_TX_PIO 3
#define AUX_INFO_XFER_RX_PIO 4
#define AUX_INFO_XFER_TX_DMA 5 /* 0x5 */
#define AUX_INFO_XFER_RX_DMA 6
#define AUX_INFO_CHECK_TIMEOUT 7
#define AUX_INFO_WRITE_FAIL 8 /* 0x8 */
#define AUX_INFO_ADDR_OVRFLOW 9
#define AUX_INFO_INVALID_TXFER_ARGS 10 /* 0xA */
#define AUX_INFO_INVALID_DMA 11
#define AUX_INFO_ALREADY_OPEN 12
#define AUX_INFO_CLOCK_CONFIG 13
#define AUX_INFO_OPEN_PARAMETER 14
#define AUX_INFO_OP_CODE 15 /* 0xF */

typedef uint32_t flush_type_t;
#define TX_FIFO_FLUSH 0
#define RX_FIFO_FLUSH 1


/* Do not use dma channel 0. It is protected and can be accessed by secure OS only. */
static struct tegrabl_qspi_info s_qspi_info[QSPI_MAX_INSTANCE] = {
	{
		.base_address = NV_ADDRESS_MAP_QSPI0_BASE,
		.instance_id = 0U,
		.dma_chan_id = 1U,
		.open_count = 0U,
		.gpcdma_req = GPCDMA_IO_QSPI0,
		.bpmpdma_req = DMA_IO_QSPI0,
	}, {
		.base_address = NV_ADDRESS_MAP_QSPI1_BASE,
		.instance_id = 1U,
		.dma_chan_id = 2U,
		.open_count = 0U,
		.gpcdma_req = GPCDMA_IO_QSPI1,
		.bpmpdma_req = DMA_IO_QSPI1,
	},
};

static struct tegrabl_qspi_platform_params s_qspi_params[QSPI_MAX_INSTANCE];
static struct tegrabl_qspi_device_property s_qspi_device[QSPI_MAX_INSTANCE];
static struct tegrabl_qspi_handle s_qspi_handle[QSPI_MAX_INSTANCE];

/* dump debug registers */
/*
void qspi_dump_registers(struct tegrabl_qspi_info *qspi)
{
	uint32_t reg32 = 0;

	pr_debug("**************QSPI registers ***************\n");
	reg32 = qspi_readl(qspi, COMMAND);
	pr_debug("command = %x\n", reg32);
	reg32 = qspi_readl(qspi, COMMAND2);
	pr_debug("command2 = %x\n", reg32);
	reg32 = qspi_readl(qspi, TIMING_REG1);
	pr_debug("timingreg1 = %x\n", reg32);
	reg32 = qspi_readl(qspi, TIMING_REG2);
	pr_debug("timingreg2 = %x\n", reg32);
	reg32 = qspi_readl(qspi, TRANSFER_STATUS);
	pr_debug("transferstatus = %x\n", reg32);
	reg32 = qspi_readl(qspi, FIFO_STATUS);
	pr_debug("fifostatus = %x\n", reg32);
	reg32 = qspi_readl(qspi, DMA_CTL);
	pr_debug("dmactl = %x\n", reg32);
	reg32 = qspi_readl(qspi, DMA_BLK_SIZE);
	pr_debug("dma_blk_size = %x\n", reg32);
	reg32 = qspi_readl(qspi, INTR_MASK);
	pr_debug("intr_mask = %x\n", reg32);
	reg32 = qspi_readl(qspi, SPARE_CTLR);
	pr_debug("spare_ctlr = %x\n", reg32);
	reg32 = qspi_readl(qspi, MISC);
	pr_debug("misc = %x\n", reg32);
	reg32 = qspi_readl(qspi, TIMING3);
	pr_debug("timing3 = %x\n", reg32);
}
*/

static tegrabl_error_t configure_qspi_clk(struct tegrabl_qspi_info *qspi,
						struct tegrabl_qspi_platform_params *qparams,
						struct tegrabl_qspi_handle *qhandle)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct qspi_clk_data clk_data;
	tegrabl_clk_pll_id_t pll_id;
	uint32_t clk_div = qparams->clk_div;
	uint32_t rate = 0U;
	uint32_t flag = 0U;
	bool is_pllc4 = false;
	switch (qparams->clk_src) {
	case 0:
		/* handling this case to keep it backward compatible */
		clk_data.clk_src = TEGRABL_CLK_SRC_PLLP_OUT0;
		break;

	case TEGRABL_CLK_SRC_PLLP_OUT0:
	case TEGRABL_CLK_SRC_CLK_M:
		clk_data.clk_src = (uint8_t)qparams->clk_src;
		break;

	case TEGRABL_CLK_SRC_PLLC_OUT0:
		pll_id = TEGRABL_CLK_PLL_ID_PLLC;
		rate = 800000U;
		break;

	case TEGRABL_CLK_SRC_PLLC2_OUT0:
		pll_id = TEGRABL_CLK_PLL_ID_PLLC2;
		rate = 645000U;
		break;

	case TEGRABL_CLK_SRC_PLLC3_OUT0:
		pll_id = TEGRABL_CLK_PLL_ID_PLLC3;
		rate = 800000U;
		break;

	case TEGRABL_CLK_SRC_PLLC4_MUXED:
		pll_id = TEGRABL_CLK_PLL_ID_PLLC4;
		rate = 800000U;
		is_pllc4 = true;
		break;

	default:
		pr_error("Invalid/Not supported src for qspi clk: %"PRIu32"\n",
				 qparams->clk_src);
		flag++;
		break;
	}

	if (flag != 0U) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, AUX_INFO_CLOCK_CONFIG);
	}

	if (rate != 0U) {
		if (qparams->clk_src_freq != 0U) {
			rate = qparams->clk_src_freq;
		}

		/* Enable PLL if it is not enabled already */
		err = tegrabl_car_init_pll_with_rate(pll_id, rate, NULL);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("PLL initialisation failed: 0x%08x\n", err);
			return err;
		}

		/* Setting clk_src for pllc4 */
		if (is_pllc4 == true) {
			err = tegrabl_car_set_clk_src_rate((uint8_t)qparams->clk_src,
											qparams->clk_src_freq / 1000U, NULL);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("QSPI clock set rate failed: 0x%08x\n", err);
				return err;
			}
		}
		clk_data.clk_src = (uint8_t)qparams->clk_src;
	}

	if (clk_div == 0U) {
		if ((qparams->clk_src_freq == 0U) ||
			 (qparams->interface_freq == 0U) ||
			  (qparams->clk_src_freq < qparams->interface_freq)) {
			pr_error("Invalid source(%u) and interface(%u) freqeuncy\n",
					 qparams->clk_src_freq, qparams->interface_freq);
			return TEGRABL_ERR_BAD_PARAMETER;
		}
		clk_div = (qparams->clk_src_freq * 2U) / qparams->interface_freq;
		clk_div -= 1U;
		qparams->clk_div = clk_div;
	}

	/* -1 since bct always comes with +1 */
	clk_data.clk_divisor = clk_div - 1U;
	err = tegrabl_car_rst_set(TEGRABL_MODULE_QSPI, qspi->instance_id);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("QSPI%d: Failed to set reset: 0x%08x\n",
				 qspi->instance_id, err);
		return err;
	}

	err = tegrabl_car_clk_enable(TEGRABL_MODULE_QSPI, qspi->instance_id,
								 &clk_data);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("QSPI%d: Failed to clock enable: 0x%08x\n",
				 qspi->instance_id, err);
		return err;
	}

	err = tegrabl_car_rst_clear(TEGRABL_MODULE_QSPI, qspi->instance_id);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("QSPI%d: Failed to clear reset: 0x%08x\n",
				 qspi->instance_id, err);
		return err;
	}

	qhandle->curr_op_mode = SDR_MODE;
	(void)tegrabl_qspi_sdr_enable(qspi->instance_id);
	return err;
}

static tegrabl_error_t qspi_hw_flush_fifos(struct tegrabl_qspi_info *qspi,
													flush_type_t type)
{
	uint32_t status_reg;
	uint32_t timeout_count = 0;
	uint32_t flush_field = 0;
	uint32_t flag = 0U;
	/* read fifo status */
	status_reg = qspi_readl(qspi, FIFO_STATUS);

	switch (type) {
	case TX_FIFO_FLUSH:
		/* return if tx fifo is empty */
		if (NV_DRF_VAL(QSPI, FIFO_STATUS, TX_FIFO_EMPTY, status_reg) ==
				 QSPI_FIFO_STATUS_0_TX_FIFO_EMPTY_EMPTY) {
			return TEGRABL_NO_ERROR;
		}
		flush_field = NV_DRF_DEF(QSPI, FIFO_STATUS, TX_FIFO_FLUSH, FLUSH);
		break;

	case RX_FIFO_FLUSH:
		/* return if rx fifo is empty */
		if (NV_DRF_VAL(QSPI, FIFO_STATUS, RX_FIFO_EMPTY, status_reg) ==
				 QSPI_FIFO_STATUS_0_RX_FIFO_EMPTY_EMPTY) {
			return TEGRABL_NO_ERROR;
		}
		flush_field = NV_DRF_DEF(QSPI, FIFO_STATUS, RX_FIFO_FLUSH, FLUSH);
		break;

	default:
		flag++;
		break;

	}
	if (flag != 0U) {
		return TEGRABL_NO_ERROR;
	}

	/* Write in to Status register to clear the FIFOs */
	qspi_writel_flush(qspi, FIFO_STATUS, flush_field);

	/* Wait until those bits become 0. */
	do {
		tegrabl_udelay(1);
		status_reg = qspi_readl(qspi, FIFO_STATUS);
		if ((status_reg & flush_field) == 0U) {
			return TEGRABL_NO_ERROR;
		}
		timeout_count++;
	} while (timeout_count <= FLUSHFIFO_TIMEOUT);

	return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_FLUSH_FIFO);
}

static tegrabl_error_t qspi_controller_init(struct tegrabl_qspi_info *qspi,
				struct tegrabl_qspi_platform_params *qparams,
				struct tegrabl_qspi_device_property *qdev,
				struct tegrabl_qspi_handle *qhandle)
{
	uint32_t reg;
	tegrabl_error_t err;

	/* Reconfigure the clock */
	err = configure_qspi_clk(qspi, qparams, qhandle);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	/* Configure initial settings */
	reg = qspi_readl(qspi, COMMAND);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, M_S, MASTER, reg);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, MODE, Mode0, reg);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, CS_SEL, CS0, reg);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, CS_POL_INACTIVE0, DEFAULT, reg);
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, CS_SW_HW, SOFTWARE, reg);
	/* Look at QSPI Spansion/Micron devices' data sheet.
	 * CS pin of flash device is active low.
	 * To rx/tx, transition CS from high to low, send/rx,
	 * transition CS from low to high
	 */
	if (qdev->cs_active_low == true) {
		reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, CS_SW_VAL, HIGH, reg);
	} else {
		reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, CS_SW_VAL, LOW, reg);
	}

	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, IDLE_SDA, DRIVE_LOW, reg);

	qspi_writel(qspi, COMMAND, reg);

	/* Flush Tx fifo */
	err = qspi_hw_flush_fifos(qspi, TX_FIFO_FLUSH);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("QSPI: failed to flush tx fifo\n");
		return err;
	}

	/* Flush Rx fifo */
	err = qspi_hw_flush_fifos(qspi, RX_FIFO_FLUSH);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("QSPI: failed to flush rx fifo\n");
		return err;
	}

	pr_trace("tx_clk_tap_delay : %u\n", qparams->trimmer1_val);
	pr_trace("rx_clk_tap_delay : %u\n", qparams->trimmer2_val);

	/* Program trimmer values based on params */
	reg =
	NV_DRF_NUM(QSPI, COMMAND2, Tx_Clk_TAP_DELAY, qparams->trimmer1_val) |
	NV_DRF_NUM(QSPI, COMMAND2, Rx_Clk_TAP_DELAY, qparams->trimmer2_val);
	qspi_writel_flush(qspi, COMMAND2, reg);

	return TEGRABL_NO_ERROR;
}

static void qspi_set_chip_select_level(struct tegrabl_qspi_info *qspi,
						struct tegrabl_qspi_device_property *qdev,
						bool is_active)
{
	/* Is effective only when SW CS is being used */
	uint32_t cmd_reg = qspi_readl(qspi, COMMAND);
	bool is_level_high;

	if (is_active == true) {
		is_level_high = !qdev->cs_active_low;
	} else {
		is_level_high = qdev->cs_active_low;
	}

	cmd_reg = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, CS_SW_VAL, is_level_high ? 1 : 0, cmd_reg);
	qspi_writel_flush(qspi, COMMAND, cmd_reg);
}

static tegrabl_error_t qspi_hw_disable_transfer(struct tegrabl_qspi_info *qspi)
{
	uint32_t reg_val;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* Disable PIO mode */
	reg_val = qspi_readl(qspi, COMMAND);
	if (NV_DRF_VAL(QSPI, COMMAND, PIO, reg_val) == QSPI_COMMAND_0_PIO_PIO) {
		reg_val = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, PIO, STOP,
				reg_val);
		qspi_writel(qspi, COMMAND, reg_val);
	}

	/* Disable DMA mode */
	reg_val = qspi_readl(qspi, DMA_CTL);
	if (NV_DRF_VAL(QSPI, DMA_CTL, DMA_EN, reg_val) ==
		QSPI_DMA_CTL_0_DMA_EN_ENABLE) {
		reg_val = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, DMA_EN, DISABLE,
				qspi_readl(qspi, DMA_CTL));
		qspi_writel(qspi, DMA_CTL, reg_val);
	}

	/* Flush Tx fifo */
	err = qspi_hw_flush_fifos(qspi, TX_FIFO_FLUSH);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Flush tx fifo failed err = %x\n", err);
		return err;
	}

	/* Flush Rx fifo */
	err = qspi_hw_flush_fifos(qspi, RX_FIFO_FLUSH);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Flush rx fifo failed err = %x\n", err);
		return err;
	}

	return TEGRABL_NO_ERROR;
}

static void qspi_clear_ready_bit(struct tegrabl_qspi_info *qspi)
{
	uint32_t rval;

	/* Clear the Ready bit in else if it is set */
	/* Ready bit is not auto clear bit */
	rval = qspi_readl(qspi, TRANSFER_STATUS);
	if (rval & NV_DRF_DEF(QSPI, TRANSFER_STATUS, RDY, READY)) {
		/* Write 1 to RDY bit field to clear it. */
		qspi_writel_flush(qspi, TRANSFER_STATUS,
						  NV_DRF_DEF(QSPI, TRANSFER_STATUS, RDY, READY));
	}
}

static tegrabl_error_t qspi_wait_for_ready_bit(
				struct tegrabl_qspi_handle *qspi_handle)
{
	uint32_t st_time = qspi_handle->xfer_start_time;
	uint32_t elapsed_time;
	uint32_t rval;

	do {
		rval = qspi_readl(qspi_handle->qspi, TRANSFER_STATUS);
		if ((rval & NV_DRF_DEF(QSPI, TRANSFER_STATUS, RDY, READY)) != 0U) {
			qspi_writel_flush(qspi_handle->qspi, TRANSFER_STATUS,
							  NV_DRF_DEF(QSPI, TRANSFER_STATUS, RDY, READY));
			return TEGRABL_NO_ERROR;
		}

		elapsed_time = (uint32_t)(tegrabl_get_timestamp_us() - st_time);
	} while (elapsed_time < qspi_handle->xfer_timeout);

	return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_CHECK_TIMEOUT);
}

static tegrabl_error_t qspi_wait_for_tx_fifo_empty(
				struct tegrabl_qspi_handle *qspi_handle)
{
	uint32_t st_time = qspi_handle->xfer_start_time;
	uint32_t elapsed_time;
	uint32_t rval = 0U;

	do {
		rval = qspi_readl(qspi_handle->qspi, FIFO_STATUS);
		if ((rval & NV_DRF_DEF(QSPI, FIFO_STATUS, TX_FIFO_FULL, FULL)) == 0U) {
			return TEGRABL_NO_ERROR;
		}

		elapsed_time = (uint32_t)(tegrabl_get_timestamp_us() - st_time);
	} while (elapsed_time < qspi_handle->xfer_timeout);

	return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_CHECK_TIMEOUT);
}

static tegrabl_error_t qspi_wait_for_tx_xfer_complete(
				struct tegrabl_qspi_handle *qspi_handle)
{
	tegrabl_error_t err;
	uint32_t st_time = qspi_handle->xfer_start_time;
	uint32_t elapsed_time;
	uint32_t rval;

	err = qspi_wait_for_ready_bit(qspi_handle);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	/* Check for Fifo empty */
	do {
		rval = qspi_readl(qspi_handle->qspi, FIFO_STATUS);
		if ((rval & NV_DRF_DEF(QSPI, FIFO_STATUS,
						TX_FIFO_EMPTY, EMPTY)) != 0U) {
			return TEGRABL_NO_ERROR;
		}

		elapsed_time = (uint32_t)(tegrabl_get_timestamp_us() - st_time);
	} while (elapsed_time < qspi_handle->xfer_timeout);

	return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_CHECK_TIMEOUT);
}

static void qspi_stop_tx(struct tegrabl_qspi_info *qspi)
{
	uint32_t rval;

	/* Disable Tx */
	rval = qspi_readl(qspi, COMMAND);
	rval = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Tx_EN, DISABLE, rval);
	qspi_writel(qspi, COMMAND, rval);

	/* clear the status register */
	rval = qspi_readl(qspi, FIFO_STATUS);
	qspi_writel_flush(qspi, FIFO_STATUS, rval);

	qspi_clear_ready_bit(qspi);
}

static void qspi_abort_tx_pio(struct tegrabl_qspi_info *qspi)
{
	qspi_stop_tx(qspi);
	qspi_hw_disable_transfer(qspi);
}

static void qspi_abort_tx_dma(struct tegrabl_qspi_handle *qspi_handle)
{
	struct tegrabl_qspi_ctxt *qspi_context = &qspi_handle->qspi_context;

	qspi_stop_tx(qspi_handle->qspi);
	tegrabl_dma_transfer_abort(qspi_context->dma_handle,
							   qspi_handle->qspi->dma_chan_id);
	qspi_hw_disable_transfer(qspi_handle->qspi);
}

static uint32_t qspi_fill_tx_fifo(struct tegrabl_qspi_handle *qspi_handle)
{
	uint32_t rval;
	uint32_t count = 0;

	while (qspi_handle->cur_remain_pio_packet != 0U) {
		/**
		 * Read the Status register and find if the Tx fifo is FULL.
		 * Push data only when tx fifo is not full.
		 */
		rval = qspi_readl(qspi_handle->qspi, FIFO_STATUS);
		if ((rval & NV_DRF_DEF(QSPI, FIFO_STATUS, TX_FIFO_FULL, FULL)) != 0U) {
			return count;
		}

		if (qspi_handle->bytes_pw > 1U) {
			rval = (*((uint32_t *)qspi_handle->cur_buf));
		} else {
			rval = (uint32_t)(*qspi_handle->cur_buf);
		}

		qspi_writel(qspi_handle->qspi, TX_FIFO, rval);
		count++;

		/* increment buffer pointer */
		qspi_handle->cur_buf += qspi_handle->bytes_pw;

		/* decrement requested number of words */
		qspi_handle->cur_remain_pio_packet--;
	}

	return count;
}

static tegrabl_error_t qspi_send_start_one_xfer_pio(
						struct tegrabl_qspi_handle *qspi_handle)
{
	uint32_t dma_blk_size;
	uint32_t words_written;
	uint32_t rval;

	qspi_handle->cur_xfer_is_dma = false;

	/* Number of bits to be transmitted per packet in unpacked mode = 32 */
	rval = qspi_readl(qspi_handle->qspi, COMMAND);
	rval = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH,
							  (qspi_handle->bits_pw - 1UL), rval);
	qspi_writel(qspi_handle->qspi, COMMAND, rval);

	dma_blk_size = MIN(qspi_handle->ramain_pio_packet, QSPI_DMA_BLK_SIZE_MAX);

	/* Set dma block size. The DMA Hardware block expects to be programmed
	 * one block less than the intended blocks to be transferred.
	 */
	qspi_writel(qspi_handle->qspi, DMA_BLK_SIZE, (dma_blk_size - 1U));

	qspi_handle->cur_req_pio_packet = dma_blk_size;
	qspi_handle->cur_remain_pio_packet = dma_blk_size;

	/* Enable Tx */
	rval = qspi_readl(qspi_handle->qspi, COMMAND);
	rval = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Tx_EN, ENABLE, rval);
	qspi_writel_flush(qspi_handle->qspi, COMMAND, rval);

	words_written = qspi_fill_tx_fifo(qspi_handle);
	if (words_written == 0U) {
		pr_error("Number of words written in TX FIFO is 0\n");
		qspi_abort_tx_pio(qspi_handle->qspi);
		return TEGRABL_ERROR(TEGRABL_ERR_WRITE_FAILED, AUX_INFO_FILL_TX_FIFO);
	}

	/* Data was written successfully, Enable PIO mode */
	rval = qspi_readl(qspi_handle->qspi, DMA_CTL);
	rval = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, DMA_EN, ENABLE, rval);
	qspi_writel(qspi_handle->qspi, DMA_CTL, rval);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_send_wait_one_xfer_pio(
			struct tegrabl_qspi_handle *qspi_handle, bool is_abort)
{
	tegrabl_error_t err;
	uint32_t words_written;
	uint32_t rval;

continue_wait:
	err = qspi_wait_for_tx_fifo_empty(qspi_handle);
	if (err != TEGRABL_NO_ERROR) {
		rval = qspi_readl(qspi_handle->qspi, COMMAND);
		pr_error("Tx fifo is not becoming empty, comand 0x%08x\n", rval);
		if (is_abort == true) {
			qspi_abort_tx_pio(qspi_handle->qspi);
			return err;
		}

		return TEGRABL_ERROR(TEGRABL_ERR_XFER_IN_PROGRESS,
							 AUX_INFO_XFER_TX_PIO);
	}

	if (qspi_handle->cur_remain_pio_packet != 0U) {
		words_written = qspi_fill_tx_fifo(qspi_handle);
		if (words_written == 0U) {
			qspi_abort_tx_pio(qspi_handle->qspi);
			return TEGRABL_ERROR(TEGRABL_ERR_WRITE_FAILED,
								 AUX_INFO_FILL_TX_FIFO);
		}
		goto continue_wait;
	}

	qspi_handle->ramain_pio_packet -= qspi_handle->cur_req_pio_packet;
	if (qspi_handle->ramain_pio_packet != 0U) {
		qspi_clear_ready_bit(qspi_handle->qspi);
		err = qspi_send_start_one_xfer_pio(qspi_handle);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}
		goto continue_wait;
	}

	qspi_stop_tx(qspi_handle->qspi);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_send_start_one_xfer_dma(
					struct tegrabl_qspi_handle *qspi_handle)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_qspi_info *qspi = qspi_handle->qspi;
	struct tegrabl_qspi_ctxt *qspi_context = &qspi_handle->qspi_context;
	uint32_t dma_blk_size;
	uint32_t rval;

	qspi_handle->cur_xfer_is_dma = true;

	dma_blk_size = MIN(qspi_handle->ramain_dma_packet, qspi_handle->qspi_info->dma_max_size);

	/* Number of bits to be transmitted per packet in unpacked mode = 32 */
	rval = qspi_readl(qspi_handle->qspi, COMMAND);
	rval = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH, 31, rval);
	qspi_writel(qspi, COMMAND, rval);

	/* Set dma block size. The DMA Hardware block expects to be programmed
	 * one block less than the intended blocks to be transferred.
	 */
	qspi_writel(qspi_handle->qspi, DMA_BLK_SIZE, (dma_blk_size - 1U));

	qspi_handle->cur_req_dma_packet = dma_blk_size;
	qspi_handle->cur_remain_dma_packet = dma_blk_size;

	/* Enable Tx */
	rval = qspi_readl(qspi_handle->qspi, COMMAND);
	rval = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Tx_EN, ENABLE, rval);
	qspi_writel_flush(qspi_handle->qspi, COMMAND, rval);

	/* Set  DMA trigger threshold to 8 packet length in QSPI FIFO */
	rval = qspi_readl(qspi, DMA_CTL);
	if (qspi_handle->qspi_info[0].trig_len == 16U) {
		rval = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, TX_TRIG, TRIG16, rval);
	} else if (qspi_handle->qspi_info[0].trig_len == 8U) {
		rval = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, TX_TRIG, TRIG8, rval);
	}
	qspi_writel_flush(qspi, DMA_CTL, rval);

	qspi_handle->dma_params.dst = qspi_handle->qspi->base_address +
										(uint32_t)QSPI_TX_FIFO_0;
	qspi_handle->dma_params.src = (uintptr_t)qspi_handle->cur_buf;
	qspi_handle->dma_params.size = dma_blk_size * 4UL;
	qspi_handle->dma_params.is_async_xfer = true;
	qspi_handle->dma_params.dir = DMA_MEM_TO_IO;
	qspi_handle->dma_params.io_bus_width = BUS_WIDTH_32;


	if (qspi_context->dma_type == DMA_GPC) {
		qspi_handle->dma_params.io = qspi_handle->qspi->gpcdma_req;
	} else if (qspi_context->dma_type == DMA_BPMP) {
		qspi_handle->dma_params.io = qspi_handle->qspi->bpmpdma_req;
	} else {
		/* No Action Required */
	}

	err = tegrabl_dma_transfer(qspi_context->dma_handle, qspi->dma_chan_id,
							   &qspi_handle->dma_params);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		pr_error("Failed to start DMA for Transmit: 0x%08x\n", err);
		return err;
	}

	rval = qspi_readl(qspi, DMA_CTL);
	rval = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, DMA_EN, ENABLE, rval);
	qspi_writel_flush(qspi, DMA_CTL, rval);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_send_wait_one_xfer_dma(
				struct tegrabl_qspi_handle *qspi_handle,
				bool is_abort)
{
	struct tegrabl_qspi_info *qspi = qspi_handle->qspi;
	struct tegrabl_qspi_ctxt *qspi_context = &qspi_handle->qspi_context;
	tegrabl_error_t err;

continue_wait:
	err = qspi_wait_for_tx_xfer_complete(qspi_handle);
	if (err != TEGRABL_NO_ERROR) {
		if (is_abort == true) {
			qspi_abort_tx_dma(qspi_handle);
			return err;
		}

		return TEGRABL_ERROR(TEGRABL_ERR_XFER_IN_PROGRESS,
							 AUX_INFO_XFER_TX_DMA);
	}

	err = tegrabl_dma_transfer_status(qspi_context->dma_handle,
									  qspi->dma_chan_id,
					  				  &qspi_handle->dma_params);
	if (err != TEGRABL_NO_ERROR) {
		if (is_abort == true) {
			qspi_abort_tx_dma(qspi_handle);
			return err;
		}

		return TEGRABL_ERROR(TEGRABL_ERR_XFER_IN_PROGRESS,
							 AUX_INFO_XFER_TX_DMA);
	}

	qspi_handle->ramain_dma_packet -= qspi_handle->cur_req_dma_packet;
	qspi_handle->cur_buf += (qspi_handle->cur_req_dma_packet *
				 			 qspi_handle->bytes_pw);
	if (qspi_handle->ramain_dma_packet != 0U) {
		qspi_clear_ready_bit(qspi_handle->qspi);
		err = qspi_send_start_one_xfer_dma(qspi_handle);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}
		goto continue_wait;
	}

	qspi_stop_tx(qspi_handle->qspi);

	return TEGRABL_NO_ERROR;
}

static void qspi_stop_rx(struct tegrabl_qspi_info *qspi)
{
	uint32_t rval;

	/* Disable Rx */
	rval = qspi_readl(qspi, COMMAND);
	rval = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Rx_EN, DISABLE, rval);
	qspi_writel(qspi, COMMAND, rval);

	/* DMA_EN bits get cleared by hw if all the data was tranferredi */
	/* successfully, else s/w will disbale it upon detection of h/w timeout */
	/* clear the status register */
	rval = qspi_readl(qspi, FIFO_STATUS);
	qspi_writel_flush(qspi, FIFO_STATUS, rval);

	qspi_clear_ready_bit(qspi);
}

static void qspi_abort_rx_pio(struct tegrabl_qspi_info *qspi)
{
	qspi_stop_rx(qspi);
	qspi_hw_disable_transfer(qspi);
}

static void qspi_abort_rx_dma(struct tegrabl_qspi_handle *qspi_handle)
{
	struct tegrabl_qspi_ctxt *qspi_context = &qspi_handle->qspi_context;

	qspi_stop_rx(qspi_handle->qspi);
	tegrabl_dma_transfer_abort(qspi_context->dma_handle,
							   qspi_handle->qspi->dma_chan_id);
	qspi_hw_disable_transfer(qspi_handle->qspi);
}

static tegrabl_error_t qspi_wait_for_rx_fifo_not_empty(
							struct tegrabl_qspi_handle *qspi_handle)
{

	uint32_t st_time = qspi_handle->xfer_start_time;
	uint32_t elapsed_time;
	uint32_t rval;

	do {
		rval = qspi_readl(qspi_handle->qspi, FIFO_STATUS);
		if ((rval & NV_DRF_DEF(QSPI, FIFO_STATUS, RX_FIFO_EMPTY, EMPTY)) == 0U) {
			return TEGRABL_NO_ERROR;
		}

		elapsed_time = (uint32_t)(tegrabl_get_timestamp_us() - st_time);
	} while (elapsed_time < qspi_handle->xfer_timeout);

	return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_CHECK_TIMEOUT);
}


static tegrabl_error_t qspi_wait_for_rx_xfer_complete(
					struct tegrabl_qspi_handle *qspi_handle)
{
	tegrabl_error_t err;
	uint32_t st_time = qspi_handle->xfer_start_time;
	uint32_t elapsed_time;
	uint32_t rval;

	err = qspi_wait_for_ready_bit(qspi_handle);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("QSPI RXDMA: Ready bit not ready\n");
		return err;
	}

	/* Check for Fifo empty */
	do {
		rval = qspi_readl(qspi_handle->qspi, FIFO_STATUS);
		if ((rval & NV_DRF_DEF(QSPI, FIFO_STATUS, RX_FIFO_EMPTY, EMPTY)) != 0U) {
			return TEGRABL_NO_ERROR;
		}

		elapsed_time = (uint32_t)(tegrabl_get_timestamp_us() - st_time);
	} while (elapsed_time < qspi_handle->xfer_timeout);

	return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, AUX_INFO_CHECK_TIMEOUT);
}

static uint32_t qspi_drain_rx_fifo(struct tegrabl_qspi_handle *qspi_handle)
{
	uint32_t rval;
	uint32_t words_count;
	uint32_t count;

	rval = qspi_readl(qspi_handle->qspi, FIFO_STATUS);
	if ((rval & NV_DRF_DEF(QSPI, FIFO_STATUS, RX_FIFO_EMPTY, EMPTY)) != 0U) {
		return 0U;
	}

	words_count = NV_DRF_VAL(QSPI, FIFO_STATUS, RX_FIFO_FULL_COUNT, rval);
	words_count = MIN(words_count, qspi_handle->cur_remain_pio_packet);
	count = 0U;
	while (words_count != 0U) {
		rval = qspi_readl(qspi_handle->qspi, RX_FIFO);
		if (qspi_handle->bytes_pw > 1U) {
			/* All 4 bytes are valid data */
			*((uint32_t *)qspi_handle->cur_buf) = rval;
		} else {
			/* only 1 byte is valid data */
			*(qspi_handle->cur_buf) = (uint8_t)(rval);
		}

		/* increment buffer pointer */
		qspi_handle->cur_buf += qspi_handle->bytes_pw;

		/* decrement requested number of words */
		qspi_handle->cur_remain_pio_packet--;
		words_count--;
		count++;
	}

	return count;
}

static tegrabl_error_t qspi_receive_start_one_xfer_pio(
								struct tegrabl_qspi_handle *qspi_handle)
{
	uint32_t dma_blk_size;
	uint32_t rval;

	qspi_handle->cur_xfer_is_dma = false;

	/* Number of bits to be transmitted per packet in unpacked mode = 32 */
	rval = qspi_readl(qspi_handle->qspi, COMMAND);
	rval = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH,
							  (qspi_handle->bits_pw - 1UL), rval);
	qspi_writel(qspi_handle->qspi, COMMAND, rval);

	dma_blk_size = MIN(qspi_handle->ramain_pio_packet, QSPI_DMA_BLK_SIZE_MAX);

	/* Set dma block size. */
	qspi_writel(qspi_handle->qspi, DMA_BLK_SIZE, (dma_blk_size - 1UL));

	qspi_handle->cur_req_pio_packet = dma_blk_size;
	qspi_handle->cur_remain_pio_packet = dma_blk_size;

	/* Enable Rx */
	rval = qspi_readl(qspi_handle->qspi, COMMAND);
	rval = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Rx_EN, ENABLE, rval);
	qspi_writel(qspi_handle->qspi, COMMAND, rval);

	/* Enable DMA mode */
	rval = qspi_readl(qspi_handle->qspi, DMA_CTL);
	rval = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, DMA_EN, ENABLE, rval);
	qspi_writel(qspi_handle->qspi, DMA_CTL, rval);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_receive_wait_one_xfer_pio(
							struct tegrabl_qspi_handle *qspi_handle,
							bool is_abort)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t words_read;

continue_wait:
	if (qspi_handle->cur_remain_pio_packet != 0U) {
		err = qspi_wait_for_rx_fifo_not_empty(qspi_handle);
		if (err != TEGRABL_NO_ERROR) {
			if (is_abort == true) {
				qspi_abort_rx_pio(qspi_handle->qspi);
				return err;
			}

			return TEGRABL_ERROR(TEGRABL_ERR_XFER_IN_PROGRESS,
								 AUX_INFO_XFER_RX_PIO);
		}

		words_read = qspi_drain_rx_fifo(qspi_handle);
		if (words_read == 0U) {
			pr_error("Number of words read in RX FIFO is 0\n");
			qspi_abort_rx_pio(qspi_handle->qspi);
			return TEGRABL_ERROR(TEGRABL_ERR_WRITE_FAILED,
								 AUX_INFO_FILL_RX_FIFO);
		}

		goto continue_wait;
	}

	qspi_handle->ramain_pio_packet -= qspi_handle->cur_req_pio_packet;
	if (qspi_handle->ramain_pio_packet != 0U) {
		qspi_clear_ready_bit(qspi_handle->qspi);
		qspi_receive_start_one_xfer_pio(qspi_handle);
		goto continue_wait;
	}

	qspi_stop_rx(qspi_handle->qspi);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_receive_start_one_xfer_dma(
						struct tegrabl_qspi_handle *qspi_handle)
{
	struct tegrabl_qspi_info *qspi = qspi_handle->qspi;
	struct tegrabl_qspi_ctxt *qspi_context = &qspi_handle->qspi_context;
	tegrabl_error_t err;
	uint32_t dma_blk_size;
	uint32_t rval;

	qspi_handle->cur_xfer_is_dma = true;

	dma_blk_size = MIN(qspi_handle->ramain_dma_packet, QSPI_DMA_BLK_SIZE_MAX);

	/* Number of bits to be transmitted per packet in unpacked mode = 32 */
	rval = qspi_readl(qspi_handle->qspi, COMMAND);
	rval = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH, 31, rval);
	qspi_writel(qspi_handle->qspi, COMMAND, rval);

	/* Set DMA trigger threshold to 8 packet length in QSPI FIFO */
	rval = qspi_readl(qspi, DMA_CTL);
	if (qspi_handle->qspi_info[0].trig_len == 16U) {
		rval = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, RX_TRIG, TRIG16, rval);
	} else if (qspi_handle->qspi_info[0].trig_len == 8U) {
		rval = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, RX_TRIG, TRIG8, rval);
	}
	qspi_writel_flush(qspi, DMA_CTL, rval);

	/* Set dma block size. The DMA Hardware block expects to be programmed
	 * one block less than the intended blocks to be transferred.
	 */
	qspi_writel(qspi_handle->qspi, DMA_BLK_SIZE, (dma_blk_size - 1U));

	qspi_handle->cur_req_dma_packet = dma_blk_size;
	qspi_handle->cur_remain_dma_packet = dma_blk_size;

	qspi_handle->dma_params.dst = (uintptr_t)qspi_handle->cur_buf;
	qspi_handle->dma_params.src = qspi_handle->qspi->base_address + (uint32_t)QSPI_RX_FIFO_0;
	qspi_handle->dma_params.size = dma_blk_size * 4UL;
	qspi_handle->dma_params.is_async_xfer = true;
	qspi_handle->dma_params.dir = DMA_IO_TO_MEM;
	qspi_handle->dma_params.io_bus_width = BUS_WIDTH_32;

	if (qspi_context->dma_type == DMA_GPC) {
		qspi_handle->dma_params.io = qspi_handle->qspi->gpcdma_req;
	} else if (qspi_context->dma_type == DMA_BPMP) {
		qspi_handle->dma_params.io = qspi_handle->qspi->bpmpdma_req;
	} else {
		/* No Action Required */
	}

	err = tegrabl_dma_transfer(qspi_context->dma_handle, qspi->dma_chan_id,
							   &qspi_handle->dma_params);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		pr_error("QSPI: dma transfer failed\n");
		return err;
	}

	/* Enable Rx */
	rval = qspi_readl(qspi_handle->qspi, COMMAND);
	rval = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, Rx_EN, ENABLE, rval);
	qspi_writel_flush(qspi_handle->qspi, COMMAND, rval);

	tegrabl_udelay(2);

	rval = qspi_readl(qspi, DMA_CTL);
	rval = NV_FLD_SET_DRF_DEF(QSPI, DMA_CTL, DMA_EN, ENABLE, rval);
	qspi_writel_flush(qspi, DMA_CTL, rval);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_receive_wait_one_xfer_dma(
							struct tegrabl_qspi_handle *qspi_handle,
							bool is_abort)
{
	struct tegrabl_qspi_info *qspi = qspi_handle->qspi;
	struct tegrabl_qspi_ctxt *qspi_context = &qspi_handle->qspi_context;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t st_time = qspi_handle->xfer_start_time;
	uint32_t elapsed_time;

continue_wait:

	do {
		tegrabl_udelay(50);
		err = tegrabl_dma_transfer_status(qspi_context->dma_handle,
										  qspi->dma_chan_id,
										  &qspi_handle->dma_params);
		if (err == TEGRABL_NO_ERROR) {
			break;
		}

		elapsed_time = (uint32_t)(tegrabl_get_timestamp_us() - st_time);
	} while (elapsed_time < qspi_handle->xfer_timeout);

	if (err != TEGRABL_NO_ERROR) {
		if (is_abort == true) {
			qspi_abort_rx_dma(qspi_handle);
			return err;
		}
		return TEGRABL_ERROR(TEGRABL_ERR_XFER_IN_PROGRESS,
							 AUX_INFO_XFER_TX_DMA);
	}

	err = qspi_wait_for_rx_xfer_complete(qspi_handle);
	if (err != TEGRABL_NO_ERROR) {
		if (is_abort == true) {
			qspi_abort_rx_dma(qspi_handle);
			return err;
		}

		return TEGRABL_ERROR(TEGRABL_ERR_XFER_IN_PROGRESS,
							 AUX_INFO_XFER_TX_DMA);
	}

	qspi_handle->ramain_dma_packet -= qspi_handle->cur_req_dma_packet;
	qspi_handle->cur_buf += (qspi_handle->cur_req_dma_packet *
							  qspi_handle->bytes_pw);
	if (qspi_handle->ramain_dma_packet != 0U) {
		qspi_clear_ready_bit(qspi_handle->qspi);
		err = qspi_receive_start_one_xfer_dma(qspi_handle);
		if (err != TEGRABL_NO_ERROR) {
			return err;
		}
		goto continue_wait;
	}

	qspi_stop_rx(qspi_handle->qspi);

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_start_remaining_one_xfer(
						struct tegrabl_qspi_handle *qspi_handle,
						bool *in_progress)
{
	if (in_progress != NULL) {
		*in_progress = true;
	}

	if (qspi_handle->is_transmit) {
		if (qspi_handle->ramain_dma_packet != 0U) {
			qspi_handle->bits_pw = 32U;
			qspi_handle->bytes_pw = 4U;
			return qspi_send_start_one_xfer_dma(qspi_handle);
		}

		if (qspi_handle->ramain_pio_4w_packet != 0U) {
			qspi_handle->bits_pw = 32U;
			qspi_handle->bytes_pw = 4U;
			qspi_handle->ramain_pio_packet = qspi_handle->ramain_pio_4w_packet;
			qspi_handle->ramain_pio_4w_packet = 0;
			return qspi_send_start_one_xfer_pio(qspi_handle);
		}

		if (qspi_handle->ramain_pio_1w_packet != 0U) {
			qspi_handle->bits_pw = 8U;
			qspi_handle->bytes_pw = 1U;
			qspi_handle->ramain_pio_packet = qspi_handle->ramain_pio_1w_packet;
			qspi_handle->ramain_pio_1w_packet = 0U;
			return qspi_send_start_one_xfer_pio(qspi_handle);
		}
	} else {
		if (qspi_handle->ramain_dma_packet != 0U) {
			qspi_handle->bits_pw = 32U;
			qspi_handle->bytes_pw = 4U;
			return qspi_receive_start_one_xfer_dma(qspi_handle);
		}

		if (qspi_handle->ramain_pio_4w_packet != 0U) {
			qspi_handle->bits_pw = 32U;
			qspi_handle->bytes_pw = 4U;
			qspi_handle->ramain_pio_packet = qspi_handle->ramain_pio_4w_packet;
			qspi_handle->ramain_pio_4w_packet = 0U;
			return qspi_receive_start_one_xfer_pio(qspi_handle);
		}

		if (qspi_handle->ramain_pio_1w_packet != 0U) {
			qspi_handle->bits_pw = 8U;
			qspi_handle->bytes_pw = 1U;
			qspi_handle->ramain_pio_packet = qspi_handle->ramain_pio_1w_packet;
			qspi_handle->ramain_pio_1w_packet = 0U;
			return qspi_receive_start_one_xfer_pio(qspi_handle);
		}
	}

	if (in_progress != NULL) {
		*in_progress = false;
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t program_dummy_cycles(
		struct tegrabl_qspi_handle *qspi_handle)
{
	struct tegrabl_qspi_transfer *xfer = qspi_handle->cur_xfer;
	struct tegrabl_qspi_info *qspi = qspi_handle->qspi;
	uint32_t reg;

	if ((qspi_handle->is_transmit == true) && (xfer->write_len < 6UL) &&
		(xfer->dummy_cycles != 0U)) {
		reg = qspi_readl(qspi, MISC);
		reg = NV_FLD_SET_DRF_NUM(QSPI, MISC, NUM_OF_DUMMY_CLK_CYCLES,
				xfer->dummy_cycles, reg);
		qspi_writel(qspi, MISC, reg);
	}

	if (xfer->op_mode != qspi_handle->curr_op_mode) {
		if (xfer->op_mode == DDR_MODE) {
			(void)tegrabl_qspi_ddr_enable(qspi->instance_id);
		} else if (xfer->op_mode == SDR_MODE) {
			(void)tegrabl_qspi_sdr_enable(qspi->instance_id);
		} else {
			return TEGRABL_ERR_BAD_PARAMETER;
		}
		qspi_handle->curr_op_mode = xfer->op_mode;
	}
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_start_one_transaction(
		struct tegrabl_qspi_handle *qspi_handle)
{
	struct tegrabl_qspi_transfer *xfer = qspi_handle->cur_xfer;
	struct tegrabl_qspi_info *qspi = qspi_handle->qspi;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t reg;
	bool pio_only = false;
	uint32_t bpw = 0;

	if (xfer->tx_buf != NULL) {
		qspi_handle->buf_addr = (uintptr_t)xfer->tx_buf;
		qspi_handle->cur_buf = xfer->tx_buf;
		qspi_handle->buf_len = xfer->write_len;
		qspi_handle->is_transmit = true;
	} else {
		qspi_handle->buf_addr = (uintptr_t)xfer->rx_buf;
		qspi_handle->cur_buf = xfer->rx_buf;
		qspi_handle->buf_len = xfer->read_len;
		qspi_handle->is_transmit = false;
	}

	qspi_handle->requested_bytes = qspi_handle->buf_len;

	if ((qspi_handle->param->fifo_access_mode != (uint32_t)QSPI_MODE_DMA)) {
		pio_only = true;
	}

	if (((qspi_handle->buf_addr & 0x3U) != 0U) ||
		(qspi_handle->requested_bytes < QSPI_DMA_THRESOLD)){
		pio_only = true;
		bpw = 8U;
	}

	if (pio_only == true) {
		if (bpw == 8U) {
			qspi_handle->req_pio_4w_bytes = 0U;
			qspi_handle->req_pio_1w_bytes = qspi_handle->requested_bytes;
		} else {
			qspi_handle->req_pio_4w_bytes = (qspi_handle->requested_bytes >> 2) << 2;
			qspi_handle->req_pio_1w_bytes = qspi_handle->requested_bytes -
												qspi_handle->req_pio_4w_bytes;
		}
		qspi_handle->req_dma_bytes = 0U;
	} else {
		qspi_handle->req_dma_bytes = (qspi_handle->requested_bytes >> 2) << 2;
		qspi_handle->req_pio_1w_bytes = qspi_handle->requested_bytes -
												qspi_handle->req_dma_bytes;
		qspi_handle->req_pio_4w_bytes = 0U;
	}

	qspi_handle->req_dma_packet = qspi_handle->req_dma_bytes >> 2;
	qspi_handle->req_pio_4w_packet = qspi_handle->req_pio_4w_bytes >> 2;
	qspi_handle->req_pio_1w_packet = qspi_handle->req_pio_1w_bytes;
	qspi_handle->ramain_dma_packet = qspi_handle->req_dma_packet;
	qspi_handle->ramain_pio_4w_packet = qspi_handle->req_pio_4w_packet;
	qspi_handle->ramain_pio_1w_packet = qspi_handle->req_pio_1w_packet;

	qspi_writel(qspi, MISC, 0);

	reg = qspi_readl(qspi, COMMAND);

	reg = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, SDR_DDR_SEL, xfer->op_mode, reg);
	reg = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, INTERFACE_WIDTH, xfer->bus_width,
							 reg);
	/* Set Packed and Unpacked mode */
	reg = NV_FLD_SET_DRF_DEF(QSPI, COMMAND, PACKED, DISABLE, reg);

	/* Number of bits to be transmitted per packet in unpacked mode = 32 */
	reg = NV_FLD_SET_DRF_NUM(QSPI, COMMAND, BIT_LENGTH,
							 (qspi_handle->bits_pw - 1UL), reg);
	qspi_writel(qspi, COMMAND, reg);

	err = program_dummy_cycles(qspi_handle);
	if (err != TEGRABL_NO_ERROR) {
		return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, AUX_INFO_OP_CODE);
	}

	return qspi_start_remaining_one_xfer(qspi_handle, NULL);
}

static tegrabl_error_t qspi_wait_transfer(struct tegrabl_qspi_handle *qspi_handle,
											bool is_abort)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	if (qspi_handle->is_transmit) {
		if (qspi_handle->cur_xfer_is_dma == true) {
			err = qspi_send_wait_one_xfer_dma(qspi_handle, is_abort);
		} else {
			err = qspi_send_wait_one_xfer_pio(qspi_handle, is_abort);
		}

		if (err == TEGRABL_NO_ERROR) {
			return TEGRABL_NO_ERROR;
		} else {
			return err;
		}
	} else {
		if (qspi_handle->cur_xfer_is_dma == true) {
			err = qspi_receive_wait_one_xfer_dma(qspi_handle, is_abort);
		} else {
			err = qspi_receive_wait_one_xfer_pio(qspi_handle, is_abort);
		}

		if (err == TEGRABL_NO_ERROR) {
			return TEGRABL_NO_ERROR;
		} else {
			return err;
		}
	}
}

static tegrabl_error_t qspi_xfer_wait(
						struct tegrabl_qspi_handle *qspi_handle,
						uint32_t timeout_us, bool is_abort)
{
	struct tegrabl_qspi_device_property *qdev = qspi_handle->device;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_error_t e_reason;
	bool in_progress;

	qspi_handle->xfer_timeout = timeout_us;

	while (true) {
		err = qspi_wait_transfer(qspi_handle, is_abort);
		e_reason = TEGRABL_ERROR_REASON(err);
		if (e_reason == TEGRABL_ERR_XFER_IN_PROGRESS) {
			return err;
		}
		if (err != TEGRABL_NO_ERROR) {
			goto clean_exit;
		}

		err = qspi_start_remaining_one_xfer(qspi_handle, &in_progress);
		if (err != TEGRABL_NO_ERROR) {
			goto clean_exit;
		}

		if (in_progress == true) {
			continue;
		}
		qspi_handle->cur_xfer_index++;
		if (qspi_handle->cur_xfer_index >= qspi_handle->req_xfer_count) {
			err = TEGRABL_NO_ERROR;
			goto clean_exit;
		}

		qspi_handle->cur_xfer = &qspi_handle->req_xfer[qspi_handle->cur_xfer_index];
		err = qspi_start_one_transaction(qspi_handle);
		if (err != TEGRABL_NO_ERROR) {
			goto clean_exit;
		}
	}

clean_exit:
	qspi_set_chip_select_level(qspi_handle->qspi, qdev, false);
	qspi_handle->xfer_is_progress = false;
	return err;
}

static tegrabl_error_t qspi_check_transfer_status(
						struct tegrabl_qspi_handle *qspi_handle,
						uint32_t timeout_us, bool is_abort)
{
	struct tegrabl_qspi_device_property *qdev = qspi_handle->device;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	tegrabl_error_t e_reason;
	bool in_progress;

	qspi_handle->xfer_timeout = timeout_us;

	while (true) {
		err = qspi_wait_transfer(qspi_handle, is_abort);
		e_reason = TEGRABL_ERROR_REASON(err);
		if (e_reason == TEGRABL_ERR_XFER_IN_PROGRESS) {
			return err;
		}
		if (err != TEGRABL_NO_ERROR) {
			goto clean_exit;
		}
		err = qspi_start_remaining_one_xfer(qspi_handle, &in_progress);
		if (err != TEGRABL_NO_ERROR) {
			goto clean_exit;
		}
		if (in_progress == true) {
			continue;
		}
		qspi_handle->cur_xfer_index++;
		if (qspi_handle->cur_xfer_index >= qspi_handle->req_xfer_count) {
			err = TEGRABL_NO_ERROR;
			goto clean_exit;
		}

		qspi_handle->cur_xfer = &qspi_handle->req_xfer[qspi_handle->cur_xfer_index];
		err = qspi_start_one_transaction(qspi_handle);
		if (err != TEGRABL_NO_ERROR) {
			goto clean_exit;
		}
		if ((qspi_handle->cur_xfer_is_dma == true) && (qspi_handle->is_async == true)) {
			return TEGRABL_NO_ERROR;
		}
	}

clean_exit:
	qspi_set_chip_select_level(qspi_handle->qspi, qdev, false);
	qspi_handle->xfer_is_progress = false;
	return err;
}

static tegrabl_error_t qspi_validate_packet(
				struct tegrabl_qspi_transfer *xfer,
				uint64_t buf_addr, uint32_t len)
{
	if (len == 0U) {
		return TEGRABL_ERR_INVALID;
	}

	if (xfer->packet_bit_length == 32U) {
		if ((len & 0x3U) != 0U) {
			return TEGRABL_ERR_INVALID;
		}

		if ((buf_addr & 0x3U) != 0U) {
			return TEGRABL_ERR_INVALID;
		}
	}
	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t qspi_validate_parameters(
				struct tegrabl_qspi_transfer *p_transfers,
				uint32_t no_of_transfers)
{
	struct tegrabl_qspi_transfer *xfer;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint64_t buf_addr = 0;
	uint32_t len;
	uint32_t i;

	if ((p_transfers == NULL) || (no_of_transfers == 0U)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_TXFER_ARGS);
	}

	for (i = 0; i < no_of_transfers; ++i) {
		xfer = &p_transfers[i];

		/* Only half duplex */
		if ((xfer->tx_buf != NULL) && (xfer->rx_buf != NULL)) {
			goto error;
		}

		if ((xfer->tx_buf == NULL) && (xfer->rx_buf == NULL)) {
			goto error;
		}

		if (xfer->tx_buf != NULL) {
			buf_addr = (uintptr_t)xfer->tx_buf;
			len = xfer->write_len;
		} else {
			buf_addr = (uintptr_t)xfer->rx_buf;
			len = xfer->read_len;
		}

		err = qspi_validate_packet(xfer, buf_addr, len);
		if (err != TEGRABL_NO_ERROR) {
			goto error;
		}
	}

	return TEGRABL_NO_ERROR;

error:
	pr_error("Validation of Transfer failed\n");
	return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_TXFER_ARGS);
}

tegrabl_error_t tegrabl_qspi_transaction(
		struct tegrabl_qspi_handle *qspi_handle,
		struct tegrabl_qspi_transfer *p_transfers,
		uint8_t no_of_transfers,
		uint32_t timeout_us)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((qspi_handle == NULL) || (qspi_handle->qspi == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_TXFER_ARGS);
	}

	err = qspi_validate_parameters(p_transfers, no_of_transfers);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	qspi_handle->xfer_start_time = (uint32_t)tegrabl_get_timestamp_us();
	qspi_handle->xfer_timeout = timeout_us;
	qspi_handle->req_xfer = p_transfers;
	qspi_handle->req_xfer_count = no_of_transfers;
	qspi_handle->cur_xfer_index = 0U;
	qspi_handle->cur_xfer = qspi_handle->req_xfer;
	qspi_handle->xfer_is_progress = true;

	qspi_set_chip_select_level(qspi_handle->qspi, qspi_handle->device, true);

	err = qspi_start_one_transaction(qspi_handle);
	if (err != TEGRABL_NO_ERROR) {
		qspi_set_chip_select_level(qspi_handle->qspi, qspi_handle->device,
								   false);
		qspi_handle->xfer_is_progress = false;
		return err;
	}

	return qspi_check_transfer_status(qspi_handle, timeout_us, true);
}

tegrabl_error_t tegrabl_qspi_check_transfer_status(
					struct tegrabl_qspi_handle *qspi_handle,
					uint32_t timeout_us, bool is_abort)
{
	return qspi_check_transfer_status(qspi_handle, timeout_us, is_abort);
}

tegrabl_error_t tegrabl_qspi_xfer_wait(
					struct tegrabl_qspi_handle *qspi_handle,
					uint32_t timeout_us, bool is_abort)
{
	return qspi_xfer_wait(qspi_handle, timeout_us, is_abort);
}

tegrabl_error_t tegrabl_qspi_open(uint32_t instance,
							struct tegrabl_qspi_device_property *qspi_device,
							struct tegrabl_qspi_platform_params *params,
							struct tegrabl_qspi_handle **qspi_handle)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_qspi_info *qspi;
	struct tegrabl_qspi_handle *qhandle = NULL;
	struct tegrabl_qspi_platform_params *qparams;
	struct tegrabl_qspi_device_property *qdevice;
	struct tegrabl_qspi_ctxt *qspi_context;
	struct qspi_soc_info *gqspi_info;

	*qspi_handle = NULL;

	if (qspi_device == NULL) {
		pr_error("Device property is not provided\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_OPEN_PARAMETER);
	}

	if (params == NULL) {
		pr_error("QSPI platform property is not provided\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_OPEN_PARAMETER);
	}

	if (instance >= QSPI_MAX_INSTANCE) {
		pr_error("QSPI instance %u is not allowed\n", instance);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_OPEN_PARAMETER);
	}

	if (params->dma_type > 2U) {
		pr_error("QSPI DMA type %d is not allowed\n", params->dma_type);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_OPEN_PARAMETER);
	}

	/* check for dma validity */
	if ((params->dma_type != DMA_BPMP) && (params->dma_type != DMA_GPC)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_INVALID_DMA);
	}

	if (s_qspi_info[instance].open_count != 0U) {
		pr_error("QSPI controller %u is already opened\n", instance);
		return TEGRABL_ERROR(TEGRABL_ERR_OPEN_FAILED, AUX_INFO_ALREADY_OPEN);
	}

	qspi = &s_qspi_info[instance];
	qparams = &s_qspi_params[instance];
	qhandle = &s_qspi_handle[instance];
	qdevice = &s_qspi_device[instance];
	qspi_context = &(qhandle->qspi_context);
	qspi_get_soc_info(&gqspi_info);

	/* reinit the structure */
	memcpy(qparams, params, sizeof(*params));

	pr_debug("Qspi using %s\n",
			 (qparams->dma_type == DMA_BPMP) ? "bpmp-dma" : "gpc-dma");
	qspi_context->dma_type = qparams->dma_type;
	qspi_context->dma_handle = tegrabl_dma_request(qparams->dma_type);

	memcpy(qdevice, qspi_device, sizeof(*qspi_device));

	err = qspi_controller_init(qspi, qparams, qdevice, qhandle);
	if (err != TEGRABL_NO_ERROR) {
		return err;
	}

	qhandle->qspi = qspi;
	qhandle->device = qdevice;
	qhandle->param = qparams;
	qhandle->qspi_info = gqspi_info;

	*qspi_handle = qhandle;
	qspi->open_count++;
	qhandle->xfer_is_progress = false;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_qspi_close(struct tegrabl_qspi_handle *qspi_handle)
{
	if (qspi_handle == NULL) {
		return TEGRABL_NO_ERROR;
	}

	if (qspi_handle->qspi == NULL) {
		return TEGRABL_NO_ERROR;
	}

	if (qspi_handle->xfer_is_progress == true) {
		pr_error("QSPI is busy in data transfer\n");
		return TEGRABL_ERROR(TEGRABL_ERR_BUSY, 0);
	}

	if (qspi_handle->qspi->open_count == 0U) {
		pr_error("QSPI is not opened\n");
		return TEGRABL_NO_ERROR;
	}

	qspi_handle->qspi->open_count--;

	return TEGRABL_NO_ERROR;
}
