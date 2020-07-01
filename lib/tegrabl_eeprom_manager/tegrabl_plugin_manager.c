/*
 * Copyright (c) 2015-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_EEPROM_MANAGER

#include <stdint.h>
#include <string.h>
#include <libfdt.h>
#include <tegrabl_error.h>
#include <tegrabl_debug.h>
#include <tegrabl_plugin_manager.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_board_info.h>
#include <tegrabl_soc_misc.h>

/**
 * @brief Parse the given string and find a config string
 *
 * @param str		string to parse
 * @param config	config string returned
 * @param *len		config string length returned
 * @param max_len	max config string length allowed
 *
 * @return the remainder of the string after a config string is parsed
 */
static uint8_t *parse_config_string(uint8_t *str, uint8_t *config,
									uint32_t *config_len, uint32_t max_len)
{
	*config_len = 0;

	/* Config string format: "<name>:<2-byte-char-value>," */
	while ((*str != '\0') && (*str != ':') && (max_len > 0)) {
		*config++ = *str++;
		(*config_len)++;
		--max_len;
	}

	/* Null terminate the config string*/
	*config = '\0';

	return str;
}

static inline uint32_t get_board_id_no(uint8_t *board_id)
{
	uint32_t i, board_id_no;

	board_id_no = 0;
	for (i = 0; i < BOARD_ID_SZ; ++i) {
		board_id_no = board_id_no * 10 + board_id[i] - '0';
	}

	return board_id_no;
}

#define c_to_i(c, i)	\
{						\
		if ((c >= 'a') && (c <= 'f'))	\
			i = c - 'a' + 0x0a;			\
		else if ((c >= 'A') && (c <= 'F'))	\
				i = c - 'A' + 0x0a;			\
		else								\
			i = c - '0';					\
}

static inline uint32_t get_config_value(uint8_t *config)
{
	uint32_t value = 0;
	uint8_t v;
	int i;

	for (i = 0; i < 2; ++i) {
		c_to_i(config[i], v);
		value = value << 4 | v;
	}

	return value;
}

static tegrabl_error_t create_pm_ids_with_config(void *fdt, int pm_node,
												 struct board_id_info *id_info)
{
	tegrabl_error_t status;
	int eeprom_node = 0;
	uint32_t n_boardid_config_list, boardid_config;
	uint32_t board_id_no;
	uint32_t i, j;
	int err;
	int config_node;

	/* Get nodes offset */
	config_node = tegrabl_add_subnode_if_absent(fdt, pm_node, "configs");
	if (config_node < 0) {
		pr_error("Could not found id and config node\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 0);
	}

	status = tegrabl_dt_get_node_with_path(fdt, "/eeprom-manager",
						&eeprom_node);
	if (status != TEGRABL_NO_ERROR) {
		pr_error("Cannot find eeprom-manager node\n");

		/* No eeprom-manageer node found is not a fatal error */
		status = TEGRABL_NO_ERROR;
		goto done;
	}

	status = tegrabl_dt_count_elems_of_size(fdt, eeprom_node,
					"boardid-with-config", sizeof(uint32_t),
					&n_boardid_config_list);
	if (status != TEGRABL_NO_ERROR) {
		n_boardid_config_list = 0;
		status = TEGRABL_NO_ERROR;
		goto done;
	}


	for (j = 0; j < n_boardid_config_list; ++j) {
		status = tegrabl_dt_get_prop_by_idx(fdt,  eeprom_node,
								"boardid-with-config",  sizeof(uint32_t), j,
								&boardid_config);
		if (status != TEGRABL_NO_ERROR) {
			pr_error("%s: Not able to read board-id config index %d\n",
					 __func__, j);
			goto done;
		}

		/* Search for a matching id in given id list */
		for (i = 0; i < id_info->count; ++i) {
			board_id_no = get_board_id_no(id_info->part[i].part_no);
			if (board_id_no == boardid_config)
				break;
		}

		/* Add all configs for the matching board */
		if (i < id_info->count) {
			uint8_t  prop_name[MAX_BOARD_CONFIG_LEN];
			uint8_t *data = id_info->part[i].config;
			uint32_t count, config_len;
			uint32_t val;

			/* Prefix board id */
			memcpy(prop_name, id_info->part[i].part_no, BOARD_ID_SZ);
			prop_name[BOARD_ID_SZ] = '-';
			count = BOARD_ID_SZ + 1;

			do {
				/* Find and append one config string */
				data = parse_config_string(data, (prop_name + count),
										   &config_len,
										   (MAX_BOARD_CONFIG_LEN - count - 1));

				/* Is a config string found ? */
				if (config_len == 0) {
					break;
				}

				/* Get config value: the byte value after "name:" */
				++data;
				val = get_config_value(data);

				pr_info("Adding plugin-manager/configs/%s %02x\n", prop_name,
						(uint8_t)val);
				err = fdt_setprop_cell(fdt, config_node, (char *)prop_name,
									   val);
				if (err < 0) {
					pr_error("Can't set plugin-manager/config/%s (%s)\n",
							 prop_name, fdt_strerror(err));
					status = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
					goto done;
				}

				/* Prepare for getting next config */
				data += 2;
				if (*data == ',')
					++data;

			} while (*data);
		}
	}

	status = TEGRABL_NO_ERROR;
done:
	return status;
}

static tegrabl_error_t create_pm_ids_with_rev(void *fdt, int pm_node,
											  struct board_id_info *id_info)
{
	tegrabl_error_t status;
	int eeprom_node = 0;
	uint32_t n_boardid_rev_list, boardid_rev;
	uint32_t board_id_no;
	uint32_t i, j;
	int err;
	int id_node;

