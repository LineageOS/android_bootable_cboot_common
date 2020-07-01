/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
 * @brief Add plugin manager ID for modules read from eeprom.
 *
 * @param fdt DT handle.
 * @param nodeoffset Plugin manager node offset.
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_add_plugin_manager_ids(void *fdt, int nodeoffset);

#if defined(__cplusplus)
}
#endif

#endif
