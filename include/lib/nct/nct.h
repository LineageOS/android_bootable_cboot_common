/*
 * Copyright (c) 2013 - 2017, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NCT_INCLUDED_H
#define NCT_INCLUDED_H

#include <stdint.h>

#define MAX_TNSPEC_LEN     (2 * 1024 * 1024) /* @2MB */
#define TNS_MAGIC_ID       "TNS1" /* 0x544E5331 */
#define TNS_MAGIC_ID_LEN   4

#define NCT_MAGIC_ID       "nVCt" /* 0x6E564374 */
#define NCT_MAGIC_ID_LEN   4
#define NCT_PART_NAME      "NCT"

#define NCT_FORMAT_VERSION    0x00010000 /* 0xABCDabcd (ABCD.abcd) */

#define NCT_ENTRY_OFFSET       0x4000
#define MAX_NCT_ENTRY          512
#define MAX_NCT_DATA_SIZE      1024
#define NCT_ENTRY_SIZE         1040
#define NCT_ENTRY_DATA_OFFSET  12

#define NCT_NUM_UUID_ENTRIES     1
#define NCT_UUID_ENTRY_SIZE     64
#define UUIDS_PER_NCT_ENTRY     (MAX_NCT_DATA_SIZE / NCT_UUID_ENTRY_SIZE)

#define NCT_MAX_SPEC_LENGTH		64 /* SW spec max length */

/* macro nct tag */
typedef uint32_t nct_tag_t;
#define NCT_TAG_1B_SINGLE 0x10
#define NCT_TAG_2B_SINGLE 0x20
#define NCT_TAG_4B_SINGLE 0x40
#define NCT_TAG_STR_SINGLE 0x80
#define NCT_TAG_1B_ARRAY 0x1A
#define NCT_TAG_2B_ARRAY 0x2A
#define NCT_TAG_4B_ARRAY 0x4A
#define NCT_TAG_STR_ARRAY 0x8A

/* macro nct id */
typedef uint32_t nct_id_t;
#define NCT_ID_START 0
#define NCT_ID_SERIAL_NUMBER NCT_ID_START /* ID: 0 */
#define NCT_ID_WIFI_ADDR 1					 /* ID: 1 */
#define NCT_ID_BT_ADDR 2                      /* ID: 2 */
#define NCT_ID_CM_ID 3                        /* ID: 3 */
#define NCT_ID_LBH_ID 4                       /* ID: 4 */
#define NCT_ID_FACTORY_MODE 5                 /* ID: 5 */
#define NCT_ID_RAMDUMP 6                      /* ID: 6 */
#define NCT_ID_TEST 7                         /* ID: 7 */
#define NCT_ID_BOARD_INFO 8                   /* ID: 8 */
#define NCT_ID_GPS_ID 9                       /* ID: 9 */
#define NCT_ID_LCD_ID 10                       /* ID:10 */
#define NCT_ID_ACCELEROMETER_ID 11             /* ID:11 */
#define NCT_ID_COMPASS_ID 12                   /* ID:12 */
#define NCT_ID_GYROSCOPE_ID 13                 /* ID:13 */
#define NCT_ID_LIGHT_ID 14                     /* ID:14 */
#define NCT_ID_CHARGER_ID 15                   /* ID:15 */
#define NCT_ID_TOUCH_ID 16                     /* ID:16 */
#define NCT_ID_FUELGAUGE_ID 17                 /* ID:17 */
#define NCT_ID_WCC 18                          /* ID:18 */
#define NCT_ID_ETH_ADDR 19                     /* ID:19 */
#define NCT_ID_UNUSED3 20                      /* ID:20 */
#define NCT_ID_UNUSED4 21                      /* ID:21 */
#define NCT_ID_UNUSED5 22                      /* ID:22 */
#define NCT_ID_UNUSED6 23                      /* ID:23 */
#define NCT_ID_UNUSED7 24                      /* ID:24 */
#define NCT_ID_UNUSED8 25                      /* ID:25 */
#define NCT_ID_UNUSED9 26                      /* ID:26 */
#define NCT_ID_UNUSED10 27                     /* ID:27 */
#define NCT_ID_UNUSED11 28                     /* ID:28 */
#define NCT_ID_UNUSED12 29                     /* ID:29 */
#define NCT_ID_UNUSED13 30                     /* ID:30 */
#define NCT_ID_UNUSED14 31                     /* ID:31 */
#define NCT_ID_UNUSED15 32                     /* ID:32 */
#define NCT_ID_UNUSED16 33                     /* ID:33 */
#define NCT_ID_UNUSED17 34                     /* ID:34 */
#define NCT_ID_UNUSED18 35                     /* ID:35 */
#define NCT_ID_UNUSED19 36                     /* ID:36 */
#define NCT_ID_UNUSED20 37                     /* ID:37 */
#define NCT_ID_BATTERY_MODEL_DATA 38           /* ID:38 */
#define NCT_ID_DEBUG_CONSOLE_PORT_ID 39        /* ID:39 */
#define NCT_ID_BATTERY_MAKE 40                 /* ID:40 */
#define NCT_ID_BATTERY_COUNT 41                /* ID:41 */
#define NCT_ID_SPEC 42                         /* ID:42 */
#define NCT_ID_UUID 43                         /* ID:43 */
#define NCT_ID_UUID_END  (NCT_ID_UUID + NCT_NUM_UUID_ENTRIES - 1)
#define NCT_ID_END NCT_ID_UUID_END
#define NCT_ID_DISABLED 0xEEEE
#define NCT_ID_MAX 0xFFFF

