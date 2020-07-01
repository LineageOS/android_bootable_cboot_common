/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_QSPI_FLASH_H
#define INCLUDED_TEGRABL_QSPI_FLASH_H

#include <tegrabl_error.h>
#include <tegrabl_gpcdma.h>
#include <tegrabl_qspi_flash_param.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief enum for dummy cycles for qspi transfers
 */
/* macro dummy cycles */
typedef uint32_t dummy_cycles_t;
#define ZERO_CYCLES 0
#define EIGHT_CYCLES 8
#define NINE_CYCLES 9
#define TEN_CYCLES 10

/**
 * @brief Initializes the given QSPI flash device and QSPI controller
 *
 * @instance to know the qspi instance
 * @params a pointer to struct tegrabl_qspi_flash_platform_params
 *
 * @retval TEGRABL_NO_ERROR Initialization is successful.
 */
tegrabl_error_t tegrabl_qspi_flash_open(uint32_t instance,
				struct tegrabl_qspi_flash_platform_params *params);

/**
 * @brief Re-initializes the given QSPI flash device and QSPI controller based
 *        on new set of params
 *
 * @instance to know the qspi instance
 * @params a pointer to struct tegrabl_qspi_flash_platform_params
 *
 * @retval TEGRABL_NO_ERROR Initialization is successful.
 */

tegrabl_error_t tegrabl_qspi_flash_reinit(uint32_t instance,
					struct tegrabl_qspi_flash_platform_params *params);

#if defined(__cplusplus)
}
#endif
#endif /* #ifndef INCLUDED_TEGRABL_QSPI_FLASH_H */
