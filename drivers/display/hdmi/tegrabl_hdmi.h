/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TEGRABL_HDMI_H
#define __TEGRABL_HDMI_H

#include <tegrabl_nvdisp.h>
#include <tegrabl_display_dtb.h>

enum {
	NVDISP_MODE_AVI_M_NO_DATA,
	NVDISP_MODE_AVI_M_4_3,
	NVDISP_MODE_AVI_M_16_9,
	NVDISP_MODE_AVI_M_64_27, /* dummy, no avi m support */
	NVDISP_MODE_AVI_M_256_135, /* dummy, no avi m support */
};

#define HDMI_LICENSING_LLC_OUI	(0x000c03)

#define NV_SOR_REFCLK_DIV_INT(x)	(((x) & 0xff) << 8)
#define NV_SOR_REFCLK_DIV_FRAC(x)	(((x) & 0x3) << 6)
#define NV_SOR_HDMI_CTRL_REKEY_DEFAULT	58
#define NV_SOR_HDMI_CTRL_REKEY(x)	(((x) & 0x7f) << 0)
#define NV_SOR_HDMI_CTRL_MAX_AC_PACKET(x)	(((x) & 0x1f) << 16)

/* SCDC block */
#define HDMI_SCDC_TMDS_CONFIG_OFFSET	0x20
#define HDMI_SCDC_TMDS_CONFIG_SCRAMBLING_EN	1
#define HDMI_SCDC_TMDS_CONFIG_SCRAMBLING_DIS	1
#define HDMI_SCDC_TMDS_CONFIG_BIT_CLK_RATIO_10	(0 << 1)
#define HDMI_SCDC_TMDS_CONFIG_BIT_CLK_RATIO_40	(1 << 1)

#define NV_SOR_HDMI_INFOFRAME_HEADER_TYPE(x)	((x) & 0xff)
#define NV_SOR_HDMI_INFOFRAME_HEADER_VERSION(x)	(((x) & 0xff) << 8)
#define NV_SOR_HDMI_INFOFRAME_HEADER_LEN(x)	(((x) & 0xf) << 16)

enum {
	HDMI_VENDOR_VIDEO_FORMAT_NONE,
	HDMI_VENDOR_VIDEO_FORMAT_EXTENDED,
	HDMI_VENDOR_VIDEO_FORMAT_3D,
};

/* excluding checksum and header bytes */
enum {
	HDMI_INFOFRAME_LEN_VENDOR, /* vendor specific */
	HDMI_INFOFRAME_LEN_AVI = 13,
	HDMI_INFOFRAME_LEN_SPD = 25,
	HDMI_INFOFRAME_LEN_AUDIO = 10,
	HDMI_INFOFRAME_LEN_MPEG_SRC = 10,
};

enum {
	HDMI_INFOFRAME_VS_VENDOR = 0x1,
	HDMI_INFOFRAME_VS_AVI = 0x2,
	HDMI_INFOFRAME_VS_SPD = 0x1,
	HDMI_INFOFRAME_VS_AUDIO = 0x1,
	HDMI_INFOFRAME_VS_MPEG_SRC = 0x1,
};

enum {
	HDMI_INFOFRAME_TYPE_VENDOR = 0x81,
	HDMI_INFOFRAME_TYPE_AVI = 0x82,
	HDMI_INFOFRAME_TYPE_SPD = 0x83,
	HDMI_INFOFRAME_TYPE_AUDIO = 0x84,
	HDMI_INFOFRAME_TYPE_MPEG_SRC = 0x85,
};

enum {
	HDMI_AVI_ASPECT_RATIO_NO_DATA = 0x0,
	HDMI_AVI_ASPECT_RATIO_4_3 = 0x1,
	HDMI_AVI_ASPECT_RATIO_16_9 = 0x2,
};

enum {
	HDMI_AVI_SCAN_NO_INFO = 0x0,
	HDMI_AVI_OVERSCAN = 0x1,
	HDMI_AVI_UNDERSCAN = 0x2,
};

enum {
	HDMI_AVI_BAR_INVALID = 0x0,
	HDMI_AVI_VERT_BAR_VALID = 0x1,
	HDMI_AVI_HOR_BAR_VALID = 0x2,
	HDMI_AVI_VERT_HOR_BAR_VALID = 0x3,
};

enum {
	HDMI_AVI_ACTIVE_FORMAT_INVALID = 0x0,
	HDMI_AVI_ACTIVE_FORMAT_VALID = 0x1,
};

enum {
	HDMI_AVI_RGB = 0x0,
	HDMI_AVI_YCC_422 = 0x1,
	HDMI_AVI_YCC_444 = 0x2,
	HDMI_AVI_YCC_420 = 0x3,
};

enum {
	HDMI_AVI_ACTIVE_FORMAT_SAME = 0x8,
	HDMI_AVI_ACTIVE_FORMAT_4_3_CENTER = 0x9,
	HDMI_AVI_ACTIVE_FORMAT_16_9_CENTER = 0xa,
	HDMI_AVI_ACTIVE_FORMAT_14_9_CENTER = 0xb,
};

