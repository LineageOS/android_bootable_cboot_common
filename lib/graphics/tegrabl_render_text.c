/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_GRAPHICS

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_utils.h>
#include <string.h>
#include <tegrabl_render_text.h>

struct color_code {
	uint32_t color;
	uint32_t argb_value;
	uint32_t abgr_value;
};

static struct color_code color_format[NUM_OF_COLORS] = {
	/*Color, ARGB_val, ABGR_VAL*/
	{RED, 0x00FF0000, 0x000000FF},
	{WHITE, 0x00FFFFFF, 0x00FFFFFF},
	{GREEN, 0x0033FF33, 0x0033FF33},
	{BLUE, 0x000000FF, 0x00FF0000},
	{YELLOW, 0x00FFFF33, 0x0033FFFF},
	{ORANGE, 0x00FF9933, 0x003399FF},
};

struct text_position current_position;
struct text_font current_font;

static uint32_t rotation_angle;

tegrabl_error_t tegrabl_render_text_set_rotation_angle(uint32_t angle)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((angle != 90) && (angle != 180) && (angle != 270) && (angle != 0)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 1);
		pr_error("(%s) only 0/90/180/270 rotation is supported\n", __func__);
		return err;
	}

	rotation_angle = angle;
	pr_debug("text rotation angle = %d\n", rotation_angle);

	return err;
}

static uint32_t get_rotation_angle(void)
{
	return rotation_angle;
}

struct text_position *tegrabl_render_text_get_position(void)
{
	return &current_position;
}

struct text_font *tegrabl_render_text_get_font(void)
{
	return &current_font;
}

void tegrabl_render_text_set_position(uint32_t x, uint32_t y)
{
	struct text_position *position;
	position = tegrabl_render_text_get_position();

	position->x = x;
	position->y = y;
}

void tegrabl_render_text_set_font(tegrabl_font_type_t type, uint32_t size)
{
	struct text_font *font;
	font = tegrabl_render_text_get_font();
	font->type = type;
	font->size = size;
	font->width = DISP_FONT_WIDTH;
	font->height = DISP_FONT_HEIGHT;
	font->width_scaled = font->width * size;
	font->height_scaled = font->height * size;
}

static void get_pixels_for_char(void *dst_pixels, uint32_t index,
								uint32_t pixel_format, uint32_t color)
{
	uint32_t j, k, l, m;
	uint8_t *font_char;
	uint32_t rotateangle = 0;
	struct text_font *font;
	uint32_t temp_pixel = 0;

	font = tegrabl_render_text_get_font();
	rotateangle = get_rotation_angle();
	font_char = (uint8_t *)&font_default[index * font->height];

	if (pixel_format == PIXEL_FORMAT_A8R8G8B8)
		temp_pixel = color_format[color].argb_value;
	else if (pixel_format == PIXEL_FORMAT_A8B8G8R8)
		temp_pixel = color_format[color].abgr_value;

	uint32_t *pixels = (uint32_t *)dst_pixels;
	pr_debug("%s: color = %d\n", __func__, color);

	for (j = 0; j < font->height; j++) {
		pr_debug("%s: j = %d\n", __func__, j);
		for (k = 0; k < font->width; k++) {
			uint32_t pixel = 0;
			uint32_t index_l = 0;
			uint32_t x, y;

			if (((*font_char) >> (font->width - k)) & 0x1) {
				pixel = temp_pixel;
			}

			switch (rotateangle) {
			case 90:
				index_l = (k * font->height) + (font->height - 1) - j;
				break;
			case 270:
				index_l = ((font->height - k - 1) * font->height) + j;
				break;
			case 180:
				index_l = ((font->height - j - 1) * font->height) +
						(font->width - 1 - k);
				break;
			case 0:
			default:
				index_l = (j * font->height) + k;
				break;
			}
			y = index_l / font->height;
			x = index_l % font->height;
			for (l = 0; l < font->size; l++)
				for (m = 0; m < font->size; m++) {
					pixels[((y * font->size) * font->height_scaled)
						+ (l * font->width_scaled)
						+ ((x * font->size) + m)] = pixel;
				}
		}

		font_char++;
	}
}

