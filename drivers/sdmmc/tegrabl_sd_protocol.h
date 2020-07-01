/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SD_PROTOCOL_H
#define TEGRABL_SD_PROTOCOL_H

#include <tegrabl_error.h>
#include <tegrabl_sdmmc_defs.h>

/**
* @brief Enumerates the sdcard
*
* @param context context of the sdmmc controller
*
* @return If success returns TEGRABL_NO_ERROR, otherwise error code
*/
tegrabl_error_t sd_identify_card(sdmmc_context_t *context);

#endif
