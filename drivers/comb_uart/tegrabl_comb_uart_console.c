/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_COMBINED_UART

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_console.h>
#include <tegrabl_comb_uart_console.h>
#include <tegrabl_comb_uart.h>

tegrabl_error_t tegrabl_comb_uart_console_putchar(struct tegrabl_console *hcnsl, char ch)
{
	if (hcnsl == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
	}

	return tegrabl_comb_uart_tx(&ch, 1, 0xffffffffULL);
}

tegrabl_error_t tegrabl_comb_uart_console_getchar(struct tegrabl_console *hcnsl, char *ch,
	time_t timeout)
{
	uint32_t bytes_received;

	if (hcnsl == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
	}

	return tegrabl_comb_uart_rx((uint8_t *)ch, 1, &bytes_received, timeout);
}

tegrabl_error_t tegrabl_comb_uart_console_puts(struct tegrabl_console *hcnsl, char *str)
{
	uint32_t len = strlen(str);
	const char *str_temp;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (hcnsl == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto fail;
	}

	if (len == 0U) {
		return TEGRABL_NO_ERROR; /*nothing to transmit*/
	}

	if (str[len - 1U] == '\n') {
		err = tegrabl_comb_uart_tx(str, (len - 1U), 0xffffffffULL);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		str_temp = "\r\n";
		err = tegrabl_comb_uart_tx(str_temp, 2, 0xffffffffULL);
	} else {
		err = tegrabl_comb_uart_tx(str, len, 0xffffffffULL);
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_comb_uart_console_close(struct tegrabl_console *hcnsl)
{
	if (hcnsl == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
	}
	return tegrabl_comb_uart_close();
}
