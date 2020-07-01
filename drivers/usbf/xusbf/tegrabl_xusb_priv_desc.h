/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_PRIV_DESC_H
#define INCLUDED_TEGRABL_PRIV_DESC_H

/**
 * USB BOS Descriptor:
 * Stores the Device descriptor data must be word aligned
 */
static uint8_t s_bos_descriptor[USB_BOS_DESCRIPTOR_SIZE] = {
	0x5,  /* bLength - Size of this descriptor in bytes */
	0xF,  /* bDescriptorType - BOS Descriptor Type */
	0x16, /* wTotalLength LSB */
	0x0,  /* wTotalLength MSB */
	0x2,  /* NumDeviceCaps */

	0x7,  /* bLength - Size of USB2.0 extension Device Capability Descriptor */
	0x10, /* bDescriptorType - Device Capability Type */
	0x2,  /* bDevCapabilityType - USB 2.0 Extention */
	0x2,  /* bmAttributes -Bit 1 Capable of generating Latency Tolerace Msgs */
	0x0,  /* Reserved */
	0x0,  /* Reserved */
	0x0,  /* Reserved */

	0xA,  /* bLength - Size of Super Speed Device Capability Descriptor */
	0x10,  /* bDescriptorType - Device Capability Type */
	0x3,  /* bDevCapabilityType - SUPER SPEED USB */
	0x0,  /* bmAttributes - Bit 1 Capable of generating Latency Tolerace Msgs */
	0xC,  /* wSpeedsSupported LSB - Device Supports High and Super speed's */
	0x0,  /* wSpeedsSupported MSB */
	0x2,  /* bFunctionalitySupport - All features available above FS. */
	0xA,  /* bU1DevExitLat - Less than 10us */
	0xFF,  /* wU2DevExitLat LSB */
	0x7,  /* wU2DevExitLat MSB - Less than 2047us */
};

	/** Stores other speed config descriptor */
static uint8_t s_other_speed_config_desc[32] = {
	/** Other speed Configuration Descriptor */
	0x09,   /*/ bLength - Size of this descriptor in bytes */
	0x07,   /*/ bDescriptorType - Other speed Configuration Descriptor Type */
	0x20,   /*/ WTotalLength (LSB) - Total length of data for configuration */
	0x00,   /*/ WTotalLength (MSB) - Total length of data for configuration */
	0x01,   /*/ bNumInterface - Nos of Interface supported by configuration */
	0x01,   /*/ bConfigurationValue */
	0x00,   /*/ iConfiguration - Index of String descriptor describing config*/
	0xc0,   /*/ bmAttributes - Config Characteristcs bitmap
			  "D4-D0: Res, D6: Self Powered,D5: Remote Wakeup */
	0x10,   /*/ MaxPower in mA - Max Power Consumption of the USB device */

	/**Interface Descriptor */
	0x09,   /*/ bLength - Size of this descriptor in bytes */
	0x04,   /*/ bDescriptorType - Interface Descriptor Type */
	0x00,   /*/ binterface_number - Number of Interface */
	0x00,   /*/ bAlternateSetting - Value used to select alternate setting */
	0x02,   /*/ bNumEndpoints - Nos of Endpoints used by this Interface */
	0x08,   /*/ bInterfaceClass - Class code "MASS STORAGE Class." */
	0x06,   /*/ bInterfaceSubClass - Subclass code "SCSI transparent cmd set" */
	0x50,   /*/ bInterfaceProtocol - Protocol code "BULK-ONLY TRANSPORT." */
	0x00,   /*/ iInterface - Index of String descriptor describing Interface */

	/** Endpoint Descriptor IN EP2 */
	0x07,   /*/ bLength - Size of this descriptor in bytes */
	0x05,   /*/ bDescriptorType - ENDPOINT Descriptor Type */
	0x81,   /*/ bEndpointAddress - The address of EP on the USB device
			  "Bit 7: Direction(0:OUT, 1: IN),Bit 6-4:Res,Bit 3-0:EP no" */
	0x02,   /*/ bmAttributes - Bit 1-0: Transfer Type
			  00:Control,01:Isochronous,10: Bulk, 11: Interrupt */
	0x40,   /*/ wMaxPacketSize(LSB) - Maximum Packet Size for this EP */
	0x00,   /*/ wMaxPacketSize(MSB) - Maximum Packet Size for this EP */
	0x00,   /*/ bIntervel - interval for polling EP, applicable for Interrupt
			  and Isochronous data transfer only */

	/** Endpoint Descriptor OUT EP1 */
	0x07,   /*/ bLength - Size of this descriptor in bytes */
	0x05,   /*/ bDescriptorType - ENDPOINT Descriptor Type */
	0x01,   /*/ bEndpointAddress - The address of EP on the USB device
			  "Bit 7: Direction(0:OUT, 1: IN),Bit 6-4:Res,Bit 3-0:EP no" */
	0x02,   /*/ bmAttributes - Bit 1-0: Transfer Type 00:Control,
			  01:Isochronous,10: Bulk, 11: Interrupt */
	0x40,   /*/ wMaxPacketSize(LSB) - Maximum Packet Size for this EP */
	0x00,   /*/ wMaxPacketSize(MSB) - Maximum Packet Size for this EP */
	0x00    /*/ bIntervel - interval for polling EP, applicable for Interrupt
			  and Isochronous data transfer only */
};


/* Stores the Manufactures ID sting descriptor data */
static uint8_t s_usb_manufacturer_id[USB_MANF_STRING_LENGTH] = {
	USB_MANF_STRING_LENGTH,  /* Length of descriptor */
	0x03,                    /* STRING descriptor type. */
	(uint8_t)'N', 0,
	(uint8_t)'V', 0,
	(uint8_t)'I', 0,
	(uint8_t)'D', 0,
	(uint8_t)'I', 0,
	(uint8_t)'A', 0,
	(uint8_t)' ', 0,
	(uint8_t)'C', 0,
	(uint8_t)'o', 0,
	(uint8_t)'r', 0,
	(uint8_t)'p', 0,
	(uint8_t)'.', 0

};

/* Stores the Language ID Descriptor data */
static uint8_t s_usb_language_id[USB_LANGUAGE_ID_LENGTH] = {
	/* Language Id string descriptor */
	USB_LANGUAGE_ID_LENGTH,  /* Length of descriptor */
	0x03,                    /* STRING descriptor type. */
	0x09, 0x04               /* LANGID Code 0: American English 0x409 */
};

/* Stores the Device Qualifier Desriptor data */
static uint8_t s_usb_device_qualifier[USB_DEV_QUALIFIER_LENGTH] = {
	/* Device Qualifier descriptor */
	USB_DEV_QUALIFIER_LENGTH,   /* Size of the descriptor */
	6,    /* Device Qualifier Type */
	0x00, /* USB specification version number: LSB */
	0x02, /*  USB specification version number: MSB */
	0xFF, /* Class Code */
	0xFF, /* Subclass Code */
	0xFF, /* Protocol Code */
	0x40, /*Maximum packet size for other speed */
	0x00, /*Number of Other-speed Configurations */
	0x00  /* Reserved for future use, must be zero */
};


/* Stores the Device status descriptor data */
static uint8_t s_usb_dev_status[USB_DEV_STATUS_LENGTH] = {
	USB_DEVICE_SELF_POWERED,
	0,
};

#endif
