/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_DEVICETREE

#include <tegrabl_devicetree.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_malloc.h>
#include <libfdt.h>
#include <tegrabl_sdram_usage.h>
#include <string.h>

static void *fdt_handle_table[TEGRABL_DT_COUNT] = {
	/* Bootloader dtb handle */
	NULL,
	/* Kernel dtb handle */
	NULL,
};

tegrabl_error_t tegrabl_dt_read_reg_by_index(const void *fdt, int nodeoffset,
											uint32_t index, uintptr_t *addr,
											uintptr_t *size)
{
	int lenp, addr_cells, size_cells, offset;
	uint32_t count = 0U;
	const uint32_t *cells_p, *prop_p;

	/* Get the pointer to the data in "reg" property */
	prop_p = fdt_getprop(fdt, nodeoffset, "reg", &lenp);
	if (prop_p == NULL) {
		return 0;
	}

	pr_debug("prop_p is %p\n", prop_p);

	/* Find the applicable #address-cell for this node.
	 * Please note that lookup starts from this nodes parent
	 * and continues till the / node is reached */
	offset = nodeoffset;
	addr_cells = 0;
	while (1) {
		offset = fdt_parent_offset(fdt, offset);
		cells_p = fdt_getprop(fdt, offset, "#address-cells", NULL);
		pr_debug("%s: offset: %d cells_p: %p\n",
				 fdt_get_name(fdt, offset, NULL),
				 offset, cells_p);
		if (cells_p != NULL) {
			addr_cells = fdt32_to_cpu(*((uint32_t *)cells_p));
			break;
		} else if (offset <= 0) {
			break;
		}
	}
	pr_debug("addr-cells: %d\n", addr_cells);

	/* Find the applicable #size-cell for this node.
	 * Please note that lookup starts from this nodes parent
	 * and continues till the / node is reached */
	offset = nodeoffset;
	size_cells = 0;
	while (1) {
		offset = fdt_parent_offset(fdt, offset);
		cells_p = fdt_getprop(fdt, offset, "#size-cells", NULL);
		pr_debug("%s: offset: %d cells_p: %p\n",
				 fdt_get_name(fdt, offset, NULL),
				 offset, cells_p);
		if (cells_p != NULL) {
			size_cells = fdt32_to_cpu(*((uint32_t *)cells_p));
			break;
		} else if (offset <= 0) {
			break;
		}
	}
	pr_debug("size-cells: %d\n", size_cells);

	/* The elements in reg property are stored in form of
	 * <addr0 size0 addr1 size1 ... addrn sizen> format
	 * #address/size-cell values are multiples of 4B and specify
	 * how many 4B words each of these fields occupy */
	if ((addr_cells + size_cells) != 0U) {
		count = lenp / ((addr_cells + size_cells) * sizeof(uint32_t));
	}
	pr_debug("lenp: %d, count: %d\n", lenp, count);
	if (index > count) {
		return TEGRABL_ERR_NOT_FOUND;
	}

	/* Move to the element we are interested in */
	prop_p += (index * (addr_cells + size_cells));

	if (addr && addr_cells) {
		if (addr_cells == 1) {
			*addr = fdt32_to_cpu(*(prop_p++));
		} else if (addr_cells == 2) {
			*addr = (uint64_t)fdt32_to_cpu(*(prop_p++)) << 32;
			*addr |= (uint64_t)fdt32_to_cpu(*(prop_p++));
		} else {
			pr_error("unsupported #address-cells value\n");
		}
		pr_debug("addr: 0x%lx\n", *addr);
	}

	if (size && size_cells) {
		if (size_cells == 1) {
			*size = fdt32_to_cpu(*(prop_p++));
		} else if (size_cells == 2) {
			*size = (uint64_t)fdt32_to_cpu(*(prop_p++)) << 32;
			*size |= (uint64_t)fdt32_to_cpu(*(prop_p++));
		} else {
			pr_error("unsupported #size-cells value\n");
		}
		pr_debug("size: 0x%lx\n", *size);
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_get_gic_intr(const void *fdt, int nodeoffset,
										uint32_t *intr, uint32_t count)
{
	int lenp, intr_cells = 3;
	uint32_t i;
	int gic_node;
	const void *prop_p, *cell_p;
	prop_p = fdt_getprop(fdt, nodeoffset, "interrupts", &lenp);

	gic_node = fdt_node_offset_by_compatible(fdt, -1 , "arm,cortex-a15-gic");
	if (gic_node < 0) {
		cell_p = fdt_getprop(fdt, gic_node, "#interrupt-cells", NULL);
		if (cell_p) {
			intr_cells = fdt32_to_cpu(*((uint32_t *)cell_p));
		}
	}
	pr_debug("intr_cells: %d\n", intr_cells);

	if (prop_p != NULL) {
		if ((lenp/intr_cells) < (int)count) {
			return TEGRABL_ERR_NOT_FOUND;
		}
		for (i = 0; i < count; i++) {
			intr[i] = fdt32_to_cpu(*((uint32_t *)prop_p + (3 * i) + 1)) + 32;
		}
	}
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_set_fdt_handle(tegrabl_dt_type_t type,
										  void *fdt)
{
	if (type >= TEGRABL_DT_COUNT) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}

	fdt_handle_table[type] = fdt;
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_get_fdt_handle(tegrabl_dt_type_t type, void **fdt)
{
	if (type >= TEGRABL_DT_COUNT) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
	}
	if (fdt_handle_table[type] == NULL) {
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
	}

	*fdt = fdt_handle_table[type];
	return TEGRABL_NO_ERROR;
}

uint32_t tegrabl_dt_get_child_count(const void *fdt, int node_offset)
{
	int sub_offset, children = 0;

	if (!fdt) {
		return 0;
	}

	sub_offset = fdt_first_subnode(fdt, node_offset);
	if (sub_offset == -FDT_ERR_NOTFOUND) {
		return children;
	}

	while (sub_offset != -FDT_ERR_NOTFOUND) {
		children++;
		sub_offset = fdt_next_subnode(fdt, sub_offset);
	}
	return children;
}

tegrabl_error_t tegrabl_dt_get_child_with_name(const void *fdt,
											int start_offset, char *name,
											int *res)
{
	int sub_offset, len;
	const char *sub_name;

	if (!fdt || !name || !res) {
		return TEGRABL_ERR_INVALID;
	}

	tegrabl_dt_for_each_child(fdt, start_offset, sub_offset) {
		sub_name = fdt_get_name(fdt, sub_offset, &len);
		if (!sub_name || !len) {
			continue;
		}
		if (!strncmp(name, sub_name, len)) {
			*res = sub_offset;
			return TEGRABL_NO_ERROR;
		}
	}

	return TEGRABL_ERR_NOT_FOUND;
}

tegrabl_error_t tegrabl_dt_get_next_child(const void *fdt, int node_offset,
											int prev_child, int *res)
{
	int next_offset;

	if (!fdt || !res) {
		return TEGRABL_ERR_INVALID;
	}

	if (prev_child == 0)
		next_offset = fdt_first_subnode(fdt, node_offset);
	else
		next_offset = fdt_next_subnode(fdt, prev_child);

	if (next_offset < 0) {
		return TEGRABL_ERR_NOT_FOUND;
	}

	*res = next_offset;
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_get_prop_by_idx(const void *fdt, int node_offset,
										char *prop_name, size_t sz,
										uint32_t idx, void *res)
{
	const void *prop;

	if (!fdt || !prop_name || !res) {
		return TEGRABL_ERR_INVALID;
	}

	if (sz != 1 && sz != 4) {
		return TEGRABL_ERR_INVALID;
	}

	prop = fdt_getprop(fdt, node_offset, prop_name, NULL);

	if (!prop) {
		return TEGRABL_ERR_NOT_FOUND;
	}

	switch (sz) {
	case 1:
		*(uint8_t *)res = *((uint8_t *)prop + idx);
		break;

	case 4:
		*(uint32_t *)res = fdt32_to_cpu(*((uint32_t *)prop + idx));
		break;
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_get_prop_array(const void *fdt, int node_offset,
										char *prop_name, size_t sz,
										uint32_t nmemb, void *res,
										uint32_t *num)
{
	int lenp;
	const void *prop;
	uint32_t target_num, found_num, i;

	if (!fdt || !prop_name || !res || (!nmemb && !num)) {
		return TEGRABL_ERR_INVALID;
	}

	if (sz != 1 && sz != 4) {
		return TEGRABL_ERR_INVALID;
	}

	prop = fdt_getprop(fdt, node_offset, prop_name, &lenp);
	if (!prop) {
		if (num) {
			*num = 0;
		}
		return TEGRABL_ERR_NOT_FOUND;
	}

	found_num = lenp / sz;
	target_num = (nmemb == 0) ? found_num : MIN(nmemb, found_num);

	/* Memory pointed by res is assumed to be allocated */
	switch (sz) {
	case 1:
		for (i = 0; i < target_num; i++) {
			*((uint8_t *)res + i) = *((uint8_t *)prop + i);
		}
		if (num) {
			*num = i;
		}
		break;

	case 4:
		for (i = 0; i < target_num; i++) {
			*((uint32_t *)res + i) = fdt32_to_cpu(*((uint32_t *)prop + i));
		}
		if (num) {
			*num = i;
		}
		break;
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_get_prop_string(const void *fdt, int node_offset,
										char *prop_name, const char **res)
{
	if (!fdt || !prop_name || !res) {
		return TEGRABL_ERR_INVALID;
	}

	*res = fdt_getprop(fdt, node_offset, prop_name, NULL);

	if (!*res) {
		return TEGRABL_ERR_NOT_FOUND;
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_get_prop_string_array(const void *fdt,
											int node_offset, char *prop_name,
											const char **res, uint32_t *num)
{
	const char *prop, *iter;
	int len;
	uint32_t i = 0;

	if (!fdt || !prop_name) {
		return TEGRABL_ERR_INVALID;
	}

	prop = fdt_getprop(fdt, node_offset, prop_name, &len);
	if (!prop) {
		return TEGRABL_ERR_NOT_FOUND;
	}

	if (res) {
		res[i] = prop;
	}

	for (iter = prop; iter < prop + len; iter++) {
		if (*iter == '\0') {
			i++;
			if (num) {
				*num = i;
			}
			if (res && (iter != prop + len - 1)) {
				res[i] = iter + 1;
			}
		}
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_count_elems_of_size(const void *fdt, int node_offset,
						char *prop_name, uint32_t sz, uint32_t *num)
{
	int len;
	const void *prop;

	if (!fdt || !prop_name || !num) {
		return TEGRABL_ERR_INVALID;
	}

	prop = fdt_getprop(fdt, node_offset, prop_name, &len);
	if (!prop) {
		return TEGRABL_ERR_NOT_FOUND;
	}

	*num = len / sz;
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_get_node_with_name(const void *fdt, int start_offset,
											char *name, int *res)
{
	int iter, len;
	const char *iter_name;

	if (!fdt || !name || !res) {
		return TEGRABL_ERR_INVALID;
	}

	tegrabl_dt_for_each_node_from(fdt, iter, start_offset) {
		iter_name = fdt_get_name(fdt, iter, &len);
		if (!iter_name || !len) {
			continue;
		}
		if (!strncmp(name, iter_name, len)) {
			*res = iter;
			return TEGRABL_NO_ERROR;
		}
	}

	*res = 0;
	return TEGRABL_ERR_NOT_FOUND;
}

tegrabl_error_t tegrabl_dt_get_node_with_compatible(const void *fdt,
												int start_offset, char *comp,
												int *res)
{
	int offset;

	if (!fdt || !comp || !res) {
		return TEGRABL_ERR_INVALID;
	}

	offset = fdt_node_offset_by_compatible(fdt, start_offset, comp);
	if (offset < 0) {
		*res = 0;
		return TEGRABL_ERR_NOT_FOUND;
	}

	*res = offset;
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_get_node_with_path(const void *fdt, const char *path,
											int *res)
{
	int node_offset;

	if (!fdt || !path || !res) {
		return TEGRABL_ERR_INVALID;
	}

	node_offset = fdt_path_offset(fdt, path);
	if (node_offset < 0) {
		pr_error("Error %d when finding node with path %s\n", node_offset,
					path);
		*res = 0;
		return TEGRABL_ERR_NOT_FOUND;
	}

	*res = node_offset;
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_is_device_available(const void *fdt, int node_offset,
											bool *res)
{
	const char *prop;

	if (!fdt || !res) {
		return TEGRABL_ERR_INVALID;
	}

	prop = fdt_getprop(fdt, node_offset, "status", NULL);

	if (!prop || !strncmp(prop, "okay", 4) || !strncmp(prop, "ok", 2))
		*res = true;
	else
		*res = false;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_dt_get_next_available(const void *fdt, int start_node,
											int *res)
{
	int iter;
	bool is_available;

	if (!fdt || !res) {
		return TEGRABL_ERR_INVALID;
	}

	tegrabl_dt_for_each_node_from(fdt, iter, start_node) {
		tegrabl_dt_is_device_available(fdt, iter, &is_available);
		if (is_available) {
			*res = iter;
			return TEGRABL_NO_ERROR;
		}
	}

	*res = 0;
	return TEGRABL_ERR_NOT_FOUND;
}

tegrabl_error_t tegrabl_dt_is_device_compatible(const void *fdt,
											int node_offset, const char *comp,
											bool *res)
{
	uint32_t num_comp, i;
	const char **compatibles;
	tegrabl_error_t ret;

	if (!fdt || !comp || !res) {
		return TEGRABL_ERR_INVALID;
	}

	/* First get the number of compatible strings in the property */
	ret = tegrabl_dt_get_prop_string_array(fdt, node_offset, "compatible", NULL,
																&num_comp);
	if (ret != TEGRABL_NO_ERROR) {
		return ret;
	}

	compatibles = tegrabl_malloc(num_comp * sizeof(const char *));
	if (!compatibles) {
		return TEGRABL_ERR_NO_MEMORY;
	}

	ret = tegrabl_dt_get_prop_string_array(fdt, node_offset, "compatible",
										compatibles, NULL);

	for (i = 0; i < num_comp; i++)
		if (!strcmp(compatibles[i], comp)) {
			*res = true;
			goto done;
		}

	*res = false;

done:
	tegrabl_free(compatibles);
	return TEGRABL_NO_ERROR;
}

int tegrabl_add_subnode_if_absent(void *fdt, int parentnode, char *nodename)
{
	int node = -1;

	TEGRABL_ASSERT(fdt);

	node = fdt_subnode_offset(fdt, parentnode, nodename);
	if (node < 0) {
		pr_error("\"%s\" doesn't exist, creating\n", nodename);
		node = fdt_add_subnode(fdt, parentnode, nodename);
		if (node < 0) {
			pr_error("Creating node \"%s\" failed\n", nodename);
		}
	}

	return node;
}

tegrabl_error_t tegrabl_get_alias_by_name(const void *fdt_ptr, char *name,
			char *namep, int *lenp)
{
	int aliasoffset, offset;
	const char *node_value;
	const char *temp;

	if (!fdt_ptr || !namep || !name || !lenp) {
		return TEGRABL_ERR_INVALID;
	}

	aliasoffset = fdt_path_offset(fdt_ptr, "/aliases");
	tegrabl_dt_for_each_prop_of(fdt_ptr, offset, aliasoffset) {
		 node_value = fdt_getprop_by_offset(fdt_ptr, offset, &temp, lenp);
		 if (node_value != NULL) {
			if (!strncmp(node_value, name, strlen(node_value))) {
				strcpy(namep, temp);
				pr_info("Find %s's alias %s\n", name, namep);
				return TEGRABL_NO_ERROR;
			}
		}
	}
	return TEGRABL_ERR_NOT_FOUND;
}

tegrabl_error_t tegrabl_get_alias_id(char *prefix, char *alias_name,
		int *alias_id)
{
	char *index;
	int id;
	char *id_char;

	if (!prefix || !alias_name || !alias_id) {
		return TEGRABL_ERR_INVALID;
	}

	index = strstr(alias_name, prefix);
	if (index) {
		id_char = alias_name + strlen(prefix);
		id = id_char[0] - '0';
		*alias_id = id;
		return TEGRABL_NO_ERROR;
	}
	return TEGRABL_ERR_NOT_FOUND;
}

tegrabl_error_t tegrabl_dt_create_space(void *fdt, uint32_t inc_size, uint32_t max_size)
{
	uint32_t newlen;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int retval;

	pr_trace("%s(): %u\n", __func__, __LINE__);

	newlen = fdt_totalsize(fdt) + inc_size;
	pr_trace("dtb     size: 0x%08x\n", fdt_totalsize(fdt));
	pr_trace("dtb new size: 0x%08x\n", newlen);

	/* avoid overflow */
	if (newlen > max_size) {
		newlen = max_size;
	}
	pr_trace("dtb new size: 0x%08x\n", newlen);

	retval = fdt_open_into(fdt, fdt, newlen);
	if (retval < 0) {
		pr_error("fdt_open_into fail (%s)\n", fdt_strerror(retval));
		err = TEGRABL_ERROR(TEGRABL_ERR_EXPAND_FAILED, 0);
	}

	return err;
}
