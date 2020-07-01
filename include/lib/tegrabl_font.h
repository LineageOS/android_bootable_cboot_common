/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_FONT_H
#define TEGRABL_FONT_H

/* Include header file of the supported fonts.
   Currently supporting only one font type */
#include <tegrabl_font_default.h>

/**
 *  @brief enum for different text fonts supported
 *
 *  We can add more font types later if supported.
 */
enum tegrabl_font_type {
	tegrabl_default = 0,
};

#endif
