/*
 * Copyright (c) 2015 - 2017, NVIDIA CORPORATION.  All rights reserved.
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
#include <stdbool.h>
#include <tegrabl_error.h>
#include <tegrabl_i2c.h>
#include <tegrabl_debug.h>

#define KHZ 1000
#define STD_SPEED		(100 * KHZ)
#define FM_SPEED		(400 * KHZ)
#define FM_PLUS_SPEED	(1000 * KHZ)
#define HS_SPEED		(3400 * KHZ)

#define I2C_I2C_CLK_DIVISOR_REGISTER (0x6c)
#define I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT (16)
#define I2C_CLK_DIVISOR_STD_FAST_MODE_WIDTH (16)
#define I2C_CLK_DIVISOR_HS_MODE_SHIFT (0)
#define I2C_CLK_DIVISOR_HS_MODE_WIDTH (16)

#define I2C_I2C_CONFIG_LOAD_REGISTER (0x8c)
#define I2C_I2C_MSTR_CONFIG_LOAD (0x1)

#define I2C_I2C_CNFG_REGISTER (0x0)

#define I2C_TIMEOUT (500)

#define CNFG_LOAD_TIMEOUT_US (20)
#define ENABLE_PACKET_MODE (1<<10)
#define I2C_IO_PACKET_HEADER_I2C_PROTOCOL (1 << 4)
#define I2C_IO_PACKET_HEADER_CONTROLLER_ID_SHIFT (12)

#define I2C_TX_PACKET_FIFO_REGISTER (0x50)
#define I2C_IO_PACKET_HEADER_SLAVE_ADDRESS_MASK (0x3FF)
#define I2C_IO_PACKET_HEADER_READ_MODE (1 << 19)
#define I2C_IO_PACKET_HEADER_REPEAT_START (1 << 16)
#define I2C_ENABLE_HS_MODE (1 << 22)

#define I2C_FIFO_STATUS_REGISTER (0x60)
#define I2C_FIFO_STATUS_TX_FIFO_EXPTY_CNT_SHIFT (4)
#define I2C_FIFO_STATUS_RX_FIFO_FULL_CNT_SHIFT (0)

#define I2C_INTERRUPT_STATUS_REGISTER (0x68)
#define I2C_INTERRUPT_STATUS_PACKET_XFER_COMPLETE (1 << 7)
#define I2C_INTERRUPT_STATUS_ARB_LOST (1 << 2)
#define I2C_INTERRUPT_STATUS_NOACK (1 << 3)
#define I2C_TFIFO_DATA_REQ (1 << 1)
#define I2C_RFIFO_DATA_REQ (1 << 0)

#define I2C_BUS_CLEAR_CONFIG_REGISTER (0x84)
#define I2C_BUS_CLEAR_ENABLE (1)
#define I2C_BUS_CLEAR_SCLK_THRESHOLD_SHIFT (0x16)
#define I2C_BUS_CLEAR_TERMINATE_IMMEDIATE (1 << 1)
#define I2C_BUS_CLEAR_STOP_COND_STOP (1 << 2)
#define I2C_BUS_CLEAR_STOP_COND_NO_STOP (0 << 2)
#define I2C_BUS_CLEAR_DONE (1 << 11)

#define I2C_BUS_CLEAR_STATUS_REGISTER (0x88)
#define I2C_BUS_CLEAR_STATUS_BUS_CLEARED (1)

#define I2C_INTERFACE_TIMING_REGISTER (0x94)
#define TLOW_MASK (0x3F)
#define TLOW_SHIFT (0x0)
#define THIGH_MASK (0xFFFFFF00)
#define THIGH_SHIFT (0x8)

#define I2C_RX_FIFO (0x54)

#endif
