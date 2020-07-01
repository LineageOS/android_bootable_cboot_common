/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_SE_CRYPTO

#include "build_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_se.h>
#include <tegrabl_malloc.h>
#include <tegrabl_crypto.h>
#include <tegrabl_sigheader.h>

#define TEGRABL_RSA_EXPONENT 65537
#define SHA256_SIZE_BYTES 32
#define TEGRABL_CRYPTO_SHA_MIN_BUF_SIZE 64

#define TEGRABL_CRYPTO_AES_BUFFERS 2
#define TEGRABL_CRYPTO_RSA_PSS_BUFFERS 2

static void *crypto_aes_buffers[TEGRABL_CRYPTO_AES_BUFFERS + 1];
uint8_t crypto_aes_buffer_free = 0xFF;
static void *crypto_rsa_pss_buffers[TEGRABL_CRYPTO_RSA_PSS_BUFFERS + 1];
uint8_t crypto_rsa_pss_buffer_free = 0xFF;

/* Allocate buffers required for AES operation once. Have sufficient
 * handles for simultaneous AES operations.
 */
static uint8_t tegrabl_crypto_alloc_aes_buffer(void)
{
	uint8_t i = 0;
	uint8_t temp;

	for (i = 0; i < TEGRABL_CRYPTO_AES_BUFFERS; i++) {
		temp = crypto_aes_buffer_free & (1 << i);
		if (temp != 0U) {
			break;
		}
	}

	if (i >= TEGRABL_CRYPTO_AES_BUFFERS) {
		return i;
	}

	crypto_aes_buffer_free &= (~(1 << i));

	/* Allocate buffers for :
	 * PK1, PK2 & IV of sizes SE_AES_BLOCK_LENGTH each
	 * Input & Computed hashes of sizes CMAC_HASH_SIZE_BYTES each.
	 */
	if (!crypto_aes_buffers[i]) {
		crypto_aes_buffers[i] = tegrabl_alloc(TEGRABL_HEAP_DMA,
			3 * SE_AES_BLOCK_LENGTH + 2 * CMAC_HASH_SIZE_BYTES);
	}

	return i;
}

/* Allocate buffers required for RSA PSS operations once. Have sufficient
 * handles for simultaneous RSA PSS operations.
 */
static uint8_t tegrabl_crypto_alloc_rsa_pss_buffer(void)
{
	uint8_t i = 0;
	uint8_t temp;

	for (i = 0; i < TEGRABL_CRYPTO_RSA_PSS_BUFFERS; i++) {
		temp = crypto_rsa_pss_buffer_free & (1 << i);
		if (temp != 0U) {
			break;
		}
	}

	if (i >= TEGRABL_CRYPTO_RSA_PSS_BUFFERS) {
		return i;
	}

	crypto_rsa_pss_buffer_free &= (~(1 << i));

	/* Allocate buffers for :
	 * Computed sha hash of size SHA256_SIZE_BYTES
	 * Input signature of size RSA_2048_KEY_SIZE_BITS / 8
	 * Buffer of size TEGRABL_CRYPTO_SHA_MIN_BUF_SIZE to store
	 * short data less than 64 bytes
	 */
	if (!crypto_rsa_pss_buffers[i]) {
		crypto_rsa_pss_buffers[i] = tegrabl_alloc(TEGRABL_HEAP_DMA,
			SHA256_SIZE_BYTES + TEGRABL_CRYPTO_SHA_MIN_BUF_SIZE +
			RSA_2048_KEY_SIZE_BITS / 8);
	}

	return i;
}

static inline void tegrabl_crypto_free_aes_buffer(uint32_t i)
{
	crypto_aes_buffer_free |= (1 << i);
}

static inline void tegrabl_crypto_free_rsa_pss_buffer(uint32_t i)
{
	crypto_rsa_pss_buffer_free |= (1 << i);
}

