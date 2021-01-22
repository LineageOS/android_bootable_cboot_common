/*
 * Copyright (c) 2018-2020, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_USBMSD

/* #define USB_DEBUG */		/* Uncomment for CBW/CSW/data dumps */

#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <tegrabl_timer.h>
#include <tegrabl_utils.h>
#include <tegrabl_usbmsd.h>
#include <tegrabl_usbh.h>
#include <xhci_priv.h>
#include <tegrabl_usbmsd_bdev.h>
#include <tegrabl_usbmsd_err_aux.h>
#include <tegrabl_usbmsd_scsi.h>
#ifdef USB_DEBUG
#include <printf.h>
#endif

static bool init_done;

#ifdef	USB_DEBUG
static void dump_cbw(usb_msd_cbw_t *cbw)
{
	uint8_t clen = cbw->Length;
	uint32_t dlen = cbw->DataTransferLength;
	uint8_t *c = cbw->CDB;
	uint32_t tag = cbw->Tag;
	uint8_t flags = cbw->Flags;
	uint32_t sig = cbw->Signature;
	uint8_t lun = cbw->LUN;

	pr_info("%s: CBW %u: sig = 0x%08X, cmdlen = %d bytes, LUN = %d\n"
		"CDB: 0x%02x %02x %02x %02x "
		"%02x %02x %02x %02x %02x %02x %02X %02X %s, "
		"dataxferlen = %db, dir = %s\n",
		__func__, tag, sig, clen, lun,
		c[0], c[1], c[2], c[3],	c[4], c[5], c[6], c[7],
		c[8], c[9], c[10], c[11], (clen > 12 ? "..." : ""),
		dlen, (flags == CBW_IN_FLAG ? "in" :
		       (flags == CBW_OUT_FLAG ? "out" : "<invalid>")));
}

static void dump_csw(usb_msd_csw_t *csw)
{
	uint32_t sig = csw->Signature;
	uint32_t tag = csw->Tag;
	uint32_t res = csw->DataResidue;
	uint8_t status = csw->Status;

	pr_info("%s: CSW %u: sig = 0x%08x (%s), tag = %d,\n"
		"residue = %d bytes, status = 0x%02x (%s)\n", __func__,
		tag, sig, (sig == CSW_SIGNATURE ? "valid" : "invalid"),
		tag, res, status,
		(status == CSW_PASS_STAT ? "good" :
		 (status == CSW_FAIL_STAT ? "failed" :
		 (status == CSW_PHASE_STAT ? "phase" : "<invalid>"))));
}

void dump_buf(const void *data, size_t size)
{
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	pr_debug("--- Buffer ---\n");
	printf(" ");
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char *)data)[i]);
		if (((unsigned char *)data)[i] >= ' ' && ((unsigned char *)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char *)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("| %s\n", ascii);
				printf(" ");
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("| %s\n", ascii);
				printf(" ");
			}
		}
	}
	printf("\n");
}

#endif	/* USB_DEBUG */

/**
 * @brief Processes ioctl request.
 *
 * @param dev Block dev device registered during 'open'
 * @param ioctl Ioctl number
 * @param argp Arguments for IOCTL call which might be updated or used
 * based on ioctl request.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_usbmsd_bdev_ioctl(struct tegrabl_bdev *dev,
						 uint32_t ioctl, void *argp)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_usbmsd_context *context = NULL;

	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(ioctl);
	TEGRABL_UNUSED(argp);
	TEGRABL_UNUSED(context);

	if (dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				      TEGRABL_USBMSD_BDEV_IOCTL);
		TEGRABL_SET_ERROR_STRING(error, "dev: %p", dev);
		goto fail;
	}

	context = (struct tegrabl_usbmsd_context *)dev->priv_data;
	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID,
				      TEGRABL_USBMSD_BDEV_IOCTL);
		TEGRABL_SET_ERROR_STRING(error, "context: %p", context);
		goto fail;
	}

	/* Add switch/case here to hand IOCTLs. Currently none for USB MSD */
	error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED,
			      TEGRABL_USBMSD_BDEV_IOCTL);
	TEGRABL_SET_ERROR_STRING(error, "ioctl %d", ioctl);

fail:
	if (error != TEGRABL_NO_ERROR)
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_COMMAND_FAILED,
					   "ioctl %d", ioctl);

	return error;
}

/** @brief Executes a read/write transaction based on the given
 *	transaction details. Transfer info block numbers to read/write.
 *	Called by the blockdev driver for the layer above.
 *
 *  @param xfer transfer info
 *
 *  @return Returns transfer status
 */
static tegrabl_error_t tegrabl_usbmsd_bdev_xfer(
					struct tegrabl_blockdev_xfer_info *xfer)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_bdev *dev = NULL;
	struct tegrabl_usbmsd_context *context = NULL;
	uint32_t xfer_length, bulk_count = 0;
	bnum_t block = 0;
	bnum_t count = 0;
	bool is_write;

	if (xfer == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				      TEGRABL_USBMSD_BDEV_XFER_1);
		TEGRABL_SET_ERROR_STRING(error, "xfer: %p", xfer);
		goto fail;
	}

	if (xfer->dev == NULL || xfer->buf == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID,
				      TEGRABL_USBMSD_BDEV_XFER_1);
		TEGRABL_SET_ERROR_STRING(error, "dev: %p, buf: %p",
					 xfer, xfer->dev, xfer->buf);
		goto fail;
	}

	dev = xfer->dev;
	context = (struct tegrabl_usbmsd_context *)dev->priv_data;
	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID,
				      TEGRABL_USBMSD_BDEV_XFER_2);
		TEGRABL_SET_ERROR_STRING(error, "context: %p", context);
		goto fail;
	}

	block = xfer->start_block;
	count = xfer->block_count;
	/* Check if we've been asked to read past the end of the device */
	if ((block + (uint64_t)count) > (uint64_t)context->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW,
				      TEGRABL_USBMSD_BDEV_XFER_1);
		TEGRABL_SET_ERROR_STRING(error, "req end block %"PRIu64,
					 "> last block %"PRIu64,
					 block + (uint64_t)count,
					 context->block_count);
		goto fail;
	}

	bulk_count = MIN(count, USBMSD_MAX_READ_WRITE_SECTORS);
	if (xfer->xfer_type == TEGRABL_BLOCKDEV_READ)
		is_write = false;
	else
		is_write = true;

	xfer_length = (bulk_count << context->block_size_log2);
	error = tegrabl_usbmsd_io(context, xfer->buf, xfer_length,
				  TEGRABL_USBMSD_READ_TIMEOUT);

	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_XFER_FAILED,
				"%s of %"PRIu32" blocks from block %"PRIu32,
				(is_write ? "write" : "read"), bulk_count,
				 block);
		goto fail;
	}

	context->xfer_info.bdev_xfer_info = xfer;
	context->xfer_info.bulk_count = bulk_count;
	context->xfer_info.is_write = is_write;

