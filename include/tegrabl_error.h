/*
 * Copyright (c) 2015-2021, NVIDIA CORPORATION.  All rights reserved.
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
 *  31                   24 23                  16 15            08 07       00
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
#define TEGRABL_ERR_MODULE_WIDTH 8
#define TEGRABL_ERR_AUX_INFO_WIDTH 8
/* Note: Update TEGRABL_ERR_REASON_MAX if you change TEGRABL_ERR_REASON_WIDTH */
#define TEGRABL_ERR_REASON_WIDTH 8
#define TEGRABL_ERR_HIGHEST_MODULE_SHIFT 24
#define TEGRABL_ERR_LOWEST_MODULE_SHIFT 16
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
	tegrabl_error_print_error(error)

#ifdef MODULE
#define TEGRABL_ERROR(reason, aux_info)	\
		tegrabl_error_value(MODULE, aux_info, reason)

#define TEGRABL_SET_HIGHEST_MODULE(error) \
	{ \
		(error) = tegrabl_err_set_highest_module(error, MODULE); \
	}

#define TEGRABL_SET_AUX_INFO(error, aux_info) \
	{ \
		(error) = tegrabl_err_set_aux_info(error, aux_info); \
	}

#endif

#define TEGRABL_SET_CRITICAL_STRING(err, ...) \
	tegrabl_error_print(true, TEGRABL_LOG_CRITICAL, err, ##__VA_ARGS__)

#define TEGRABL_SET_ERROR_STRING(err, ...) \
	tegrabl_error_print(true, TEGRABL_LOG_ERROR, err, ##__VA_ARGS__)

#define TEGRABL_PRINT_CRITICAL_STRING(err, ...) \
	tegrabl_error_print(false, TEGRABL_LOG_CRITICAL, \
			tegrabl_err_set_lowest_module(err, MODULE), ##__VA_ARGS__)

#define TEGRABL_PRINT_ERROR_STRING(err, ...) \
	tegrabl_error_print(false, TEGRABL_LOG_ERROR, \
			tegrabl_err_set_lowest_module(err, MODULE), ##__VA_ARGS__)

#define TEGRABL_PRINT_INFO_STRING(err, ...) \
	tegrabl_error_print(false, TEGRABL_LOG_INFO, \
			tegrabl_err_set_lowest_module(err, MODULE), ##__VA_ARGS__)

#define TEGRABL_PRINT_WARN_STRING(err, ...) \
	tegrabl_error_print(false, TEGRABL_LOG_WARN, \
			tegrabl_err_set_lowest_module(err, MODULE), ##__VA_ARGS__)

#define TEGRABL_PRINT_DEBUG_STRING(err, ...) \
	tegrabl_error_print(false, TEGRABL_LOG_DEBUG, \
			tegrabl_err_set_lowest_module(err, MODULE), ##__VA_ARGS__)

typedef uint32_t tegrabl_error_t;

#define TEGRABL_NO_ERROR 0U
/**
 * @brief Defines the list of errors that could happen.
 */

/* macro tegrabl err */
#define TEGRABL_ERR_NOT_SUPPORTED 0x01U
#define TEGRABL_ERR_INVALID 0x02U
#define TEGRABL_ERR_NO_MEMORY 0x03U
#define TEGRABL_ERR_OVERFLOW 0x04U
#define TEGRABL_ERR_UNDERFLOW 0x05U
#define TEGRABL_ERR_TIMEOUT 0x06U
#define TEGRABL_ERR_TOO_LARGE 0x07U
#define TEGRABL_ERR_TOO_SMALL 0x08U
#define TEGRABL_ERR_BAD_ADDRESS 0x09U
#define TEGRABL_ERR_NAME_TOO_LONG 0x0aU
#define TEGRABL_ERR_OUT_OF_RANGE 0x0bU
#define TEGRABL_ERR_NO_ACCESS 0x0cU
#define TEGRABL_ERR_NOT_FOUND 0x0dU
#define TEGRABL_ERR_BUSY 0x0eU
#define TEGRABL_ERR_HALT 0x0fU
#define TEGRABL_ERR_LOCK_FAILED 0x10U
#define TEGRABL_ERR_OPEN_FAILED 0x11U
#define TEGRABL_ERR_INIT_FAILED 0x12U
#define TEGRABL_ERR_RESET_FAILED 0x13U
#define TEGRABL_ERR_NOT_STARTED 0x14U
#define TEGRABL_ERR_INVALID_STATE 0x15U
#define TEGRABL_ERR_UNKNOWN_COMMAND 0x16U
#define TEGRABL_ERR_COMMAND_FAILED 0x17U
#define TEGRABL_ERR_VERIFY_FAILED 0x18U
#define TEGRABL_ERR_READ_FAILED 0x19U
#define TEGRABL_ERR_WRITE_FAILED 0x1aU
#define TEGRABL_ERR_ERASE_FAILED 0x1bU
#define TEGRABL_ERR_INVALID_CONFIG 0x1cU
#define TEGRABL_ERR_INVALID_TOKEN 0x1dU
#define TEGRABL_ERR_INVALID_VERSION 0x1eU
#define TEGRABL_ERR_OUT_OF_SEQUENCE 0x1fU
#define TEGRABL_ERR_NOT_INITIALIZED 0x20U
#define TEGRABL_ERR_ALREADY_EXISTS 0x21U
#define TEGRABL_ERR_ADD_FAILED 0x22U
#define TEGRABL_ERR_DEL_FAILED 0x23U
#define TEGRABL_ERR_SET_FAILED 0x24U
#define TEGRABL_ERR_EXPAND_FAILED 0x25U
#define TEGRABL_ERR_NOT_CONNECTED 0x26U
#define TEGRABL_ERR_NO_RESOURCE 0x27U
#define TEGRABL_ERR_EMPTY 0x28U
#define TEGRABL_ERR_STOP_FAILED 0x29U
#define TEGRABL_ERR_BAD_PARAMETER 0x2aU
#define TEGRABL_ERR_FATAL 0x2bU
#define TEGRABL_ERR_NOT_PROGRAMMED 0x2cU
#define TEGRABL_ERR_UNKNOWN_STATUS 0x2dU
#define TEGRABL_ERR_CONDITION 0x2eU
#define TEGRABL_ERR_XFER_IN_PROGRESS 0x2fU
#define TEGRABL_ERR_REGISTER_FAILED 0x30U
#define TEGRABL_ERR_NOT_DETECTED 0x31U
#define TEGRABL_ERR_CLEAR_FAILED 0x32U
#define TEGRABL_ERR_SEND_FAILED 0x33U
#define TEGRABL_ERR_XFER_FAILED 0x34U
#define TEGRABL_ERR_UPDATE_FAILED 0x35U
#define TEGRABL_ERR_RECEIVE_FAILED 0x36U
#define TEGRABL_ERR_MISMATCH 0x37U
#define TEGRABL_ERR_NACK 0x38U
#define TEGRABL_ERR_FAILED_GENERIC 0x39U
#define TEGRABL_ERR_GET_FAILED 0x3aU
#define TEGRABL_ERR_MAP_FAILED 0x3bU
#define TEGRABL_ERR_COMPUTE_FAILED 0x3cU
#define TEGRABL_ERR_PARSE_FAILED 0x3dU
#define TEGRABL_ERR_CONFIG_FAILED 0x3eU
#define TEGRABL_ERR_ENABLE_FAILED 0x3fU
#define TEGRABL_ERR_DISABLE_FAILED 0x40U
#define TEGRABL_ERR_CALIBRATE_FAILED 0x41U
#define TEGRABL_ERR_INVALID_MODE 0x42U
#define TEGRABL_ERR_NOT_READY 0x43U
#define TEGRABL_ERR_CP_MV_FAILED 0x44U
#define TEGRABL_ERR_NOT_ALIGNED 0x45U
 /**** This should be last *******/
#define TEGRABL_ERR_REASON_END 0x46U
#define TEGRABL_ERR_REASON_MAX 0xffU

typedef uint32_t tegrabl_err_reason_t;

/**
 * @brief Defines different modules
 */
/*macro tegrabl_err_module*/
/* After adding new err module, please also add corresponding err module string in
 * $TOP/bootloader/partner/common/lib/tegrabl_error/tegrabl_error.c
 */
#define TEGRABL_ERR_NO_MODULE 0x0U
#define TEGRABL_ERR_TEGRABCT 0x1U
#define TEGRABL_ERR_TEGRASIGN 0x2U
#define TEGRABL_ERR_TEGRARCM 0x3U
#define TEGRABL_ERR_TEGRADEVFLASH 0x4U
#define TEGRABL_ERR_TEGRAHOST 0x5U
#define TEGRABL_ERR_ARGPARSER 0x6U
#define TEGRABL_ERR_XMLPARSER 0x7U
#define TEGRABL_ERR_BCTPARSER 0x8U
#define TEGRABL_ERR_BRBCT 0x9U
#define TEGRABL_ERR_MB1BCT 0xaU
#define TEGRABL_ERR_BRBIT 0xbU
#define TEGRABL_ERR_FILE_MANAGER 0xcU
#define TEGRABL_ERR_PARTITION_MANAGER 0xdU
#define TEGRABL_ERR_BLOCK_DEV 0xeU
#define TEGRABL_ERR_SDMMC 0xfU
#define TEGRABL_ERR_SATA 0x10U
#define TEGRABL_ERR_SPI_FLASH 0x11U
#define TEGRABL_ERR_SPI 0x12U
#define TEGRABL_ERR_GPCDMA 0x13U
#define TEGRABL_ERR_BPMP_FW 0x14U
#define TEGRABL_ERR_SE_CRYPTO 0x15U
#define TEGRABL_ERR_SW_CRYPTO 0x16U
#define TEGRABL_ERR_NV3P 0x17U
#define TEGRABL_ERR_FASTBOOT 0x18U
#define TEGRABL_ERR_OTA 0x19U
#define TEGRABL_ERR_HEAP 0x1aU
#define TEGRABL_ERR_PAGE_ALLOCATOR 0x1bU
#define TEGRABL_ERR_GPT 0x1cU
#define TEGRABL_ERR_LOADER 0x1dU
#define TEGRABL_ERR_SOCMISC 0x1eU
#define TEGRABL_ERR_CARVEOUT 0x1fU
#define TEGRABL_ERR_UART 0x20U
#define TEGRABL_ERR_CONSOLE 0x21U
#define TEGRABL_ERR_DEBUG 0x22U
#define TEGRABL_ERR_TOS 0x23U
#define TEGRABL_ERR_MB2_PARAMS 0x24U
#define TEGRABL_ERR_AON 0x25U
#define TEGRABL_ERR_I2C 0x26U
#define TEGRABL_ERR_I2C_DEV 0x27U
#define TEGRABL_ERR_I2C_DEV_BASIC 0x28U
#define TEGRABL_ERR_FUSE 0x29U
#define TEGRABL_ERR_TRANSPORT 0x2aU
#define TEGRABL_ERR_LINUXBOOT 0x2bU
#define TEGRABL_ERR_MB1_PLATFORM_CONFIG 0x2cU
#define TEGRABL_ERR_MB1_BCT_LAYOUT 0x2dU
#define TEGRABL_ERR_WARMBOOT 0x2eU
#define TEGRABL_ERR_XUSBF 0x2fU
#define TEGRABL_ERR_CLK_RST 0x30U
#define TEGRABL_ERR_FUSE_BYPASS 0x31U
#define TEGRABL_ERR_CPUINIT 0x32U
#define TEGRABL_ERR_SPARSE 0x33U
#define TEGRABL_ERR_NVDEC 0x34U
#define TEGRABL_ERR_EEPROM_MANAGER 0x35U
#define TEGRABL_ERR_EEPROM 0x36U
#define TEGRABL_ERR_POWER 0x37U
#define TEGRABL_ERR_SCE 0x38U
#define TEGRABL_ERR_APE 0x39U
#define TEGRABL_ERR_MB1_WARS 0x3aU
#define TEGRABL_ERR_UPHY 0x3bU
#define TEGRABL_ERR_AOTAG 0x3cU
#define TEGRABL_ERR_DRAM_ECC 0x3dU
#define TEGRABL_ERR_NVPT 0x3eU
#define TEGRABL_ERR_AST 0x3fU
#define TEGRABL_ERR_AUTH 0x40U
#define TEGRABL_ERR_PWM 0x41U
#define TEGRABL_ERR_ROLLBACK 0x42U
#define TEGRABL_ERR_NCT 0x43U
#define TEGRABL_ERR_VERIFIED_BOOT 0x44U
#define TEGRABL_ERR_PKC_OP 0x45U
#define TEGRABL_ERR_DISPLAY 0x46U
#define TEGRABL_ERR_GRAPHICS 0x47U
#define TEGRABL_ERR_NVDISP 0x48U
#define TEGRABL_ERR_DSI 0x49U
#define TEGRABL_ERR_HDMI 0x4aU
#define TEGRABL_ERR_DPAUX 0x4bU
#define TEGRABL_ERR_BOARD_INFO 0x4cU
#define TEGRABL_ERR_GPIO 0x4dU
#define TEGRABL_ERR_KEYBOARD 0x4eU
#define TEGRABL_ERR_MENU 0x4fU
#define TEGRABL_ERR_KERNELBOOT 0x50U
#define TEGRABL_ERR_PANEL 0x51U
#define TEGRABL_ERR_NVBLOB 0x52U
#define TEGRABL_ERR_EXIT 0x53U
#define TEGRABL_ERR_AB_BOOTCTRL 0x54U
#define TEGRABL_ERR_FRP 0x55U
#define TEGRABL_ERR_PMIC 0x56U
#define TEGRABL_ERR_REGULATOR 0x57U
#define TEGRABL_ERR_PWM_BASIC 0x58U
#define TEGRABL_ERR_BOOTLOADER_UPDATE 0x59U
#define TEGRABL_ERR_UFS 0x5aU
#define TEGRABL_ERR_RATCHET 0x5bU
#define TEGRABL_ERR_DEVICETREE 0x5cU
#define TEGRABL_ERR_SECURITY 0x5dU
#define TEGRABL_ERR_ROLLBACK_PREVENTION 0x5eU
#define TEGRABL_ERR_CARVEOUT_MAPPER 0x5fU
#define TEGRABL_ERR_KEYSLOT 0x60U
#define TEGRABL_ERR_DP 0x61U
#define TEGRABL_ERR_SOR 0x62U
#define TEGRABL_ERR_DISPLAY_PDATA 0x63U
#define TEGRABL_ERR_RAMDUMP 0x64U
#define TEGRABL_ERR_PLUGIN_MANAGER 0x65U
#define TEGRABL_ERR_STORAGE 0x66U
#define TEGRABL_ERR_TESTS 0x67U
#define TEGRABL_ERR_MPHY 0x68U
#define TEGRABL_ERR_SUBCARVEOUT 0x69U
#define TEGRABL_ERR_DECOMPRESS 0x6aU
#define TEGRABL_ERR_PCI 0x6bU
#define TEGRABL_ERR_MAPPER 0x6cU
#define TEGRABL_ERR_COMBINED_UART 0x6dU
#define TEGRABL_ERR_RCE 0x6eU
#define TEGRABL_ERR_IST 0x6fU
#define TEGRABL_ERR_QUAL_ENGINE 0x70U
#define TEGRABL_ERR_SOFT_FUSE 0x71U
#define TEGRABL_ERR_MPU 0x72U
#define TEGRABL_ERR_ODM_DATA 0x73U
#define TEGRABL_ERR_NV3P_SERVER 0x74U
#define TEGRABL_ERR_CCG 0x75U
#define TEGRABL_ERR_GR_SUPPORT 0x76U
#define TEGRABL_ERR_EQOS 0x77U
#define TEGRABL_ERR_PHY 0x78U
#define TEGRABL_ERR_USBH 0x79U
#define TEGRABL_ERR_DEVICE_PROD 0x7AU
#define TEGRABL_ERR_CONFIG_STORAGE 0x7BU
#define TEGRABL_ERR_USBMSD 0x7CU
#define TEGRABL_ERR_CBO 0x7DU
#define TEGRABL_ERR_SHELL 0x7EU
#define TEGRABL_ERR_PCIE 0x7FU
#define TEGRABL_ERR_NVME 0x80U

/**** This should be last ****/
#define TEGRABL_ERR_MODULE_END 0x81U
#define TEGRABL_ERR_MODULE_MAX 0xffU

typedef uint32_t tegrabl_err_module_t;

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

	BITFIELD_SET(error, module, TEGRABL_ERR_MODULE_WIDTH, TEGRABL_ERR_HIGHEST_MODULE_SHIFT);
	BITFIELD_SET(error, module, TEGRABL_ERR_MODULE_WIDTH, TEGRABL_ERR_LOWEST_MODULE_SHIFT);
	BITFIELD_SET(error, aux_info, TEGRABL_ERR_AUX_INFO_WIDTH, TEGRABL_ERR_AUX_INFO_SHIFT);
	BITFIELD_SET(error, reason, TEGRABL_ERR_REASON_WIDTH, TEGRABL_ERR_REASON_SHIFT);

	return error;
}

/**
 * @brief Sets the highest layer module field
 *
 * @param error		Error value in which highest layer module field to be set
 * @param module	Module to be set
 *
 * @return new error value.
 */
static TEGRABL_INLINE tegrabl_error_t tegrabl_err_set_highest_module(
		tegrabl_error_t error, tegrabl_err_module_t module)
{
	BITFIELD_SET(error, module, TEGRABL_ERR_MODULE_WIDTH, TEGRABL_ERR_HIGHEST_MODULE_SHIFT);
	return error;
}

/**
 * @brief Sets the lowest layer module field
 *
 * @param error		Error value in which lowest layer module field to be set
 * @param module	Module to be set
 *
 * @return new error value.
 */
static TEGRABL_INLINE tegrabl_error_t tegrabl_err_set_lowest_module(
		tegrabl_error_t error, tegrabl_err_module_t module)
{
	BITFIELD_SET(error, module, TEGRABL_ERR_MODULE_WIDTH, TEGRABL_ERR_LOWEST_MODULE_SHIFT);
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
	BITFIELD_SET(error, aux_info, TEGRABL_ERR_AUX_INFO_WIDTH, TEGRABL_ERR_AUX_INFO_SHIFT);
	return error;
}

/**
 * @brief Prints error messages as per pre-defined string.
 * For string see tegrabl_error_strings.h.
 *
 * @param set_first_error True if want to set first error string pointer
 * @param level log level of error print
 * @param error error value
 * @param ... variable arguments.
 */
void tegrabl_error_print(bool set_first_error, uint32_t level, tegrabl_error_t error, ...);

/**
 * @brief Dissects error and prints each part.
 *
 * @param error error to be printed.
 */
void tegrabl_error_print_error(tegrabl_error_t error);

/**
 * @brief Clears first error string pointer
 */
void tegrabl_error_clear_first_error(void);

/**
 * @brief Retrieve first error string pointer
 *
 * @return NULL if not set else location of first error string
 */
const char *tegrabl_error_get_first_error(void);

/**
 * @brief Returns string of module
 *
 * @param module module type
 *
 * @return string of module
 */
const char *tegrabl_error_module_str(tegrabl_err_module_t module);

/**
 * @brief Prints the assert failure error message
 *
 * @param filename name of the file having failed assert
 * @param line no of the line where assert is added
 */
void print_assert_fail(const char *filename, uint32_t line);

#endif  /*  INCLUDED_TEGRABL_ERROR_H */
