/*
 * Copyright (c) 2016-2021, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE  TEGRABL_ERR_BOARD_INFO

#include <tegrabl_devicetree.h>
#include <tegrabl_error.h>
#include <tegrabl_board_info.h>
#include <tegrabl_debug.h>
#include <string.h>
#include <tegrabl_soc_misc.h>
#include "board_info_local.h"
#include <ctype.h>

#define MAC_ADDR_SIZE_BYTES		6UL

static struct board_info_ops *ops;
static bool board_info_initialized;

void create_mac_addr_string(char *mac_addr_s, uint8_t *mac_addr_n,
						bool big_endian)
{
	uint8_t *mac_n = mac_addr_n;
	uint8_t bytes[6];
	uint32_t i;

	/* ethnet address is in big endian */
	if (big_endian != true) {
		for (i = 0; i < 6; ++i)
			bytes[i] = mac_addr_n[5-i];
		mac_n = &bytes[0];
	}

	tegrabl_snprintf(mac_addr_s, MAC_ADDR_STRING_LEN,
					 "%02x:%02x:%02x:%02x:%02x:%02x",
					 mac_n[0], mac_n[1],
					 mac_n[2], mac_n[3],
					 mac_n[4], mac_n[5]);
}

/* check DT node /chosen/board-has-eeprom */
static bool board_has_eeprom(tegrabl_dt_type_t dt_id)
{
	tegrabl_error_t err;
	int val, node = 0;
	void *fdt;
	bool value = false;

	err = tegrabl_dt_get_fdt_handle(dt_id, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get dtb handle\n");
		goto done;
	}

	err = tegrabl_dt_get_node_with_path(fdt, "/chosen", &node);
	if (err == TEGRABL_NO_ERROR) {
		err = tegrabl_dt_get_prop(fdt, node, "board-has-eeprom", 4, &val);
		if (err == TEGRABL_NO_ERROR) {
			value = true;
		}
	}

done:
	return value;
}

static void tegrabl_board_info_init(void)
{
	bool eeprom;

	if (board_info_initialized)
		return;		/* Return early */

	if (tegrabl_is_vdk() || tegrabl_is_fpga()) {
		goto done;
	}

	eeprom = board_has_eeprom(TEGRABL_DT_BL);
	pr_debug("board has eeprom:%d\n", eeprom);
#if defined(CONFIG_ENABLE_EEPROM)
	if (!eeprom) {
		eeprom = eeprom_detect();
	}

	if (eeprom) {
		ops = eeprom_get_ops();	/* Retrieve info from EEPROM */
	}
#endif

#if defined(CONFIG_ENABLE_NCT)
	if (!ops) {
		ops = nct_get_ops();	/* Retrieve info from NCT */
	}
#endif

done:
	board_info_initialized = true;
}

tegrabl_error_t tegrabl_get_serial_no(uint8_t *buf)
{
	tegrabl_error_t err;
	uint32_t i = 0;

	if (!board_info_initialized)
		tegrabl_board_info_init();

	if (ops != NULL) {
		err = (ops->get_serial_no)((void *)buf);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

		while (buf[i] != '\0') {
			if (!isdigit(buf[i]) && !isalpha(buf[i])) {
				pr_warn("Serial number invalid!\n");
				buf[0] = '\0';
				err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
				break;
			}
			i++;
		}
	} else {
		pr_warn("No serial number available, using 0\n");
		memset(buf, '0', SNO_SIZE);
		buf[SNO_SIZE] = '\0';
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_get_mac_address(mac_addr_type_t type, uint8_t *mac_bytes,
										uint8_t *mac_string)
{
	tegrabl_error_t err;
	struct mac_addr mac_addr_info;

	if ((mac_bytes == NULL) && (mac_string == NULL)) {
		return TEGRABL_NO_ERROR;
	}

	mac_addr_info.type = type;
	mac_addr_info.mac_byte_array = mac_bytes;
	mac_addr_info.mac_string = mac_string;

	if (!board_info_initialized)
		tegrabl_board_info_init();

	if (ops != NULL) {
		err = (ops->get_mac_addr)((void *)&mac_addr_info);
	} else {
		pr_warn("No MAC address type %d available\n", type);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 1);
	}

	return err;
}

tegrabl_error_t tegrabl_get_board_ids(void *id_info)
{
	tegrabl_error_t err;

	if (!board_info_initialized)
		tegrabl_board_info_init();

	if (ops != NULL) {
		err = (ops->get_board_ids)(id_info);
	} else {
		pr_warn("No board IDs available\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
	}

	return err;
}

bool is_valid_mac_addr(uint8_t *mac_addr)
{
	uint8_t mac_addr_zero[MAC_ADDR_SIZE_BYTES];
	uint8_t mac_addr_ff[MAC_ADDR_SIZE_BYTES];
	bool ret = false;

	if (mac_addr == NULL) {
		goto fail;
	}

	memset(mac_addr_zero, 0x0, MAC_ADDR_SIZE_BYTES);
	memset(mac_addr_ff, 0xFF, MAC_ADDR_SIZE_BYTES);

	if ((memcmp(mac_addr, mac_addr_zero, MAC_ADDR_SIZE_BYTES) != 0UL) &&
		(memcmp(mac_addr, mac_addr_ff, MAC_ADDR_SIZE_BYTES) != 0UL)) {
		ret = true;
	}

fail:
	return ret;
}
