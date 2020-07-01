/*
 * Copyright (c) 2016-2019, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE  TEGRABL_ERR_BOARD_INFO

#include <tegrabl_error.h>
#include <tegrabl_board_info.h>
#include <tegrabl_nct.h>
#include <tegrabl_debug.h>
#include <string.h>
#include "board_info_local.h"

static tegrabl_error_t nct_get_serial_no(void *param)
{
	tegrabl_error_t err;
	union nct_item item;
	uint8_t *buf = (uint8_t *)param;

	err = tegrabl_nct_read_item(NCT_ID_SERIAL_NUMBER, &item);

	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error: %u: Failed to read Serial Number from NCT\n", err);
	} else {
		int len = strlen((char *)&item.serial_number);
		TEGRABL_ASSERT(SNO_SIZE >= len);
		memcpy(buf, &item.serial_number, len);
		buf[len] = '\0';
		pr_debug("Read Serial Number from NCT successfully.\n");
	}

	return err;
}

static nct_id_t mac_type_nct_id[MAC_ADDR_TYPE_MAX] = {
				NCT_ID_WIFI_ADDR,
				NCT_ID_BT_ADDR,
				NCT_ID_ETH_ADDR,
};

static tegrabl_error_t nct_get_mac_addr(void *param)
{
	tegrabl_error_t err;
	struct mac_addr *mac_addr_info = (struct mac_addr *)param;
	mac_addr_type_t type = mac_addr_info->type;
	char *bytes = (char *)mac_addr_info->mac_byte_array;
	char *string = (char *)mac_addr_info->mac_string;

	nct_id_t id;
	union nct_item item;
	uint8_t *data;

	if (type >= MAC_ADDR_TYPE_MAX) {
		pr_error("Error: Invalid MAC address type: %u\n", type);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	id = mac_type_nct_id[type];
	err = tegrabl_nct_read_item(id, &item);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error: Failed to read MAC address from NCT (id: %u)\n", id);
		goto fail;
	}

	switch (type) {
	case MAC_ADDR_TYPE_WIFI:
		data = &item.wifi_addr.addr[0];
		break;
	case MAC_ADDR_TYPE_BT:
		data = &item.bt_addr.addr[0];
		break;
	case MAC_ADDR_TYPE_ETHERNET:
		data = &item.eth_addr.addr[0];
		break;
	default:
		pr_error("Error: Undefined MAC address type: %u\n", type);
		goto fail;
	}

	if (is_valid_mac_addr(data) == false) {
		pr_warn("MAC addr invalid!\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	if (bytes != NULL) {
		memcpy(bytes, data, MAC_ADDR_BYTES);
	}

	if (string != NULL) {
		create_mac_addr_string(string, data, true);
		pr_info("MAC (type: %u): %s\n", type, string);
	}

fail:
	return err;
}

static inline bool is_separator(uint8_t c)
{
	return c == ID_SEPARATOR;
}

static inline bool is_terminator(uint8_t c)
{
	return c == '\0';
}

/**
 * @brief Parse the given string to verify bytes are number in given length
 *
 * @param str		the addr of pointer of string to parse
 * @param len		number of byte to parse
 *
 * @return true if the string are all number bytes in given length
 * and move str to point to next byte. Otherwise, return false
 */
static inline bool is_n_num(uint8_t **str, uint32_t len)
{
	while (len > 0) {
		if ((**str < '0') || (**str > '9'))
			return false;
		++(*str);
		--len;
	}
	return true;
}

static inline bool is_n_alnum(uint8_t **str, uint32_t len)
{
	while (len > 0) {
		if (((**str >= '0') && (**str <= '9')) ||
			((**str >= 'a') && (**str <= 'z')) ||
			((**str >= 'A') && (**str <= 'Z'))) {
				++(*str);
				--len;
				continue;
		}
		return false;
	}
	return true;
}

static tegrabl_error_t is_valid_id(uint8_t *id)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t *str = id;

	/* Full board id: <id>-<sku>-<fab>[-<rev>] */
	if (is_n_alnum(&str, BOARD_ID_SZ) && is_separator(*str++) &&
		is_n_alnum(&str, BOARD_SKU_SZ) && is_separator(*str++) &&
		is_n_alnum(&str, BOARD_FAB_SZ)) {
		/* Is an id without rev ? */
		if (is_terminator(*str))
			goto done;

		/* Is an id with valid rev ? */
		if (is_separator(*str++) && is_n_alnum(&str, BOARD_REV_SZ))
			goto done;
	}

	err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
done:
	return err;
}

static tegrabl_error_t nct_get_board_ids(void *param)
{
	struct board_id_info *id_info = (struct board_id_info *)param;
	tegrabl_error_t err;
	union nct_item item;
	struct nct_uuid_container *uuid = &item.uuids[0];
	uint32_t uuid_entries = 0;

	/* Init the number of id return */
	id_info->count = 0;

	/* Get UUIDs from NCT */
	err = tegrabl_nct_read_item(NCT_ID_UUID, &item);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error: %u: Failed to read UUID (board ids) from NCT\n", err);
		goto fail;
	}

	/* Go thru all entries until null */
	/*
	 *  index  content
	 * --------------------------------------------
	 *   0     id1
	 *   1     id2
	 *   2      configs for id2 (configs could occupy 0 or multiple entries)
	 *   3      (more) configs for id2
	 *   4     id3
	 *   ...   ...
	 *
	 */
	while ((uuid_entries < UUIDS_PER_NCT_ENTRY) && (*uuid->id)) {
		/* Validate board id */
		err = is_valid_id((uint8_t *)uuid->id);
		if (err != TEGRABL_NO_ERROR) {
			/* Invalid board id is not fatal error
			 * Board id is searched from UUID list, and it's kind of WAR to
			 * treat a UUID as the form of <id>-<sku>-<fab>[-<rev>]
			 * It can be properly re-arch'ed in the future */
			pr_warn("Warn: %u: Invalid board id: %s\n", err, uuid->id);
			err = TEGRABL_NO_ERROR;
		}

		/* Store board id */
		tegrabl_snprintf((char *)id_info->part[id_info->count].part_no,
						 MAX_BOARD_PART_NO_LEN, "%s", uuid->id);

		pr_info("ID: %s, len: %d\n", id_info->part[id_info->count].part_no,
				(int)strlen((char *)id_info->part[id_info->count].part_no));

		/*
		 * TODO
		 * Although configs are not needed from NCT,
		 * tt could be handled here if it is added in the future
		 */
		id_info->part[id_info->count].config[0] = '\0';

		/* Update id count found */
		++id_info->count;

		if (id_info->count >= MAX_SUPPORTED_BOARDS) {
			/* warning: reach max board count supported */
			pr_info("Warning: Reach maximum boards supported (%d)\n",
					MAX_SUPPORTED_BOARDS);
			err = TEGRABL_NO_ERROR;
			goto fail;
		}

		/*
		 * Prepare for next uuid entry
		 */
		++uuid;
		++uuid_entries;
	}

	err = TEGRABL_NO_ERROR;

fail:
	pr_info("Number of valid board id found from NCT: %d\n", id_info->count);

	return err;
}

static struct board_info_ops nct_ops = {
	.get_serial_no = nct_get_serial_no,
	.get_mac_addr = nct_get_mac_addr,
	.get_board_ids = nct_get_board_ids,
};

struct board_info_ops *nct_get_ops(void)
{
	return &nct_ops;
}
