/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_TRANSPORT

#include <stdbool.h>
#include <string.h>
#include <tegrabl_drf.h>
#include <tegrabl_ar_macro.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include <tegrabl_debug.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_malloc.h>
#include <tegrabl_io.h>

#include <tegrabl_usbf.h>
#include <tegrabl_transport_usbf.h>
#include <tegrabl_board_info.h>
#include <tegrabl_fuse.h>

#include <armiscreg.h>

#if defined(CONFIG_DT_SUPPORT)
#include <tegrabl_devicetree.h>
#include <libfdt.h>

#define FASTBOOT_USB_PID_PROP_NAME "nvidia,fastboot-usb-pid"
#define FASTBOOT_USB_VID_PROP_NAME "nvidia,fastboot-usb-vid"
#endif

#define MAX_TFR_LENGTH	 (64U * 1024U)
#define MAX_TCM_BUFFER_SUPPORTED 1024U

#define MAX_SERIALNO_LEN 32
#define USB_DESCRIPTOR_SKU_MASK	0xFU


/**
 * USB Device Descriptor: 12 bytes as per the USB2.0 Specification
 * Stores the Device descriptor data must be word aligned
 */
static uint8_t s_ss_device_descr[] = {
	0x12,   /* bLength - Size of this descriptor in bytes */
	0x01,   /* bDescriptorType - Device Descriptor Type */
	0x00,   /* bcd USB (LSB) - USB Spec. Release number */
	0x03,   /* bcd USB (MSB) - USB Spec. Release number (3.0) */
	0x00,   /* bDeviceClass - Class is specified in the interface descriptor. */
	0x00,   /* bDeviceSubClass - SubClass is specified interface descriptor. */
	0x00,   /* bDeviceProtocol - Protocol is specified interface descriptor. */
	0x09,   /* bMaxPacketSize0 - Maximum packet size for EP0 */
	0x55,   /* idVendor(LSB) - Vendor ID assigned by USB forum */
	0x09,   /* idVendor(MSB) - Vendor ID assigned by USB forum */
	0x00,  /* idProduct(LSB) - Product ID assigned by Organization */
	0x70,   /* idProduct(MSB) - Product ID assigned by Organization */
	0x00,   /* bcd Device (LSB) - Device Release number in BCD */
	0x00,   /* bcd Device (MSB) - Device Release number in BCD */
	USB_MANF_ID,   /* Index of String descriptor describing Manufacturer */
	USB_PROD_ID,   /* Index of String descriptor describing Product */
	USB_SERIAL_ID,   /* Index of String descriptor describing Serial number */
	0x01   /* bNumConfigurations - Number of possible configuration */
};

static uint8_t s_hs_device_descr[] = {
	0x12,   /* bLength - Size of this descriptor in bytes */
	0x01,   /* bDescriptorType - Device Descriptor Type */
	0x00,   /* bcd USB (LSB) - USB Spec. Release number */
	0x02,   /* bcd USB (MSB) - USB Spec. Release number (2.1) */
	0x00,   /* bDeviceClass - Class is specified in the interface descriptor. */
	0x00,   /* bDeviceSubClass - SubClass is specified interface descriptor. */
	0x00,   /* bDeviceProtocol - Protocol is specified interface descriptor. */
	0x40,   /* bMaxPacketSize0 - Maximum packet size for EP0 */
	0x55,   /* idVendor(LSB) - Vendor ID assigned by USB forum */
	0x09,   /* idVendor(MSB) - Vendor ID assigned by USB forum */
	0x00,   /* idProduct(LSB) - Product ID assigned by Organization */
	0x71,   /* idProduct(MSB) - Product ID assigned by Organization */
	0x00,   /* bcd Device (LSB) - Device Release number in BCD */
	0x00,   /* bcd Device (MSB) - Device Release number in BCD */
	USB_MANF_ID,   /* Index of String descriptor describing Manufacturer */
	USB_PROD_ID,   /* Index of String descriptor describing Product */
	USB_SERIAL_ID,   /* Index of String descriptor describing Serial number */
	0x01   /* bNumConfigurations - Number of possible configuration */
};

