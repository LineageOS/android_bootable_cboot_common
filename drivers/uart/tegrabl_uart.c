/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
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
#include <aruart.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_drf.h>
#include <tegrabl_module.h>
#include <tegrabl_clock.h>
#include <tegrabl_timer.h>
#include <tegrabl_io.h>
#include <tegrabl_compiler.h>

/* FIXME: this needs to be configurable */
#define BAUD_RATE	115200
#define uart_readl(huart, reg) \
	NV_READ32(((uintptr_t)(huart->base_addr) + UART_##reg##_0));

#define uart_writel(huart, reg, value) \
	NV_WRITE32(((uintptr_t)(huart->base_addr) + UART_##reg##_0), (value));

#define MAX_UART_INSTANCES 9
struct tegrabl_uart uart[MAX_UART_INSTANCES];

static uint32_t uart_addr_map[MAX_UART_INSTANCES] = {
	[0] = NV_ADDRESS_MAP_UARTA_BASE,
	[1] = NV_ADDRESS_MAP_UARTB_BASE,
	[2] = NV_ADDRESS_MAP_UARTC_BASE,
	[3] = NV_ADDRESS_MAP_UARTD_BASE,
	[4] = NV_ADDRESS_MAP_UARTE_BASE,
	[5] = NV_ADDRESS_MAP_UARTF_BASE,
	[6] = NV_ADDRESS_MAP_UARTG_BASE,
};

static inline uint32_t uart_tx_ready(struct tegrabl_uart *huart)
{
	uint32_t reg;

	reg = uart_readl(huart, LSR);
	return reg & UART_LSR_0_THRE_FIELD;
}

static inline uint32_t uart_rx_ready(struct tegrabl_uart *huart)
{
	uint32_t reg;

	reg = uart_readl(huart, LSR);
	return reg & UART_LSR_0_RDR_FIELD;
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

static inline uint32_t uart_trasmit_complete(struct tegrabl_uart *huart)
{
	uint32_t reg;

	reg = uart_readl(huart, LSR);
	return reg & UART_LSR_0_TMTY_FIELD;
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
	reg_value = (pllp_freq * 1000) / (huart->baud_rate * 16);

	/* Program DLAB */
	uart_writel(huart, THR_DLAB_0, reg_value & 0xFF);
	uart_writel(huart, IER_DLAB_0, (reg_value >> 8) & 0xFF);

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

	if (instance >= MAX_UART_INSTANCES) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	huart = &uart[instance];

	huart->instance = instance;
	huart->base_addr = uart_addr_map[huart->instance];
	huart->baud_rate = BAUD_RATE;

	error = tegrabl_car_rst_set(TEGRABL_MODULE_UART, instance);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = tegrabl_car_clk_enable(TEGRABL_MODULE_UART, instance, 0);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = tegrabl_car_rst_clear(TEGRABL_MODULE_UART, instance);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	error = tegrabl_car_get_clk_rate(TEGRABL_MODULE_UART, instance, &uart_freq);
	if (error != TEGRABL_NO_ERROR) {
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
	delay = ((1000000 / huart->baud_rate) + 1) * 2;
	tegrabl_udelay((uint64_t)delay);

	/* Write Line Control reg to set no parity, 1 stop bit, word Lengh 8 */
	reg_value = NV_DRF_NUM(UART, LCR, PAR, 0) |
						 NV_DRF_NUM(UART, LCR, STOP, 0) |
						 NV_DRF_NUM(UART, LCR, WD_SIZE, 3);
	uart_writel(huart, LCR, reg_value);

	uart_writel(huart, MCR, UART_MCR_0_SW_DEFAULT_VAL);
	uart_writel(huart, MSR, UART_MSR_0_SW_DEFAULT_VAL);
	uart_writel(huart, SPR, UART_SPR_0_SW_DEFAULT_VAL);
	uart_writel(huart, IRDA_CSR, UART_IRDA_CSR_0_SW_DEFAULT_VAL);
	uart_writel(huart, ASR, UART_ASR_0_SW_DEFAULT_VAL);

	/* Flush any old characters out of the RX FIFO */
	while (uart_rx_ready(huart) != 0U) {
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
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	*bytes_transmitted = 0;
	start_time =  tegrabl_get_timestamp_us();

	while (index < len) {
		present_time = tegrabl_get_timestamp_us();
		if ((present_time - start_time) >= tfr_timeout) {
			error = TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 0);
			break;
		}
		while (!uart_tx_ready(huart))
			;
		if (buf[index] == '\n') {
			uart_tx_byte(huart, (uint8_t)('\r'));
			while (!uart_tx_ready(huart)) {
				;
			}
		}
		uart_tx_byte(huart, buf[index]);
		index++;
	}

	while (!uart_trasmit_complete(huart)) {
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

	if ((huart == NULL) || (rx_buf == NULL) || (bytes_received == NULL)) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
	}

	*bytes_received = 0;
	start_time =  tegrabl_get_timestamp_us();

	while (index < len) {
		while (1) {
			present_time = tegrabl_get_timestamp_us();

			if ((present_time - start_time) >= tfr_timeout) {
				return TEGRABL_ERROR(TEGRABL_ERR_TIMEOUT, 2);
			}

			reg = uart_readl(huart, LSR);

			if (!(reg & 1 << 0))
				continue;
			else
				break;
		}
		reg = uart_readl(huart, THR_DLAB_0);
		buf[index++] = (uint8_t)reg;
	}
	*bytes_received = index;
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_uart_close(struct tegrabl_uart *huart)
{
	if (huart == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
	}
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_uart_get_address(uint32_t instance, uint64_t *addr)
{
	if (instance >= MAX_UART_INSTANCES || addr == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
	}

	*addr = uart_addr_map[instance];
	return TEGRABL_NO_ERROR;
}
