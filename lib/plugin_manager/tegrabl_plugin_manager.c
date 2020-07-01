/*
 * Copyright (c) 2017-2018, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_PLUGIN_MANAGER

#include <stdint.h>
#include <string.h>
#include <libfdt.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_plugin_manager.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_board_info.h>
#include <tegrabl_soc_misc.h>
#include <tegrabl_utils.h>
#include <tegrabl_malloc.h>
#include <tegrabl_odmdata_lib.h>
#include <tegrabl_fuse.h>

#define CHIP_ID_MAX 5
#define PM_MATCH_PROP_MAX_LEN 20

struct pm_match_info {
	char name[16];
	uint32_t count;
	bool (*is_match)(void *fdt, const char *item, void *param);
};

struct pm_fuse_info {
	char *name;
	fuse_type_t type;
	uint32_t value;
};

#define BIT(n) (1UL << (n))

struct pm_fuse_info fuse_info_array[] = {
#if defined(FUSE_ISP_DISABLE)
	{ "fuse-disable-isp", FUSE_ISP_DISABLE, BIT(0) },
#endif
#if defined(FUSE_NVENC_DISABLE)
	{ "fuse-disable-nvenc", FUSE_NVENC_DISABLE, (BIT(0) | BIT(1)) },
#endif
#if defined(FUSE_PVA_DISABLE)
	{ "fuse-disable-pva", FUSE_PVA_DISABLE, (BIT(0) | BIT(1)) },
#endif
#if defined(FUSE_DLA_DISABLE)
	{ "fuse-disable-dla", FUSE_DLA_DISABLE, (BIT(0) | BIT(1)) },
#endif
#if defined(FUSE_CV_DISABLE)
	{ "fuse-disable-cv", FUSE_CV_DISABLE, BIT(0) },
#endif
};

typedef uint32_t pm_match_type_t;
#define PLUGIN_MANAGER_MATCH_EXACT 0
#define PLUGIN_MANAGER_MATCH_PARTIAL 1
#define PLUGIN_MANAGER_MATCH_GT 2
#define PLUGIN_MANAGER_MATCH_GE 3
#define PLUGIN_MANAGER_MATCH_LT 4
#define PLUGIN_MANAGER_MATCH_LE 5

static char chip_id[CHIP_ID_MAX] = {'\0'};

static bool match_configs(void *, const char *, void *);
static bool match_id(void *, const char *, void *);
static bool match_odm_data(void *, const char *, void *);
static bool match_chip_id(void *, const char *, void *);
static bool match_nct_data(void *, const char *, void *);
static bool match_fuse_info(void *, const char *, void *);

struct pm_match_info match_info_array[] = {
	{
		.name = "config-names",
		.is_match = match_configs,
	},
	{
		.name = "configs",
		.is_match = NULL,
	},
	{
		.name = "ids",
		.is_match = match_id,
	},
	{
		.name = "odm-data",
		.is_match = match_odm_data,
	},
	{
		.name = "nct-data",
		.is_match = match_nct_data,
	},
	{
		.name = "chip-id",
		.is_match = match_chip_id,
	},
	{
		.name = "fuse-info",
		.is_match = match_fuse_info,
	},
};

static inline int pm_get_fabid(char *board_id)
{
	int fabid = 0;
	int i, id;

	if (strlen(board_id) < 13) {
		return -1;
	}

	for (i = 0; i < 3; i++) {
		id = board_id[i + 10];
		if (id >= '0' && id <= '9') {
			id = id - '0';
		} else if (id >= 'a' && id <= 'z') {
			id = id - 'a' + 10;
		} else if (id >= 'A' && id <= 'Z') {
			id = id - 'A' + 10;
		} else {
			return -1;
		}
		fabid = fabid * 100 + id;
	}

	return fabid;
}

static bool match_id(void *fdt, const char *id, void *param)
{
	int id_len = strlen(id);
	char *id_str = (char *)id;
	pm_match_type_t match_type = PLUGIN_MANAGER_MATCH_EXACT;
	int fabid, board_fabid, i;
	tegrabl_error_t err;
	int boardid_node, prop_node, boardid_len;
	char *board_id;
	bool matched = false;

	TEGRABL_UNUSED(param);

	board_fabid = 0;
	fabid = 0;

	if ((id_len > 2) && (id_str[0] == '>') && (id_str[1] == '=')) {
		id_str += 2;
		id_len -= 2;
		match_type = PLUGIN_MANAGER_MATCH_GE;
		goto match_type_done;
	}

	if ((id_len > 1) && (id_str[0] == '>')) {
		id_str += 1;
		id_len -= 1;
		match_type = PLUGIN_MANAGER_MATCH_GT;
		goto match_type_done;
	}

	if ((id_len > 2) && (id_str[0] == '<') && (id_str[1] == '=')) {
		id_str += 2;
		id_len -= 2;
		match_type = PLUGIN_MANAGER_MATCH_LE;
		goto match_type_done;
	}

	if ((id_len > 1) && (id_str[0] == '<')) {
		id_str += 1;
		id_len -= 1;
		match_type = PLUGIN_MANAGER_MATCH_LT;
		goto match_type_done;
	}

	if ((id_len > 1) && (id_str[0] == '^')) {
		id_str += 1;
		id_len -= 1;
		match_type = PLUGIN_MANAGER_MATCH_PARTIAL;
		goto match_type_done;
	}

	for (i = 0; i < id_len; i++) {
		if (id_str[i] == '*') {
			id_len = i;
			match_type = PLUGIN_MANAGER_MATCH_PARTIAL;
			break;
		}
	}

match_type_done:
	if ((match_type == PLUGIN_MANAGER_MATCH_GE) || (match_type == PLUGIN_MANAGER_MATCH_GT)
		|| (match_type == PLUGIN_MANAGER_MATCH_LE) || (match_type == PLUGIN_MANAGER_MATCH_LT)) {
		fabid = pm_get_fabid(id_str);
		if (fabid < 0) {
			goto finish;
		}
	}

	/* TODO: evaluate if can retrieve board_id directly instead of
	 * from /chosen/plugin-manager/id
	 */
	err = tegrabl_dt_get_node_with_path(fdt, "/chosen/plugin-manager/ids", &boardid_node);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to find node plugin-manager/ids\n");
		goto finish;
	}

	tegrabl_dt_for_each_prop_of(fdt, prop_node, boardid_node) {
		fdt_getprop_by_offset(fdt, prop_node, (const char **)&board_id, NULL);
		if (!board_id) {
			pr_error("Failed to read prop on /chosen/plugin-manager/ids\n");
			goto finish;
		}

		boardid_len = strlen(board_id);
		board_fabid = pm_get_fabid(board_id);
		pr_debug("%s: check if %s(fab:%x) match with plugin %s(fab:%x)\n",
				 __func__, id, fabid, board_id, board_fabid);

		switch (match_type) {
		case PLUGIN_MANAGER_MATCH_EXACT:
			if (boardid_len != id_len) {
				break;
			}
			if (!memcmp(id_str, board_id, id_len)) {
				matched = true;
			}
			break;

		case PLUGIN_MANAGER_MATCH_PARTIAL:
			if (boardid_len < id_len) {
				break;
			}
			if (!memcmp(id_str, board_id, id_len)) {
				matched = true;
			}
			break;

		case PLUGIN_MANAGER_MATCH_GT:
		case PLUGIN_MANAGER_MATCH_GE:
		case PLUGIN_MANAGER_MATCH_LT:
		case PLUGIN_MANAGER_MATCH_LE:
			if (boardid_len < 13) {
				break;
			}
			if (memcmp(id_str, board_id, 10)) {
				break;
			}
			if (board_fabid < 0) {
				break;
			}
			if ((board_fabid > fabid) &&
				(match_type == PLUGIN_MANAGER_MATCH_GT)) {
				matched = true;
			} else if ((board_fabid >= fabid) &&
				(match_type == PLUGIN_MANAGER_MATCH_GE)) {
				matched = true;
			} else if ((board_fabid < fabid) &&
				(match_type == PLUGIN_MANAGER_MATCH_LT)) {
				matched = true;
			} else if ((board_fabid <= fabid) &&
				(match_type == PLUGIN_MANAGER_MATCH_LE)) {
				matched = true;
			}
			break;
		}
	}

