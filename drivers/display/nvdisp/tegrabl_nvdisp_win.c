/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_NVDISP

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <stdint.h>
#include <string.h>
#include <ardisplay.h>
#include <ardisplay_a.h>
#include <tegrabl_io.h>
#include <tegrabl_drf.h>
#include <tegrabl_nvdisp.h>
#include <tegrabl_nvdisp_win.h>

static struct nvdisp_csc default_csc = {
	.blue2green = 0x00000,
	.red2green = 0x00000,
	.green2green = 0x10000,
	.const2green = 0x00000,
	.blue2red = 0x00000,
	.green2red = 0x00000,
	.red2red = 0x10000,
	.const2red = 0x00000,
	.green2blue = 0x00000,
	.red2blue = 0x00000,
	.blue2blue = 0x10000,
	.const2blue = 0x00000
};

void nvdisp_win_select(struct tegrabl_nvdisp *nvdisp, uint32_t win_id)
{
	uint32_t val = 0;

	val = NV_DRF_DEF(DC, CMD_DISPLAY_WINDOW_HEADER, WINDOW_A_SELECT, ENABLE);
	val = val << win_id;
	pr_debug("win header = %x\n", val);
	nvdisp_writel(nvdisp, CMD_DISPLAY_WINDOW_HEADER, val);
}

void nvdisp_win_set_buf(struct tegrabl_nvdisp *nvdisp, uint32_t win_id,
						uintptr_t buf)
{
	struct nvdisp_win *win;
	uint32_t val;
	uint64_t addr;

	pr_debug("%s: entry\n", __func__);
	win = nvdisp_win_get(nvdisp, win_id);
	win->buf = buf;

	addr = (uint64_t) buf;
	val = (uint32_t) addr;
	nvdisp_writel(nvdisp, WINBUF_A_START_ADDR, val);

	val = (uint32_t) (addr >> 32);
	nvdisp_writel(nvdisp, WINBUF_A_START_ADDR_HI, val);
}

void nvdisp_win_list(uint32_t *count)
{
	*count = N_WINDOWS;
}

struct nvdisp_win *nvdisp_win_get(struct tegrabl_nvdisp *nvdisp,
								  uint32_t win_id)
{
	return &nvdisp->windows[win_id];
}

void nvdisp_win_set_rotation(struct tegrabl_nvdisp *nvdisp, uint32_t win_id,
							 uint32_t angle)
{
	uint32_t val;

	pr_debug("%s: entry\n", __func__);
	nvdisp_win_select(nvdisp, win_id);

	val = nvdisp_readl(nvdisp, WIN_A_WIN_OPTIONS);

	switch (angle) {
	case 0:
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_SCAN_COLUMN,
								 DISABLE, val);
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_H_DIRECTION,
								 INCREMENT, val);
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_V_DIRECTION,
								 INCREMENT, val);
		break;
	case 90:
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_SCAN_COLUMN,
								 ENABLE, val);
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_H_DIRECTION,
								 INCREMENT, val);
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_V_DIRECTION,
								 DECREMENT, val);
		break;
	case 180:
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_SCAN_COLUMN,
								 DISABLE, val);
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_H_DIRECTION,
								 DECREMENT, val);
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_V_DIRECTION,
								 DECREMENT, val);
		break;
	case 270:
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_SCAN_COLUMN,
								 ENABLE, val);
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_H_DIRECTION,
								 DECREMENT, val);
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_V_DIRECTION,
								 INCREMENT, val);
		break;
	default:
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_SCAN_COLUMN,
								 DISABLE, val);
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_H_DIRECTION,
								 INCREMENT, val);
		val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_V_DIRECTION,
								 INCREMENT, val);
		break;
	}

	nvdisp_writel(nvdisp, WIN_A_WIN_OPTIONS, val);
	pr_debug("%s: exit\n", __func__);
}

void nvdisp_win_set_owner(struct tegrabl_nvdisp *nvdisp, uint32_t win_id)
{
	uint32_t val;

	nvdisp_win_select(nvdisp, win_id);

	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CONTROL, A_OWNER,
					 nvdisp->instance);

	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CONTROL, val);
}

void nvdisp_win_config(struct tegrabl_nvdisp *nvdisp, uint32_t win_id)
{
	uint32_t val;
	struct nvdisp_win *win;

	pr_debug("%s: entry\n", __func__);

	win = nvdisp_win_get(nvdisp, win_id);

	nvdisp_win_select(nvdisp, win_id);

	pr_debug("win -> h = %d\n", win->out_h);
	val = NV_DRF_NUM(DC, WIN_A_POSITION, A_H_POSITION, 0) |
		NV_DRF_NUM(DC, WIN_A_POSITION, A_V_POSITION, 0);
	nvdisp_writel(nvdisp, WIN_A_POSITION, val);

	val = NV_DRF_NUM(DC, WIN_A_SIZE, A_H_SIZE, win->w) |
		NV_DRF_NUM(DC, WIN_A_SIZE, A_V_SIZE, win->h);
	nvdisp_writel(nvdisp, WIN_A_SIZE, val);

	if (nvdisp->color_format == PIXEL_FORMAT_A8B8G8R8)
		val = NV_DRF_NUM(DC, WIN_A_COLOR_DEPTH, A_COLOR_DEPTH, 13); /*R8G8B8A8*/
	else if (nvdisp->color_format == PIXEL_FORMAT_A8R8G8B8)
		val = NV_DRF_NUM(DC, WIN_A_COLOR_DEPTH, A_COLOR_DEPTH, 12); /*B8G8R8A8*/
	else
		val = NV_DRF_NUM(DC, WIN_A_COLOR_DEPTH, A_COLOR_DEPTH, 13); /*R8G8B8A8*/

	nvdisp_writel(nvdisp, WIN_A_COLOR_DEPTH, val);

	val = NV_DRF_NUM(DC, WIN_A_PCALC_WINDOW_SET_CROPPED_SIZE_IN, A_WIDTH,
					 win->w) |
		NV_DRF_NUM(DC, WIN_A_PCALC_WINDOW_SET_CROPPED_SIZE_IN, A_HEIGHT,
				   win->h);

	nvdisp_writel(nvdisp, WIN_A_PCALC_WINDOW_SET_CROPPED_SIZE_IN, val);

	val = NV_DRF_NUM(DC, WIN_A_WINDOW_SET_PLANAR_STORAGE, A_PITCH,
					 win->pitch / NVDISP_PITCH_UNIT);
	nvdisp_writel(nvdisp, WIN_A_WINDOW_SET_PLANAR_STORAGE, val);

	val = nvdisp_readl(nvdisp, WIN_A_WIN_OPTIONS);
	val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_WIN_ENABLE, ENABLE, val);
	nvdisp_writel(nvdisp, WIN_A_WIN_OPTIONS, val);

	pr_debug("%s: exit\n", __func__);
}

