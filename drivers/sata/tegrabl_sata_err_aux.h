/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SATA_ERR_AUX_H
#define TEGRABL_SATA_ERR_AUX_H

#include <stdint.h>

#define TEGRABL_SATA_BDEV_IOCTL 0x1U
#define TEGRABL_SATA_BDEV_XFER_WAIT_1 0x2U
#define TEGRABL_SATA_BDEV_XFER_1 0x3U
#define TEGRABL_SATA_BDEV_READ_BLOCK 0x4U
#define TEGRABL_SATA_BDEV_WRITE_BLOCK 0x5U
#define TEGRABL_SATA_BDEV_ERASE 0x6U
#define TEGRABL_SATA_BDEV_OPEN 0x7U
#define TEGRABL_SATA_REGISTER_REGION 0x8U
#define TEGRABL_SATA_START_COMMAND_1 0x9U
#define TEGRABL_SATA_START_COMMAND_2 0xAU
#define TEGRABL_SATA_XFER_COMPLETE_1 0xBU
#define TEGRABL_SATA_XFER_COMPLETE_2 0xCU
#define TEGRABL_SATA_AHCI_XFER_1 0xDU
#define TEGRABL_SATA_AHCI_XFER_2 0xEU
#define TEGRABL_SATA_AHCI_XFER_3 0xFU
#define TEGRABL_SATA_AHCI_ERASE 0x10U
#define TEGRABL_SATA_AHCI_FLUSH_DEVICE_1 0x11U
#define TEGRABL_SATA_AHCI_FLUSH_DEVICE_2 0x12U
#define TEGRABL_SATA_AHCI_IDENTIFY_DEVICE_1 0x13U
#define TEGRABL_SATA_AHCI_IDENTIFY_DEVICE_2 0x14U
#define TEGRABL_SATA_AHCI_IDENTIFY_DEVICE_3 0x15U
#define TEGRABL_SATA_AHCI_INIT_CMD_LIST_RECEIVE_FIS_BUFFERS_1 0x16U
#define TEGRABL_SATA_AHCI_INIT_CMD_LIST_RECEIVE_FIS_BUFFERS_2 0x17U
#define TEGRABL_SATA_AHCI_HOST_RESET_1 0x18U
#define TEGRABL_SATA_AHCI_HOST_RESET_2 0x19U
#define TEGRABL_SATA_AHCI_INIT_MEMORY_REGIONS_1 0x1AU
#define TEGRABL_SATA_AHCI_INIT_MEMORY_REGIONS_2 0x1BU
#define TEGRABL_SATA_AHCI_INIT_MEMORY_REGIONS_3 0x1CU
#define TEGRABL_SATA_AHCI_INIT_MEMORY_REGIONS_4 0x1DU
#define TEGRABL_SATA_AHCI_SKIP_INIT_1 0x1EU
#define TEGRABL_SATA_AHCI_SKIP_INIT_2 0x1FU
#define TEGRABL_SATA_BDEV_XFER_WAIT_2 0x20U
#define TEGRABL_SATA_BDEV_XFER_2 0x21U
#endif
