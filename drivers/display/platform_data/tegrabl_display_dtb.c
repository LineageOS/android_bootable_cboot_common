/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_DISPLAY

#include <string.h>
#include <stdbool.h>
#include <libfdt.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_gpio.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_display_dtb.h>
#include <tegrabl_display_dtb_dp.h>
#include <tegrabl_display_dtb_hdmi.h>
#include <tegrabl_display_dtb_util.h>
#include <tegrabl_display_panel.h>
#if defined(CONFIG_ENABLE_DP)
#include <tegrabl_dpaux.h>
#endif

#define NVDISPLAY_NODE "nvidia,tegra186-dc"
#define HOST1X_NODE "nvidia,tegra186-host1x\0simple-bus"
#define DC_OR_NODE "nvidia,dc-or-node"
#define MAX_NVDISP_CONTROLLERS 3
#define HDMI_PROD_TUPLES 7
#define DPAUX_PROD_TUPLES 1
#define DP_PROD_TUPLES 9
#define DP_BR_PROD_TUPLES 1

static char *dpaux_node[] = {
	"nvidia,tegra186-dpaux",
	"nvidia,tegra186-dpaux1",
};

struct offset {
	int32_t out;
	int32_t host1x;
	int32_t nvdisp;
	int32_t dpaux;
};

struct prod_pair tmds_config_modes[] = {
	{ /* 54 MHz */
		.clk = 54000000,
		.name = "prod_c_54M"
	},
	{ /* 75 MHz */
		.clk = 75000000,
		.name = "prod_c_75M"
	},
	{ /* 150 MHz */
		.clk = 150000000,
		.name = "prod_c_150M"
	},
	{ /* 300 MHz */
		.clk = 300000000,
		.name = "prod_c_300M"
	},
	{ /* 600 MHz */
		.clk = 600000000,
		.name = "prod_c_600M"
	}
};
uint32_t num_tmds_config_modes = ARRAY_SIZE(tmds_config_modes);

struct prod_pair dp_node[] = {
	{
		.clk = 0, /*not used*/
		.name = "prod_c_dp"
	},
};
uint32_t num_dp_nodes = ARRAY_SIZE(dp_node);

struct prod_pair dp_br_nodes[] = {
	{ /*SOR_LINK_SPEED_G1_62*/
		.clk = 6,
		.name = "prod_c_rbr"
	},
	{ /*SOR_LINK_SPEED_G2_67*/
		.clk = 10,
		.name = "prod_c_hbr"
	},
	{ /*SOR_LINK_SPEED_G5_64*/
		.clk = 20,
		.name = "prod_c_hbr2"
	},
};
uint32_t num_dp_br_nodes = ARRAY_SIZE(dp_br_nodes);

struct prod_pair dp_dpaux_node[] = {
	{
		.clk = 0, /*not used*/
		.name = "prod_c_dpaux_dp"
	},
};
uint32_t num_dpaux_nodes = ARRAY_SIZE(dp_dpaux_node);

