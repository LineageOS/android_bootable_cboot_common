
/*
 * Copyright (c) 2016-2019, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_SD_CARD_H
#define INCLUDED_TEGRABL_SD_CARD_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_sd_param.h>

/**
* @brief Checks if the sdcard is present. If presents gives the sdmmc instance
		 of sdcard.
*
* @param instance Address of the variable, where instance has to be stored.
* @param is_present Address of the bool, where card presence has to be stored.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_sd_is_card_present(struct gpio_info *cd_gpio, bool *is_present);

#endif
