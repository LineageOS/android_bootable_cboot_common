/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TEGRABL_KEYBOARD_H__
#define __TEGRABL_KEYBOARD_H__

#include <tegrabl_error.h>

/**
 * Different type for key operation event
 */
enum key_event {
	KEY_RELEASE_FLAG = 1,
	KEY_PRESS_FLAG,
	KEY_HOLD_FLAG,
	KEY_EVENT_IGNORE = 0xFF
};

/**
 * Key code enum that indicate which key is being operated
 */
enum key_code {
	KEY_DOWN = 1,
	KEY_UP,
	KEY_UP_DOWN,
	KEY_ENTER,
	KEY_DOWN_ENTER,
	KEY_UP_ENTER,
	KEY_UP_DOWN_ENTER,
	KEY_HOLD,
	KEY_IGNORE = 0xFF
};

/**
 * @brief Initialises keyboard driver
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code
 */
tegrabl_error_t tegrabl_keyboard_init(void);

/**
 * @brief detect keyboard status and get the info about which key is pressed or
 *        released. This API is non-blocking hence user may get KEY_IGNORE if
 *        there's no keyboard event.
 *
 * @param code keycode of keys that are pressed or released (output param)
 * @param event key press or release event (output param)
 *
 * @return TEGRABL_NO_ERROR if successful else appropriate error code
 */
tegrabl_error_t tegrabl_keyboard_get_key_data(enum key_code *code,
											  enum key_event *event);

#endif /* __KEYBOARD_H__ */
