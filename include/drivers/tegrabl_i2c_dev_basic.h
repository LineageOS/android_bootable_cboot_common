/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/* This I2C Dev Basic driver is a special driver with specific interface needed
 * by MB1 PMIC configuration.
 */

#ifndef TEGRABL_I2C_DEV_BASIC_H
#define TEGRABL_I2C_DEV_BASIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_i2c.h>

/**
* @brief Write the given data to the given register in the i2c slave.
*
* @param instance I2C controller instance, to which slave is connected.
* @param slave_addr Address of the i2c slave
* @param is_addr_16_bit Gives whether register address is 16 bit or 8-bit
* @param is_data_16_bit Gives whether register data size is 16 bit or 8 bit
* @param reg_addr Address of the register in the i2c slave
* @param value Value to write in register.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_dev_basic_write(tegrabl_instance_i2c_t instance,
	uint16_t slave_addr, bool is_addr_16_bit, bool is_data_16_bit,
	uint16_t reg_addr, uint16_t value);

/**
* @brief Reads the  data from the  given register in the i2c slave.
*
* @param instance I2C controller instance, to which slave is connected.
* @param slave_addr Address of the i2c slave
* @param is_addr_16_bit Gives whether register address is 16 bit or 8-bit
* @param is_data_16_bit Gives whether register data size is 16 bit or 8 bit
* @param reg_addr Address of the register in the i2c slave
* @param value Value to write in register.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_dev_basic_read(tegrabl_instance_i2c_t instance,
	uint16_t slave_addr, bool is_addr_16_bit, bool is_data_16_bit,
	uint16_t reg_addr, uint16_t *value);

/**
* @brief Updates the given data to the giver register in the i2c slave.
*
* @param instance I2C controller instance, to which slave is connected.
* @param slave_addr Address of the i2c slave
* @param is_addr_16_bit Gives whether register address is 16 bit or 8-bit
* @param is_data_16_bit Gives whether register data size is 16 bit or 8 bit
* @param reg_addr Address of the register in the i2c slave
* @param mask Mask for the data. Cosiders only bits, which have value 1.
* @param value Value to write in register.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_dev_basic_update(tegrabl_instance_i2c_t instance,
	uint16_t slave_addr, bool is_addr_16_bit, bool is_data_16_bit,
	uint16_t reg_addr, uint16_t mask, uint16_t value);

#endif
