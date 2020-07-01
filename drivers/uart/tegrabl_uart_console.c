/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_UART

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_console.h>
#include <tegrabl_uart_console.h>
#include <tegrabl_uart.h>

struct tegrabl_uart *tegrabl_uart_console_open(uint32_t instance)
{
	return tegrabl_uart_open(instance);
}

tegrabl_error_t tegrabl_uart_console_putchar(struct tegrabl_console *hcnsl,
	char ch)
{
	uint32_t bytes_transmitted;

	if (hcnsl == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
	}

	/* bytes transmitted dummy here */
	return tegrabl_uart_tx(hcnsl->dev, &ch, 1, &bytes_transmitted, 0XFFFFFFFFUL);
}

tegrabl_error_t tegrabl_uart_console_getchar(struct tegrabl_console *hcnsl,
	char *ch, time_t timeout)
{
	uint32_t bytes_received;

	if (hcnsl == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
	}

	/* bytes received dummy here */
	return tegrabl_uart_rx(hcnsl->dev, ch, 1, &bytes_received, timeout);
}

tegrabl_error_t tegrabl_uart_console_puts(struct tegrabl_console *hcnsl,
	char *str)
{
	uint32_t bytes_transmitted;

	if (hcnsl == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
	}

	/* bytes transmitted dummy here */
	return tegrabl_uart_tx(hcnsl->dev, str, strlen(str), &bytes_transmitted, 0XFFFFFFFFUL);
}

tegrabl_error_t tegrabl_uart_console_close(struct tegrabl_console *hcnsl)
{
	if (hcnsl == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
	}
	return tegrabl_uart_close(hcnsl->dev);
}
