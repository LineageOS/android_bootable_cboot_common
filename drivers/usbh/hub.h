/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef __HUB_H
#define __HUB_H

#include <tegrabl_error.h>

/* Hub Characteristics */
#define HUB_POWER_SWITCHING_MASK				0x0003
#define HUB_GET_POWER_SWITCHING_MODE(val)		(val & HUB_POWER_SWITCHING_MASK)

#define HUB_COMPOUND_DEV_MASK					0x0004
#define HUB_COMPOUND_DEV_OFFSET					2
#define HUB_IDENTIFY_COMP_DEV(val)				((val & HUB_COMPOUND_DEV_MASK) >> HUB_COMPOUND_DEV_OFFSET)

#define HUB_OVER_CURR_PROTECTION_MODE_MASK		0x0018
#define HUB_OVER_CURR_PROTECTION_MODE_OFFSET	3
#define HUB_GET_OVER_CURR_PROTECTION_MODE(val)	\
						((val & HUB_OVER_CURR_PROTECTION_MODE_MASK) >> HUB_OVER_CURR_PROTECTION_MODE_OFFSET)

/* Hub port status & port status change fields and functions */
#define HUB_PORT_STATUS_DATA_LEN		4

#define HUB_PORT_STATUS_MASK			0x0000FFFF
#define HUB_PORT_CONN_MASK				0x0001
#define HUB_PORT_RESET_MASK				0x0010
#define HUB_PORT_SPEED_MASK				0x0600
#define HUB_PORT_SPEED_OFFSET			9
#define HUB_PORT_LOW_SPEED_MASK			0x0200
#define HUB_PORT_HIGH_SPEED_MASK		0x0400
#define HUB_GET_PORT_STATUS(val)		(val & HUB_PORT_STATUS_MASK)
#define IS_LOW_SPEED_DEVICE(val)		((val & HUB_PORT_LOW_SPEED_MASK) ? true : false)
#define IS_HIGH_SPEED_DEVICE(val)		((val & HUB_PORT_HIGH_SPEED_MASK) ? true : false)
#define HUB_PORT_GET_SPEED(val)			((val & HUB_PORT_SPEED_MASK) >> HUB_PORT_SPEED_OFFSET)

#define USB_LOW_SPEED					1
#define USB_FULL_SPEED					0
#define USB_HIGH_SPEED					2

#define HUB_PORT_CHANGE_MASK			0xFFFF0000
#define HUB_PORT_CHANGE_OFFSET			16
#define HUB_PORT_CONN_CHANGE_MASK		0x0001
#define HUB_PORT_RESET_CHANGE_MASK		0x0010
#define HUB_GET_PORT_CHANGE(val)		((val & HUB_PORT_CHANGE_MASK) >> HUB_PORT_CHANGE_OFFSET)

/* Hub class feature selectors */
#define HF_PORT_CONNECTION				0
#define HF_PORT_RESET					4
#define HF_PORT_POWER					8
#define HF_C_PORT_CONNECTION			16
#define HF_C_PORT_RESET					20

/* Feature list */
#define DEVICE_REMOTE_WAKEUP			1

/**
 * @brief Initialize hub
 *
 * @param ctx Pointer to host context data structure
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t hub_init(struct xusb_host_context *ctx);

/**
 * @brief Detect device on hub ports
 *
 * @param ctx Pointer to host context data structure
 * @param port_device_bitmap Bitmap where each set bit represent that port has device connected  (output)
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t hub_detect_device(struct xusb_host_context *ctx, uint32_t *port_device_bitmap);

/**
 * @brief Reset hub port and get speed
 *
 * @param ctx Pointer to host context data structure
 * @param port_no Port to reset
 * @param dev_speed Get device speed on this port (output)
 *
 * @return TEGRABL_NO_ERROR if success, specific error if fails
 */
tegrabl_error_t hub_reset_port(struct xusb_host_context *ctx, uint32_t port_no, uint32_t *dev_speed);

#endif
