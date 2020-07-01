/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_PWM_H
#define TEGRABL_PWM_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <list.h>
#include <tegrabl_module.h>

struct tegrabl_pwm {
	struct list_node node;
	uintptr_t base_addr;
	tegrabl_module_t module_id;
	uint32_t instance;
	uint32_t freq;
	uint32_t duty_cycle;
	bool is_initialized;
};

/**
* @brief Registers the PWM
*/
void tegrabl_pwm_register(void);

/** @brief Initializes the pwm controller
 *
 *  @param channel pwm channel to be open
 *
 *  @return Handle to the pwm if success, NULL if fails.
 */
struct tegrabl_pwm *tegrabl_pwm_open(uint32_t channel);

/** @brief Enables and configures the given pwm channel with required
 *  frequency and duty cycle
 *
 *  @param hpwm Handle to the pwm.
 *  @param freq frequency of the pulses
 *  @param duty_cycle duty cycle is the percentage of high puse width.
 *                     should be in the range 0 to 100.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_pwm_config(struct tegrabl_pwm *hpwm, uint32_t freq,
	uint32_t duty_cycle);

/** @brief Disables the given channel and asserts the pwm controller
 *
 *  @param channel tegrabl_pwm channel to be close
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_pwm_close(struct tegrabl_pwm *hpwm);

#endif /* TEGRABL_PWM_H */

