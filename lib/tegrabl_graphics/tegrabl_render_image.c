/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 */

#define MODULE TEGRABL_ERR_GRAPHICS

#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_malloc.h>
#include <tegrabl_utils.h>
#include <string.h>
#include <tegrabl_render_image.h>

#define BMP_HEADER_LENGTH 54

/**
 * Defines BMP file Header
 */
struct bitmap_file_header {
	uint16_t magic;
	int32_t file_size;
	uint16_t reserved1;
	uint16_t reserved2;
	uint32_t start_offset;
};

/**
 * Defines Bitmap Information Header
 *
 * Contains information about Bitmap Data in Bitmap Image file
 */

struct bitmap_info_header {
	uint32_t header_size;
	int32_t width;
	int32_t height;
	uint16_t planes;
	uint16_t depth;
	uint32_t compression_type;
	int32_t image_size;
	uint32_t horizontal_resolution;
	uint32_t vertical_resolution;
	uint32_t num_colors;
	uint32_t num_imp_colors;
};

/**
 * Structure to hold all information of BMP
 */
struct bitmap_file {
	struct bitmap_file_header bfh;
	struct bitmap_info_header bih;
	uint8_t *bitmap_data;
};

static uint32_t rotation_angle;

tegrabl_error_t tegrabl_render_image_set_rotation_angle(uint32_t angle)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((angle != 90) && (angle != 180) && (angle != 270) && (angle != 0)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 1);
		pr_error("(%s) only 0/90/180/270 rotation is supported\n", __func__);
		return err;
	}

	rotation_angle = angle;
	pr_debug("image rotation angle = %d\n", rotation_angle);

	return err;
}

static uint32_t image_get_rotation_angle(void)
{
	return rotation_angle;
}

static void fill_bmp_header(struct bitmap_file *bmf, uint8_t *buf)
{
	uint16_t *hdr;

	hdr = (uint16_t *)buf;
	/* Fill bitmap file header */
	bmf->bfh.magic = hdr[0];
	bmf->bfh.file_size = hdr[1] | hdr[2] << 16;
	bmf->bfh.reserved1 = hdr[3];
	bmf->bfh.reserved2 = hdr[4];
	bmf->bfh.start_offset = hdr[5] | hdr[6] << 16;

	/* Fill bitmap information header */
	bmf->bih.header_size = hdr[7] | hdr[8] << 16;
	bmf->bih.width = hdr[9] | hdr[10] << 16;
	bmf->bih.height = hdr[11] | hdr[12] << 16;
	bmf->bih.planes = hdr[13];
	bmf->bih.depth = hdr[14] | hdr[15] << 16;
	bmf->bih.compression_type = hdr[15] | hdr[16] << 16;
	bmf->bih.image_size = hdr[17] | hdr[18] << 16;
	bmf->bih.horizontal_resolution = hdr[19] | hdr[20] << 16;
	bmf->bih.vertical_resolution = hdr[21] | hdr[22] << 16;
	bmf->bih.num_colors = hdr[23] | hdr[24] << 16;
	bmf->bih.num_imp_colors = hdr[25] | hdr[26] << 16;
}

static tegrabl_error_t parse_bmp(uint8_t *buf, uint32_t len,
								 struct bitmap_file *bmf)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if ((!buf) || (!bmf) || (len < BMP_HEADER_LENGTH)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	/* read header */
	fill_bmp_header(bmf, buf);

	if (memcmp((void *)&(bmf->bfh.magic), "BM", strlen("BM"))) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
		pr_error("(%s) Only BMP image is supported\n", __func__);
		goto fail;
	}

	if ((bmf->bfh.file_size < 0) || (bmf->bih.image_size < 0)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}

	if (bmf->bih.image_size == 0) {
		bmf->bih.image_size = bmf->bfh.file_size - BMP_HEADER_LENGTH;
	}

	bmf->bitmap_data = buf + BMP_HEADER_LENGTH;

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("(%s) Unsuccesful attempt to read BMP image\n", __func__);
	}

	return err;
}

