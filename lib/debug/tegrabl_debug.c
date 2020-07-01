/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_DEBUG

#include "build_config.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <tegrabl_stdarg.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_timer.h>
#include <tegrabl_console.h>

static char msg[1024];
static struct tegrabl_console *hdev;

int tegrabl_snprintf(char *str, size_t size, const char *format, ...)
{
	int n = 0;
	va_list ap;

	va_start(ap, format);
	n = tegrabl_vsnprintf(str, size, format, ap);
	va_end(ap);
	return n;
}

static int tegrabl_vprintf(const char *format, va_list ap)
{
	uint32_t size = 0;
	uint32_t ret = 0;

#if defined(CONFIG_DEBUG_TIMESTAMP)
	uint32_t i = 0;
	uint64_t msec = 0;

	size = 11;
	msec = tegrabl_get_timestamp_ms();

	ret += tegrabl_snprintf(msg, size, "[%07d] ", (uint32_t)msec);

	for (i = size; i > 5; i--) {
		msg[i] = msg[i - 1];
	}

	msg[i] = '.';
#endif

	ret += tegrabl_vsnprintf(msg + size, sizeof(msg) - size, format, ap);
	tegrabl_console_puts(hdev, msg);

	return ret;
}

tegrabl_error_t tegrabl_debug_init(bool is_enable)
{
	if (is_enable != true) {
		hdev = NULL;
		return TEGRABL_NO_ERROR;
	}

	hdev = tegrabl_console_open();
	if (hdev == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

	return TEGRABL_NO_ERROR;
}

int tegrabl_printf(const char *format, ...)
{
	va_list ap;
	int ret = 0;

	if (hdev == NULL) {
		return 0;
	}

	va_start(ap, format);
	ret = tegrabl_vprintf(format, ap);
	va_end(ap);

	return ret;
}

int tegrabl_putc(char ch)
{
	tegrabl_error_t error;
	if (hdev == NULL) {
		return 0;
	}

	error = tegrabl_console_putchar(hdev, ch);
	if (error != TEGRABL_NO_ERROR) {
		return 0;
	}
	return 1;
}

int tegrabl_getc(void)
{
	tegrabl_error_t error;
	char ch;

	if (hdev == NULL) {
		return -1;
	}

	error = tegrabl_console_getchar(hdev, &ch);
	if (error != TEGRABL_NO_ERROR) {
		return -1;
	}
	return ch;
}
