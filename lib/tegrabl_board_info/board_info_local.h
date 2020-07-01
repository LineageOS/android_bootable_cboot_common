/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_BOARD_INFO_LOCAL_H
#define INCLUDED_TEGRABL_BOARD_INFO_LOCAL_H

/**
 * @brief A helper function for forming mac address string
 *
 * @param mac_addr_s - return mac addr (in string format) location
 * @param mac_addr_n - source mac addr (in number format) location
 * @param big_endian - source mac addr is in big endian format
 *
 */
void create_mac_addr_string(char *mac_addr_s, uint8_t *mac_addr_n,
								   bool big_endian);

/**
 * @brief Get ops table pointer
 */
struct board_info_ops *nct_get_ops(void);
struct board_info_ops *eeprom_get_ops(void);
#endif
