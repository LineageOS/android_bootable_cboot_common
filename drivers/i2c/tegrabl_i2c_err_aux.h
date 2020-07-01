/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_I2C_ERR_AUX_H
#define TEGRABL_I2C_ERR_AUX_H

#include <stdint.h>

#define TEGRABL_I2C_REGISTER_PROD_SETTINGS_1 0x1U
#define TEGRABL_I2C_REGISTER_PROD_SETTINGS_2 0x2U
#define TEGRABL_I2C_SET_BUS_FREQ_INFO 0x3U
#define I2C_LOAD_CONFIG 0x4U
#define I2C_WAIT_FOR_TX_FIFO_EMPTY 0x5U
#define I2C_WAIT_FOR_RX_FIFO_FILLED 0x6U
#define I2C_WAIT_FOR_TRANSFER_COMPLETE 0x7U
#define TEGRABL_I2C_OPEN_1 0x8U
#define TEGRABL_I2C_OPEN_2 0x9U
#define TEGRABL_I2C_READ 0xAU
#define TEGRABL_I2C_WRITE 0xBU
#define TEGRABL_I2C_TRANSACTION 0xCU
#define TEGRABL_I2C_BUS_CLEAR 0xDU
#define TEGRABL_VIRTUAL_I2C_BPMP_XFER 0xEU
#define TEGRABL_I2C_CLOSE 0xFU
#define I2C_RESET_CONTROLLER 0x10U

#endif
