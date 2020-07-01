/*
 * TCA9539 16-bit I2C I/O Expander
 *
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TCA9539_GPIO_H
#define __TCA9539_GPIO_H

/* GPIO number definition */
/* macro tca9539 gpio num */
typedef uint32_t tca9539_gpio_num_t;
#define TCA9539_GPIO_P0_0 0
#define TCA9539_GPIO_P0_1 1
#define TCA9539_GPIO_P0_2 2
#define TCA9539_GPIO_P0_3 3
#define TCA9539_GPIO_P0_4 4
#define TCA9539_GPIO_P0_5 5
#define TCA9539_GPIO_P0_6 6
#define TCA9539_GPIO_P0_7 7
#define TCA9539_GPIO_P1_0 8
#define TCA9539_GPIO_P1_1 9
#define TCA9539_GPIO_P1_2 10
#define TCA9539_GPIO_P1_3 11
#define TCA9539_GPIO_P1_4 12
#define TCA9539_GPIO_P1_5 13
#define TCA9539_GPIO_P1_6 14
#define TCA9539_GPIO_P1_7 15
#define TCA9539_GPIO_MAX 16

struct tca9539_driver_property {
	uint32_t chip_id;
	int i2c_inst;
	uint32_t i2c_addr;
	char *i2c_name;
};

/**
 * @brief Initialize tca9539 gpio expander
 *
 * @return TEGRABL_NO_ERROR if successful, else error code
 */
tegrabl_error_t tegrabl_tca9539_init(void);

#endif
