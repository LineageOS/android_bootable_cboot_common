/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef __TEGRABL_DISPLAY_DTB_H
#define __TEGRABL_DISPLAY_DTB_H

#include <stdbool.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>

#define POST_CURSOR2_COUNT 4
#define PRE_EMPHASIS_COUNT 4
#define VOLTAGE_SWING_COUNT 4
#define XBAR_CNT 5

enum tegrabl_display_unit_type {
	DISPLAY_OUT_DSI,
	DISPLAY_OUT_HDMI,
	DISPLAY_OUT_DP,
	DISPLAY_OUT_EDP,
	DISPLAY_OUT_MAX,
};

struct prod_pair {
	uint32_t clk;
	const char *name;
};

extern struct prod_pair tmds_config_modes[];
extern uint32_t num_tmds_config_modes;
extern struct prod_pair dp_node[];
extern uint32_t num_dp_nodes;
extern struct prod_pair dp_br_nodes[];
extern uint32_t num_dp_br_nodes;
extern struct prod_pair dp_dpaux_node[];
extern uint32_t num_dpaux_nodes;

struct prod_tuple {
	uint32_t addr;
	uint32_t mask;
	uint32_t val;
};

struct prod_settings {
	struct prod_tuple *prod_tuple;
	uint32_t count;
};

struct prod_list {
	struct prod_settings *prod_settings;
	uint32_t num;
};

struct dp_lt_settings {
	uint32_t drive_current[VOLTAGE_SWING_COUNT];
	uint32_t pre_emphasis[PRE_EMPHASIS_COUNT];
	uint32_t post_cursor2[POST_CURSOR2_COUNT];
	uint32_t tx_pu;
	uint32_t load_adj;
};

struct dp_lt_data {
	uint32_t data[POST_CURSOR2_COUNT][PRE_EMPHASIS_COUNT][VOLTAGE_SWING_COUNT];
	char name[64];
};

/**
 *  @brief structure to hold hdmi params parsed from dtb
 */
struct tegrabl_display_hdmi_dtb {
	int32_t avdd_hdmi_supply;
	int32_t avdd_hdmi_pll_supply;
	int32_t vdd_hdmi_5v0_supply;
	uint32_t hpd_gpio;
	uint32_t polarity;
	struct prod_list *prod_list;
};

/**
 *  @brief structure to hold dp params parsed from dtb
 */
struct tegrabl_display_dp_dtb {
	uint32_t is_ext_dp_panel;
	int32_t vdd_dp_pwr_supply;
	int32_t avdd_dp_pll_supply;
	int32_t vdd_dp_pad_supply;
	struct prod_list *prod_list;
	struct prod_list *br_prod_list;
	struct prod_list *dpaux_prod_list;
	struct dp_lt_data *lt_data;
	struct dp_lt_settings *lt_settings;
};

/**
 *  @brief structure to hold all display params parsed from dtb
 */
struct tegrabl_display_pdata {
	uint32_t flags;
	uint32_t win_id;
	int32_t sor_instance;
	int32_t nvdisp_instance;
	uint32_t xbar_ctrl[XBAR_CNT];
	struct nvdisp_mode *mode;
	struct tegrabl_display_hdmi_dtb hdmi_dtb;
	struct tegrabl_display_dp_dtb dp_dtb;
	/* TODO: add dsi/edp dtb structure here */
};

/**
 *  @brief structure to hold all display unit types and their platform data
 */
struct tegrabl_display_list {
	int32_t du_type;
	struct tegrabl_display_pdata *pdata;
	struct tegrabl_display_list *next;
};

/**
 *  @brief Parse dtb and get valid display unit types and their pdata
 *
 *  @param du_list holds type and pdata of each valid display unit type
 *
 *  @return TEGRABL_NO_ERROR if success, specific error if fails.
 */
tegrabl_error_t tegrabl_display_get_du_list(
	struct tegrabl_display_list **du_list);

#endif
