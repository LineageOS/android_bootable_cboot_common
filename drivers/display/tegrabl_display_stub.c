/*
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdint.h>
#include <tegrabl_debug.h>
#include <tegrabl_error.h>
#include <tegrabl_display.h>

tegrabl_error_t tegrabl_display_init(void)
{
	pr_debug("%s: stub\n", __func__);
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_display_printf(color_t color,
									   const char *format, ...)
{
	pr_debug("%s: stub\n", __func__);
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_display_show_image(struct tegrabl_image_info *image)
{
	pr_debug("%s: stub\n", __func__);
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_display_shutdown(void)
{
	pr_debug("%s: stub\n", __func__);
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_display_clear(void)
{
	pr_debug("%s: stub\n", __func__);
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_display_text_set_cursor(uint32_t position)
{
	pr_debug("%s: stub\n", __func__);
	return TEGRABL_NO_ERROR;
}
