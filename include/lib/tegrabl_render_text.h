/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_RENDER_TEXT_H
#define TEGRABL_RENDER_TEXT_H

#include <stdint.h>
#include <tegrabl_surface.h>
#include <tegrabl_font.h>

/* text colors */
/* macro color_t */
typedef uint32_t color_t;
#define RED 0
#define WHITE 1
#define GREEN 2
#define BLUE 3
#define YELLOW 4
#define ORANGE 5
#define BLACK 6
#define CYAN 7
#define MAGENTA 8
#define BROWN 9
#define LIGHTGRAY 10
#define DARKGRAY 11
#define LIGHTBLUE 12
#define LIGHTGREEN 13
#define LIGHTCYAN 14
#define LIGHTRED 15
#define LIGHTMAGENTA 16
#define NUM_OF_COLORS 17

struct text_position {
	uint32_t x;
	uint32_t y;
};

/* macro font */
typedef uint32_t font_t;
#define FONT_DEFAULT 0

struct text_font {
	uint32_t type;
	uint32_t size;
	uint32_t width;
	uint32_t height;
	uint32_t width_scaled;
	uint32_t height_scaled;
};

/**
 * @brief get text position
 *
 * @return text_position current text position
 */
struct text_position *tegrabl_render_text_get_position(void);

/**
 *  @brief Rendors the given text on the given surface.
 *
 *  @param surf A pointer to structure describing the surface.
 *  @param msg Address of the buffer which holds the text message.
 *  @param color Color of characters in the text.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_render_text(struct tegrabl_surface *surf,
									const char *msg, uint32_t color);

/**
 *  @brief Sets the position of the text on the surface.
 *
 *  @param x,y  Gives the X, Y coordinate of the position.
 */
void tegrabl_render_text_set_position(uint32_t x, uint32_t y);

/**
 *  @brief Sets the font-type and size of the text.
 *
 *  @param type Font of the text
 *  @param size size of the text
 */
void tegrabl_render_text_set_font(tegrabl_font_type_t type, uint32_t size);

/** @brief Sets the rotation angle for the text, at which it has be rendered.
 *
 *  @param angle Rotation angle for the text 0/90/180/270 degress.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_render_text_set_rotation_angle(uint32_t angle);

/**
 *  @brief Get the font params of text
 */
struct text_font *tegrabl_render_text_get_font(void);

#endif