void nvdisp_win_csc_init_defaults(struct nvdisp_csc *csc)
{
	pr_debug("%s: entry\n", __func__);
	memcpy(csc, &default_csc, sizeof(struct nvdisp_csc));
	pr_debug("%s: exit\n", __func__);
}

void nvdisp_win_csc_set(struct tegrabl_nvdisp *nvdisp, uint32_t win_id,
						struct nvdisp_csc *csc)
{
	uint32_t val;
	struct nvdisp_win *win;

	pr_debug("%s: entry\n", __func__);

	win = nvdisp_win_get(nvdisp, win_id);
	win->csc = csc;
	val = NV_DRF_DEF(DC, CMD_DISPLAY_WINDOW_HEADER, WINDOW_A_SELECT, ENABLE);
	val = val << win_id;
	nvdisp_writel(nvdisp, CMD_DISPLAY_WINDOW_HEADER, val);

	/* 1 to 4 registers */
	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_RED2RED, A_COEFF,
					 csc->red2red);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_RED2RED, val);

	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_GREEN2RED, A_COEFF,
					 csc->green2red);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_GREEN2RED, val);

	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_BLUE2RED, A_COEFF,
					 csc->blue2red);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_BLUE2RED, val);

	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_CONSTANT2RED, A_COEFF,
					 csc->const2red);
	nvdisp_writel(nvdisp,  WIN_A_CORE_WINDOWGROUP_SET_CSC_CONSTANT2RED, val);

	/* 5 to 8 registers */
	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_RED2GREEN, A_COEFF,
					 csc->red2green);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_RED2GREEN, val);

	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_GREEN2GREEN, A_COEFF,
					 csc->green2green);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_GREEN2GREEN, val);

	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_BLUE2GREEN, A_COEFF,
					 csc->blue2green);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_BLUE2GREEN, val);

	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_CONSTANT2GREEN, A_COEFF,
					 csc->const2green);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_CONSTANT2GREEN, val);

	/* 9 to 12 registers */
	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_RED2BLUE, A_COEFF,
					 csc->red2blue);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_RED2BLUE, val);

	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_GREEN2BLUE, A_COEFF,
					 csc->green2blue);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_GREEN2BLUE, val);

	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_BLUE2BLUE, A_COEFF,
					 csc->blue2blue);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_BLUE2BLUE, val);

	val = NV_DRF_NUM(DC, WIN_A_CORE_WINDOWGROUP_SET_CSC_CONSTANT2BLUE, A_COEFF,
					 csc->const2blue);
	nvdisp_writel(nvdisp, WIN_A_CORE_WINDOWGROUP_SET_CSC_CONSTANT2BLUE, val);

	pr_debug("%s: exit\n", __func__);
}

void nvdisp_win_cp_init_defaults(struct nvdisp_cp *cp)
{
	uint64_t i;

	for (i = 0; i < 256; i++) {
		cp->rgb[i] = ((i << 40) | (i << 24) | (i << 8));
	}
}

void nvdisp_win_cp_set(struct tegrabl_nvdisp *nvdisp,
	uint32_t win_id, struct nvdisp_cp *cp)
{
	uint32_t val = 0;
	struct nvdisp_win *win;
	dma_addr_t cp_base_addr;

	win = nvdisp_win_get(nvdisp, win_id);
	win->cp = cp;
	val = NV_DRF_DEF(DC, CMD_DISPLAY_WINDOW_HEADER, WINDOW_A_SELECT, ENABLE);
	val = val << win_id;
	nvdisp_writel(nvdisp, CMD_DISPLAY_WINDOW_HEADER, val);

	cp_base_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_NVDISPLAY0_HEAD,
		nvdisp->instance, (void *)(cp), sizeof(struct nvdisp_cp),
		TEGRABL_DMA_TO_DEVICE);
	nvdisp_writel(nvdisp, WIN_A_COREPVT_WINDOWGROUP_SET_INPUT_LUT_BASE,
				  U64_TO_U32_LO(cp_base_addr));
	nvdisp_writel(nvdisp, WIN_A_COREPVT_WINDOWGROUP_SET_INPUT_LUT_BASE_HI,
				  U64_TO_U32_HI(cp_base_addr));

	val = nvdisp_readl(nvdisp, WIN_A_WIN_OPTIONS);
	val = NV_FLD_SET_DRF_DEF(DC, WIN_A_WIN_OPTIONS, A_CP_ENABLE, ENABLE, val);
	nvdisp_writel(nvdisp, WIN_A_WIN_OPTIONS, val);
}
