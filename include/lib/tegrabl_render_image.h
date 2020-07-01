/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef TEGRABL_RENDOR_BMP_H
#define TEGRABL_RENDOR_BMP_H

#include <stdint.h>
#include <tegrabl_surface.h>

/**
* @brief Image format type
*/
/* macro tegrabl image format */
typedef uint32_t tegrabl_image_format_t;
#define TEGRABL_IMAGE_FORMAT_BMP 0
#define TEGRABL_IMAGE_FORMAT_JPEG 1

/** @brief Rendors the bmp image in the given surface.
 *
 *  @param surf address of the surface to which bmp image has to be rendered.
 *  @param buf address of bmp image data.
 *  @param size size of the buffer, which holds bmp image data.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_render_image(struct tegrabl_surface *surf, uint8_t *buf,
									 uint32_t size, uint32_t image_format);

/** @brief Set the rotation angle of the image
 *
 *  @param angle rotation angle, if image needs to be rotated.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_render_image_set_rotation_angle(uint32_t angle);

#endif
