/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_PLUGIN_MANAGER_H
#define TEGRABL_PLUGIN_MANAGER_H

#include <tegrabl_error.h>

#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * @brief Add plugin manager ID for modules read from eeprom by offset.
 *
 * @param fdt DT handle.
 * @param nodeoffset Plugin manager node offset.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_add_plugin_manager_ids(void *fdt, int nodeoffset);

/**
 * @brief Add plugin manager ID for modules read from eeprom by path.
 *
 * @param fdt DT handle.
 * @param path Plugin manager node path.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_add_plugin_manager_ids_by_path(void *fdt, char *path);

/**
 * @brief Copy plugin manager properties from src DT to dest DT
 *
 * @param fdt_src source DT handle.
 * @param fdt_dst destination DT handle.
 * @param nodeoffset Plugin manager offset in destination DT.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_copy_plugin_manager_ids(void *fdt_dst, void *fdt_src, int nodeoffset);

/**
 * @brief Overlay DTB as per plugined modules read from eeprom
 *
 * @param fdt DT handle
 *
 * @return TEGRABL_NO_ERROR if overlay succeed else appropriate error
 */
tegrabl_error_t tegrabl_plugin_manager_overlay(void *fdt);

#if defined(__cplusplus)
}
#endif

#endif
