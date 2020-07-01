/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_SD_PARAM_H
#define INCLUDED_TEGRABL_SD_PARAM_H

#include <stdint.h>
#include <tegrabl_error.h>

struct gpio_info {
	uint32_t handle;
	uint32_t pin;
	uint32_t flags;
};

struct tegrabl_sd_platform_params {
	uint32_t vmmc_supply;
	struct gpio_info cd_gpio;
};

#endif
