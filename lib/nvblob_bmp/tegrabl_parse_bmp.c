/*
 * Copyright (c) 2014-2017, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_NVBLOB

#include <tegrabl_parse_bmp.h>
#include <tegrabl_nvblob.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>

tegrabl_blob_handle bh;
bool is_initialized;
uint32_t num_images;

static tegrabl_bmp_resolution_t get_optimal_bmp_resolution(
	uint32_t panel_resolution, bool is_panel_portrait, uint32_t rotation_angle)
{
	bool is_bmp_portrait = false;

	if (rotation_angle == 0 || rotation_angle == 180)
		is_bmp_portrait = is_panel_portrait ? true : false;
	else
		is_bmp_portrait = is_panel_portrait ? false : true;

	if (is_bmp_portrait) {
		/* For portrait panels, currently only support 1200x1920 panel.
		   More portrait panel resolution can be added later */
		return BMPRES_1200P_P;
	} else {
		if (panel_resolution >= 2160)
			return BMPRES_4K;
		else if (panel_resolution >= 1080 && panel_resolution < 2160)
			return BMPRES_1080P;
		else if (panel_resolution >= 810 && panel_resolution < 1080)
			return BMPRES_810P;
		else if (panel_resolution >= 720 && panel_resolution < 810)
			return BMPRES_720P;
		else
			return BMPRES_480P;
	}
}

tegrabl_error_t tegrabl_load_bmp_blob(char* part_name)
{
	tegrabl_blob_type_t btype = BLOB_NONE;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (bh == 0)
	{
		error = tegrabl_blob_init(part_name, NULL, &bh);
		if(error != TEGRABL_NO_ERROR)
		{
			pr_error("%s: BMP blob initialization failed\n", __func__);
			goto fail;
		}
	}

	error = tegrabl_blob_get_type(bh, &btype);
	if (error != TEGRABL_NO_ERROR)
	{
		bh = 0;
		goto fail;
	}

	if (btype != BLOB_BMP)
	{
		tegrabl_blob_close(bh);
		bh = 0;
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		pr_error("No valid bmp blob found\n");
		goto fail;
	}

	error = tegrabl_blob_get_num_entries(bh, &num_images);
	if (error != TEGRABL_NO_ERROR)
	{
		goto fail;
	}

	is_initialized = true;
fail:
	return error;
}

void tegrabl_unload_bmp_blob(void)
{
	tegrabl_blob_close(bh);
	is_initialized = false;
}

tegrabl_error_t tegrabl_get_bmp(struct tegrabl_bmp_image *img)
{
	tegrabl_bmp_resolution_t default_image_res = BMPRES_480P;
	tegrabl_bmp_resolution_t optimal_image_res;
	uint32_t i = 0;
	struct tegrabl_bmp_entry *image_info = NULL;
	int desired_entry = -1;
	uint32_t bmp_length = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (img == NULL) {
		error = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
		goto fail;
	}

	if (is_initialized != true)
	{
		pr_error("bmp blob is not loaded and initialized\n");
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 1);
		goto fail;
	}

	optimal_image_res = get_optimal_bmp_resolution(img->panel_resolution,
		img->is_panel_portrait, img->rotation_angle);

	while (i < num_images) {
		error = tegrabl_blob_get_entry(bh, i, (void **)&image_info);
		if (image_info->bmp_type == img->img_type)
		{
			if (image_info->bmp_res >= default_image_res &&
				image_info->bmp_res <= optimal_image_res)
				{
					desired_entry = i;
					default_image_res = image_info->bmp_res;
				}
		}
		i++;
	}

	if (desired_entry == -1) {
		pr_error("%s: Required BMP %d with resolution type=%d \
				not found\n", __func__, img->img_type, optimal_image_res);
		error = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 2);
		goto fail;
	}

	error = tegrabl_blob_get_entry_data(bh, desired_entry, &(img->bmp),
										&bmp_length);
	if (error != TEGRABL_NO_ERROR) {
		goto fail;
	}

	img->image_size = bmp_length;

fail:
	return error;
}
