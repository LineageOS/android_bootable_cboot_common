/*
 * Copyright (c) 2018-2020, NVIDIA Corporation.  All Rights Reserved.
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

#define NVIDIA_BOOT_PARTITION_GUID	"6637b54f-c21b-48a7-952e-a8d071029d6b"
#define CBO_DT_SIZE					(4 * 1024) /* 4KB */

static struct cbo_info g_cbo_info;

/*
 * p_boot_dev_order is a pointer to an array of pointers pointed to a boot_dev string.
 * Those strings are the boot_device specified in "boot-order=" in cbo.dtb, or in default_boot_dev_order
[].
 */
static char **p_boot_dev_order;

static const char *default_boot_dev_order[] = {
	/* Specified in the order of priority from top to bottom */
	"sd",
	"usb",
	"nvme",
	"emmc",
	"net",
};

static uint8_t default_boot_order[NUM_SECONDARY_STORAGE_DEVICES] = {
	/* Specified in the order of priority from top to bottom */
	BOOT_FROM_SD,
	BOOT_FROM_USB,
	BOOT_FROM_NVME,
	BOOT_FROM_BUILTIN_STORAGE,
	BOOT_FROM_NETWORK,
	BOOT_DEFAULT,
};

char *boot_cfg_vars[] = {
	"boot-order",
	"tftp-server-ip",
	"dhcp-enabled",
	"static-ip",
	"ip-netmask",
	"ip-gateway",
	"boot_pt_guid",
};

static struct boot_devices g_boot_devices[] = {
	{"sd",		BOOT_FROM_SD},
	{"usb",		BOOT_FROM_USB},
	{"net",		BOOT_FROM_NETWORK},
	{"emmc",	BOOT_FROM_BUILTIN_STORAGE},
	{"ufs",		BOOT_FROM_BUILTIN_STORAGE},
	{"sata",	BOOT_FROM_BUILTIN_STORAGE},
	{"nvme",	BOOT_FROM_NVME},
};

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

	cbo_buf = tegrabl_calloc(CBO_DT_SIZE, 1);
	if (cbo_buf == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		pr_error("%s: Not enough memory for buffer (%ld bytes)\n", __func__, partition_size);
		goto fail;
	}

	err = tegrabl_partition_read(&partition, cbo_buf, CBO_DT_SIZE);
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

static tegrabl_error_t map_boot_priority(uint32_t count, const char **boot_order, uint8_t **boot_priority)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t *boot_p;
	uint32_t i, j;

	boot_p = tegrabl_calloc(sizeof(uint8_t), count + 1);
	if (boot_p == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		pr_error("%s: memory allocation failed for boot_priority\n", __func__);
		goto fail;
	}

	/* mapping boot-devices read from dtb with linux_load code, this implementation will be updated
	  * to add more device names and enums as supported in code.
	  * Support will be added to send the controller info also along with boot_device name, however
	  * that requires linux_load code to be updated accordingly.
	  */
	for (i = 0; i < count; i++) {
		for (j = 0; j < ARRAY_SIZE(g_boot_devices); j++) {
			if (!strncmp(boot_order[i], g_boot_devices[j].name, strlen(g_boot_devices[j].name))) {
				boot_p[i] = g_boot_devices[j].device_id;
				break;
			}
		}
	}
	boot_p[i] = BOOT_DEFAULT;

	*boot_priority = boot_p;

fail:
	if (err != TEGRABL_NO_ERROR) {
		tegrabl_free(boot_p);
		boot_p = NULL;
	}
	return err;
}

static tegrabl_error_t parse_boot_order(void *fdt, int32_t offset, uint8_t **boot_priority)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	const char **boot_order = NULL;
	uint32_t count, i;

	pr_debug("%s: Entry\n", __func__);

	err = tegrabl_dt_get_prop_count_strings(fdt, offset, boot_cfg_vars[0], &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get number of boot devices from CBO file.\n");
		goto fail;
	}
	pr_debug("Num of boot devices = %u\n", count);

	/* use count+1 to NULL terminate the array */
	boot_order = tegrabl_calloc(sizeof(char *), count);
	if (boot_order == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 2);
		pr_error("Memory allocation failed for boot_order\n");
		goto fail;
	}

	err = tegrabl_dt_get_prop_string_array(fdt, offset, boot_cfg_vars[0], boot_order, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("boot-order info not found in CBO options file\n");
		goto fail;
	}

	err = tegrabl_set_boot_order(count, boot_order);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_info("boot-order :-\n");
	for (i = 0; i < count; i++) {
		pr_info("%d.%s\n", i + 1, boot_order[i]);
	}

	err = map_boot_priority(count, boot_order, boot_priority);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	pr_debug("%s: EXIT\n", __func__);