tegrabl_error_t tegrabl_render_bmp(struct tegrabl_surface *surf,
								   uint8_t *buf, uint32_t length)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct bitmap_file *bmf = NULL;

	uint32_t draw_height = 0;
	uint32_t draw_width = 0;
	uint32_t *temp1 = NULL;
	uint32_t surface_offset, image_offset;
	int32_t x, y;
	uint32_t r, g, b;
	uint32_t color = 0xffffff;
	uint32_t bytes_per_pixel = 0;
	uint32_t pixel_offset = 0;
	uint32_t rotate_angle;
	uint32_t x_off = 0, y_off = 0;

	if (!buf || !surf || !length) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto fail;
	}

	/* Allocate bmp file structure */
	bmf = tegrabl_malloc(sizeof(struct bitmap_file));
	if (!bmf) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 1);
		goto fail;
	}

	/* Read bmp header and data */
	err = parse_bmp(buf, length, bmf);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	length = bmf->bih.image_size;
	pr_debug("%s, image size = %d\n", __func__, length);

	if (bmf->bih.compression_type) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 4);
		pr_error("(%s) Only uncompressed BMP image is supported\n",	__func__);
		goto fail;
	}

	if ((bmf->bih.height < 0) || (bmf->bih.width < 0)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
		pr_error("(%s) Invalid height or width in BMP image\n",	__func__);
		goto fail;
	}

	/* Get Panel details before setting up logistics */
	rotate_angle = image_get_rotation_angle();

	if ((rotate_angle == 90) || (rotate_angle == 270)) {
		draw_height = bmf->bih.width;
		draw_width = bmf->bih.height;
	} else if ((rotate_angle == 0) || (rotate_angle == 180)) {
		draw_height = bmf->bih.height;
		draw_width = bmf->bih.width;
	} else {
		pr_error("Not a valid rotation angle\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 5);
		goto fail;
	}

	if (draw_width > surf->width) {
		pr_error("Image dimensions not supported\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 6);
		goto fail;
	}
	if (draw_height > surf->height) {
		pr_error("Image dimensions not supported\n");
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 7);
		goto fail;
	}

	x_off = (surf->width - draw_width) / 2;
	y_off = (surf->height - draw_height) / 2;
	pr_debug("draw width = %d, draw height = %d\n",	draw_width, draw_height);

	bytes_per_pixel = bmf->bih.depth / 8;
	temp1 = tegrabl_malloc(draw_width * draw_height * sizeof(uint32_t));
	memset(temp1, 0, draw_width * draw_height * sizeof(uint32_t));

	for (y = 0; y < bmf->bih.height; y++) {
		image_offset =  y * ALIGN(bmf->bih.width *
			(bmf->bih.depth / (8 * sizeof(uint8_t))), sizeof(uint32_t));

		for (x = 0; x < bmf->bih.width; x++) {
			pixel_offset = image_offset + (x * bytes_per_pixel);
			if (bytes_per_pixel == 2) {
				color = bmf->bitmap_data[pixel_offset] |
					(bmf->bitmap_data[pixel_offset + 1] << 8);
				r = color & 0x1f;
				g = (color >> 5) & 0x1f;
				b = (color >> 10) & 0x1f;
			} else if (bytes_per_pixel == 3 || bytes_per_pixel == 4) {
				r = bmf->bitmap_data[pixel_offset];
				g = bmf->bitmap_data[pixel_offset + 1];
				b = bmf->bitmap_data[pixel_offset + 2];
			} else {
				err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 8);
				pr_error("(%s) Only 16,24 and 32 bits per pixel is supported\n",
						 __func__);
				goto fail;
			}
			color = b | (g << 8) | (r << 16);
			if (rotate_angle == 90) {
				surface_offset = x * draw_width;
				temp1[surface_offset + y] = color;
			} else if (rotate_angle == 180) {
				surface_offset = y * draw_width;
				temp1[surface_offset + (draw_width - x - 1)] = color;
			} else if (rotate_angle == 270) {
				surface_offset = (draw_height - x - 1) * draw_width;
				temp1[surface_offset + (draw_width - y - 1)] = color;
			} else {
				surface_offset = (draw_height - y - 1) * draw_width;
				temp1[surface_offset + x] = color;
			}
		}
	}
	err = tegrabl_surface_write(surf, x_off, y_off,	draw_width, draw_height,
								temp1);

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Unsuccesful attempt to draw BMP image\n");
	}
	if (temp1 != NULL) {
		tegrabl_free(temp1);
	}
	if (bmf != NULL) {
		tegrabl_free(bmf);
	}
	return err;
}

tegrabl_error_t tegrabl_render_jpeg(struct tegrabl_surface *surf,
									uint8_t *buf, uint32_t length)
{
	/*Dummy func - Not supported yet*/
	return TEGRABL_ERR_NOT_SUPPORTED;
}

tegrabl_error_t tegrabl_render_image(struct tegrabl_surface *surf, uint8_t *buf,
									 uint32_t length, uint32_t image_format)
{
	if (image_format == TEGRABL_IMAGE_FORMAT_BMP)
		return tegrabl_render_bmp(surf, buf, length);
	else
		return tegrabl_render_jpeg(surf, buf, length);
}
