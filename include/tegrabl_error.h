/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_TEGRABL_ERROR_H
#define INCLUDED_TEGRABL_ERROR_H

/* Format of error codes will be as follows:
 *
 * |-----------------------|----------------------|----------------|-----------|
 * | HIGHEST LAYER MODULE  | LOWEST LAYER MODULE  | AUXILIARY INFO |   REASON  |
 * |-----------------------|----------------------|----------------|-----------|
 *  31                   23 22                  14 13            08 07       00
 *
 * Lowest layer is the layer where error has occurred in module stack.
 *
 * Module in which error is originated should fill all fields as per the error
 * condition. Upper layer modules will update only highest layer module field
 * on error return from low layer module. Auxiliary info field will be
 * module specific which will be used for debugging purpose.
 *
 */

#include <lib/tegrabl_utils.h>
#include <tegrabl_compiler.h>

/* Note: Update TEGRABL_ERR_MODULE_MAX if you change TEGRABL_ERR_MODULE_WIDTH */
#define TEGRABL_ERR_MODULE_WIDTH 9
#define TEGRABL_ERR_AUX_INFO_WIDTH 6
/* Note: Update TEGRABL_ERR_REASON_MAX if you change TEGRABL_ERR_REASON_WIDTH */
#define TEGRABL_ERR_REASON_WIDTH 8
#define TEGRABL_ERR_HIGHEST_MODULE_SHIFT 23
#define TEGRABL_ERR_LOWEST_MODULE_SHIFT 14
#define TEGRABL_ERR_AUX_INFO_SHIFT 8
#define TEGRABL_ERR_REASON_SHIFT 0

#define TEGRABL_ERROR_REASON(error) \
	((error) & BITFIELD_ONES(TEGRABL_ERR_REASON_WIDTH))

#define TEGRABL_ERROR_AUX_INFO(error)					\
	(((error) >> TEGRABL_ERR_AUX_INFO_SHIFT) &			\
	 (BITFIELD_ONES(TEGRABL_ERR_AUX_INFO_WIDTH)))

#define TEGRABL_ERROR_MODULE(error)						\
	(((error) >> TEGRABL_ERR_LOWEST_MODULE_SHIFT) &		\
	 (BITFIELD_ONES(TEGRABL_ERR_MODULE_WIDTH)))

#define TEGRABL_ERROR_HIGHEST_MODULE(error)					\
	(((error) >> TEGRABL_ERR_HIGHEST_MODULE_SHIFT) &			\
	 (BITFIELD_ONES(TEGRABL_ERR_MODULE_WIDTH)))

#define TEGRABL_ERROR_PRINT(error)							\
	pr_critical("ERROR: Highest Layer Module = 0x%x, "		\
			"Lowest Layer Module = 0x%x,\n"					\
			"Aux Info = 0x%x, Reason = 0x%x\n",				\
			TEGRABL_ERROR_HIGHEST_MODULE(error),			\
			TEGRABL_ERROR_MODULE(error),					\
			TEGRABL_ERROR_AUX_INFO(error),					\
			TEGRABL_ERROR_REASON(error));

#ifdef MODULE
#define TEGRABL_ERROR_CHECK(expr, ...)										\
	do {																	\
		error = (expr);														\
		if (TEGRABL_ERROR_REASON(error) != TEGRABL_NO_ERROR) {				\
			__VA_ARGS__;													\
			if (MODULE != 0) {												\
				error = tegrabl_err_set_highest_module(error, MODULE);		\
			}																\
			goto fail;														\
		}																	\
	} while(0)

#define TEGRABL_ERROR(reason, aux_info)	\
		tegrabl_error_value(MODULE, aux_info, reason)

#define TEGRABL_SET_HIGHEST_MODULE(error) \
	{ \
		error = tegrabl_err_set_highest_module(error, MODULE); \
	}

#define TEGRABL_SET_AUX_INFO(error, aux_info) \
	{ \
		error = tegrabl_err_set_aux_info(error, aux_info); \
	}

#endif


typedef uint32_t tegrabl_error_t;

#define TEGRABL_NO_ERROR 0
/**
 * @brief Defines the list of errors that could happen.
 */

