/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDE_TEGRABL_I2C_SOC_COMMON_H
#define INCLUDE_TEGRABL_I2C_SOC_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_i2c.h>

struct i2c_soc_info {
	uint32_t base_addr;
	uint32_t clk_freq;
	uint32_t dpaux_instance;
	bool is_bpmpfw_controlled;
	bool is_cldvfs_required;
	bool is_muxed_dpaux;
};

void i2c_get_soc_info(struct i2c_soc_info **hi2c_info, uint32_t *num_of_instances);

/**
 * @brief Update internal prod setting values for given i2c instance
 *
 * @param instance controller instance
 * @param mode speed mode
 * @param settings array of uint32_t containing settings <reg, mask, value>
 * @param num_settings number of settings
 *
 * @return TEGRABL_NO_ERROR on success else appropriate error
 */
tegrabl_error_t tegrabl_i2c_register_prod_settings(uint32_t instance, uint32_t mode, uint32_t *settings,
												   uint32_t num_settings);

/**
 * @brief Sets prod setting for given i2c controller.
 *
 * @param hi2c Handle of the i2c.
 */
void i2c_set_prod_settings(struct tegrabl_i2c *hi2c);

/**
 * @brief Returns the source frequency in KHz
 *
 * @param hi2c Handle of the i2c
 *
 * @return frequency in KHz
 */
uint32_t tegrabl_i2c_get_clk_source_rate(const struct tegrabl_i2c *hi2c);

#endif /* INCLUDE_TEGRABL_I2C_SOC_COMMON_H */