fail:
	if (error != TEGRABL_NO_ERROR)
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_XFER_FAILED, "usbmsd");

	return error;
}

/**
 * @brief Reads number of blocks from specified device into buffer
 *
 * @param dev Block device from which to read
 * @param buf Buffer for saving read content
 * @param block Start block from which read to start
 * @param count Number of blocks to read
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error..
 */
static tegrabl_error_t tegrabl_usbmsd_bdev_read_block(struct tegrabl_bdev *dev,
		 void *buffer, bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t xfer_length, bulk_count = 0;
	struct tegrabl_usbmsd_context *context = NULL;
	uint8_t *buf = buffer;
	bnum_t start_block = block;
	bnum_t total_blocks = count;

	pr_debug("%s: entry\n", __func__);
	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				      TEGRABL_USBMSD_BDEV_READ_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "dev: %p, buffer: %p",
					 dev, buffer);
		goto fail;
	}

	xfer_length = count * TEGRABL_BLOCKDEV_BLOCK_SIZE(dev);
	if ((xfer_length > SZ_64K) && (IS_ALIGNED((uintptr_t)buffer, SZ_64K) == false)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_ALIGNED, 0);
		TEGRABL_SET_ERROR_STRING(error, "buffer: %p", "64KB", buffer);
		goto fail;
	}

	context = (struct tegrabl_usbmsd_context *)dev->priv_data;
	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID,
				      TEGRABL_USBMSD_BDEV_READ_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "context: %p", context);
		goto fail;
	}

	/* Check if we've been asked to read past the end of the device */
	if ((block + (uint64_t)count) > context->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW,
				      TEGRABL_USBMSD_BDEV_READ_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "req end block %"PRIu64,
					 "> last block %"PRIu64,
					 block + (uint64_t)count,
					 context->block_count);
		goto fail;
	}

	/* Set up SCSI CDB in here for READ_10 */
	context->cmdlen = 10;
	memset(context->cmd, 0x0, context->cmdlen);
	context->cmd[0] = READ_10;
	context->xfer_info.is_write = 0;	/* data comes from device */

	pr_debug("start block = %d, count = %d\n", block, count);
	while (count != 0U) {
		bulk_count = MIN(count, USBMSD_MAX_READ_WRITE_SECTORS);

		pr_debug("%s: bulk count = %d\n", __func__, bulk_count);
		/*
		 * Find a cleaner way of storing block/count in the CDB
		 * NOTE: Assumes 32-bit LBA and 16-bit count, max!
		 */
		context->cmd[2] = ((uint8_t)(block >> 24)) & 0xFF;
		context->cmd[3] = ((uint8_t)(block >> 16)) & 0xFF;
		context->cmd[4] = ((uint8_t)(block >> 8)) & 0xFF;
		context->cmd[5] = ((uint8_t)(block)) & 0xFF;
		context->cmd[7] = ((uint8_t)(bulk_count >> 8)) & 0xFF;
		context->cmd[8] = ((uint8_t)(bulk_count)) & 0xFF;

		xfer_length = (bulk_count << context->block_size_log2);
		pr_debug("%s: xfer_length = %d\n", __func__, xfer_length);
		pr_debug("%s: calling USBMSD_IO ...\n", __func__);
		error = tegrabl_usbmsd_io(context, (void *)buf, xfer_length,
					  TEGRABL_USBMSD_READ_TIMEOUT*2);

		if (error != TEGRABL_NO_ERROR) {
			pr_error("Error reading %d blocks @ %d\n",
				 bulk_count, block);
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_READ_FAILED,
					"sector %"PRIu32" count %"PRIu32,
					block, bulk_count);
			goto fail;
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		block += bulk_count;
		pr_debug("%s: read successful, now block = %d, count = %d\n",
			 __func__, block, count);
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_READ_FAILED,
					   "sector %"PRIu32" count %"PRIu32,
					   start_block, total_blocks);
	}

	pr_debug("%s: Exiting with error code 0x%X\n", __func__, error);
	return error;
}

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
/**
 * @brief Writes number of blocks to device with contents from buffer
 *
 * @param dev Block device in which to write
 * @param buf Buffer containing data to be written
 * @param block Start block from which write should start
 * @param count Number of blocks to write
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_usbmsd_bdev_write_block(struct tegrabl_bdev *dev,
			 const void *buffer, bnum_t block, bnum_t count)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t xfer_length, bulk_count = 0;
	struct tegrabl_usbmsd_context *context = NULL;
	const uint8_t *buf = buffer;
	bnum_t start_block = block;
	bnum_t total_blocks = count;

	if ((dev == NULL) || (buffer == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				TEGRABL_USBMSD_BDEV_WRITE_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "dev: %p, buffer: %p",
					 dev, buffer);
		goto fail;
	}

	context = (struct tegrabl_usbmsd_context *)dev->priv_data;
	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID,
				      TEGRABL_USBMSD_BDEV_WRITE_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "context: %p", context);
		goto fail;
	}

	/* Check if we've been asked to read past the end of the device */
	if ((block + (uint64_t)count) > context->block_count) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW,
				      TEGRABL_USBMSD_BDEV_WRITE_BLOCK);
		TEGRABL_SET_ERROR_STRING(error, "req end block %"PRIu64,
					 "> last block %"PRIu64,
					 block + (uint64_t)count,
					 context->block_count);
		goto fail;
	}

	/* Set up SCSI CDB in here for WRITE_10 */
	context->cmdlen = 10;
	memset(context->cmd, 0x0, context->cmdlen);
	context->cmd[0] = WRITE_10;
	context->xfer_info.is_write = 1;	/* data comes from host */

	pr_debug("start block = %d, count = %d\n", block, count);
	while (count > 0UL) {
		bulk_count = MIN(count, USBMSD_MAX_READ_WRITE_SECTORS);

		/*
		 * Find a cleaner way of storing block/count in the CDB
		 * NOTE: Assumes 32-bit LBA and 16-bit count, max!
		 */
		context->cmd[2] = ((uint8_t)(block >> 24)) & 0xFF;
		context->cmd[3] = ((uint8_t)(block >> 16)) & 0xFF;
		context->cmd[4] = ((uint8_t)(block >> 8)) & 0xFF;
		context->cmd[5] = ((uint8_t)(block)) & 0xFF;
		context->cmd[7] = ((uint8_t)(bulk_count >> 8)) & 0xFF;
		context->cmd[8] = ((uint8_t)(bulk_count)) & 0xFF;

		xfer_length = (bulk_count << context->block_size_log2);
		error = tegrabl_usbmsd_io(context, (void *)buf, xfer_length,
					  TEGRABL_USBMSD_READ_TIMEOUT);

		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_WRITE_FAILED,
					"sector %"PRIu32" count %"PRIu32,
					block, bulk_count);
			goto fail;
		}

		count -= bulk_count;
		buf += (bulk_count << context->block_size_log2);
		block += bulk_count;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_READ_FAILED,
					   "sector %"PRIu32" count %"PRIu32,
					   start_block, total_blocks);
	}

	return error;
}

