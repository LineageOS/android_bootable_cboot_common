/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_SDMMC
#define NVBOOT_TARGET_FPGA 0
#include <string.h>
#include <tegrabl_blockdev.h>
#include <tegrabl_sdmmc_defs.h>
#include <tegrabl_sdmmc_rpmb.h>
#include <stdint.h>
#include <tegrabl_clock.h>
#include <tegrabl_malloc.h>
#include <tegrabl_error.h>
#if defined(CONFIG_ENABLE_CCC)
#include <tegrabl_crypto_se.h>
#else
#include <tegrabl_se.h>
#endif
#include <tegrabl_sdmmc_bdev.h>
#include <tegrabl_sdmmc_protocol.h>
#include <tegrabl_sdmmc_protocol_rpmb.h>

#define SDMMC_RPMB_DEBUG			0
#define SDMMC_RPMB_DUMP_DATA(x...)			\
	do {						\
		if (SDMMC_RPMB_DEBUG) {			\
			sdmmc_rpmb_dump_data(x); }	\
	} while (0)


/** @brief Dump specified buffer contents.
 *
 *  @param func Pointer to function name string.
 *  @param str  Pointer to debug string.
 *  @param buf  Pointer to buffer to dump.
 *  @param buf  Number of bytes to dump.
 *
 *  @return void
 */
static void sdmmc_rpmb_dump_data(const char *func, const char *str,
				uint8_t *buf, uint32_t buflen)
{
	uint32_t i;

	/* unused in some conditional compilation */
	TEGRABL_UNUSED(func);
	TEGRABL_UNUSED(str);
	TEGRABL_UNUSED(buf);

	pr_trace("%s: %s\n", func, str);

	for (i = 0; i < buflen; i++) {
		if ((i % 32U) == 0U) {
			pr_trace("%03x: ", i);
		}

		pr_trace("%02x", buf[i]);
		if (i == 0U) {
			continue;
		}
		if (((i+1U) % 8U) == 0U) {
			pr_trace(" ");
		}
		if (((i+1U) % 32U) == 0U)	{
			pr_trace("\n");
		}
	}
	pr_trace("\n");
}


/** @brief Convert 16 byte CPU integer to RPMB format.
 *
 *  @param u16_val Integer to convert to RPMB format.
 *
 *  @return 16 byte integer in RPMB format.
 */
static inline rpmb_u16_t sdmmc_convert_to_rpmb_u16(uint16_t u16_val)
{
	rpmb_u16_t rpmb_val;

	rpmb_val.data[0] = (uint8_t)(u16_val >> 8);
	rpmb_val.data[1] = (uint8_t)(u16_val >> 0);

	return rpmb_val;
}

/** @brief Convert 32 byte CPU integer to RPMB format.
 *
 *  @param u32_val Integer to convert to RPMB format.
 *
 *  @return 32 byte integer in RPMB format.
 */
static inline rpmb_u32_t sdmmc_convert_to_rpmb_u32(uint32_t u32_val)
{
	rpmb_u32_t rpmb_val;

	rpmb_val.data[0] = (uint8_t)((u32_val >> 24) & 0xffu);
	rpmb_val.data[1] = (uint8_t)((u32_val >> 16) & 0xffu);
	rpmb_val.data[2] = (uint8_t)((u32_val >> 8) & 0xffu);
	rpmb_val.data[3] = (uint8_t)(u32_val & 0xffu);

	return rpmb_val;
}

/** @brief Convert 16 byte RPMB integer to CPU format.
 *
 *  @param u16_val RPMB integer to convert to CPU format.
 *
 *  @return 16 byte integer in CPU format.
 */
static inline uint16_t sdmmc_convert_from_rpmb_u16(rpmb_u16_t rpmb_u16)
{
	uint16_t val;

	val = ((uint16_t)rpmb_u16.data[0] << 8) | rpmb_u16.data[1];

	return val;
}

/** @brief Convert 32 byte RPMB integer to CPU format.
 *
 *  @param u32_val RPMB integer to convert to CPU format.
 *
 *  @return 32 byte integer in CPU format.
 */
static inline uint32_t sdmmc_convert_from_rpmb_u32(rpmb_u32_t rpmb_u32)
{
	uint32_t val;

	val = (((uint32_t)rpmb_u32.data[0] << 24) |
		   ((uint32_t)rpmb_u32.data[1] << 16) |
		   ((uint32_t)rpmb_u32.data[2] << 8) | (uint32_t)rpmb_u32.data[3]);

	return val;
}