/* Stores the Device Qualifier Desriptor data */
static const uint8_t s_device_qual[] = {
	/* Device Qualifier descriptor */
	0x0a, /* Size of the descriptor */
	0x06, /* Device Qualifier Type */
	0x00, /* USB specification version number: LSB */
	0x02, /*  USB specification version number: MSB */
	0xFF, /* Class Code */
	0xFF, /* Subclass Code */
	0xFF, /* Protocol Code */
	0x40, /*Maximum packet size for other speed */
	0x01, /*Number of Other-speed Configurations */
	0x00  /* Reserved for future use, must be zero */
};

/*
 * USB Device Configuration Descriptors for super speed:
 */
static const uint8_t s_usb_ss_config_descr_3p[] = {
	/* Configuration Descriptor 32 bytes  */
	0x09,   /* bLength - Size of this descriptor in bytes */
	0x02,   /* bDescriptorType - Configuration Descriptor Type */
	0x2c,   /* (LSB)Total length of data for this configuration */
	0x00,   /* (MSB)Total length of data for this configuration */
	0x01,   /* bNumInterface - Nos of Interface supported */
	0x01,   /* bConfigurationValue */
	0x00,   /* iConfiguration - Index for this configuration */
	0xc0,   /* bmAttributes - */
	0x10,   /* MaxPower in mA - */

	/* Interface Descriptor */
	0x09,   /* bLength - Size of this descriptor in bytes */
	0x04,   /* bDescriptorType - Interface Descriptor Type */
	0x00,   /* bInterfaceNumber - Number of Interface */
	0x00,   /* bAlternateSetting - Value used to select alternate setting */
	0x02,   /* bNumEndpoints - Nos of Endpoints used by this Interface */
	0xFF,   /* bInterfaceClass - Class code "Vendor Specific Class." */
	0xFF,   /* bInterfaceSubClass - Subclass code "Vendor specific". */
	0xFF,   /* bInterfaceProtocol - Protocol code "Vendor specific". */
	0x00,   /* iInterface - Index of String descriptor describing Interface */

	/* Endpoint Descriptor IN EP2 */
	0x07,   /* bLength - Size of this descriptor in bytes */
	0x05,   /* bDescriptorType - ENDPOINT Descriptor Type */
	0x81,   /* bEndpointAddress - The address of EP on the USB device */
	0x02,   /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk, */
	0x00,   /* wMaxPacketSize(LSB) - Maximum Packet Size for this EP */
	0x04,   /* wMaxPacketSize(MSB) - Maximum Packet Size for this EP */
	0x00,   /* bIntervel - */

	/* Endpoint IN Companion */
	0x6,    /* bLength - Size of this descriptor in bytes */
	0x30,   /* bDescriptorType - ENDPOINT companion Descriptor */
	0x1,    /* MaxBurst */
	0x0,    /* Attributes */
	0x0,    /* Interval */
	0x0,    /* Interval */

	/** Endpoint Descriptor OUT EP1 */
	0x07,   /* bLength - Size of this descriptor in bytes */
	0x05,   /* bDescriptorType - ENDPOINT Descriptor Type */
	0x01,   /* bEndpointAddress - The address of EP on the USB device */
	0x02,   /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk */
	0x00,   /* wMaxPacketSize(LSB) - Maximum Packet Size for this EP */
	0x04,   /* wMaxPacketSize(MSB) - Maximum Packet Size for this EP */
	0x00,   /* bIntervel - */

	/* Endpoint OUT Companion */
	0x6,   /* bLength - Size of this descriptor in bytes */
	0x30,  /* bDescriptorType - ENDPOINT companion Descriptor */
	0xF,   /* MaxBurst */
	0x0,   /* Attributes */
	0x0,   /* Interval */
	0x0    /* Interval */
};

/*
 * USB Device Configuration Descriptors for high speed:
 * 32 bytes as per the USB2.0 Specification. This contains
 * Configuration descriptor, Interface descriptor and endpoint descriptors.
 */
