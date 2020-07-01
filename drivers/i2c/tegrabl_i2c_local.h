/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_I2C_LOCAL_H
#define TEGRABL_I2C_LOCAL_H

#include <stdint.h>

#define STD_SPEED		100UL
#define FM_SPEED		400UL
#define FM_PLUS_SPEED	1000UL
#define HS_SPEED		3400UL

#define I2C_TIMEOUT (500)
#define CNFG_LOAD_TIMEOUT_US (20)

#define PACKET_HEADER_I2C_PROTOCOL (1UL << 4)
#define PACKET_HEADER_CONTROLLER_ID_SHIFT (12U)
#define PACKET_HEADER_SLAVE_ADDRESS_MASK (0x3FFU)
#define PACKET_HEADER_READ_MODE (1UL << 19)
#define PACKET_HEADER_REPEAT_START (1UL << 16)
#define PACKET_HEADER_HS_MODE (1UL << 22)

#endif
