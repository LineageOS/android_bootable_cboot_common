/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_COMB_UART_H
#define TEGRABL_COMB_UART_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_timer.h>

#define COMB_UART_BPMP_MAILBOX 0
#define COMB_UART_CPU_MAILBOX 1

typedef enum {
	COMB_UART_MAILBOX_TX_PORT,
	COMB_UART_MAILBOX_RX_PORT,
} tegrabl_comb_uart_mailbox_port_t;

/**
* @brief Initializes the combined uart controller.
*
* @param mailbox_type 0:bpmp, 1:cpu.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_comb_uart_init(uint32_t mailbox_type);

/**
* @brief This function splits the string to be printed (const char *s) into multiple
* packets. Each packet contains a max of 3 characters. Packets are sent to the
* SPE-based combined UART server for printing. Communication with SPE is done
* through mailbox registers which can generate interrupts for SPE and BPMP.
*
* @param tx_buf Buffer which has data to send.
* @param len Number of bytes to send.
* @param timeout Timeout to wait for the data transfer. (in ms)
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_comb_uart_tx(const void *tx_buf, uint32_t len, time_t timeout);

/**
* @brief Receives the data on the comb uart interface.
*
* @param rx_buf Buffer to which received data has to be stored.
* @param len Number bytes to read.
* @param bytes_received Pointer to the number of bytes received.
* @param timeout Timeout to wait for the data. (in ms)
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_comb_uart_rx(uint8_t *rx_buf, uint32_t len, uint32_t *bytes_received,
	time_t timeout);

/**
* @brief Get combined uart's mailbox tx/rx address
*
* @param port tx/rx port
* @param addr output tx/rx address into this variable
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_comb_uart_get_mailbox_port_addr(tegrabl_comb_uart_mailbox_port_t port,
														uint32_t *const addr);

/**
* @brief Disables the comb uart interface.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_comb_uart_close(void);

/**
 * Flushes the physical UART buffer. This should only be called when
 * rebooting or shutting down the system.
 *
 * Return TEGRABL_NO_ERROR on success, -TEGRABL_ERR_TIMEOUT in failure.
 */
tegrabl_error_t tegrabl_comb_uart_hw_flush(void);

#endif
