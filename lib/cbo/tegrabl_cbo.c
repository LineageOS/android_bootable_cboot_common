/*
 * Copyright (c) 2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_CBO

#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <tegrabl_error.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_partition_manager.h>
#include <tegrabl_cbo.h>
#include <string.h>

#if defined(CONFIG_ENABLE_CBO)

static struct cbo_info g_cbo_info;

tegrabl_error_t tegrabl_read_cbo(char *part_name)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *cbo_buf = NULL;
	struct tegrabl_partition partition;
	uint64_t partition_size;

	pr_debug("%s: Entry\n", __func__);

	if (part_name == NULL) {
		pr_error("%s: invalid partition name\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	err = tegrabl_partition_open(part_name, &partition);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed to open %s partition\n", __func__, part_name);
		goto fail;
	}
	partition_size = partition.partition_info->total_size;
	pr_debug("%s: CBO partiton opened successfully.\n", __func__);

	cbo_buf = tegrabl_calloc(partition_size, 1);
	if (cbo_buf == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		pr_error("%s: Not enough memory for buffer (%ld bytes)\n", __func__, partition_size);
		goto fail;
	}

	err = tegrabl_partition_read(&partition, cbo_buf, partition_size);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s Failed to read %s partition\n", __func__, part_name);
		goto fail;
	}
	pr_debug("%s: CBO data read successfully at %p\n", __func__, cbo_buf);

	err = tegrabl_dt_set_fdt_handle(TEGRABL_DT_CBO, cbo_buf);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: cbo-dtb init failed\n", __func__);
		goto fail;
	}

	pr_debug("%s: EXIT\n\n", __func__);

fail:
	if (err != TEGRABL_NO_ERROR) {
		tegrabl_free(cbo_buf);
	}

	return err;
}

static int8_t *parse_boot_order(void *fdt, int32_t offset)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	const char **boot_order = NULL;
	int8_t *boot_priority = NULL;
	uint32_t count, i;

	pr_debug("%s: Entry\n", __func__);

	err = tegrabl_dt_get_prop_count_strings(fdt, offset, "boot-order", &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed to get number of boot devices from CBO file.\n", __func__);
		goto fail;
	}

	pr_debug("%s: num of boot devices = %u\n", __func__, count);

	boot_order = tegrabl_calloc(sizeof(char *), count);
	if (boot_order == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		pr_error("%s: memory allocation failed for boot_order\n", __func__);
		goto fail;
	}

	err = tegrabl_dt_get_prop_string_array(fdt, offset, "boot-order", boot_order, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: boot-order info not found in CBO options file\n", __func__);
		goto fail;
	}

	pr_info("%s: boot-order :-\n", __func__);
	for (i = 0; i < count; i++) {
		pr_info("%d.%s\n", i + 1, boot_order[i]);
	}

	boot_priority = tegrabl_calloc(sizeof(uint8_t), count + 1);
	if (boot_priority == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 2);
		pr_error("%s: memory allocation failed for boot_priority\n", __func__);
		goto fail;
	}

	/* mapping boot-devices read from dtb with linux_load code, this implementation will be updated
	  * to add more device names and enums as supported in code.
	  * Support will be added to send the controller info also along with boot_device name, however
	  * that requires linux_load code to be updated accordingly.
	  */
	for (i = 0; i < count; i++) {
		if (!strncmp(boot_order[i], "sd", 2)) {
			boot_priority[i] = BOOT_FROM_SD;
		} else if (!strncmp(boot_order[i], "usb", 3)) {
			boot_priority[i] = BOOT_FROM_USB;
		} else if (!strncmp(boot_order[i], "net", 3)) {
			boot_priority[i] = BOOT_FROM_NETWORK;
		} else if (!strncmp(boot_order[i], "emmc", 4)) {
			boot_priority[i] = BOOT_FROM_BUILTIN_STORAGE;
		} else if (!strncmp(boot_order[i], "ufs", 3)) {
			boot_priority[i] = BOOT_FROM_BUILTIN_STORAGE;
		} else if (!strncmp(boot_order[i], "sata", 4)) {
			boot_priority[i] = BOOT_FROM_BUILTIN_STORAGE;
		} else {
			boot_priority[i] = BOOT_INVALID;
		}
	}
	pr_debug("%s: EXIT\n", __func__);

fail:
	tegrabl_free(boot_order);
	if (err != TEGRABL_NO_ERROR) {
		tegrabl_free(boot_priority);
		return NULL;
	}
	return boot_priority;
}

static void print_ip(char *name, uint8_t *ip)
{
	pr_info("%s: %d.%d.%d.%d\n", name, ip[0], ip[1], ip[2], ip[3]);
}

static void parse_ip_info(void *fdt, int32_t offset, struct ip_info *ip_info)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t count;
	const char *status;

	err = tegrabl_dt_get_prop_u8_array(fdt, offset, "tftp-server-ip", 0, ip_info->tftp_server_ip, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_info("%s: tftp-server-ip info not found in CBO options file\n", __func__);
	} else {
		print_ip("tftp-ip", ip_info->tftp_server_ip);
	}

	status = fdt_getprop(fdt, offset, "dhcp-enabled", NULL);
	if (status != NULL) {
		pr_warn("%s: static-ip info is not required, only tftp-server-ip is required.\n", __func__);
		ip_info->is_dhcp_enabled = true;
		goto skip_static_ip_parse;
	} else {
		ip_info->is_dhcp_enabled = false;
	}

	err = tegrabl_dt_get_prop_u8_array(fdt, offset, "static-ip", 0, ip_info->static_ip, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("%s: static-ip info not found in CBO options file\n", __func__);
		goto skip_static_ip_parse;
	} else {
		print_ip("static-ip", ip_info->static_ip);
	}

	err = tegrabl_dt_get_prop_u8_array(fdt, offset, "ip-netmask", 0, ip_info->ip_netmask, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("%s: netmask for static-ip not found in CBO options file\n", __func__);
		goto skip_static_ip_parse;
	} else {
		print_ip("netmask", ip_info->ip_netmask);
	}

	err = tegrabl_dt_get_prop_u8_array(fdt, offset, "ip-gateway", 0, ip_info->ip_gateway, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("%s: gateway-ip for static-ip not found in CBO options file\n", __func__);
		goto skip_static_ip_parse;
	} else {
		print_ip("gateway", ip_info->ip_gateway);
	}

skip_static_ip_parse:
	if (err != TEGRABL_NO_ERROR) {
		/* all 3 ip's must be available else clear all */
		memset(ip_info->static_ip, 0, 4);
		memset(ip_info->ip_netmask, 0, 4);
		memset(ip_info->ip_gateway, 0, 4);
	}
	return;
}

tegrabl_error_t tegrabl_cbo_parse_info(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *fdt = NULL;
	int32_t offset = -1;

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_CBO, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: get fdt handle failed for cbo-dtb\n", __func__);
		goto fail;
	}

	err = tegrabl_dt_get_node_with_path(fdt, "/boot-configuration", &offset);
	if ((err != TEGRABL_NO_ERROR) || (offset < 0)) {
		pr_error("%s: \"boot-configuration\" not found in CBO file.\n", __func__);
		goto fail;
	}

	g_cbo_info.boot_priority = parse_boot_order(fdt, offset);

	parse_ip_info(fdt, offset, &g_cbo_info.ip_info);

fail:
	return err;
}

int8_t *tegrabl_get_boot_order(void)
{
	return g_cbo_info.boot_priority;
}

struct ip_info tegrabl_get_ip_info(void)
{
	return g_cbo_info.ip_info;
}

#endif	/* CONFIG_ENABLE_CBO */

