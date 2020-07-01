/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all uint32_tellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef _TEGRABL_EDID_H__
#define _TEGRABL_EDID_H__

#include <tegrabl_nvdisp_local.h>

struct timing {
	uint32_t width;
	uint32_t height;
	uint32_t frequency;
};

struct detailed_timing {
	uint32_t pixel_clock;
	uint32_t h_addr;
	uint32_t h_blank;
	uint32_t h_sync;
	uint32_t h_front_porch;
	uint32_t v_addr;
	uint32_t v_blank;
	uint32_t v_sync;
	uint32_t v_front_porch;
	uint32_t width_mm;
	uint32_t height_mm;
	uint32_t right_border;
	uint32_t top_border;
	uint32_t interlaced;
};

struct hdmi_timings {
	uint32_t h_ref_to_sync;
	uint32_t v_ref_to_sync;
	uint32_t h_sync_width;
	uint32_t v_sync_width;
	uint32_t h_back_porch;
	uint32_t v_back_porch;
	uint32_t h_disp_active;
	uint32_t v_disp_active;
	uint32_t h_front_porch;
	uint32_t v_front_porch;
};

struct hdmi_mode {
	uint32_t width;
	uint32_t height;
	uint32_t bpp;
	uint32_t refresh;
	uint32_t frequency;
	uint32_t flags;
	struct hdmi_timings timings;
	uint32_t vic;
};

struct monitor_data {
	uint32_t checksum;
	uint8_t manufacturer_code[4];
	uint8_t monitor[14];
	uint8_t serial_no[14];
	uint8_t ascii[14];
	uint32_t product_code;
	uint32_t serial_number;
	uint32_t production_week; /* -1 if not specified */
	uint32_t production_year; /* -1 if not specified */
	uint32_t model_year; /* -1 if not specified */
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t is_digital;
	uint32_t width_mm; /* -1 if not specified */
	uint32_t height_mm; /* -1 if not specified */
	uint64_t aspect_ratio; /* -1.0 if not specififed */
	uint64_t gamma; /* -1.0 if not specified */
	uint32_t standby;
	uint32_t suspend;
	uint32_t active_off;
	uint32_t srgb_is_standard;
	uint32_t preferred_timing_includes_native;
	uint32_t continuous_frequency;
	uint64_t red_x;
	uint64_t red_y;
	uint64_t green_x;
	uint64_t green_y;
	uint64_t blue_x;
	uint64_t blue_y;
	uint64_t white_x;
	uint64_t white_y;
	struct timing established[24]; /* Terminated by 0x0x0 */
	struct timing standard[8];
	struct hdmi_mode modes[30];
	uint32_t n_modes;
	bool hf_vsdb_present;
	uint32_t quirks;
};

struct edid {
	size_t len;
	uint8_t buf[0];
};

tegrabl_error_t tegrabl_edid_get_mode(struct nvdisp_mode *modes,
									  uint32_t module, uint32_t instance);
/**
 * @brief Func to distinguish between hdmi and dvi panels.
 *
 * @return True if panel is HDMI, false if DVI.
 */
bool tegrabl_edid_is_panel_hdmi(void);

#endif
