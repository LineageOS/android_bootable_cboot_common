/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_LP8556_H
#define TEGRABL_LP8556_H

#include <tegrabl_pwm.h>

/* Registers */
#define LP8556_BRIGHTNESS_CTRL	0x00
#define LP8556_DEVICE_CTRL		0x01

#define MAX_BRIGHTNESS		255
#define INVERT_OF_NSEC		1000000000

#define BACKLIGHT_LP8556 "ti,lp8556"

enum lp8556_mode {
	PWM_BASED = 1,
	REGISTER_BASED,
};

/* struct lp8556_platform_data
 * @dev_ctrl : value of DEVICE CONTROL register
 * @init_brt : initial brightness of backlight device
 * @tegrabl_pwm : data for pwm control (init brt, max brt, period, etc)
 */
struct lp8556_platform_data {
	uint8_t dev_ctrl;
	uint8_t init_brt;
	struct tegrabl_pwm bl_pwm;
};

/* struct lp8556_regs
 * @brightness: register address for brightness control
 * @dev_ctrl: register address for device control
 */
struct lp8556_regs {
	uint8_t brightness;
	uint8_t dev_ctrl;
};

struct backlight_pdata {
	enum lp8556_mode mode;
	struct lp8556_regs *regs;
	uint32_t i2c_instance;
	uint32_t i2c_slave_addr;
	struct lp8556_platform_data *lp_pdata;
};

tegrabl_error_t lp8556_init(struct backlight_pdata *lp);

#endif /*TEGRABL_LP8556_H*/
