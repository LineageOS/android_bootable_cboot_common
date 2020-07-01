/*
 * Copyright (c) 2015-2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_LINUXBOOT_HELPER_H
#define INCLUDED_LINUXBOOT_HELPER_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include <tegrabl_error.h>

/**
 * @brief Describes the type of information required by linuxboot library
 */
/* macro tegrabl linux boot info */
typedef uint32_t tegrabl_linux_boot_info_t;
#define TEGRABL_LINUXBOOT_INFO_EXTRA_CMDLINE_PARAMS 0
#define TEGRABL_LINUXBOOT_INFO_EXTRA_DT_NODES 1
#define TEGRABL_LINUXBOOT_INFO_DEBUG_CONSOLE 2
#define TEGRABL_LINUXBOOT_INFO_EARLYUART_BASE 3
#define TEGRABL_LINUXBOOT_INFO_TCU_MBOX_ADDR 4
#define TEGRABL_LINUXBOOT_INFO_CARVEOUT 5
#define TEGRABL_LINUXBOOT_INFO_BOARD 6
#define TEGRABL_LINUXBOOT_INFO_MEMORY 7
#define TEGRABL_LINUXBOOT_INFO_INITRD 8
#define TEGRABL_LINUXBOOT_INFO_BOOTIMAGE_CMDLINE 9
#define TEGRABL_LINUXBOOT_INFO_SECUREOS 10
#define TEGRABL_LINUXBOOT_INFO_MAX 11

/**
 * @brief Helper API (with BL-specific implementation), to extract what information
 * as required by the linuxboot library
 *
 * @param info Type of information required
 * @param in_data Additional Input parameters
 * @param out_data Output parameters
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_linuxboot_helper_get_info(
					tegrabl_linux_boot_info_t info,
					const void *in_data, void *out_data);

#if defined(CONFIG_ENABLE_VERIFIED_BOOT)
/**
 * @brief Helper API (with BL-specific implementation), to set vbmeta info to
 *        be used in cmdline if needed
 *
 * @param vbmeta vbmeta info string provided by libavb
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_linuxboot_set_vbmeta_info(const char *vbmeta);

/**
 * @brief Helper API (with BL-specific implementation), to set vbstate to be
 *        used in cmdline if needed
 *
 * @param vbstate verified boot state string
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_linuxboot_set_vbstate(const char *vbstate);
#endif

#if defined(CONFIG_ENABLE_A_B_SLOT)
/**
 * @brief check if system as root is enabled
 *
 * @return true if it is enabled
 */
bool is_system_as_root_enabled(void);
#endif

/**
 * @brief Represents what processing is required for a commandline parameter
 */
struct tegrabl_linuxboot_param {
	/* str - name of the commandline param */
	char *str;
	/* @brief Function to append given commandline param (if required) with the
	 * proper value, to an existing codmmandline string
	 *
	 * @param cmdline - cmdline string where the next param is to be added
	 * @param len - maximum length of the string
	 * @param param - the cmdline param for which the function is invoked
	 * @param priv - private data for the function
	 *
	 * @returns the number of characters being added to the cmdline and a
	 * negative value in case of an error
	 *
	 * @note Preferably tegrabl_snprintf should be used to print/append the
	 * commandline param and its value
	 */
	int (*append)(char *cmdline, int len, char *param, void *priv);
	/* priv - private data meaningful for the append function */
	void *priv;
};

/**
 * @brief Represents what processing is required for a device-tree node
 */
struct tegrabl_linuxboot_dtnode_info {
	/* node_name - name of the node */
	char *node_name;
	/**
	 * @brief Function to add/update a kernel device-tree node
	 *
	 * @param fdt - pointer to the kernel FDT
	 * @param offset - starting offset from where to search for given node.
	 *
	 * @return TEGRABL_NO_ERROR in case of success, otherwise appropriate error
	 */
	tegrabl_error_t (*fill_dtnode)(void *fdt, int offset);
};

/**
 * @brief Type of carveout
 */
/* macro tegrabl linuxboot carveout type */
typedef uint32_t tegrabl_linuxboot_carveout_type_t;
#define TEGRABL_LINUXBOOT_CARVEOUT_VPR 0
#define TEGRABL_LINUXBOOT_CARVEOUT_TOS 1
#define TEGRABL_LINUXBOOT_CARVEOUT_BPMPFW 2
#define TEGRABL_LINUXBOOT_CARVEOUT_LP0 3
#define TEGRABL_LINUXBOOT_CARVEOUT_NVDUMPER 4