static const uint8_t s_usb_hs_config_descr_3p[] = {
	/* Configuration Descriptor 32 bytes  */
	0x09,   /* bLength - Size of this descriptor in bytes */
	0x02,   /* bDescriptorType - Configuration Descriptor Type */
	0x20,   /* (LSB)Total length of data for this configuration */
	0x00,   /* (MSB)Total length of data for this configuration */
	0x01,   /* bNumInterface - Nos of Interface supported */
	0x01,   /* bConfigurationValue */
	0x00,   /* iConfiguration - Index for this configuration */
	0xc0,   /* bmAttributes - */
	0x10,   /* MaxPower in mA - */

	/* Interface Descriptor */
	0x09,   /* bLength - Size of this descriptor in bytes */
	0x04,   /* bDescriptorType - Interface Descriptor Type */
	0x00,   /* bInterfaceNumber - Number of Interface */
	0x00,   /* bAlternateSetting - Value used to select alternate setting */
	0x02,   /* bNumEndpoints - Nos of Endpoints used by this Interface */
	0xFF,   /* bInterfaceClass - Class code "Vendor Specific Class." */
	0xFF,   /* bInterfaceSubClass - Subclass code "Vendor specific". */
	0xFF,   /* bInterfaceProtocol - Protocol code "Vendor specific". */
	0x00,   /* iInterface - Index of String descriptor describing Interface */

	/* Endpoint Descriptor IN EP2 */
	0x07,   /* bLength - Size of this descriptor in bytes */
	0x05,   /* bDescriptorType - ENDPOINT Descriptor Type */
	0x81,   /* bEndpointAddress - The address of EP on the USB device */
	0x02,   /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk, */
	0x00,   /* wMaxPacketSize(LSB) - Maximum Packet Size for this EP */
	0x02,   /* wMaxPacketSize(MSB) - Maximum Packet Size for this EP */
	0x00,   /* bIntervel - */

	/** Endpoint Descriptor OUT EP1 */
	0x07,   /* bLength - Size of this descriptor in bytes */
	0x05,   /* bDescriptorType - ENDPOINT Descriptor Type */
	0x01,   /* bEndpointAddress - The address of EP on the USB device */
	0x02,   /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk */
	0x00,   /* wMaxPacketSize(LSB) - Maximum Packet Size for this EP */
	0x02,   /* wMaxPacketSize(MSB) - Maximum Packet Size for this EP */
	0x00    /* bIntervel - */
};

/** super speed  config descriptor for fastboot */
static uint8_t s_usb_ss_config_descr_fastboot[] = {
	/* Configuration Descriptor 32 bytes  */
	0x09,   /* bLength - Size of this descriptor in bytes */
	0x02,   /* bDescriptorType - Configuration Descriptor Type */
	0x2c,   /* WTotalLength (LSB) - length of data for this configuration */
	0x00,   /* WTotalLength (MSB) - length of data for this configuration */
	0x01,   /* bNumInterface - Nos of Interface supported by configuration */
	0x01,   /* bConfigurationValue */
	0x00,   /* iConfiguration - Index of descriptor describing configuration */
	0xc0,   /* bmAttributes-bitmap "D4-D0:Res,D6:Self Powered,D5:Remote Wakeup*/
	0x10,   /* MaxPower in mA - Max Power Consumption of the USB device */

	/* Interface Descriptor */
	0x09,   /* bLength - Size of this descriptor in bytes */
	0x04,   /* bDescriptorType - Interface Descriptor Type */
	0x00,   /* binterface_number - Number of Interface */
	0x00,   /* bAlternateSetting - Value used to select alternate setting */
	0x02,   /* bNumEndpoints - Nos of Endpoints used by this Interface */
	0xFF,   /* bInterfaceClass - Class code "Vendor Specific Class." */
	0x42,   /* bInterfaceSubClass - Subclass code "Vendor specific". */
	0x03,   /* bInterfaceProtocol - Protocol code "Vendor specific". */
	0x00,   /* iInterface - Index of String descriptor describing Interface */

	/* Endpoint Descriptor IN EP1 */
	0x07,   /* bLength - Size of this descriptor in bytes */
	0x05,   /* bDescriptorType - ENDPOINT Descriptor Type */
	0x81,   /* bEndpointAddress - The address of EP on the USB device  */
	0x02,   /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk,  */
	0x00,   /* wMaxPacketSize(LSB) - Maximum Packet Size for EP */
	0x04,   /* wMaxPacketSize(MSB) - Maximum Packet Size for EP */
	0x00,   /* bInterval - interval for polling EP, for Int and Isochronous  */

	/* Endpoint IN Companion */
	0x6,    /* bLength - Size of this descriptor in bytes */
	0x30,   /* bDescriptorType - ENDPOINT companion Descriptor */
	0x1,    /* MaxBurst */
	0x0,    /* Attributes */
	0x0,    /* Interval */
	0x0,    /* Interval */

	/** Endpoint Descriptor OUT EP1 */
	0x07,   /* bLength - Size of this descriptor in bytes */
	0x05,   /* bDescriptorType - ENDPOINT Descriptor Type */
	0x01,   /* bEndpointAddress - The address of EP on the USB device  */
	0x02,   /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk,  */
	0x00,   /* wMaxPacketSize(LSB) - Maximum Packet Size for EP */
	0x04,   /* wMaxPacketSize(MSB) - Maximum Packet Size for EP */
	0x00,   /* bInterval - interval for polling EP, for Int Isochronous  */

	/* Endpoint OUT Companion */
	0x6,    /* bLength - Size of this descriptor in bytes */
	0x30,   /* bDescriptorType - ENDPOINT companion Descriptor */
	0xF,    /* MaxBurst */
	0x0,    /* Attributes */
	0x0,    /* Interval */
	0x0,    /* Interval */
};

