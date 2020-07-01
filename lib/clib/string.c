/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/*
 * Copyright (c) 2008-2013 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef long word;

#define lsize sizeof(word)
#define lmask (lsize - 1)

clib_dma_memcpy_t clib_dma_memcpy_callback = NULL;
size_t clib_dma_memcpy_threshold = 0;
void *clib_dma_memcpy_priv = 0;
clib_dma_memset_t clib_dma_memset_callback = NULL;
size_t clib_dma_memset_threshold = 0;
void *clib_dma_memset_priv = 0;

void tegrabl_clib_dma_register(struct tegrabl_clib_dma *dma_info)
{
	if (dma_info != NULL) {
		clib_dma_memcpy_callback = dma_info->memcpy_callback;
		clib_dma_memcpy_threshold = dma_info->memcpy_threshold;
		clib_dma_memcpy_priv = dma_info->memcpy_priv;
		clib_dma_memset_callback = dma_info->memset_callback;
		clib_dma_memset_threshold = dma_info->memset_threshold;
		clib_dma_memset_priv = dma_info->memset_priv;
	}
}

#if !defined(__ARM_ARCH_7R__)
void *memset(void *s, int c, size_t n)
{
	char *xs = (char *)s;
	size_t len = (-(word)s) & lmask;
	int not_done = 1;
	word cc = c & 0xff;

	if (n > len) {
		n -= len;
		cc |= cc << 8;
		cc |= cc << 16;
#if (__SIZEOF_LONG__ == 8)
		cc |= cc << 32;
#endif

		/* write to non-aligned memory byte-wise */
		for (; len > 0; len--) {
			*xs++ = c;
		}

		len = (n / lsize);

		if (clib_dma_memset_callback &&
			((len * lsize) >= clib_dma_memset_threshold)) {
			not_done = clib_dma_memset_callback(clib_dma_memset_priv, xs,
												c, len * lsize);
		}

		if (not_done) {
			/* write to aligned memory word-wise */
			while (len--) {
				*((word *)xs) = cc;
				xs += lsize;
			}
		} else {
			xs += (len * lsize);
		}

		n &= lmask;
	}

	/* write remaining bytes */
	for (; n > 0; n--) {
		*xs++ = c;
	}

	return s;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	char *d = (char *)dest;
	const char *s = (const char *)src;
	size_t len;
	int not_done = 1;

	if (n == 0 || dest == src) {
		return dest;
	}

	if (!src || !dest) {
		return NULL;
	}

	if (((uintptr_t)d | (uintptr_t)s) & lmask) {
		/* src and/or dest do not align on word boundary */
		if ((((uintptr_t)d ^ (uintptr_t)s) & lmask) || (n < lsize))
			len = n; /* copy the rest of the buffer with the byte mover */
		else /* move ptrs up to word boundary */
			len = lsize - ((uintptr_t)d & lmask);

		n -= len;
		for (; len > 0; len--)
			*d++ = *s++;
	}

	len = (n / lsize);

	if (clib_dma_memcpy_callback &&
		((len * lsize) >= clib_dma_memcpy_threshold))
		not_done = clib_dma_memcpy_callback(clib_dma_memcpy_priv, d,
											s, len * lsize);

	if (not_done) {
		while (len--) {
			*(word *)d = *(word *)s;
			d += lsize;
			s += lsize;
		}
	} else {
		d += (len * lsize);
		s += (len * lsize);
	}

	for (len = (n & lmask); len > 0; len--)
		*d++ = *s++;

	return dest;
}
#endif

static void *rmemcpy(void *dest, const void *src, size_t n)
{
	char *d = (char *)dest + n;
	const char *s = (const char *)src +n;
	int len;

	if (n == 0 || dest == src)
		return dest;

	if ((src == NULL) || (dest == NULL))
		return NULL;

	if ((((uintptr_t)d | (uintptr_t)s) & lmask) != 0U) {
		/* src and/or dest do not align on word boundary */
		if (((((uintptr_t)d ^ (uintptr_t)s) & lmask) != 0U) || (n < lsize))
			len = n; /* copy the rest of the buffer with the byte mover */
		else /* move ptrs up to word boundary */
			len = ((uintptr_t)d & lmask);

		n -= len;
		for (; len > 0; len--) {
			*--d = *--s;
		}
	}
	for (len = (n / lsize); len > 0; len--) {
		d -= lsize;
		s -= lsize;
		*(word *)d = *(word *)s;
	}
	for (len = (n & lmask); len > 0; len--) {
		*--d = *--s;
	}

	return dest;
}
int memcmp(const void *s1, const void *s2, size_t n)
{
	const char* p1 = s1;
	const char* end1 = p1 + n;
	const char* p2 = s2;
	int d = 0;

	for (;;) {
		if ((d != 0) || (p1 >= end1))
			break;
		d = (int)*p1++ - (int)*p2++;

		if ((d != 0) || (p1 >= end1))
			break;
		d = (int)*p1++ - (int)*p2++;

		if ((d != 0) || (p1 >= end1))
			break;
		d = (int)*p1++ - (int)*p2++;

		if ((d != 0) || (p1 >= end1))
			break;
		d = (int)*p1++ - (int)*p2++;
	}
	return d;
}

void* memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = s;

	if (s == NULL)
		return NULL;

	while (n-- != 0) {
		if ((unsigned char)c == *p++) {
			return (void *)(p - 1);
		}
	}
	return NULL;
}

void* memmove(void *dest, const void *src, size_t n)
{
	if ((dest == NULL) || (src == NULL))
		return NULL;

	if (dest == src)
		return dest;

	if ((dest < src ) || (dest > (void *)((char *)src + n)))
		return memcpy(dest, src, n);
	else
		return rmemcpy(dest, src, n);

}

char* strcat(char* dest, const char* src)
{
	char* save = dest;
	char ch;

	while (*dest != '\0') {
		++dest;
	}
	do {
		ch = *dest = *src;
		dest++;
		src++;
	} while (ch != '\0');

	return save;
}

int strcmp(const char* s1, const char* s2)
{
	while (*s1 == *s2++) {
		if (*s1++ == 0)
			return 0;
	}

	return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}

int strncmp(const char* s1, const char* s2, size_t n)
{
	if (n == 0)
		return 0;

	do {
		if (*s1 != *s2++)
			return (*(unsigned char *)s1 - *(unsigned char *)--s2);
		if (*s1++ == 0)
			break;
	} while (--n != 0);

	return 0;
}

char* strcpy(char* dest, const char* src)
{
	char* save = dest;
	while ((*dest = *src) != '\0') {
		++src;
		++dest;
	}

	return save;
}

char* strncpy(char* dest, const char* src, size_t n)
{
	if (n != 0) {
		size_t offset = 0UL;
		char ch;

		do {
			ch = (char)(src[offset]);
			dest[offset] = ch;
			offset++;
			if (ch == '\0') {
				/* NUL pad remaining n-offset bytes */
				for (; offset < n; offset++) {
					dest[offset] = '\0';
				}
			}
		} while (offset < n);
	}

	return dest;
}

size_t strlen(const char* str)
{
	const char* s = str;

	while (*s != '\0') {
		s++;
	}

	return (s - str);
}

size_t strlcpy(char *dest, char const *src, size_t n)
{
	size_t i = 0;

	if (n == 0UL) {
		return strlen(src);
	}

	for (i = 0; ((i < (n-1)) && (*(src+i) != 0)); i++) {
		*(dest+i) = *(src+i);
	}

	*(dest+i)= 0;

	return i + strlen(src+i);
}

char *strchr(const char *s, int c)
{
	if (s == NULL) {
		return NULL;
	}

	for (; *s != (char)c; ++s) {
		if (*s == '\0')
			return NULL;
	}

	return (char *)s;
}

char *strstr(const char *str, const char *sub)
{
	char *a, *b;

	b = (char *)sub;
	if (*b == 0) {
		return (char *)str;
	}

	for (; *str != '\0'; str++) {
		if (*str != *b) {
			continue;
		}

		a = (char *)str;
		while (*a++ == *b++) {
			if (*b == 0) {
				return (char *)str;
			}
		}

		b = (char *)sub;
	}

	return (char *) 0;
}

char *strrchr(const char *s, int c)
{
	const char *last = s + strlen(s);

	do {
		if (*last == (char)c) {
			return (char *)last;
		}
	} while (--last >= s);

	return NULL;
}

size_t strspn(char const *s1, char const *s2)
{
	const char *p;
	const char *a;
	size_t count = 0;

	for (p = s1; *p != '\0'; ++p) {
		for (a = s2; *a != '\0'; ++a) {
			if (*p == *a) {
				break;
			}
		}
		if (*a == '\0')
			return count;
		++count;
	}

	return count;
}

char *strpbrk(char const *cs, char const *ct)
{
	const char *sc1;
	const char *sc2;

	for (sc1 = cs; *sc1 != '\0'; ++sc1) {
		for (sc2 = ct; *sc2 != '\0'; ++sc2) {
			if (*sc1 == *sc2) {
				return (char *)sc1;
			}
		}
	}

	return NULL;
}

char *strtok(const char *str, const char *delim)
{
	static char *___strtok;
	char *sbegin, *send;

	sbegin  = str ? (char *)str : ___strtok;
	if (sbegin == NULL) {
		return NULL;
	}

	sbegin += strspn(sbegin, delim);
	if (*sbegin == '\0') {
		___strtok = NULL;
		return NULL;
	}
	send = strpbrk(sbegin, delim);
	if (send && *send != '\0')
		*send++ = '\0';
	___strtok = send;
	return sbegin;
}
