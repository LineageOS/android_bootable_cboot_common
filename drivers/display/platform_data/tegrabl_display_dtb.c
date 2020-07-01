/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define MODULE TEGRABL_ERR_DISPLAY_PDATA

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
#include <tegrabl_clock.h>
#include <tegrabl_soc_misc.h>
#if defined(CONFIG_ENABLE_DP)
#include <tegrabl_dpaux.h>
#endif
#include <tegrabl_display_soc.h>

#define DC_OR_NODE "nvidia,dc-or-node"

/* Delay required due to the PD/Alt Mode negotiation process in usb type-C.
 * Marking it 1 for now we can modify this delay as per board requirement
 */
#define HPD_TIMEOUT_MS 1

/* structure for safe-keeping node offsets*/
struct offset {
	int32_t out;
	int32_t host1x;
	int32_t nvdisp;
	int32_t display;
	int32_t disp_out;
	int32_t dpaux;
};

/* structure to read and determine which prod_pair we should consider based on clk*/
#if defined(IS_T186)
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
#else
struct prod_pair tmds_config_modes[] = {
	{ /* 54 MHz */
		.clk = 54000000,
		.name = "prod_c_hdmi_0m_54m"
	},
	{ /* 111 MHz */
		.clk = 111000000,
		.name = "prod_c_hdmi_54m_111m"
	},
	{ /* 223 MHz */
		.clk = 223000000,
		.name = "prod_c_hdmi_111m_223m"
	},
	{ /* 300 MHz */
		.clk = 300000000,
		.name = "prod_c_hdmi_223m_300m"
	},
	{ /* 600 MHz */
		.clk = 600000000,
		.name = "prod_c_hdmi_300m_600m"
	}
};
#endif
uint32_t num_tmds_config_modes = ARRAY_SIZE(tmds_config_modes);

/* struct to read dp prod setting*/
struct prod_pair dp_node[] = {
	{
		.clk = 0, /*not used*/
		.name = "prod_c_dp"
	},
};

/* struct to read dp prod setting based on bitrate*/
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

/*either status or status_bl should be "okay"*/
static tegrabl_error_t check_status_of_node(void *fdt, int32_t node_offset)
{
	const char *status;

	status = fdt_getprop(fdt, node_offset, "bootloader-status", NULL);
	if (!status) {
		pr_debug("no bootloader-status property found\n");
	} else if (strcmp(status, "okay")) {
		pr_debug("status of this node is \"disabled\"\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NO_ACCESS, 0);
	} else {
		return TEGRABL_NO_ERROR;
	}

	/*if bootloader-status node is not there we should check status node*/
	status = fdt_getprop(fdt, node_offset, "status", NULL);
	if (!status) {
		pr_debug("error while finding \"status\" property\n");
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	} else if (strcmp(status, "okay")) {
		pr_debug("status of this node is \"disabled\"\n");
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	return TEGRABL_NO_ERROR;
}

#if defined(CONFIG_ENABLE_DP)
/* check if DP panel is connected or not*/
static tegrabl_error_t is_dp_connected(void *fdt, struct offset *off, struct tegrabl_display_pdata *pdata_dp,
	int32_t dpaux_instance)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_dpaux *hdpaux;
	const struct fdt_property *property;
	bool cur_hpd;
	time_t start_time, elapsed_time;

	property = fdt_get_property(fdt, off->display, "nvidia,is_ext_dp_panel", NULL);
	if (property != NULL) {
		pdata_dp->dp_dtb.is_ext_dp_panel = fdt32_to_cpu(*(property->data32));
		pr_debug("dp_dtb->is_ext_dp_panel = %d\n", pdata_dp->dp_dtb.is_ext_dp_panel);
	} else {
		pdata_dp->dp_dtb.is_ext_dp_panel = 0; /*default to internal panel*/
		pr_warn("nvidia,is_ext_dp_panel property not found, set default as internal panel\n");
	}

	if (!tegrabl_is_fpga()) {
		if (pdata_dp->dp_dtb.is_ext_dp_panel == 1) {
			err = parse_dp_regulator_settings(fdt, off->nvdisp, &(pdata_dp->dp_dtb));
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}

			err = tegrabl_display_init_regulator(DISPLAY_OUT_DP, pdata_dp);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
		} else {
			err = parse_edp_regulator_settings(fdt, off->nvdisp, &(pdata_dp->dp_dtb));
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}

			err = tegrabl_display_init_regulator(DISPLAY_OUT_EDP, pdata_dp);
			if (err != TEGRABL_NO_ERROR) {
				goto fail;
			}
		}
	}

	/*check for DP cable connection*/
	err = tegrabl_dpaux_init_aux(dpaux_instance, &hdpaux);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto fail;
	}

	/*polling is required to check hpd status until usb type-c bug 200404683 is fixed.*/
	start_time = tegrabl_get_timestamp_ms();
	do {
		err = tegrabl_dpaux_hpd_status(hdpaux, &cur_hpd);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("DP hpd status read failed\n");
			TEGRABL_SET_HIGHEST_MODULE(err);
			goto fail;
		}

		elapsed_time = tegrabl_get_timestamp_ms() - start_time;
	} while ((elapsed_time < HPD_TIMEOUT_MS) && !cur_hpd);

	if (!cur_hpd) {
		pr_info("DP is not connected\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_CONNECTED, 0);
		goto fail;
	} else {
		pr_info("DP is connected\n");
	}

fail:
	return err;
}

