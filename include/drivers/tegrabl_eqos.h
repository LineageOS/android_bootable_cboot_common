/*
 * Copyright (c) 2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_EQOS_H
#define TEGRABL_EQOS_H

#include <tegrabl_error.h>

tegrabl_error_t tegrabl_eqos_init(void);
void tegrabl_eqos_send(void *packet, size_t len);
void tegrabl_eqos_receive(void *packet, size_t *len);
bool tegrabl_eqos_is_dma_rx_intr_occured(void);
void tegrabl_eqos_set_mac_addr(uint8_t * const addr);
void tegrabl_eqos_clear_dma_rx_intr(void);
void tegrabl_eqos_deinit(void);

#endif
