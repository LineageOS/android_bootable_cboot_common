/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_USBH_H
#define TEGRABL_USBH_H

#include <stdint.h>
#include <stdbool.h>
#include <tegrabl_error.h>

/* Specifies the device request strucutre
 * to send the set up data across the USB */
struct device_request {
	union {
		uint32_t bmRequestType:8;
		struct {
			/* Specifies the recipient of the request */
			uint32_t recipient:5;
			/* Specifies the type, whether this request goes on
			   device, endpoint, interface or other */
			uint32_t type:2;
			/* Specifies the data transfer direction */
			uint32_t dataTransferDirection:1;
		} stbitfield;
	} bmRequestTypeUnion;
	uint32_t bRequest:8;    /* Specifies the specific request */
	/* Specifies the field that varies according to request */
	uint32_t wValue:16;
	/* Specifies the field that varies according to request;
	   typically used to pass an index or offset */
	uint32_t wIndex:16;
	/* Specifies the number of bytes to transfer if there is a data state */
	uint32_t wLength:16;

};

/* Initialize host controller, clocks and enumerate the device that is atttached */
tegrabl_error_t tegrabl_usbh_init(void);

/**
 * @brief Powergate XUSBA, XUSBC partitions and assert reset to XUAB PADCTL, XUABA, XUSBC
 * partitions before jumping to kernel.
 *
 * @return tegrabl error code
 */
tegrabl_error_t tegrabl_usbh_close(void);

/**
 * @brief check if specified device is connected or not
 *
 * @param vendor_id
 * @param production_id
 * @return device ID 0: No Device attached or Host driver is not ready
 *                   Others: ready to communicate with this device
 *
 */
uint8_t tegrabl_usbh_get_device_id(uint16_t vendor_id, uint16_t product_id);

/**
 * @brief send command to device through EP0
 *
 * @param device_request_ptr request pointer
 * @return tegrabl error code
 *
 */
tegrabl_error_t tegrabl_usbh_snd_command(struct device_request request, void *buffer);

/**
 * @brief send data to specified device
 *
 * @param device_request_ptr request pointer
 * @return tegrabl error code
 *
 */
tegrabl_error_t tegrabl_usbh_snd_data(uint8_t dev_id, void *buffer, uint32_t *length);

/**
 * @brief receive data from specified device
 *
 * @param device_request_ptr request pointer
 * @return tegrabl error code
 *
 */
tegrabl_error_t tegrabl_usbh_rcv_data(uint8_t dev_id, void *buffer, uint32_t *length);

struct xusb_host_context *tegrabl_get_usbh_context(void);

tegrabl_error_t tegrabl_xusbh_test_sample(void);
#endif  /* TEGRABL_USBH_H */
