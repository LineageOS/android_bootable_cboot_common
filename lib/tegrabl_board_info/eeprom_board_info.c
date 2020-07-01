/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE  TEGRABL_ERR_BOARD_INFO

#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_eeprom.h>
#include <tegrabl_eeprom_layout.h>
#include <tegrabl_eeprom_manager.h>
#include <tegrabl_board_info.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include "board_info_local.h"

static tegrabl_error_t eeprom_get_serial_no(void *param)
{
	tegrabl_error_t err;
	uint8_t *buf = (uint8_t *)param;
	struct tegrabl_eeprom *cvm_eeprom = NULL;
	struct eeprom_layout *eeprom;
	uint32_t i, j;

	TEGRABL_ASSERT(SNO_SIZE >= sizeof(eeprom->serial_no));

	err = tegrabl_eeprom_manager_get_eeprom_by_name("cvm", &cvm_eeprom);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u: Failed to read CVM EEPROM\n", err);
		return err;
	}

	/* Get eeprom data */
	eeprom = (struct eeprom_layout *)cvm_eeprom->data;

	for (j = 0, i = 0; i < sizeof(eeprom->serial_no); i++) {
		if (eeprom->serial_no[i] != '-') {
			buf[j] = eeprom->serial_no[i];
			j++;
		}
	}
	buf[j] = '\0';

	return err;
}

/**
 * @brief Metadata for a MAC Address of a certain interface
 *
 * @param param - pointer points to structure containing mac addr type and
 *                buffer pointer for storing return data
 */
static tegrabl_error_t eeprom_get_mac_addr(void *param)
{
	tegrabl_error_t err;
	struct tegrabl_eeprom *cvm_eeprom = NULL;
	struct eeprom_layout *eeprom;
	uint8_t *data;

	struct mac_addr *mac_addr_info = (struct mac_addr *)param;
	enum mac_addr_type type = mac_addr_info->type;
	char *buf = (char *)mac_addr_info->buf;

	err = tegrabl_eeprom_manager_get_eeprom_by_name("cvm", &cvm_eeprom);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error %u: Failed to get CVM EEPROM contents\n", err);
		pr_error("Booting w/o MAC ddresses for WIFI, Bluetooth & Ethernet\n");
		goto fail;
	}

	/* Get eeprom data */
	eeprom = (struct eeprom_layout *)cvm_eeprom->data;

	switch (type) {
	case MAC_ADDR_TYPE_WIFI:
		data = &eeprom->wifi_mac_addr[0];
		break;
	case MAC_ADDR_TYPE_BT:
		data = &eeprom->bt_mac_addr[0];
		break;
	case MAC_ADDR_TYPE_ETHERNET:
		data = &eeprom->eth_mac_addr[0];
		break;
	default:
		pr_error("Error: Undefined MAC address type: %u\n", type);
		goto fail;
	}

	/* mac address stored in eeprom is in little endian */
	create_mac_addr_string(buf, data, false);
	pr_info("%s: MAC (type: %u): %s\n", __func__, type, buf);

fail:
	return err;
}

struct board_special_config {
	const char *name;
	int index;
};

static struct board_special_config board_sp_config[] = {
	{.name = "mem-type", .index = offsetof(struct eeprom_layout, mem_type)},
	{.name = "power-config",
					.index = offsetof(struct eeprom_layout, power_config)},
	{.name = "misc-config",
					.index = offsetof(struct eeprom_layout, misc_config)},
	{.name = "modem-config",
					.index = offsetof(struct eeprom_layout, modem_config)},
	{.name = "touch-config",
					.index = offsetof(struct eeprom_layout, touch_config)},
	{.name = "display-config",
					.index = offsetof(struct eeprom_layout, display_config)},
};

static char get_valid_char(int x)
{
	switch (x) {
	case 65 ... 90:
	case 48 ... 57:
	case 97 ... 122:
		return x;
	case 0:
		return '0';
	default:
		return 'X';
	}
}