/**
 * @brief Structure for representing a memory block
 */
struct tegrabl_linuxboot_memblock {
	uint64_t base;
	uint64_t size;
};

/**
 * @brief Charging related android-boot mode
 */
/* macro tegrabl linuxboot androidmode */
typedef uint32_t tegrabl_linuxboot_androidmode_t;
#define TEGRABL_LINUXBOOT_ANDROIDMODE_REGULAR 0
#define TEGRABL_LINUXBOOT_ANDROIDMODE_CHARGER 1

/**
 * @brief Debug console type
 */
/* macro tegrabl linuxboot debug console */
typedef uint32_t tegrabl_linuxboot_debug_console_t;
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_NONE 0
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_DCC 1
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTA 2
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTB 3
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTC 4
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTD 5
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTE 6
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTF 7
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTG 8
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTH 9
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_AUTOMATION 10
#define TEGRABL_LINUXBOOT_DEBUG_CONSOLE_COMB_UART 11

/**
 * @brief Board-type
 */
/* macro tegrabl linuxboot board type */
typedef uint32_t tegrabl_linuxboot_board_type_t;
#define TEGRABL_LINUXBOOT_BOARD_TYPE_PROCESSOR 0
#define TEGRABL_LINUXBOOT_BOARD_TYPE_PMU 1
#define TEGRABL_LINUXBOOT_BOARD_TYPE_DISPLAY 2

/**
 * @brief Fields of board-info structure
 */
/* macro tegrabl linuxboot board info */
typedef uint32_t tegrabl_linuxboot_board_info_t;
#define TEGRABL_LINUXBOOT_BOARD_ID 0
#define TEGRABL_LINUXBOOT_BOARD_SKU 1
#define TEGRABL_LINUXBOOT_BOARD_FAB 2
#define TEGRABL_LINUXBOOT_BOARD_MAJOR_REV 3
#define TEGRABL_LINUXBOOT_BOARD_MINOR_REV 4
#define TEGRABL_LINUXBOOT_BOARD_MAX_FIELDS 5

/*******************************************************************************
 * Enum to define supported Trusted OS types
 ******************************************************************************/
/* macro tegrabl tos type */
typedef uint32_t tegrabl_tos_type_t;
#define TEGRABL_TOS_TYPE_UNDEFINED 0
#define TEGRABL_TOS_TYPE_TLK 1
#define TEGRABL_TOS_TYPE_TRUSTY 2

/**
 * @brief Calculates the list of free dram regions by excluding permanent
 * carveouts from DRAM
 *
 * @param address of list of free dram regions
 *
 * @return count of the free dram regions
 */
uint32_t get_free_dram_regions_info(struct tegrabl_linuxboot_memblock
		**free_dram_regions);

#if defined(CONFIG_ENABLE_STAGED_SCRUBBING)
/**
 * @brief Scrubs the DRAM regions not already scrubbed by early BL
 *
 * @return TEGRABL_NO_ERROR if success; relevant error codes in case of failure
 */
tegrabl_error_t dram_staged_scrub(void);
#endif

/**
 * @brief Get NCT image load address
 *
 * @param load_addr ptr to load address of NCT image (output)
 *
 * @return TEGRABL_NO_ERROR if success; relevant error code in case of failure
 */
tegrabl_error_t tegrabl_get_nct_load_addr(void **load_addr);

/**
 * @brief Get boot image load address
 *
 * @param load_addr ptr to load address of boot image (output)
 *
 * @return TEGRABL_NO_ERROR if success; relevant error code in case of failure
 */
tegrabl_error_t tegrabl_get_boot_img_load_addr(void **load_addr);

/**
 * @brief Get kernel load address
 *
 * @return kernel load address
 */
uint64_t tegrabl_get_kernel_load_addr(void);

/**
 * @brief Get dtb load address
 *
 * @return dtb load address
 */
uint64_t tegrabl_get_dtb_load_addr(void);

/**
 * @brief Get ramdisk load address
 *
 * @return ramdisk load address
 */
uint64_t tegrabl_get_ramdisk_load_addr(void);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_LINUXBOOT_HELPER_H */