/** Hish speed config descriptor for fastboot protocol */
static uint8_t s_usb_hs_config_descr_fastboot[] = {
	/* Configuration Descriptor 32 bytes  */
	0x09,   /* bLength - Size of this descriptor in bytes */
	0x02,   /* bDescriptorType - Configuration Descriptor Type */
	0x20,   /* WTotalLength (LSB) - length of data for this configuration */
	0x00,   /* WTotalLength (MSB) - length of data for this configuration */
	0x01,   /* bNumInterface - Nos of Interface supported by configuration */
	0x01,   /* bConfigurationValue */
	0x00,   /* iConfiguration - Index of descriptor describing configuration */
	0xc0,   /* bmAttributes-bitmap "D4-D0:Res,D6:Self Powered,D5:Remote Wakeup*/
	0x10,   /* MaxPower in mA - Max Power Consumption of the USB device */

	/* Interface Descriptor */
	0x09,   /* bLength - Size of this descriptor in bytes */
	0x04,   /* bDescriptorType - Interface Descriptor Type */
	0x00,   /* binterface_number - Number of Interface */
	0x00,   /* bAlternateSetting - Value used to select alternate setting */
	0x02,   /* bNumEndpoints - Nos of Endpoints used by this Interface */
	0xFF,   /* bInterfaceClass - Class code "Vendor Specific Class." */
	0x42,   /* bInterfaceSubClass - Subclass code "Vendor specific". */
	0x03,   /* bInterfaceProtocol - Protocol code "Vendor specific". */
	0x00,   /* iInterface - Index of String descriptor describing Interface */

	/* Endpoint Descriptor IN EP1 */
	0x07,   /* bLength - Size of this descriptor in bytes */
	0x05,   /* bDescriptorType - ENDPOINT Descriptor Type */
	0x81,   /* bEndpointAddress - The address of EP on the USB device  */
	0x02,   /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk,  */
	0x00,   /* wMaxPacketSize(LSB) - Maximum Packet Size for EP */
	0x02,   /* wMaxPacketSize(MSB) - Maximum Packet Size for EP */
	0x00,   /* bInterval - interval for polling EP, for Int and Isochronous  */

	/** Endpoint Descriptor OUT EP1 */
	0x07,   /* bLength - Size of this descriptor in bytes */
	0x05,   /* bDescriptorType - ENDPOINT Descriptor Type */
	0x01,   /* bEndpointAddress - The address of EP on the USB device  */
	0x02,   /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk,  */
	0x00,   /* wMaxPacketSize(LSB) - Maximum Packet Size for EP */
	0x02,   /* wMaxPacketSize(MSB) - Maximum Packet Size for EP */
	0x00    /* bInterval - interval for polling EP, for Int Isochronous  */
};

/*
 * USB Device other speed Configuration Descriptors:
 * 32 bytes as per the USB2.0 Specification. This contains
 * Configuration descriptor, Interface descriptor and endpoint descriptors.
 */
