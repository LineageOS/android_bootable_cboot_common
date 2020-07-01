/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_TEGRABL_COMB_UART_SOC_LOCAL_H
#define INCLUDE_TEGRABL_COMB_UART_SOC_LOCAL_H

#include <tegrabl_comb_uart.h>

struct comb_uart_mailbox {
	uint32_t tx_addr;
	uint32_t rx_addr;
};

/**
* @brief get the mailbox address for a particular module.
*
* @param mailbox_type 0:bpmp, 1:cpu.
*
* @return structure containing the mailbox addresses for Tx and Rx.
*/
struct comb_uart_mailbox comb_uart_get_mailbox_info(uint32_t mailbox_type);

#endif /* INCLUDE_TEGRABL_COMB_UART_SOC_LOCAL_H */