fail:
	tegrabl_free(boot_order);
	return err;
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

	err = tegrabl_dt_get_prop_u8_array(fdt, offset, boot_cfg_vars[1], 0, ip_info->tftp_server_ip, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("%s: tftp-server-ip info not found in CBO options file\n", __func__);
	} else {
		print_ip("tftp-server-ip", ip_info->tftp_server_ip);
	}

	status = fdt_getprop(fdt, offset, boot_cfg_vars[2], NULL);
	if (status != NULL) {
		pr_warn("%s: static-ip info is not required, only tftp-server-ip is required.\n", __func__);
		ip_info->is_dhcp_enabled = true;
		goto skip_static_ip_parse;
	} else {
		ip_info->is_dhcp_enabled = false;
	}

	err = tegrabl_dt_get_prop_u8_array(fdt, offset, boot_cfg_vars[3], 0, ip_info->static_ip, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("%s: static-ip info not found in CBO options file\n", __func__);
		goto skip_static_ip_parse;
	} else {
		print_ip("static-ip", ip_info->static_ip);
	}

	err = tegrabl_dt_get_prop_u8_array(fdt, offset, boot_cfg_vars[4], 0, ip_info->ip_netmask, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("%s: netmask for static-ip not found in CBO options file\n", __func__);
		goto skip_static_ip_parse;
	} else {
		print_ip("ip-netmask", ip_info->ip_netmask);
	}

	err = tegrabl_dt_get_prop_u8_array(fdt, offset, boot_cfg_vars[5], 0, ip_info->ip_gateway, &count);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("%s: gateway-ip for static-ip not found in CBO options file\n", __func__);
		goto skip_static_ip_parse;
	} else {
		print_ip("ip-gateway", ip_info->ip_gateway);
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

static void parse_boot_pt_guid(void *fdt, int32_t offset)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	char *guid = NULL;
	uint8_t guid_str_len;

	pr_debug("%s: Entry\n", __func__);

	err = tegrabl_dt_get_prop_string(fdt, offset, boot_cfg_vars[6], (const char **)&guid);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Failed to parse GUID\n");
		goto fail;
	}

	guid_str_len = strlen(guid);
	if (guid_str_len == GUID_STR_LEN) {
		strncpy(g_cbo_info.boot_pt_guid, guid, guid_str_len);
		g_cbo_info.boot_pt_guid[GUID_STR_LEN] = '\0';
		pr_info("Boot partition GUID: %s\n", g_cbo_info.boot_pt_guid);
	} else {
		pr_error("Invalid GUID (len: %u, expected len: %u)\n", guid_str_len, GUID_STR_LEN);
	}

fail:
	return;
}

tegrabl_error_t tegrabl_cbo_parse_info(bool is_cbo_read)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *fdt = NULL;
	int32_t offset = -1;
	bool set_default_boot_order = true;

	memset(&g_cbo_info.ip_info, 0, sizeof(g_cbo_info.ip_info));
	memset(&g_cbo_info.boot_pt_guid, 0, GUID_STR_SIZE);

	if (is_cbo_read) {
		err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_CBO, &fdt);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s: get fdt handle failed for cbo-dtb\n", __func__);
			goto default_boot_order;
		}

		err = tegrabl_dt_get_node_with_path(fdt, "/boot-configuration", &offset);
		if ((err != TEGRABL_NO_ERROR) || (offset < 0)) {
			pr_error("%s: \"boot-configuration\" not found in CBO file.\n", __func__);
			goto default_boot_order;
		}

		err = parse_boot_order(fdt, offset, &g_cbo_info.boot_priority);
		if (err == TEGRABL_NO_ERROR) {
			set_default_boot_order = false;
		}

		parse_ip_info(fdt, offset, &g_cbo_info.ip_info);

		parse_boot_pt_guid(fdt, offset);
	}

