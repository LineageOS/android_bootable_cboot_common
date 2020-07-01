/*
 * TCA9539 16-bit I2C I/O Expander
 *
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TCA9539_GPIO_H
#define __TCA9539_GPIO_H

#include <tegrabl_error.h>

/* GPIO number definition */
enum tca9539_gpio_num {
	TCA9539_GPIO_P0_0,
	TCA9539_GPIO_P0_1,
	TCA9539_GPIO_P0_2,
	TCA9539_GPIO_P0_3,
	TCA9539_GPIO_P0_4,
	TCA9539_GPIO_P0_5,
	TCA9539_GPIO_P0_6,
	TCA9539_GPIO_P0_7,
	TCA9539_GPIO_P1_0,
	TCA9539_GPIO_P1_1,
	TCA9539_GPIO_P1_2,
	TCA9539_GPIO_P1_3,
	TCA9539_GPIO_P1_4,
	TCA9539_GPIO_P1_5,
	TCA9539_GPIO_P1_6,
	TCA9539_GPIO_P1_7,
	TCA9539_GPIO_MAX
};

/**
 * @brief Initialize tca9539 gpio expander
 *
 * @return TEGRABL_NO_ERROR if successful, else error code
 */
tegrabl_error_t tegrabl_tca9539_init(void);

#endif