static tegrabl_error_t create_pm_ids(struct board_id_info *id_info,
									 struct tegrabl_eeprom *bl_eeprom,
									 struct eeprom_layout *eeprom,
									 int data_max)
{
	int err = TEGRABL_NO_ERROR;
	char name[EEPROM_FULL_BDID_LEN];
	char cust_board_name[MAX_BOARD_PART_NO_LEN];
	char boardid[EEPROM_BDID_SZ + 1], sku[EEPROM_SKU_SZ + 1];
	char fab_str[EEPROM_FAB_SZ + 1];
	char *data;
	uint32_t i = 0U;
	int count;

	if (id_info->count >= MAX_SUPPORTED_BOARDS) {
		pr_error("Error: Reach maximum supported board count: %d\n",
				 MAX_SUPPORTED_BOARDS);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto done;
	}

	if (data_max < (int)EEPROM_SZ) {
		pr_error("Incomplete number of board ID eeprom data\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto done;
	}

	if (eeprom->part_no.leading[0] == 0xCC) {
		data = (char *)&eeprom->part_no.leading[1];
		memset(cust_board_name, 0, MAX_BOARD_PART_NO_LEN);
		strncpy(cust_board_name, data, MAX_BOARD_PART_NO_LEN);
		cust_board_name[MAX_BOARD_PART_NO_LEN - 1] = '\0';

		/* Store board id string */
		memset(id_info->part[id_info->count].part_no, 0,
			   MAX_BOARD_PART_NO_LEN);
		memcpy(id_info->part[id_info->count].part_no, cust_board_name,
			   strlen(cust_board_name) + 1);
		pr_info("Customer board %s found\n", cust_board_name);
		id_info->part[id_info->count].customer_part_id = true;

		goto id_done;
	}

	for (i = 0; i < EEPROM_BDID_SZ; ++i) {
		boardid[i] = get_valid_char(eeprom->part_no.id[i]);
	}

	for (i = 0; i < EEPROM_SKU_SZ; ++i) {
		sku[i] = get_valid_char(eeprom->part_no.sku[i]);
	}

	for (i = 0; i < EEPROM_FAB_SZ; i++) {
		fab_str[i] = get_valid_char(eeprom->part_no.fab[i]);
	}

	memcpy(name, boardid, EEPROM_BDID_SZ);
	count = EEPROM_BDID_SZ;
	name[count++] = ID_SEPARATOR;

	memcpy(name + count, sku, EEPROM_SKU_SZ);
	count += EEPROM_SKU_SZ;
	name[count++] = ID_SEPARATOR;

	memcpy(name + count, fab_str, EEPROM_FAB_SZ);
	count += EEPROM_FAB_SZ;

	name[count++] = ID_SEPARATOR;
	name[count++] = get_valid_char(eeprom->part_no.rev);
	name[count] = '\0';

	pr_info("%s: id: %s, len: %d\n", __func__, name, count);

	if (count >= MAX_BOARD_PART_NO_LEN) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto done;
	}

	/* Store board id string */
	memcpy(id_info->part[id_info->count].part_no, name, (count + 1));
	id_info->part[id_info->count].customer_part_id = false;

	/* Store all configs */
	data = (char *)id_info->part[id_info->count].config;
	count = 0;
	for (i = 0; i < ARRAY_SIZE(board_sp_config); ++i) {
		int len = strlen(board_sp_config[i].name);

		/* config fmt: "<name_string>:<2-byte-char-value>," */
		if ((count + len + 4) > (MAX_BOARD_CONFIG_LEN - 1)) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
			goto done;
		}

		tegrabl_snprintf(data, (MAX_BOARD_CONFIG_LEN - count), "%s:%02x,",
						 (char *)board_sp_config[i].name,
						 *((char *)eeprom + board_sp_config[i].index));
		count += (len + 4);
		data += (len + 4);
	}

	*data = 0x0;
	pr_info("config: %s, len: %d\n",
			(char *)id_info->part[id_info->count].config, count);

id_done:
	data = (char *)id_info->part[id_info->count].location;
	tegrabl_snprintf(data, MAX_BOARD_LOCATION_LEN, "%s:module@0x%02x",
					 bl_eeprom->bus_node_name, bl_eeprom->slave_addr >> 1);

	/* update entry count */
	++id_info->count;

	err = TEGRABL_NO_ERROR;
done:
	return err;
}

static tegrabl_error_t eeprom_get_board_ids(void *param)
{
	struct board_id_info *id_info = (struct board_id_info *)param;
	struct tegrabl_eeprom *bl_eeprom = NULL;
	tegrabl_error_t status = TEGRABL_NO_ERROR;
	uint8_t i, count;

	/* Init the number of id return */
	id_info->count = 0;

	status = tegrabl_eeprom_manager_max(&count);
	if (status != TEGRABL_NO_ERROR) {
		pr_error("Cannot get eeprom max count\n");
		return status;
	}

	for (i = 0; i < count; i++) {
		status = tegrabl_eeprom_manager_get_eeprom_by_id(i, &bl_eeprom);
		if (status != TEGRABL_NO_ERROR) {
			pr_error("Error: %u: Failed to read EEPROM\n", status);
			return status;
		}

		status = create_pm_ids(id_info, bl_eeprom,
							   (struct eeprom_layout *)bl_eeprom->data,
							   bl_eeprom->size);
		if (status != TEGRABL_NO_ERROR)
			return status;
	}

	return status;
}

static struct board_info_ops eeprom_ops = {
	.get_serial_no = eeprom_get_serial_no,
	.get_mac_addr = eeprom_get_mac_addr,
	.get_board_ids = eeprom_get_board_ids,
};

struct board_info_ops *eeprom_get_ops(void)
{
	return &eeprom_ops;
}
