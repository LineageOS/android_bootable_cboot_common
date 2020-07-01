/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_CONSOLE

#include "build_config.h"
#include <stdint.h>
#include <stddef.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_console.h>
#include <tegrabl_uart_console.h>
#include <tegrabl_comb_uart_console.h>

static struct tegrabl_console s_console;

tegrabl_error_t tegrabl_console_register(
	tegrabl_console_interface_t interface, uint32_t instance, void *data)
{
	struct tegrabl_console *hconsole;
#if defined(CONFIG_ENABLE_UART)
	struct tegrabl_uart *huart;
#endif
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	hconsole = &s_console;
	hconsole->interface = interface;
	hconsole->instance = instance;

	TEGRABL_UNUSED(data);

	switch (hconsole->interface) {
#if defined(CONFIG_ENABLE_UART)
	case TEGRABL_CONSOLE_UART:
		hconsole->putchar = tegrabl_uart_console_putchar;
		hconsole->getchar = tegrabl_uart_console_getchar;
		hconsole->puts = tegrabl_uart_console_puts;
		hconsole->close = tegrabl_uart_console_close;
		huart = tegrabl_uart_open(hconsole->instance);
		if (huart != NULL) {
			hconsole->dev = huart;
			hconsole->is_registered = true;
		} else {
			error = TEGRABL_ERROR(TEGRABL_ERR_INIT_FAILED, 0);
		}
		break;
#endif

#if defined(CONFIG_ENABLE_COMB_UART)
	case TEGRABL_CONSOLE_COMB_UART:
		hconsole->putchar = tegrabl_comb_uart_console_putchar;
		hconsole->getchar = tegrabl_comb_uart_console_getchar;
		hconsole->puts = tegrabl_comb_uart_console_puts;
		hconsole->close = tegrabl_comb_uart_console_close;
		if (tegrabl_comb_uart_init(*((uint32_t *)data)) == TEGRABL_NO_ERROR) {
			hconsole->is_registered = true;
		} else {
			error = TEGRABL_ERROR(TEGRABL_ERR_INIT_FAILED, 1);
		}
		break;
#endif

#if defined(CONFIG_ENABLE_SEMIHOST)
	case TEGRABL_CONSOLE_SEMIHOST:
		hconsole->putchar = tegrabl_semihost_console_putchar;
		hconsole->getchar = tegrabl_semihost_console_getchar;
		hconsole->puts = tegrabl_semihost_console_puts;
		hconsole->close = tegrabl_semihost_console_close;
		hconsole->is_registered = true;
		break;
#endif
	default:
		hconsole->is_registered = false;
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		break;
	}
	return error;
}

struct tegrabl_console *tegrabl_console_open(void)
{
	struct tegrabl_console *hconsole;

	hconsole = &s_console;
	if (hconsole->is_registered == true) {
		return hconsole;
	} else {
		return NULL;
	}
}

tegrabl_error_t tegrabl_console_putchar(struct tegrabl_console *hconsole,
	char ch)
{
	tegrabl_error_t error;

	if (hconsole == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	error = hconsole->putchar(hconsole, ch);
	if (error != TEGRABL_NO_ERROR) {
		tegrabl_err_set_highest_module(error, MODULE);
	}
	return error;
}

tegrabl_error_t tegrabl_console_getchar(struct tegrabl_console *hconsole,
	char *ch, time_t timeout)
{
	tegrabl_error_t error;

	if (hconsole == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
	}

	error = hconsole->getchar(hconsole, ch, timeout);
	if (error != TEGRABL_NO_ERROR) {
		tegrabl_err_set_highest_module(error, MODULE);
	}
	return error;
}

tegrabl_error_t tegrabl_console_puts(struct tegrabl_console *hconsole,
	char *str)
{
	tegrabl_error_t error;

	if (hconsole == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
	}

	error = hconsole->puts(hconsole, str);
	if (error != TEGRABL_NO_ERROR) {
		tegrabl_err_set_highest_module(error, MODULE);
	}
	return error;
}

tegrabl_error_t tegrabl_console_close(struct tegrabl_console *hconsole)
{
	tegrabl_error_t error;

	if (hconsole == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
	}

	error = hconsole->close(hconsole);
	if (error != TEGRABL_NO_ERROR) {
		tegrabl_err_set_highest_module(error, MODULE);
	}
	return error;
}