/** @brief Compare MAC in RPMB frame with passed in MAC buffer.
 *
 *  @param frame   Pointer to RPMB frame.
 *  @param mac_buf Pointer to MAC buffer.
 *
 *  @return TEGRABL_ERR_INVALID if there is a mismatch TEGRABL_NO_ERROR otherwise.
 */
static tegrabl_error_t sdmmc_compare_mac(sdmmc_rpmb_frame_t *frame,
										 uint8_t *mac_buf)
{
	if (memcmp(frame->key_or_mac,
			mac_buf, sizeof(frame->key_or_mac)) != 0) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return TEGRABL_NO_ERROR;
}

/** @brief Compare nonce in RPMB frame with passed in nonce buffer.
 *
 *  @param frame Pointer to RPMB frame.
 *  @param nonce Pointer to nonce buffer.
 *
 *  @return TEGRABL_ERR_INVALID if there is a mismatch TEGRABL_NO_ERROR otherwise.
 */
static tegrabl_error_t sdmmc_compare_nonce(sdmmc_rpmb_frame_t *frame, uint8_t *nonce)
{
	if (memcmp(frame->nonce, nonce, sizeof(frame->nonce)) != 0) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return TEGRABL_NO_ERROR;
}

/** @brief Sanity check RPMB response frame.
 *
 *  @param frame Pointer to RPMB response frame.
 *  @param resp  Expected response value.
 *
 *  @return TEGRABL_ERR_INVALID if there is a mismatch TEGRABL_NO_ERROR otherwise.
 */
static tegrabl_error_t sdmmc_rpmb_check_response(sdmmc_rpmb_frame_t *frame,
					uint16_t resp)
{
	uint16_t val16;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Check result. */
	val16 = sdmmc_convert_from_rpmb_u16(frame->result);
	if (val16 != (uint16_t)RPMB_RES_OK) {
		pr_warn("frame result is %d\n",val16);
		if (val16 == (uint16_t)RPMB_RES_NO_AUTH_KEY) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NOT_PROGRAMMED, 0);
		} else {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		}
		goto fail;
	}

	/* Check response type. */
	val16 = sdmmc_convert_from_rpmb_u16(frame->req_or_resp);
	pr_trace("val16 = %d\n", val16);
	if (val16 != resp) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}


fail:
	return error;
}


static tegrabl_error_t sdmmc_rpmb_generate_sha256_hash(uintptr_t input,
	uintptr_t output, uint32_t size)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

#if defined(CONFIG_ENABLE_CCC)
	error = tegrabl_crypto_compute_sha2((uint8_t *)input, size, (uint8_t *)output);
#else
	struct se_sha_input_params input_params;
	struct se_sha_context context;

	context.input_size = size;
	input_params.size_left = size;
	input_params.hash_addr = output;
	input_params.block_addr = input;
	input_params.block_size = size;
	context.hash_algorithm = 5;

	error = tegrabl_se_sha_process_block(&input_params, &context);
#endif
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		pr_error("SHA256 MAC calculation failed\n");
	}
	return error;
}


/** @brief Generate a SHA256 MAC
 *
 *  @param key     Pointer to RPMB key.
 *  @param frame   Pointer to RPMB frame.
 *  @param out     Pointer to MAC output buffer.
 *
 *  @return TEGRABL_NO_ERROR on success and failure condition otherwise.
 */
#define HMAC_BLOCK_SIZE		(64U)