enum {
	HDMI_AVI_COLORIMETRY_DEFAULT = 0x0,
	HDMI_AVI_COLORIMETRY_SMPTE170M_ITU601 = 0x1,
	HDMI_AVI_COLORIMETRY_ITU709 = 0x2,
	HDMI_AVI_COLORIMETRY_EXTENDED_VALID = 0x3,
};

enum {
	HDMI_AVI_SCALING_UNKNOWN = 0x0,
	HDMI_AVI_SCALING_HOR = 0x1,
	HDMI_AVI_SCALING_VERT = 0x2,
	HDMI_AVI_SCALING_VERT_HOR = 0x3,
};

enum {
	HDMI_AVI_RGB_QUANT_DEFAULT = 0x0,
	HDMI_AVI_RGB_QUANT_LIMITED = 0x1,
	HDMI_AVI_RGB_QUANT_FULL = 0x2,
};

enum {
	HDMI_AVI_EXT_COLORIMETRY_INVALID = 0x0,
	HDMI_AVI_EXT_COLORIMETRY_xvYCC601 = 0x0,
	HDMI_AVI_EXT_COLORIMETRY_xvYCC709 = 0x1,
};

enum {
	HDMI_AVI_IT_CONTENT_FALSE = 0x0,
	HDMI_AVI_IT_CONTENT_TRUE = 0x0,
};

enum {
	HDMI_AVI_NO_PIX_REPEAT = 0x0,
};

enum {
	HDMI_AVI_IT_CONTENT_NONE = 0x0,
	HDMI_AVI_IT_CONTENT_GRAPHICS = 0x0,
	HDMI_AVI_IT_CONTENT_PHOTO = 0x1,
	HDMI_AVI_IT_CONTENT_CINEMA = 0x2,
	HDMI_AVI_IT_CONTENT_GAME = 0x3,
};

enum {
	HDMI_AVI_YCC_QUANT_NONE = 0x0,
	HDMI_AVI_YCC_QUANT_LIMITED = 0x0,
	HDMI_AVI_YCC_QUANT_FULL = 0x1,
};

/* all fields little endian */
struct avi_infoframe {
	/* PB0 */
	uint32_t csum:8;	/* checksum */

	/* PB1 */
	uint32_t scan:2;	/* scan information */
	uint32_t bar_valid:2;	/* bar info data valid */
	uint32_t act_fmt_valid:1;	/* active info present */
	uint32_t rgb_ycc:2;	/* RGB or YCbCr */
	uint32_t res1:1;	/* reserved */

	/* PB2 */
	uint32_t act_format:4;	/* active format aspect ratio */
	uint32_t aspect_ratio:2;	/* picture aspect ratio */
	uint32_t colorimetry:2;	/* colorimetry */

	/* PB3 */
	uint32_t scaling:2;	/* non-uniform picture scaling */
	uint32_t rgb_quant:2;	/* rgb quantization range */
	uint32_t ext_colorimetry:3;	/* extended colorimetry */
	uint32_t it_content:1;	/* it content */

	/* PB4 */
	uint32_t video_format:7; /* video format id code */
	uint32_t res4:1;	/* reserved */

	/* PB5 */
	uint32_t pix_rep:4;	/* pixel repetition factor */
	uint32_t it_content_type:2;	/* it content type */
	uint32_t ycc_quant:2;	/* YCbCr quantization range */

	/* PB6-7 */
	uint32_t top_bar_end_line_low_byte:8;
	uint32_t reg_hole1:8;
	uint32_t top_bar_end_line_high_byte:8;

	/* PB8-9 */
	uint32_t bot_bar_start_line_low_byte:8;
	uint32_t bot_bar_start_line_high_byte:8;

	/* PB10-11 */
	uint32_t left_bar_end_pixel_low_byte:8;
	uint32_t left_bar_end_pixel_high_byte:8;

	/* PB12-13 */
	uint32_t right_bar_start_pixel_low_byte:8;
	uint32_t right_bar_start_pixel_high_byte:8;

	uint32_t reg_hole2:8;
};/* __PACKED*//*check_later*/

/* all fields little endian */
struct hdmi_vendor_infoframe {
	/* PB0 */
	uint32_t csum:8;

	/* PB1, PB2, PB3 */
	uint32_t oui:24; /* organizationally unique identifier */

	/* PB4 */
	uint32_t res1:5;
	uint32_t video_format:3;

	/* PB5 */
	uint32_t extended_vic:8;
	uint32_t res2:4;
	uint32_t format_3d:4;

	/* PB6 */
	uint32_t res3:4;
	uint32_t ext_data_3d:4;
};/* __PACKED*//*check_later*/

struct hdmi {
	struct tegrabl_nvdisp *nvdisp;
	struct tegrabl_display_hdmi_dtb *hdmi_dtb;
	struct sor_data *sor;
	struct avi_infoframe avi;
	struct hdmi_vendor_infoframe vsi;
	int32_t sor_instance;
	bool enabled;
	bool is_panel_hdmi; /*true if hdmi, false if dvi*/
};

#endif
