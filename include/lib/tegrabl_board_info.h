/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_BOARD_INFO_H
#define INCLUDED_TEGRABL_BOARD_INFO_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_i2c.h>
#include <tegrabl_eeprom_layout.h>
#include <tegrabl_eeprom_manager.h>

/**
 * @brief Callback functions that provide implementation from nct and eeprom.
 */
struct board_info_ops {
	tegrabl_error_t (*get_serial_no)(void *param);
	tegrabl_error_t (*get_mac_addr)(void *param);
	tegrabl_error_t (*get_board_ids)(void *param);
};

#define SNO_SIZE 30 /* Max length of serial number on cmdline */

/**
 * @brief Load in serial_no
 *
 * @param buf Buffer to store serial no.
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise appropriate error code
 */
tegrabl_error_t tegrabl_get_serial_no(uint8_t *buf);

#define MAC_ADDR_STRING_LEN 18	/* XX:XX:XX:XX:XX:XX */
#define MAC_ADDR_BYTES 6
/* macro mac addr type */
typedef uint32_t mac_addr_type_t;
#define MAC_ADDR_TYPE_WIFI 0
#define MAC_ADDR_TYPE_BT 1
#define MAC_ADDR_TYPE_ETHERNET 2
#define MAC_ADDR_TYPE_MAX 3


struct mac_addr {
	mac_addr_type_t type;
	uint8_t *mac_byte_array;
	uint8_t *mac_string;
};

/**
 * @brief Load in mac address
 *
 * @param type - mac type WIFI, BT or ETHERNET
 * @param mac_bytes - Buffer to store mac addr in byte wise format
 * @param mac_string - Buffer to store mac addr in string format
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_get_mac_address(mac_addr_type_t type, uint8_t *mac_bytes,
										uint8_t *mac_string);

/**
 * @brief board id struct version.
 *
 * board id struct may change from time to time. Add new version here.
 * Version 1 format is based on EEPROM layout
 */
typedef uint32_t board_id_version_t;
#define BOARD_ID_INFO_VERSION_1 0x0100
#define BOARD_ID_INFO_VERSION_LAST 0x0101

/* Constands defined for version 1 */
#define BOARD_ID_SZ		EEPROM_BDID_SZ
#define BOARD_SKU_SZ	EEPROM_SKU_SZ
#define BOARD_FAB_SZ	EEPROM_FAB_SZ
#define BOARD_REV_SZ	EEPROM_REV_SZ
#define BOARD_FULL_REV_SZ	EEPROM_FULL_BDID_LEN
#define ID_SEPARATOR	'-'

#define MAX_SUPPORTED_BOARDS	8
#define MAX_BOARD_NAME			16
#define MAX_BOARD_PART_NO_LEN	64
#define MAX_BOARD_CONFIG_LEN	(64*2)
#define MAX_BOARD_LOCATION_LEN	100
/**
 * @brief The structure that contains a board's part no and configs
 *
 * @customer_part_id: Tells whether board ID format is customer specific or
 *		      Nvidia specific. True for customer specific.
 * @param name - board's info name, can be NULL. Eg: cvm
 * @param part_no - board's id, sku, fab, and rev. Eg: 3310-1000-100-A
 * @param config - board's configurations. Eg: mem-type:0, power-config:1, ...
 * @location: board's location to find out which bus it is connected.
 */
struct board_part {
	bool customer_part_id;
	char name[MAX_BOARD_NAME];
	uint8_t part_no[MAX_BOARD_PART_NO_LEN];
	uint8_t config[MAX_BOARD_CONFIG_LEN];
	uint8_t location[MAX_BOARD_LOCATION_LEN];
};

/**
 * @brief Version 1 structure
 *
 * @param version - board id structure version. Must be at the first two bytes
 *					of board_id_info struture.
 * @param count - the effect count of board part array.
 * @param part - the buffer to store board ids.
 */
struct board_id_info {
	uint16_t version;
	uint16_t count;
	struct board_part part[MAX_SUPPORTED_BOARDS];
};

/**
 * @brief Get all board ids
 *
 * @param id_info - pointer points to returning board_ids buffer
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 * In case NO_ERROR, count field indicates board ids returned.
 */
tegrabl_error_t tegrabl_get_board_ids(void *id_info);

#endif /* INCLUDED_TEGRABL_BOARD_INFO_H */
