/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <tegrabl_edid.h>
#include <tegrabl_modes.h>
#include <tegrabl_hdmi.h>

struct hdmi_mode s_640_480_1 = {
	640,  /* width */
	480,  /* height */
	24,  /* bpp */
	0,  /* refresh */
	25174825,  /* frequency in hz */
	NVDISP_MODE_AVI_M_4_3,  /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		96,  /* h_sync_width */
		2,  /* v_sync_width */
		48,  /* h_back_porch */
		33,  /* v_back_porch */
		640,  /* h_disp_active */
		480,  /* v_disp_active */
		16,  /* h_front_porch */
		10,  /* v_front_porch */
	},
	1, /* VIC */
};

struct hdmi_mode s_720_480_2 = {
	720,  /* width */
	480,  /* height */
	24,  /* bpp */
	0,  /* refresh */
	26973026,  /* frequency in hz */
	NVDISP_MODE_AVI_M_4_3,  /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		62,  /* h_sync_width */
		6,  /* v_sync_width */
		60,  /* h_back_porch */
		30,  /* v_back_porch */
		720,  /* h_disp_active */
		480,  /* v_disp_active */
		16,  /* h_front_porch */
		9,  /* v_front_porch */
	},
	2, /* VIC */
};

struct hdmi_mode s_1280_720_4 = {
	1280,  /* width */
	720,  /* height */
	24,  /* bpp */
	0,  /* refresh */
	74175824,  /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9,  /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		40,  /* h_sync_width */
		5,  /* v_sync_width */
		220,  /* h_back_porch */
		20,  /* v_back_porch */
		1280,  /* h_disp_active */
		720,  /* v_disp_active */
		110,  /* h_front_porch */
		5,  /* v_front_porch */
	},
	4, /* VIC */
};

struct hdmi_mode s_1920_1080_16 = {
	1920,  /* width */
	1080,  /* height */
	24,  /* bpp */
	0,  /* refresh */
	148351648,  /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9,  /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		44,  /* h_sync_width */
		5,  /* v_sync_width */
		148,  /* h_back_porch */
		36,  /* v_back_porch */
		1920,  /* h_disp_active */
		1080,  /* v_disp_active */
		88,  /* h_front_porch */
		4,  /* v_front_porch */
	},
	16, /* VIC */
};

struct hdmi_mode s_720_576_17 = {
	720,  /* width */
	576,  /* height */
	24,  /* bpp */
	(50 << 16),  /* refresh */
	26973026,  /* frequency in hz */
	NVDISP_MODE_AVI_M_4_3,  /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		64,  /* h_sync_width */
		5,  /* v_sync_width */
		68,  /* h_back_porch */
		39,  /* v_back_porch */
		720,  /* h_disp_active */
		576,  /* v_disp_active */
		12,  /* h_front_porch */
		5,  /* v_front_porch */
	},
	17, /* VIC */
};

struct hdmi_mode s_1280_720_19 = {
	1280,  /* width */
	720,  /* height */
	24,  /* bpp */
	(50 << 16),  /* refresh */
	74175824,  /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9,  /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		40,  /* h_sync_width */
		5,  /* v_sync_width */
		220,  /* h_back_porch */
		20,  /* v_back_porch */
		1280,  /* h_disp_active */
		720,  /* v_disp_active */
		440,  /* h_front_porch */
		5,  /* v_front_porch */
	},
	19, /* VIC */
};

struct hdmi_mode s_1920_1080_31 = {
	1920,  /* width */
	1080,  /* height */
	24,  /* bpp */
	(50 << 16),  /* refresh */
	148351648,  /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9,  /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		44,  /* h_sync_width */
		5,  /* v_sync_width */
		148,  /* h_back_porch */
		36,  /* v_back_porch */
		1920,  /* h_disp_active */
		1080,  /* v_disp_active */
		528,  /* h_front_porch */
		4,  /* v_front_porch */
	},
	31, /* VIC */
};

struct hdmi_mode s_1920_1080_32 = {
	1920,  /* width */
	1080,  /* height */
	24,  /* bpp */
	(24 << 16),  /* refresh */
	74175824,  /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9,  /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		44,  /* h_sync_width */
		5,  /* v_sync_width */
		148,  /* h_back_porch */
		36,  /* v_back_porch */
		1920,  /* h_disp_active */
		1080,  /* v_disp_active */
		638,  /* h_front_porch */
		4,  /* v_front_porch */
	},
	32, /* VIC */
};

struct hdmi_mode s_1920_1080_33 = {
	1920,  /* width */
	1080,  /* height */
	24,  /* bpp */
	(25 << 16),  /* refresh */
	74175824,  /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9,  /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		44,  /* h_sync_width */
		5,  /* v_sync_width */
		148,  /* h_back_porch */
		36,  /* v_back_porch */
		1920,  /* h_disp_active */
		1080,  /* v_disp_active */
		528,  /* h_front_porch */
		4,  /* v_front_porch */
	},
	33, /* VIC */
};

