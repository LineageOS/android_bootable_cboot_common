/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_FASTBOOT_PROTOCOL_H
#define TEGRABL_FASTBOOT_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <tegrabl_error.h>

#define IS_VAR_TYPE(var) (!(memcmp(arg, var, strlen(var))))
#define COPY_RESPONSE(msg)									\
	do {													\
		strncpy(response + strlen(response), msg,			\
				MAX_RESPONSE_SIZE - strlen(response) - 1);		\
		*(response + strlen(response)) = '\0';					\
	} while (0)

/* TODO:: max download size needs to come from  uncached ram*/
/* currently  max-download size is 32mb */
#define MAX_DOWNLOAD_SIZE 0x20000000
#define MAX_RESPONSE_SIZE 64
#define STATE_OFFLINE   0
#define STATE_COMMAND   1
#define STATE_COMPLETE  2
#define STATE_ERROR 3
#define USB_BUFFER_ALIGNMENT 4096
#define FASTBOOT_CMD_RCV_BUF_SIZE 512

/* Max timeout for any USB transfer is set to 5 sec */
#define FB_TFR_TIMEOUT  5000

/* macro fastboot lockcmd */
typedef uint32_t fastboot_lockcmd_t;
#define FASTBOOT_LOCK 0
#define FASTBOOT_UNLOCK 1

/**
 * @brief register fastboot commands
 */
void tegrabl_fastboot_cmd_init(void);

/**
 * @brief receive fastboot commands from host and execute them
 *
 * @return TEGRABL_NO_ERROR if success, error code if fails
 */
tegrabl_error_t tegrabl_fastboot_command_handler(void);

/**
 * @brief send ackowledgement message to fastboot host
 */
void fastboot_ack(const char *code, const char *reason);

/**
 * @brief send success message to fastboot host
 */
void fastboot_okay(const char *info);

/**
 * @brief send failure message to fastboot host
 */
void fastboot_fail(const char *info);

#endif