finish:
	pr_debug("%s: result: %d\n", __func__, matched);
	return matched;
}

static bool match_configs(void *fdt, const char *config_name, void *param)
{
	uint32_t pm_val = *(uint32_t *)param;
	uint32_t mask, value, board_val;
	tegrabl_error_t err;
	int config_node;
	bool matched = false;

	/* TODO: evaluate if can retrive configs directly instead of from
	 * /chosen/plugin-manager/configs
	 */

	err = tegrabl_dt_get_node_with_path(fdt, "/chosen/plugin-manager/configs",
										&config_node);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Failed to access plugin-manager/configs\n");
		return false;
	}

	err = tegrabl_dt_get_prop_u32(fdt, config_node, (char *)config_name,
								  &board_val);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("Failed to read plugin-manager/configs/%s\n", config_name);
		return false;
	}

	mask = (pm_val >> 8) & 0xFF;
	value = pm_val & 0xff;

	board_val = (board_val & 0xFF) & mask;
	if (board_val == value) {
		matched = true;
	}

	pr_debug("%s: result: %d\n", __func__, (int)matched);
	return matched;
}

static bool match_odm_data(void *fdt, const char *odm_data, void *param)
{
	bool matched = false;

	TEGRABL_UNUSED(fdt);
	TEGRABL_UNUSED(param);

	if (odm_data) {
		matched = tegrabl_odmdata_get_config_by_name((char *)odm_data);
	}

	pr_debug("%s: result: %d\n", __func__, (int)matched);
	return matched;
}