static tegrabl_error_t text_surface_read(struct tegrabl_surface *surf,
	uint32_t x, uint32_t y, uint32_t width,	uint32_t height, void *src_pixels)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t rotateangle = get_rotation_angle();

	switch (rotateangle) {
	case 0:
		break;
	case 90:
		SWAP(x, y);
		SWAP(width, height);
		x = surf->width - x - 1 - width;
		break;
	case 180:
		y = surf->height - y - 1 - height;
		x = surf->width - x - 1 - width;
		break;
	case 270:
		SWAP(x, y);
		SWAP(width, height);
		y = surf->height - y - 1 - height;
		break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
		break;
	}

	if (err != TEGRABL_NO_ERROR)
		return err;
	else
		return tegrabl_surface_read(surf, x, y, width, height, src_pixels);
}

static tegrabl_error_t text_surface_write(struct tegrabl_surface *surf,
	uint32_t x, uint32_t y, uint32_t width, uint32_t height,
	const void *src_pixels)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t rotateangle = get_rotation_angle();

	switch (rotateangle) {
	case 0:
		break;
	case 90:
		SWAP(x, y);
		SWAP(width, height);
		x = surf->width - x - 1 - width;
		break;
	case 180:
		y = surf->height - y - 1 - height;
		x = surf->width - x - 1 - width;
		break;
	case 270:
		SWAP(x, y);
		SWAP(width, height);
		y = surf->height - y - 1 - height;
		break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 3);
		break;
	}

	if (err != TEGRABL_NO_ERROR)
		return err;
	else
		return tegrabl_surface_write(surf, x, y, width, height, src_pixels);
}

static tegrabl_error_t line_feed(struct tegrabl_surface *surf,
								 struct text_font *font)
{
	uint32_t i;
	uint32_t size = surf->width * surf->pitch * font->height_scaled;
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	void *line = tegrabl_malloc(size);

	if (!line) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		pr_debug("Failed to allocate memory for line\n");
		return err;
	}

	for (i = font->height_scaled; i < surf->height;	i += font->height_scaled) {
		err = text_surface_read(surf, 0, i, surf->width, font->height_scaled,
								line);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

		err = text_surface_write(surf, 0, i - font->height_scaled, surf->width,
								 font->height_scaled, line);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}
	}

	memset(line, 0, size);
	err = text_surface_write(surf, 0, surf->height - font->height_scaled,
							 surf->width, font->height_scaled, line);

fail:
	tegrabl_free(line);
	return err;
}

tegrabl_error_t tegrabl_render_text(struct tegrabl_surface *surf,
									const char *msg, uint32_t color)
{
	uint32_t len;
	uint32_t i;
	struct text_position *position;
	struct text_font *font;
	uint32_t index;
	uint32_t *pixels;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (!surf || !msg) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		return err;
	}

	position = tegrabl_render_text_get_position();
	font = tegrabl_render_text_get_font();
	pixels = (uint32_t *)tegrabl_malloc(font->width_scaled *
		font->height_scaled * sizeof(uint32_t));
	if (pixels == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 2);
		pr_debug("Failed to allocate memory for pixels\n");
		return err;
	}

	len = strlen(msg);

	for (i = 0; i < len; i++) {
		pr_debug("%s: i=%d, x=%d, y=%d char=%c\n", __func__, i, position->x,
				 position->y, msg[i]);
		if (msg[i] == '\n' || position->y >= surf->height) {
			position->x = 0;
			position->y += font->height_scaled;
			if (position->y >= surf->height) {
				err = line_feed(surf, font);
				if (err != TEGRABL_NO_ERROR) {
					goto fail;
				}

				position->y = surf->height - font->height_scaled;
			}

			if (msg[i] == '\n')
				continue;
		}
		index = msg[i] - ' ';
		get_pixels_for_char(pixels, index, surf->pixel_format, color);
		err = text_surface_write(surf, position->x, position->y,
			font->width_scaled, font->height_scaled, pixels);
		if (err != TEGRABL_NO_ERROR) {
			goto fail;
		}

		position->x += font->width_scaled;
		if (position->x + font->width_scaled >= surf->width) {
			position->x = 0;
			position->y += font->height_scaled;
		}
	}

fail:
	tegrabl_free(pixels);
	return err;
}
