/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_DISPLAY

#include <stdint.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_surface.h>
#include <tegrabl_render_text.h>
#include <tegrabl_render_image.h>
#include <tegrabl_nvdisp.h>
#include <tegrabl_display_unit.h>
#include <tegrabl_display_dtb.h>

/* Number of bottom rows reserved for the CURSOR_END position */
static const uint32_t end_rows = 2;

struct tegrabl_surface *display_unit_get_surface(
	struct tegrabl_display_unit *du, uint32_t win_idx)
{
	return du->surf[win_idx];
}

tegrabl_error_t display_unit_set_surface(struct tegrabl_display_unit *du,
	uint32_t win_idx, struct tegrabl_surface *surf)
{
	dma_addr_t dma_addr;

	if ((du == NULL) || (du->nvdisp == NULL) || (surf == NULL))
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	pr_debug("%s: du instance %d, type %d, win_id %d, surf 0x%lx\n", __func__,
			 du->nvdisp->instance, du->type, du->win_id, (unsigned long)surf);

	du->surf[win_idx] = surf;

	dma_addr = tegrabl_dma_map_buffer(TEGRABL_MODULE_NVDISPLAY0_HEAD,
		du->nvdisp->instance, (void *)(surf->base), surf->size,
		TEGRABL_DMA_TO_DEVICE);

	tegrabl_nvdisp_win_set_surface(du->nvdisp, du->win_id, dma_addr);

	return TEGRABL_NO_ERROR;
}

struct tegrabl_display_unit *tegrabl_display_unit_init(
	enum tegrabl_display_unit_type type, struct tegrabl_display_pdata *pdata)
{
	struct tegrabl_surface *surf = NULL;
	uint32_t count;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_display_unit *du = NULL;
	struct tegrabl_nvdisp *nvdisp;
	uint32_t rotation = 0;

	/* initialize disp controller */
	pr_debug("%s: entry\n", __func__);

