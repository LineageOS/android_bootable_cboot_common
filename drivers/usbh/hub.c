/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_USBH

#include <stdint.h>
#include <stdio.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_timer.h>
#include <usbh_protocol.h>
#include <tegrabl_usbh.h>
#include <xhci.h>
#include <hub.h>

#define HUB_DEVICE_DETECT_TIMEOUT_MS		1000
#define HUB_PORT_RESET_TIMEOUT_MS			200

static tegrabl_error_t send_dev_req(struct xusb_host_context *ctx, uint8_t req_type, uint8_t req,
									uint16_t val, uint16_t index, uint16_t len)
{
	struct device_request device_request_var = {{0}};
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (ctx == NULL) {
		pr_error("%s(): Invalid param\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	pr_trace("Send command\n");
	device_request_var.bmRequestTypeUnion.bmRequestType = req_type;
	device_request_var.bRequest = req;
	device_request_var.wValue = val;
	device_request_var.wIndex = index;
	device_request_var.wLength = len;
	err = tegrabl_xusbh_process_ctrl_req(ctx, &device_request_var, NULL);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	tegrabl_dma_unmap_buffer(TEGRABL_MODULE_XUSB_HOST, 0, ctx->xusb_data, SETUP_DATA_BUFFER_SIZE,
							 TEGRABL_DMA_FROM_DEVICE);

fail:
	return err;
}

static tegrabl_error_t hub_get_port_status(struct xusb_host_context *ctx,
										   uint32_t port_no,
										   uint16_t *port_status,
										   uint16_t *port_status_change)
{
	uint32_t status = 0;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((port_no == 0) || (port_status == NULL) || (port_status_change == NULL)) {
		pr_error("%s(): Invalid params\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	err = send_dev_req(ctx, DEV2HOST_CLASS_OTHER, GET_STATUS, 0, port_no, HUB_PORT_STATUS_DATA_LEN);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get port %u status\n", port_no);
		goto fail;
	}
	status = *((uint32_t *)ctx->xusb_data);
	*port_status = HUB_GET_PORT_STATUS(status);
	*port_status_change = HUB_GET_PORT_CHANGE(status);
	pr_trace("Port %u: status: 0x%04x, status change: 0x%04x\n",
			 port_no, *port_status, *port_status_change);

fail:
	return err;
}

tegrabl_error_t hub_init(struct xusb_host_context *ctx)
{
	uint8_t over_curr_protection_mode;
	char *over_curr_protection_mode_str = NULL;
	uint32_t port_no;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	/* Get hub descriptor */
	err = send_dev_req(ctx, DEV2HOST_CLASS_DEVICE, GET_DESCRIPTOR, (USB_DT_HUB << 8), 0,
					   USB_HUB_DESCRIPTOR_SIZE);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get hub descriptor\n");
		goto fail;
	}
	ctx->hub_num_ports = ((struct usb_hub_desc *)(ctx->xusb_data))->bNbrPorts;
	pr_info("Hub:\n");
	pr_info("\tDownstream ports        : %u\n", ctx->hub_num_ports);
	pr_info("\tPower switching         : %s\n",
			HUB_GET_POWER_SWITCHING_MODE(((struct usb_hub_desc *)(ctx->xusb_data))->wHubCharacteristics) ?
				"Individual" : "Ganged");
	pr_info("\tCompound device         : %s\n",
			HUB_IDENTIFY_COMP_DEV(((struct usb_hub_desc *)(ctx->xusb_data))->wHubCharacteristics) ?
				"Yes" : "No");
	over_curr_protection_mode =
		HUB_GET_OVER_CURR_PROTECTION_MODE(((struct usb_hub_desc *)(ctx->xusb_data))->wHubCharacteristics);
	switch (over_curr_protection_mode) {
	case 0:
		over_curr_protection_mode_str = "Global";
		break;
	case 1:
		over_curr_protection_mode_str = "Individual Port";
		break;
	default:
		over_curr_protection_mode_str = "None";
		break;
	}
	pr_info("\tOver-current protection : %s\n", over_curr_protection_mode_str);
	pr_info("\tHub avg delay           : %u ns\n", ((struct usb_hub_desc *)(ctx->xusb_data))->wHubDelay);

	if (ctx->curr_dev_priv->enum_dev.bDeviceProtocol == USB_HUB_HS_MULTI_TT) {
		pr_info("Set interface = 1 (alt setting)\n");
		err = send_dev_req(ctx, HOST2DEV_INTERFACE, SET_INTERFACE, 1, 0, 0);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Failed to set interface\n");
			goto fail;
		}
	}

	pr_info("Enable hub ports\n");
	for (port_no = 1; port_no <= ctx->hub_num_ports; port_no++) {
		pr_trace("Set power feature for port %u\n", port_no);
		err = send_dev_req(ctx, HOST2DEV_CLASS_OTHER, SET_FEATURE, HF_PORT_POWER, port_no, 0);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Failed to set power feature for port %u\n", port_no);
			goto fail;
		}
	}