	id_node = tegrabl_add_subnode_if_absent(fdt, pm_node, "ids");
	if (id_node < 0) {
		pr_error("Could not found id and config node\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 0);
	}

	status = tegrabl_dt_get_node_with_path(fdt, "/eeprom-manager",
						&eeprom_node);
	if (status != TEGRABL_NO_ERROR) {
		pr_error("Cannot find eeprom-manager node\n");

		/* No eeprom-manageer node found is not a fatal error */
		status = TEGRABL_NO_ERROR;
		goto done;
	}

	/* Get the number of rev list */
	status = tegrabl_dt_count_elems_of_size(fdt, eeprom_node,
					"boardid-with-revision", sizeof(uint32_t),
					&n_boardid_rev_list);
	if (status != TEGRABL_NO_ERROR) {
		n_boardid_rev_list = 0;
		status = TEGRABL_NO_ERROR;
		goto done;
	}

	for (j = 0; j < n_boardid_rev_list; ++j) {
		status = tegrabl_dt_get_prop_by_idx(fdt,  eeprom_node,
					"boardid-with-revision",  sizeof(uint32_t), j,
					&boardid_rev);
		if (status != TEGRABL_NO_ERROR) {
			pr_error("%s: Not able to read board-id revision index %d\n",
					__func__, j);
			goto done;
		}

		/* Search for a matching id in given id list */
		for (i = 0; i < id_info->count; ++i) {
			board_id_no = get_board_id_no(id_info->part[i].part_no);
			if (board_id_no == boardid_rev)
				break;
		}

		/* Add id string with rev for the matching board */
		if (i < id_info->count) {
			uint8_t prop_name[BOARD_FULL_REV_SZ];

			memcpy(prop_name, id_info->part[i].part_no, BOARD_FULL_REV_SZ - 1);
			prop_name[BOARD_FULL_REV_SZ - 1] = '\0';

			pr_info("Adding plugin-manager/ids/%s\n", prop_name);
			err = fdt_setprop_cell(fdt, id_node, (char *)prop_name, 1);
			if (err < 0) {
				pr_error("Can't set /chosen/plugin-manager/ids/%s (%s)\n",
						 prop_name, fdt_strerror(err));
				status = TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
				goto done;
			}
		}
	}
	status = TEGRABL_NO_ERROR;
done:
	return status;
}

static tegrabl_error_t create_pm_ids(void *fdt, int pm_node,
									 struct board_id_info *id_info)
{
	int err;
	uint32_t len = BOARD_ID_SZ + 1 + BOARD_SKU_SZ + 1 + BOARD_FAB_SZ + 1;
	int con_node;
	char prop_name[MAX_BOARD_PART_NO_LEN];
	char loc_name[MAX_BOARD_LOCATION_LEN];
	int prev_node, next_node;
	char node_name[MAX_BOARD_LOCATION_LEN];
	int loc_len, j;
	uint32_t i, name_len;
	int id_node;

	/* Get nodes offset */
	id_node = tegrabl_add_subnode_if_absent(fdt, pm_node, "ids");
	if (id_node < 0) {
		pr_error("Could not found id and config node\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 0);
	}

	for (i = 0; i < id_info->count; ++i) {
		memset(prop_name, 0, MAX_BOARD_PART_NO_LEN);
		if (id_info->part[i].customer_part_id) {
			memcpy(prop_name, id_info->part[i].part_no,
			      strlen((char *)id_info->part[i].part_no));
		} else {
			memcpy(prop_name, id_info->part[i].part_no, len - 1);
			prop_name[len - 1] = '\0';
		}

		loc_len = strlen((char *)id_info->part[i].location);
		if (loc_len > 0) {
			memcpy(loc_name, id_info->part[i].location, loc_len);
		} else {
			tegrabl_snprintf(loc_name, MAX_BOARD_LOCATION_LEN, "1");
			loc_len = 1;
		}
		loc_name[loc_len] = '\0';

		pr_info("Adding plugin-manager/ids/%s=%s\n", prop_name, loc_name);

		err = fdt_setprop_string(fdt, id_node, prop_name, loc_name);
		if (err < 0) {
			pr_error("Can't set /chosen/plugin-manager/ids/%s (%s)\n",
					 prop_name, fdt_strerror(err));
			return TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
		}

		con_node = tegrabl_add_subnode_if_absent(fdt, id_node, "connection");
		if (con_node < 0) {
			pr_error("Can't add /chosen/plugin-manager/ids/conneection\n");
			return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 1);
		}

		prev_node = con_node;
		next_node = prev_node;
		name_len = 0;
		for (j = 1; j < loc_len; ++j) {
			node_name[name_len] = loc_name[j];
			if ((j != (loc_len - 1)) && (node_name[name_len] != ':')) {
				name_len++;
				continue;
			}
			if (name_len == 0) {
				continue;
			}

			if (j != (loc_len - 1))
				node_name[name_len] = '\0';
			else
				node_name[name_len + 1] = '\0';

			next_node = tegrabl_add_subnode_if_absent(fdt, prev_node,
						node_name);
			if (next_node < 0) {
				pr_error("Can not create node %s\n", node_name);
				return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 0);
			}
			name_len = 0;
			prev_node = next_node;
		}

