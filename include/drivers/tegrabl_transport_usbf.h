/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef __TEGRABL_TRANSPORT_USBF_H
#define __TEGRABL_TRANSPORT_USBF_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>

enum usb_product {
	TEGRABL_USB_CLASS_3P,
	TEGRABL_USB_CLASS_FASTBOOT,
	TEGRABL_USB_CLASS_CHARGING
};

struct usbf_priv_info {
	bool reopen;
	uint32_t usb_class;
};

/**
 * @brief open initializes the USB.
 *
 * @note This is a blocking interface.
 *
 * @param instance Number indicating the usb instance
 * @param dev_info Pointer to device info
 *
 * @return Returns if there is any failure
 */
tegrabl_error_t tegrabl_transport_usbf_open(uint32_t instance, void *dev_info);

/**
 * @brief Sends data over the Usb.
 *
 * @note This is a blocking interface.
 *
 * @param buffer A pointer to the data bytes to send.
 * @param length The number of bytes to send.
 * @param bytes_transmitted Actual bytes sent returned from call
 * @param timeout transfer timeout value in msec.
 *
 * @return NO_ERROR if init is successful.
 */
tegrabl_error_t tegrabl_transport_usbf_send(const void *buffer, uint32_t length,
											uint32_t *bytes_transmitted,
											time_t timeout);

/**
 * Receives data over the Usb.
 *
 * @note This is a blocking interface.
 *
 * @param buf A pointer to the data bytes to receive.
 * @param length The maximum number of bytes to to receive.
 * @param received A pointer to the bytes received. \a received may be null.
 * @param timeout transfer timeout value in msec.
 *
 * @return NO_ERROR if init is successful.
 */
tegrabl_error_t tegrabl_transport_usbf_receive(void *buf, uint32_t length,
		uint32_t *received, time_t timeout);

/**
 * Closes USB Device.
 *
 */
tegrabl_error_t tegrabl_transport_usbf_close(uint32_t instance);

#endif /* __TRANSPORT_USBF_H */
