/*
 * Copyright (c) 2016, NVIDIA Corporation.  All rights reserved.
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
#include "board_info_local.h"

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

#if defined(CONFIG_ENABLE_NCT)
static void tegrabl_board_info_init(void)
{
	tegrabl_error_t err;
	int node = 0;
	int val;
	void *fdt;

	if (board_info_initialized)
		return; /* Return early */

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get BL-dtb handle\n");
		return;
	}

	/* check dt node /chosen/board-has-eeprom */
	err = tegrabl_dt_get_node_with_path(fdt, "/chosen", &node);
	if (err == TEGRABL_NO_ERROR) {
		err = tegrabl_dt_get_prop(fdt, node, "board-has-eeprom", 4, &val);
		if (err == TEGRABL_NO_ERROR) {
			ops = eeprom_get_ops(); /* Retrieve info from EEPROM */
			goto done;
		}
	}

	ops = nct_get_ops(); /* Retrieve info from NCT as a default */

done:
	board_info_initialized = true;
}
#else
static void tegrabl_board_info_init(void)
{
	if (board_info_initialized)
		return; /* Return early */

#if defined(CONFIG_ENABLE_EEPROM)
	ops = eeprom_get_ops(); /* Retrieve info from EEPROM */
#endif

	board_info_initialized = true;
}
#endif
/* CONFIG_ENABLE_NCT */

tegrabl_error_t tegrabl_get_serial_no(uint8_t *buf)
{
	if (!board_info_initialized)
		tegrabl_board_info_init();

	if (ops)
		return (ops->get_serial_no)((void *)buf);
	else {
		memset(buf, '0', SNO_SIZE);
		buf[SNO_SIZE] = '\0';
		return TEGRABL_NO_ERROR;
	}
}

tegrabl_error_t tegrabl_get_mac_address(enum mac_addr_type type, uint8_t *buf)
{
	struct mac_addr mac_addr_info;

	mac_addr_info.type = type;
	mac_addr_info.buf = buf;

	if (!board_info_initialized)
		tegrabl_board_info_init();

	return (ops->get_mac_addr)((void *)&mac_addr_info);
}

tegrabl_error_t tegrabl_get_board_ids(void *id_info)
{
	if (!board_info_initialized)
		tegrabl_board_info_init();

	return (ops->get_board_ids)(id_info);
}

