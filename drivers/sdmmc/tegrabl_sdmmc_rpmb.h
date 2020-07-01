/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_SDMMC_RPMB_H
#define TEGRABL_SDMMC_RPMB_H

#include <tegrabl_blockdev.h>

/* Defines supported RPMB request types. */
typedef enum {
	RPMB_REQ_PROGRAM_KEY = 1,
	RPMB_REQ_GET_COUNTER = 2,
	RPMB_REQ_WRITE = 3,
	RPMB_REQ_READ = 4,
	RPMB_REQ_GET_RESULT = 5,
} sdmmc_rpmb_req;

/* Defines RPMB response values. */
typedef enum {
	RPMB_RESP_PROGRAM_KEY = 0x0100,
	RPMB_RESP_GET_COUNTER = 0x0200,
	RPMB_RESP_WRITE = 0x0300,
	RPMB_RESP_READ = 0x0400,
} sdmmc_rpmb_resp;

/* Defines RPMB result values. */
typedef enum {
	RPMB_RES_OK = 0x0000,
	RPMB_RES_GENERAL_FAILURE = 0x0001,
	RPMB_RES_AUTH_FAILURE = 0x0002,
	RPMB_RES_COUNT_FAILURE = 0x0003,
	RPMB_RES_ADDR_FAILURE = 0x0004,
	RPMB_RES_WRITE_FAILURE = 0x0005,
	RPMB_RES_READ_FAILURE = 0x0006,
	RPMB_RES_NO_AUTH_KEY = 0x0007,
	RPMB_RES_WRITE_COUNTER_EXPIRED = 0x0080,
} sdmmc_rpmb_result;

/* Defines RPMB frame format. */
#define RPMB_STUFF_SIZE			196
#define RPMB_KEY_OR_MAC_SIZE	32
#define RPMB_DATA_SIZE_LOG2		8
#define RPMB_DATA_SIZE			(1 << RPMB_DATA_SIZE_LOG2)
#define RPMB_NONCE_SIZE			16
/*
 * RPMB generates SHA256 MAC over 284 bytes starting at the first byte of the
 * the data field.
 */
#define RPMB_MAC_MSG_SIZE	284

typedef struct rpmb_u16 {
	uint8_t data[2];
} rpmb_u16_t;

typedef struct rpmb_u32 {
	uint8_t data[4];
} rpmb_u32_t;

typedef struct sdmmc_rpmb_frame {
	uint8_t	stuff[RPMB_STUFF_SIZE];
	uint8_t key_or_mac[RPMB_KEY_OR_MAC_SIZE];
	union {
		uint8_t mac_msg[RPMB_MAC_MSG_SIZE];
		struct {
			uint8_t	data[RPMB_DATA_SIZE];
			uint8_t nonce[RPMB_NONCE_SIZE];
			rpmb_u32_t write_counter;
			rpmb_u16_t address;
			rpmb_u16_t block_count;
			rpmb_u16_t result;
			rpmb_u16_t req_or_resp;
		};
	};
} sdmmc_rpmb_frame_t;

/* Defines format of RPMB key. */
typedef struct sdmmc_rpmb_key {
	uint8_t data[RPMB_KEY_OR_MAC_SIZE];
} sdmmc_rpmb_key_t;

/* Defines buffers for use in RPMB access */
typedef struct rpmb_context {
	sdmmc_rpmb_frame_t req_frame;
	sdmmc_rpmb_frame_t req_resp_frame;
	sdmmc_rpmb_frame_t resp_frame;
} sdmmc_rpmb_context_t;

/* Defined default none value. */
#define RPMB_NONCE	\
	{ 0x01, 0xFF, 0x3E, 0x26, 0x49, 0x67, 0x80, 0x63,		\
			0x99, 0xE1, 0xCB, 0x11, 0xDA, 0xD6, 0x05, 0x00 }


/** @brief Issue RPMB get write counter command.
 *
 *  @param dev           Bio layer handle for RPMB device.
 *  @param key           Pointer to RPMB key.
 *  @param counter       Pointer to buffer in which current counter is returned.
 *  @param rpmb_context  RPMB context.
 *  @param context       Context for device on which RPMB access is desired.
 *
 *  @return TEGRABL_NO_ERROR on success and failure condition otherwise.
 */

tegrabl_error_t sdmmc_rpmb_get_write_counter(tegrabl_bdev_t *dev,
				sdmmc_rpmb_key_t *key,
				uint32_t *counter,
				sdmmc_rpmb_context_t *rpmb_context,
				sdmmc_context_t *context);

/** @brief Issue RPMB program key command.
 *
 *  @param context  Context for device on which key should be programmed.
 *  @param key_blob Pointer to data to use for RPMB key.
 *
 *  @return NO_ERROR on success and failure condition otherwise.
 */
tegrabl_error_t sdmmc_rpmb_program_key(tegrabl_bdev_t *bdev, void *key_blob,
				sdmmc_context_t *context);


#endif /* TEGRABL_SDMMC_RPMB_H */
