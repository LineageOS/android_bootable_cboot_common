/*
 * Copyright (c) 2018-2022, NVIDIA Corporation.  All Rights Reserved.
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
#define NUM_SECONDARY_STORAGE_DEVICES 6

/* specifies pluggable devices */
#define BOOT_DEFAULT				0 /*default is builtin*/
#define BOOT_FROM_NETWORK			1
#define BOOT_FROM_SD				2
#define BOOT_FROM_USB				3
/* Builtin storage specifies primary / secondary fixed storage where kernel is expected to be */
#define BOOT_FROM_BUILTIN_STORAGE	4
#define BOOT_FROM_NVME				5

#define IP_LEN					4

#define GUID_STR_LEN				36
#define GUID_STR_SIZE				(GUID_STR_LEN + 1)

struct ip_info {
	bool is_dhcp_enabled;
	uint8_t static_ip[IP_LEN];
	uint8_t ip_netmask[IP_LEN];
	uint8_t ip_gateway[IP_LEN];
	uint8_t tftp_server_ip[IP_LEN];
};

struct cbo_info {
	uint8_t *boot_priority;
	struct ip_info ip_info;
	char boot_pt_guid[GUID_STR_SIZE];
};

struct boot_devices {
	char *name;
	uint8_t device_id;
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
* @brief parse boot configuration from CBO partition or set to default
*
* @param is_cbo_read is CBO partition read or not
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_cbo_parse_info(bool is_cbo_read);

/**
* @brief set/update boot order
*
* @param count number of boot devices
* @param boot_order pointer to boot device strings in priority order
*
* @return TEGRABL_NO_ERROR if success. Error code in case of failure.
*/
tegrabl_error_t tegrabl_set_boot_order(uint32_t count, const char **boot_order);

/**
* @brief set/update ip info
*
* @param var_name name of variable to set/udpate
* @param ip pointer to ip value
* @param is_dhcp_enabled bool value of dhcp
*/
void tegrabl_set_ip_info(const char *var_name, uint8_t *ip, bool is_dhcp_enabled);

/**
 * @brief set/update boot partition GUID
 *
 * @param var_name name of variable to set/udpate
 * @param guid pointer to GUID
 */
void tegrabl_set_boot_pt_guid(const char *var_name, const char *guid);

/**
* @brief clear boot order
*/
void tegrabl_clear_boot_order(void);

/**
* @brief clear ip info
*
* @param var_name name of variable to clear
*/
void tegrabl_clear_ip_info(const char *var_name);

/**
 * @brief clear boot partition GUID
 */
void tegrabl_clear_boot_pt_guid(void);

/**
* @brief print boot order
*/
void tegrabl_print_boot_order(void);

/**
* @brief print ip info
*
* @param var_name name of variable to print
*/
void tegrabl_print_ip_info(const char *var_name);

/**
 * @brief print boot partition GUID
 */
void tegrabl_print_boot_pt_guid(void);

/**
* @brief get new boot order
*
* @return pointer to the new boot order info or NULL in case of failure.
*/
uint8_t *tegrabl_get_boot_order(void);

/**
* @brief get ip info
*
* @return ip_info struct containing all the ip's.
*/
struct ip_info tegrabl_get_ip_info(void);

/**
 * @brief get boot partition GUID
 *
 * @return guid boot parition type GUID
 */
char *tegrabl_get_boot_pt_guid(void);

/**
* @brief check if given variable is boot_configuration variable
*
* @param var_name variable name to check
*
* @return true or false
*/
bool is_var_boot_cfg(const char *var_name);

/**
* @brief get boot_dev_order[] array
*
* @return pointer to the boot_dev_order[] array of boot devices.
*/
char **tegrabl_get_boot_dev_order(void);

/**
* @brief print boot_dev_order[] array
*/
void tegrabl_print_boot_dev_order(void);

/**
* @brief map the boot_dev name to device_id
*
* @param boot_dev name of the boot_dev
* @param device_id pointer to device_id
*
* @return pointer to the boot_dev after the matched base boot device name
*         NULL is no matched base boot device name
*/
char *tegrabl_cbo_map_boot_dev(char *boot_dev, uint8_t *device_id);

#endif /* INCLUDED_TEGRABL_CBO_H */
