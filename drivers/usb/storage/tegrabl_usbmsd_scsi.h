/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_USBMSD_SCSI_H
#define TEGRABL_USBMSD_SCSI_H

/* SCSI command codes from SPC-5 */
#define TEST_UNIT_READY		0x00
#define REQUEST_SENSE		0x03
#define READ_6			0x08
#define WRITE_6			0x0A
#define INQUIRY			0x12
#define VERIFY_6		0x13
#define MODE_SELECT		0x15
#define MODE_SENSE		0x1A
#define START_STOP_UNIT		0x1B
#define READ_CAPACITY		0x25
#define READ_10			0x28
#define WRITE_10		0x2A
#define VERIFY_10		0x2F
#define MODE_SELECT_10		0x55
#define MODE_SENSE_10		0x5A
#define REPORT_LUNS		0xA0
#define READ_12			0xA8
#define WRITE_12		0xAA
#define VERIFY_12		0xAF

#endif	/* TEGRABL_USBMSD_SCSI_H */