static const uint8_t s_other_speed_config_descr[] = {
	/* Configuration Descriptor 32 bytes  */
	0x09,   /* bLength - Size of this descriptor in bytes */
	0x07,   /* bDescriptorType - Configuration Descriptor Type */
	0x20,   /* (LSB)Total length of data for this configuration */
	0x00,   /* (MSB)Total length of data for this configuration */
	0x01,   /* bNumInterface - Nos of Interface supported */
	0x01,   /* bConfigurationValue */
	0x00,   /* iConfiguration - Index for this configuration */
	0xc0,   /* bmAttributes - */
	0x10,   /* MaxPower in mA - */

	/* Interface Descriptor */
	0x09,   /* bLength - Size of this descriptor in bytes */
	0x04,   /* bDescriptorType - Interface Descriptor Type */
	0x00,   /* bInterfaceNumber - Number of Interface */
	0x00,   /* bAlternateSetting - Value used to select alternate setting */
	0x02,   /* bNumEndpoints - Nos of Endpoints used by this Interface */
	0xFF,   /* bInterfaceClass - Class code "Vendor Specific Class." */
	0xFF,   /* bInterfaceSubClass - Subclass code "Vendor specific". */
	0xFF,   /* bInterfaceProtocol - Protocol code "Vendor specific". */
	0x00,   /* iInterface - Index of String descriptor describing Interface */

	/* Endpoint Descriptor IN EP2 */
	0x07,   /* bLength - Size of this descriptor in bytes */
	0x05,   /* bDescriptorType - ENDPOINT Descriptor Type */
	0x81,   /* bEndpointAddress - The address of EP on the USB device */
	0x02,   /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk, */
	0x40,   /* wMaxPacketSize(LSB) - Maximum Packet Size for this EP */
	0x00,   /* wMaxPacketSize(MSB) - Maximum Packet Size for this EP */
	0x00,   /* bIntervel - */

	/** Endpoint Descriptor OUT EP1 */
	0x07,   /* bLength - Size of this descriptor in bytes */
	0x05,   /* bDescriptorType - ENDPOINT Descriptor Type */
	0x01,   /* bEndpointAddress - The address of EP on the USB device */
	0x02,   /* bmAttributes - Bit 1-0: Transfer Type 10: Bulk */
	0x40,   /* wMaxPacketSize(LSB) - Maximum Packet Size for this EP */
	0x00,   /* wMaxPacketSize(MSB) - Maximum Packet Size for this EP */
	0x00    /* bIntervel - */
};

/* Stores the Language ID Descriptor data */
static const uint8_t s_usb_language_id[] = {
	/* Language Id string descriptor */
	4,               /* Length of descriptor */
	0x03,          /* STRING descriptor type. */
	0x09, 0x04  /* LANGID Code 0: American English 0x409 */
};

/* Stores the Manufactures ID sting descriptor data */
static const uint8_t s_usb_manufacturer_id[] = {
	0x1A,  /* Length of descriptor */
	0x03,  /* STRING descriptor type. */
	'N', 0,
	'V', 0,
	'I', 0,
	'D', 0,
	'I', 0,
	'A', 0,
	' ', 0,
	'C', 0,
	'o', 0,
	'r', 0,
	'p', 0,
	'.', 0
};

/* Stores the Product ID string descriptor data */
static const uint8_t s_usb_product_id_3p[] = {
	0x8,   /* Length of descriptor */
	0x03, /* STRING descriptor type. */
	'A', 0x00,
	'P', 0x00,
	'X', 0x00
};

/* Stores the Product ID string descriptor data */
static uint8_t s_usb_product_id_fastboot[] = {
	0x12,  /* Length of descriptor */
	0x03,  /* STRING descriptor type. */
	'F', 0x00,
	'a', 0x00,
	's', 0x00,
	't', 0x00,
	'b', 0x00,
	'o', 0x00,
	'o', 0x00,
	't', 0x00
};

/* Stores the Serial Number String descriptor data */
static uint8_t s_usb_serial_number[MAX_SERIALNO_LEN * 2 + 2] = {
	[0] = 0xc,    /* Length of descriptor */
	[1] = 0x03,  /* STRING descriptor type. */
	[2] = '0', [3] = 0x00,
	[4] = '0', [5] = 0x00,
	[6] = '0', [7] = 0x00,
	[8] = '0', [9] = 0x00,
	[10] = '0', [11] = 0x00
};

