/* Copyright (c) 2017 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <tegrabl_io.h>
#include <tegrabl_error.h>
#include <tegrabl_usbh.h>
#include <tegrabl_dmamap.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>

struct usb_scsi_cmd {
	uint32_t	signature;
	uint32_t	tag;
	uint32_t	data_size;
	uint8_t		flags;
	uint8_t		lun;
	uint8_t		len;
	uint8_t		cmd[16];
};

/* sample code to access usb storage device */
tegrabl_error_t tegrabl_xusbh_test_sample(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t transfer_len;
	uint8_t dev_id;
	uint8_t *in_buf;
	uint8_t *out_buf;
	struct usb_scsi_cmd *cmd;
	int i;
	char *pp;
	char c;

	/* Sandisk U3 Cruzer USB stick */
//	dev_id = tegrabl_usbh_get_device_id(0x0781, 0x5406);
	dev_id = tegrabl_usbh_get_device_id(0x1221, 0x3234);
	if (dev_id == 0) {
		pr_info("Device is not found!\n");
		return TEGRABL_ERR_NOT_FOUND;
	}

	in_buf  = (uint8_t *)tegrabl_alloc_align(TEGRABL_HEAP_DMA, 8, 256);
	if (in_buf == NULL) {
		pr_info("No enough memory\n");
		return TEGRABL_ERR_NO_MEMORY;
	}
	out_buf = (uint8_t *)tegrabl_alloc_align(TEGRABL_HEAP_DMA, 8, 256);
	if (out_buf == NULL) {
		pr_info("No enough memory\n");
		return TEGRABL_ERR_NO_MEMORY;
	}

	/* Send scsi command to usn device */
	memset(in_buf, 0, 256);
	memset(out_buf, 0, 256);
	cmd = (struct usb_scsi_cmd *)out_buf;
	cmd->signature = 0x43425355;	/* 'USBC' */
	cmd->tag = 1;
	cmd->data_size = 36;
	cmd->flags = 0x80;
	cmd->lun = 0;
	cmd->len = 6;
	cmd->cmd[0] = 0x12;		/* INQUIRY */
	cmd->cmd[4] = 36;
	transfer_len = 31;
	err = tegrabl_usbh_snd_data(dev_id, out_buf, &transfer_len);
	if (err != TEGRABL_NO_ERROR)
		return TEGRABL_ERR_NOT_SUPPORTED;

	cmd = (struct usb_scsi_cmd *)in_buf;
	transfer_len = 36;
	err = tegrabl_usbh_rcv_data(dev_id, in_buf, &transfer_len);
	if (err != TEGRABL_NO_ERROR)
		return TEGRABL_ERR_NOT_SUPPORTED;

	pr_info("==== INQUIRY input buffer ====\n");
	pp = (char *)in_buf;
	for (i = 0; i < 256; i += 16) {
		pr_info("%02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x  "
			"%02x  %02x  %02x  %02x  %02x  %02x  %02x  %02x\n",
				pp[i+0],  pp[i+1],  pp[i+2],  pp[i+3],
				pp[i+4],  pp[i+5],  pp[i+6],  pp[i+7],
				pp[i+8],  pp[i+9],  pp[i+10], pp[i+11],
				pp[i+12], pp[i+13], pp[i+14], pp[i+15]);
	}
	c = pp[16];
	pp[16] = 0;
	pr_info("Vendor Identification  :  %s\n", pp + 8);
	pp[16] = c;
	c = pp[32];
	pp[32] = 0;
	pr_info("Product Identification  :  %s\n", pp + 16);
	pp[32] = c;
	pr_info("Product Revision Level  :  %s\n", pp + 32);

	return err;
}

