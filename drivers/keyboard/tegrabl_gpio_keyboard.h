/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TEGRABL_GPIO_KEYBOARD_H_
#define __TEGRABL_GPIO_KEYBOARD_H_

#include <tegrabl_keyboard.h>

/**
 * @brief Initialised gpio keyboard driver
 * @return TEGRABL_NO_ERROR always
 */
tegrabl_error_t tegrabl_gpio_keyboard_init(void);

/** @brief Gets key code of pressed GPIO keys
 *
 * @param key_code address of key code of pressed keys to be stored
 * @param key_event press or release event
 *
 * @return TEGRABL_NO_ERROR if no error, else appropriate error code
 */
tegrabl_error_t tegrabl_gpio_keyboard_get_key_data(key_code_t *key_code,
												   key_event_t *key_event);

#endif