typedef enum {
	TEGRABL_ERR_NOT_SUPPORTED = 0x1,
	TEGRABL_ERR_INVALID = 0x2,
	TEGRABL_ERR_NO_MEMORY = 0x3,
	TEGRABL_ERR_OVERFLOW = 0x4,
	TEGRABL_ERR_UNDERFLOW = 0x5,
	TEGRABL_ERR_TIMEOUT = 0x6,
	TEGRABL_ERR_TOO_LARGE = 0x7,
	TEGRABL_ERR_TOO_SMALL = 0x8,
	TEGRABL_ERR_BAD_ADDRESS = 0x9,
	TEGRABL_ERR_NAME_TOO_LONG = 0xa,
	TEGRABL_ERR_OUT_OF_RANGE = 0xb,
	TEGRABL_ERR_NO_ACCESS = 0xc,
	TEGRABL_ERR_NOT_FOUND = 0xd,
	TEGRABL_ERR_BUSY = 0xe,
	TEGRABL_ERR_HALT = 0xf,
	TEGRABL_ERR_LOCK_FAILED = 0x10,
	TEGRABL_ERR_OPEN_FAILED = 0x11,
	TEGRABL_ERR_OPEN_TIMEOUT = 0x12,
	TEGRABL_ERR_INIT_FAILED = 0x13,
	TEGRABL_ERR_INIT_TIMEOUT = 0x14,
	TEGRABL_ERR_RESET_FAILED = 0x15,
	TEGRABL_ERR_RESET_TIMEOUT = 0x16,
	TEGRABL_ERR_NOT_STARTED = 0x17,
	TEGRABL_ERR_INVALID_STATE = 0x18,
	TEGRABL_ERR_UNKNOWN_COMMAND = 0x19,
	TEGRABL_ERR_COMMAND_FAILED = 0x1a,
	TEGRABL_ERR_COMMAND_TIMEOUT = 0x1b,
	TEGRABL_ERR_VERIFY_FAILED = 0x1c,
	TEGRABL_ERR_READ_FAILED = 0x1d,
	TEGRABL_ERR_READ_TIMEOUT = 0x1e,
	TEGRABL_ERR_WRITE_FAILED = 0x1f,
	TEGRABL_ERR_WRITE_TIMEOUT = 0x20,
	TEGRABL_ERR_ERASE_FAILED = 0x21,
	TEGRABL_ERR_ERASE_TIMEOUT = 0x22,
	TEGRABL_ERR_INVALID_CONFIG = 0x23,
	TEGRABL_ERR_INVALID_TOKEN = 0x24,
	TEGRABL_ERR_INVALID_VERSION = 0x25,
	TEGRABL_ERR_OUT_OF_SEQUENCE = 0x26,
	TEGRABL_ERR_NOT_INITIALIZED = 0x27,
	TEGRABL_ERR_ALREADY_EXISTS = 0x28,
	TEGRABL_ERR_INVALID_XFER_SIZE = 0x29,
	TEGRABL_ERR_INVALID_CHANNEL_NUM = 0x2a,
	TEGRABL_ERR_CHANNEL_BUSY = 0x2b,
	TEGRABL_ERR_PORT_ERROR = 0x2c,
	TEGRABL_ERR_DT_NODE_NOT_FOUND = 0x2d,
	TEGRABL_ERR_DT_PROP_NOT_FOUND = 0x2e,
	TEGRABL_ERR_DT_NODE_ADD_FAILED = 0x2f,
	TEGRABL_ERR_DT_PROP_ADD_FAILED = 0x30,
	TEGRABL_ERR_DT_EXPAND_FAILED = 0x31,
	TEGRABL_ERR_BAD_PARAMETER = 0x32,
	TEGRABL_ERR_SOURCE_NOT_STARTED = 0x33,
	TEGRABL_ERR_NOT_CONNECTED = 0x34,
	TEGRABL_ERR_RESOURCE_MAX = 0x35,
	TEGRABL_ERR_DME_COMMAND = 0x36,
	TEGRABL_ERR_FATAL = 0x37,
	TEGRABL_ERR_CHECK_CONDITION = 0x38,
	TEGRABL_ERR_UNKNOWN_SCSI_STATUS = 0x39,
	TEGRABL_ERR_RPMB_AUTH_KEY_NOT_PROGRAMMED = 0x3a,

	TEGRABL_ERR_XUSB_DEV_NOT_ATTACHED = 0x3b,
	TEGRABL_ERR_XUSB_PORT_RESET_FAILED = 0x3c,
	TEGRABL_ERR_XUSB_EP_STALL = 0x3d,
	TEGRABL_ERR_XUSB_EP_NOT_READY = 0x3e,
	TEGRABL_ERR_XUSB_EP_ERR = 0x3f,
	TEGRABL_ERR_XUSB_DEV_RESPONSE_ERR = 0x40,
	TEGRABL_ERR_XUSB_RETRY = 0x41,
	TEGRABL_ERR_XUSB_COMMAND_FAIL = 0x42,

	TEGRABL_ERR_REASON_MAX = 0x100,
} tegrabl_err_reason_t;

