/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_EEPROM_MANAGER_H
#define TEGRABL_EEPROM_MANAGER_H

#include <tegrabl_error.h>
#include <tegrabl_i2c.h>
#include <tegrabl_eeprom.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define TEGRABL_EEPROM_MAX_SIZE 256
#define TRGRABL_EEPROM_MAX_PER_BUS 8
#define TEGRABL_EEPROM_MAX_NUM (TEGRABL_INSTANCE_I2C_INVALID * \
				TRGRABL_EEPROM_MAX_PER_BUS)

/**
 * @brief holds definations of different device types
 */
/* macro tegrabl eeprom device */
typedef uint32_t tegrabl_eeprom_device_t;
#define TEGRABL_EEPROM_CAM 0
#define TEGRABL_EEPROM_CVM 1
#define TEGRABL_EEPROM_GENERIC 2
#define TEGRABL_EEPROM_DEVICE_MAX 3

/**
 *@brief eeprom ops table
 */
struct tegrabl_eeprom_ops_info {
	const char *name;
	tegrabl_error_t (*ops) (struct tegrabl_eeprom *, const void *in_data);
};

/**
 * @brief Gets EEPROM data with name provided. Will initialize manager first
 *        if not. This function primarily helps to facilitates EEPROM
 *        indentification by mnemonic strings.
 *
 * @param name   name of the EEPROM to be read
 * @param eeprom Callee filled. Holds a pointer to the appropriate eeprom
 *				 data structure in the array, NULL on error
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error
 */
tegrabl_error_t tegrabl_eeprom_manager_get_eeprom_by_name(const char *name,
									struct tegrabl_eeprom **eeprom);

/**
 * @brief Return total number of eeprom managed by eeprom manager. Will
 *        initialize manager first if not. This function basically provide
 *        a easy way to traverse all eeproms without knowing actual name.
 *
 * @param num Callee filled. Holds the numbers of total eeprom
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_eeprom_manager_max(uint8_t *num);

/**
 * @brief Gets EEPROM data with id provided. Id is internal index of eeprom
 *        read by manager.Will initialize manager first if not.
 *        Re-reads are avoided.
 *
 * @param module_id index of eeprom to be read
 * @param eeprom    Callee filled. Holds a pointer to the appropriate eeprom
 *				    data structure in the array, NULL on error
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_eeprom_manager_get_eeprom_by_id(uint8_t module_id,
									struct tegrabl_eeprom **eeprom);

/**
 * @brief Releases any resources allocated by eeprom manager.
 */
void tegrabl_eeprom_manager_release_resources(void);

#if defined(__cplusplus)
}
#endif

#endif
