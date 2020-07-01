/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_USBMSD_ERR_AUX_H
#define TEGRABL_USBMSD_ERR_AUX_H

#define TEGRABL_USBMSD_BDEV_ERASE	0x01U
#define TEGRABL_USBMSD_BDEV_IOCTL	0x02U
#define TEGRABL_USBMSD_BDEV_OPEN	0x03U
#define TEGRABL_USBMSD_BDEV_READ_BLOCK	0x04U
#define TEGRABL_USBMSD_BDEV_WRITE_BLOCK	0x05U
#define TEGRABL_USBMSD_BDEV_XFER_1	0x06U
#define TEGRABL_USBMSD_BDEV_XFER_2	0x07U
#define TEGRABL_USBMSD_BDEV_XFER_WAIT_1	0x08U
#define TEGRABL_USBMSD_BDEV_XFER_WAIT_2	0x09U
#define TEGRABL_USBMSD_REGISTER		0x0AU
#define TEGRABL_USBMSD_START_COMMAND_1	0x0BU
#define TEGRABL_USBMSD_START_COMMAND_2	0x0CU
#define TEGRABL_USBMSD_XFER_COMPLETE_1	0x0DU
#define TEGRABL_USBMSD_XFER_COMPLETE_2	0x0EU
#define TEGRABL_USBMSD_GET_MAX_LUN	0x0FU
#define TEGRABL_USBMSD_BULK_RESET	0x10U
#define TEGRABL_USBMSD_BAD_SIG		0x11U
#define TEGRABL_USBMSD_BAD_TAG		0x12U
#define TEGRABL_USBMSD_BAD_STATUS	0x13U
#define TEGRABL_USBMSD_PHASE_ERR	0x14U
#define TEGRABL_USBMSD_BAD_CMD		0x15U
#define TEGRABL_USBMSD_RESET_RECOVERY	0x16U
#endif
