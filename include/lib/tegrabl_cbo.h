/*
 * Copyright (c) 2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_CBO_H
#define INCLUDED_TEGRABL_CBO_H

#include <stdint.h>
#include <tegrabl_error.h>

#define CBO_PARTITION "CPUBL-CFG"

/* specifies number of storage options to boot from */
#define NUM_SECONDARY_STORAGE_DEVICES 5

/* specifies pluggable devices */
#define BOOT_INVALID			   -1
#define BOOT_DEFAULT				0 /*default is builtin*/
#define BOOT_FROM_NETWORK			1
#define BOOT_FROM_SD				2
#define BOOT_FROM_USB				3
/* Builtin storage specifies primary / secondary fixed storage where kernel is expected to be */
#define BOOT_FROM_BUILTIN_STORAGE	4

struct ip_info {
	bool is_dhcp_enabled;
	uint8_t static_ip[4];
	uint8_t ip_netmask[4];
	uint8_t ip_gateway[4];
	uint8_t tftp_server_ip[4];
};

struct cbo_info {
	int8_t *boot_priority;
	struct ip_info ip_info;
};

/**
* @brief read the CBO partition
*
* @param part_name name of partition
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_read_cbo(char *part_name);

/**
* @brief parse boot configuration from CBO partition data
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_cbo_parse_info(void);

/**
* @brief get new boot order
*
* @return pointer to the new boot order info or NULL in case of failure.
*/
int8_t *tegrabl_get_boot_order(void);

/**
* @brief get ip info
*
* @return ip_info struct containing all the ip's.
*/
struct ip_info tegrabl_get_ip_info(void);

#endif /* INCLUDED_TEGRABL_CBO_H */