/**
 * @brief Defines different modules
 */
typedef enum {
	TEGRABL_ERR_NO_MODULE = 0x0,
	TEGRABL_ERR_TEGRABCT = 0x1,
	TEGRABL_ERR_TEGRASIGN = 0x2,
	TEGRABL_ERR_TEGRARCM = 0x3,
	TEGRABL_ERR_TEGRADEVFLASH = 0x4,
	TEGRABL_ERR_TEGRAHOST = 0x5,
	TEGRABL_ERR_ARGPARSER = 0x6,
	TEGRABL_ERR_XMLPARSER = 0x7,
	TEGRABL_ERR_BCTPARSER = 0x8,
	TEGRABL_ERR_BRBCT = 0x9,
	TEGRABL_ERR_MB1BCT = 0xa,
	TEGRABL_ERR_BRBIT = 0xb,
	TEGRABL_ERR_FILE_MANAGER = 0xc,
	TEGRABL_ERR_PARTITION_MANAGER = 0xd,
	TEGRABL_ERR_BLOCK_DEV = 0xe,
	TEGRABL_ERR_SDMMC = 0xf,
	TEGRABL_ERR_SATA = 0x10,
	TEGRABL_ERR_SPI_FLASH = 0x11,
	TEGRABL_ERR_SPI = 0x12,
	TEGRABL_ERR_GPCDMA = 0x13,
	TEGRABL_ERR_BPMP_FW = 0x14,
	TEGRABL_ERR_SE_CRYPTO = 0x15,
	TEGRABL_ERR_SW_CRYPTO = 0x16,
	TEGRABL_ERR_NV3P = 0x17,
	TEGRABL_ERR_FASTBOOT = 0x18,
	TEGRABL_ERR_OTA = 0x19,
	TEGRABL_ERR_HEAP = 0x1a,
	TEGRABL_ERR_PAGE_ALLOCATOR = 0x1b,
	TEGRABL_ERR_GPT = 0x1c,
	TEGRABL_ERR_LOADER = 0x1d,
	TEGRABL_ERR_SOCMISC = 0x1e,
	TEGRABL_ERR_CARVEOUT = 0x1f,
	TEGRABL_ERR_UART = 0x20,
	TEGRABL_ERR_CONSOLE = 0x21,
	TEGRABL_ERR_DEBUG = 0x22,
	TEGRABL_ERR_TOS = 0x23,
	TEGRABL_ERR_MB2_PARAMS = 0x24,
	TEGRABL_ERR_SPE_CAN = 0x25,
	TEGRABL_ERR_I2C = 0x26,
	TEGRABL_ERR_I2C_DEV = 0x27,
	TEGRABL_ERR_I2C_DEV_BASIC = 0x28,
	TEGRABL_ERR_FUSE = 0x29,
	TEGRABL_ERR_TRANSPORT = 0x2a,
	TEGRABL_ERR_LINUXBOOT = 0x2b,
	TEGRABL_ERR_MB1_PLATFORM_CONFIG = 0x2c,
	TEGRABL_ERR_MB1_BCT_LAYOUT = 0x2d,
	TEGRABL_ERR_WARMBOOT = 0x2e,
	TEGRABL_ERR_XUSBF = 0x2f,
	TEGRABL_ERR_CLK_RST = 0x30,
	TEGRABL_ERR_FUSE_BYPASS = 0x31,
	TEGRABL_ERR_CPUINIT = 0x32,
	TEGRABL_ERR_SPARSE = 0x33,
	TEGRABL_ERR_NVDEC = 0x34,
	TEGRABL_ERR_EEPROM_MANAGER = 0x35,
	TEGRABL_ERR_EEPROM = 0x36,
	TEGRABL_ERR_POWER = 0x37,
	TEGRABL_ERR_SCE = 0x38,
	TEGRABL_ERR_APE = 0x39,
	TEGRABL_ERR_MB1_WARS = 0x3a,
	TEGRABL_ERR_UPHY = 0x3b,
	TEGRABL_ERR_AOTAG = 0x3c,
	TEGRABL_ERR_DRAM_ECC = 0x3d,
	TEGRABL_ERR_NVPT = 0x3e,
	TEGRABL_ERR_AST = 0x3f,
	TEGRABL_ERR_AUTH = 0x40,
	TEGRABL_ERR_PWM = 0x41,
	TEGRABL_ERR_ROLLBACK = 0x42,
	TEGRABL_ERR_NCT = 0x43,
	TEGRABL_ERR_VERIFIED_BOOT = 0x44,
	TEGRABL_ERR_PKC_OP = 0x45,
	TEGRABL_ERR_DISPLAY = 0x46,
	TEGRABL_ERR_GRAPHICS = 0x47,
	TEGRABL_ERR_NVDISP = 0x48,
	TEGRABL_ERR_DSI = 0x49,
	TEGRABL_ERR_HDMI = 0x4a,
	TEGRABL_ERR_DPAUX = 0x4b,
	TEGRABL_ERR_BOARD_INFO = 0x4c,
	TEGRABL_ERR_GPIO = 0x4d,
	TEGRABL_ERR_KEYBOARD = 0x4e,
	TEGRABL_ERR_MENU = 0x4f,
	TEGRABL_ERR_ANDROIDBOOT = 0x50,
	TEGRABL_ERR_PANEL = 0x51,
	TEGRABL_ERR_NVBLOB = 0x52,
	TEGRABL_ERR_EXIT = 0x53,
	TEGRABL_ERR_AB_BOOTCTRL = 0x54,
	TEGRABL_ERR_FRP = 0x55,
	TEGRABL_ERR_PMIC = 0x56,
	TEGRABL_ERR_REGULATOR = 0x57,
	TEGRABL_ERR_PWM_BASIC = 0x58,
	TEGRABL_ERR_BOOTLOADER_UPDATE = 0x59,
	TEGRABL_ERR_UFS = 0x5a,
	TEGRABL_ERR_RATCHET = 0x5b,
	TEGRABL_ERR_DEVICETREE = 0x5c,
	TEGRABL_ERR_SECURITY = 0x5d,
	TEGRABL_ERR_ROLLBACK_PREVENTION = 0x5e,
	TEGRABL_ERR_CARVEOUT_MAPPER = 0x5f,
	TEGRABL_ERR_KEYSLOT = 0x60,
	TEGRABL_ERR_DP = 0x61,
	TEGRABL_ERR_SOR = 0x62,
	TEGRABL_ERR_RAMDUMP = 0x64,
	TEGRABL_ERR_STORAGE = 0x65,
	TEGRABL_ERR_PCI = 0x66,
	TEGRABL_ERR_XUSB_HOST = 0x67,

	TEGRABL_ERR_MODULE_MAX = 0x200,
} tegrabl_err_module_t;