	du = tegrabl_malloc(sizeof(struct tegrabl_display_unit));
	if (du == NULL) {
		pr_debug("memory allocation failed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	du->type = type;
	du->win_id = pdata->win_id;

	pr_debug("%s: du type %d, win_id 0x%x\n", __func__, type, pdata->win_id);
	nvdisp = tegrabl_nvdisp_init(type, pdata);
	if (nvdisp == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
		goto fail;
	}

	tegrabl_nvdisp_list_windows(nvdisp, &count);

	/* setup and set surface */
	surf = tegrabl_malloc(sizeof(struct tegrabl_surface));
	if (surf == NULL) {
		pr_debug("memory allocation failed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}

	tegrabl_nvdisp_get_resolution(nvdisp, &(surf->height), &(surf->width));
	pr_debug("%s: surface height = %d, width = %d\n", __func__, surf->height,
			 surf->width);

	err = tegrabl_surface_setup(surf);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("%s: surface setup error\n", __func__);
		goto fail;
	}
	tegrabl_nvdisp_configure_window(nvdisp, du->win_id, surf);

	du->nvdisp = nvdisp;
	display_unit_set_surface(du, 0, surf);

	du->is_init_done = true;

	err = tegrabl_display_unit_clear(du);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	tegrabl_render_text_set_rotation_angle(rotation);

	return du;

fail:
	if (surf)
		tegrabl_free(surf);

	if (du)
		tegrabl_free(du);

	pr_debug("%s: exit\n", __func__);
	return NULL;
}

tegrabl_error_t tegrabl_display_unit_printf(struct tegrabl_display_unit *du,
	uint32_t color, const char *text)
{
	struct tegrabl_surface *surf;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_debug("%s: entry\n", __func__);

	if (du == NULL) {
		pr_error("display unit handle is null\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	/* get the surface, update it with text */
	surf = display_unit_get_surface(du, 0);
	if (surf == NULL) {
		pr_error("surface is not assigned\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	err = tegrabl_render_text(surf, text, color);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = display_unit_set_surface(du, 0, surf);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_debug("%s: exit\n", __func__);
	return err;

fail:
	TEGRABL_SET_HIGHEST_MODULE(err);
	pr_error("%s: exit error\n", __func__);
	return err;
}

tegrabl_error_t tegrabl_display_unit_show_image(struct tegrabl_display_unit *du,
	struct tegrabl_image_info *image_info)
{
	struct tegrabl_surface *surf;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (du == NULL || du->nvdisp == NULL) {
		pr_error("display unit or nvdisp handle is null\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}
	pr_debug("%s: du instance %d, type %d, win_id %d\n", __func__,
			 du->nvdisp->instance, du->type, du->win_id);

	/* get the surface, update it with text */
	surf = display_unit_get_surface(du, 0);
	if (surf == NULL) {
		pr_error("surface is not assigned\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	err = tegrabl_render_image(surf, image_info->image_buf, image_info->size,
							   image_info->format);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	err = display_unit_set_surface(du, 0, surf);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	pr_debug("%s: exit\n", __func__);
	return err;

fail:
	TEGRABL_SET_HIGHEST_MODULE(err);
	pr_error("%s: exit error\n", __func__);

	return err;
}

tegrabl_error_t tegrabl_display_unit_clear(struct tegrabl_display_unit *du)
{
	struct tegrabl_surface *surf;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (du == NULL || du->nvdisp == NULL) {
		pr_error("display unit or nvdisp handle is null\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}
	pr_debug("%s: du instance %d, type %d\n", __func__,
			 du->nvdisp->instance, du->type);

	/* get the surface, clear the contents, set text position to 0,0 */
	surf = display_unit_get_surface(du, 0);
	if (surf == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}
	tegrabl_surface_clear(surf);
	display_unit_set_surface(du, 0, surf);
	tegrabl_render_text_set_position(0, 0);
	tegrabl_render_text_set_font(FONT_DEFAULT, 2);

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: exit error\n", __func__);
	}
	return err;
}

tegrabl_error_t tegrabl_display_unit_ioctl(struct tegrabl_display_unit *du,
										   uint32_t ioctl, void *args)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t rotation_angle;
	struct text_font *font;
	struct tegrabl_display_unit_params *disp_params;
	struct tegrabl_surface *surf;
	struct tegrabl_nvdisp *nvdisp;
	uint32_t position;

	pr_debug("display console ioctl = %d\n", ioctl);

	if (du == NULL) {
		pr_error("display unit handle is null\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	switch (ioctl) {
	case DISPLAY_UNIT_IOCTL_SET_ROTATION:
		rotation_angle = *(uint32_t *)args;
		tegrabl_render_text_set_rotation_angle(rotation_angle);
		tegrabl_render_image_set_rotation_angle(rotation_angle);
		break;

	case DISPLAY_UNIT_IOCTL_SET_FONT:
		font = (struct text_font *)args;
		if (font == NULL) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			break;
		}
		tegrabl_render_text_set_font(font->type, font->size);
		break;

	case DISPLAY_UNIT_IOCTL_CONTROL_BACKLIGHT:
		/* TODO: Handle this while enabling DSI */

		/* is_on = *(bool *)args;
		tegrabl_panel_backlight_enable(is_on); */
		break;

	case DISPLAY_UNIT_IOCTL_GET_DISPLAY_PARAMS:
		disp_params = (struct tegrabl_display_unit_params *)args;
		surf = display_unit_get_surface(du, 0);
		if (!surf) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			break;
		}
		disp_params->size = surf->size;
		disp_params->height = surf->height;
		disp_params->width = surf->width;
		disp_params->addr = (uintptr_t)surf->base;

		nvdisp = du->nvdisp;
		disp_params->instance = nvdisp->instance;
		disp_params->lut_addr = nvdisp->cmu_base_addr;
		disp_params->lut_size = sizeof(struct nvdisp_cmu);

		disp_params->rotation_angle = du->rotation_angle;
		break;

	case DISPLAY_UNIT_IOCTL_SET_TEXT_POSITION:
		position = *(uint32_t *)args;
		struct text_font *font;
		font = tegrabl_render_text_get_font();

		surf = display_unit_get_surface(du, 0);
		if (position == CURSOR_START)
			tegrabl_render_text_set_position(0 , 0);
		else if (position == CURSOR_CENTER)
			tegrabl_render_text_set_position(0 , surf->height / 2);
		else if (position == CURSOR_END)
			tegrabl_render_text_set_position(0 , surf->height - end_rows *
											 font->height_scaled);
		else
			return TEGRABL_ERR_INVALID_CONFIG;
		break;

	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		break;
	}
fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: exit error\n", __func__);
	}
	return err;
}

tegrabl_error_t tegrabl_display_unit_shutdown(struct tegrabl_display_unit *du)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_nvdisp *nvdisp;

	if (du == NULL || du->nvdisp == NULL) {
		pr_error("du or nvdisp handle is null\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (du->is_init_done != true) {
		goto fail;
	}

	nvdisp = du->nvdisp;

	/* disable panel */
	if (nvdisp->out_ops && nvdisp->out_ops->disable) {
		nvdisp->out_ops->disable(nvdisp);
	}

	/* Power down panel power rails */
	/* TODO: Handle this while enabling DSI */
	/* tegrabl_panel_shutdown(); */

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s: exit error\n", __func__);
	}

	return err;
}

