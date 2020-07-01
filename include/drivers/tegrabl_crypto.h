/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_CRYPTO_H
#define INCLUDED_TEGRABL_CRYPTO_H

#include "build_config.h"
#include <tegrabl_se.h>

#define CMAC_HASH_SIZE_BYTES 16U
#define RSA_2048_KEY_SIZE_BITS 2048UL
#define ECDSA_SIG_SIZE 96U

/*
 * @brief List of crypto modes
 */
/* macro tegrabl crypto mode */
typedef uint32_t tegrabl_crypto_mode_t;
#define TEGRABL_CRYPTO_AES 0
#define TEGRABL_CRYPTO_RSA_PSS 1
#define TEGRABL_CRYPTO_ECC 2
#define TEGRABL_CRYPTO_MAX 3

/*
 * @brief Size of aes key to be used
 */
/* macro tegrabl crypto aes keysize */
typedef uint32_t tegrabl_crypto_aes_keysize_t;
#define TEGRABL_CRYPTO_AES_KEYSIZE_128 0
#define TEGRABL_CRYPTO_AES_KEYSIZE_192 1
#define TEGRABL_CRYPTO_AES_KEYSIZE_256 2

/**
 * @brief Defines information required to resume
 * aes operation.
 */
struct tegrabl_crypto_aes_context {
	tegrabl_crypto_mode_t mode;
	struct se_aes_input_params se_input_params;
	struct se_aes_context se_context;
	uint8_t *in_hash;
	uint8_t buffer_id;
	bool is_verify;
};

/**
 * @brief Defines information required to resume
 * rsa operation.
 */
struct tegrabl_crypto_rsa_pss_context {
	tegrabl_crypto_mode_t mode;
	struct se_sha_input_params se_input_params;
	struct se_sha_context se_context;
	uint32_t salt_length;
	uint32_t key_size;
	uint32_t short_packet_size;
	uint32_t short_packet_offset;
	uint32_t *key;
	uint32_t *signature;
	uint8_t *short_packet;
	uint8_t buffer_id;
};

#if defined(CONFIG_ENABLE_ECDSA)
/**
 * @brief Defines information required to resume
 * ecdsa operation.
 */

struct tegrabl_ecdsa_key {
	uint8_t x[48];
	uint8_t y[48];
};
struct tegrabl_crypto_ecdsa_context {
	tegrabl_crypto_mode_t mode;
	struct se_sha_input_params se_input_params;
	struct se_sha_context se_context;
	uint32_t salt_length;
	uint32_t key_size;
	uint32_t short_packet_size;
	uint32_t short_packet_offset;
	struct tegrabl_ecdsa_key *key;
	uint32_t *signature;
	uint8_t *short_packet;
	uint8_t buffer_id;
};
#endif

/**
 * @brief Defines information required to resume
 * crypto operation.
 */
union tegrabl_crypto_context {
	uint32_t mode;
	struct tegrabl_crypto_aes_context aes;
	struct tegrabl_crypto_rsa_pss_context rsa;
#if defined(CONFIG_ENABLE_ECDSA)
	struct tegrabl_crypto_ecdsa_context ecdsa;
#endif
};

#if defined(CONFIG_ENABLE_SECURE_BOOT)
/**
 * @brief Initialize crypto for an operation specified by mode.
 * Allocates any resources required.
 *
 * @param mode One of the operation mode supported by crypto.
 * @param init_params Parameters to initialize crypto.
 * @param context Information to be used and updated by crypto
 * to resume incomplete operation.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_crypto_init(tegrabl_crypto_mode_t mode,
		union tegrabl_crypto_context *context);

/**
 * @brief Performs operation on input buffer as per the information
 * saved in context.
 *
 * @param context Information required by crypto to continue.
 * @param buffer Address of buffer to be processed.
 * @param size Size of the buffer to be processed.
 * @param dest destination address of decrypted data
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_crypto_process_block(
		union tegrabl_crypto_context *context, void *buffer, uint32_t size,
		void *dest);

/**
 * @brief Finalizes the output of operation done so far. Input signature/hash
 * will be compared with computed values.
 *
 * @param context Information required by crypto
 *
 * @return TEGRABL_NO_ERROR if signature/hash matches else appropriate error.
 */
tegrabl_error_t tegrabl_crypto_finalize(union tegrabl_crypto_context *context);


/**
 * @brief Releases all resources acquired in init.
 *
 * @param context Information required by crypto.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_crypto_close(union tegrabl_crypto_context *context);

#else

static inline tegrabl_error_t tegrabl_crypto_init(
		tegrabl_crypto_mode_t mode,
		union tegrabl_crypto_context *context)
{
	TEGRABL_UNUSED(mode);
	TEGRABL_UNUSED(context);

	return TEGRABL_NO_ERROR;
}

static inline tegrabl_error_t tegrabl_crypto_process_block(
		union tegrabl_crypto_context *context, void *buffer, uint32_t size,
		void *dest)
{
	TEGRABL_UNUSED(context);
	TEGRABL_UNUSED(buffer);
	TEGRABL_UNUSED(size);
	TEGRABL_UNUSED(dest);

	return TEGRABL_NO_ERROR;
}

static inline tegrabl_error_t tegrabl_crypto_finalize(
		union tegrabl_crypto_context *context)
{
	TEGRABL_UNUSED(context);

	return TEGRABL_NO_ERROR;
}

static inline tegrabl_error_t tegrabl_crypto_close(
		union tegrabl_crypto_context *context)
{
	TEGRABL_UNUSED(context);

	return TEGRABL_NO_ERROR;
}
#endif /* CONFIG_ENABLE_SECURE_BOOT */

#endif /* INCLUDED_TEGRABL_CRYPTO_H */
