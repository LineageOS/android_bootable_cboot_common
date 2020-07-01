/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
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

struct i2c_soc_info {
	uint32_t base_addr;
	uint32_t mode;
	uint32_t dpaux_instance;
	bool is_bpmpfw_controlled;
	bool is_cldvfs_required;
	bool is_muxed_dpaux;
};

void i2c_get_soc_info(struct i2c_soc_info **hi2c_info, uint32_t *num_of_instances);

#endif /* INCLUDE_TEGRABL_I2C_SOC_COMMON_H */