		err = fdt_setprop_string(fdt, next_node, prop_name, loc_name);
		if (err < 0) {
			pr_error("Can't set /chosen/plugin-manager/ids/%s (%s)\n",
				 prop_name, fdt_strerror(err));
			return TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
		}
	}

	return TEGRABL_NO_ERROR;
}

#define CHIP_ID_MAX 5
static tegrabl_error_t create_chip_id(void *fdt, int pm_node)
{
	int err, chip_node;
	char prop_name[CHIP_ID_MAX];
	struct tegrabl_chip_info info;

	chip_node = tegrabl_add_subnode_if_absent(fdt, pm_node, "chip-id");
	if (chip_node < 0) {
		pr_error("Could not create chip-id node\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 0);
	}

	tegrabl_get_chip_info(&info);

	tegrabl_snprintf(prop_name, CHIP_ID_MAX, "%c%02u%c",
					info.major + 64, info.minor,
					(info.revision == 0) ? 0 : info.revision + 'O');

	pr_info("Adding plugin-manager/chip-id/%s\n", prop_name);

	err = fdt_setprop_cell(fdt, chip_node, prop_name, 1);
	if (err < 0) {
		pr_error("Can't set /chosen/plugin-manager/chip-id/%s (%s)\n",
				prop_name, fdt_strerror(err));
		return TEGRABL_ERROR(TEGRABL_ERR_DT_PROP_ADD_FAILED, 0);
	}

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t add_id_info_to_dt(void *fdt, int pm_node,
										 struct board_id_info *id_info)
{
	tegrabl_error_t status;

	/* Add board_id string without rev */
	status = create_pm_ids(fdt, pm_node, id_info);
	if (status != TEGRABL_NO_ERROR) {
		pr_error("Error: Failed to add id to PM\n");
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 1);
		goto done;
	}

	/* Add board_id with rev for specified boards */
	status = create_pm_ids_with_rev(fdt, pm_node, id_info);
	if (status != TEGRABL_NO_ERROR) {
		pr_error("Error: Failed to add id with rev to PM\n");
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 2);
		goto done;
	}

	/* Add board_id with configs specified boards */
	status = create_pm_ids_with_config(fdt, pm_node, id_info);
	if (status != TEGRABL_NO_ERROR) {
		pr_error("Error: Failed to add id with configs to PM\n");
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 3);
		goto done;
	}

	/* Add chipid string */
	status = create_chip_id(fdt, pm_node);
	if (status != TEGRABL_NO_ERROR) {
		pr_error("Error: Failed to add chip id to PM\n");
		status = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 4);
		goto done;
	}

done:
	return status;
}

tegrabl_error_t tegrabl_add_plugin_manager_ids(void *fdt, int nodeoffset)
{
	int pm_node, id_node, config_node, con_node;
	struct board_id_info id_info;
	tegrabl_error_t err;

	TEGRABL_ASSERT(fdt);

	pm_node = tegrabl_add_subnode_if_absent(fdt, nodeoffset, "plugin-manager");
	if (pm_node < 0) {
		pr_error("Could not add /chosen/plugin-manager node\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 0);
	}

	id_node = tegrabl_add_subnode_if_absent(fdt, pm_node, "ids");
	if (id_node < 0) {
		pr_error("Could not add /chosen/plugin-manager/ids subnode\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 1);
	}

	con_node = tegrabl_add_subnode_if_absent(fdt, id_node, "connection");
	if (con_node < 0) {
		pr_error("Could not add /chosen/plugin-manager/ids/conneection\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 1);
	}

	config_node = tegrabl_add_subnode_if_absent(fdt, pm_node, "configs");
	if (config_node < 0) {
		pr_error("Could not add /chosen/plugin-manager/config subnode\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_ADD_FAILED, 1);
	}

	/* Init buffer before retrieving ids */
	id_info.version = BOARD_ID_INFO_VERSION_1;
	id_info.count = 0;

	err = tegrabl_get_board_ids((void *)&id_info);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Error: Failed to get board ids (err: %d)\n", err);
		return err;
	}

	return add_id_info_to_dt(fdt, pm_node, &id_info);
}
