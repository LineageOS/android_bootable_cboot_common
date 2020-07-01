/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef INCLUDED_INTTTYPES_H
#define INCLUDED_INTTTYPES_H

#include <stdint.h>

#if defined(__x86_64__) || defined(__aarch64__)
#define TEGRABL_PRI64_PREFIX	"l"
#define TEGRABL_PRIPTR_PREFIX	"l"
#else
#define TEGRABL_PRI64_PREFIX	"ll"
#define TEGRABL_PRIPTR_PREFIX
#endif

/* Format specifier macros for printing fixed length integers */

/* Decimal notation.  */
# define PRId8		"d"
# define PRId16		"d"
# define PRId32		"d"
# define PRId64		TEGRABL_PRI64_PREFIX "d"

# define PRIi8		"i"
# define PRIi16		"i"
# define PRIi32		"i"
# define PRIi64		TEGRABL_PRI64_PREFIX "i"

/* Octal notation.  */
# define PRIo8		"o"
# define PRIo16		"o"
# define PRIo32		"o"
# define PRIo64		TEGRABL_PRI64_PREFIX "o"

/* Unsigned integers.  */
# define PRIu8		"u"
# define PRIu16		"u"
# define PRIu32		"u"
# define PRIu64		TEGRABL_PRI64_PREFIX "u"

/* lowercase hexadecimal notation.  */
# define PRIx8		"x"
# define PRIx16		"x"
# define PRIx32		"x"
# define PRIx64		TEGRABL_PRI64_PREFIX "x"

/* UPPERCASE hexadecimal notation.  */
# define PRIX8		"X"
# define PRIX16		"X"
# define PRIX32		"X"
# define PRIX64		TEGRABL_PRI64_PREFIX "X"

/* Macros for printing `intptr_t' and `uintptr_t'.  */
# define PRIdPTR	TEGRABL_PRIPTR_PREFIX "d"
# define PRIiPTR	TEGRABL_PRIPTR_PREFIX "i"
# define PRIoPTR	TEGRABL_PRIPTR_PREFIX "o"
# define PRIuPTR	TEGRABL_PRIPTR_PREFIX "u"
# define PRIxPTR	TEGRABL_PRIPTR_PREFIX "x"
# define PRIXPTR	TEGRABL_PRIPTR_PREFIX "X"

#endif // INCLUDED_INTTTYPES_H