static struct usbf_config config_3p = {
	.hs_device = USB_DESC_STATIC(s_hs_device_descr),
	.ss_device = USB_DESC_STATIC(s_ss_device_descr),
	.device_qual = USB_DESC_STATIC(s_device_qual),
	.ss_config = USB_DESC_STATIC(s_usb_ss_config_descr_3p),
	.hs_config = USB_DESC_STATIC(s_usb_hs_config_descr_3p),
	.other_config = USB_DESC_STATIC(s_other_speed_config_descr),
	.langid = USB_DESC_STATIC(s_usb_language_id),
	.manufacturer = USB_DESC_STATIC(s_usb_manufacturer_id),
	.product = USB_DESC_STATIC(s_usb_product_id_3p),
	.serialno = USB_DESC_STATIC(s_usb_serial_number),
};

static struct usbf_config config_fastboot = {
	.hs_device = USB_DESC_STATIC(s_hs_device_descr),
	.ss_device = USB_DESC_STATIC(s_ss_device_descr),
	.device_qual = USB_DESC_STATIC(s_device_qual),
	.ss_config = USB_DESC_STATIC(s_usb_ss_config_descr_fastboot),
	.hs_config = USB_DESC_STATIC(s_usb_hs_config_descr_fastboot),
	.other_config = USB_DESC_STATIC(s_other_speed_config_descr),
	.langid = USB_DESC_STATIC(s_usb_language_id),
	.manufacturer = USB_DESC_STATIC(s_usb_manufacturer_id),
	.product = USB_DESC_STATIC(s_usb_product_id_fastboot),
	.serialno = USB_DESC_STATIC(s_usb_serial_number),
};

static uint8_t *ptempbuf;

static bool is_buffer_from_tcm(const void *buf)
{
	uint8_t *tcm_start = (uint8_t *)(NV_ADDRESS_MAP_BPMP_BTCM_BASE);
	uint32_t tcm_size = NV_ADDRESS_MAP_BPMP_BTCM_SIZE;
	uint8_t *tbuf = (uint8_t *)buf;

	if ((tbuf >= tcm_start) && (tbuf <= (tcm_start + tcm_size)))
		return true;
	else
		return false;
}

tegrabl_error_t tegrabl_transport_usbf_send(const void *buffer,
											uint32_t length,
											uint32_t *bytes_transmitted,
											time_t timeout)
{
	tegrabl_error_t retval = TEGRABL_NO_ERROR;
	uint32_t tfr_length;
	uint32_t bytes_sent = 0;
	uint8_t *buf = (uint8_t *)buffer;

	TEGRABL_UNUSED(timeout);
	*bytes_transmitted = 0;
	/*
	 1) Check given buffer is from BTCM, need to maintain local buffer
	 copy data to that.
    */
	if (is_buffer_from_tcm(buffer)) {
		if (length <= MAX_TCM_BUFFER_SUPPORTED) {
			memcpy((void *)ptempbuf, buffer, length);
			buf = ptempbuf;
		} else {
			pr_info(
							   "ERROR: Passed TCM buffer greater than 1K\n");
			return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		}
	}

	while (length != 0U) {
		if (length > MAX_TFR_LENGTH)
			tfr_length = MAX_TFR_LENGTH;
		else
			tfr_length = length;

		retval = tegrabl_usbf_transmit((uint8_t *)buf, tfr_length, &bytes_sent);
		if (retval != TEGRABL_NO_ERROR) {
			pr_critical(
							   "ERROR: Add to request queue failed\n");
			goto fail;
		}

		length = length - tfr_length;
		buf = buf + tfr_length;
		*bytes_transmitted += bytes_sent;
	}
	return TEGRABL_NO_ERROR;

fail:
	pr_critical("ERROR: USB SEND FAILED\n");
	return retval;
}

