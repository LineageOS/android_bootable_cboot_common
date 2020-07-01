/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
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

#if defined(CONFIG_DEBUG_TIMESTAMP)
	static bool enable_timestamp = true;
#else
	static bool enable_timestamp = false;
#endif

#if !defined(CONFIG_DEBUG_PRINT_LENGTH)
#define CONFIG_DEBUG_PRINT_LENGTH 2048
#endif

static char msg[CONFIG_DEBUG_PRINT_LENGTH];
static struct tegrabl_console *hdev;

#if defined(CONFIG_ENABLE_LOGLEVEL_RUNTIME)
uint32_t tegrabl_debug_loglevel = TEGRABL_LOG_INFO;

void tegrabl_debug_set_loglevel(tegrabl_loglevel_t level)
{
	tegrabl_debug_loglevel = level;
}
#endif

bool tegrabl_enable_timestamp(bool is_timestamp_enable)
{
	bool curr_timestamp_setting;

	curr_timestamp_setting = enable_timestamp;
	enable_timestamp = is_timestamp_enable;

	return curr_timestamp_setting;
}

int tegrabl_snprintf(char *str, size_t size, const char *format, ...)
{
	int n = 0;
	va_list ap;

	va_start(ap, format);
	n = tegrabl_vsnprintf(str, size, format, ap);
	va_end(ap);
	return n;
}

int tegrabl_vprintf(const char *format, va_list ap)
{
	uint32_t size = 0;
	int32_t ret = 0;
	uint32_t i = 0;
	uint64_t msec = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (enable_timestamp) {
		size = 11;
		msec = tegrabl_get_timestamp_ms();

		ret += tegrabl_snprintf(msg, size, "[%07d] ", (uint32_t)msec);

		for (i = size; i > 5UL; i--) {
			msg[i] = msg[i - 1UL];
		}

		msg[i] = '.';
	}

	ret += tegrabl_vsnprintf(msg + size, sizeof(msg) - size, format, ap);
	err = tegrabl_console_puts(hdev, msg);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("failed to print\n");
	}
	return ret;
}

tegrabl_error_t tegrabl_debug_init(void)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	hdev = tegrabl_console_open();
	if (hdev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

	return error;
}

void tegrabl_debug_deinit(void)
{
	hdev = NULL;
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

int32_t tegrabl_puts(char *str)
{
	tegrabl_error_t error;
	if (hdev == NULL) {
		return 0;
	}

	error = tegrabl_console_puts(hdev, str);
	if (error != TEGRABL_NO_ERROR) {
		return 0;
	}
	return 1;
}

int32_t tegrabl_getc(void)
{
	tegrabl_error_t error;
	char ch;

	if (hdev == NULL) {
		return -1;
	}

	error = tegrabl_console_getchar(hdev, &ch, ~(0x0u));
	if (error != TEGRABL_NO_ERROR) {
		return -1;
	}
	return (int32_t)ch;
}

int32_t tegrabl_getc_wait(uint64_t timeout)
{
	tegrabl_error_t error;
	char ch;

	if (hdev == NULL) {
		return -1;
	}

	error = tegrabl_console_getchar(hdev, &ch, (time_t)timeout);
	if (error != TEGRABL_NO_ERROR) {
		return -1;
	}
	return (int32_t)ch;
}
