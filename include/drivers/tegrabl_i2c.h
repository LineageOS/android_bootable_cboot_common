/*
 * Copyright (c) 2015 - 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_I2C_H
#define TEGRABL_I2C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <list.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>

enum tegrabl_instance_i2c {
	TEGRABL_INSTANCE_I2C1,
	TEGRABL_INSTANCE_I2C2,
	TEGRABL_INSTANCE_I2C3,
	TEGRABL_INSTANCE_I2C4,
	TEGRABL_INSTANCE_I2C5,
	TEGRABL_INSTANCE_I2C6,
	TEGRABL_INSTANCE_I2C7,
	TEGRABL_INSTANCE_I2C8,
	TEGRABL_INSTANCE_I2C9,
	TEGRABL_INSTANCE_I2C_MAX = TEGRABL_INSTANCE_I2C9,
	TEGRABL_INSTANCE_I2C_INVALID,
};

#define MAX_I2C_TRANSFER_SIZE (0x1000)

/**
 * @brief Store information about controller.
 */
struct tegrabl_i2c {
	struct list_node node;
	bool is_initialized;
	enum tegrabl_instance_i2c instance;
	uintptr_t base;
	bool requires_cldvfs;
	uint32_t module_id;
	uint32_t clk_freq;
	time_t single_fifo_timeout;
	time_t fifo_timeout;
	time_t byte_xfer_timeout;
	time_t xfer_timeout;
#if defined(CONFIG_POWER_I2C_BPMPFW)
	/* Specifies if the I2C interface is controlled through BPMP-FW or
	 * through ccplex */
	bool is_bpmpfw_controlled;
#endif
};

/**
* @brief I2c transaction information
*/
struct tegrabl_i2c_transaction {
	uint16_t slave_addr;
	uint8_t *buf;
	uint32_t len;
	bool is_write;
	bool is_repeat_start;
	time_t wait_time;
};

/**
* @brief Registers the i2c
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_register(void);

/**
* @brief De-register the i2c instance from the list
*
* @return NIL
*/
void tegrabl_i2c_unregister_instance(enum tegrabl_instance_i2c instance);

/**
 * @brief Sets information about frequencies to be used per i2c bus
 * based on specified frequency. Input frequency must be in KHz.
 *
 * @param freq Array frequencies in KHz
 * @param num Number of buses.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_i2c_set_bus_freq_info(uint32_t *freq, uint32_t num);

/**
 * @brief Register prod settings for particular instance.
 *
 * @param instance I2C controller instance
 * @param mode Transfer speed mode
 * @param settings Pointer to buffer containing <address, mask, value> triplets
 * @param num_settings Number of triplets in buffer
 *
 */
tegrabl_error_t tegrabl_i2c_register_prod_settings(uint32_t instance,
		uint32_t mode, uint32_t *settings, uint32_t num_settings);

/**
* @brief Initializes the given i2c controller.
*
* @param instance Instance of the i2c controller.
* @param freq Frequency of the i2c clock.
*
* @return Handle of the struct tegrabl_i2c if success, NULL if fails.
*/
struct tegrabl_i2c *tegrabl_i2c_open(enum tegrabl_instance_i2c instance);

/**
* @brief Writes the given data on the i2c interface.
*
* @param hi2c Handle of the i2c.
* @param slave_addr Address of the i2c slave.
* @param repeat_start Whether the repeat start is required or not
* @param buf Buffer from which data has to be written.
* @param len Number of bytes to write.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_write(struct tegrabl_i2c *hi2c, uint16_t slave_addr,
	bool repeat_start, void *buf, uint32_t len);

/**
* @brief Reads the data on the i2c interface.
*
* @param hi2c Handle of the i2c.
* @param slave_addr Address of the i2c slave.
* @param repeat_start Whether the repeat start is required or not
* @param buf Buffer to which read data has to be passed.
* @param len Number of bytes to read.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_read(struct tegrabl_i2c *hi2c, uint16_t slave_addr,
	bool repeat_start, void *buf, uint32_t len);

/**
* @brief Performs the given i2c transactions.
*
* @param hi2c Handle of the i2c.
* @param trans Lists the i2c transactions to perform.
* @param num_trans Num of transactions.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_transaction(struct tegrabl_i2c *hi2c,
	struct tegrabl_i2c_transaction *trans, uint32_t num_trans);

/**
* @brief Performs i2c bus clear opearation
*
* @param hi2c Handle of the i2c.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_bus_clear(struct tegrabl_i2c *hi2c);
/**
* @brief Disables the given i2c controller.
*
* @param hi2c Handle of the i2c.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_i2c_close(struct tegrabl_i2c *hi2c);

#endif

