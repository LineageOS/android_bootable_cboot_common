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

#include <tegrabl_qspi.h>
#include <tegrabl_error.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @brief enum for dummy cycles for qspi transfers
 */
enum dummy_cycles {
	ZERO_CYCLES = 0,
	EIGHT_CYCLES = 8,
	NINE_CYCLES = 9,
	TEN_CYCLES = 10,
};

/**
 * @brief Initializes the given QSPI flash device and QSPI controller
 *
 * @param params a pointer to struct tegrabl_mb1bct_qspi_params
 *
 * @retval TEGRABL_NO_ERROR Initialization is successful.
 */
tegrabl_error_t tegrabl_qspi_flash_open(
				struct tegrabl_mb1bct_qspi_params *params);

/**
 * @brief Re-initializes the given QSPI flash device and QSPI controller based
 *        on new set of params
 *
 * @param params a pointer to struct tegrabl_mb1bct_qspi_params
 *
 * @retval TEGRABL_NO_ERROR Initialization is successful.
 */

tegrabl_error_t tegrabl_qspi_flash_reinit(
					struct tegrabl_mb1bct_qspi_params *params);

#if defined(__cplusplus)
}
#endif
#endif /* #ifndef INCLUDED_TEGRABL_QSPI_FLASH_H */
