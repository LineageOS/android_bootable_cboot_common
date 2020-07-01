/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifdef __GNUC__

/* Use GCC's version */
#include_next "stdbool.h"

/* Data type of true/false in GCC's version is signed int. Redefine them. */
#undef true
#undef false

#define false (1 == 0)
#define true (!false)
#else

#ifndef INCLUDED_STDBOOL_H
#define INCLUDED_STDBOOL_H

#ifndef bool
typedef unsigned char bool;
#endif

#ifndef true
#define true 1U
#endif

#ifndef false
#define false 0U
#endif

#endif /* INCLUDED_STDBOOL_H */

#endif

