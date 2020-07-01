/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "build_config.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <tegrabl_stdarg.h>
#include <tegrabl_error.h>
#include <tegrabl_error_strings.h>
#include <tegrabl_debug.h>

#if defined(CONFIG_ENABLE_LOGLEVEL_RUNTIME)
extern uint32_t tegrabl_debug_loglevel;
#endif

#define ADD_ERROR_STRING(reason) \
	[TEGRABL_ERR_##reason] = TEGRABL_ERR_##reason##_STRING

#define STRING(s) #s

#define ADD_ERROR_MODULE(module) \
	[TEGRABL_ERR_##module] = STRING(module)

static char error_string[256];
const char *first_error;

const char *error_reason_strings[TEGRABL_ERR_REASON_END] = {
	ADD_ERROR_STRING(NOT_SUPPORTED),
	ADD_ERROR_STRING(INVALID),
	ADD_ERROR_STRING(NO_MEMORY),
	ADD_ERROR_STRING(OVERFLOW),
	ADD_ERROR_STRING(UNDERFLOW),
	ADD_ERROR_STRING(TIMEOUT),
	ADD_ERROR_STRING(TOO_LARGE),
	ADD_ERROR_STRING(TOO_SMALL),
	ADD_ERROR_STRING(BAD_ADDRESS),
	ADD_ERROR_STRING(NAME_TOO_LONG),
	ADD_ERROR_STRING(OUT_OF_RANGE),
	ADD_ERROR_STRING(NO_ACCESS),
	ADD_ERROR_STRING(NOT_FOUND),
	ADD_ERROR_STRING(BUSY),
	ADD_ERROR_STRING(HALT),
	ADD_ERROR_STRING(LOCK_FAILED),
	ADD_ERROR_STRING(OPEN_FAILED),
	ADD_ERROR_STRING(INIT_FAILED),
	ADD_ERROR_STRING(RESET_FAILED),
	ADD_ERROR_STRING(NOT_STARTED),
	ADD_ERROR_STRING(INVALID_STATE),
	ADD_ERROR_STRING(UNKNOWN_COMMAND),
	ADD_ERROR_STRING(COMMAND_FAILED),
	ADD_ERROR_STRING(VERIFY_FAILED),
	ADD_ERROR_STRING(READ_FAILED),
	ADD_ERROR_STRING(WRITE_FAILED),
	ADD_ERROR_STRING(ERASE_FAILED),
	ADD_ERROR_STRING(INVALID_CONFIG),
	ADD_ERROR_STRING(INVALID_TOKEN),
	ADD_ERROR_STRING(INVALID_VERSION),
	ADD_ERROR_STRING(OUT_OF_SEQUENCE),
	ADD_ERROR_STRING(NOT_INITIALIZED),
	ADD_ERROR_STRING(ALREADY_EXISTS),
	ADD_ERROR_STRING(ADD_FAILED),
	ADD_ERROR_STRING(DEL_FAILED),
	ADD_ERROR_STRING(SET_FAILED),
	ADD_ERROR_STRING(EXPAND_FAILED),
	ADD_ERROR_STRING(NOT_CONNECTED),
	ADD_ERROR_STRING(NO_RESOURCE),
	ADD_ERROR_STRING(EMPTY),
	ADD_ERROR_STRING(STOP_FAILED),
	ADD_ERROR_STRING(BAD_PARAMETER),
	ADD_ERROR_STRING(FATAL),
	ADD_ERROR_STRING(NOT_PROGRAMMED),
	ADD_ERROR_STRING(UNKNOWN_STATUS),
	ADD_ERROR_STRING(CONDITION),
	ADD_ERROR_STRING(XFER_IN_PROGRESS),
	ADD_ERROR_STRING(REGISTER_FAILED),
	ADD_ERROR_STRING(NOT_DETECTED),
	ADD_ERROR_STRING(CLEAR_FAILED),
	ADD_ERROR_STRING(SEND_FAILED),
	ADD_ERROR_STRING(XFER_FAILED),
	ADD_ERROR_STRING(UPDATE_FAILED),
	ADD_ERROR_STRING(RECEIVE_FAILED),
	ADD_ERROR_STRING(MISMATCH),
	ADD_ERROR_STRING(NACK),
	ADD_ERROR_STRING(FAILED_GENERIC),
	ADD_ERROR_STRING(GET_FAILED),
	ADD_ERROR_STRING(COMPUTE_FAILED),
	ADD_ERROR_STRING(PARSE_FAILED),
	ADD_ERROR_STRING(CONFIG_FAILED),
	ADD_ERROR_STRING(ENABLE_FAILED),
	ADD_ERROR_STRING(DISABLE_FAILED),
	ADD_ERROR_STRING(CALIBRATE_FAILED),
	ADD_ERROR_STRING(INVALID_MODE),
	ADD_ERROR_STRING(NOT_READY),
	ADD_ERROR_STRING(CP_MV_FAILED),
	ADD_ERROR_STRING(NOT_ALIGNED),
};

const char *module_strings[TEGRABL_ERR_MODULE_END] = {
	"NONE",
	ADD_ERROR_MODULE(TEGRABCT),
	ADD_ERROR_MODULE(TEGRASIGN),
	ADD_ERROR_MODULE(TEGRARCM),
	ADD_ERROR_MODULE(TEGRADEVFLASH),
	ADD_ERROR_MODULE(TEGRAHOST),
	ADD_ERROR_MODULE(ARGPARSER),
	ADD_ERROR_MODULE(XMLPARSER),
	ADD_ERROR_MODULE(BCTPARSER),
	ADD_ERROR_MODULE(BRBCT),
	ADD_ERROR_MODULE(MB1BCT),
	ADD_ERROR_MODULE(BRBIT),
	ADD_ERROR_MODULE(FILE_MANAGER),
	ADD_ERROR_MODULE(PARTITION_MANAGER),
	ADD_ERROR_MODULE(BLOCK_DEV),
	ADD_ERROR_MODULE(SDMMC),
	ADD_ERROR_MODULE(SATA),
	ADD_ERROR_MODULE(SPI_FLASH),
	ADD_ERROR_MODULE(SPI),
	ADD_ERROR_MODULE(GPCDMA),
	ADD_ERROR_MODULE(BPMP_FW),
	ADD_ERROR_MODULE(SE_CRYPTO),
	ADD_ERROR_MODULE(SW_CRYPTO),
	ADD_ERROR_MODULE(NV3P),
	ADD_ERROR_MODULE(FASTBOOT),
	ADD_ERROR_MODULE(OTA),
	ADD_ERROR_MODULE(HEAP),
	ADD_ERROR_MODULE(PAGE_ALLOCATOR),
	ADD_ERROR_MODULE(GPT),
	ADD_ERROR_MODULE(LOADER),
	ADD_ERROR_MODULE(SOCMISC),
	ADD_ERROR_MODULE(CARVEOUT),
	ADD_ERROR_MODULE(UART),
	ADD_ERROR_MODULE(CONSOLE),
	ADD_ERROR_MODULE(DEBUG),
	ADD_ERROR_MODULE(TOS),
	ADD_ERROR_MODULE(MB2_PARAMS),
	ADD_ERROR_MODULE(AON),
	ADD_ERROR_MODULE(I2C),
	ADD_ERROR_MODULE(I2C_DEV),
	ADD_ERROR_MODULE(I2C_DEV_BASIC),
	ADD_ERROR_MODULE(FUSE),
	ADD_ERROR_MODULE(TRANSPORT),
	ADD_ERROR_MODULE(LINUXBOOT),
	ADD_ERROR_MODULE(MB1_PLATFORM_CONFIG),
	ADD_ERROR_MODULE(MB1_BCT_LAYOUT),
	ADD_ERROR_MODULE(WARMBOOT),
	ADD_ERROR_MODULE(XUSBF),
	ADD_ERROR_MODULE(CLK_RST),
	ADD_ERROR_MODULE(FUSE_BYPASS),
	ADD_ERROR_MODULE(CPUINIT),
	ADD_ERROR_MODULE(SPARSE),
	ADD_ERROR_MODULE(NVDEC),
	ADD_ERROR_MODULE(EEPROM_MANAGER),
	ADD_ERROR_MODULE(EEPROM),
	ADD_ERROR_MODULE(POWER),
	ADD_ERROR_MODULE(SCE),
	ADD_ERROR_MODULE(APE),
	ADD_ERROR_MODULE(MB1_WARS),
	ADD_ERROR_MODULE(UPHY),
	ADD_ERROR_MODULE(AOTAG),
	ADD_ERROR_MODULE(DRAM_ECC),
	ADD_ERROR_MODULE(NVPT),
	ADD_ERROR_MODULE(AST),
	ADD_ERROR_MODULE(AUTH),
	ADD_ERROR_MODULE(PWM),
	ADD_ERROR_MODULE(ROLLBACK),
	ADD_ERROR_MODULE(NCT),
	ADD_ERROR_MODULE(VERIFIED_BOOT),
	ADD_ERROR_MODULE(PKC_OP),
	ADD_ERROR_MODULE(DISPLAY),
	ADD_ERROR_MODULE(GRAPHICS),
	ADD_ERROR_MODULE(NVDISP),
	ADD_ERROR_MODULE(DSI),
	ADD_ERROR_MODULE(HDMI),
	ADD_ERROR_MODULE(DPAUX),
	ADD_ERROR_MODULE(BOARD_INFO),
	ADD_ERROR_MODULE(GPIO),
	ADD_ERROR_MODULE(KEYBOARD),
	ADD_ERROR_MODULE(MENU),
	ADD_ERROR_MODULE(KERNELBOOT),
	ADD_ERROR_MODULE(PANEL),
	ADD_ERROR_MODULE(NVBLOB),
	ADD_ERROR_MODULE(EXIT),
	ADD_ERROR_MODULE(AB_BOOTCTRL),
	ADD_ERROR_MODULE(FRP),
	ADD_ERROR_MODULE(PMIC),
	ADD_ERROR_MODULE(REGULATOR),
	ADD_ERROR_MODULE(PWM_BASIC),
	ADD_ERROR_MODULE(BOOTLOADER_UPDATE),
	ADD_ERROR_MODULE(UFS),
	ADD_ERROR_MODULE(RATCHET),
	ADD_ERROR_MODULE(DEVICETREE),
	ADD_ERROR_MODULE(SECURITY),
	ADD_ERROR_MODULE(ROLLBACK_PREVENTION),
	ADD_ERROR_MODULE(CARVEOUT_MAPPER),
	ADD_ERROR_MODULE(KEYSLOT),
	ADD_ERROR_MODULE(DP),
	ADD_ERROR_MODULE(SOR),
	ADD_ERROR_MODULE(DISPLAY_PDATA),
	ADD_ERROR_MODULE(RAMDUMP),
	ADD_ERROR_MODULE(PLUGIN_MANAGER),
	ADD_ERROR_MODULE(STORAGE),
	ADD_ERROR_MODULE(TESTS),
	ADD_ERROR_MODULE(MPHY),
	ADD_ERROR_MODULE(SUBCARVEOUT),
	ADD_ERROR_MODULE(DECOMPRESS),
	ADD_ERROR_MODULE(PCI),
	ADD_ERROR_MODULE(MAPPER),
	ADD_ERROR_MODULE(COMBINED_UART),
	ADD_ERROR_MODULE(RCE),
	ADD_ERROR_MODULE(IST),
	ADD_ERROR_MODULE(QUAL_ENGINE),
	ADD_ERROR_MODULE(SOFT_FUSE),
	ADD_ERROR_MODULE(MPU),
	ADD_ERROR_MODULE(ODM_DATA),
	ADD_ERROR_MODULE(NV3P_SERVER),
	ADD_ERROR_MODULE(CCG),
	ADD_ERROR_MODULE(USBH),
	ADD_ERROR_MODULE(DEVICE_PROD),
	ADD_ERROR_MODULE(CONFIG_STORAGE),
	ADD_ERROR_MODULE(USBMSD),
	ADD_ERROR_MODULE(CBO),
};

/**
 * @brief Converts error string pattern into format which can be given to printf function
 *
 * @param err buffer
 * @param err_size size of buffer
 * @param reason error reason
 * @param args variable arguments
 *
 * @return number of positional arguments from va list consumed
 */
static int32_t pre_process_error_string(char *err,
										uint32_t err_size,
										tegrabl_err_reason_t reason,
										va_list args)
{
	char *format = NULL;
	uint32_t index = 0;
	bool open_bracket = false;
	const char *err_str = error_reason_strings[reason];
	int32_t count = 0;

	while ((*err_str != '\0') && (index < err_size)) {
		switch (*err_str) {
		case '{':
			count++;
			format = va_arg(args, char *);
			while ((*format != '\0') && (index < err_size)) {
				err[index] = *format;
				format++;
				index++;
			}
			open_bracket = true;
			break;
		case '}':
			open_bracket = false;
			break;
		default:
			if (!open_bracket) {
				err[index] = *err_str;
				index++;
			}
			break;
		}
		err_str++;
	}

	err[MIN(index, (err_size - 1U))] = '\0';

	return count;
}

void tegrabl_error_print(bool set_first_error, uint32_t level, tegrabl_error_t error, ...)
{
	va_list args;
	va_list copy_args;
	int32_t count;
	int ret;
	char err[100];
	tegrabl_err_reason_t reason;
	tegrabl_err_module_t module;
	uint32_t log_level = 0;
	char level_char = 'D';

#if defined(CONFIG_ENABLE_LOGLEVEL_RUNTIME)
	log_level = tegrabl_debug_loglevel;
#else
	log_level = CONFIG_DEBUG_LOGLEVEL;
#endif

	module = TEGRABL_ERROR_MODULE(error);
	reason = TEGRABL_ERROR_REASON(error);

	if (reason >= TEGRABL_ERR_REASON_END) {
		pr_error("Invalid reason 0x%02x\n", reason);
		return;
	}

	if (module >= TEGRABL_ERR_MODULE_END) {
		pr_error("Invalid module 0x%02x\n", module);
		return;
	}

	/* Need to create copy of va args, else it might get corrupted if it is passed
	 * to function which uses it.
	 */
	va_start(args, error);
	va_copy(copy_args, args);

	switch (level) {
	case TEGRABL_LOG_CRITICAL:
		level_char = 'C';
		break;
	case TEGRABL_LOG_ERROR:
		level_char = 'E';
		break;
	case TEGRABL_LOG_INFO:
		level_char = 'I';
		break;
	case TEGRABL_LOG_DEBUG:
		level_char = 'D';
		break;
	default:
		level_char = ' ';
		break;
	}

	/* Create format string */
	ret = tegrabl_snprintf(err, sizeof(err), "%c> %s: ", level_char, module_strings[module]);

	count = pre_process_error_string(&err[ret], (sizeof(err) - (uint32_t)ret), reason, copy_args);

	/* Leave va args being substituted. All arguments substituted are of type char *. */
	while (count > 0) {
		va_arg(args, char *);
		count--;
	}

	err[sizeof(err) - 1U] = '\0';

	/* Save error into local buffer */
	if (set_first_error) {
		if (first_error == NULL) {
			tegrabl_vsnprintf(error_string, sizeof(error_string), err, args);
			first_error = error_string;
		}
	}

	/* Do not print if print level is greater than current log level. */
	if (log_level >= level) {
		/* Pass format and remaining arguments from variable args to printf */
		ret = tegrabl_vprintf(err, args);
	}

	va_end(copy_args);
	va_end(args);
}

const char *tegrabl_error_module_str(tegrabl_err_module_t module)
{
	const char *ret = "";

	if (module < TEGRABL_ERR_MODULE_END) {
		ret = module_strings[module];
	}

	return ret;
}

void tegrabl_error_print_error(tegrabl_error_t error)
{
	tegrabl_err_module_t top_module;
	tegrabl_err_module_t lowest_module;
	uint32_t aux_info;
	tegrabl_error_t reason;

	lowest_module = TEGRABL_ERROR_MODULE(error);
	aux_info = TEGRABL_ERROR_AUX_INFO(error);
	top_module = (uint8_t)TEGRABL_ERROR_HIGHEST_MODULE(error);
	reason = TEGRABL_ERROR_REASON(error);

	if (lowest_module >= TEGRABL_ERR_MODULE_END) {
		lowest_module = TEGRABL_ERR_NO_MODULE;
	}

	if (top_module >= TEGRABL_ERR_MODULE_END) {
		top_module = TEGRABL_ERR_NO_MODULE;
	}

	pr_error("Top caller module: %s, error module: %s, reason: 0x%02x, aux_info: 0x%02x\n",
			 module_strings[top_module], module_strings[lowest_module], reason, aux_info);
}

void tegrabl_error_clear_first_error(void)
{
	first_error = NULL;
}

const char *tegrabl_error_get_first_error(void)
{
	return first_error;
}

void print_assert_fail(const char *filename, uint32_t line)
{
	pr_error("Assertion failed at %s:%d\n", filename, line);
}