static bool match_chip_id(void *fdt, const char *id, void *param)
{
	struct tegrabl_chip_info info;
	bool matched = false;

	TEGRABL_UNUSED(fdt);
	TEGRABL_UNUSED(param);

	if (!strlen(chip_id)) {
		tegrabl_get_chip_info(&info);

		tegrabl_snprintf(chip_id, CHIP_ID_MAX, "%c%02u%c",
						 info.major + 64, info.minor,
						 (info.revision == 0) ? 0 : (info.revision + 'O'));
	}

	if (!strncmp(chip_id, id, strlen(chip_id))) {
		matched = true;
	}

	pr_debug("%s: result: %d\n", __func__, (int)matched);
	return matched;
}

static bool match_nct_data(void *fdt, const char *nct_data, void *param)
{
	bool matched = false;

	TEGRABL_UNUSED(fdt);
	TEGRABL_UNUSED(nct_data);
	TEGRABL_UNUSED(param);

	/* add nct info match check if needed */

	pr_debug("%s: result: %d\n", __func__, (int)matched);
	return matched;
}

static bool match_fuse_info(void *fdt, const char *fuse_info, void *param)
{
	bool matched = false;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t value, i;

	TEGRABL_UNUSED(fdt);
	TEGRABL_UNUSED(param);

	if (fuse_info) {
		for (i = 0; i != ARRAY_SIZE(fuse_info_array); i++) {
			if (!strncmp(fuse_info, fuse_info_array[i].name, strlen(fuse_info))) {
				err = tegrabl_fuse_read(fuse_info_array[i].type, (uint32_t *)&value, sizeof(value));
				if (err == TEGRABL_NO_ERROR && (value & fuse_info_array[i].value)) {
					matched = true;
				}
				break;
			}
		}
	}

	pr_debug("%s: result: %d\n", __func__, (int)matched);
	return matched;
}

