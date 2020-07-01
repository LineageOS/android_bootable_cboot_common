/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_REGULATOR_H
#define TEGRABL_REGULATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <list.h>
#include <tegrabl_error.h>

/* defines regulator volt type */
/* macro regular volt type */
#define USER_DEFINED_VOLTS 0
#define STANDARD_VOLTS 1
typedef uint32_t regulator_volt_type;

/* regulator info */
typedef struct {
	/* regulator list */
	struct list_node node;
	/* regulator phandle */
	int phandle;
	/* regulator name */
	char name[20];
	/* gpio regulator */
	bool is_fixed;
	/* enabled/disabled */
	bool is_enabled;
	/* gpio property available/unavailable */
	bool is_gpio_available;
	/* voltage to be set as per schematics */
	uint32_t set_volts;
	/* operations associated to regulator */
	tegrabl_error_t (*enable)(int32_t);
	tegrabl_error_t (*disable)(int32_t);
	tegrabl_error_t (*set_voltage)(int32_t, uint32_t);
} tegrabl_regulator_t;

/**
 * @brief api to register regulator for pmic driver or
 *        fixed regulator driver.
 *
 * @param regulator pointer to regulator_t.
 * @return NO_ERROR on success otherwise error.
 */
tegrabl_error_t tegrabl_regulator_register(tegrabl_regulator_t *regulator);

/**
 * @brief api to init the regulator list
 *
 * @return NO_ERROR on success otherwise error.
 */
tegrabl_error_t tegrabl_regulator_init(void);

/**
 * @brief api to set voltage for a selected rail
 *
 * @phandle handle of the dt entry
 * @volts desired voltage in microvolts
 * @volt_type - refer 'regulator_volt_type'
 *
 * @return NO_ERROR on success otherwise error.
 */
tegrabl_error_t tegrabl_regulator_set_voltage(int32_t phandle, uint32_t volts,
					regulator_volt_type volt_type);

/**
 * @brief api to enable selected regulator
 *
 * @phandle handle of the dt entry
 *
 * @return NO_ERROR on success otherwise error.
 */
tegrabl_error_t tegrabl_regulator_enable(int32_t phandle);

/**
 * @brief api to disable selected regulator
 *
 * @phandle handle of the dt entry
 *
 * @return NO_ERROR on success otherwise error.
 */
tegrabl_error_t tegrabl_regulator_disable(int32_t phandle);

/**
 * @brief api to find if the regulator has fixed voltage
 *
 * @phandle handle of the dt entry
 * @is_fixed ptr to return true/false for 'is_fixed' condition
 *
 * @return NO_ERROR on success otherwise error.
 */
tegrabl_error_t tegrabl_regulator_is_fixed(int32_t phandle, bool *is_fixed);

/**
 * @brief api to check whether regulator is enabled or not
 *
 * @phandle handle of the dt entry
 * @is_enabled ptr to return true/false
 *
 * @return NO_ERROR on success otherwise error.
 */
tegrabl_error_t tegrabl_regulator_is_enabled(int32_t phandle, bool *is_enabled);

#endif /*TEGRABL_REGULATOR_H*/
