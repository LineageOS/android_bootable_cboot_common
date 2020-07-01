/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_PANEL

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_pwm.h>
#include <lp8556.h>
#include <tegrabl_i2c_dev.h>

static struct lp8556_regs lp8556_dev_regs = {
	.brightness = LP8556_BRIGHTNESS_CTRL,
	.dev_ctrl = LP8556_DEVICE_CTRL,
};

static tegrabl_error_t lp8556_write_u8(struct backlight_pdata *bl_pdata, uint8_t reg, uint8_t data)
{
	struct tegrabl_i2c_dev *hi2c;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_debug("inst: %x slave_addr:%#x reg:%#x data:%#x\n", bl_pdata->i2c_instance,
			 bl_pdata->i2c_slave_addr, reg, data);

	hi2c = tegrabl_i2c_dev_open(bl_pdata->i2c_instance, bl_pdata->i2c_slave_addr, 1, 1);
	if (hi2c == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_OPEN_FAILED, 0);
		goto fail;
	}

	err = tegrabl_i2c_dev_write(hi2c, &data, reg, 1);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

fail:
	return err;
}

/*
 * Configure LP5886 by initializing registers
 */
static tegrabl_error_t lp8556_configure(struct backlight_pdata *bl_pdata)
{
	struct lp8556_platform_data *pd = bl_pdata->lp_pdata;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	bl_pdata->regs = &lp8556_dev_regs;

	/* set initial brightness */
	err = lp8556_write_u8(bl_pdata, bl_pdata->regs->brightness, pd->init_brt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Couldn't set initial brightness: %d\n", __func__, err);
		goto fail;
	}

	/* write device configuration */
	err = lp8556_write_u8(bl_pdata, bl_pdata->regs->dev_ctrl, pd->dev_ctrl);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed to write device configuration: %d\n", __func__, err);
		goto fail;
	}

fail:
	return err;
}

static tegrabl_error_t lp8556_pwm_ctrl(struct backlight_pdata *bl_pdata, uint32_t br)
{
	uint32_t period = bl_pdata->lp_pdata->bl_pwm.freq;
	uint32_t instance = bl_pdata->lp_pdata->bl_pwm.instance;
	uint32_t duty = br * 100 / MAX_BRIGHTNESS;
	struct tegrabl_pwm *hpwm;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	hpwm = tegrabl_pwm_open(instance);
	if (hpwm == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, 0);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	err = tegrabl_pwm_config(hpwm, INVERT_OF_NSEC/period, duty);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

fail:
	return err;
}

tegrabl_error_t lp8556_init(struct backlight_pdata *bl_pdata)
{
	tegrabl_pwm_register();

	if (bl_pdata->lp_pdata->bl_pwm.freq > 0) {
		bl_pdata->mode = PWM_BASED;
		return lp8556_pwm_ctrl(bl_pdata, bl_pdata->lp_pdata->init_brt);
	} else {
		bl_pdata->mode = REGISTER_BASED;
		return lp8556_configure(bl_pdata);
	}
}