/**
 * @brief Erases specified number of blocks from specified device
 *
 * @param dev Block device which is to be erased
 * @param block Start block from which erase should start
 * @param count Number of blocks to be erased
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error..
 */
static tegrabl_error_t tegrabl_usbmsd_bdev_erase(struct tegrabl_bdev *dev,
				 bnum_t block, bnum_t count, bool is_secure)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	TEGRABL_UNUSED(dev);
	TEGRABL_UNUSED(block);
	TEGRABL_UNUSED(count);
	TEGRABL_UNUSED(is_secure);

	/* NOTE: Erase not implemented, use bdev_write w/zeroes? */
	error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED,
			    TEGRABL_USBMSD_BDEV_ERASE);
	TEGRABL_SET_ERROR_STRING(error, "erase");
	return error;
}
#endif	/* !CONFIG_ENABLE_BLOCKDEV_BASIC */

/**
 * @brief Closes the block device instance, frees any buffers
 *
 * @param dev Block device which is to be closed.
 */
static tegrabl_error_t tegrabl_usbmsd_bdev_close(struct tegrabl_bdev *dev)
{
	struct tegrabl_usbmsd_context *context = NULL;

	if (dev != NULL) {
		if (dev->priv_data != NULL) {
			/*
			 * context was allocated during 'bdev_open', and
			 *  stored in priv_data during 'register'.
			 */
			context =
				(struct tegrabl_usbmsd_context *)dev->priv_data;
			if (context->instance) {
				/* free any buffers/context here */
				pr_debug("%s: Freeing context\n", __func__);
				tegrabl_free(dev->priv_data);
			}
		}
		init_done = false;
	}

	return TEGRABL_NO_ERROR;
}

/**
 * @brief Fills member of context structure with default values
 *
 * @param context Context of USBMSD
 * @param instance USBMSD controller instance
 */
static void tegrabl_usbmsd_set_default_context(
		struct tegrabl_usbmsd_context *context, uint32_t instance)
{
	pr_debug("Default Context\n");
	context->instance = instance;
	context->block_size_log2 = TEGRABL_USBMSD_SECTOR_SIZE_LOG2;
}

/**
 * @brief Fills in the device context with default values
 *
 * @param context Context of USBMSD
 */
static tegrabl_error_t tegrabl_usbmsd_register(
					struct tegrabl_usbmsd_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	size_t block_size_log2 = context->block_size_log2;
	bnum_t block_count = context->block_count;
	struct tegrabl_bdev *user_dev = NULL;
	uint32_t device_id = 0;

	user_dev = tegrabl_calloc(1, sizeof(struct tegrabl_bdev));
	if (user_dev == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY,
				      TEGRABL_USBMSD_REGISTER);
		TEGRABL_SET_ERROR_STRING(error, "%d", "user dev",
					 (uint32_t)sizeof(struct tegrabl_bdev));
		goto fail;
	}

	device_id = TEGRABL_STORAGE_USB_MS << 16 | context->instance;
	pr_debug("usbmsd device id %08x", device_id);

	/*
	 * This calls back into ./lib/blockdev/tegrabl_blockdev.c
	 *  and sets up 'default' read/write function pointers, etc.
	 */
	error = tegrabl_blockdev_initialize_bdev(user_dev, device_id,
						 block_size_log2, block_count);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INIT_FAILED,
					   "blockdev bdev");
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	user_dev->buf_align_size = TEGRABL_USB_BUF_ALIGN_SIZE;

	/* Replace default r/w functions with our bdev function pointers */
	user_dev->read_block = tegrabl_usbmsd_bdev_read_block;
#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)
	user_dev->write_block = tegrabl_usbmsd_bdev_write_block;
	user_dev->erase = tegrabl_usbmsd_bdev_erase;
#endif
	user_dev->close = tegrabl_usbmsd_bdev_close;
	user_dev->ioctl = tegrabl_usbmsd_bdev_ioctl;
	user_dev->priv_data = (void *)context;
	user_dev->xfer = tegrabl_usbmsd_bdev_xfer;

	/*
	 * This calls back into ./lib/blockdev/tegrabl_blockdev.c
	 *  and adds this dev to block device list 'bdevs' at tail if not dupe
	 */
	error = tegrabl_blockdev_register_device(user_dev);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_REGISTER_FAILED,
				 "instance %d", "blockdev", context->instance);
		TEGRABL_SET_HIGHEST_MODULE(error);
	}

fail:
	return error;
}

static void do_bulk_reset(struct tegrabl_usbmsd_context *context)
{
	uint8_t iface_num, protocol;
	struct device_request req;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Get host context, bInterfaceNumber is in enum_dev.interface_indx */
	iface_num = context->host_context.curr_dev_priv->enum_dev.interface_indx;
	pr_debug("bIinterfaceNumber = %d\n", iface_num);

	/* Check protocol here, must be BBB */
	protocol = context->host_context.curr_dev_priv->enum_dev.protocol;
	if (protocol != BULK_PROTOCOL) {
		pr_error("Bad protocol = 0x%02X\n", protocol);
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				      TEGRABL_USBMSD_BULK_RESET);
		TEGRABL_SET_ERROR_STRING(error, "protocol: 0x%02X", protocol);
		goto fail;
	}

	/*
	 * Issue a class specific Bulk-Only Mass Storage Reset request,
	 *  according to section 3.1 of USB Mass Storage Class Bulk-Only
	 *  Transport Spec, v1.0.
	 */
	pr_debug("setting up request\n");

	/*
	 * NOTE: Need a complete define for this request type,
	 *  like 'UT_WRITE_CLASS_IFACE' (INTERFACE+OUT+CLASS)
	 */
	req.bmRequestTypeUnion.bmRequestType = (HOST2DEV_INTERFACE | 0x20);
	req.bRequest = BBB_RESET;
	req.wValue = 0;
	req.wIndex = iface_num;
	req.wLength = 0;

	pr_debug("calling snd_command\n");
	error = tegrabl_usbh_snd_command(req, NULL);
	/*
	 * NOTE: Should snd_command return ERR_NACK if the device NAK's,
	 *  or does the host driver handle that condition? Does it only
	 *  return after reset is complete?
	 * Can RESET be STALLed?
	 */
	if (error == TEGRABL_NO_ERROR) {
		tegrabl_mdelay(100);
	} else {
		pr_error("usbh_snd_command returned error 0x%X\n", error);
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				      TEGRABL_USBMSD_BULK_RESET);
		TEGRABL_SET_ERROR_STRING(error, "request 0x%02X", BBB_RESET);
	}
