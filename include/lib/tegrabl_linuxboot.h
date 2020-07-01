/*
 * Copyright (c) 2014-2019, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_LINUXBOOT_H
#define INCLUDED_LINUXBOOT_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include <stdint.h>
#include <tegrabl_error.h>

/**
 * @brief Updates in-memory FDT with linux-kernel related parameters (including
 * the commandline)
 *
 * @param fdt pointer to the in-memory linux-kernel DTB
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error
 */
tegrabl_error_t tegrabl_linuxboot_update_dtb(void *fdt);

/**
 * @brief Prepares the commandline for linux-kernel
 *
 * @param initcmdline initial commandline to which rest of the missing boot
 * params are appended
 *
 * @return pointer to the commandline string
 */
char *tegrabl_linuxboot_prepare_cmdline(char *initcmdline);

/**
 * @brief Function to append given cmdline-param string to existing cmdline.
 * This function implements the interface expected by append field of
 * struct tegrabl_linuxboot_param.
 *
 * @param cmdline - cmdline string where the next param is to be added
 * @param len - maximum length of the string
 * @param param - parameter string for which the function is invoked
 * @param value - value of the cmdline param as a string, this could be NULL
 * if the parameter is not required.
 *
 * @note examples:
 * 1) param="foo", value="val" - causes "foo=val" to be added
 * 2) param="foo", value=NULL - causes "foo" to be added
 *
 * @returns the number of characters being added to the cmdline and a
 * negative value in case of an error
 */
int tegrabl_linuxboot_add_string(char *cmdline, int len,
								 char *param, void *value);

/**
 * @brief Function to append given cmdline-param with numeric value to
 * existing cmdline.
 * This function implements the interface expected by append field of
 * struct tegrabl_linuxboot_param.

 * @param cmdline - cmdline string where the next param is to be added
 * @param len - maximum length of the string
 * @param param - parameter string for which the function is invoked
 * @param value - pointer to the 64bit integer value
 *
 * @note example:
 * param="foo", value=0x123 - causes "foo=0x123" to be added
 *
 * @returns the number of characters being added to the cmdline and a
 * negative value in case of an error
 */
int tegrabl_linuxboot_add_number(char *cmdline, int len,
								 char *param, void *value);

/**
 * @brief Helper function to retreive the location of ramdisk
 *
 * @param start (output parameter) Start of the ramdisk in memory
 * @param size (output parameter) Size of the ramdisk
 */
void tegrabl_get_ramdisk_info(uint64_t *start, uint64_t *size);

/**
 * @brief Helper function to retreive the cmdline string present in boot.img
 * header
 *
 * @return Cmdline string in boot.img header
 */

char *tegrabl_get_bootimg_cmdline(void);

/**
 * @brief Update bootargs param in the dtb
 *
 * @param fdt pointer to the in-memory linux-kernel DTB
 * @param boot_args pointer to additional boot args
 *
 * @return 0 on success, otherwise appropriate FDT error
 */
int tegrabl_linuxboot_update_bootargs(void *fdt, char *boot_args);

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_LINUXBOOT_H */
