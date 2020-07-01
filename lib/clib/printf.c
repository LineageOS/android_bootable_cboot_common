/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <stdint.h>
#include <string.h>
#include <tegrabl_stdarg.h>

static int integer_to_string(unsigned long long n, int sign, size_t pad,
	char *str, size_t size, int alternate_form, unsigned radix, char padchar)
{
	static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	size_t len = 0;
	size_t total;
	unsigned long long x = n;

	/* calculate length */
	do {
		x /= radix;
		len++;
	} while (x > 0);

	if (sign < 0) {
		len++;
	}

	if (alternate_form != 0) {
		if (radix == 8)
			len++;
		else if (radix == 16)
			len += 2;
	}

	if (len > pad) {
		total = len;
		pad = 0;
	} else {
		total = pad;
		pad -= len;
	}

	/* doesn't fit, just bail out */
	if (total > size) {
		return -1;
	}

	/* pad */
	while (pad > 0) {
		*str = padchar;
		str++;
		pad--;
	}
	/* write sign */
	if (sign < 0) {
		*str = '-';
		str++;
		len--;
	}

	/* prefix '0x' (hex-values) or '0' (octal-values) */
	if (alternate_form != 0) {
		if (radix == 8) {
			*str = '0';
			str++;
			len--;
		} else if (radix == 16) {
			*str = '0';
			str++;
			*str = 'x';
			str++;
			len -= 2;
		} else {
			/* No Action Required */
		}
	}

	/* write integer */
	x = n;
	while (len > 0ULL) {
		len--;
		str[len] = digits[x % radix];
		x /= radix;
	}

	return (int32_t)total;
}

int tegrabl_vsnprintf(char *buf, size_t size, const char *format, va_list ap)
{
	const char *f = format;
	char *out = buf;
	size_t remaining = size - 1;
	char padchar = ' ';
	int wrote;
	char cur;
	const char *arg = f;
	uint32_t pad = 0;
	int longint = 0;
	int sign = 0;
	uint32_t base = 0;
	int alternate_form = 0;
	unsigned long long val;
	char *ptr;
	size_t len;
	char ch;
	int32_t temp_arg;

	while (remaining > 0) {
		wrote = 0;
		cur = *f;
		f++;

		/* end of format */
		if (cur == '\0')
			break;

		/* formatted argument */
		if (cur != '%') {
			/* print string literals */
			*out = cur;
			wrote = 1;
		} else {
			base = 0;
			sign = 0;
			alternate_form = 0;
			longint = 0;
			pad = 0;

			arg = f;
			if (*arg == '#') {
				alternate_form = 1;
				arg++;
			}

			/* support for %0N zero-padding */
			if (*arg == '0') {
				padchar = '0';
				arg++;
			}
			while ((*arg >= '0') && (*arg <= '9')) {
				pad *= 10UL;
				temp_arg = *arg - '0';
				pad += (uint32_t)temp_arg;
				arg++;
			}

			if (*arg == 'p') {
				/* %p == %#lx */
				alternate_form = 1;
				longint = 1;
			}

			if (*arg == 'l') {
				arg++;
				longint = 1;
				if (*(arg) == 'l') {
					arg++;
					longint = 2;
				}
			}

			switch (*(arg++)) {
			case 'i':
			case 'd':
			case 'u':
			case 'o':
			case 'p':
			case 'x':
			case 'X':
				if ((*(arg - 1) == 'i') || (*(arg - 1) == 'd')) {
					sign = 1;
					base = 10;
				}

				else if (*(arg - 1) == 'u')
					base = 10;

				else if (*(arg - 1) == 'o')
					base = 8;

				else
					base = 16;

				if (longint == 2) {
					long long int tmp = va_arg(ap, unsigned long long int);
					if ((sign != 0) && (tmp < 0)) {
						sign = -1;
						tmp = -tmp;
					}
					val = (unsigned long long int)tmp;
				} else if (longint == 1) {
					long int tmp = va_arg(ap, unsigned long int);
					if ((sign != 0) && (tmp < 0)) {
						sign = -1;
						tmp = -tmp;
					}
					val = (unsigned long int)tmp;
				} else {
					int tmp = va_arg(ap, unsigned int);
					if ((sign != 0) && (tmp < 0)) {
						sign = -1;
						tmp = -tmp;
					}
					val = (unsigned int)tmp;
				}

				wrote = integer_to_string(val, sign, pad, out, remaining,
					alternate_form, base, padchar);
				break;
			case 's':
				ptr = va_arg(ap, char *);
				len = strlen(ptr);

				if (len > remaining) {
					wrote = -1;
				} else {
					strcpy(out, ptr);
					wrote = (int32_t)len;
					if (wrote == 0) {
						f = arg;
						wrote = -2;
					}
				}
				break;
			case 'c':
				ch = (char)va_arg(ap, unsigned);
				*out = ch;
				wrote = 1;
				break;
			case '%':
				*out = '%';
				wrote = 1;
				break;
			default:
				/* unsupported, just return with whatever already printed */
				wrote = -1;
				break;
			}

			if (wrote >= 0) {
				f = arg;
			}
		}

		if (wrote == -1) {
			break;
		}

		/* put this after the previous if statement (wrote == 0) */
		if (wrote == -2)
			wrote = 0;

		remaining -= wrote;
		out += wrote;
	}

	*out = '\0';
	return ((int32_t)size - 1) - (int32_t)remaining;
}
