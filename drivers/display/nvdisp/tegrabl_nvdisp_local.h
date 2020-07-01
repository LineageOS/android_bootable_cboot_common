/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_NVDISP_LOCAL_H
#define TEGRABL_NVDISP_LOCAL_H

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <stdint.h>
#include <tegrabl_surface.h>
#include <tegrabl_dmamap.h>

#define N_WINDOWS	6

struct nvdisp_pdata {
	uint32_t test;
};

/* bits for nvdisp_out.flags */
#define NVDISP_OUT_HOTPLUG_HIGH				(0 << 1)
#define NVDISP_OUT_HOTPLUG_LOW				(1 << 1)
#define NVDISP_OUT_HOTPLUG_MASK				(1 << 1)
#define NVDISP_OUT_NVHDCP_POLICY_ALWAYS_ON	(0 << 2)
#define NVDISP_OUT_NVHDCP_POLICY_ON_DEMAND	(1 << 2)
#define NVDISP_OUT_NVHDCP_POLICY_MASK		(1 << 2)
#define NVDISP_OUT_CONTINUOUS_MODE			(0 << 3)
#define NVDISP_OUT_ONE_SHOT_MODE			(1 << 3)
#define NVDISP_OUT_N_SHOT_MODE				(1 << 4)
#define NVDISP_OUT_ONE_SHOT_LP_MODE			(1 << 5)
#define NVDISP_OUT_INITIALIZED_MODE			(1 << 6)

/* window flags */
#define WIN_FLAG_ENABLED		(1 << 0)
#define WIN_FLAG_BLEND_PREMULT	(1 << 1)
#define WIN_FLAG_BLEND_COVERAGE	(1 << 2)
#define WIN_FLAG_INVERT_H		(1 << 3)
#define WIN_FLAG_INVERT_V		(1 << 4)
#define WIN_FLAG_TILED			(1 << 5)
#define WIN_FLAG_H_FILTER		(1 << 6)
#define WIN_FLAG_V_FILTER		(1 << 7)
#define WIN_FLAG_BLOCKLINEAR	(1 << 8)
#define WIN_FLAG_SCAN_COLUMN	(1 << 9)
#define WIN_FLAG_INTERLACE		(1 << 10)
#define WIN_FLAG_FB				(1 << 11)
#define WIN_FLAG_INVALID		(1 << 31) /* window does not exist. */

#define nvdisp_readl(nvdisp, reg) \
	NV_READ32(nvdisp->base_addr + ((DC_##reg##_0) * 4))

#define nvdisp_writel(nvdisp, reg, value) \
	NV_WRITE32(nvdisp->base_addr + ((DC_##reg##_0) * 4), value)


#define NVDISP_PITCH_UNIT 64

enum {
	NVDISP_OUT_PIN_DATA_ENABLE,
	NVDISP_OUT_PIN_H_SYNC,
	NVDISP_OUT_PIN_V_SYNC,
	NVDISP_OUT_PIN_PIXEL_CLOCK,
};

enum {
	NVDISP_OUT_PIN_POL_LOW,
	NVDISP_OUT_PIN_POL_HIGH,
};

enum {
	NVDISP_UNDEFINED_DITHER,
	NVDISP_DISABLE_DITHER,
	NVDISP_ORDERED_DITHER,
	NVDISP_ERRDIFF_DITHER,
	NVDISP_TEMPORAL_DITHER,
};

struct dsi_panel {
	uint32_t panel_id;
	char panel_name[32];
};

struct nvdisp_lut {
	uint64_t *rgb;
	dma_addr_t phy_addr;
	size_t size;
};

struct nvdisp_mode {
	uint32_t pclk;
	uint32_t rated_pclk;
	uint32_t h_ref_to_sync;
	uint32_t v_ref_to_sync;
	uint32_t h_sync_width;
	uint32_t v_sync_width;
	uint32_t h_back_porch;
	uint32_t v_back_porch;
	uint32_t h_active;
	uint32_t v_active;
	uint32_t h_front_porch;
	uint32_t v_front_porch;
	uint32_t flags;
	uint8_t avi_m;
	uint32_t vic;
};

struct nvdisp_out_pin {
	uint32_t name;
	uint32_t pol;
};

/* color palette lookup table */
struct nvdisp_cp {
	uint64_t rgb[256];
};

struct nvdisp_csc {
	uint32_t blue2green;
	uint32_t red2green;
	uint32_t green2green;
	uint32_t const2green;
	uint32_t blue2red;
	uint32_t green2red;
	uint32_t red2red;
	uint32_t const2red;
	uint32_t green2blue;
	uint32_t red2blue;
	uint32_t blue2blue;
	uint32_t const2blue;
};

struct nvdisp_cmu {
	uint64_t rgb[1025];
};

struct nvdisp_win {
	uint32_t flags;
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
	uint32_t out_h;
	struct nvdisp_csc *csc;
	struct nvdisp_cp *cp;
	struct tegrabl_surface *surf;
	uintptr_t buf;
	uint32_t pitch;
};

extern struct nvdisp_out_ops hdmi_ops;
extern struct nvdisp_out_ops dp_ops;

#endif
