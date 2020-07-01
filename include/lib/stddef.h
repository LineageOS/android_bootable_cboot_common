/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifdef __GNUC__

/* Use GCC's version */
#include_next "stddef.h"

#else

#ifndef INCLUDED_STDDEF_H
#define INCLUDED_STDDEF_H

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef unsigned long size_t;

typedef long int ptrdiff_t;

/* Offset of member MEMBER in a struct of type TYPE. */
#define offsetof(TYPE, MEMBER) ((size_t)(&((TYPE*)0)->MEMBER))

typedef unsigned short wchar_t;

#endif // INCLUDED_STDDEF_H

#endif

