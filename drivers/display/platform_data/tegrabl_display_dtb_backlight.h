/*
 * Copyright (c) 2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TEGRABL_DISPLAY_DTB_BACKLIGHT_H
#define __TEGRABL_DISPLAY_DTB_BACKLIGHT_H

#include <tegrabl_error.h>
#include <tegrabl_display_dtb.h>

 /**
 * @brief Enable backlight for EDP
 */
#if defined(CONFIG_ENABLE_EDP)
tegrabl_error_t tegrabl_display_panel_backlight_enable(void);
#endif

#endif
