/*
 * Copyright (c) 2014-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __PARSE_BMP_H
#define __PARSE_BMP_H

#include <tegrabl_nvblob.h>

/**
 * @brief bmp image descriptor
 *
 * @img_type type of the bmp image
 * @bmp image buffer
 * @image_size size of bmp image
 * @panel_resolution resolution of the panel for which best bmp is searched
 * @is_panel_portrait bool value denoting if panel is portrait
 * @rotation_angle rotation angle (0, 90, 180, 270)
 */
struct tegrabl_bmp_image {
	tegrabl_image_type_t img_type;
	uint8_t *bmp;
	size_t image_size;
	uint32_t panel_resolution;
	bool is_panel_portrait;
	uint32_t rotation_angle;
};

/**
 * @brief load partition that contains bmp images to be displayed
 *        and get display resolution
 *
 * @param part_name Name of the partition which contains NvBlob
.*
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_load_bmp_blob(char *part_name);

/**
 * @brief free memory used by bmp partition, if it is loaded using
 *        load_bmp_blob api
 */
void tegrabl_unload_bmp_blob(void);

/**
 * @brief get location and size of bmp image (of specified image_type
 *        resolution - set by load_bmp_blob).
 *        user of this api should not try to free bmp, as it will be done
 *        by unload_bmp_blob at the end of android_boot
 *
 * @param img img structure that contains all bmp image properties
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error.
 */
tegrabl_error_t tegrabl_get_bmp(struct tegrabl_bmp_image *img);

#endif
