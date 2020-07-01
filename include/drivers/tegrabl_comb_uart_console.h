/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_COMB_UART_CONSOLE_H
#define TEGRABL_COMB_UART_CONSOLE_H

#include <tegrabl_error.h>
#include <tegrabl_console.h>
#include <stdint.h>
#include <stddef.h>
#include <tegrabl_comb_uart.h>

/**
* @brief Sends a character to comb uart serial console.
*
* @param hcnsl console handle.
* @param ch character to be sent.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_comb_uart_console_putchar(struct tegrabl_console *hcnsl, char ch);

/**
* @brief Receives a character from comb uart serial console.
*
* @param hcnsl console handle.
* @param ch Address at which received character has to be stored.
* @param timeout timout in ms
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_comb_uart_console_getchar(struct tegrabl_console *hcnsl, char *ch,
	time_t timeout);

/**
* @brief Sends the string to comb uart serial console.
*
* @param hcnsl console handle.
* @param str Address of the string to be sent.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_comb_uart_console_puts(struct tegrabl_console *hcnsl, char *str);

/**
* @brief Closes the comb uart console interface.
*
* @param hcnsl console handle.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_comb_uart_console_close(struct tegrabl_console *hcnsl);

#endif

