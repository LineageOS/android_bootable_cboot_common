/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef I2C_BPMP_H
#define I2C_BPMP_H

#include <tegrabl_power_i2c.h>

tegrabl_error_t tegrabl_virtual_i2c_xfer(struct tegrabl_i2c *hi2c,
						uint16_t slave_addr, bool repeat_start,
						void *buf, uint32_t len, bool is_read);

#endif /*__I2C_BPMP_H__*/

