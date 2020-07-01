/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_UART_CONSOLE_H
#define TEGRABL_UART_CONSOLE_H

#include <tegrabl_error.h>
#include <tegrabl_console.h>
#include <stdint.h>
#include <stddef.h>
#include <tegrabl_uart.h>

/**
* @brief Opens the uart console interface.
*
* @param instance Instance of the uart controller.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
struct tegrabl_uart *tegrabl_uart_console_open(uint32_t instance);

/**
* @brief Sends the character to uart serial console.
*
* @param instance Instance of the uart controller.
* @param ch character to be sent.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_uart_console_putchar(struct tegrabl_console *hcnsl,
	char ch);

/**
* @brief Receives the character from uart serial console.
*
* @param instance Instance of the uart controller.
* @param ch Address at which received character has to be stored.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_uart_console_getchar(struct tegrabl_console *hcnsl,
	char *ch);
/**
* @brief Sends the string to uart serial console.
*
* @param instance Instance of the uart controller.
* @param str Address of the string to be sent.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_uart_console_puts(struct tegrabl_console *hcnsl,
	char *str);

/**
* @brief Closes the uart console interface.
*
* @param instance Instance of the uart controller.
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_uart_console_close(struct tegrabl_console *hcnsl);

#endif

