/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_POWER_H
#define TEGRABL_POWER_H

#include <tegrabl_error.h>
#include <tegrabl_module.h>

/**
* @brief Unpowergates the given module.
*
* @param module Module to unpower gate.
*
* @return Returns TEGRABL_NO_ERROR if success, error code if fails.
*/
tegrabl_error_t tegrabl_unpowergate(tegrabl_module_t module);

#endif
