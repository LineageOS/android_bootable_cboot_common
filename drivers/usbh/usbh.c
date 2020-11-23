/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_USBH

#include <string.h>
#include <stdint.h>
#include <tegrabl_clock.h>
#include <tegrabl_timer.h>
#include <tegrabl_io.h>
#include <tegrabl_drf.h>
#include <tegrabl_addressmap.h>
#include <usbh_protocol.h>
#include <tegrabl_usbh.h>
#include <xhci.h>
#include <tegrabl_malloc.h>
#include <tegrabl_debug.h>

static struct xusb_host_context *g_context;

struct xusb_host_context *tegrabl_get_usbh_context(void)
{
	return g_context;
}

tegrabl_error_t tegrabl_usbh_init(void)
{
	struct xusb_host_context *context = NULL;
	tegrabl_error_t e = TEGRABL_NO_ERROR;

	/* Allocate and initialize usbh context */
	context = (struct xusb_host_context *)tegrabl_alloc(TEGRABL_HEAP_DEFAULT,
														sizeof(struct xusb_host_context));
	if (context == NULL) {
		pr_error("failed to allocate memory for usbh context\n");
		e = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	memset(context, 0x0, sizeof(struct xusb_host_context));
	g_context = context;

	/* Initialize xhci */
	e = xhci_controller_init(context);
	if (e != TEGRABL_NO_ERROR) {
		pr_error("failed to initialize xhci controller\n");
		goto fail;
	}

	e = xhci_start(context);
	if (e != TEGRABL_NO_ERROR) {
		pr_error("failed to start xhci controller\n");
		goto fail;
	}

fail:
	return e;
}

uint8_t tegrabl_usbh_get_device_id(uint16_t vendor_id, uint16_t product_id)
{
	struct xusb_host_context *ctx = tegrabl_get_usbh_context();

	if (ctx->curr_dev_priv->enum_dev.dev_addr == 0) {
		return 0;
	}

	if ((ctx->curr_dev_priv->enum_dev.vendor_id == vendor_id) &&
		(ctx->curr_dev_priv->enum_dev.product_id == product_id)) {
		return ctx->curr_dev_priv->enum_dev.dev_addr;
	}

	return 0;
}

tegrabl_error_t tegrabl_usbh_snd_command(struct device_request request, void *buffer)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct xusb_host_context *ctx = tegrabl_get_usbh_context();

	err = tegrabl_xusbh_process_ctrl_req(ctx, &request, buffer);
	if (err == TEGRABL_NO_ERROR) {
		if (request.bmRequestTypeUnion.stbitfield.dataTransferDirection == DEV2HOST) {
			tegrabl_dma_unmap_buffer(TEGRABL_MODULE_XUSB_HOST, 0, buffer, request.wLength,
									 TEGRABL_DMA_FROM_DEVICE);
		}
	}

	return err;
}

#define MAX_TRANSFER_SIZE (254 * 65536)
tegrabl_error_t tegrabl_usbh_snd_data(uint8_t dev_id, void *buffer, uint32_t *length)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct xusb_host_context *ctx = tegrabl_get_usbh_context();
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t transfered_total = 0;
	uint32_t transfered = 0;

	/* xHC reserved 255(normal) + 1(link) trbs for transfer EPs */
	/* So the max. size of each transfer is 254 * 64K bytes     */
	/* if the buffer is not 64K aligned                         */
	i = *length;
	while (i > 0) {
		transfered = (i >= MAX_TRANSFER_SIZE) ? MAX_TRANSFER_SIZE : i;
		err = tegrabl_xhci_xfer_data(ctx, ctx->curr_dev_priv->enum_dev.ep[0].addr, buffer + j, &transfered);
		transfered_total += transfered;
		if (err != TEGRABL_NO_ERROR) {
			*length = transfered_total;
			return err;
		}
		i -= transfered;
		j += transfered;
	}
	*length = transfered_total;
	return err;
}

tegrabl_error_t tegrabl_usbh_rcv_data(uint8_t dev_id, void *buffer, uint32_t *length)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct xusb_host_context *ctx = tegrabl_get_usbh_context();
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t transfered_total = 0;
	uint32_t transfered = 0;

	/* xHC reserved 255(normal) + 1(link) trbs for transfer EPs */
	/* So the max. size of each transfer is 255 * 64K bytes     */
	i = *length;
	while (i > 0) {
		transfered = (i >= MAX_TRANSFER_SIZE) ? MAX_TRANSFER_SIZE : i;
		err = tegrabl_xhci_xfer_data(ctx, (ctx->curr_dev_priv->enum_dev.ep[1].addr | 0x80), buffer + j,
									 &transfered);
		transfered_total += transfered;
		tegrabl_dma_unmap_buffer(TEGRABL_MODULE_XUSB_HOST, 0, buffer + j, transfered,
								 TEGRABL_DMA_FROM_DEVICE);
		if (err != TEGRABL_NO_ERROR) {
			*length = transfered_total;
			return err;
		}
		i -= transfered;
		j += transfered;
	}
	*length = transfered_total;
	return err;
}
