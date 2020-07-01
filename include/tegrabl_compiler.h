/*
 * Copyright (c) 2015-2016, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_COMPILER_H
#define INCLUDED_TEGRABL_COMPILER_H

#ifdef __GNUC__

#define TEGRABL_INLINE 				__inline__
#define TEGRABL_FORCE_INLINE		__attribute__((always_inline)) __inline__

#define TEGRABL_ALIGN(size)			 __attribute__ ((aligned (size)))
#define TEGRABL_DECLARE_ALIGNED(var, size) var __attribute__ ((aligned (size)))
#define TEGRABL_PACKED(var) 		var __attribute__((packed))

#define TEGRABL_PRINTFLIKE(__fmt,__varargs)	\
					__attribute__((__format__ (__printf__, __fmt, __varargs)))

#define TEGRABL_NAKED				__attribute__((naked))

#ifndef va_start
#define va_start(v, l)				__builtin_va_start((v),(l))
#endif

#ifndef va_end
#define va_end						__builtin_va_end
#endif

#ifndef va_arg
#define va_arg						__builtin_va_arg
#endif

#ifndef clz
#define clz							__builtin_clz
#endif

#elif defined(__arm)

#define TEGRABL_INLINE				__inline
#define TEGRABL_FORCE_INLINE		__forceinline

#define TEGRABL_ALIGN(size)			__align(size)
#define TEGRABL_DECLARE_ALIGNED(var, size) var __align(size)
#define TEGRABL_PACKED(var) var

#define TEGRABL_NAKED

#define TEGRABL_PRINTFLIKE(__fmt, __varargs)
#include <stdarg.h>

#elif defined(_MSC_VER)

#define TEGRABL_INLINE				__inline
#define TEGRABL_FORCE_INLINE		__forceinline

#define TEGRABL_DECLARE_ALIGNED(var, size) \
						__declspec(align(size)) var
#define TEGRABL_PACKED(var) 		\
						__pragma(pack(push, 1)) var __pragma(pack(pop))

#define TEGRABL_NAKED

#define TEGRABL_PRINTFLIKE(__fmt, __varargs)

#include <stdarg.h>

#else

#define TEGRABL_INLINE
#define TEGRABL_FORCE_INLINE

#define TEGRABL_ALIGN(size)
#define TEGRABL_DECLARE_ALIGNED(var, size) var
#define TEGRABL_PACKED(var) var

#define TEGRABL_NAKED

#define TEGRABL_PRINTFLIKE(__fmt, __varargs)
#endif

#ifdef __GNUC__    /* Excluded from previous to avoid MISRA error */
typedef __builtin_va_list   va_list;
#endif

#define TEGRABL_UNUSED(var)			((void)var)

#endif /*INCLUDED_TEGRABL_COMPILER_H*/