fail:
	pr_debug("%s: Exiting, error code = 0x%X\n", __func__,  error);
	return;
}

void clear_endpoint_stall(struct tegrabl_usbmsd_context *context, uint8_t ep)
{
	struct device_request req;
	tegrabl_error_t error;

	/* clear endpoint stall for specified EP */
	req.bmRequestTypeUnion.bmRequestType = HOST2DEV_ENDPOINT;
	req.bRequest = ENDPOINT_CLEAR_FEATURE;
	req.wValue = ENDPOINT_HALT;
	req.wIndex = ep;
	req.wLength = 0;

	pr_debug("%s: doing endpoint clear on EP %d\n", __func__, req.wIndex);
	error = tegrabl_usbh_snd_command(req, NULL);

	/* Handle response */
	if (error != TEGRABL_NO_ERROR) {
		pr_error("usbh_snd_command returned error 0x%X\n", error);
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				      TEGRABL_USBMSD_RESET_RECOVERY);
		TEGRABL_SET_ERROR_STRING(error, "request 0x%02X",
					 ENDPOINT_CLEAR_FEATURE);
	}

	pr_debug("%s: Exiting, error code = 0x%X\n", __func__, error);
	return;
}

/** @brief Initializes the USB host controller with the given instance.
 *
 *  @param instance USBMSD instance to be initialized
 *
 *  @return TEGRABL_NO_ERROR if init is successful else appropriate error.
 */

tegrabl_error_t tegrabl_usbmsd_bdev_open(uint32_t instance)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct tegrabl_usbmsd_context *context = NULL;
	uint8_t *in_buf = NULL;
	uint8_t retry = 5;
	struct xusb_host_context *host_ctx = NULL;

	if (init_done)
		goto fail;

	pr_debug("Initializing usbmsd device instance %d\n", instance);

	context = tegrabl_calloc(1, sizeof(struct tegrabl_usbmsd_context));

	if (context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY,
				      TEGRABL_USBMSD_BDEV_OPEN);
		TEGRABL_SET_ERROR_STRING(error, "%d", "context",
			(uint32_t)sizeof(struct tegrabl_usbmsd_context));
		goto fail;
	}

	pr_debug("Setting default context\n");
	tegrabl_usbmsd_set_default_context(context, instance);

	/*
	 * NOTE: usb-host driver queries the device for descriptors and stores
	 *  that data in xhci_host_context->enum_dev, etc.
	 */

	/* Get host context here */
	pr_debug("calling get_usbh_context ..\n");
	host_ctx = tegrabl_get_usbh_context();

	if (host_ctx == NULL) {
		pr_error("Host context not found, host driver not loaded?\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INIT_FAILED,
				      TEGRABL_USBMSD_BDEV_OPEN);
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INIT_FAILED,
					   "usb host driver get_context failed");
		goto fail;
	}

	/* Check host context - zeros means it didn't enumerate a device */
	if ((host_ctx->curr_dev_priv->enum_dev.vendor_id == 0) &&
		(host_ctx->curr_dev_priv->enum_dev.product_id == 0)) {
		pr_error("Empty host context, host driver not loaded?\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_INIT_FAILED,
				      TEGRABL_USBMSD_BDEV_OPEN);
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INIT_FAILED,
					   "usb host driver context empty");
		goto fail;
	}

	/* Check if enumerated device belongs to MSD class and protocol is bulk-only */
	if ((host_ctx->curr_dev_priv->enum_dev.class != USB_MSD_CLASS) ||
		(host_ctx->curr_dev_priv->enum_dev.protocol != BULK_PROTOCOL)) {
		pr_error("Enumerated device doesn't belong to MSD class or protocol is not bulk-only!!\n");
		pr_error("Class = %X, Protocol = %X\n",
			 host_ctx->curr_dev_priv->enum_dev.class,
			 host_ctx->curr_dev_priv->enum_dev.protocol);
		error = TEGRABL_ERROR(TEGRABL_ERR_INIT_FAILED,
				      TEGRABL_USBMSD_BDEV_OPEN);
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INIT_FAILED,
			"Enumerated USB device either not an MSD device or supported protocol is not BULK_ONLY protocol");
		goto fail;
	}

	/* Save away host context in usbmsd context */
	memcpy(&context->host_context, host_ctx,
		sizeof(struct xusb_host_context));

#ifdef USB_DEBUG
	/* Verify that host context is OK w/a few debug prints */
	pr_info("host context, Vendor 0x%04X, Product 0x%04X\n",
		 context->host_context.curr_dev_priv->enum_dev.vendor_id,
		 context->host_context.curr_dev_priv->enum_dev.product_id);
	pr_info("Device address = %d, InterfaceNumber = %d\n",
		 context->host_context.curr_dev_priv->enum_dev.dev_addr,
		 context->host_context.curr_dev_priv->enum_dev.interface_indx);
	pr_info("class/subclass/protocol = %02X/%02X/%02X\n",
		 context->host_context.curr_dev_priv->enum_dev.class,
		 context->host_context.curr_dev_priv->enum_dev.subclass,
		 context->host_context.curr_dev_priv->enum_dev.protocol);
	pr_info("USB_DIR_OUT = %d: EP0 = address %d, maxpackets out = %d\n",
		 USB_DIR_OUT, context->host_context.curr_dev_priv->enum_dev.ep[0].addr,
		 context->host_context.curr_dev_priv->enum_dev.ep[0].packet_size);
	pr_info("USB_DIR_IN  = %d: EP1 = address %d, maxpackets in = %d\n",
		 USB_DIR_IN, context->host_context.curr_dev_priv->enum_dev.ep[1].addr,
		 context->host_context.curr_dev_priv->enum_dev.ep[1].packet_size);
