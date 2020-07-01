/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_XUSB_ERR_AUX
#define INCLUDED_TEGRABL_XUSB_ERR_AUX

#include <stdint.h>

#define AUX_INFO_INIT_TRANSFER_RING			0x01U
#define AUX_INFO_STALL_EP				0X02U
#define AUX_INFO_DISABLE_EP				0x03U
#define AUX_INFO_QUEUE_TRB				0x04U
#define AUX_INFO_INIT_EPCONTEXT				0x05U
#define AUX_INFO_INITEP					0x06U
#define AUX_INFO_POLL_FIELD				0x07U
#define AUX_INFO_HANDLE_SETUPPKT			0x08U
#define AUX_INFO_USBF_PRIV_INIT_1			0x09U
#define AUX_INFO_USBF_PRIV_INIT_2			0x0aU
#define AUX_INFO_POLL_FOR_EVENT				0x0bU
#define AUX_INFO_HANDLE_PORT_STATUS			0x0cU
#define AUX_INFO_HANDLE_TXFER_EVENT_1			0x0dU
#define AUX_INFO_HANDLE_TXFER_EVENT_2			0x0eU
#define AUX_INFO_USBF_RECEIVE				0x0fU
#define AUX_INFO_USBF_TRANSMIT				0x10U
#define AUX_INFO_USBF_RECEIVE_START			0x11U
#define AUX_INFO_USBF_RECEIVE_COMPLETE			0x12U
#define AUX_INFO_USBF_TRANSMIT_START_1			0x13U
#define AUX_INFO_USBF_TRANSMIT_START_2			0x14U
#define AUX_INFO_USBF_REGULATOR_INIT_1			0x15U
#define AUX_INFO_USBF_REGULATOR_INIT_2			0x16U

#endif