static tegrabl_error_t pm_do_prop_overlay(void *fdt, int32_t target_nd,
										  void *fdt_buf, int32_t overlay_nd)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	char *prop_name;
	void *prop_data;
	int32_t prop_nd, prop_size;
	char target_path[128] = {'\0'};
	int fdt_err;

	pr_debug("Overriding prop %s to target %s\n",
			 (char *)fdt_get_name(fdt_buf, overlay_nd, NULL),
			 (char *)fdt_get_name(fdt, target_nd, NULL));

	tegrabl_dt_for_each_prop_of(fdt_buf, prop_nd, overlay_nd) {

		prop_data = (char *)fdt_getprop_by_offset(fdt_buf, prop_nd,
								(const char **)&prop_name, &prop_size);

		if (!strcmp(prop_name, "name") || !strcmp(prop_name, "phandle")
			|| !strcmp(prop_name, "linux,phandle")) {
			continue;
		}

		if (!strcmp(prop_name, "delete-target-property")) {
			pr_info("Removing prop %s from %s\n", (char *)prop_data,
					(char *)fdt_get_name(fdt, target_nd, NULL));

			fdt_err = fdt_delprop(fdt, target_nd, prop_data);
			if (fdt_err < 0) {
				if (!strlen(target_path)) {
					fdt_get_path(fdt, target_nd, target_path, 128);
				}
				pr_error("Failed to delete prop %s from %s\n",
						 (char *)prop_data, target_path);
				err = TEGRABL_ERROR(TEGRABL_ERR_DEL_FAILED, 0);
			}
			goto finish;
		}

		if (!strcmp(prop_name, "append-string-property")) {
			fdt_err = fdt_appendprop(fdt, target_nd, prop_data, NULL, 0);
			if (fdt_err < 0) {
				if (!strlen(target_path)) {
					fdt_get_path(fdt, target_nd, target_path, 128);
				}
				pr_error("Failed to append prop %s on %s\n",
						 (char *)prop_data, target_path);
				err = TEGRABL_ERROR(TEGRABL_ERR_ADD_FAILED, 0);
			}
			goto finish;
		}

		fdt_err = fdt_setprop(fdt, target_nd, prop_name, prop_data,
							  prop_size);
		if (fdt_err) {
			if (!strlen(target_path)) {
				fdt_get_path(fdt, target_nd, target_path, 128);
			}
			pr_error("Failed to update prop %s on %s\n", prop_name,
					 target_path);
			err = TEGRABL_ERROR(TEGRABL_ERR_SET_FAILED, 0);
		}

finish:
		if (err != TEGRABL_NO_ERROR) {
			break;
		}
	}

	return err;
}

static tegrabl_error_t pm_overlay_handle(void *fdt, int32_t target_nd,
										 void *fdt_buf, int32_t overlay_nd)
{
	tegrabl_error_t err;
	int child_nd, tchild_nd;
	char *child_name;

	err = pm_do_prop_overlay(fdt, target_nd, fdt_buf, overlay_nd);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to overlay property\n");
		return err;
	}

	tegrabl_dt_for_each_child(fdt_buf, overlay_nd, child_nd) {
		child_name = (char *)fdt_get_name(fdt_buf, child_nd, NULL);
		err = tegrabl_dt_get_child_with_name(fdt, target_nd, child_name,
											 &tchild_nd);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Failed to find %s in target node %s\n", child_name,
					 fdt_get_name(fdt, target_nd, NULL));
			continue;
		}

		err = pm_overlay_handle(fdt, tchild_nd, fdt_buf, child_nd);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Failed to overlay child node\n");
			return err;
		}
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t pm_override_fragment(void *fdt, void *fdt_buf,
											int32_t override_nd)
{
	int target_nd, overlay_nd;
	uint32_t target_phd;
	tegrabl_error_t err;
	const char *fr_name;
	char target_path[128];

	err = tegrabl_dt_get_prop_u32(fdt_buf, override_nd, "target", &target_phd);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get target handle\n");
		return err;
	}

	target_nd = fdt_node_offset_by_phandle(fdt, target_phd);
	if (target_nd < 0) {
		pr_error("Failed to find phandle for %s\n",
				 fdt_get_name(fdt_buf, override_nd, NULL));
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	err = tegrabl_dt_get_node_with_name(fdt_buf, override_nd, "_overlay_",
										&overlay_nd);
	if (err != TEGRABL_NO_ERROR) {
		fr_name = fdt_get_name(fdt_buf, fdt_parent_offset(fdt_buf, override_nd),
							   NULL);
		pr_error("Failed to access /plugin-manager/%s/%s/_overlay_\n",
				 fr_name, fdt_get_name(fdt_buf, override_nd, NULL));
		return err;
	}

	err = pm_overlay_handle(fdt, target_nd, fdt_buf, overlay_nd);
	if (err != TEGRABL_NO_ERROR) {
		fdt_get_path(fdt, target_nd, target_path, 128);
		fr_name = fdt_get_name(fdt_buf, fdt_parent_offset(fdt_buf, override_nd),
							   NULL);
		pr_error("Failed to update %s from /plugin-manager/%s/%s/_overlay_/\n",
				 target_path, fr_name,
				 fdt_get_name(fdt_buf, override_nd, NULL));
		return err;
	}

	return err;
}