/**
 * @brief Initializes aes context from init params.
 *
 * @param context Crypto information used to resume operation.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_crypto_init_aes(
		struct tegrabl_crypto_aes_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct se_aes_context *se_aes_context;
	uint8_t *buf = NULL;

	pr_debug("Initializing crypto AES context\n");

	TEGRABL_ASSERT(context);

	se_aes_context = &context->se_context;

	pr_debug("Allocating memory for pk1, pk2, iv and hash\n");
	context->buffer_id = tegrabl_crypto_alloc_aes_buffer();
	buf = crypto_aes_buffers[context->buffer_id];
	if (!buf) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	se_aes_context->pk1 = buf;
	buf += SE_AES_BLOCK_LENGTH;
	se_aes_context->pk2 = buf;
	buf += SE_AES_BLOCK_LENGTH;
	(void)memset(buf , 0, SE_AES_BLOCK_LENGTH);
	se_aes_context->iv_encrypt = buf;

	if (se_aes_context->is_hash) {
		buf += SE_AES_BLOCK_LENGTH;
		memset(buf, 0x0, CMAC_HASH_SIZE_BYTES);
		se_aes_context->phash = buf;
		if (context->in_hash != NULL) {
			buf += CMAC_HASH_SIZE_BYTES;
			memcpy(buf, context->in_hash, CMAC_HASH_SIZE_BYTES);
			context->in_hash  = buf;
		}
	}

	context->se_input_params.size_left = se_aes_context->total_size;
fail:
	return error;
}

/**
 * @brief Initializes rsa context from init params.
 *
 * @param context Crypto information used to resume operation.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
static tegrabl_error_t tegrabl_crypto_init_rsa_pss(
		struct tegrabl_crypto_rsa_pss_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	uint8_t *buf = NULL;
	struct se_sha_context *se_sha_context;
	struct se_sha_input_params *se_sha_input_params;

	pr_debug("Initializing crypto context for rsa operation\n");

	TEGRABL_ASSERT(context);

	context->salt_length = SHA256_SIZE_BYTES;

	se_sha_context = &context->se_context;
	se_sha_context->hash_algorithm = SE_SHAMODE_SHA256;
	se_sha_input_params = &context->se_input_params;
	se_sha_input_params->size_left = se_sha_context->input_size;

	pr_debug("Allocating memory for storing computed sha hash\n");
	context->buffer_id = tegrabl_crypto_alloc_rsa_pss_buffer();
	buf = crypto_rsa_pss_buffers[context->buffer_id];
	if (!buf) {
		error = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	memset(buf, 0x0, SHA256_SIZE_BYTES);

	se_sha_input_params->hash_addr = (uintptr_t)buf;
	buf += SHA256_SIZE_BYTES;
	if (context->signature != NULL) {
		memcpy(buf, context->signature, RSA_2048_KEY_SIZE_BITS / 8);
		context->signature = (uint32_t *)buf;
	}
	buf += (RSA_2048_KEY_SIZE_BITS / 8);

	/* SE engine has requirement that input buffer must be of
	 * multiple of 64 in size except the last buffer.
	 */
	context->short_packet = buf;
	context->short_packet_offset = 0;

fail:
	return error;
}

tegrabl_error_t tegrabl_crypto_init(enum tegrabl_crypto_mode mode,
		union tegrabl_crypto_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!context) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	context->mode = mode;

	switch (mode) {
	case TEGRABL_CRYPTO_AES:
		error = tegrabl_crypto_init_aes(&context->aes);
		break;
	case TEGRABL_CRYPTO_RSA_PSS:
		error = tegrabl_crypto_init_rsa_pss(&context->rsa);
		break;
	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

fail:
	return error;
}

static tegrabl_error_t tegrabl_crypto_process_aes(
		struct tegrabl_crypto_aes_context *context, void *buffer, uint32_t size,
		void *dest)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct se_aes_input_params *aes_input;

	TEGRABL_ASSERT(context);
	TEGRABL_ASSERT(buffer);

	pr_debug("Processing buffer of size %d @ %p for AES\n",
			size, buffer);

	aes_input = &context->se_input_params;
	aes_input->src = buffer;
	aes_input->dst = dest;
	aes_input->input_size = size;

	pr_debug("Size left: %d, Total size: %d\n",
			aes_input->size_left, context->se_context.total_size);

	if (size > aes_input->size_left) {
		error = TEGRABL_ERROR(TEGRABL_ERR_OVERFLOW, 0);
		goto fail;
	}

	pr_debug("Invoking se engine to peform AES operation\n");
	error = tegrabl_se_aes_process_block(aes_input, &context->se_context);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		goto fail;
	}
fail:
	return error;
}