struct hdmi_mode s_1920_1080_34 = {
	1920,  /* width */
	1080,  /* height */
	24,  /* bpp */
	(30 << 16),  /* refresh */
	74175824,  /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9,  /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		44,  /* h_sync_width */
		5,  /* v_sync_width */
		148,  /* h_back_porch */
		36,  /* v_back_porch */
		1920,  /* h_disp_active */
		1080,  /* v_disp_active */
		88,  /* h_front_porch */
		4,  /* v_front_porch */
	},
	34, /* VIC */
};

struct hdmi_mode s_3840_2160_95 = {
	3840, /* width */
	2160, /* height */
	24, /* bpp */
	(30 << 16), /* refresh */
	296703296, /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9, /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		88,  /* h_sync_width */
		10,  /* v_sync_width */
		296,  /* h_back_porch */
		72,  /* v_back_porch */
		3840,  /* h_disp_active */
		2160,  /* v_disp_active */
		176,  /* h_front_porch */
		8,  /* v_front_porch */
	},
	95, /* VIC */
};

struct hdmi_mode s_3840_2160_97 = {
	3840, /* width */
	2160, /* height */
	24, /* bpp */
	(60 << 16), /* refresh */
	593406593, /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9, /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		88,  /* h_sync_width */
		10,  /* v_sync_width */
		296,  /* h_back_porch */
		72,  /* v_back_porch */
		3840,  /* h_disp_active */
		2160,  /* v_disp_active */
		176,  /* h_front_porch */
		8,  /* v_front_porch */
	},
	97, /* VIC */
};

/* HDMI_VIC 0x01: 3840x2160p @ 29.97/30Hz */
struct hdmi_mode s_hdmi_vic_1 = {
	3840, /* width */
	2160, /* height */
	24, /* bpp */
	(30 << 16), /* refresh */
	296703296, /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9, /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		88,  /* h_sync_width */
		10,  /* v_sync_width */
		296,  /* h_back_porch */
		72,  /* v_back_porch */
		3840,  /* h_disp_active */
		2160,  /* v_disp_active */
		176,  /* h_front_porch */
		8,  /* v_front_porch */
	},
	0, /* VIC */
};

/* HDMI_VIC 0x02: 3840x2160p @ 25Hz */
struct hdmi_mode s_hdmi_vic_2 = {
	3840, /* width */
	2160, /* height */
	24, /* bpp */
	(25 << 16), /* refresh */
	296703296, /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9, /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		88,  /* h_sync_width */
		10,  /* v_sync_width */
		296,  /* h_back_porch */
		72,  /* v_back_porch */
		3840,  /* h_disp_active */
		2160,  /* v_disp_active */
		1056,  /* h_front_porch */
		8,  /* v_front_porch */
	},
	0, /* VIC */
};

/* HDMI_VIC 0x03: 3840x2160p @ 23.98/24Hz */
struct hdmi_mode s_hdmi_vic_3 = {
	3840, /* width */
	2160, /* height */
	24, /* bpp */
	(24 << 16), /* refresh */
	296703296, /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9, /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		88,  /* h_sync_width */
		10,  /* v_sync_width */
		296,  /* h_back_porch */
		72,  /* v_back_porch */
		3840,  /* h_disp_active */
		2160,  /* v_disp_active */
		1276,  /* h_front_porch */
		8,  /* v_front_porch */
	},
	0, /* VIC */
};

/* HDMI_VIC 0x04: 4096x2160p @ 24Hz */
struct hdmi_mode s_hdmi_vic_4 = {
	4096, /* width */
	2160, /* height */
	24, /* bpp */
	(24 << 16), /* refresh */
	296703296, /* frequency in hz */
	NVDISP_MODE_AVI_M_16_9, /* flags */
	{
		1,  /* h_ref_to_sync */
		1,  /* v_ref_to_sync */
		88,  /* h_sync_width */
		10,  /* v_sync_width */
		296,  /* h_back_porch */
		72,  /* v_back_porch */
		4096,  /* h_disp_active */
		2160,  /* v_disp_active */
		1020,  /* h_front_porch */
		8,  /* v_front_porch */
	},
	0, /* VIC */
};

struct hdmi_mode *s_hdmi_modes[] = {
	&s_640_480_1,
	&s_720_480_2,
	&s_1280_720_4,
	&s_1920_1080_16,
	&s_720_576_17,
	&s_1280_720_19,
	&s_1920_1080_31,
	&s_1920_1080_32,
	&s_1920_1080_33,
	&s_1920_1080_34,
#if defined(CONFIG_ENABLE_HDMI_4K_60)
	&s_3840_2160_97,
#endif
	/* Removing 4k@30 & 4k@60 modes from supported modes list since its not
	 * included in POR
	 */
	/*
	&s_3840_2160_95,
	&s_hdmi_vic_1,
	&s_hdmi_vic_2,
	&s_hdmi_vic_3,
	&s_hdmi_vic_4,
	*/
};

uint32_t size_s_hdmi_modes = ARRAY_SIZE(s_hdmi_modes);
