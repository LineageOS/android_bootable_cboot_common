/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_EEPROM_H
#define TEGRABL_EEPROM_H

#include <tegrabl_error.h>
#include <stdbool.h>
#include <tegrabl_i2c.h>

/**
 * @brief Defines config parameters for eeprom driver
 *
 * @param name name of the eeprom instance; Can be NULL
 * @param bus_node_name name of the i2c node on which eeprom is found
 * @param instance I2C instance for eeprom
 * @param slave_addr Slave address of the eeprom
 * @param size Size of the eeprom to be read
 * @param crc_valid defines whether eeprom has crc field programmed
 * @param data stores the data that is read from eeprom. initialized to null.
 * @param data_valid true if the data from EEPROM is already read, else false
*/
struct tegrabl_eeprom {
	char *name;
	char *bus_node_name;
	enum tegrabl_instance_i2c instance;
	uint8_t slave_addr;
	uint32_t size;
	bool crc_valid;
	uint8_t *data;
	bool data_valid;
};

/**
 * @brief Performs I2C transactions, fetches the eeprom data and populates
 *        the data field in tegrabl_eeprom. Note that the memory for the data
 *        buffer is assumed to be allocated
 *
 * @param eeprom object corresponding to the eeprom to be read eeprom driver.
 *               Requires all fields to be populated by the caller, except data
 *				 which shall be populated by callee
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_eeprom_read(struct tegrabl_eeprom *eeprom);

/**
 * @brief Dumps the contents of EEPROM data structure, and the contents of the
 *		  physical EEPROM. Mostly used for debugging purposes
 *
 * @param eeprom EEPROM object to be dumped
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_eeprom_dump(struct tegrabl_eeprom *eeprom);

#endif
