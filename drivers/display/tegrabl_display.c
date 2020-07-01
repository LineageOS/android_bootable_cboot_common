/*
 * Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_DISPLAY

#include <tegrabl_debug.h>
#include <tegrabl_stdarg.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_nvdisp.h>
#include <tegrabl_display.h>
#include <tegrabl_render_text.h>
#include <tegrabl_render_image.h>
#include <tegrabl_display_unit.h>
#include <tegrabl_parse_bmp.h>
#include <tegrabl_display_dtb.h>
#include <tegrabl_timer.h>
#include <tegrabl_display_soc.h>

#define TEXT_SIZE   1024

struct tegrabl_display {
	struct tegrabl_display_unit *du[DISPLAY_OUT_MAX];
	uint32_t n_du;
};
static struct tegrabl_display *hdisplay;

tegrabl_error_t tegrabl_display_init(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t n_du = 0;
	struct tegrabl_display_list *du_list = NULL;

	if (hdisplay) {
		if (hdisplay->n_du > 0) {
			pr_debug("Display already initialized\n");
			goto fail;
		} else {
			tegrabl_free(hdisplay);
		}
	}

	hdisplay = tegrabl_malloc(sizeof(struct tegrabl_display));
	if (hdisplay == NULL) {
		pr_debug("memory allocation failed\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		goto fail;
	}
	hdisplay->n_du = 0;

	tegrabl_display_unpowergate();

	err = tegrabl_display_get_du_list(&du_list);
	if (err != TEGRABL_NO_ERROR || (du_list == NULL)) {
		goto fail;
	}

	while (du_list != NULL) {
		pr_debug("initialize du = %d, type = %d\n", n_du, du_list->du_type);
		hdisplay->du[n_du] = tegrabl_display_unit_init(du_list->du_type,
													   du_list->pdata);
		if (hdisplay->du[n_du] == NULL) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
			pr_error("%s: Initialize du %d failed\n", __func__, n_du);
		} else {
			n_du = ++hdisplay->n_du;
		}

		du_list = du_list->next;
	};

	/* TODO: Get orientation from accelerometer and then set */
	return err;
fail:
	if (hdisplay) {
		if (hdisplay->n_du == 0) {
			tegrabl_display_powergate();
			tegrabl_free(hdisplay);
			hdisplay = NULL;
		}
	}
	return err;
}

tegrabl_error_t tegrabl_display_printf(color_t color,
									   const char *format, ...)
{
	va_list ap;
	char text[TEXT_SIZE] = "";
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t du_idx = 0;

	if (!hdisplay) {
		pr_error("%s: display is not initialized\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
		goto fail;
	}

	/* prepare text */
	va_start(ap, format);
	tegrabl_vsnprintf(text, sizeof(text), format, ap);
	va_end(ap);

	for (du_idx = 0; du_idx < hdisplay->n_du; du_idx++) {
		err = tegrabl_display_unit_printf(hdisplay->du[du_idx], color, text);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s, du %d failed to print messages\n", __func__, du_idx);
			goto fail;
		}
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_display_show_image(struct tegrabl_image_info *image)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct tegrabl_display_unit_params *disp_param = NULL;
	struct tegrabl_bmp_image bmp_img = {0};
	uint32_t du_idx = 0;

	if (!hdisplay) {
		pr_error("%s: display is not initialized\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 1);
		goto fail;
	}

	bmp_img.img_type = image->type;
	bmp_img.is_panel_portrait = false; /*TODO: READ from DTB*/
	bmp_img.rotation_angle = 0; /*TODO: from accelerometer or READ from DTB*/

	pr_debug("%s: Show image for %d display(s)\n", __func__, hdisplay->n_du);

	disp_param = tegrabl_malloc(sizeof(struct tegrabl_display_unit_params));
	if (!disp_param) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		goto fail;
	}

	for (du_idx = 0; du_idx < hdisplay->n_du; du_idx++) {
		pr_debug("%s: Processing du %d, type %d\n", __func__,
				 du_idx, hdisplay->du[du_idx]->type);

		if (image->type == IMAGE_USER_DEFINED) /*skip parsing bmp blob*/ {
			goto display_image;
		}

		err = tegrabl_display_unit_ioctl(hdisplay->du[du_idx],
				DISPLAY_UNIT_IOCTL_GET_DISPLAY_PARAMS, (void *)disp_param);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s: failed to process du %d\n", __func__, du_idx);
			goto fail;
		}
		pr_debug("%s: Get disp_params for instance %d\n", __func__,
				 disp_param->instance);

		if (!bmp_img.is_panel_portrait)
			bmp_img.panel_resolution = disp_param->height;
		else
			bmp_img.panel_resolution = disp_param->width;
		pr_debug("%s, du %d panel resolution = %d\n", __func__, du_idx,
				 bmp_img.panel_resolution);

		if (image->format == TEGRABL_IMAGE_FORMAT_BMP) {
			err = tegrabl_get_bmp(&bmp_img);
			if (err != TEGRABL_NO_ERROR) {
				pr_error("%s, du %d failed to read bmp from blob\n",
						 __func__, du_idx);
				goto fail;
			}
			image->image_buf = bmp_img.bmp;
			image->size = bmp_img.image_size;
		}
		/*support for JPEG can be added in else*/

display_image:
		err = tegrabl_display_unit_show_image(hdisplay->du[du_idx], image);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s, du %d failed to show image\n", __func__, du_idx);
			goto fail;
		}
	}