#endif
	/* Save some host context to our context for ease of use */

	/* NOTE: host driver hardcodes OUT EP as ep[0], IN EP as ep[1]! */
	context->out_ep = context->host_context.curr_dev_priv->enum_dev.ep[USB_DIR_OUT].addr;
	context->in_ep = context->host_context.curr_dev_priv->enum_dev.ep[USB_DIR_IN].addr;

	context->tag = 0x1;		/* increment after each CSW xfer */

	pr_debug("Initializing usbmsd controller\n");
	error = tegrabl_usbmsd_init(context);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INIT_FAILED,
					   "usbmsd init");
		goto fail;
	}

	/* Allocate a small buffer for local use (INQ, TUR, etc.) */
	pr_debug("%s: allocating buffer\n", __func__);
	in_buf = (uint8_t *)tegrabl_alloc_align(TEGRABL_HEAP_DMA, 8, 256);
	if (in_buf == NULL) {
		pr_error("Not enough memory! Need %d bytes\n", 256);
		error =  TEGRABL_ERR_NO_MEMORY;
		goto fail;
	}

	memset(in_buf, 0, 256);
	context->xfer_info.buf = in_buf;

	/* call INQUIRY here to test device I/O */
	error = tegrabl_usbmsd_inquiry(context);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("INQUIRY returned error 0x%X ...\n", error);
	}

	/* See if device is ready with TEST_UNIT_READY */
	do {
		error = tegrabl_usbmsd_test_unit_ready(context);
		if (error == TEGRABL_NO_ERROR)
			break;

		pr_warn("TEST UNIT READY returned error 0x%X, retries @ %d ...\n",
			error, retry);
		if (error != TEGRABL_ERR_NOT_READY) {
			/* any other error, skip READ CAP */
			goto regint;
		}
		/* NOT READY, retry TUR */
	} while (retry--);

	/* READ_CAP: fill in context->block_count with # blocks in device */
	error = tegrabl_usbmsd_read_capacity(context);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("READ CAPACITY returned error 0x%X ...\n", error);
	} else
		pr_info("READ CAPACITY - last LBA = %" PRIu64 ", %"
			PRIu64 "MB\n", context->block_count,
			(context->block_count << context->block_size_log2)
			/ (1024*1024));
regint:
	pr_debug("Registering instance ..\n");
	error = tegrabl_usbmsd_register(context);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_REGISTER_FAILED,
					   "usbmsd register, %d", instance);
		goto fail;
	}

	init_done = true;

fail:
	if (in_buf != NULL) {
		pr_debug("%s: freeing temp local buffer\n", __func__);
		tegrabl_dealloc(TEGRABL_HEAP_DMA, in_buf);
	}

	if (error != TEGRABL_NO_ERROR)
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_OPEN_FAILED,
					   "usbmsd open, %d", instance);

	if (context) {
		context->initialized = init_done;
	}

	pr_debug("%s: Exiting, w/error code 0x%X ...\n", __func__, error);
	return error;
}

static int get_max_lun(struct tegrabl_usbmsd_context *context)
{
	int maxlun;
	uint8_t iface_num, buf = 0;
	struct device_request req;
	tegrabl_error_t error;

	/* Get host context, bInterfaceNumber is in enum_dev.interface_indx */
	iface_num = context->host_context.curr_dev_priv->enum_dev.interface_indx;
	pr_debug("%s: bIinterfaceNumber = %d\n", __func__, iface_num);

	maxlun = 0;				/* if request fails, use 0 */

	/* Send Get Max Lun command to device */

	/*
	 * NOTE: CLASS_SPECIFIC_REQUEST is CLASS+IN+INTERFACE.
	 *  Needs a better name!
	 */
	req.bmRequestTypeUnion.bmRequestType = CLASS_SPECIFIC_REQUEST;
	req.bRequest = GET_MAX_LUN;
	req.wValue = 0;
	req.wIndex = iface_num;
	req.wLength = USB_GET_MAX_LUN_LENGTH;

	error = tegrabl_usbh_snd_command(req, &buf);
	/*
	 * NOTE: usbh_snd_command should return STALL or SHORT on error,
	 *  instead of just a tegrabl error!
	 */

	/* Handle response */
	switch (error) {
	case TEGRABL_NO_ERROR:
		maxlun = buf;
		pr_debug("max LUN = %d\n", maxlun);
		if (maxlun == 0xFF)
			maxlun = 0;
		break;
	case COMP_STALL_ERROR:
		/* If device STALLs on Max Lun, just use maxlun = 0 */
		pr_warn("Device STALLed Get Max Lun, using %d\n", maxlun);
		break;
	default:
		/* Device doesn't support Get Max Lun request, use 0 */
		pr_warn("Get Max Lun not supported (using %d)\n", maxlun);
		break;
	}

	pr_debug("%s: Exiting, w/error code 0x%X ...\n", __func__, error);
	return maxlun;
}

tegrabl_error_t tegrabl_usbmsd_init(struct tegrabl_usbmsd_context *context)
{
	/*
	 * All init should have been done by the host driver below us.
	 * If we need any additional init, do it here.
	 */

	TEGRABL_ASSERT(context != NULL);

	context->max_lun = get_max_lun(context);
	pr_info("Max LUN = %d\n", context->max_lun);
	context->current_lun = 0;	/* set default LUN */

	return TEGRABL_NO_ERROR;
}

/*
 * CMD/DATA/STATUS state machine functions:
 *   Send CBW (command)
 *   Transfer data (if any, 0 is OK)
 *   Receive CSW (status)
 */

/*
 * status = NO_ERROR: Success, command sent to device
 * status = NOT_READY: Device NAK'd the command
 * status = anything else: Command phase failed
 * entry: context = MSD context
 * entry: num_bytes = data length
 */
tegrabl_error_t usbmsd_send_cbw(struct tegrabl_usbmsd_context *context,
				uint32_t num_bytes)
{
	usb_msd_cbw_t cbw;
	uint32_t xfer_length;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t cmdlen, dev_id;

	TEGRABL_ASSERT(context != NULL);
	pr_debug("Entry, num_bytes = %d\n", num_bytes);

	/* Check params */
	cmdlen = context->cmdlen;
	if (cmdlen == 0 || cmdlen > CBW_CDBLENGTH) {
		error = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				      TEGRABL_USBMSD_START_COMMAND_1);
		TEGRABL_SET_ERROR_STRING(error, "cmdlen: %d", cmdlen);
		goto fail;
	}
	pr_debug("cmdlen = %d\n", cmdlen);

	dev_id = context->host_context.curr_dev_priv->enum_dev.dev_addr;

	/*
	 * NOTE: num_bytes == 0 is OK here, just means no data phase.
	 *  Also, if this is a SCSI query op, like INQUIRY, TUR, etc.,
	 *  then is_write will be 0, meaning CBW_IN, DEV2HOST xfer.
	 */

	pr_debug("Set up CBW...\n");
	/* Wrap xfer_info in CBW, send down to host driver */
	cbw.Signature = CBW_SIGNATURE;
	cbw.Tag = context->tag;
	cbw.DataTransferLength = num_bytes;
	cbw.Flags = context->xfer_info.is_write ? CBW_OUT_FLAG : CBW_IN_FLAG;
	cbw.LUN = context->current_lun;

	pr_debug("Copy CDB from context...\n");
	cbw.Length = cmdlen;
	memset(cbw.CDB, 0x0, CBW_CDBLENGTH);
	/* CDB (context->cmd) should have already been set up by the caller */
	memcpy(cbw.CDB, context->cmd, cmdlen);
	xfer_length = CBW_SIZE;