static tegrabl_error_t pm_get_prop_count(void *fdt, int32_t node)
{
	struct pm_match_info *match_iter = match_info_array;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t all_count = 0;
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(match_info_array); match_iter++, i++) {
		match_iter->count = 0;
		err = tegrabl_dt_get_prop_count_strings(fdt, node, match_iter->name,
												&match_iter->count);
		if (err != TEGRABL_NO_ERROR) {
			if (err == TEGRABL_ERR_NOT_FOUND) {
				continue;
			}
			return err;
		}

		pr_debug("%s: [%s] count:%d\n", __func__, match_iter->name,
				 match_iter->count);

		all_count += match_iter->count;
	}

	if (!all_count) {
		pr_error("Find no ids, nct, odm data\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t pm_fragment_handle(void *fdt, void *fdt_buf,
										  int32_t fr_nd)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool override_on_all_match = false;
	bool odm_anded_override = false;
	bool found = false;
	struct pm_match_info *match_iter = match_info_array;
	int32_t override_nd;
	uint32_t i, j;
	const char *fr_name;

	fr_name = fdt_get_name(fdt_buf, fr_nd, NULL);
	if (!fr_name) {
		pr_error("Failed to get fragment name at node(%d)\n", fr_nd);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (fdt_get_property(fdt_buf, fr_nd, "enable-override-on-all-matches",
						 NULL)) {
		pr_debug("enable-override-on-all-matches is enabled\n");
		override_on_all_match = true;
	}

	if (fdt_get_property(fdt_buf, fr_nd, "odm-anded-override",
						 NULL)) {
		pr_debug("odm-anded-override is enabled\n");
		odm_anded_override = true;
	}

	err = pm_get_prop_count(fdt_buf, fr_nd);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to count prop on /plugin-manager/%s\n", fr_name);
		return err;
	}

	if (!tegrabl_dt_get_child_count(fdt_buf, fr_nd)) {
		pr_error("Failed to count overlay on /plugin-manager/%s\n", fr_name);
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
	}

	for (i = 0; i < ARRAY_SIZE(match_info_array); i++, match_iter++) {

		if (match_iter->count > 0 && match_iter->is_match) {

			uint32_t data = 0;
			const char *prop_string[match_iter->count];

			err = tegrabl_dt_get_prop_string_array(fdt_buf, fr_nd,
												   match_iter->name,
												   &prop_string[0], NULL);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("Failed to get prop on /plugin-manager/%s/%s\n",
						 fr_name, match_iter->name);
				return err;
			}

			for (j = 0, found = false; j < match_iter->count; j++) {
				pr_debug("check if prop %s[%s] on /plugin-manager/%s match\n",
						 match_iter->name, prop_string[j], fr_name);

				if (!strcmp(match_iter->name, "config-names")) {
					err = tegrabl_dt_get_prop_u32_by_idx(fdt_buf, fr_nd,
														 "configs", j, &data);
					if (err != TEGRABL_NO_ERROR) {
						break;
					}
				}

				found = match_iter->is_match(fdt_buf, prop_string[j], &data);
				if (odm_anded_override && (0 == strcmp(match_iter->name, "odm-data"))) {
					if (!found) {
						break;
					}
				} else {
					if (found) {
						break;
					}
				}
			}

			if (override_on_all_match && !found) {
				return TEGRABL_NO_ERROR;
			} else if (!override_on_all_match && found) {
				goto apply_override;
			}
		}
	}

	if (!found) {
		return TEGRABL_NO_ERROR;
	}

