/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#ifndef __XHCI_H
#define __XHCI_H

#include <usbh_protocol.h>
#include <tegrabl_usbh.h>
#include <xhci_priv.h>

#define NUM_TRB_EVENT_RING  2048
#define NUM_TRB_CMD_RING    2048
#define NUM_TRB_TX_RING     2048

#define DCBAA_SIZE 2048
#define DEV_CONTEXT_SIZE 2048
#define INPUT_CONTEXT_SIZE (2048 + 64)
#define EP_CONTEXT_SIZE 64
#define ER_SEGMENT_SIZE (64 * 1024)
#define CR_SEGMENT_SIZE (64 * 1024)
#define TR_SEGMENT_SIZE (64 * 1024)
#define ERST_SIZE 2048
#define SETUP_DATA_BUFFER_SIZE 0x200

tegrabl_error_t xhci_controller_init(struct xusb_host_context *context);

tegrabl_error_t xhci_start(struct xusb_host_context *ctx);

tegrabl_error_t init_data_struct(struct xusb_host_context *context);

tegrabl_error_t tegrabl_xusbh_process_ctrl_req(struct xusb_host_context *ctx,
											   struct device_request *device_request_ptr,
											   void *buffer);

tegrabl_error_t tegrabl_xhci_xfer_data(struct xusb_host_context *ctx, uint8_t ep_id, void *buffer,
									   uint32_t *length);
#endif
