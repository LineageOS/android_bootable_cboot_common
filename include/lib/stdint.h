/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_STDINT_H
#define INCLUDED_STDINT_H

#include <libc_limits.h>  /* for ULONG_MAX */

#if (defined(__GNUC__) && (__GNUC__ != 0))

#if defined(__arm__)
/* different versions of gcc ARM port define INT32_TYPE in different manner,
 * some define it as long and some define it as int. Hence redefining them
 * as per our understanding  */
#ifdef __INT32_TYPE__
#undef __INT32_TYPE__
#define __INT32_TYPE__			int
#endif

#ifdef __UINT32_TYPE__
#undef __UINT32_TYPE__
#define __UINT32_TYPE__			unsigned int
#endif

#ifdef __UINTPTR_TYPE__
#undef __UINTPTR_TYPE__
#define __UINTPTR_TYPE__		unsigned long
#endif

#endif  // defined(__arm__)

/* Use GCC's internal definitions */
#include "stdint-gcc.h"

#else

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long long	uint64_t;
typedef signed char			int8_t;
typedef short				int16_t;
typedef int					int32_t;
typedef long long			int64_t;

typedef long				intptr_t;
typedef unsigned long		uintptr_t;

typedef long long intmax_t;
typedef unsigned long long uintmax_t;

#define SIZE_MAX ULONG_MAX

#endif

#endif // #ifndef INCLUDED_STDINT_H

/* The following 4 macros are helper macros used to convert
 * signed integer macros to unsigned integer constants */

#define temp8(x)  (x##U)
#define temp16(x) (x##U)
#define temp32(x) (x##UL)
#define temp64(x) (x##ULL)

/* Use these macros to convert a  signed integer macros to unsigned integer constants*/

#define U8(x)  temp8(x)
#define U16(x) temp16(x)
#define U32(x) temp32(x)
#define U64(x) temp64(x)
