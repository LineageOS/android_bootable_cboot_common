/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_STRING_H
#define INCLUDED_STRING_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @brief DMA callback routine for memcpy optimization
 *
 * @param priv private data memcpy_callback_priv
 * @param dest destination buffer
 * @param src dource buffer
 * @param n number of bytes to be copied
 *
 * @return 0 in case of success, non-0 in case of failure (this would be
 * used to do cpu-based memcpy)
 */
typedef int (*clib_dma_memcpy_t)(void *priv, void *dest,
								 const void *src, size_t n);

/**
 * @brief DMA callback routine for memset optimization
 *
 * @param priv private data memset_callback_priv
 * @param dest destination buffer
 * @param c character to set each byte to
 * @param n number of bytes to be set
 *
 * @return 0 in case of success, non-0 in case of failure (this would be
 * used to do cpu-based memset)
 */
typedef int (*clib_dma_memset_t)(void *priv, void *s, uint32_t c, size_t n);

struct tegrabl_clib_dma {
	/**
	 * @brief Minimal number of bytes for which memcpy_callback should be
	 * invoked.
	 */
	size_t memcpy_threshold;

	/**
	 * @brief Minimal number of bytes for which memset_callback should be
	 * invoked.
	 */
	size_t memset_threshold;

	/**
	 * @brief Private data to be passed to memcpy callback. This could be used
	 * to provide additional info like DMA engine instance, channel.
	 */
	void *memcpy_priv;

	/**
	 * @brief Private data to be passed to memset callback. This could be used
	 * to provide additional info like DMA engine instance, channel.
	 */
	void *memset_priv;

	/**
	 * @brief DMA callback routine for memcpy optimization
	 */
	clib_dma_memcpy_t memcpy_callback;

	/**
	 * @brief DMA callback routine for memset optimization
	 */
	clib_dma_memset_t memset_callback;

};

/**
 * @brief Register DMA callbacks and the parameters based on which those should
 * be invoked.
 *
 * @param dma_info pointer to the structure describing dma operations/parameters
 */
void tegrabl_clib_dma_register(struct tegrabl_clib_dma *dma_info);

/* Copy N bytes of SRC to DEST.  */
extern void *memcpy(void *dest, const void *src, size_t n);

/* Copy N bytes of SRC to DEST, guaranteeing
   correct behavior for overlapping strings.  */
extern void *memmove(void *dest, const void *src, size_t n);

/* Set N bytes of S to C.  */
extern void *memset(void *s, int c, size_t n);

/* Compare N bytes of S1 and S2.  */
extern int memcmp(const void *s1, const void *s2, size_t n);

/* Search N bytes of S for C.  */
extern void *memchr(const void *s, int c, size_t n);

/* Copy SRC to DEST.  */
extern char *strcpy(char *dest, const char *src);

/* Copy no more than N characters of SRC to DEST.  */
extern char *strncpy(char *dest, const char *src, size_t n);

/* Copy no more than N-1 characters of SRC to DEST and NUL terminate it */
extern size_t strlcpy(char *dest, char const *src, size_t n);

/* Append SRC onto DEST.  */
extern char *strcat(char *dest, const char *src);

/* Append no more than N characters from SRC onto DEST.  */
extern char *strncat(char *dest, const char *src, size_t n);

/* Compare S1 and S2.  */
extern int strcmp(const char *s1, const char *s2);

/* Compare N characters of S1 and S2.  */
extern int strncmp(const char *s1, const char *s2, size_t n);

/* Find the first occurrence of C in S.  */
extern char *strchr(const char *s, int c);

/* Return the length of S.  */
extern size_t strlen(const char *s);

/* Find the first occurrence of SUB in STR  */
extern char *strstr(const char *str, const char *sub);

/* Find the last occurence of char c in string str */
extern char *strrchr(const char *str, int c);

/* calculates the length of the initial segment of s1 which consists
 * entirely of characters in s2.
 */
extern size_t strspn(char const *s1, char const *s2);

/* finds the first character in the string cs that matches any
 * character specified in ct.
 */
extern char *strpbrk(char const *cs, char const *ct);

/* Split string based on a delimiter */
extern char *strtok(const char *str, const char *delim);

int tegrabl_clib_test_memcpy(size_t maxsize, bool alloc, void *testbuf);

int tegrabl_clib_test_memset(size_t maxsize, bool alloc, void *testbuf);


#endif // INCLUDED_STRING_H

