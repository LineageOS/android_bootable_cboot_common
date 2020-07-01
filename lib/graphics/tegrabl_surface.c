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
#include <string.h>
#include <tegrabl_surface.h>

#define LOWEST_BIT_ONLY(v)	((uint32_t)(v) & (uint32_t)-(int32_t)(v))
#define IS_POWER_OF_2(v)	(LOWEST_BIT_ONLY(v) == (uint32_t)(v))

#define SURFACE_LINEAR_HEIGHT_ALIGN	16
#define SURFACE_LINEAR_SIZE_ALIGN	(128*1024)
#define SURFACE_LINEAR_PITCH_ALIGN	256
#define SURFACE_LINEAR_BASE_ALIGN	4096
#define BITS_PER_PIXEL 32

static inline uint32_t align_value(uint32_t value, uint32_t alignment)
{
	return (value + (alignment-1)) & ~(alignment-1);
}

static bool is_npot(uint32_t val)
{
	return !IS_POWER_OF_2(val);
}

void surface_free(struct tegrabl_surface *surf)
{
	/* Free surface from non-RM alloc'd memory */
	tegrabl_free((void *)surf->base);
	surf->base = 0;
}

tegrabl_error_t surface_compute_pitch(struct tegrabl_surface *surf)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t pitch;

	/* Calculate pitch & align (by adding pad bytes) */
	if (surf->layout == SURFACE_LAYOUT_PITCH) {
		pitch = surf->width * BITS_PER_PIXEL;
		pitch = (pitch + 7) >> 3;
		pitch = align_value(pitch, SURFACE_LINEAR_PITCH_ALIGN);
	} else {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 1);
		pitch = 0;
	}
	surf->pitch = pitch;
	return err;
}

tegrabl_error_t surface_compute_size(struct tegrabl_surface *surf)
{
	uint32_t num_bytes;
	uint32_t aligned_height = surf->height;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (surf->width == 0 ||	surf->height == 0) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		return err;
	}

	err = surface_compute_pitch(surf);

	if (surf->pitch == 0) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		return err;
	}

	switch (surf->layout) {
	case SURFACE_LAYOUT_PITCH:
		if (is_npot(surf->width) || is_npot(surf->height)) {
			aligned_height = align_value(aligned_height,
				SURFACE_LINEAR_HEIGHT_ALIGN);
		}
			break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 2);
		return err;
	}

	num_bytes = surf->pitch * aligned_height;

	/* Adding extra row and a pixel fixes */
	num_bytes += surf->pitch + ((BITS_PER_PIXEL + 7) >> 3);

	num_bytes = align_value(num_bytes, SURFACE_LINEAR_SIZE_ALIGN);

	surf->size = num_bytes;
	return err;
}

tegrabl_error_t surface_compute_alignment(struct tegrabl_surface *surf)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (surf->layout == SURFACE_LAYOUT_PITCH)
		surf->alignment = SURFACE_LINEAR_BASE_ALIGN;
	else
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 3);

	return err;
}

void tegrabl_surface_clear(struct tegrabl_surface *surf)
{
	memset((void *)surf->base, 0, surf->size);
}

tegrabl_error_t tegrabl_surface_setup(struct tegrabl_surface *surf)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uintptr_t base;

	if (!surf) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		return err;
	}

	surf->layout = SURFACE_LAYOUT_PITCH;
	surf->pixel_format = PIXEL_FORMAT_A8B8G8R8;
	err = surface_compute_pitch(surf);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("compute pitch failed\n");
		goto fail;
	}

	err = surface_compute_alignment(surf);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("compute alignment failed\n");
		goto fail;
	}

	err = surface_compute_size(surf);
	if (err != TEGRABL_NO_ERROR) {
		pr_debug("compute size failed\n");
		goto fail;
	}

	/* This buffer is not freed */
	base = (uintptr_t)tegrabl_malloc(surf->size + surf->alignment - 1);
	if ((void *)base == NULL) {
		pr_debug("allocation for framebuffer failed\n");
		err = TEGRABL_ERR_NO_MEMORY;
		goto fail;
	}
	base += surf->alignment;
	base &= ~(surf->alignment - 1);
	pr_debug("base = %p, width = %d, height = %d\n", (void *)base, surf->width,
			 surf->height);
	pr_debug("pitch  = %d, alignment = %d, size = %d\n", surf->pitch,
			 surf->alignment, surf->size);
	surf->base = base;
	memset((void *)surf->base, 0, surf->size);

