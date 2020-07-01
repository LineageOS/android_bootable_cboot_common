/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_I2C

#include <stdint.h>
#include <stddef.h>
#include <bpmp_abi.h>
#include <tegrabl_error.h>
#include <tegrabl_i2c.h>
#include <tegrabl_malloc.h>
#include <string.h>
#include <tegrabl_debug.h>
#include <tegrabl_bpmp_fw_interface.h>
#include <tegrabl_i2c_bpmpfw.h>
#include <tegrabl_i2c_err_aux.h>

/*
 * Note:
 * Format of BPMP I2C transaction is (little endian):
 * [slave address (2 Bytes)] [flags (2 Bytes)] [length (2 Bytes)] [data]
 * 'struct serial_i2c_request' from bpmp-abi is used to achieve this
 */
#define HDR_LEN                 6
#define TO_I2C_7BIT_ADDR(x)     (((x) >> 1) & (0x7F))

struct tegrabl_i2c_send_recv_info {
	uint32_t bus_id;
	uint16_t slave_addr;
	uint8_t *xfer_data;
	uint16_t xfer_data_size;
	uint32_t i2c_flags;
};

static tegrabl_error_t tegrabl_virtual_i2c_bpmp_xfer
						(struct tegrabl_i2c_send_recv_info *info, bool is_read)
{
	tegrabl_error_t err;
	struct mrq_i2c_request i2c_request;
	struct mrq_i2c_response i2c_response;
	struct serial_i2c_request *hdr_xfer = NULL;

	TEGRABL_ASSERT(info != NULL);

	if ((info->xfer_data_size != 0) && (info->xfer_data == NULL)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER,
				TEGRABL_VIRTUAL_I2C_BPMP_XFER);
		TEGRABL_SET_ERROR_STRING(err, "xfer_data_size: %d, xfer_data: %p",
				info->xfer_data_size, info->xfer_data);
		goto fail;
	}

	memset(&i2c_request, 0, sizeof(i2c_request));
	memset(&i2c_response, 0, sizeof(i2c_response));

	i2c_request.cmd = CMD_I2C_XFER;
	i2c_request.xfer.bus_id = info->bus_id;

	if ((info->xfer_data_size + HDR_LEN) > TEGRA_I2C_IPC_MAX_IN_BUF_SIZE) {
		err = TEGRABL_ERROR(TEGRABL_ERR_TOO_LARGE,
				TEGRABL_VIRTUAL_I2C_BPMP_XFER);
		TEGRABL_SET_ERROR_STRING(err, "xfer data: %d", "%d", info->xfer_data_size,
				TEGRA_I2C_IPC_MAX_IN_BUF_SIZE - HDR_LEN);
	}

	hdr_xfer = (struct serial_i2c_request *)i2c_request.xfer.data_buf;
	hdr_xfer->addr = info->slave_addr;
	hdr_xfer->flags = info->i2c_flags;
	hdr_xfer->len = info->xfer_data_size;

	if (!is_read) { /* write */
		memcpy(i2c_request.xfer.data_buf + HDR_LEN,
			   info->xfer_data,
			   info->xfer_data_size);
		i2c_request.xfer.data_size = HDR_LEN + info->xfer_data_size;
	} else { /* read */
		hdr_xfer->flags |= SERIALI2C_RD;
		i2c_request.xfer.data_size = HDR_LEN;
	}

	err = tegrabl_ccplex_bpmp_xfer(
									&i2c_request, &i2c_response,
									sizeof(struct mrq_i2c_request),
									sizeof(struct mrq_i2c_response),
									MRQ_I2C);

	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_XFER_FAILED, "bpmp xfer request");
		return err;
	}

	if (is_read) {
		memcpy(info->xfer_data, i2c_response.xfer.data_buf,
			   info->xfer_data_size);
	}

fail:
	return err; /*TEGRABL_NO_ERROR*/
}

tegrabl_error_t tegrabl_virtual_i2c_xfer(struct tegrabl_i2c *hi2c,
		uint16_t slave_addr, bool repeat_start
		, void *buf, uint32_t len, bool is_read)
{
	tegrabl_error_t err;
	struct tegrabl_i2c_send_recv_info xfer_info;

	pr_trace("%s: entry\n", __func__);
	memset(&xfer_info, 0, sizeof(xfer_info));

	xfer_info.bus_id = (hi2c->instance + 1);
	xfer_info.slave_addr = TO_I2C_7BIT_ADDR(slave_addr);
	xfer_info.xfer_data = (uint8_t *)buf;
	xfer_info.xfer_data_size = len;

	if (!repeat_start) {
		xfer_info.i2c_flags |= SERIALI2C_STOP;
	}

	err = tegrabl_virtual_i2c_bpmp_xfer(&xfer_info, is_read);

	if (err != TEGRABL_NO_ERROR) {
		if (is_read) {
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_READ_FAILED,
					"%d bytes from slave 0x%02x via i2c %d wth repeat start %s",
					len, slave_addr, hi2c->instance, repeat_start ? "true" : "false");
		} else {
			TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_WRITE_FAILED,
					"%d bytes to slave 0x%02x via i2c %d wth repeat start %s",
					len, slave_addr, hi2c->instance, repeat_start ? "true" : "false");
		}

		TEGRABL_SET_HIGHEST_MODULE(err);
	}

	return err;
}

