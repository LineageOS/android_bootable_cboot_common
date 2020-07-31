/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_QSPI_FLASH_SPANSION_H
#define INCLUDED_TEGRABL_QSPI_FLASH_SPANSION_H

#define MODULE TEGRABL_ERR_SPI_FLASH

#include <stdint.h>
#include <tegrabl_qspi_flash_private.h>

#if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC)

tegrabl_error_t qspi_flash_qpi_mode_enable_spansion(struct tegrabl_qspi_flash_driver_info *hqfdi,
													bool benable);

#endif /* #if !defined(CONFIG_ENABLE_BLOCKDEV_BASIC) */

tegrabl_error_t qspi_flash_x4_enable_spansion(struct tegrabl_qspi_flash_driver_info *hqfdi, uint8_t bset);

#if !defined(CONFIG_DISABLE_QSPI_FLASH_WRITE_512B_PAGE)
tegrabl_error_t qspi_flash_page_512bytes_enable_spansion(struct tegrabl_qspi_flash_driver_info *hqfdi);
#endif

tegrabl_error_t qspi_flash_blank_check_enable_spansion(struct tegrabl_qspi_flash_driver_info *hqfdi);

#endif /* INCLUDED_TEGRABL_QSPI_FLASH_SPANSION_H*/

