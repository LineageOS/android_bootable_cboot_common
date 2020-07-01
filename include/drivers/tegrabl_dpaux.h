/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/**
 * @file
 * @breif <b>NVIDIA TEGRABL Interface: DPAUX </b>
 *
 * @b Description: This file declares APIs for dpaux module configuration
 */

#ifndef TEGRABL_DPAUX
#define TEGRABL_DPAUX

#include <stdbool.h>
#include <tegrabl_error.h>
#include <stdint.h>
#include <tegrabl_i2c.h>

/* macro dpaux instance */
typedef uint32_t dpaux_instance_t;
#define DPAUX_INSTANCE_0 0
#define DPAUX_INSTANCE_1 1
#define DPAUX_INSTANCE_2 2
#define DPAUX_INSTANCE_3 3

/* macro dpaux aux cmd */
typedef uint32_t dpaux_aux_cmd_t;
#define AUX_CMD_I2CWR 0
#define AUX_CMD_I2CRD 1
#define AUX_CMD_I2CREQWSTAT 2
#define AUX_CMD_MOTWR 4
#define AUX_CMD_MOTRD 5
#define AUX_CMD_MOTREQWSTAT 6
#define AUX_CMD_AUXWR 8
#define AUX_CMD_AUXRD 9

struct tegrabl_dpaux {
	uint32_t instance;
	void *base;
	uint8_t mode;
	uint32_t module;
};

/**
* @brief Initliazes the dpaux of given instance in I2C mode.
*
* @param instance Instance of the DPAUX
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_dpaux_init_ddc_i2c(dpaux_instance_t instance);

/**
* @brief Initilizes the dpaux of given instance in AUX mode.
*
* @param instance Instance of the DPAUX
* @param phdpaux Address of the variable, in which dpaux handle has to keep.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_dpaux_init_aux(dpaux_instance_t instance,
	struct tegrabl_dpaux **phdpaux);

/**
* @brief Writes the data of max DPAUX_MAX_BYTES on the aux channel.
*
* @param hdpaux Handle to dpaux
* @param cmd command I2C Read, Aux Read
* @param addr Address
* @param data Address of the buffer, which has data to be written
* @param size Address of the variable, which specifies size of the data
* @param aux_stat Address of the variable, in which status has to be kept
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_dpaux_write_chunk(struct tegrabl_dpaux *hdpaux,
	uint32_t cmd, uint32_t addr, uint8_t *data, uint32_t *size,
	uint32_t *aux_stat);

/**
* @brief Reads the data of max DPAUX_MAX_BYTES on the aux channel.
*
* @param hdpaux Handle to dpaux
* @param cmd command I2C Read, Aux Read
* @param addr Address
* @param data Address of the buffer, which has data to be written
* @param size Address of the variable, which specifies size of the data
* @param aux_stat Address of the variable, in which status has to be kept
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_dpaux_read_chunk(struct tegrabl_dpaux *hdpaux,
	uint32_t cmd, uint32_t addr, uint8_t *data, uint32_t *size,
	uint32_t *aux_stat);

/**
* @brief Writes the data on the aux channel.
*
* @param hdpaux Handle to dpaux
* @param cmd command I2C Read, Aux Read
* @param addr Address
* @param data Address of the buffer, which has data to be written
* @param size Address of the variable, which specifies size of the data
* @param aux_stat Address of the variable, in which status has to be kept
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_dpaux_write(struct tegrabl_dpaux *hdpaux, uint32_t cmd,
	uint32_t addr, uint8_t *data, uint32_t *size, uint32_t *aux_stat);
/**
* @brief Reads data on the aux channel.
*
* @param hdpaux Handle to dpaux
* @param cmd command I2C Read, Aux Read
* @param addr Address
* @param data Address of the buffer, which has data to be written
* @param size Address of the variable, which specifies size of the data
* @param aux_stat Address of the variable, in which status has to be kept
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_dpaux_read(struct tegrabl_dpaux *hdpaux, uint32_t cmd,
	uint32_t addr, uint8_t *data, uint32_t *size, uint32_t *aux_stat);

/**
* @brief Gives the hpd status
*
* @param hdpaux Handle to dpaux
* @param hpd_status Address of the variable, to which status has to keep.
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_dpaux_hpd_status(struct tegrabl_dpaux *hdpaux,
	bool *hpd_status);

/**
* @brief Writes the given I2C data on dpaux channel
*
* @param hdpaux Handle to dpaux
* @param slave_addr I2c slave address
* @param data Address of the buffer, which has data to be written
* @param size Address of the variable, which specifies size of the data
* @param aux_stat Address of the variable, in which status has to be kept
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_dpaux_i2c_write(struct tegrabl_dpaux *hdpaux,
	uint16_t slave_addr, uint8_t *data, uint32_t *size, uint32_t *aux_stat);

/**
* @brief Reads the I2C data on dpaux channel
*
* @param hdpaux Handle to dpaux
* @param slave_addr I2c slave address
* @param data Address of the buffer, to which data read has to be kept.
* @param size Address of the variable, which specifies size of the data
* @param aux_stat Address of the variable, in which status has to be kept
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_dpaux_i2c_read(struct tegrabl_dpaux *hdpaux,
	uint16_t slave_addr, uint8_t *data, uint32_t *size, uint32_t *aux_stat);

/**
* @brief Executes the given i2c transactions
*
* @param hdpaux Handle to dpaux
* @param msgs Address of the i2c transactions info array.
* @param num Number of i2c transactions
*
* @return TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_dpaux_i2c_transactions(struct tegrabl_dpaux *hdpaux,
	struct tegrabl_i2c_transaction *trans, uint32_t num);
#endif