/*if DP is connected, parse DP dtb params*/
static tegrabl_error_t parse_dp_dtb(void *fdt, struct offset *off, struct tegrabl_display_dp_dtb *pdata_dp)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int32_t prod_offset;

	err = parse_dp_dtb_settings(fdt, off->display, pdata_dp);
	if (err != TEGRABL_NO_ERROR) {
		pr_warn("dp lt-settings not read properly from dtb, we can still continue\n");
	}

	/* get prod-settings offset of dp*/
	prod_offset = fdt_subnode_offset(fdt, off->out, "prod-settings");
	if (prod_offset < 0) {
		pr_error("prod-settings subnode not found\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 3);
		goto fail_parse;
	}

	err = parse_prod_settings(fdt, prod_offset, &(pdata_dp->prod_list), dp_node, ARRAY_SIZE(dp_node));
	if (err != TEGRABL_NO_ERROR) {
		goto fail_parse;
	}

	err = parse_prod_settings(fdt, prod_offset, &(pdata_dp->br_prod_list), dp_br_nodes,
							  ARRAY_SIZE(dp_br_nodes));
	if (err != TEGRABL_NO_ERROR) {
		goto fail_parse;
	}

fail_parse:
	return err;
}
#endif

/*parse platform data for particular display unit based on du_type*/
static tegrabl_error_t tegrabl_display_get_pdata(void *fdt, struct offset *off, int32_t du_type,
	struct tegrabl_display_pdata **pdata, int32_t dpaux_instance)
{
	struct tegrabl_display_pdata *pdata_l = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	int32_t prod_offset;

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
		err = is_dp_connected(fdt, off, pdata_l, dpaux_instance);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		tegrabl_display_parse_xbar(fdt, off->out, &(pdata_l->sor_dtb));