fail:
	return err;
}

tegrabl_error_t tegrabl_surface_write(struct tegrabl_surface *surf,	uint32_t x,
	uint32_t y, uint32_t width, uint32_t height, const void *src_pixels)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	const uint8_t *src = src_pixels;
	uint32_t bits_per_pixel = BITS_PER_PIXEL;
	uint8_t *dest;
	bool is_interlace;
	uint32_t cury;
	uint8_t *dest_current;

	pr_debug("x = %d, y = %d, width = %d, height = %d\n",
			 x, y, width, height);
	/* Make sure we don't fall off the end of the surface */
	if (x + width > surf->width) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 2);
		goto fail;
	}
	if (y + height > surf->height) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto fail;
	}

	/* Convert x and width from units of pixels to units of bytes
	 * We assert here that the rectangle is aligned to byte boundaries */
	x *= bits_per_pixel;
	if (x & 7) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 4);
		goto fail;
	}
	x >>= 3;

	width *= bits_per_pixel;
	if (width & 7) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 5);
		goto fail;
	}
	width >>= 3;
	is_interlace = (surf->scan_format == SCAN_FORMAT_INTERLACIVE);
	if (surf->layout == SURFACE_LAYOUT_PITCH) {
		uint32_t second_field_offset = surf->second_field_offset;
		dest = (uint8_t *)surf->base + y * surf->pitch + x;
		if (is_interlace) {
			cury = 0;
			for (; cury < height; cury++) {
				if ((cury + y) & 1)
					dest_current = dest + (cury >> 1) * (surf->pitch) +
						second_field_offset;
				else
					dest_current = dest + (cury >> 1) * (surf->pitch);

				 memcpy(dest_current, src, width);
				 src += width;
			}
		} else {
			while (height--) {
				memcpy(dest, src, width);
				src += width;
				dest += surf->pitch;
			}
		}
	} else {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 4);
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_surface_read(struct tegrabl_surface *surf, uint32_t x,
	uint32_t y, uint32_t width, uint32_t height, void *dest_pixels)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint8_t *dest = dest_pixels;
	uint32_t bits_per_pixel = BITS_PER_PIXEL;
	const uint8_t *src;
	bool is_interlace = (surf->scan_format == SCAN_FORMAT_INTERLACIVE);
	uint32_t cury;

	/* Make sure that the surface has data associated with it */
	if (!(surf->base)) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 6);
		goto fail;
	}
	pr_debug("x = %d, y = %d, width = %d, height = %d\n",
			 x, y, width, height);

	/* Make sure we don't fall off the end of the surface */
	if (x + width > surf->width) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 7);
		goto fail;
	}
	if (y + height > surf->height) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 8);
		goto fail;
	}

	/* Convert x and width from units of pixels to units of bytes */
	/* We assert here that the rectangle is aligned to byte boundaries */
	x *= bits_per_pixel;
	if (x & 7) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 9);
		goto fail;
	}
	x >>= 3;

	width *= bits_per_pixel;
	if (width & 7) {
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 10);
		goto fail;
	}
	width >>= 3;

	if (surf->layout == SURFACE_LAYOUT_PITCH) {
		uint32_t second_field_offset = surf->second_field_offset;

		src = (uint8_t *)surf->base + y * surf->pitch + x;
		if (is_interlace) {
			cury = 0;
			for (; cury < height; cury++) {
				if ((cury + y) & 1)
					src = src + (cury >> 1) * (surf->pitch) +
						second_field_offset;
				else
					src = src + (cury >> 1) * (surf->pitch);

				 memcpy(dest, src, width);
				 dest += width;
				 src = (uint8_t *)surf->base + y * surf->pitch + x;
			}
		} else {
			while (height--) {
				memcpy(dest, src, width);
				dest += width;
				src += surf->pitch;
			}
		}
	} else {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 5);
	}

fail:
	return err;
}