fail:
	if (disp_param)
		tegrabl_free(disp_param);

	return err;
}

tegrabl_error_t tegrabl_display_clear(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t du_idx = 0;

	if (!hdisplay) {
		pr_error("%s: display is not initialized\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 2);
		goto fail;
	}

	pr_debug("%s: Clear screen for %d display(s)\n", __func__, hdisplay->n_du);
	for (du_idx = 0; du_idx < hdisplay->n_du; du_idx++) {
		pr_debug("%s: shutdown du%d, type %d\n", __func__, du_idx,
				 hdisplay->du[du_idx]->type);
		err = tegrabl_display_unit_clear(hdisplay->du[du_idx]);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s, du %d failed to clear display unit\n", __func__,
					 du_idx);
			goto fail;
		}
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_display_text_set_cursor(cursor_position_t position)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t du_idx = 0;

	if (!hdisplay) {
		pr_error("%s: display is not initialized\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 3);
		goto fail;
	}

	for (du_idx = 0; du_idx < hdisplay->n_du; du_idx++) {
		err =  tegrabl_display_unit_ioctl(hdisplay->du[du_idx],
			DISPLAY_UNIT_IOCTL_SET_TEXT_POSITION, (void **)&position);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s, du %d failed to set text position\n", __func__,
					 du_idx);
			goto fail;
		}
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_display_shutdown(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t du_idx = 0;

	if (!hdisplay) {
		pr_error("%s: display is not initialized\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 4);
		goto fail;
	}

	for (du_idx = 0; du_idx < hdisplay->n_du; du_idx++) {
		pr_debug("%s: shutdown du%d, type %d\n", __func__, du_idx,
				 hdisplay->du[du_idx]->type);
		err = tegrabl_display_unit_shutdown(hdisplay->du[du_idx]);
		if (err != TEGRABL_NO_ERROR) {
			pr_error("%s, du %d failed to shutdown display unit\n", __func__,
					 du_idx);
			goto fail;
		}
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_display_get_params(
	uint32_t du_idx, struct tegrabl_display_unit_params *disp_param)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (!hdisplay || du_idx >= hdisplay->n_du) {
		pr_debug("%s: display or du %d is not initialized\n", __func__, du_idx);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 5);
		goto fail;
	}
	pr_debug("%s: Get disp_params of du %d from %d display(s)\n", __func__,
			 du_idx, hdisplay->n_du);

	err = tegrabl_display_unit_ioctl(hdisplay->du[du_idx],
			DISPLAY_UNIT_IOCTL_GET_DISPLAY_PARAMS, (void *)disp_param);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("%s, du %d failed to get display params\n", __func__, du_idx);
		goto fail;
	}

fail:
	return err;
}
