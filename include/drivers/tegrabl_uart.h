/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_UART_H
#define TEGRABL_UART_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>
#include "build_config.h"

/**
* @brief uart context structure
*/
struct tegrabl_uart {
	uint32_t base_addr;
	uint32_t instance;
	uint32_t baud_rate;
};

/**
* @brief Initializes the uart controller.
*
* @param instance Instance of the uart controller.
*
* @return Returns handle to the uart. NULL in case of failure.
*/
struct tegrabl_uart *tegrabl_uart_open(uint32_t instance);

/**
* @brief Sends the given data on the uart interface.
*
* @param huart Handle to the uart.
* @param tx_buf Buffer which has data to send.
* @param len Number of bytes to send.
* @param bytes_transmitted Pointer to the number of bytes transmitted.
* @param tfr_timeout Timeout to wait for the data transfer.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_uart_tx(struct tegrabl_uart *huart, const void *tx_buf,
	uint32_t len, uint32_t *bytes_transmitted, time_t tfr_timeout);

/**
* @brief Receives the data on the uart interface.
*
* @param huart Handle to the uart.
* @param buf Buffer to which received data has to be stored.
* @param len Number bytes to read.
* @param bytes_received Pointer to the number of bytes received.
* @param tfr_timeout Timeout to wait for the data.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_uart_rx(struct tegrabl_uart *huart,  void *rx_buf,
	uint32_t len, uint32_t *bytes_received, time_t tfr_timeout);

/**
* @brief Gives the base address of given uart instance
*
* @param instance Instance of the uart
* @param addr Pointer to store the base address.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
#if defined(CONFIG_ENABLE_UART)
tegrabl_error_t tegrabl_uart_get_address(uint32_t instance, uint64_t *addr);
#else
static inline tegrabl_error_t tegrabl_uart_get_address(uint32_t instance,
													   uint64_t *addr)
{
	TEGRABL_UNUSED(instance);
	*addr = 0;

	return TEGRABL_NO_ERROR;
}
#endif

/**
* @brief Disables the uart controller.
*
* @param huart Handle to the uart.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_uart_close(struct tegrabl_uart *huart);

#endif
