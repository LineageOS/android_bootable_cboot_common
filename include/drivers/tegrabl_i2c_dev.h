/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_I2C_DEV_H
#define TEGRABL_I2C_DEV_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_i2c.h>

/**
* @brief I2c slave information.
*/
struct tegrabl_i2c_dev {
	enum tegrabl_instance_i2c instance;
	struct tegrabl_i2c *hi2c;
	uint16_t slave_addr;
	uint32_t reg_addr_size;
	uint32_t bytes_per_reg;
	time_t wait_time_for_write_us;
};

/**
* @brief i2c dev ioctls
*/
enum tegrabl_i2c_dev_ioctl {
	/* to configure wait time after register write in slave */
	TEGRABL_I2C_DEV_IOCTL_WAIT_TIME_WRITE,
	TEGRABL_I2C_DEV_IOCTL_INVALID,
};

/**
* @brief Initializes the given i2c slave interface.
*
* @param instance instance of the i2c controller
* @param slave_addr address of the slave
* @param reg_addr_size size of the address of registers in the slave in bytes.
* @param bytes_per_reg size of the registers in the slave in bytes.
*
* @return Returns handle of the i2c slave device if sucess, error code if fails.
*/
struct tegrabl_i2c_dev *tegrabl_i2c_dev_open(enum tegrabl_instance_i2c instance,
	uint32_t slave_addr, uint32_t reg_addr_size, uint32_t bytes_per_reg);

/**
* @brief Reads the given registers of the i2c slave
*
* @param hi2cdev Handle to the i2c slave device
* @param buf Pointer to the buffer to write
* @param reg_addr Address of the register in the slave.
* @param reg_count Number of registers to read
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_dev_read(struct tegrabl_i2c_dev *hi2cdev, void *buf,
	uint32_t reg_addr, uint32_t reg_count);

/**
* @brief Writes the data to the requested registers of the i2c slave
*
* @param hi2cdev Handle to the i2c slave device.
* @param buf Pointer to the buffer to read.
* @param reg_addr Address of the register in the slave.
* @param reg_count Number of registers to write
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_dev_write(struct tegrabl_i2c_dev *hi2cdev,
	const void *buf, uint32_t reg_addr, uint32_t reg_count);

/**
* @brief Performs i2c dev ioctl
*
* @param hi2cdev Handle to the i2c slave device.
* @param ioctl Ioctl to execute.
* @param args Ioctl arguments.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_dev_ioctl(struct tegrabl_i2c_dev *hi2cdev,
	enum tegrabl_i2c_dev_ioctl ioctl, void *args);
/**
* @brief  Disables the given i2c controller.
*
* @param hi2cdev Handle to the i2c slave device
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_dev_close(struct tegrabl_i2c_dev *hi2cdev);
#endif
