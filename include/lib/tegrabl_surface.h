/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRABL_SURFACE_H
#define TEGRABL_SURFACE_H

#include <stdint.h>

/**
 * enum for different possible pixel formats
 */
enum tegrabl_pixel_format {
	PIXEL_FORMAT_A8R8G8B8,
	PIXEL_FORMAT_R8G8B8A8,
	PIXEL_FORMAT_B8G8R8A8,
	PIXEL_FORMAT_A8B8G8R8,
};

/** Defines the different display scan formats in the video planes.
 *  This enumeration is built to support 2 display formats :
 *  progressive,
 *  interlaced
 */
enum tegrabl_scan_format {
	SCAN_FORMAT_PROGRESSIVE,
	SCAN_FORMAT_INTERLACIVE,
};

/**
 *  The possible layouts that a surface can presently have. More layouts can be
 *  added when invented by HW.
 */
enum tegrabl_surface_layout {
	SURFACE_LAYOUT_PITCH,
	SURFACE_LAYOUT_LINEAR,
	SURFACE_LAYOUT_BLOCKLINEAR,
};

/**
 *  A structure representing a surface
 */
struct tegrabl_surface {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uintptr_t base;
	uint32_t size;
	uint32_t alignment;
	enum tegrabl_pixel_format pixel_format;
	enum tegrabl_scan_format scan_format;
	enum tegrabl_surface_layout layout;
	uint32_t second_field_offset;
};

/**
 *  @brief Writes a subregion of a surface.
 *
 *  @param surf A pointer to structure describing the surface.
 *  @param x X coordinate of top-left pixel of rectangle to write.
 *  @param y Y coordinate of top-left pixel of rectangle to write.
 *  @param width Width of rectangle to write.
 *  @param height Height of rectangle to write.
 *  @param src_pixels A pointer to an array of Width*Height pixels to write.
 *                    The pixels are stored in pitch format with a pitch of
 *                    Width*BytesPerPixel. Must be aligned to no less than
 *                    the word size of ColorFormat.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_surface_write(struct tegrabl_surface *surf, uint32_t x,
	uint32_t y, uint32_t width, uint32_t height, const void *src_pixels);

/**
 *  @brief Reads a subregion of a surface.
 *
 *  @param surf A pointer to structure describing the surface.
 *  @param x X coordinate of top-left pixel of rectangle to read.
 *  @param y Y coordinate of top-left pixel of rectangle to read.
 *  @param width Width of rectangle to read.
 *  @param height Height of rectangle to read.
 *  @param dest_pixels A pointer to an array of Width*Height pixels to be read.
 *                     The pixels are stored in pitch format with a pitch of
 *                     Width*BytesPerPixel. Must be aligned to no less than the
 *                     word size of ColorFormat.
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_surface_read(struct tegrabl_surface *surf, uint32_t x,
	uint32_t y, uint32_t width, uint32_t height, void *dest_pixels);

/**
 *  @brief Sets up give surface. Calculates pitch, alignment and also allocates
 *         memory for the frame buffer.
 *
 *  @param surf A pointer to structure describing the surface.
 *
 *  @return TEGRABL_NO_ERROR if success, error code if fails.
 */
tegrabl_error_t tegrabl_surface_setup(struct tegrabl_surface *surf);

/**
 *  @brief Clear a give surface. Free the frame buffer.
 *
 *  @param surf A pointer to structure describing the surface.
 */
void tegrabl_surface_clear(struct tegrabl_surface *surf);

#endif