/**
 * @brief Creates the error value as per the format
 *
 * @param module	Module with error.
 * @param aux_info	Auxiliary information.
 * @param reason	Error reason.
 *
 * @return error value as per format.
 */
static TEGRABL_INLINE tegrabl_error_t tegrabl_error_value(tegrabl_err_module_t module,
		uint8_t aux_info, tegrabl_err_reason_t reason)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	BITFIELD_SET(error, module, TEGRABL_ERR_MODULE_WIDTH,
					TEGRABL_ERR_HIGHEST_MODULE_SHIFT);
	BITFIELD_SET(error, module, TEGRABL_ERR_MODULE_WIDTH,
					TEGRABL_ERR_LOWEST_MODULE_SHIFT);
	BITFIELD_SET(error, aux_info, TEGRABL_ERR_AUX_INFO_WIDTH,
					TEGRABL_ERR_AUX_INFO_SHIFT);
	BITFIELD_SET(error, reason, TEGRABL_ERR_REASON_WIDTH,
					TEGRABL_ERR_REASON_SHIFT);

	return error;
}

/**
 * @brief Sets the highest layer module field
 *
 * @param error 	Error value in which highest layer module field to be set
 * @param module 	Module to be set
 *
 * @return new error value.
 */
static TEGRABL_INLINE tegrabl_error_t tegrabl_err_set_highest_module(
		tegrabl_error_t error, tegrabl_err_module_t module)
{
	BITFIELD_SET(error, module, TEGRABL_ERR_MODULE_WIDTH,
					TEGRABL_ERR_HIGHEST_MODULE_SHIFT);
	return error;
}

/**
 * @brief Sets the aux info field
 *
 * @param error Error value in which aux info field to be set
 * @param module aux info to be set
 *
 * @return new error value.
 */
static TEGRABL_INLINE tegrabl_error_t tegrabl_err_set_aux_info(
		tegrabl_error_t error, uint8_t aux_info)
{
	BITFIELD_SET(error, aux_info, TEGRABL_ERR_AUX_INFO_WIDTH,
					TEGRABL_ERR_AUX_INFO_SHIFT);
	return error;
}

#endif  /*  INCLUDED_TEGRABL_ERROR_H */
