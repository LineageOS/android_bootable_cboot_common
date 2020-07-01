/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All Rights Reserved.
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
enum tegrabl_linux_boot_info {
	TEGRABL_LINUXBOOT_INFO_EXTRA_CMDLINE_PARAMS,
	TEGRABL_LINUXBOOT_INFO_EXTRA_DT_NODES,
	TEGRABL_LINUXBOOT_INFO_DEBUG_CONSOLE,
	TEGRABL_LINUXBOOT_INFO_EARLYUART_BASE,
	TEGRABL_LINUXBOOT_INFO_CARVEOUT,
	TEGRABL_LINUXBOOT_INFO_BOARD,
	TEGRABL_LINUXBOOT_INFO_MEMORY,
	TEGRABL_LINUXBOOT_INFO_INITRD,
	TEGRABL_LINUXBOOT_INFO_BOOTIMAGE_CMDLINE,
	TEGRABL_LINUXBOOT_INFO_SECUREOS,
	TEGRABL_LINUXBOOT_INFO_MAX,
};

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
					enum tegrabl_linux_boot_info info,
					const void *in_data, void *out_data);


/**
 * @brief Helper API (with BL-specific implementation), to compare based on
 * base address as required for calculating free dram regions
 *
 * @param index-1
 * @param index-2
 *
 * @return 0 or 1 based on comparision and -1 otherwise
 */
int32_t bom_compare(const uint32_t a, const uint32_t b);

/**
 * @brief Helper API (with BL-specific implementation), to sort in ascending order
 * based on base addresses of permanent carveouts. This will be used to calcuate
 * free dram regions.
 *
 * @param parm_carveout array
 * @param number of parm carvouts
 *
 * @return void
 */
void sort(uint32_t array[], int32_t count);



/**
 * @brief Helper API (with BL-specific implementation), to set vbstate to be
 *        used in cmdline if needed
 *
 * @param vbstate verified boot state string
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_linuxboot_set_vbstate(const char *vbstate);

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
enum tegrabl_linuxboot_carveout_type {
	TEGRABL_LINUXBOOT_CARVEOUT_VPR,
	TEGRABL_LINUXBOOT_CARVEOUT_TOS,
	TEGRABL_LINUXBOOT_CARVEOUT_BPMPFW,
	TEGRABL_LINUXBOOT_CARVEOUT_LP0,
	TEGRABL_LINUXBOOT_CARVEOUT_NVDUMPER,
};

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
enum tegrabl_linuxboot_androidmode {
	TEGRABL_LINUXBOOT_ANDROIDMODE_REGULAR,
	TEGRABL_LINUXBOOT_ANDROIDMODE_CHARGER,
};

/**
 * @brief Debug console type
 */
enum tegrabl_linuxboot_debug_console {
	TEGRABL_LINUXBOOT_DEBUG_CONSOLE_NONE,
	TEGRABL_LINUXBOOT_DEBUG_CONSOLE_DCC,
	TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTA,
	TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTB,
	TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTC,
	TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTD,
	TEGRABL_LINUXBOOT_DEBUG_CONSOLE_UARTE,
	TEGRABL_LINUXBOOT_DEBUG_CONSOLE_AUTOMATION,
};

/**
 * @brief Board-type
 */
enum tegrabl_linuxboot_board_type {
	TEGRABL_LINUXBOOT_BOARD_TYPE_PROCESSOR,
	TEGRABL_LINUXBOOT_BOARD_TYPE_PMU,
	TEGRABL_LINUXBOOT_BOARD_TYPE_DISPLAY,
};

/**
 * @brief Fields of board-info structure
 */
enum tegrabl_linuxboot_board_info {
	TEGRABL_LINUXBOOT_BOARD_ID,
	TEGRABL_LINUXBOOT_BOARD_SKU,
	TEGRABL_LINUXBOOT_BOARD_FAB,
	TEGRABL_LINUXBOOT_BOARD_MAJOR_REV,
	TEGRABL_LINUXBOOT_BOARD_MINOR_REV,
	TEGRABL_LINUXBOOT_BOARD_MAX_FIELDS,
};

/*******************************************************************************
 * Enum to define supported Trusted OS types
 ******************************************************************************/
enum tegrabl_tos_type {
	TEGRABL_TOS_TYPE_UNDEFINED,
	TEGRABL_TOS_TYPE_TLK,
	TEGRABL_TOS_TYPE_TRUSTY,
};

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_LINUXBOOT_HELPER_H */