static tegrabl_error_t sdmmc_rpmb_calculate_mac(sdmmc_rpmb_key_t *key,
					sdmmc_rpmb_frame_t *frame,
					uint8_t *out_mac)
{
	uint8_t mkey[HMAC_BLOCK_SIZE];
	uint8_t *buf = NULL;
	uint8_t *digest = NULL;
	uint32_t buflen;
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint32_t count = 0;

	/* Error check. */
	if ((key == NULL) || (frame == NULL) || (out_mac == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/*
	 * Using key, we want to generate a 32byte SHA256 MAC over
	 * 284 bytes starting at sdmmc_rpmb_frame_t.data.
	 */
	buflen = HMAC_BLOCK_SIZE + RPMB_MAC_MSG_SIZE;
	buf = tegrabl_alloc(TEGRABL_HEAP_DMA, buflen);
	if (buf == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 11);
		goto fail;
	}
	memset(buf, 0x0, buflen);

	digest = tegrabl_alloc(TEGRABL_HEAP_DMA, RPMB_KEY_OR_MAC_SIZE + 1U);
	if (digest == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 12);
		goto fail;
	}

	/* Prep key. */
	memset(mkey, 0, sizeof(mkey));
	memcpy(mkey, key->data, sizeof(key->data));

	memset(digest, 0 , RPMB_KEY_OR_MAC_SIZE + 1U);

	/*
	* ipad == <KEY> XOR <block(0x36)>
	* buf = concatenate(ipad,msg)
	*/
	for (count = 0; count < HMAC_BLOCK_SIZE; count++) {
		buf[count] = mkey[count] ^ 0x36U;
	}

	memcpy(&buf[HMAC_BLOCK_SIZE], frame->mac_msg, RPMB_MAC_MSG_SIZE);

	/* calculate SHA256 Hash */
	error = sdmmc_rpmb_generate_sha256_hash((uintptr_t)buf,
		(uintptr_t)digest, buflen);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}

	/*
	* opad == <KEY> XOR <block(0x5C)>
	* buf = concatenate(opad,dgst)
	*/
	for (count = 0; count < HMAC_BLOCK_SIZE; count++) {
		buf[count] = mkey[count] ^ 0x5cu;
	}
	memcpy(&buf[HMAC_BLOCK_SIZE], digest, RPMB_KEY_OR_MAC_SIZE);

	buflen = HMAC_BLOCK_SIZE + RPMB_KEY_OR_MAC_SIZE;

	/* generate final MAC */
	error = sdmmc_rpmb_generate_sha256_hash((uintptr_t)buf,
		(uintptr_t)digest, buflen);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}
	memcpy(out_mac, digest, RPMB_KEY_OR_MAC_SIZE);

fail:
	if (buf != NULL) {
		tegrabl_free(buf);
	}

	if (digest != NULL) {
		tegrabl_free(digest);
	}

	if (error != TEGRABL_NO_ERROR) {
		pr_debug("RPMB MAC calculation failed, error = %08X\n", error);
	}

	return error;
}

/** @brief Issue RPMB get write counter command.
 *
 *  @param dev           Bio layer handle for RPMB device.
 *  @param key           Pointer to RPMB key.
 *  @param counter       Pointer to buffer in which current counter is returned.
 *  @param rpmb_context  RPMB context.
 *  @param hsdmmc       Context for device on which RPMB access is desired.
 *
 *  @return TEGRABL_NO_ERROR on success and failure condition otherwise.
 */
tegrabl_error_t sdmmc_rpmb_get_write_counter(tegrabl_bdev_t *dev,
					sdmmc_rpmb_key_t *key,
					uint32_t *counter,
					sdmmc_rpmb_context_t *rpmb_context,
					struct tegrabl_sdmmc *hsdmmc)
{
	sdmmc_rpmb_frame_t *req_frame, *resp_frame;
	uint8_t mac_buf[RPMB_KEY_OR_MAC_SIZE];
	uint8_t nonce[RPMB_NONCE_SIZE] = RPMB_NONCE;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Allocate RPMB frame buffers. */
	if (rpmb_context == NULL) {
		rpmb_context = tegrabl_alloc(TEGRABL_HEAP_DMA,
			sizeof(sdmmc_rpmb_context_t));
		if (rpmb_context == NULL) {
			error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 10);
			goto fail;
		}
	}

	TEGRABL_UNUSED(dev);
	/* Setup RPMB frame pointers for getting write counter. */
	req_frame = &rpmb_context->req_frame;
	resp_frame = &rpmb_context->resp_frame;
	memset(req_frame, 0, sizeof(*req_frame));
	memset(resp_frame, 0, sizeof(*resp_frame));

	/* Init request frame. */
	req_frame->req_or_resp =
		sdmmc_convert_to_rpmb_u16(RPMB_REQ_GET_COUNTER);
	memcpy(req_frame->nonce, nonce, sizeof(req_frame->nonce));

	sdmmc_rpmb_dump_data(__func__, "Dumping pre-request frame:",
			(uint8_t *)req_frame, sizeof(*req_frame));
	sdmmc_rpmb_dump_data(__func__, "Dumping pre-response frame:",
			(uint8_t *)resp_frame, sizeof(*resp_frame));

	/* Send read request here. */
	error = sdmmc_rpmb_io(0, rpmb_context, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	sdmmc_rpmb_dump_data(__func__, "Dumping post-response frame:",
			(uint8_t *)resp_frame, sizeof(*resp_frame));

	/* Check response frame. */
	error = sdmmc_rpmb_check_response(resp_frame, RPMB_RESP_GET_COUNTER);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Generate MAC. */
	error = sdmmc_rpmb_calculate_mac(key, resp_frame, mac_buf);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Compare MACs. */
	error = sdmmc_compare_mac(resp_frame, mac_buf);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	*counter = sdmmc_convert_from_rpmb_u32(resp_frame->write_counter);
	pr_trace("Counter value is %0x\n", *counter);
fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("RPMB get write counter failed, error = %08X\n", error);
	}
	return error;
}

