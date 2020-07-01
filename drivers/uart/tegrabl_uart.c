/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_UART

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_uart.h>
#include <tegrabl_uart_soc_local.h>
#include <aruart.h>
#include <tegrabl_drf.h>
#include <tegrabl_module.h>
#include <tegrabl_clock.h>
#include <tegrabl_timer.h>
#include <tegrabl_io.h>
#include <tegrabl_compiler.h>

/* FIXME: this needs to be configurable */
#define BAUD_RATE	115200
#define uart_readl(huart, reg) \
	NV_READ32(((uintptr_t)((huart)->base_addr) + (uint8_t)(UART_##reg##_0)));

#define uart_writel(huart, reg, value) \
	NV_WRITE32(((uintptr_t)((huart)->base_addr) + (uint8_t)(UART_##reg##_0)), \
			   (value));

static inline bool uart_tx_ready(struct tegrabl_uart *huart)
{
	uint32_t reg;

	reg = uart_readl(huart, LSR);
	if ((NV_DRF_VAL(UART, LSR, THRE, reg)) == 1U) {
		return true;
	}
	return false;
}

static inline bool uart_rx_ready(struct tegrabl_uart *huart)
{
	uint32_t reg;

	reg = uart_readl(huart, LSR);
	if ((NV_DRF_VAL(UART, LSR, RDR, reg)) == 1U) {
		return true;
	}
	return false;
}

static inline void uart_tx_byte(struct tegrabl_uart *huart, uint8_t reg)
{
	uart_writel(huart, THR_DLAB_0, reg);
}

static inline uint32_t uart_rx_byte(struct tegrabl_uart *huart)
{
	uint32_t reg;

	reg = uart_readl(huart, THR_DLAB_0);
	return reg;
}

static inline bool uart_trasmit_complete(struct tegrabl_uart *huart)
{
	uint32_t reg;

	reg = uart_readl(huart, LSR);
	if (NV_DRF_VAL(UART, LSR, TMTY, reg) == 1U) {
		return true;
	}
	return false;
}

static tegrabl_error_t uart_set_baudrate(struct tegrabl_uart *huart,
	uint32_t pllp_freq)
{
	uint32_t reg_value;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	/* Enable DLAB access */
	reg_value = NV_DRF_NUM(UART, LCR, DLAB, 1);
	uart_writel(huart, LCR, reg_value);

	/* Prepare the divisor value */
	reg_value = (pllp_freq * 1000U) / (huart->baud_rate * 16U);

	/* Program DLAB */
	uart_writel(huart, THR_DLAB_0, reg_value & 0xFFU);
	uart_writel(huart, IER_DLAB_0, (reg_value >> 8) & 0xFFU);

	/* Disable DLAB access */
	reg_value = NV_DRF_NUM(UART, LCR, DLAB, 0);
	uart_writel(huart, LCR, reg_value);
	return e;
}

struct tegrabl_uart *tegrabl_uart_open(uint32_t instance)
{
	uint32_t reg_value;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t uart_freq = 0;
	uint32_t delay;
	struct tegrabl_uart *huart = NULL;
	uint32_t num_of_instances;
	uint32_t count = 0;

	tegrabl_uart_soc_get_info(&huart, &num_of_instances);

	if (instance >= num_of_instances) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	while (count++ < num_of_instances) {
		if (huart->instance == instance) {
			break;
		}
		if (count >= num_of_instances) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
			goto fail;
		}
		huart++;
	}


	huart->baud_rate = BAUD_RATE;

	error = tegrabl_car_rst_set(TEGRABL_MODULE_UART, (uint8_t)instance);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_car_clk_enable(TEGRABL_MODULE_UART, (uint8_t)instance, 0);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_car_rst_clear(TEGRABL_MODULE_UART, (uint8_t)instance);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = tegrabl_car_get_clk_rate(TEGRABL_MODULE_UART, (uint8_t)instance, &uart_freq);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	error = uart_set_baudrate(huart, uart_freq);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Program FIFO control reg to clear Tx, Rx FIRO and to enable them */
	reg_value = NV_DRF_NUM(UART, IIR_FCR, TX_CLR, 1) |
						 NV_DRF_NUM(UART, IIR_FCR, RX_CLR, 1) |
						 NV_DRF_NUM(UART, IIR_FCR, FCR_EN_FIFO, 1);
	uart_writel(huart, IIR_FCR, reg_value);

	/* wait 2 bauds after tx flush */
	delay = ((1000000U / huart->baud_rate) + 1U) * 2U;
	tegrabl_udelay((uint64_t)delay);

	/* Write Line Control reg to set no parity, 1 stop bit, word Lengh 8 */
	reg_value = NV_DRF_NUM(UART, LCR, PAR, 0) |
						 NV_DRF_NUM(UART, LCR, STOP, 0) |
						 NV_DRF_NUM(UART, LCR, WD_SIZE, 3);
	uart_writel(huart, LCR, reg_value);

	/* configure with reset values only */
	uart_writel(huart, MCR, 0);
	uart_writel(huart, MSR, 0);
	uart_writel(huart, SPR, 0);
	uart_writel(huart, IRDA_CSR, 0);
	uart_writel(huart, ASR, 0);

	/* Flush any old characters out of the RX FIFO */
	while (uart_rx_ready(huart) == true) {
		(void)uart_rx_byte(huart);
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		huart = NULL;
	}

	return huart;
}

tegrabl_error_t tegrabl_uart_tx(struct tegrabl_uart *huart, const void *tx_buf,
	uint32_t len, uint32_t *bytes_transmitted, time_t tfr_timeout)
{
	const uint8_t *buf = tx_buf;
	uint32_t index = 0;
	time_t start_time = 0;
	time_t present_time = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if ((huart == NULL) || (tx_buf == NULL) || (bytes_transmitted == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
	}

	*bytes_transmitted = 0;
	start_time =  tegrabl_get_timestamp_us();

	while (index < len) {
		present_time = tegrabl_get_timestamp_us();
		if ((present_time - start_time) >= tfr_timeout) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			break;
		}
		while (uart_tx_ready(huart) != true) {
			;
		}
		if (buf[index] == (uint8_t)'\n') {
			uart_tx_byte(huart, (uint8_t)('\r'));
			while (uart_tx_ready(huart) != true) {
				;
			}
		}
		uart_tx_byte(huart, buf[index]);
		index++;
	}

	while (uart_trasmit_complete(huart) != true) {
		present_time = tegrabl_get_timestamp_us();
		if ((present_time - start_time) >= tfr_timeout) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 1);
			break;
		}
	}
	*bytes_transmitted = index;
	return error;
}

tegrabl_error_t tegrabl_uart_rx(struct tegrabl_uart *huart,  void *rx_buf,
	uint32_t len, uint32_t *bytes_received, time_t tfr_timeout)
{
	uint32_t reg;
	uint32_t index = 0;
	time_t start_time;
	time_t present_time;
	uint8_t *buf = rx_buf;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((huart == NULL) || (rx_buf == NULL) || (bytes_received == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
	}

	start_time =  tegrabl_get_timestamp_us();

	while (index < len) {
		while (true) {
			if (uart_rx_ready(huart) == true) {
				break;
			}

			present_time = tegrabl_get_timestamp_us();
			if ((present_time - start_time) >= tfr_timeout) {
				err = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 2);
				goto fail;
			}
		}
		reg = uart_rx_byte(huart);
		buf[index++] = (uint8_t)reg;
	}

fail:
	*bytes_received = index;
	return err;
}

tegrabl_error_t tegrabl_uart_get_address(uint32_t instance, uint64_t *addr)
{
	struct tegrabl_uart *huart;
	uint32_t num_of_instances;
	uint32_t count = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	tegrabl_uart_soc_get_info(&huart, &num_of_instances);

	if ((instance >= num_of_instances) || (addr == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
		goto fail;
	}

	while (count++ < num_of_instances) {
		if (huart->instance == instance) {
			*addr = huart->base_addr;
			break;
		}
		if (count >= num_of_instances) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
			goto fail;
		}
		huart++;
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_uart_close(struct tegrabl_uart *huart)
{
	if (huart == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
	}
	return TEGRABL_NO_ERROR;
}
