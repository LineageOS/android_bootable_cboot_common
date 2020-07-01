/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef TEGRABL_TIMER_H
#define TEGRABL_TIMER_H

#include <stdint.h>

typedef uint64_t time_t;

/** @brief Wait for requested number of microseconds
 *
 *  @param usec Number of microseconds to wait.
 */
void tegrabl_udelay(time_t usec);

/** @brief Wait for requested number of milliseconds
 *
 *  @param msec Number of milliseconds to wait.
 */
void tegrabl_mdelay(time_t msec);

/** @brief Returns microsecond-timer timestamp in
 *         microseconds
 *
 *  @return Timestamp is microseconds.
 */
time_t tegrabl_get_timestamp_us(void);

/** @brief Returns microsecond-timer timestamp in
 *         milliseconds
 *
 *  @return Timestamp is milliseconds.
 */
time_t tegrabl_get_timestamp_ms(void);

#endif
