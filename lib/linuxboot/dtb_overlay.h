/*
 * Copyright (c) 2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_DTB_OVERLAY_H
#define INCLUDED_DTB_OVERLAY_H

#include <tegrabl_error.h>

/**
 * @brief Override DTBO into DTB as merged kernel DTB for Android
 *
 * @param kernel DTB handle.
 * @param kernel DTBO handle.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_dtb_overlay(void **kernel_dtb, void *kernel_dtbo);

#endif