default_boot_order:
	if (set_default_boot_order) {
		g_cbo_info.boot_priority = tegrabl_calloc(sizeof(uint8_t), NUM_SECONDARY_STORAGE_DEVICES);
		if (g_cbo_info.boot_priority == NULL) {
			err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 3);
			pr_error("%s: memory allocation failed for boot_priority\n", __func__);
			goto fail;
		}
		memcpy(g_cbo_info.boot_priority, default_boot_order, sizeof(default_boot_order));

		pr_info("Using default boot order\n");
		err = tegrabl_set_boot_order(ARRAY_SIZE(default_boot_dev_order), default_boot_dev_order);
	}

	tegrabl_print_boot_dev_order();

fail:
	tegrabl_free(fdt);
	return err;
}

tegrabl_error_t tegrabl_set_boot_order(uint32_t count, const char **boot_order)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t i, j;
	char *src, *dst;

	/* clear the boot priority and set the new values */
	tegrabl_clear_boot_order();

	/* use count+1 to NULL terminate the array */
	p_boot_dev_order = tegrabl_calloc(sizeof(char *), count + 1);
	if (p_boot_dev_order == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 2);
		pr_error("Memory allocation failed for p_boot_dev_order\n");
		goto fail;
	}

	/* save strings in boot_order[] to our own memory (pointed by p_boot_dev_order) */
	for (i = 0; i < count; i++) {
		src = (char *)boot_order[i];
		if (src == NULL) {
			goto exit;
		}
		dst = tegrabl_calloc(sizeof(char), strlen(src) + 1);
		if (dst == NULL) {
			err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 4);
			pr_error("%s: memory allocation failed for boot_order[%u]\n", __func__, i);
			/* free previously allocated memory. */
			for (j = 0; j < i; ++j) {
				if (p_boot_dev_order[j]) {
					tegrabl_free(p_boot_dev_order[j]);
				}
			}
			goto fail;
		}
		pr_debug("saving %s\n", src);
		strcpy(dst, src);
		p_boot_dev_order[i] = dst;
	}
	p_boot_dev_order[i] = NULL;

	err = map_boot_priority(count, boot_order, &g_cbo_info.boot_priority);
	if (err != TEGRABL_NO_ERROR) {
		pr_info("Error updating boot-priority\n");
	}

exit:
fail:
	return err;
}

void tegrabl_set_ip_info(const char *var_name, uint8_t *ip, bool is_dhcp_enabled)
{
	if (!strcmp(var_name, "dhcp-enabled")) {
		g_cbo_info.ip_info.is_dhcp_enabled = is_dhcp_enabled;
	} else if (!strcmp(var_name, "tftp-server-ip")) {
		memcpy(g_cbo_info.ip_info.tftp_server_ip, ip, 4);
	} else if (!strcmp(var_name, "static-ip")) {
		memcpy(g_cbo_info.ip_info.static_ip, ip, 4);
	} else if (!strcmp(var_name, "ip-netmask")) {
		memcpy(g_cbo_info.ip_info.ip_netmask, ip, 4);
	} else if (!strcmp(var_name, "ip-gateway")) {
		memcpy(g_cbo_info.ip_info.ip_gateway, ip, 4);
	} else {
		pr_info("invalid variable\n");
	}
}

void tegrabl_set_boot_pt_guid(const char *var_name, const char *guid)
{
	uint8_t guid_str_len;

	guid_str_len = strlen(guid);

	if (guid_str_len == GUID_STR_LEN) {
		strncpy(g_cbo_info.boot_pt_guid, guid, guid_str_len);
		g_cbo_info.boot_pt_guid[GUID_STR_LEN] = '\0';
	} else {
		pr_error("Invalid GUID (len: %u, expected len: %u)\n", guid_str_len, GUID_STR_LEN);
	}
}

void tegrabl_clear_boot_order(void)
{
	char *boot_dev;
	uint32_t i = 0;

	if (p_boot_dev_order) {
		while (true) {
			boot_dev = p_boot_dev_order[i];
			if (boot_dev == NULL) {
				break;
			}
			tegrabl_free(boot_dev);
			++i;
		}
		*p_boot_dev_order = NULL;
	}

	if (g_cbo_info.boot_priority != NULL) {
		tegrabl_free(g_cbo_info.boot_priority);
		g_cbo_info.boot_priority = NULL;
	}
}

