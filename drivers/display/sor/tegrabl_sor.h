/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef __TEGRABL_SOR_H__
#define __TEGRABL_SOR_H__

#include <tegrabl_nvdisp.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_io.h>

/* macro tegra sor clock */
#define TEGRA_SOR_SAFE_CLK 1
#define TEGRA_SOR_LINK_CLK 2

#define MAX_1_4_FREQUENCY 340000000 /*340 MHz*/

#define SOR_LINK_SPEED_G1_62	6
#define SOR_LINK_SPEED_G2_7		10
#define SOR_LINK_SPEED_G5_4		20
#define SOR_LINK_SPEED_LVDS		7

#define SOR_TIMEOUT_MS			1000
#define SOR_ATTACH_TIMEOUT_MS	100000
#define SOR_SEQ_BUSY_TIMEOUT_MS	10000
#define NVDISP_POLL_TIMEOUT_MS	50

#define sor_writel_def(reg, fld, def, v) \
	do { \
		v = sor_readl(sor, SOR_NV_PDISP_##reg##_0); \
		v = NV_FLD_SET_DRF_DEF(SOR_NV_PDISP, reg, fld, def, v); \
		sor_writel(sor, SOR_NV_PDISP_##reg##_0, v); \
	} while (0)

#define sor_writel_num(reg, fld, num, v) \
	do { \
		v = sor_readl(sor, SOR_NV_PDISP_##reg##_0); \
		v = NV_FLD_SET_DRF_NUM(SOR_NV_PDISP, reg, fld, num, v); \
		sor_writel(sor, SOR_NV_PDISP_##reg##_0, v); \
	} while (0)

/**
* @brief data structure for DP Link Configuration
*/
struct tegrabl_dp_link_config {
	bool is_valid; /* True if link config adheres to dp spec.
					* Does not guarantee link training success.
					*/

	/* Supported configuration */
	uint8_t max_link_bw;
	uint8_t max_lane_count;
	bool downspread;
	bool support_enhanced_framing;
	bool support_vsc_ext_colorimetry;
	uint32_t bits_per_pixel;
	bool alt_scramber_reset_cap; /* true for eDP */
	bool only_enhanced_framing; /* enhanced_frame_en ignored */
	bool edp_cap; /* eDP display control capable */
	bool support_fast_lt; /* Support fast link training */

	/* Actual configuration */
	uint8_t link_bw;
	uint8_t lane_count;
	bool enhanced_framing;
	bool scramble_ena;

	uint32_t activepolarity;
	uint32_t active_count;
	uint32_t tu_size;
	uint32_t active_frac;
	uint32_t watermark;

	int32_t hblank_sym;
	int32_t vblank_sym;

	bool lt_data_valid; /* True only if link training passed with this
						 * drive_current, preemphasis and postcursor.
						 */
	uint32_t drive_current[4];
	uint32_t preemphasis[4];
	uint32_t postcursor[4];

	/*
	 * Training Pattern Sequence to start channel equalization with,
	 * calculated based on an intersection of source and sink capabilities
	 */
	uint32_t tps;

	uint8_t aux_rd_interval;
};

/**
* @brief data structure for Sor controller
*/
struct sor_data {
	struct tegrabl_nvdisp *nvdisp;
	void *base;
	uint32_t instance;
	uint8_t portnum; /* 0 or 1 */
	uint8_t clk_type;
	uint32_t xbar_ctrl[5];
	uint32_t parent_clk;
	const struct tegrabl_dp_link_config *link_cfg;
};

/**
 *  @brief Initialize Sor controller
 *
 *  @param phsor pointer to SOR Handle
 *  @param sor_dtb pointer to sor dtb data structure
 */
tegrabl_error_t sor_init(struct sor_data **phsor, struct tegrabl_display_sor_dtb *sor_dtb);

/**
 *  @brief Set panel settings in Sor register
 *
 *  @param sor SOR Handle
 *  @param is_int internal or external panel
 */
void sor_set_internal_panel(struct sor_data *sor, bool is_int);

/**
 *  @brief Set power state in Sor register
 *
 *  @param sor SOR Handle
 *  @param pu_pd Pull-Up or Pull-Down
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sor_set_power_state(struct sor_data *sor, uint32_t pu_pd);

/**
 *  @brief Program HDMI Pad Power up sequence in Sor register
 *
 *  @param sor SOR Handle
 */
void sor_hdmi_pad_power_up(struct sor_data *sor);

/**
 *  @brief Set power lane settings in Sor register
 *
 *  @param sor SOR Handle
 *  @param lane_count the lane configuration of the DP datapath
 *  @param pu Pull-Up or not
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t sor_power_lanes(struct sor_data *sor, uint32_t lane_count, bool pu);

/**
 *  @brief Set link bandwidth in Sor register
 *
 *  @param sor SOR Handle
 *  @param link_bw link bandwidth (2.7GHz, 5.4Ghz etc.)
 */
void sor_set_link_bandwidth(struct sor_data *sor, uint8_t link_bw);

/**
 *  @brief config HDMI clock setting in SOR register
 *
 *  @param sor SOR Handle
 *  @param pclk clock in Hz
 */
void sor_config_hdmi_clk(struct sor_data *sor, uint32_t pclk);

/**
 *  @brief Program Sor attach sequence in Sor register
 *
 *  @param sor SOR Handle
 */
void sor_attach(struct sor_data *sor);

/**
 *  @brief Polling Sor register status
 *
 *  @param sor SOR Handle
 *  @param reg Register to check
 *  @param mask mask bit
 *  @param exp_val expected value
 *  @param poll_interval_us polling interval
 *  @param timeout_ms timeout in ms
 */
tegrabl_error_t sor_poll_register(struct sor_data *sor, uint32_t reg,
	uint32_t mask, uint32_t exp_val, uint32_t poll_interval_us,
	uint32_t timeout_ms);

/**
 *  @brief Dump Sor registers
 *
 *  @param sor SOR Handle
 */
void sor_dump_registers(struct sor_data *sor);

/**
 *  @brief Sor super update
 *
 *  @param sor SOR Handle
 */
void sor_super_update(struct sor_data *sor);

/**
 *  @brief Sor update
 *
 *  @param sor SOR Handle
 */
void sor_update(struct sor_data *sor);

/**
 *  @brief Sor Enable
 *
 *  @param sor SOR Handle
 *  @param enable true/false
 */
void sor_enable_sor(struct sor_data *sor, bool enable);

/**
 *  @brief Configures Sor Xbar (lane sequence)
 *
 *  @param sor SOR Handle
 */
void sor_config_xbar(struct sor_data *sor);

/**
 *  @brief power on/off pad calibration logic
 *
 *  @param sor SOR Handle
 *  @param power_up true or false
 */
void sor_pad_cal_power(struct sor_data *sor, bool power_up);

/**
 *  @brief sor termination calibration
 *
 *  @param sor SOR Handle
 */
void sor_termination_cal(struct sor_data *sor);

/**
 *  @brief rterm calibration for hdmi/DP
 *
 *  @param sor SOR Handle
 */
void sor_cal(struct sor_data *sor);

/**
 *  @brief configures prod settings for DP and HDMI
 *
 *  @param sor SOR Handle
 *  @param prod_list prod settings to be configured
 *  @param node dp or hdmi node to be configured
 *  @param clk clock - valid for only hdmi
 */
void sor_config_prod_settings(struct sor_data *sor, struct prod_list *prod_list,
	struct prod_pair *node, uint32_t clk);

static inline uint32_t sor_readl(struct sor_data *sor, uint32_t reg)
{
	return NV_READ32(sor->base + reg * 4);
}

static inline void sor_writel(struct sor_data *sor, uint32_t reg, uint32_t val)
{
	NV_WRITE32(sor->base + reg * 4, val);
}

static inline void tegrabl_sor_write_field(struct sor_data *sor,
	uint32_t reg, uint32_t mask, uint32_t val)
{
	uint32_t reg_val = sor_readl(sor, reg);
	reg_val &= ~mask;
	reg_val |= val;
	sor_writel(sor, reg, reg_val);
}
#endif
