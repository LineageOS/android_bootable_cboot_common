/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All Rights Reserved.
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
#include <tegrabl_io.h>
#include <tegrabl_error.h>
#include <tegrabl_compiler.h>

/**
* @brief Initializes the debug console
*
* @param is_enable Gives whether to enable or disable debug prints.
*
* @return TEGRABL_NO_ERROR if success, error code in case of failure
*/
tegrabl_error_t tegrabl_debug_init(bool is_enable);

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
enum {
	TEGRABL_LOG_CRITICAL,
	TEGRABL_LOG_ERROR,
	TEGRABL_LOG_WARN,
	TEGRABL_LOG_INFO,
	TEGRABL_LOG_DEBUG,
};

#if !defined(CONFIG_DEBUG_LOGLEVEL)
#define TEGRABL_CURRENT_LOGLEVEL	TEGRABL_LOG_INFO
#else
#define TEGRABL_CURRENT_LOGLEVEL	CONFIG_DEBUG_LOGLEVEL
#endif

/**
 * @brief printf-like function for printing formatted output with different
 * log-levels
 *
 * @param loglevel identifies the type of message being printed (allows certain
 * type of messages to be filtered/disabled)
 * @param fmt format string (same as format in tegrabl_printf)
 * @param ... variable arguments (same as tegrabl_printf
 *
 * @return the number of characters printed (excluding the null byte used
 * to end output to strings)
 */
#define tegrabl_log_printf(loglevel, fmt, ...)			\
	do {												\
		if ((loglevel) <= TEGRABL_CURRENT_LOGLEVEL) {	\
			tegrabl_printf(fmt, ##__VA_ARGS__);			\
		}												\
	} while(0)

/* Convenience macros around tegrabl_log_printf */
#define pr_critical(fmt, ...) \
			tegrabl_log_printf(TEGRABL_LOG_CRITICAL, "C> " fmt, ##__VA_ARGS__)

#define pr_error(fmt, ...) \
			tegrabl_log_printf(TEGRABL_LOG_ERROR, "E> " fmt, ##__VA_ARGS__)

#define pr_warn(fmt, ...) \
			tegrabl_log_printf(TEGRABL_LOG_INFO, "W> " fmt, ##__VA_ARGS__)

#define pr_info(fmt, ...) \
			tegrabl_log_printf(TEGRABL_LOG_INFO, "I> " fmt, ##__VA_ARGS__)

#define pr_debug(fmt, ...) \
			tegrabl_log_printf(TEGRABL_LOG_DEBUG, "D> " fmt, ##__VA_ARGS__)

/**
 * @brief Debug macro to print the function name and line number when the
 * macro TEGRABL_LOCAL_DEBUG is defined in the including source file.
 */
#if defined(TEGRABL_LOCAL_DEBUG)
#define TEGRABL_LTRACE	tegrabl_printf("%s: %d\n", __func__, __LINE__)
#else
#define TEGRABL_LTRACE
#endif

/**
 * @brief Debug function that acts as wrapper on top of NV_WRITE32 and allows
 * conditionally logging (i.e. if TEGRABL_TRACE_REG_RW is defined in including
 * source file) the address/value of the register being written to.
 *
 * @param addr address of the register being written
 * @param val 32bit data to be written to the register
 */
static TEGRABL_INLINE void tegrabl_trace_write32(volatile uint32_t addr,
		uint32_t val)
{
#if defined(TEGRABL_TRACE_REG_RW)
	pr_debug("%s: [0x%08"PRIx32"] <= 0x%08"PRIx32"\n", __func__, addr, val);
#endif
	NV_WRITE32(addr, val);
}

/**
 * @brief Debug function that acts as wrapper on top of NV_READ32 and allows
 * conditionally logging (i.e. if TEGRABL_TRACE_REG_RW is defined in including
 * source file) the address/value of the register being read from.
 *
 * @param addr address of the register being read
 *
 * @return 32bit data read from the register
 */
static TEGRABL_INLINE uint32_t tegrabl_trace_read32(volatile uint32_t addr)
{
	uint32_t val = NV_READ32(addr);
#if defined(TEGRABL_TRACE_REG_RW)
	pr_debug("%s: [0x%08"PRIx32"] <= 0x%08"PRIx32"\n", __func__, addr, val);
#endif
	return val;
}

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
			pr_error("Assertion failed at %s:%d\n", __func__, __LINE__);	\
			while (1)														\
				;															\
		}																	\
	} while(0)
#else
#define TEGRABL_ASSERT(condition)
#endif

#define TEGRABL_BUG(...)							\
	do {											\
		pr_error("BUG: " __VA_ARGS__);				\
		TEGRABL_ASSERT(0);							\
	} while (0)

/**
* @brief putc like function to display char on debug port
*
* @param ch char to be put on debug port.
*
* @return 1 if success, 0 in case of failure
*/
int tegrabl_putc(char ch);

/**
* @brief getc like function to read char on debug port
*
* @return the read character, else -1 on failure
*/
int tegrabl_getc(void);

#endif /* INCLUDED_TEGRABL_DEBUG_H */
