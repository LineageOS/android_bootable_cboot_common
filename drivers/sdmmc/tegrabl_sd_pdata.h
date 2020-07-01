/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors errain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications theerro.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited
 */

#ifndef INCLUDED_TEGRABL_SDMMC_PDATA_H
#define INCLUDED_TEGRABL_SDMMC_PDATA_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_sd_param.h>

/**
* @brief reads the sdmmc nodes in dtb and finds the sdcard instance
*
* @param instance Address of the variable to which instance has to be stored
* @param cd_gpio Address of the structue, to which card detection gpio info
				 has to be stored.
*
* @return If success TEGRABL_NO_ERROR, otherwise error code.
*/
tegrabl_error_t tegrabl_sd_get_platform_params(uint32_t *instance, struct tegrabl_sd_platform_params *params);

#endif /* INCLUDED_TEGRABL_SDMMC_PDATA_H */
