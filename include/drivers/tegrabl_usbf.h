/*
 * Copyright (c) 2015 - 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_USBF_H
#define TEGRABL_USBF_H

#include <stdint.h>
#include <stddef.h>
#include <tegrabl_error.h>


/**
 * Defines USB string descriptor Index
 * As per USB Specification.
 */
enum string_descriptor_index {
	/* Specifies a Language ID string descriptor index */
	USB_LANGUAGE_ID = 0,
	/*Specifies a Manufacturer ID string descriptor index */
	USB_MANF_ID = 1,
	/* Specifies a Product ID string descriptor index */
	USB_PROD_ID = 2,
	/* Specifies a Serial No string descriptor index */
	USB_SERIAL_ID = 3
};

struct usb_descriptor {
	void *desc;
	size_t len;
	uint32_t flags;
};

#define USB_DESC_FLAG_STATIC (0x1)

#define USB_DESC_STATIC(x)      \
	{ .desc = (void *)(x), .len = sizeof(x), .flags = USB_DESC_FLAG_STATIC}

struct usb_string {
	struct usb_descriptor string;
	uint8_t id;
};

/* complete usb config struct, passed in to usb_setup() */
struct usbf_config {
	/* device desc for high speed.*/
	struct usb_descriptor hs_device;
	/* device desc fpr super speed.*/
	struct usb_descriptor ss_device;
	/* device qualifier desc data.*/
	struct usb_descriptor device_qual;
	/* configuration desc data for super speed.*/
	struct usb_descriptor ss_config;
	/* configuration desc data for non-super speed.*/
	struct usb_descriptor hs_config;
	/* Other Speed configuration desc data.*/
	struct usb_descriptor other_config;
	/* Language id desc data.*/
	struct usb_descriptor langid;
	/* Manufacturer string desc data. */
	struct usb_descriptor manufacturer;
	/* product string desc data.*/
	struct usb_descriptor product;
	/* serialno string desc data. */
	struct usb_descriptor serialno;
};

/**
 * @brief USB descriptor data will be passed to the udc driver through this API.
 *
 * @param config pointer to the usb_config structure.
 */
void tegrabl_usbf_setup(struct usbf_config *config);

/**
 * @brief Initializes the USBF controller driver. As soon as device is connected
 * to Host, does enumeration. After enumeration, informs the registered
 * function driver through notifier.
 *
 * @return returns the status.
 */
tegrabl_error_t  tegrabl_usbf_init(void);

/**
 * @brief Start the already initialized controller.
 * This API needs to be called only after tegrabl_usbf_init
 * and no need to call after tegrabl_usbf_reinit() API.
 *
 * @return returns the status.
 */
tegrabl_error_t tegrabl_usbf_start(void);

/**
 * @brief ReInitializes the USBF controller driver. This API needs to be used
 * only if controller is already initialized and enumerated.
 *
 * @return returns the status.
 */
tegrabl_error_t tegrabl_usbf_reinit(void);


/**
 * @brief Starts the enumeration process and exits after enumeration is done.
 *
 * @param buffer buffer pointer which can be used for control transfer.
 * If it is NULL, stack allocates its own buffer.
 *
 * @return returns the status.
 */
tegrabl_error_t tegrabl_usbf_enumerate(uint8_t *buffer);

/**
 * @brief Transmit the data on the USB  bus and exit only after transfer
 * completed.
 *
 * @param buffer buffer pointer contains the data to be transfered.
 *
 * @param bytes Number of bytest to be transfered.
 *
 * @param bytes_transmitted Pointer retuns the number of bytes
 * actually transfered.
 *
 * @return returns the status.
 */
tegrabl_error_t tegrabl_usbf_transmit(uint8_t *buffer, uint32_t bytes,
			uint32_t *bytes_transmitted);

/**
 * @brief Transmit the data on the USB  bus and exit after transfer
 * request submitted.
 *
 * @param buffer buffer pointer contains the data to be transfered.
 *
 * @param bytes Number of bytest to be transfered.
 *
 * @return returns the status.
 */
tegrabl_error_t tegrabl_usbf_transmit_start(uint8_t *buffer, uint32_t bytes);


/**
 * @brief Checks the whether transfer completion status
 * completed.
 *
 * @param P_bytes_transferred Pointer retuns the number of bytes
 * actually transfered.

 * @param timeout Maximum timeout value in msec.
 *
 * @return returns the status.
 */
tegrabl_error_t tegrabl_usbf_transmit_complete(uint32_t *p_bytes_transferred,
		uint32_t timeout);

/**
 * @brief Receive data from the USB  bus and exit only after transfer
 * completed.
 *
 * @param buffer buffer pointer contains the data to be transfered.
 *
 * @param bytes Number of bytest to be transfered.
 *
 * @param bytes_received Pointer retuns the number of bytes
 * actually received.
 *
 * @return returns the status.
 */
tegrabl_error_t tegrabl_usbf_receive(uint8_t *buffer, uint32_t bytes,
		uint32_t *bytes_received);

/**
 * @brief Submit the data receive request and return immediatly.
 *
 * @param buffer buffer pointer contains the data to be transfered.
 *
 * @param bytes Number of bytest to be transfered.
 *
 * @return returns the status.
 */
tegrabl_error_t tegrabl_usbf_receive_start(uint8_t *buffer, uint32_t bytes);

/**
 * @brief Checks the whether previous receive request completed or not.
 *
 * @param P_bytes_received Pointer retuns the number of bytes
 * actually received.

 * @param timeout Maximum timeout value in msec.
 *
 * @return returns the status.
 */
tegrabl_error_t tegrabl_usbf_receive_complete(uint32_t *bytes_received,
		uint32_t timeout_us);

/**
 * @brief stop the already initialized controller.
 *
 * @param instance controller instance.
 *
 * @return returns TEGRABL_NO_ERROR if success.
 * Error code if fails.
 */
tegrabl_error_t tegrabl_usbf_close(uint32_t instance);

#endif
