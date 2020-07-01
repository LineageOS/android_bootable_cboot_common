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
#include_next "stdbool.h"

#else

#ifndef INCLUDED_STDBOOL_H
#define INCLUDED_STDBOOL_H

#ifndef bool
typedef unsigned char bool;
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#endif /* INCLUDED_STDBOOL_H */

#endif