fail:
	return err;
}

tegrabl_error_t hub_detect_device(struct xusb_host_context *ctx, uint32_t *port_device_bitmap)
{
	uint16_t port_status;
	uint16_t port_status_change;
	uint32_t port_no;
	uint32_t port_dev_bmap = 0;
	time_t elapsed_time_ms;
	time_t start_time_ms;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (port_device_bitmap == NULL) {
		pr_error("%s(): Invalid param\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	elapsed_time_ms = 0;
	start_time_ms = tegrabl_get_timestamp_ms();

	/* Check what all ports has device connected */
	while (elapsed_time_ms < HUB_DEVICE_DETECT_TIMEOUT_MS) {

		for (port_no = 1; port_no <= ctx->hub_num_ports; port_no++) {
			if (((0x1 << port_no) & port_dev_bmap) != 0) {
				/* Try next port if this port is already registered having device connected */
				continue;
			}
			err = hub_get_port_status(ctx, port_no, &port_status, &port_status_change);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
			if ((port_status & HUB_PORT_CONN_MASK) && (port_status_change & HUB_PORT_CONN_CHANGE_MASK)) {
				pr_info("Device detected on port %u\n", port_no);
				port_dev_bmap = port_dev_bmap | (0x1 << port_no);
			}
		}
		tegrabl_mdelay(200);
		elapsed_time_ms = tegrabl_get_timestamp_ms() - start_time_ms;
		pr_trace("elapsed time: %lu\n\n", elapsed_time_ms);
	}
	if (port_dev_bmap == 0) {
		pr_error("No device found on any of the hub ports\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

fail:
	if (port_device_bitmap != NULL) {
		*port_device_bitmap = port_dev_bmap;
	}
	return err;
}

tegrabl_error_t hub_reset_port(struct xusb_host_context *ctx, uint32_t port_no, uint32_t *dev_speed)
{
	uint16_t port_status;
	uint16_t port_status_change;
	uint32_t speed;
	time_t elapsed_time_ms;
	time_t start_time_ms;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((port_no == 0) || (dev_speed == NULL)) {
		pr_error("%s(): Invalid params\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Get port status */
	err = hub_get_port_status(ctx, port_no, &port_status, &port_status_change);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	if (((port_status & HUB_PORT_CONN_MASK) == 0) || ((port_status & HUB_PORT_CONN_CHANGE_MASK) == 0)) {
		pr_info("No device connected, skip port %u reset\n", port_no);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	pr_trace("Clear port %u conn status change bit\n", port_no);
	err = send_dev_req(ctx, HOST2DEV_CLASS_OTHER, CLEAR_FEATURE, HF_C_PORT_CONNECTION, port_no, 0);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_info("Reset port %u\n", port_no);
	err = send_dev_req(ctx, HOST2DEV_CLASS_OTHER, SET_FEATURE, HF_PORT_RESET, port_no, 0);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to set port reset feature\n");
		goto fail;
	}

	/* Wait till reset completes */
	elapsed_time_ms = 0;
	start_time_ms = tegrabl_get_timestamp_ms();
	while (elapsed_time_ms < HUB_PORT_RESET_TIMEOUT_MS) {
		err = hub_get_port_status(ctx, port_no, &port_status, &port_status_change);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
		if ((port_status & HUB_PORT_CONN_MASK) && (port_status_change & HUB_PORT_RESET_CHANGE_MASK)) {
			/* Port reset completed */
			break;
		}
		tegrabl_mdelay(50);
		elapsed_time_ms = tegrabl_get_timestamp_ms() - start_time_ms;
		pr_trace("elapsed time: %lu\n", elapsed_time_ms);
	}
	if (elapsed_time_ms > HUB_PORT_RESET_TIMEOUT_MS) {
		pr_error("Port %u is not reset\n", port_no);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Extract speed and convert into xHCI code */
	speed = HUB_PORT_GET_SPEED(port_status);
	switch (speed) {
	case USB_HIGH_SPEED:
		*dev_speed = XHCI_HIGH_SPEED;
		break;
	case USB_LOW_SPEED:
		*dev_speed = XHCI_LOW_SPEED;
		break;
	case USB_FULL_SPEED:
		*dev_speed = XHCI_FULL_SPEED;
		break;
	default:
		*dev_speed = XHCI_SUPER_SPEED;
		break;
	}

	/* Clear port reset change bit */
	err = send_dev_req(ctx, HOST2DEV_CLASS_OTHER, CLEAR_FEATURE, HF_C_PORT_RESET, port_no, 0);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to clear port reset feature\n");
		goto fail;
	}

fail:
	return err;
}
