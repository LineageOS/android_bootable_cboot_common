/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_USB_MSD_H
#define TEGRABL_USB_MSD_H

#include <usbh_protocol.h>
#include <tegrabl_error.h>

#define TEGRABL_USBMSD_SECTOR_SIZE_LOG2	(9)

#define USBMSD_BUFFER_ALIGNMENT		(4096)
#define TEGRABL_USB_BUF_ALIGN_SIZE	8
#define USBMSD_MAX_READ_WRITE_SECTORS	128 /* TODO: This is a WAR for usb read/write to work */

#define TEGRABL_USBMSD_WRITE_TIMEOUT		1000000	/* usec */
#define TEGRABL_USBMSD_READ_TIMEOUT		1000000	/* usec */
#define TEGRABL_USBMSD_REQUEST_SENSE_TIMEOUT	2000000	/* usec */

#define BBB_RESET       0xFF
#define BULK_PROTOCOL   0x50
#define ENDPOINT_HALT	0
#define USB_MSD_CLASS	0x8

/* From  USB Mass Storage Class, Bulk-Only Transport doc, rev 1.0 */
/* TBD: Use TEGRABL_PACKED() here? */

/* Command Block Wrapper */
#define CBW_SIGNATURE	0x43425355
#define CBW_OUT_FLAG	0x00
#define CBW_IN_FLAG	0x80
#define CBW_CDBLENGTH	16
#define	CBW_SIZE	31

typedef struct {
	uint32_t	Signature;		/* 'USBC' */
	uint32_t	Tag;			/* matches dCSWTag */
	uint32_t	DataTransferLength;	/* num data bytes */
	uint8_t		Flags;			/* b7 = direction */
	uint8_t		LUN;			/* device LUN */
	uint8_t		Length;			/* cmd length */
	uint8_t		CDB[CBW_CDBLENGTH];	/* command block */
} usb_msd_cbw_t;

/* Command Status Wrapper */
#define CSW_SIGNATURE	0x53425355
#define CSW_PASS_STAT	0
#define CSW_FAIL_STAT	1
#define CSW_PHASE_STAT	2
#define	CSW_SIZE	13

typedef struct {
	uint32_t	Signature;	/* 'USBS' */
	uint32_t	Tag;		/* matches dCBWTag */
	uint32_t	DataResidue;	/* data difference */
	uint8_t		Status;		/* cmd status */
} usb_msd_csw_t;

/* Bulk-only class specific requests */
#define BULK_RESET_REQUEST		0xFF
#define BULK_GET_MAX_LUN_REQUEST	0xFE

struct tegrabl_usbmsd_xfer_info {
	struct tegrabl_blockdev_xfer_info *bdev_xfer_info;

	/* Current outstanding block count */
	uint32_t bulk_count;

	/* Specifies the block count for the current outstanding xfer */
	uint32_t count;
	void *buf;
	bool is_write;
};

/**
 * @brief Defines the structure for bookkeeping
 */
struct tegrabl_usbmsd_context {
	/* usbmsd instance id */
	uint32_t instance;
	/* Size of a block */
	size_t block_size_log2;
	/* Number of blocks in device */
	uint64_t block_count;
	/* Device-specific transfer info */
	struct tegrabl_usbmsd_xfer_info xfer_info;
	/* host context with descriptor info, etc. from dev enumeration */
	struct xusb_host_context host_context;
	/* general flags, like Unit Attention done, etc. */
	uint16_t flags;
	/* interface number, s/b always 0 */
	uint16_t interface_num;
	/* Which protocol, s/b always BBB */
	uint16_t protocol;
	/* Subclass, s/b always SCSI */
	uint16_t subclass;
	/* Endpoints, an IN and an OUT, holds the EP# */
	uint8_t in_ep;
	uint8_t out_ep;
	/* CBW stuff */
	uint32_t tag;
	uint8_t cmd[CBW_CDBLENGTH];
	uint8_t cmdlen;
	/* CSW stuff */
	uint8_t csw_status;
	/* Hightest logical unit number the device supports */
	uint8_t max_lun;
	/* Active logical unit number, almost always 0 */
	uint8_t current_lun;
	/* Is usbmsd controller initialized */
	bool initialized;
	/* Sense data from request_sense */
	uint8_t sense_data[20];
};

/**
 * @brief initializes the usbmsd controller and context
 *
 * @param context usbmsd context
 * @return TEGRABL_NO_ERROR if successful else appropriate error..
 */
tegrabl_error_t tegrabl_usbmsd_init(struct tegrabl_usbmsd_context *context);

/**
 * @brief Read or write number block starting from specified
 * block
 *
 * @param context Context information
 * @param buf Buffer to save read content or to write to device
 * @param block Start sector for read/write
 * @param count Number of sectors to read/write
 * @param is_write True if write operation
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_usbmsd_xfer(struct tegrabl_usbmsd_context *context,
				    void *buf, bnum_t block, bnum_t count,
				    bool is_write, time_t timeout);

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
/**
 * @brief Erases storage device connected to usbmsd controller
 *
 * @param context Context information
 * @param block start sector from which erasing should start
 * @param count number of sectors to erase
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_usbmsd_erase(struct tegrabl_usbmsd_context *context,
				     bnum_t block, bnum_t count);
#endif	/* ENABLE_BLOCKDEV_BASIC */

/**
 * @brief Frees all buffers allocated during init.
 *
 * @param context usbmsd context.
 */
void tegrabl_usbmsd_free_buffers(struct tegrabl_usbmsd_context *context);

/**
 * @brief Transfer 'length' bytes to/from device
 *
 * @param context Context information
 * @param buf Buffer to save read content or to write to device
 * @param length Number of bytes to transfer
 * @param timeout Time out in uSec
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_usbmsd_io(struct tegrabl_usbmsd_context *context,
					 void *buf, uint32_t length,
					 time_t timeout);

tegrabl_error_t tegrabl_usbmsd_inquiry(struct tegrabl_usbmsd_context *context);

tegrabl_error_t tegrabl_usbmsd_test_unit_ready(
					struct tegrabl_usbmsd_context *context);

tegrabl_error_t tegrabl_usbmsd_read_capacity(
					struct tegrabl_usbmsd_context *context);

#endif	/* TEGRABL_USB_MSD_H */
