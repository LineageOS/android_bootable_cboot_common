/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_DISPLAY_UNIT_H
#define TEGRABL_DISPLAY_UNIT_H

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_surface.h>
#include <tegrabl_render_text.h>
#include <tegrabl_render_image.h>
#include <tegrabl_nvdisp.h>
#include <tegrabl_nvblob.h>
#include <tegrabl_display_dtb.h>

/**
 *  @brief enum for different cursor position
 *
 */
/* macro cursor position */
typedef uint32_t cursor_position_t;
#define CURSOR_START 0
#define CURSOR_CENTER 1
#define CURSOR_END 2

/**
* @brief display unit ioctl list
*/
/* macro display unit ioctl */
typedef uint32_t tegrabl_display_unit_ioctl_t;
#define DISPLAY_UNIT_IOCTL_SET_TEXT_POSITION 0
#define DISPLAY_UNIT_IOCTL_GET_DISPLAY_PARAMS 1
#define DISPLAY_UNIT_IOCTL_SET_FONT 2
#define DISPLAY_UNIT_IOCTL_CONTROL_BACKLIGHT 3
#define DISPLAY_UNIT_IOCTL_SET_ROTATION 4

/**
 *  @brief structure for image info
 *
 */
struct tegrabl_image_info {
	tegrabl_image_type_t type;
	tegrabl_image_format_t format;
	uint8_t *image_buf;
	size_t size;
};

/**
* @brief display unit parameter structure
*/
struct tegrabl_display_unit_params {
	uint32_t size;
	uintptr_t addr;
	uint32_t instance;
	uint32_t rotation_angle;
	uint32_t height;
	uint32_t width;
	uintptr_t lut_addr;
	uint32_t lut_size;
};

/**
* @brief display unit definition
*/
struct tegrabl_display_unit {
	tegrabl_display_unit_type_t type;
	font_t font;
	uint32_t height;
	uint32_t width;
	uint32_t n_surf;
	uint32_t win_id;
	struct tegrabl_surface *surf[1];
	struct text_position position;
	uint32_t rotation_angle;
	bool is_init_done;
	struct tegrabl_nvdisp *nvdisp;
};

/**
 *  @brief Initializes the display
 *
 *  @param hdu display unit type
 *  @return returns handle to the display.
 */
struct tegrabl_display_unit *tegrabl_display_unit_init(
	tegrabl_display_unit_type_t hdu, struct tegrabl_display_pdata *pdata);

/**
 *  @brief Prints the given text in given color on display console
 *
 *  @param du Handle of the display
 *  @param color Color of characters in the text.
 *  @param text Pointer to the text string to print
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_unit_printf(struct tegrabl_display_unit *du,
	uint32_t color, const char *text);

/**
 *  @brief Prints the given bmp image on the centre of display console
 *
 *  @param du Handle of the display
 *  @param image_info contains information about the image to be displayed
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_unit_show_image(struct tegrabl_display_unit *du,
	struct tegrabl_image_info *image_info);

/**
 *  @brief Provides multiple functionalities through ioctls like
 *         get_display_params, set_rotation, set_cursor_position etc.
 *
 *  @param du Handle of the display
 *  @param ioctl ioctl of the required functionality
 *  @param args poiinter to the input/output arguments
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_unit_ioctl(struct tegrabl_display_unit *du,
	uint32_t ioctl, void *args);

/**
 *  @brief Clears the display console and sets the cursor position to start
 *
 *  @param du Handle of the display
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_unit_clear(struct tegrabl_display_unit *du);

/**
 *  @brief Shoutdown display unit
 *
 *  @param du Handle of the display
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_unit_shutdown(struct tegrabl_display_unit *du);

#endif