apply_override:
	pr_info("node /plugin-manager/%s matches\n", fr_name);
	tegrabl_dt_for_each_child(fdt_buf, fr_nd, override_nd) {
		err = pm_override_fragment(fdt, fdt_buf, override_nd);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("failed to override fragment: %x\n", fr_nd);
		}
	}

	return err;
}

tegrabl_error_t tegrabl_plugin_manager_overlay(void *fdt)
{
	int32_t pm_node, fr_nd;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int fdt_size, fdt_err;
	void *fdt_buf = NULL;
	char *status;
	bool available;

	TEGRABL_ASSERT(fdt);

	pr_info("Plugin-manager override starting\n");

	/* TODO: add connection manager function here */

	pm_node = fdt_path_offset(fdt, "/plugin-manager");
	if (pm_node <= 0) {
		pr_warn("Failed to find /plugin-manager in DT\n");
		return TEGRABL_NO_ERROR;
	}

	err = tegrabl_dt_get_prop_string(fdt, pm_node, "status",
									 (const char **)&status);
	if (err != TEGRABL_NO_ERROR && err != TEGRABL_ERR_NOT_FOUND)  {
		pr_error("Failed to get status on /plugin-manager\n");
		return TEGRABL_NO_ERROR;
	}

	if (err == TEGRABL_NO_ERROR) {
		if (strcmp(status, "okay") && strcmp(status, "ok")) {
			pr_warn("/plugin-manager is disabled\n");
			return TEGRABL_NO_ERROR;
		}
	}

	/* duplicate DTB as temp source for plugin-manager override, original
	 * DTB is used as target to be overriden to
	 */
	fdt_size = fdt_totalsize(fdt);

	fdt_buf = (void *)tegrabl_malloc(fdt_size);
	if (!fdt_buf) {
		pr_error("Not enough memory\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
	}

	fdt_err = fdt_open_into(fdt, fdt_buf, fdt_size);
	if (fdt_err) {
		pr_error("Failed to duplicate DTB\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	tegrabl_dt_for_each_child(fdt_buf, pm_node, fr_nd) {
		err = tegrabl_dt_is_device_available(fdt_buf, fr_nd, &available);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Failed to get fragment status\n");
			break;
		}

		if (!available) {
			continue;
		}

		err = pm_fragment_handle(fdt, fdt_buf, fr_nd);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("Failed to handle /plugin-manager/%s Error(%d)\n",
					 fdt_get_name(fdt_buf, fr_nd, NULL), err);
			break;
		}
	}

	/* Disable plugin-manager status for kernel */
	pm_node = fdt_path_offset(fdt, "/plugin-manager");
	if (pm_node <= 0) {
		pr_warn("Failed to find /plugin-manager in DT\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}

	fdt_err = fdt_setprop_string(fdt, pm_node, "status", "disabled");
	if (fdt_err < 0) {
		pr_error("Failed to disable plugin-manager status.\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_SET_FAILED, 0);
		goto fail;
	}
	pr_info("Disable plugin-manager status in FDT\n");

	pr_info("Plugin-manager override finished %s\n",
			(err == TEGRABL_NO_ERROR) ? "successfully" : "with Error");

fail:
	if (fdt_buf) {
		tegrabl_free(fdt_buf);
	}

	return err;
}