/*either status or status_bl should be "okay"*/
static tegrabl_error_t check_status_of_node(void *fdt, int32_t node_offset)
{
	const char *status;

	status = fdt_getprop(fdt, node_offset, "bootloader-status", NULL);
	if (!status) {
		pr_debug("error while finding bootloader-status property\n");
	} else if (strcmp(status, "okay")) {
		pr_debug("status of this node is \"disabled\"\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_ACCESS, 0);
	} else {
		return TEGRABL_NO_ERROR;
	}

	/*if bootloader-status node is not there we should check status node*/
	status = fdt_getprop(fdt, node_offset, "status", NULL);
	if (!status) {
		pr_debug("error while finding status property\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 0);
	} else if (strcmp(status, "okay")) {
		pr_debug("status of this node is \"disabled\"\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return TEGRABL_NO_ERROR;
}

/*check which of hdmi_display or dp_display has status=okay*/
static tegrabl_error_t check_hdmi_or_dp_node(void *fdt, int32_t offset,
	int32_t *du_type)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int32_t temp_offset;
	bool hdmi_node_found = false;

	temp_offset = fdt_subnode_offset(fdt, offset, "hdmi-display");
	if (temp_offset < 0) {
		pr_error("hdmi subnode not found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 1);
	} else {
		err = check_status_of_node(fdt, temp_offset);
		if (err == TEGRABL_NO_ERROR) {
			pr_debug("hdmi subnode found with status = okay\n");
			*du_type = DISPLAY_OUT_HDMI;
			hdmi_node_found = true;
		}
	}

	temp_offset = fdt_subnode_offset(fdt, offset, "dp-display");
	if (temp_offset < 0) {
		pr_error("DP subnode not found\n");
		return err;
	}
	err = check_status_of_node(fdt, temp_offset);
	if (err == TEGRABL_NO_ERROR) {
		if (hdmi_node_found == true) {
			pr_error("HDMI and DP both subnodes are enabled\n");
			return TEGRABL_ERROR(TEGRABL_ERR_INVALID_STATE, 0);
		} else {
			pr_debug("DP subnode found with status = okay\n");
			*du_type = DISPLAY_OUT_DP;
			return err;
		}
	} else if (hdmi_node_found == true) {
		return TEGRABL_NO_ERROR;
	} else {
		return err;
	}
}

/*check dpaux node status is okay or not*/
static tegrabl_error_t check_dpaux(void *fdt, struct offset *off, char *dpaux)
{
	int32_t offset;

	offset = fdt_node_offset_by_compatible(fdt, off->host1x, dpaux);
	if (offset < 0) {
		pr_debug("dpaux node not found\n");
		return TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 2);
	}
	off->dpaux = offset;
	return check_status_of_node(fdt, offset);
}

/*parse platform data for particular display unit*/
static tegrabl_error_t tegrabl_display_get_pdata(void *fdt,
	struct offset *off, int32_t du_type, struct tegrabl_display_pdata **pdata,
	int32_t sor_instance)
{
	struct tegrabl_display_pdata *pdata_l = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int32_t prod_offset;
#if defined(CONFIG_ENABLE_DP)
	struct tegrabl_dpaux *hdpaux;
	bool cur_hpd;
#endif
	pdata_l = tegrabl_malloc(sizeof(struct tegrabl_display_pdata));
	if (!pdata_l) {
		pr_error("memory allocation failed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail_parse;
	}
	memset(pdata_l, 0, sizeof(struct tegrabl_display_pdata));

	parse_nvdisp_dtb_settings(fdt, off->nvdisp, pdata_l);

	if (du_type == DISPLAY_OUT_DSI) {
		pr_error("dsi not supported yet\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
		goto fail_parse;
#if defined(CONFIG_ENABLE_DP)
	} else if (du_type == DISPLAY_OUT_DP) {
		err = parse_dp_regulator_settings(fdt, off->nvdisp, pdata_l);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		err = tegrabl_display_init_regulator(du_type, pdata_l);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		/*check for DP cable connection*/
		err = tegrabl_dpaux_init_aux(sor_instance, &hdpaux);
		if (err != TEGRABL_NO_ERROR) {
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail_parse;
		}

		err = tegrabl_dpaux_hpd_status(hdpaux, &cur_hpd);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("DP hpd status read failed\n");
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail_parse;
		}
		if (!cur_hpd) {
			pr_error("DP not connected\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_CONNECTED, 0);
			goto fail_parse;
		}
		pr_info("DP is connected\n");

		tegrabl_display_parse_xbar(fdt, off->out, pdata_l);

		err = parse_dp_dtb_settings(fdt, off->out, &(pdata_l->dp_dtb));
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		/* get prod-settings offset of dp*/
		prod_offset = fdt_subnode_offset(fdt, off->out, "prod-settings");
		if (prod_offset < 0) {
			pr_error("prod-settings subnode not found\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 3);
			goto fail_parse;
		}

		err = parse_prod_settings(fdt, prod_offset,
					&(pdata_l->dp_dtb.prod_list), dp_node, num_dp_nodes,
					DP_PROD_TUPLES);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		err = parse_prod_settings(fdt, prod_offset,
					&(pdata_l->dp_dtb.br_prod_list), dp_br_nodes,
					num_dp_br_nodes, DP_BR_PROD_TUPLES);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		/* get prod-offset of dpaux separately as it is in different node*/
		prod_offset = fdt_subnode_offset(fdt, off->dpaux, "prod-settings");
		if (prod_offset < 0) {
			pr_error("prod-settings node not found\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 4);
			goto fail_parse;
		}
		err = parse_prod_settings(fdt, prod_offset,
					&(pdata_l->dp_dtb.dpaux_prod_list), dp_dpaux_node,
					num_dpaux_nodes, DPAUX_PROD_TUPLES);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}
#endif
	} else if (du_type == DISPLAY_OUT_HDMI) {
		err = parse_hdmi_regulator_settings(fdt, off->nvdisp, pdata_l);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		err = parse_hpd_gpio(fdt, off->out, pdata_l);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		err = tegrabl_display_init_regulator(du_type, pdata_l);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		prod_offset = fdt_subnode_offset(fdt, off->out, "prod-settings");
		if (prod_offset < 0) {
			pr_error("prod-settings node not found\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 5);
			goto fail_parse;
		}

		err = parse_prod_settings(fdt, prod_offset,
					&(pdata_l->hdmi_dtb.prod_list), tmds_config_modes,
					num_tmds_config_modes, HDMI_PROD_TUPLES);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}
	} else {
		pr_error("invalid display type\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	*pdata = pdata_l;
	return err;

fail_parse:
	pr_debug("%s, failed to parse dtb settings\n", __func__);
	if (pdata_l) {
		tegrabl_free(pdata_l);
	}
	return err;
}

/* add valid display units in a linked list */
static tegrabl_error_t tegrabl_display_add_du(int32_t du_type,
	struct tegrabl_display_pdata *pdata,
	struct tegrabl_display_list **head_du)
{
	struct tegrabl_display_list *new_node = NULL;
	struct tegrabl_display_list *last = *head_du;

	/* allocate node */
	new_node = tegrabl_malloc(sizeof(struct tegrabl_display_list));
	if (new_node == NULL) {
		pr_debug("memory allocation failed\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
	}

	/* put in the data  */
	new_node->du_type = du_type;
	new_node->pdata = pdata;
	/* This new node will be the last node, so make next of it as NULL */
	new_node->next = NULL;

	/* If the Linked List is empty, then make the new node as head */
	if (*head_du == NULL) {
		*head_du = new_node;
		return TEGRABL_NO_ERROR;
	}

	/* Else traverse till the last node */
	while (last->next != NULL) {
		last = last->next;
	}

	/* Change the next of last node */
	last->next = new_node;
	return TEGRABL_NO_ERROR;
}

/*generate a linked list of enabled display uints & read their platform data*/
tegrabl_error_t tegrabl_display_get_du_list(
	struct tegrabl_display_list **du_list)
{
	void *fdt = NULL;
	struct offset *off;
	int32_t temp_offset = -1;
	int32_t du_type = -1;
	int32_t sor_instance = -1;
	const char *dc_or_node;
	struct tegrabl_display_pdata *pdata = NULL;
	struct tegrabl_display_list *du_list_l = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	off = tegrabl_malloc(sizeof(struct offset));
	if (!off) {
		pr_error("memory allocation failed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 2);
		goto fail;
	}

	err = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get bl-dtb handle\n");
		goto fail;
	}

	off->host1x = fdt_node_offset_by_compatible(fdt, -1, HOST1X_NODE);
	if (off->host1x < 0) {
		pr_error("error while finding compatible node\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 6);
		goto fail;
	}

	for (uint32_t i = 0; i < MAX_NVDISP_CONTROLLERS; i++) {
		off->nvdisp = fdt_node_offset_by_compatible(fdt, temp_offset,
													NVDISPLAY_NODE);
		temp_offset = off->nvdisp;
		if ((i == 0) && (off->nvdisp < 0)) {
			pr_error("cannot find any nvdisp node\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_DT_NODE_NOT_FOUND, 7);
			goto fail;
		} else if ((i > 0) && (off->nvdisp < 0)) {
			pr_error("cannot find any other nvdisp nodes\n");
			break;
		} else {
			pr_info("found one nvdisp nodes at offset = %d\n",
					off->nvdisp);
		}

		err = check_status_of_node(fdt, off->nvdisp);
		if (err != TEGRABL_NO_ERROR) {
			continue;
		}

		dc_or_node = fdt_getprop(fdt, off->nvdisp, DC_OR_NODE, NULL);
		pr_debug("dc_or_node is %s\n", dc_or_node);
		if (!dc_or_node) {
			pr_debug("dc-or-node property not found in this nvdisp node\n");
			continue;
		} else if (strcmp(dc_or_node, "/host1x/dsi") == 0) {
			off->out = fdt_subnode_offset(fdt, off->host1x, "dsi");
			if (off->out < 0) {
				pr_debug("/host1x/dsi node not found\n");
				continue;
			}
			err = check_status_of_node(fdt, off->out);
			if (err != TEGRABL_NO_ERROR) {
				continue;
			}
			du_type = DISPLAY_OUT_DSI;
			sor_instance = -1;
		} else if (strcmp(dc_or_node, "/host1x/sor1") == 0) {
			off->out = fdt_subnode_offset(fdt, off->host1x, "sor1");
			if (off->out < 0) {
				pr_debug("/host1x/sor1 node not found\n");
				continue;
			}
			err = check_status_of_node(fdt, off->out);
			if (err != TEGRABL_NO_ERROR) {
				continue;
			}
			err = check_hdmi_or_dp_node(fdt, off->out, &du_type);
			if (err != TEGRABL_NO_ERROR) {
				continue;
			}
			if (du_type == DISPLAY_OUT_DP) {
				err = check_dpaux(fdt, off, dpaux_node[1]);
				if (err != TEGRABL_NO_ERROR) {
					continue;
				}
			}
			sor_instance = 1;
		} else if (strcmp(dc_or_node, "/host1x/sor") == 0) {
			off->out = fdt_subnode_offset(fdt, off->host1x, "sor");
			if (off->out < 0) {
				pr_debug("/host1x/sor node not found\n");
				continue;
			}
			err = check_status_of_node(fdt, off->out);
			if (err != TEGRABL_NO_ERROR) {
				continue;
			}
			err = check_hdmi_or_dp_node(fdt, off->out, &du_type);
			if (err != TEGRABL_NO_ERROR) {
				continue;
			}
			if (du_type == DISPLAY_OUT_DP) {
				err = check_dpaux(fdt, off, dpaux_node[0]);
				if (err != TEGRABL_NO_ERROR) {
					continue;
				}
			}
			sor_instance = 0;
		} else {
			pr_debug("no valid dc_or_node in this controller\n");
			continue;
		}

		err = tegrabl_display_get_pdata(fdt, off, du_type, &pdata,
										sor_instance);
		if (err != TEGRABL_NO_ERROR) {
			continue;
		}
		pdata->sor_instance = sor_instance;
		pdata->nvdisp_instance = i;

		err = tegrabl_display_add_du(du_type, pdata, &du_list_l);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}
	if (du_list_l == NULL) {
		pr_info("no valid display unit config found in dtb\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	} else {
		*du_list = du_list_l;
		return TEGRABL_NO_ERROR;
	}

fail:
	return err;
}