#ifdef	USB_DEBUG
	pr_info("device ID (address) = %d, IO direction is %s\n", dev_id,
		 cbw.Flags & CBW_IN_FLAG ? "IN" : "OUT");
	dump_cbw(&cbw);
#endif

	/* Send CBW to device via host driver I/O */
	error = tegrabl_usbh_snd_data(dev_id, &cbw, &xfer_length);

	/* check xfer_length, s/b == CBW_SIZE, 31 bytes */
	if (xfer_length != CBW_SIZE) {
		pr_error("Warning: Transferred %d CBW bytes, should be %d!\n",
			 xfer_length, CBW_SIZE);
		error = TEGRABL_ERROR(TEGRABL_ERR_SEND_FAILED,
				      TEGRABL_USBMSD_START_COMMAND_2);
	}
fail:
	pr_debug("%s: Exit, returning error code 0x%X\n", __func__, error);
	return error;
}

/*
 * status = NO_ERROR: Success, data transferred OK
 * status = NOT_READY: Device NAK'd the transfer
 * status = anything else: Data phase failed
 * entry: context = MSD context
 * entry: num_bytes = data length
 */
tegrabl_error_t usbmsd_transfer_data(struct tegrabl_usbmsd_context *context,
				uint32_t num_bytes)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t xfer_length;
	uint8_t dev_id;

	TEGRABL_ASSERT(context != NULL);
	pr_debug("%s: Entry, num_bytes = %d\n", __func__, num_bytes);

	/* Transfer data, either HOST2DEV or DEV2HOST */

	/* Just return SUCCESS/NO_ERROR if no data transfer required */
	if (num_bytes == 0)
		goto fail;

	xfer_length = num_bytes;
	dev_id = context->host_context.curr_dev_priv->enum_dev.dev_addr;

	if (context->xfer_info.buf == NULL) {
		pr_error("xfer_info buffer is NULL!\n");
		error =  TEGRABL_ERR_NO_MEMORY;
		goto fail;
	}
	pr_debug("%s: xfer_info buf = %p\n", __func__, context->xfer_info.buf);

#ifdef	USB_DEBUG
	pr_info("device ID (address) = %d, IO direction is %s\n", dev_id,
		 context->xfer_info.is_write ? "OUT" : "IN");
	pr_info("requested data length = %u bytes\n", xfer_length);
#endif
	/* Transfer data via host driver I/O */
	if (context->xfer_info.is_write)
		error = tegrabl_usbh_snd_data(dev_id,
				context->xfer_info.buf,
				&xfer_length);
	else
		error = tegrabl_usbh_rcv_data(dev_id,
				context->xfer_info.buf,
				&xfer_length);

	/* TBD: Check xfer length, handle short xfer condition */
	pr_debug("transferred data length = %u bytes\n", xfer_length);

#ifdef	USB_DEBUG
	if (num_bytes <= 1024)
		dump_buf(context->xfer_info.buf, num_bytes);
	else
		dump_buf(context->xfer_info.buf, 1024);	/* 1st 2 sectors */
#endif
	/* TBD: Process error, send bulk reset/clear endpoint on STALL */
	/* TBD: Return NOT_READY if device NAK'd so caller can retry? */

fail:
	pr_debug("%s: Exit, returning error code 0x%X\n", __func__, error);
	return error;
}

/*
 * status = NO_ERROR: Success, status returned by device
 * status = NOT_READY: Device NAK'd the request
 * status = anything else: Status phase failed
 * entry: context = MSD context
 * entry: status = CSW status byte
 */
tegrabl_error_t usbmsd_get_csw(struct tegrabl_usbmsd_context *context,
			       uint8_t *status)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct xusb_host_context *host_ctx = NULL;
	usb_msd_csw_t csw;
	uint32_t xfer_length;
	uint8_t dev_id, comp_code = 0;

	TEGRABL_ASSERT(context != NULL);
	pr_debug("Entry, status = %d\n", *status);

	/* Get status of last CBW from device */
	xfer_length = CSW_SIZE;
	memset(&csw, 0, xfer_length);
	dev_id = context->host_context.curr_dev_priv->enum_dev.dev_addr;

#ifdef	USB_DEBUG
	pr_debug("device ID (address) = %d\n", dev_id);
	pr_debug("CSW length = %u\n", xfer_length);
#endif
	/* Get device status into our CSW */
	error = tegrabl_usbh_rcv_data(dev_id, &csw, &xfer_length);

	/* If STALL, clear IN endpoint, retry once */
	host_ctx = tegrabl_get_usbh_context();	/* Current host context */
	if (host_ctx != NULL)
		comp_code = host_ctx->comp_code;
	if (comp_code != COMP_SUCCESS) {
		pr_debug("%s: STATUS: Host driver returned COMP CODE 0x%02X\n",
			 __func__, comp_code);
		if (comp_code == COMP_STALL_ERROR) {
			clear_endpoint_stall(context, context->in_ep);
			/* Retry CSW */
			xfer_length = CSW_SIZE;
			error = tegrabl_usbh_rcv_data(dev_id, &csw,
						      &xfer_length);
		}
	}

	/* Validate xfer_length == CSW_SIZE */
	if (xfer_length != CSW_SIZE) {
		pr_error("Warning: Transferred %d CSW bytes, should be %d!\n",
			 xfer_length, CSW_SIZE);
		error = TEGRABL_ERROR(TEGRABL_ERR_UNKNOWN_STATUS,
				      TEGRABL_USBMSD_BAD_STATUS);
		goto fail;
	}

	/* Handle data/phase errors, do reset recovery */
#ifdef	USB_DEBUG
	dump_csw(&csw);
