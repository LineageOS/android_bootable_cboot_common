/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TEGRABL_DISPLAY_H_
#define __TEGRABL_DISPLAY_H_

#include <stdint.h>
#include <tegrabl_error.h>
#include <tegrabl_render_text.h>
#include <tegrabl_render_image.h>
#include <tegrabl_display_unit.h>

/**
 *  @brief Initializes the display
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_init(void);

/**
 *  @brief Prints the given text in given color on display
 *
 *  @param color Color of characters in the text.
 *  @param format  Pointer to the text string to print
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_printf(enum color color,
									   const char *format, ...);

/**
 *  @brief Prints the given image on the display
 *
 *  @param image Pointer to image info structure.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_show_image(struct tegrabl_image_info *image);

/**
 *  @brief Clears the display
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_clear(void);

/**
 *  @brief Shutdowns the display
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_shutdown(void);

/**
 *  @brief Set text cursor
 *
 *  @param position to set cursor
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_text_set_cursor(enum cursor_position position);

/**
 *  @brief Get display params like fb address, surface height/width etc.
 *
 *  @param du_idx index of display unit to get display params
 *  @param disp_param struct to fill with display params
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_display_get_params(uint32_t du_idx,
	struct tegrabl_display_unit_params *disp_param);

#endif