static tegrabl_error_t tegrabl_crypto_process_sha(
		struct tegrabl_crypto_rsa_pss_context *context, void *buffer,
		uint32_t size, void *dest)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	struct se_sha_input_params *sha_input;
	uint32_t chunk_size = 0;
	uint32_t short_packet_offset = 0;

	TEGRABL_UNUSED(dest);

	pr_debug("Processing buffer of size %d @ %p for SHA\n",
			size, buffer);

	sha_input = &context->se_input_params;
	short_packet_offset = context->short_packet_offset;

	if (short_packet_offset) {
		pr_debug("Filling short packet\n");
		chunk_size = TEGRABL_CRYPTO_SHA_MIN_BUF_SIZE - short_packet_offset;
		chunk_size = MIN(size, chunk_size);
		memcpy(context->short_packet + short_packet_offset, buffer, chunk_size);

		sha_input->block_addr = (uintptr_t)context->short_packet;
		sha_input->block_size = TEGRABL_CRYPTO_SHA_MIN_BUF_SIZE;
		short_packet_offset += chunk_size;
		context->short_packet_offset = short_packet_offset;
		size -= chunk_size;

		if (short_packet_offset != TEGRABL_CRYPTO_SHA_MIN_BUF_SIZE) {
			pr_debug("Short packet is not full skipping process\n");
			goto fail;
		}

		pr_debug("Feeding short buffer to se engine for sha operation\n");
		error = tegrabl_se_sha_process_block(sha_input, &context->se_context);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
		buffer = (void *)((uintptr_t)buffer + chunk_size);
		context->short_packet_offset = 0;
	}

	if (size) {
		if (sha_input->size_left < size)
			chunk_size = ROUND_DOWN_POW2(size, TEGRABL_CRYPTO_SHA_MIN_BUF_SIZE);
		else
			chunk_size = size;

		sha_input->block_addr = (uintptr_t)buffer;
		sha_input->block_size = chunk_size;

		pr_debug("Feeding buffer to se engine for sha operation\n");
		error = tegrabl_se_sha_process_block(sha_input, &context->se_context);
		if (error != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
		buffer = (void *)((uintptr_t)buffer + chunk_size);
		size = size - chunk_size;
	}

	if (size != 0U) {
		TEGRABL_ASSERT(size < TEGRABL_CRYPTO_SHA_MIN_BUF_SIZE);
		context->short_packet_offset = size;
		memcpy(context->short_packet, buffer, size);
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_crypto_process_block(
		union tegrabl_crypto_context *context, void *buffer, uint32_t size,
		void *dest)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	switch (context->mode) {
	case TEGRABL_CRYPTO_AES:
		error = tegrabl_crypto_process_aes(&context->aes, buffer,
				size, dest);
		break;
	case TEGRABL_CRYPTO_RSA_PSS:
		error = tegrabl_crypto_process_sha(&context->rsa, buffer,
				size, dest);
		break;
	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

	return error;
}

tegrabl_error_t tegrabl_crypto_finalize(
		union tegrabl_crypto_context *context)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!context) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	pr_debug("Finalizing crypto operation %d\n", context->mode);
	switch (context->mode) {
	case TEGRABL_CRYPTO_AES:
	{
		struct tegrabl_crypto_aes_context *aes_op;
		aes_op = &context->aes;

		if (!aes_op->is_verify) {
			break;
		}

		void *in_hash = aes_op->in_hash;
		void *computed_hash = aes_op->se_context.phash;
		TEGRABL_ASSERT(computed_hash);

		if (!in_hash) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
			goto fail;
		}

		pr_debug("Comparing input hash and computed hash\n");
		if (memcmp(in_hash, computed_hash,
				CMAC_HASH_SIZE_BYTES) != 0) {
			error = TEGRABL_ERROR(TEGRABL_ERR_VERIFY_FAILED, 0);
			goto fail;
		}
		break;
	}
	case TEGRABL_CRYPTO_RSA_PSS:
	{
		uint32_t exponent[1];
		struct tegrabl_crypto_rsa_pss_context *rsa_pss_context;
		struct se_sha_input_params *sha_input;

		exponent[0] = TEGRABL_RSA_EXPONENT;
		rsa_pss_context = &context->rsa;
		sha_input = &rsa_pss_context->se_input_params;

		if (!rsa_pss_context->signature) {
			error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
			goto fail;
		}

		pr_debug("Writing modulus\n");
		error = tegrabl_se_rsa_write_key(rsa_pss_context->key,
				rsa_pss_context->key_size, 0, SELECT_MODULUS);
		if (error != TEGRABL_NO_ERROR) {
			pr_debug("Failed to initialize modulus\n");
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		pr_debug("Writing exponent\n");
		error = tegrabl_se_rsa_write_key(exponent, 32, 0, SELECT_EXPONENT);
		if (error != TEGRABL_NO_ERROR) {
			pr_debug("Failed to initialize exponent\n");
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}

		pr_debug("Verifying rsa pss signature\n");
		error = tegrabl_se_rsa_pss_verify(
			0, rsa_pss_context->key_size, (uint8_t *)sha_input->hash_addr,
			rsa_pss_context->signature,
			rsa_pss_context->se_context.hash_algorithm,
			rsa_pss_context->salt_length);

		if (error != TEGRABL_NO_ERROR) {
			pr_debug("Rsa pss signature verification failed\n");
			TEGRABL_SET_HIGHEST_MODULE(error);
			goto fail;
		}
		break;
	}
	default:
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		goto fail;
	}

fail:
	return error;
}

tegrabl_error_t tegrabl_crypto_close(union tegrabl_crypto_context *context)
{
	if (!context) {
		return TEGRABL_NO_ERROR;
	}

	switch (context->mode) {
	case TEGRABL_CRYPTO_AES:
	{
		struct se_aes_context *se_context = NULL;
		se_context = &context->aes.se_context;
		if (se_context->is_decrypt) {
			tegrabl_se_aes_close();
		}

		tegrabl_crypto_free_aes_buffer(context->aes.buffer_id);
		break;
	}
	case TEGRABL_CRYPTO_RSA_PSS:
	{
		tegrabl_crypto_free_rsa_pss_buffer(context->rsa.buffer_id);
		break;
	}
	default:
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
	}

	return TEGRABL_NO_ERROR;
}