#endif
	if (csw.Signature != CSW_SIGNATURE) {
		/* Invalid data, bulk reset and fail */
		pr_error("Bad CSW SIG (%X)!\n", csw.Signature);
		do_bulk_reset(context);
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID,
				      TEGRABL_USBMSD_BAD_SIG);
		TEGRABL_SET_ERROR_STRING(error, "CSW SIG %X",
					 csw.Signature);
		goto fail;
	} else if (csw.Tag != context->tag) {
		/* Device confused?, bulk reset and fail */
		do_bulk_reset(context);
		pr_error("Bad CSW Tag (%X), context->tag = %X\n",
			 csw.Tag, context->tag);
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID,
				      TEGRABL_USBMSD_BAD_TAG);
		TEGRABL_SET_ERROR_STRING(error, "CSW TAG %X", csw.Tag);
		goto fail;
	} else if (csw.Status >= CSW_PHASE_STAT) {
		/* Phase error, do reset recovery */
		pr_error("Bad CSW status (%d)!\n", csw.Status);
		do_bulk_reset(context);
		clear_endpoint_stall(context, context->in_ep);
		clear_endpoint_stall(context, context->out_ep);
		error = TEGRABL_ERROR(TEGRABL_ERR_UNKNOWN_STATUS,
				      TEGRABL_USBMSD_PHASE_ERR);
		TEGRABL_SET_ERROR_STRING(error, "CSW status 0x%02X",
					 csw.Status);
	}

	/* OK, return csw.Status, error s/b NO_ERROR */
	*status = csw.Status;
	context->csw_status = csw.Status;	/* save for later */

fail:
	pr_debug("%s: Incrementing tag, was %X\n", __func__, context->tag);
	context->tag++;		/* Bump tag regardless */

	pr_debug("%s: Exit, returning error code 0x%X, status = %d, tag = %X\n",
		 __func__, error, *status, context->tag);
	return error;
}

/**
 * @brief Read or write number of blocks starting from specified block
 *
 * @param context Context information
 * @param buf Buffer to save read content or to write to device
 * @param block Start sector for read/write
 * @param count Number of sectors to read/write
 * @param is_write True if write operation
 * @param timeout Timeout in uSec
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_usbmsd_xfer(struct tegrabl_usbmsd_context *context,
				void *buf, bnum_t block, bnum_t count,
				bool is_write, time_t timeout)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t xfer_length;

	TEGRABL_ASSERT(context != NULL);
	TEGRABL_ASSERT(buf != NULL);
	TEGRABL_ASSERT(count != 0UL);

	context->xfer_info.is_write = is_write;
	context->xfer_info.bdev_xfer_info->block_count = count;
	context->xfer_info.bdev_xfer_info->start_block = block;
	xfer_length = (count << context->block_size_log2);

	pr_debug("block = %u, count = %u\n", block, count);
	pr_debug("%sING\n", is_write ? "WRIT" : "READ");
	pr_debug("  %u bytes\n", xfer_length);

	error = tegrabl_usbmsd_io(context, buf, xfer_length, timeout);

	pr_debug("%s: Exiting with error code 0x%X\n", __func__, error);
	return error;
}

tegrabl_error_t tegrabl_usbmsd_inquiry(struct tegrabl_usbmsd_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t cmdlen;
	uint32_t xfer_length;

	TEGRABL_ASSERT(context != NULL);
	pr_debug("Entry\n");

	cmdlen = 12;		/* only 6 bytes in INQUIRY command block?? */
	xfer_length = 36;	/* expect at most 36 bytes of info */

	memset(context->cmd, 0x0, cmdlen);
	context->cmd[0] = INQUIRY;
	context->cmd[4] = (uint8_t)xfer_length;	/* only need write LSB */

	context->cmdlen = cmdlen;
	context->xfer_info.is_write = 0;	/* data comes from device */

	pr_debug("%s: Calling USBMSD_IO, xferlen = %u, cmdlen = %d\n",
		 __func__, xfer_length, cmdlen);
	error = tegrabl_usbmsd_io(context, context->xfer_info.buf, xfer_length,
				  TEGRABL_USBMSD_READ_TIMEOUT);

	if (error == TEGRABL_NO_ERROR) {
		/* dump INQUIRY info in readable format */
		char *pp = (char *)context->xfer_info.buf;
		char c = pp[16];
		pp[16] = 0;
		pr_info("Vendor Identification   :  %s\n", pp + 8);
		pp[16] = c;
		c = pp[32];
		pp[32] = 0;
		pr_info("Product Identification  :  %s\n", pp + 16);
		pp[32] = c;
		pp[36] = 0;
		pr_info("Product Revision Level  :  %s\n", pp + 32);
	}
	pr_debug("%s: Exiting with error code 0x%X\n", __func__, error);
	return error;
}

tegrabl_error_t tegrabl_usbmsd_request_sense(
					struct tegrabl_usbmsd_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t cmdlen, sense_buf[20];		/* TBD How much do we need? */
	uint32_t xfer_length;

	TEGRABL_ASSERT(context != NULL);
	pr_debug("Entry\n");

	cmdlen = 12;		/* # of bytes in REQUEST SENSE command block */
	xfer_length = 18;	/* get 18 bytes of sense data for now */

	memset(context->cmd, 0x0, cmdlen);
	context->cmd[0] = REQUEST_SENSE;
	context->cmd[4] = (uint8_t)xfer_length;	/* only need write LSB */

	context->cmdlen = cmdlen;
	context->xfer_info.is_write = 0;	/* data comes to us */

	pr_debug("%s: Calling USBMSD_IO, xferlen = %u, cmdlen = %d\n",
		 __func__, xfer_length, cmdlen);
	error = tegrabl_usbmsd_io(context, &sense_buf, xfer_length,
				  TEGRABL_USBMSD_REQUEST_SENSE_TIMEOUT);
#ifdef USB_DEBUG
	pr_debug("%s: Full Sense Data:\n", __func__);
	dump_buf(sense_buf, xfer_length);
#endif
	if (error == TEGRABL_NO_ERROR) {
		pr_debug("REQUEST_SENSE returned sense key: %02X,"
			 " ASC: %02X, ASCQ: %02X\n", sense_buf[2],
			 sense_buf[12], sense_buf[13]);
		/* copy sense buffer to context */
		memcpy(context->sense_data, sense_buf, xfer_length);
	}

	pr_debug("%s: Exiting with error code 0x%X\n", __func__, error);
	return error;

}

