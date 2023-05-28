/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __TEGRABL_MODES_H__
#define __TEGRABL_MODES_H__

#include <tegrabl_edid.h>

extern struct hdmi_mode s_640_480_1;

extern struct hdmi_mode s_720_480_2;

extern struct hdmi_mode s_1024_768_0;

extern struct hdmi_mode s_1280_720_4;

extern struct hdmi_mode s_1920_1080_16;

extern struct hdmi_mode s_720_576_17;

extern struct hdmi_mode s_1280_720_19;

extern struct hdmi_mode s_1920_1080_31;

extern struct hdmi_mode s_1920_1080_32;

extern struct hdmi_mode s_1920_1080_33;

extern struct hdmi_mode s_1920_1080_34;

extern struct hdmi_mode s_3840_2160_95;

extern struct hdmi_mode s_3840_2160_97;

/* HDMI_VIC 0x01: 3840x2160p @ 29.97/30Hz */
extern struct hdmi_mode s_hdmi_vic_1;

/* HDMI_VIC 0x02: 3840x2160p @ 25Hz */
extern struct hdmi_mode s_hdmi_vic_2;

/* HDMI_VIC 0x03: 3840x2160p @ 23.98/24Hz */
extern struct hdmi_mode s_hdmi_vic_3;

/* HDMI_VIC 0x04: 4096x2160p @ 24Hz */
extern struct hdmi_mode s_hdmi_vic_4;

extern struct hdmi_mode *s_hdmi_modes[];

extern uint32_t size_s_hdmi_modes;

/* @brief selects the best mode out of given modes
 *
 * @param monitor_info list of modes from edid
 * @param best_mode best mode returned by this algorithm
 */
void get_best_mode(struct monitor_data *monitor_info,
				   struct hdmi_mode *best_mode);

#endif