void tegrabl_clear_ip_info(const char *var_name)
{
	if (!strcmp(var_name, "dhcp-enabled")) {
		g_cbo_info.ip_info.is_dhcp_enabled = false;
	} else if (!strcmp(var_name, "tftp-server-ip")) {
		memset(g_cbo_info.ip_info.tftp_server_ip, 0, 4);
	} else if (!strcmp(var_name, "static-ip")) {
		memset(g_cbo_info.ip_info.static_ip, 0, 4);
	} else if (!strcmp(var_name, "ip-netmask")) {
		memset(g_cbo_info.ip_info.ip_netmask, 0, 4);
	} else if (!strcmp(var_name, "ip-gateway")) {
		memset(g_cbo_info.ip_info.ip_gateway, 0, 4);
	} else {
		pr_info("invalid variable\n");
	}
}

void tegrabl_clear_boot_pt_guid(void)
{
	memset(g_cbo_info.boot_pt_guid, 0, GUID_STR_SIZE);
}

void tegrabl_print_boot_order(void)
{
	uint32_t i, j;

	if (g_cbo_info.boot_priority == NULL) {
		pr_info("boot-order is not set\n");
		return;
	}

	pr_info("boot-order :-\n");
	for (i = 0; g_cbo_info.boot_priority[i] != BOOT_DEFAULT; i++) {
		for (j = 0; j < ARRAY_SIZE(g_boot_devices); j++) {
			if (g_cbo_info.boot_priority[i] == g_boot_devices[j].device_id) {
				pr_info("%d.%s\n", i + 1, g_boot_devices[j].name);
				break;
			}
		}
	}
}

void tegrabl_print_ip_info(const char *var_name)
{
	if (!strcmp(var_name, "dhcp-enabled")) {
		pr_info("dhcp_enabled is %s\n", g_cbo_info.ip_info.is_dhcp_enabled ? "set" : "not set");
	} else if (!strcmp(var_name, "tftp-server-ip")) {
		print_ip("tftp-server-ip", g_cbo_info.ip_info.tftp_server_ip);
	} else if (!strcmp(var_name, "static-ip")) {
		print_ip("static-ip", g_cbo_info.ip_info.static_ip);
	} else if (!strcmp(var_name, "ip-netmask")) {
		print_ip("ip-netmask", g_cbo_info.ip_info.ip_netmask);
	} else if (!strcmp(var_name, "ip-gateway")) {
		print_ip("ip-gateway", g_cbo_info.ip_info.ip_gateway);
	} else {
		pr_info("invalid variable\n");
	}
}

void tegrabl_print_boot_pt_guid(void)
{
	pr_info("%s\n", g_cbo_info.boot_pt_guid);
}

uint8_t *tegrabl_get_boot_order(void)
{
	return g_cbo_info.boot_priority;
}

struct ip_info tegrabl_get_ip_info(void)
{
	return g_cbo_info.ip_info;
}

char *tegrabl_get_boot_pt_guid(void)
{
	if (g_cbo_info.boot_pt_guid[0] == 0) {
		memcpy(&g_cbo_info.boot_pt_guid, NVIDIA_BOOT_PARTITION_GUID, GUID_STR_SIZE);
	}
	return g_cbo_info.boot_pt_guid;
}

bool is_var_boot_cfg(const char *var_name)
{
	unsigned int i = 0;

	while (i < ARRAY_SIZE(boot_cfg_vars)) {
		if (!strcmp(boot_cfg_vars[i++], var_name)) {
			return true;
		}
	}

	return false;
}

char **tegrabl_get_boot_dev_order(void)
{
	return p_boot_dev_order;
}

void tegrabl_print_boot_dev_order(void)
{
	uint32_t i;
	char *boot_dev;

	pr_info("boot-dev-order :-\n");

	i = 0;
	while (true) {
		boot_dev = p_boot_dev_order[i];
		if (boot_dev == NULL) {
			break;
		}

		pr_info("%d.%s\n", i + 1, boot_dev);
		++i;
	}
}

char *tegrabl_cbo_map_boot_dev(char *boot_dev, uint8_t *device_id)
{
	uint32_t j;

	for (j = 0; j < ARRAY_SIZE(g_boot_devices); j++) {
		if (!strncmp(boot_dev, g_boot_devices[j].name, strlen(g_boot_devices[j].name))) {
			*device_id = g_boot_devices[j].device_id;
			boot_dev += strlen(g_boot_devices[j].name);
			return boot_dev;
		}
	}

	return NULL;
}