tegrabl_error_t tegrabl_usbmsd_test_unit_ready(
					struct tegrabl_usbmsd_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	tegrabl_error_t rs_err = TEGRABL_NO_ERROR;
	uint8_t cmdlen, buf[4];
	uint32_t xfer_length;
	uint8_t retry = 3;

	TEGRABL_ASSERT(context != NULL);
	pr_debug("Entry\n");

	do {
		cmdlen = 12;		/* # of bytes in TUR command block */
		xfer_length = 0;	/* TUR returns no data */

		memset(context->cmd, 0x0, cmdlen);
		context->cmd[0] = TEST_UNIT_READY;

		context->cmdlen = cmdlen;
		context->xfer_info.is_write = 0;	/* No data xfer */

		pr_debug("%s: Calling USBMSD_IO, xferlen = %u, cmdlen = %d\n",
			 __func__, xfer_length, cmdlen);
		error = tegrabl_usbmsd_io(context, &buf, xfer_length,
					  TEGRABL_USBMSD_READ_TIMEOUT);

		if (error == TEGRABL_NO_ERROR)
			goto done;

		if (TEGRABL_ERROR_AUX_INFO(error) == TEGRABL_USBMSD_BAD_STATUS) {
			/* TBD: Check csw_status in context instead? */
			/* csw.Status was bad, do request sense and retry */
			pr_debug("%s: Bad CSW status\n", __func__);
			if (retry < 3)
				pr_warn("Trying Request Sense, retry @ %d\n", retry);
			rs_err = tegrabl_usbmsd_request_sense(context);
			if (rs_err != TEGRABL_NO_ERROR)
				pr_warn("Request sense failed, error 0x%X\n",
					rs_err);
			else {
				/* Parse sense data here, quit if NOT READY */
				if (context->sense_data[2] == 0x02) {
					if ((context->sense_data[12] == 0x3A) ||
					    (context->sense_data[12] == 0x04))
					/* device NOT READY, bail out */
					pr_warn("USB device not ready!\n");
					error = TEGRABL_ERR_NOT_READY;
					goto fail;
				}
			}
			tegrabl_mdelay(100);		/* TBD 100mSec OK? */
		}
	} while (retry--);
done:
	if (error == TEGRABL_NO_ERROR && retry < 3)
		pr_debug("Request Sense cleared status error ...\n");
fail:
	pr_debug("%s: Exiting with error code 0x%X\n", __func__, error);
	return error;
}

tegrabl_error_t tegrabl_usbmsd_read_capacity(
					struct tegrabl_usbmsd_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t cmdlen;
	uint32_t xfer_length, buf[2];

	TEGRABL_ASSERT(context != NULL);
	pr_debug("%s: Entry\n", __func__);

	cmdlen = 12;		/* # of bytes in READ_CAP command block */
	xfer_length = 8;	/* expect 8 bytes of info */

	memset(context->cmd, 0x0, cmdlen);
	context->cmd[0] = READ_CAPACITY;

	context->cmdlen = cmdlen;
	context->xfer_info.is_write = 0;	/* data comes from device */

	context->xfer_info.buf = &buf;

	pr_debug("%s: Calling USBMSD_IO, xferlen = %u, cmdlen = %d\n",
		 __func__, xfer_length, cmdlen);
	error = tegrabl_usbmsd_io(context, &buf, xfer_length,
				  TEGRABL_USBMSD_READ_TIMEOUT);
	/* TBD: Retry a few times on error? */

#ifdef	USB_DEBUG
	pr_debug("%s: local 'buf' data ..\n", __func__);
	dump_buf(buf, xfer_length);
#endif
	buf[0] = be32tole32(buf[0]);	/* Max LBA */
	buf[1] = be32tole32(buf[1]);	/* Block size */

	/* TBD: If FFFFFFFFh is returned, use READ_CAPACITY(16)? */
	pr_debug("Max LBA (buf[0]) = 0x%X, block size (buf[1]) = %u\n",
		 (buf[0])+1, buf[1]);

	if (error == TEGRABL_NO_ERROR)
		context->block_count = (uint64_t)(buf[0] + 1);

	/* TBD: Use buf[1] from READ_CAP as block size? (s/b 512) */

	pr_debug("%s: Exiting with error code 0x%X\n", __func__, error);
	return error;
}

/**
 * @brief Do I/O using CBW/CSW
 *
 * @param context Context information
 * @param buf Buffer to save read content or to write to device
 * @param length Number of bytes to read/write (data phase)
 * @param timeout Timeout in uSec
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_usbmsd_io(struct tegrabl_usbmsd_context *context,
					 void *buf, uint32_t length,
					 time_t timeout)
{
	/* context has direction (is_write = H2D, else D2H)
	 * if read/write block xfer, length = block_count * block_size_log2
	 * if read/write block xfer, start_block  is in xfer_info
	 * if SCSI op, length is number bytes in response data (INQUIRY, etc.)
	 */
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t csw_status, data_dir, comp_code = 0;
	struct xusb_host_context *host_ctx = NULL;

	TEGRABL_ASSERT(context != NULL);
	TEGRABL_ASSERT(buf != NULL);
	pr_debug("%s: Entry, buf = %p\n", __func__, buf);

	context->xfer_info.buf = buf;
	if (context->xfer_info.is_write)
		data_dir = HOST2DEV;
	else
		data_dir = DEV2HOST;

	pr_debug("Calling Send_CBW ...\n");
	error = usbmsd_send_cbw(context, length);
	if (error != TEGRABL_NO_ERROR)
		goto fail;

	pr_debug("%sING\n", data_dir ? "READ" : "WRIT");
	pr_debug("  %u bytes\n", length);

	/* TBD: Check for short data xfer here? (pass &xfer_length) */
	pr_debug("Calling Transfer_Data, buffer @ %p ...\n",
		 context->xfer_info.buf);
	error = usbmsd_transfer_data(context, length);
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("DATA TRANSFER FAILED, INCREMENT TAG!?\n");
	}

	/* Handle DATA STALL here */
	host_ctx = tegrabl_get_usbh_context();	/* Current host context */
	if (host_ctx != NULL)
		comp_code = host_ctx->comp_code;
	if (comp_code != COMP_SUCCESS) {
		pr_debug("%s: Host driver returned COMP CODE 0x%02X\n",
			 __func__, comp_code);
		if (comp_code == COMP_STALL_ERROR) {
			if (data_dir == DEV2HOST)
				clear_endpoint_stall(context, context->in_ep);
			else
				clear_endpoint_stall(context, context->out_ep);
		}
		/* TBD: Handle any other bad completion codes here? */
	}

	pr_debug("Calling Get_CSW ...\n");
	error = usbmsd_get_csw(context, &csw_status);
	pr_debug("%s: CSW status = 0x%02X, tegrabl error = 0x%X\n", __func__,
		 csw_status, error);

	/* TBD Translate CSW status to appropriate error here? */
	if (error == TEGRABL_NO_ERROR && csw_status != CSW_PASS_STAT) {
		error = TEGRABL_ERROR(TEGRABL_ERR_CONDITION,
				      TEGRABL_USBMSD_BAD_STATUS);
		pr_debug("CSW status = 0x%02X\n", csw_status);
	}
fail:
	pr_debug("%s: Exiting with error code 0x%X\n", __func__, error);

	return error;
}
