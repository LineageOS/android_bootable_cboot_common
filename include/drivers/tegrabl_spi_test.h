/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_SPI_TEST_H
#define INCLUDED_TEGRABL_SPI_TEST_H

#include <stdint.h>
#include <tegrabl_error.h>

/**
* @brief spi bus and message
*/
/* macro tegrabl spi message and buf */
#define SPI_BUF_LEN_RD       64
#define SPI_BUF_LEN_WR       64
#define SPI_BUS_FREQ_KHZ     500
#define SPI_FLASH_ID_RD_CMD  0x9f
#define SPI_BUS_MODE         0
#define SPI_BUS_NUM          0

/**
 * @brief scans the spi flash
 *
 * @return TEGRABL_NO_ERROR if success. Error code in case of failure.
 */
tegrabl_error_t do_spi_flash_probe(void);

#endif				/* #ifndef INCLUDED_TEGRABL_SPI_H */