tegrabl_error_t tegrabl_transport_usbf_receive(void *buf, uint32_t length,
											   uint32_t *received,
											   time_t timeout)
{
	tegrabl_error_t retval = TEGRABL_NO_ERROR;
	uint32_t tfr_length;
	uint32_t bytes_received = 0;
	void *dataptr = buf;
	bool is_tcm_buffer = is_buffer_from_tcm(buf);


	TEGRABL_UNUSED(timeout);
	*received = 0;

	if (is_tcm_buffer) {
		if (length <= MAX_TCM_BUFFER_SUPPORTED) {
			dataptr = ptempbuf;
		} else {
			pr_info(
							   "ERROR: Passed TCM buffer greater than 1K\n");
			return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 1);
		}
	}

	while (length != 0U) {
		if (length > MAX_TFR_LENGTH)
			tfr_length = MAX_TFR_LENGTH;
		else
			tfr_length = length;

		retval = tegrabl_usbf_receive((uint8_t *)dataptr, tfr_length,
					 &bytes_received);
		if (retval != TEGRABL_NO_ERROR) {
			goto fail;
		}

		if (is_tcm_buffer) {
			memcpy(buf, dataptr, tfr_length);
		}

		*received += bytes_received;
		length = length - tfr_length;

		dataptr = (uint8_t *)dataptr + tfr_length;
	}

	return TEGRABL_NO_ERROR;

fail:
	pr_critical("ERROR: USB RECEIVE FAILED\n");
	return retval;
}

#if defined(CONFIG_ENABLE_USBF_SNO)
static tegrabl_error_t update_usbf_serial_no(void)
{
	tegrabl_error_t ret = TEGRABL_NO_ERROR;
	uint8_t serial_str[MAX_SERIALNO_LEN];
	uint32_t i;

	ret = tegrabl_get_serial_no(serial_str);
	if (ret != TEGRABL_NO_ERROR) {
		pr_error("Failed to get serial no\n");
		return ret;
	}

	/**
	 * s_usb_serial_number definition
	 * s_usb_serial_number[MAX_SERIALNO_LEN * 2 + 2] = {
	 *     length,                    valid length of this array
	 *     0x03,                      means string type
	 *     serial_str[0],  0x00,
	 *     serial_str[1],  0x00,
	 *     serial_str[2],  0x00,
	 *     serial_str[3],  0x00,
	 *     ...
	 *     serial_str[n],  0x00
	 * }
	 */
	for (i = 0; (serial_str[i] != ' ') && (serial_str[i] != '\0'); i++)
		s_usb_serial_number[i * 2 + 2] = serial_str[i];

	s_usb_serial_number[0] = i * 2 + 2;

	return TEGRABL_NO_ERROR;
}
#else
static tegrabl_error_t update_usbf_serial_no(void)
{
	return TEGRABL_NO_ERROR;
}
#endif

static tegrabl_error_t tegrabl_set_pid(uint32_t class)
{
	uint32_t regval;
	uint32_t sku;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

#if defined(CONFIG_DT_SUPPORT)
	void *fdt;
	const uint8_t *product_id = NULL;

	/* Take fastboot product id from dtb if exists */
	if (class != TEGRABL_USB_CLASS_FASTBOOT) {
		goto skip_dt;
	}

	e = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if ((e != TEGRABL_NO_ERROR) || (fdt == NULL)) {
		goto skip_dt;
	}

	product_id = fdt_getprop(fdt, 0, FASTBOOT_USB_PID_PROP_NAME, NULL);

	if (product_id) {
		s_hs_device_descr[10] = product_id[3];
		s_hs_device_descr[11] = product_id[2];
		return TEGRABL_NO_ERROR;
	}

skip_dt:
#endif
	/** Product ID definition (2 bytes as per USB 2.0 Spec)
	 *  idProduct(LSB - 8bits) = Chip ID for 3p, 0xFB for fastboot
	 *  idProduct(MSB - 8bits) = (HIDFAM << 4 | SKU)
	*/

	 /* Read the Chip ID revision register */
	regval  = NV_READ32(NV_ADDRESS_MAP_MISC_BASE + MISCREG_HIDREV_0);

	if (class == TEGRABL_USB_CLASS_FASTBOOT) {
		s_hs_device_descr[10] = 0xFB;
	} else {
		s_hs_device_descr[10] =
				(uint8_t)NV_DRF_VAL(MISCREG, HIDREV, CHIPID, regval);
	}
	s_ss_device_descr[10] = s_hs_device_descr[10];
	/* idProduct(MSB) - Read SKu value, HIDFAM and define the MSB */
	e = tegrabl_fuse_read(FUSE_SKU_INFO, &sku, sizeof(sku));
	if (TEGRABL_NO_ERROR != e) {
		pr_warn("Fuse read failed\n");
		return e;
	}

	s_hs_device_descr[11] = s_ss_device_descr[11] =
		(uint8_t)((NV_DRF_VAL(MISCREG, HIDREV, HIDFAM, regval) << 4) |
											   (sku & USB_DESCRIPTOR_SKU_MASK));
	return e;
}

