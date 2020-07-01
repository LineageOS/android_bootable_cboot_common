/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TEGRABL_DISPLAY_PANEL_H
#define __TEGRABL_DISPLAY_PANEL_H

#include <tegrabl_error.h>
#include <tegrabl_display_dtb.h>

#define TEGRABL_HDMI_HOTPLUG_MASK (1 << 1)

/**
 * @brief Initialize display regulators
 *
 * @param du_type display unit type.
 * @param pdata platform data containing power rail info.
 *
 * @return TEGRABL_NO_ERROR if pass, error code if fails.
 */
tegrabl_error_t tegrabl_display_init_regulator(uint32_t du_type, struct tegrabl_display_pdata *pdata);
#endif
