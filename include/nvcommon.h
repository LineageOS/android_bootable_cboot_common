/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCOMMON_H
#define INCLUDED_NVCOMMON_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include <stdint.h>

/// OS-related defines.
#if defined(_WIN32) && !defined(NVOS_IS_WINDOWS)
  #define NVOS_IS_WINDOWS 1

#elif defined(__linux__)
  #if !defined(NVOS_IS_LINUX)
    #define NVOS_IS_LINUX 1
    #define NVOS_IS_UNIX 1
    #if defined(__KERNEL__)
      #error nvcommon.h no longer supports the Linux kernel
    #endif
  #endif
#elif defined(__QNX__) || defined(__QNXNTO__)
  #define NVOS_IS_UNIX 1
  #define NVOS_IS_QNX 1
#elif defined(__hos__)
  #define NVOS_IS_HOS 1
#elif defined(__INTEGRITY)
#define NVOS_IS_INTEGRITY 1
#elif defined(__arm__)  && defined(__ARM_EABI__)
    /* GCC arm eabi compiler, potentially used for kernel compilation without
     * __linux__, but also for straight EABI (AOS) executable builds */
#  if defined(__KERNEL__)
#    error nvcommon.h no longer supports the Linux kernel
#  endif
    /* Nothing to define for AOS */
#elif defined(__arm)
  /// For ARM RVDS compiler, we don't know the final target OS at compile time.
#elif defined(__APPLE__)
# define NVOS_IS_DARWIN 1
# define NVOS_IS_UNIX 1
#endif

#if !defined(NVOS_IS_WINDOWS)
#define NVOS_IS_WINDOWS 0
#endif

#if !defined(NVOS_IS_LINUX)
#define NVOS_IS_LINUX 0
#endif
#if !defined(NVOS_IS_UNIX)
#define NVOS_IS_UNIX 0
#endif
#if !defined(NVOS_IS_INTEGRITY)
#define NVOS_IS_INTEGRITY 0
#endif
#if !defined(NVOS_IS_LINUX_KERNEL)
#define NVOS_IS_LINUX_KERNEL 0
#endif
#if !defined(NVOS_IS_QNX)
#define NVOS_IS_QNX 0
#endif
#if !defined(NVOS_IS_HOS)
#define NVOS_IS_HOS 0
#endif

#if defined(__aarch64__)
#define NVCPU_IS_AARCH64 1
#else
#define NVCPU_IS_AARCH64 0
#endif

/* We don't currently support any big-endian CPUs. */
#define NVCPU_IS_BIG_ENDIAN 0

#if NVCPU_IS_AARCH64
#define NVCPU_IS_64_BITS 1
#else
#define NVCPU_IS_64_BITS 0
#endif

/// Explicitly sized signed and unsigned ints.
typedef unsigned char      NvU8;  /**< 0 to 255 */
typedef unsigned short     NvU16; /**< 0 to 65535 */
typedef unsigned int       NvU32; /**< 0 to 4294967295 */
typedef unsigned long long NvU64; /**< 0 to 18446744073709551615 */
typedef signed char        NvS8;  /**< -128 to 127 */
typedef signed short       NvS16; /**< -32768 to 32767 */
typedef signed int         NvS32; /**< -2147483648 to 2147483647 */
typedef signed long long   NvS64; /**< 2^-63 to 2^63-1 */

/// Explicitly sized floats.
typedef float              NvF32; /**< IEEE Single Precision (S1E8M23) */
typedef double             NvF64; /**< IEEE Double Precision (S1E11M52) */

/// Boolean type
#define NV_FALSE 0
#define NV_TRUE 1
typedef NvU8 NvBool;

/// Pointer-sized signed and unsigned ints
#if NVCPU_IS_64_BITS
typedef NvU64 NvUPtr;
typedef NvS64 NvSPtr;
#else
typedef NvU32 NvUPtr;
typedef NvS32 NvSPtr;
#endif

#if (!defined(NVTBOOT_T124))
typedef NvU64 NvCpuReg;
typedef NvU64 NvCpuPtr;
#else
typedef NvU32 NvCpuReg;
typedef NvU32 NvCpuPtr;
#endif

/// Function attributes are lumped in here too.
/// INLINE - Make the function inline.
/// NAKED - Create a function without a prologue or an epilogue.
#if NVOS_IS_WINDOWS

#define NV_INLINE __inline
#define NV_FORCE_INLINE __forceinline
#define NV_NAKED __declspec(naked)
#define NV_LIKELY(c)   (c)
#define NV_UNLIKELY(c) (c)
#define NV_UNUSED

#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif

#elif defined(__ghs__) // GHS COMP
#define NV_INLINE inline
#define NV_FORCE_INLINE inline
#define NV_NAKED
#define NV_LIKELY(c)   (c)
#define NV_UNLIKELY(c) (c)
#define NV_UNUSED __attribute__((unused))

#elif defined(__GNUC__)
#define NV_INLINE __inline__
#define NV_FORCE_INLINE __attribute__((always_inline)) __inline__
#define NV_NAKED __attribute__((naked))
#define NV_LIKELY(c)   __builtin_expect((c),1)
#define NV_UNLIKELY(c) __builtin_expect((c),0)
#define NV_UNUSED __attribute__((unused))

#elif defined(__arm) // ARM RVDS compiler
#define NV_INLINE __inline
#define NV_FORCE_INLINE __forceinline
#define NV_NAKED __asm
#define NV_LIKELY(c)   (c)
#define NV_UNLIKELY(c) (c)
#define NV_UNUSED

#else
#error Unknown compiler
#endif

/** Macro for determining the size of an array */
#define NV_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/** Macro for taking min or max of a pair of numbers */
#ifndef NV_MIN
#define NV_MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef NV_MAX
#define NV_MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

/** Macro for determining offset of element e in struct s */
#define NV_OFFSETOF(s,e)        ((NvUPtr)(&(((s*)0)->e)))

/** Macro for determining sizeof an element e in struct s */
#define NV_SIZEOF(s,e)          (sizeof(((s*)0)->e))

/** Get just the lowest bit of the 32 bit number */
#define NV_LOWEST_BIT_ONLY(v)   ((NvU32)(v) & (NvU32)-(NvS32)(v))

/** True if unsigned int v is a power of 2 */
#define NV_IS_POWER_OF_2(v)     (NV_LOWEST_BIT_ONLY(v) == (NvU32)(v))

/** Align a variable declaration to a particular # of bytes (should
 * always be a power of two) */
#if defined(__GNUC__)
#define NV_ALIGN(size) __attribute__ ((aligned (size)))
#elif defined(__arm)
#define NV_ALIGN(size) __align(size)
#elif defined(_WIN32)
#define NV_ALIGN(size) __declspec(align(size))
#else
#error Unknown compiler
#endif


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVCOMMON_H