static tegrabl_error_t tegrabl_set_vid(uint32_t class)
{
	tegrabl_error_t e = TEGRABL_NO_ERROR;
#if defined(CONFIG_DT_SUPPORT)
	void *fdt;
	const uint8_t *vendor_id = NULL;

	/* Take fastboot vendor id from dtb if exists */
	if (class != TEGRABL_USB_CLASS_FASTBOOT) {
		goto skip_dt;
	}

	e = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if ((e != TEGRABL_NO_ERROR) || (fdt == NULL)) {
		goto skip_dt;
	}

	vendor_id = fdt_getprop(fdt, 0, FASTBOOT_USB_VID_PROP_NAME, NULL);

	if (vendor_id) {
		s_hs_device_descr[8] = vendor_id[3];
		s_hs_device_descr[9] = vendor_id[2];
	}

skip_dt:
#else
	TEGRABL_UNUSED(class);
#endif
	return e;
}

static tegrabl_error_t transport_usbf_priv_open(bool reinit,
	uint32_t usb_class)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool default_flag = false;

	ptempbuf = tegrabl_alloc(TEGRABL_HEAP_DMA, MAX_TCM_BUFFER_SUPPORTED);
	if (ptempbuf == NULL) {
		pr_error("Failed to allocate memory for xusb temp buffer\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	err = tegrabl_set_pid(usb_class);
	if (TEGRABL_NO_ERROR != err) {
		pr_warn("Set pid failed:Use default PID\n");
	}

	err = tegrabl_set_vid(usb_class);
	if (TEGRABL_NO_ERROR != err) {
		pr_warn("Set vid failed:Use default VID");
	}

	switch (usb_class) {
	case TEGRABL_USB_CLASS_3P:
		tegrabl_usbf_setup(&config_3p);
		break;
	case TEGRABL_USB_CLASS_FASTBOOT:
		err = update_usbf_serial_no();
		if (err != TEGRABL_NO_ERROR){
			pr_warn("Failed to fastboot usb");
		}

		tegrabl_usbf_setup(&config_fastboot);
		break;
	case TEGRABL_USB_CLASS_CHARGING:
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		default_flag = true;
		break;
	}

	if (default_flag) {
		goto fail;
	}

	if (reinit == true) {
		err = tegrabl_usbf_reinit();
	} else {
		err = tegrabl_usbf_init();
	}

	if (err != TEGRABL_NO_ERROR) {
		pr_error("usbf_init failed err = 0x%x\n", err);
		goto fail;
	}

	err = tegrabl_usbf_start();
	if (err != TEGRABL_NO_ERROR) {
		pr_error("usbf_start failed err = 0x%x\n", err);
		goto fail;
	}

	if (reinit == false) {
		err = tegrabl_usbf_enumerate(NULL);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("usbf_enum failed err = 0x%x\n", err);
			goto fail;
		}
	}

	pr_info("USB configuration success\n");
fail:
	return err;
}

tegrabl_error_t tegrabl_transport_usbf_open(uint32_t instance, void *dev_info)
{
	struct usbf_priv_info *info = (struct usbf_priv_info *)dev_info;

	TEGRABL_UNUSED(instance);
	pr_trace("Reopen = %d\n", info->reopen);
	return transport_usbf_priv_open(info->reopen, info->usb_class);
}

tegrabl_error_t tegrabl_transport_usbf_close(uint32_t instance)
{
	/* Free local allocated buffers */
	return tegrabl_usbf_close(instance);
}

