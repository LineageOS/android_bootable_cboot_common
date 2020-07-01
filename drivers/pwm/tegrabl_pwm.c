/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_PWM

#include <build_config.h>
#include <stdint.h>
#include <tegrabl_malloc.h>
#include <tegrabl_ar_macro.h>
#include <tegrabl_clock.h>
#include <tegrabl_module.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_drf.h>
#include <tegrabl_pwm.h>
#include <list.h>
#include <arpwm.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_io.h>

/**
 * @brief List of controllers
 */
static struct list_node pwm_list;

#define NUM_OF_CHANNELS		8

#define PWM_DEFAULT_DIVIDER		256
#define MAX_WIDTH		255
#define MAX_DIVIDER			4096
#define FREQ_TO_PWM_CONTROLLER	(48 * 1000 * 1000)

uint32_t pwm_base_addr[] = {
	NV_ADDRESS_MAP_PWM1_BASE,
	NV_ADDRESS_MAP_PWM2_BASE,
	NV_ADDRESS_MAP_PWM3_BASE,
	NV_ADDRESS_MAP_PWM4_BASE,
	NV_ADDRESS_MAP_PWM5_BASE,
	NV_ADDRESS_MAP_PWM6_BASE,
	NV_ADDRESS_MAP_PWM7_BASE,
	NV_ADDRESS_MAP_PWM8_BASE,
};

#define pwm_writel(context, reg, value) \
	NV_WRITE32((context->base_addr + PWM_CONTROLLER_PWM_##reg##_0), value);

void tegrabl_pwm_register(void)
{
	list_initialize(&pwm_list);
}

struct tegrabl_pwm *tegrabl_pwm_open(uint32_t instance)
{
	struct tegrabl_pwm *hpwm = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t rate_set;
	tegrabl_module_t module = TEGRABL_MODULE_PWM;

	/* check for any invalid arguments */
	if (instance >= NUM_OF_CHANNELS) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	list_for_every_entry(&pwm_list, hpwm, struct tegrabl_pwm, node) {
		if (hpwm->instance == instance) {
			pr_debug("handle is already open for this instance\n");
			return hpwm;
		}
	}

	hpwm = tegrabl_malloc(sizeof(*hpwm));
	if (hpwm == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		pr_error("Failed to allocate memory for pwm struct\n");
		goto fail;
	}

	hpwm->module_id = module;
	hpwm->instance = instance;
	hpwm->base_addr = (uintptr_t)pwm_base_addr[hpwm->instance];
	hpwm->freq = 0;
	hpwm->duty_cycle = 0;

	/* Assert reset to pwm */
	err = tegrabl_car_rst_set(hpwm->module_id, hpwm->instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("unable to assert reset to pwm instance %u\n",
			 hpwm->instance);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* Set the pwm controller clock source */
	err = tegrabl_car_set_clk_src(hpwm->module_id, hpwm->instance,
				      TEGRABL_CLK_SRC_PLLP_OUT0);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("unable to change clk source to pwm instance %u\n",
			 hpwm->instance);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* Set the pwm controller frequency */
	err = tegrabl_car_set_clk_rate(hpwm->module_id, hpwm->instance,
				       FREQ_TO_PWM_CONTROLLER, &rate_set);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("unable to set clk rate to pwm instance %u\n",
			 hpwm->instance);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/* Deassert reset to pwm */
	err = tegrabl_car_rst_clear(hpwm->module_id, hpwm->instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("unable to clear reset to pwm instance %u\n",
			 hpwm->instance);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	hpwm->is_initialized = true;
	list_add_tail(&pwm_list, &hpwm->node);

fail:
	return hpwm;
}

tegrabl_error_t tegrabl_pwm_config(struct tegrabl_pwm *hpwm, uint32_t freq,
	uint32_t duty_cycle)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t config = 0;
	uint32_t div;

	/* check for any invalid arguments */
	if ((freq > (FREQ_TO_PWM_CONTROLLER / PWM_DEFAULT_DIVIDER)) ||
		(duty_cycle > 100)) {
		pr_debug("pwm: invalid arguments. freq = %d, duty_cycle = %d",
			freq, duty_cycle);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	/* frequency zero mean, always low. so duty cycle is 0 */
	if (freq == 0) {
		freq = 1;
		duty_cycle = 0;
	}

	/* calculate divider based on frequency */
	div = FREQ_TO_PWM_CONTROLLER / (PWM_DEFAULT_DIVIDER * freq);

	if (div > MAX_DIVIDER) {
		div = MAX_DIVIDER;
	}

	/* caculate pulse width based on duty cycle */
	duty_cycle = (duty_cycle * MAX_WIDTH) / 100;
	if (duty_cycle > MAX_WIDTH) {
		duty_cycle = MAX_WIDTH;
	}

	/* program frequency and dutycycle */
	config = NV_DRF_NUM(PWM_CONTROLLER, PWM_CSR_0, PFM_0, div) |
			NV_DRF_NUM(PWM_CONTROLLER, PWM_CSR_0, PWM_0, duty_cycle) |
			NV_DRF_NUM(PWM_CONTROLLER, PWM_CSR_0, ENB, 1);
	pr_debug("PWM config value = %#x pwm_instance = %#x\n", config, hpwm->instance);

	pwm_writel(hpwm, CSR_0, config);

	hpwm->freq = freq;
	hpwm->duty_cycle = duty_cycle;

fail:
	return err;
}

tegrabl_error_t tegrabl_pwm_close(struct tegrabl_pwm *hpwm)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t config = 0;

	if (hpwm == NULL) {
		pr_debug("pwm: invalid arguments");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	if (hpwm->is_initialized) {
		config = NV_DRF_NUM(PWM_CONTROLLER, PWM_CSR_0, ENB, 0);
		pwm_writel(hpwm, CSR_0, config);
	}

	/* Assert reset to pwm */
	err = tegrabl_car_rst_set(hpwm->module_id, hpwm->instance);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("unable to assert reset to pwm instance %u\n",
			 hpwm->instance);
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}
	hpwm->is_initialized = false;
	list_remove_tail(&pwm_list);
	tegrabl_free(hpwm);

fail:
	return err;
}