/** @brief Issue RPMB read command.
 *
 *  @param dev           Bio layer handle for RPMB device.
 *  @param key           Pointer to RPMB key.
 *  @param addr          Sector address to target with read operation.
 *  @param bufp          Pointer to buffer in which read data will be returned.
 *  @param buflen        Length of data to return in bufp.
 *  @param rpmb_context  RPMB context.
 *  @param hsdmmc       Context for device on which RPMB access is desired.
 *
 *  @return TEGRABL_NO_ERROR on success and failure condition otherwise.
 */
static tegrabl_error_t sdmmc_rpmb_read(tegrabl_bdev_t *dev, sdmmc_rpmb_key_t *key,
				uint16_t addr, uint8_t *bufp, uint32_t buflen,
				sdmmc_rpmb_context_t *rpmb_context,
				struct tegrabl_sdmmc *hsdmmc)
{
	sdmmc_rpmb_frame_t *req_frame, *resp_frame;
	uint8_t mac_buf[RPMB_KEY_OR_MAC_SIZE];
	uint8_t nonce[RPMB_NONCE_SIZE] = RPMB_NONCE;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	TEGRABL_UNUSED(dev);
	/* Error check key arg. */
	if (key == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Error check buffer args. */
	if ((bufp == NULL) || (buflen == 0U) || (buflen > RPMB_DATA_SIZE)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	/* Setup RPMB frame pointers for reading. */
	req_frame = &rpmb_context->req_frame;
	resp_frame = &rpmb_context->resp_frame;
	memset(req_frame, 0, sizeof(*req_frame));
	memset(resp_frame, 0, sizeof(*resp_frame));

	/* Init request frame. */
	req_frame->address = sdmmc_convert_to_rpmb_u16(addr);
	req_frame->req_or_resp = sdmmc_convert_to_rpmb_u16(RPMB_REQ_READ);
	memcpy(req_frame->nonce, nonce, sizeof(req_frame->nonce));

	sdmmc_rpmb_dump_data(__func__, "Dumping pre-request frame:",
			(uint8_t *)req_frame, sizeof(*req_frame));
	sdmmc_rpmb_dump_data(__func__, "Dumping pre-response frame:",
			(uint8_t *)resp_frame, sizeof(*resp_frame));

	/* Send request to RPMB partition. */
	error = sdmmc_rpmb_io(0, rpmb_context, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	sdmmc_rpmb_dump_data(__func__, "Dumping post-response frame:",
			(uint8_t *)resp_frame, sizeof(*resp_frame));

	/* Check response frame. */
	error = sdmmc_rpmb_check_response(resp_frame, RPMB_RESP_READ);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Compare nonce. */
	error = sdmmc_compare_nonce(resp_frame, nonce);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Generate MAC. */
	error = sdmmc_rpmb_calculate_mac(key, resp_frame, mac_buf);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Compare macs. */
	error = sdmmc_compare_mac(resp_frame, mac_buf);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Transfer data to buffer. */
	memcpy(bufp, resp_frame->data, buflen);

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_debug("RPMB read failed, error = %08X\n", error);
	}
	return error;
}

/** @brief Issue RPMB write command.
 *
 *  @param dev           Bio layer handle for RPMB device.
 *  @param key           Pointer to RPMB key.
 *  @param addr          Sector address to target with write operation.
 *  @param bufp          Pointer to buffer containing data to write.
 *  @param buflen        Length of data in bufp.
 *  @param rpmb_context  RPMB context.
 *  @param hsdmmc       Context for device on which RPMB access is desired.
 *
 *  @return TEGRABL_NO_ERROR on success and failure condition otherwise.
 */
static tegrabl_error_t sdmmc_rpmb_write(tegrabl_bdev_t *dev, sdmmc_rpmb_key_t *key,
				uint16_t addr, uint8_t *bufp, uint32_t buflen,
				sdmmc_rpmb_context_t *rpmb_context,
				struct tegrabl_sdmmc *hsdmmc)
{
	sdmmc_rpmb_frame_t *req_frame, *req_resp_frame, *resp_frame;
	uint32_t counter = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Error check key arg. */
	if (key == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Error check buffer args. */
	if ((bufp == NULL) || (buflen == 0U) || (buflen > RPMB_DATA_SIZE)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Get current write counter. */
	error = sdmmc_rpmb_get_write_counter(dev, key, &counter,
				rpmb_context, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Setup RPMB frame pointers for writing. */
	req_frame = &rpmb_context->req_frame;
	req_resp_frame = &rpmb_context->req_resp_frame;
	resp_frame = &rpmb_context->resp_frame;
	memset(req_frame, 0, sizeof(*req_frame));
	memset(req_resp_frame, 0, sizeof(*req_resp_frame));
	memset(resp_frame, 0, sizeof(*resp_frame));

	/* Init request frame. */
	req_frame->write_counter = sdmmc_convert_to_rpmb_u32(counter);
	req_frame->address = sdmmc_convert_to_rpmb_u16(addr);
	req_frame->block_count = sdmmc_convert_to_rpmb_u16(1);
	req_frame->req_or_resp = sdmmc_convert_to_rpmb_u16(RPMB_REQ_WRITE);
	memcpy(req_frame->data, bufp, buflen);

	/* Generate MAC. */
	error = sdmmc_rpmb_calculate_mac(key, req_frame, req_frame->key_or_mac);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Init request-response frame. */
	req_resp_frame->req_or_resp =
		sdmmc_convert_to_rpmb_u16(RPMB_REQ_GET_RESULT);

	sdmmc_rpmb_dump_data(__func__, "Dumping pre-request frame:",
			(uint8_t *)req_frame, sizeof(*req_frame));
	sdmmc_rpmb_dump_data(__func__, "Dumping pre-req-response frame:",
			(uint8_t *)req_resp_frame, sizeof(*req_resp_frame));
	sdmmc_rpmb_dump_data(__func__, "Dumping pre-response frame:",
			(uint8_t *)resp_frame, sizeof(*resp_frame));

	/* Send request to RPMB partition. */
	error = sdmmc_rpmb_io(1, rpmb_context, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	sdmmc_rpmb_dump_data(__func__, "Dumping post-response frame:",
			(uint8_t *)resp_frame, sizeof(*resp_frame));

	/* Check response frame. */
	error = sdmmc_rpmb_check_response(resp_frame, RPMB_RESP_WRITE);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	if (error != TEGRABL_NO_ERROR) {
		pr_error("RPMB write failed, error = %08X\n", error);
	}
	return error;
}

/** @brief Run quick RPMB I/O test to verify key programming.
 *
 *  @param dev           Bio layer handle for RPMB device.
 *  @param key           RPMB key.
 *  @param rpmb_context  RPMB context.
 *  @param hsdmmc       Context for device on which RPMB access is desired.
 *
 *  @return TEGRABL_NO_ERROR on success and failure condition otherwise.
 */
static tegrabl_error_t sdmmc_rpmb_test(tegrabl_bdev_t *dev,
	sdmmc_rpmb_key_t *key, sdmmc_rpmb_context_t *rpmb_context,
	struct tegrabl_sdmmc *hsdmmc)
{
	uint8_t *write_buf = NULL, *read_buf = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Allocate read/write data buffers */
	write_buf = tegrabl_alloc(TEGRABL_HEAP_DMA, RPMB_DATA_SIZE);
	if (write_buf == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 8);
		goto fail;
	}
	read_buf = tegrabl_alloc(TEGRABL_HEAP_DMA, RPMB_DATA_SIZE);
	if (read_buf == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 9);
		goto fail;
	}

	/* Zero out block 0. */
	memset(write_buf, 0, RPMB_DATA_SIZE);
	error = sdmmc_rpmb_write(dev, key, 0, write_buf, RPMB_DATA_SIZE,
				rpmb_context, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Fill block 0 with some pattern. */
	memset(write_buf, 0x4D, RPMB_DATA_SIZE);
	error = sdmmc_rpmb_write(dev, key, 0, write_buf, RPMB_DATA_SIZE,
				rpmb_context, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Issue read of block 0. */
	error = sdmmc_rpmb_read(dev, key, 0, read_buf, RPMB_DATA_SIZE,
				rpmb_context, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Compare read/write results. */
	if (memcmp(read_buf, write_buf, RPMB_DATA_SIZE) != 0) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Zero out block 0. */
	memset(write_buf, 0, RPMB_DATA_SIZE);
	error = sdmmc_rpmb_write(dev, key, 0, write_buf, RPMB_DATA_SIZE,
				rpmb_context, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	if (read_buf != NULL) {
		tegrabl_free(read_buf);
	}
	if (write_buf != NULL) {
		tegrabl_free(write_buf);
	}
	if (error != TEGRABL_NO_ERROR) {
		pr_error("RPMB test failed, error = %08X\n", error);
	} else {
		pr_info("RPMB test successful\n");
	}
	return error;
}

/** @brief Issue RPMB program key command.
 *
 *  @param bdev Bio layer handle for RPMB device.
 *  @param key_blob Pointer to data to use for RPMB key.
 *  @param hsdmmc Context for device on which RPMB access is desired.
 *
 *  @return TEGRABL_NO_ERROR on success and failure condition otherwise.
 */
tegrabl_error_t sdmmc_rpmb_program_key(tegrabl_bdev_t *bdev, void *key_blob,
									   struct tegrabl_sdmmc *hsdmmc)
{
	sdmmc_rpmb_key_t key;
	sdmmc_rpmb_context_t *rpmb_context = NULL;
	sdmmc_rpmb_frame_t *req_frame, *req_resp_frame, *resp_frame;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	/* Error check key arg. */
	if ((key_blob == NULL) || (bdev == NULL) || (hsdmmc == NULL)) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* Allocate RPMB frame buffers. */
	rpmb_context = tegrabl_alloc(TEGRABL_HEAP_DMA,
		sizeof(sdmmc_rpmb_context_t));
	if (rpmb_context == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 10);
		goto fail;
	}

	/* Transfer contents of key blob. */
	memcpy(key.data, key_blob, sizeof(key.data));

	sdmmc_rpmb_dump_data(__func__, "Dumping key data",
			key.data, sizeof(key.data));

	/* Setup RPMB frame pointers for key programming. */
	req_frame = &rpmb_context->req_frame;
	req_resp_frame = &rpmb_context->req_resp_frame;
	resp_frame = &rpmb_context->resp_frame;
	memset(req_frame, 0, sizeof(*req_frame));
	memset(req_resp_frame, 0, sizeof(*req_resp_frame));
	memset(resp_frame, 0, sizeof(*resp_frame));

	/* Init request frame. */
	req_frame->req_or_resp =
		sdmmc_convert_to_rpmb_u16(RPMB_REQ_PROGRAM_KEY);
	memcpy(req_frame->key_or_mac, key.data, sizeof(key.data));

	/* Init request-response frame. */
	req_resp_frame->req_or_resp =
		sdmmc_convert_to_rpmb_u16(RPMB_REQ_GET_RESULT);

	sdmmc_rpmb_dump_data(__func__, "Dumping pre-request frame:",
			(uint8_t *)req_frame, sizeof(*req_frame));
	sdmmc_rpmb_dump_data(__func__, "Dumping pre-req-response frame:",
			(uint8_t *)req_resp_frame, sizeof(*req_resp_frame));
	sdmmc_rpmb_dump_data(__func__, "Dumping pre-response frame:",
			(uint8_t *)resp_frame, sizeof(*resp_frame));
	/* Send key programming request to RPMB partition. */
	error = sdmmc_rpmb_io(1, rpmb_context, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	sdmmc_rpmb_dump_data(__func__, "Dumping post-response frame:",
			(uint8_t *)resp_frame, sizeof(*resp_frame));

	/* Check response frame. */
	error = sdmmc_rpmb_check_response(resp_frame, RPMB_RESP_PROGRAM_KEY);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}
	error = sdmmc_rpmb_test(bdev, &key, rpmb_context, hsdmmc);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

fail:
	memset(&key, 0, sizeof(key));
	if (rpmb_context != NULL) {
		tegrabl_free(rpmb_context);
	}
	if (error != TEGRABL_NO_ERROR) {
		pr_error("RPMB program key, error = %08X\n", error);
	}

	return error;
}
