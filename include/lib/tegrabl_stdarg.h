/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_STDARG_H
#define INCLUDED_TEGRABL_STDARG_H

#include <stdarg.h>
#include <stddef.h>

/**
 * @brief vsnprintf- function to print formatted output to a character
 * string
 *
 * @param str output string buffer
 * @param size length of the string buffer including null byte. the output
 * would be truncated to this length.
 * @param format the format string specifying how subsequent arguments are
 * converted for outputg
 * @param ap additional arguments to be formatted as per the format string
 *
 * @return the number of characters printed (excluding the null byte used
 * to end output to strings)
 */
int tegrabl_vsnprintf(char *buf, size_t size, const char *format, va_list ap);

#endif