		err = parse_dp_dtb(fdt, off, &(pdata_l->dp_dtb));
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		pdata_l->mode = parse_display_timings(fdt, off->display);
#endif
	} else if (du_type == DISPLAY_OUT_HDMI) {
		if (!tegrabl_is_fpga()) {
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
		}

		tegrabl_display_parse_xbar(fdt, off->out, &(pdata_l->sor_dtb));

		prod_offset = fdt_subnode_offset(fdt, off->out, "prod-settings");
		if (prod_offset < 0) {
			pr_error("prod-settings node not found\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 5);
			goto fail_parse;
		}

		err = parse_prod_settings(fdt, prod_offset, &(pdata_l->hdmi_dtb.prod_list), tmds_config_modes,
								  num_tmds_config_modes);
		if (err != TEGRABL_NO_ERROR) {
			goto fail_parse;
		}

		pdata_l->mode = parse_display_timings(fdt, off->display);
	} else {
		pr_error("invalid display type\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
	}

	*pdata = pdata_l;
	return err;

fail_parse:
	pr_error("%s, failed to parse dtb settings\n", __func__);
	if (pdata_l) {
		tegrabl_free(pdata_l);
	}
	return err;
}

/* add valid display units in a linked list */
static tegrabl_error_t tegrabl_display_add_du(int32_t du_type, struct tegrabl_display_pdata *pdata,
	struct tegrabl_display_list **head_du)
{
	struct tegrabl_display_list *new_node = NULL;
	struct tegrabl_display_list *last = *head_du;

	/* allocate node */
	new_node = tegrabl_malloc(sizeof(struct tegrabl_display_list));
	if (new_node == NULL) {
		pr_error("memory allocation failed\n");
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

/* from parent node, get the phandle of node pointed to by nodename
  * and then gets that handle's parent-node-offset */
static tegrabl_error_t get_offset_by_phandle(void *fdt, int32_t parent_off, char *nodename, int32_t *off)
{
	int32_t offset;
	uint32_t phandle;
	const uint32_t *temp;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	temp = fdt_getprop(fdt, parent_off, nodename, NULL);
	if (temp != NULL) {
		phandle = fdt32_to_cpu(*temp);
		pr_debug("phandle = 0x%x\n", phandle);
	} else {
		pr_error("invalid phandle for %s\n", nodename);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	offset = fdt_node_offset_by_phandle(fdt, phandle);
	if (offset < 0) {
		pr_debug("invalid node offset\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 7);
		goto fail;
	}
	err = check_status_of_node(fdt, offset);
	if (err != TEGRABL_NO_ERROR) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto fail;
	}
	*off = offset;

fail:
	return err;
}

/*generate a linked list of enabled display uints & read their platform data*/
tegrabl_error_t tegrabl_display_get_du_list(
	struct tegrabl_display_list **du_list)
{
	void *fdt = NULL;
	int32_t temp_offset = -1;
	const uint32_t *temp = NULL;
	const char *clk;
	uint32_t clk_src;
	int32_t du_type = -1;
	int32_t sor_instance = -1;
	int32_t dpaux_instance = -1;
	struct offset *off;
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
		pr_error("error while finding host1x node\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 6);
		goto fail;
	}

	for (uint32_t i = 0;; i++) {
		off->nvdisp = fdt_node_offset_by_compatible(fdt, temp_offset, NVDISPLAY_NODE);
		if ((i == 0) && (off->nvdisp < 0)) {
			pr_error("cannot find any nvdisp node\n");
			err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 7);
			goto fail;
		} else if ((i > 0) && (off->nvdisp < 0)) {
			pr_error("cannot find any other nvdisp nodes\n");
			break;
		} else {
			pr_debug("found one nvdisp nodes at offset = %d\n", off->nvdisp);
		}
		temp_offset = off->nvdisp; /*search next node starting from this offset*/

		err = check_status_of_node(fdt, off->nvdisp);
		if (err != TEGRABL_NO_ERROR) {
			continue;
		}

		/*find out if nvdisp is connected to dsi/sor/sor1*/
		err = get_offset_by_phandle(fdt, off->nvdisp, "nvidia,dc-connector", &(off->out));
		if (err != TEGRABL_NO_ERROR) {
			continue;
		}

		/*find out the active panel in dsi/sor/sor1, it can be a panel or hdmi or dp*/
		err = get_offset_by_phandle(fdt, off->out, "nvidia,active-panel", &(off->display));
		if (err != TEGRABL_NO_ERROR) {
			continue;
		}

		off->disp_out = fdt_subnode_offset(fdt, off->display, "disp-default-out");
		if (off->disp_out < 0) {
			pr_debug("disp-default-out node not found\n");
			continue;
		}

		/*determine the display type 1:HDMI, 2:DSI, 3:DP*/
		temp = fdt_getprop(fdt, off->disp_out, "nvidia,out-type", NULL);
		if (temp != NULL) {
			du_type = fdt32_to_cpu(*temp);
			pr_debug("du_type = %d\n", du_type);
		} else {
			pr_error("error in getting \"nvidia,out-type\" property\n");
			continue;
		}

		/*determine sor instance and dpaux info for HDMI/DP*/
		if ((du_type == DISPLAY_OUT_HDMI) || (du_type == DISPLAY_OUT_DP)) {
			temp = fdt_getprop(fdt, off->out, "nvidia,sor-ctrlnum", NULL);
			if (temp != NULL) {
				sor_instance = fdt32_to_cpu(*temp);
				pr_debug("sor_instance = %d\n", sor_instance);
			} else {
				pr_error("error in getting \"nvidia,sor-ctrlnum\" property\n");
				continue;
			}

			err = get_offset_by_phandle(fdt, off->out, "nvidia,dpaux", &(off->dpaux));
			if (err != TEGRABL_NO_ERROR) {
				continue;
			}

			temp = fdt_getprop(fdt, off->dpaux, "nvidia,dpaux-ctrlnum", NULL);
			if (temp != NULL) {
				dpaux_instance = fdt32_to_cpu(*temp);
				pr_debug("dpaux_instance = %d\n", dpaux_instance);
			} else {
				pr_error("error in getting \"nvidia,dpaux-ctrlnum\" property\n");
				continue;
			}
		}

		clk = fdt_getprop(fdt, off->disp_out, "nvidia,out-parent-clk", NULL);
		if (clk != NULL) {
			if (!strcmp(clk, "plld2")) {
				clk_src = TEGRABL_CLK_SRC_PLLD2_OUT0;
			} else if (!strcmp(clk, "plld3")) {
				clk_src = TEGRABL_CLK_SRC_PLLD3_OUT0;
			} else if (!strcmp(clk, "pll_d")) {
				clk_src = TEGRABL_CLK_SRC_PLLD_OUT1;
			} else {
				continue;
			}
		} else {
			pr_error("error in getting \"nvidia,out-parent-clk\" property\n");
			continue;
		}

		err = tegrabl_display_get_pdata(fdt, off, du_type, &pdata, dpaux_instance);
		if (err != TEGRABL_NO_ERROR) {
			continue;
		}

		pdata->sor_dtb.sor_instance = sor_instance;
		pdata->sor_dtb.dpaux_instance = dpaux_instance;
		pdata->disp_clk_src = clk_src;

		err = tegrabl_display_add_du(du_type, pdata, &du_list_l);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	if (du_list_l == NULL) {
		pr_error("no valid display unit config found in dtb\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	} else {
		*du_list = du_list_l;
		return TEGRABL_NO_ERROR;
	}

fail:
	return err;
}