struct nct_serial_number {
	uint8_t sn[30];
};

struct nct_wifi_addr {
	uint8_t addr[6];
};

struct nct_bt_addr {
	uint8_t addr[6];
};

struct nct_eth_addr {
	uint8_t addr[6];
};

struct nct_cm_id {
	uint16_t id;
};

struct nct_lbh_id {
	uint16_t id;
};

struct nct_factory_mode {
	uint32_t flag;
};

struct nct_ramdump {
	uint32_t flag;
};

struct nct_wcc {
	uint32_t flag;
};

struct nct_id_test {
	uint8_t addr[257];
};

struct nct_board_info {
	uint32_t proc_board_id;
	uint32_t proc_sku;
	uint32_t proc_fab;
	uint32_t pmu_board_id;
	uint32_t pmu_sku;
	uint32_t pmu_fab;
	uint32_t display_board_id;
	uint32_t display_sku;
	uint32_t display_fab;
};

struct nct_debug_port_id {
	uint32_t port_id;
};

struct nct_spec {
	uint8_t data[MAX_NCT_DATA_SIZE];
};

struct nct_uuid_container {
	char id[NCT_UUID_ENTRY_SIZE];
};

TEGRABL_PACKED(
union nct_item {
	struct nct_serial_number	serial_number;
	struct nct_wifi_addr		wifi_addr;
	struct nct_bt_addr			bt_addr;
	struct nct_cm_id			cm_id;
	struct nct_lbh_id			lbh_id;
	struct nct_factory_mode		factory_mode;
	struct nct_ramdump			ramdump;
	struct nct_wcc				wcc;
	struct nct_eth_addr			eth_addr;
	struct nct_id_test			id_test;
	struct nct_board_info		board_info;
	struct nct_lbh_id			gps_id;
	struct nct_lbh_id			lcd_id;
	struct nct_lbh_id			accelerometer_id;
	struct nct_lbh_id			compass_id;
	struct nct_lbh_id			gyroscope_id;
	struct nct_lbh_id			light_id;
	struct nct_lbh_id			charger_id;
	struct nct_lbh_id			touch_id;
	struct nct_lbh_id			fuelgauge_id;
	struct nct_debug_port_id	debug_port;
	struct nct_spec				spec;
	struct nct_uuid_container	uuids[UUIDS_PER_NCT_ENTRY];
	uint8_t						u8data;
	uint16_t					u16data;
	uint32_t					u32data;
	uint8_t						u8[MAX_NCT_DATA_SIZE];
	uint16_t					u16[MAX_NCT_DATA_SIZE/2];
	uint32_t					u32[MAX_NCT_DATA_SIZE/4];
}
);

struct nct_entry {
	uint32_t index;
	uint32_t reserved[2];
	union nct_item data;
	uint32_t checksum;
};

/*
 * tnspec in NCT lies in space between NCT header and first NCT entry
 * (at 0x4000).
 * tns_off: offset where tnspec lies from the start of NCT partition.
 * tns_len: length of tnspec
 */
struct nct_part_head {
	uint32_t magic_id;
	uint32_t vendor_id;
	uint32_t product_id;
	uint32_t version;
	uint32_t revision;
	uint32_t tns_id;
	uint32_t tns_off;
	uint32_t tns_len;
	uint32_t tns_crc32;
};

struct nct_cust_info {
	struct nct_board_info board_info;
};

#endif /* NCT_INCLUDED_H */
