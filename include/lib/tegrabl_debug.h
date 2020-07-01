/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_DEBUG_H
#define INCLUDED_TEGRABL_DEBUG_H

#ifndef NO_BUILD_CONFIG
#include "build_config.h"
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <tegrabl_error.h>
#include <tegrabl_compiler.h>

/**
 * @brief Initialize the debug console
 *
 * @return On success return TEGRABL_NO_ERROR otherwise appropriate error
 */
tegrabl_error_t tegrabl_debug_init(void);

/**
 * @brief Deinitialize the debug console
 */
void tegrabl_debug_deinit(void);

/**
 * @brief printf-like function to diplay formatted output on pre-selected debug
 * port (UART/semi-hosting/etc).
 *
 * @param format the format string specifying how subsequent arguments are
 * converted for outputg
 * @param ... additional arguments to be formatted as per the format string
 *
 * @return the number of characters printed (excluding the null byte used
 * to end output to strings)
 */
int tegrabl_printf(const char *format, ...) TEGRABL_PRINTFLIKE(1,2);

/**
 * @brief snprintf-like function to print formatted output to a character
 * string
 *
 * @param str output string buffer
 * @param size length of the string buffer including null byte. the output
 * would be truncated to this length.
 * @param format the format string specifying how subsequent arguments are
 * converted for outputg
 * @param ... additional arguments to be formatted as per the format string
 *
 * @return the number of characters printed (excluding the null byte used
 * to end output to strings)
 */
int tegrabl_snprintf(char *str, size_t size, const char *format, ...) \
		TEGRABL_PRINTFLIKE(3,4);

/**
 * @brief Log-levels for use with tegrabl_log_printf
 *
 * NOTE: Please define a pr_<loglevel> API everytime you add a new loglevel
 */
typedef uint32_t tegrabl_loglevel_t;
#define TEGRABL_LOG_CRITICAL 1U
#define TEGRABL_LOG_ERROR 2U
#define TEGRABL_LOG_WARN 3U
#define TEGRABL_LOG_INFO 4U
#define TEGRABL_LOG_DEBUG 5U

/**
 * @brief printf-like function for printing formatted output with different log-levels
 *
 * @param loglevel identifies the type of message being printed (allows certain
 * type of messages to be filtered/disabled)
 * @param fmt format string (same as format in tegrabl_printf)
 * @param ... variable arguments (same as tegrabl_printf
 */
#if defined(CONFIG_ENABLE_LOGLEVEL_RUNTIME)
	extern uint32_t tegrabl_debug_loglevel;

	#define tegrabl_log_printf(level, fmt, ...)				\
		do {												\
			if ((level) <= tegrabl_debug_loglevel) {			\
				(void)tegrabl_printf(fmt, ## __VA_ARGS__);	\
			}												\
		} while (false)

#else

	#if defined(CONFIG_DEBUG_LOGLEVEL)
		#define TEGRABL_CURRENT_LOGLEVEL	CONFIG_DEBUG_LOGLEVEL
	#else
		#define TEGRABL_CURRENT_LOGLEVEL	TEGRABL_LOG_INFO
	#endif

	#define tegrabl_log_printf(level, fmt, ...)				\
		do {												\
			if ((level) <= TEGRABL_CURRENT_LOGLEVEL) {		\
				(void)tegrabl_printf(fmt, ## __VA_ARGS__);	\
			}												\
		} while (false)
#endif

/* Convenience macros around tegrabl_log_printf */
#define pr_critical(fmt, ...)	tegrabl_log_printf(TEGRABL_LOG_CRITICAL, "C> " fmt, ## __VA_ARGS__)

#define pr_error(fmt, ...)		tegrabl_log_printf(TEGRABL_LOG_ERROR, "E> " fmt, ## __VA_ARGS__)

#define pr_warn(fmt, ...)		tegrabl_log_printf(TEGRABL_LOG_INFO, "W> " fmt, ## __VA_ARGS__)

#define pr_info(fmt, ...)		tegrabl_log_printf(TEGRABL_LOG_INFO, "I> " fmt, ## __VA_ARGS__)

#define pr_debug(fmt, ...)		tegrabl_log_printf(TEGRABL_LOG_DEBUG, "D> " fmt, ## __VA_ARGS__)

#if defined(CONFIG_ENABLE_DEBUG_TRACE)
	#define pr_trace(fmt, ...)	tegrabl_printf("%s: %s: %d: " fmt, "TRACE", __func__, __LINE__, ## __VA_ARGS__)
#else
	#define pr_trace(fmt, ...)
#endif

/**
 * @brief Set log level
 *
 * @param level log level
 */
#if defined(CONFIG_ENABLE_LOGLEVEL_RUNTIME)
void tegrabl_debug_set_loglevel(tegrabl_loglevel_t level);
#endif

/**
 * @brief Assert a condition, i.e. blocks execution if the specified condition
 * is not true
 *
 * @param condition The condition to be asserted
 */
#if defined(CONFIG_ENABLE_ASSERTS)
#define TEGRABL_ASSERT(condition)											\
	do {																	\
		if (!(condition)) {													\
			print_assert_fail(FILENAME, __LINE__);							\
			while (true) {													\
				;															\
			}																\
		}																	\
	} while (false)
#else
#define TEGRABL_ASSERT(condition)
#endif

#define TEGRABL_BUG(...)							\
	do {											\
		pr_error("BUG: " __VA_ARGS__);				\
		TEGRABL_ASSERT(false);							\
	} while (false)

/**
* @brief putc like function to display char on debug port
*
* @param ch char to be put on debug port.
*
* @return 1 if success, 0 in case of failure
*/
int tegrabl_putc(char ch);

/**
* @brief puts function to display string on debug port
*
* @param str string to be put on debug port.
*
* @return 1 if success, 0 in case of failure
*/
int32_t tegrabl_puts(char *str);

/**
* @brief getc like function to read char on debug port
*
* @return the read character, else -1 on failure
*/
int32_t tegrabl_getc(void);
int32_t tegrabl_getc_wait(uint64_t timeout);

/**
 * @brief Produces output according to a format and parameters
 * passed in variable argument list and write to output stream.
 *
 * @param format format of print
 * @param ap variable argument list
 *
 * @return number of characters written to output stream
 */
int tegrabl_vprintf(const char *format, va_list ap);

/**
 * @brief Enable/Disable timestamp print in logs at runtime
 *
 * @param is_timestamp_enable enable/disable the timestamp
 *
 * @return old enable_timestamp variable configuration
 */
bool tegrabl_enable_timestamp(bool is_timestamp_enable);

#endif /* INCLUDED_TEGRABL_DEBUG_H */
