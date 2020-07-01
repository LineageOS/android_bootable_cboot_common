#
# Copyright (c) 2016-2018, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_DEPS += \
	$(LOCAL_DIR)/../../lib/graphics

GLOBAL_INCLUDES += \
	$(LOCAL_DIR) \
	$(LOCAL_DIR)/nvdisp \
	$(LOCAL_DIR)/hdmi \
	$(LOCAL_DIR)/dp \
	$(LOCAL_DIR)/sor \
	$(LOCAL_DIR)/edid \
	$(LOCAL_DIR)/platform_data \
	$(LOCAL_DIR)/panel \
	$(LOCAL_DIR)/backlight \
	$(LOCAL_DIR)/../../include/drivers/display \
	$(LOCAL_DIR)/../../include/lib \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/drivers/soc/$(TARGET)/display \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/drivers/padctl

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_display.c \
	$(LOCAL_DIR)/tegrabl_display_unit.c \
	$(LOCAL_DIR)/nvdisp/tegrabl_nvdisp.c \
	$(LOCAL_DIR)/nvdisp/tegrabl_nvdisp_win.c \
	$(LOCAL_DIR)/nvdisp/tegrabl_nvdisp_cmu.c \
	$(LOCAL_DIR)/nvdisp/tegrabl_nvdisp_dump.c \
	$(LOCAL_DIR)/hdmi/tegrabl_hdmi.c \
	$(LOCAL_DIR)/dp/tegrabl_dp.c \
	$(LOCAL_DIR)/dp/tegrabl_dp_lt.c \
	$(LOCAL_DIR)/sor/tegrabl_sor.c \
	$(LOCAL_DIR)/sor/tegrabl_sor_dp.c \
	$(LOCAL_DIR)/edid/tegrabl_edid.c \
	$(LOCAL_DIR)/edid/tegrabl_modes.c \
	$(LOCAL_DIR)/edid/tegrabl_mode_selection.c \
	$(LOCAL_DIR)/platform_data/tegrabl_display_dtb.c \
	$(LOCAL_DIR)/platform_data/tegrabl_display_dtb_hdmi.c \
	$(LOCAL_DIR)/platform_data/tegrabl_display_dtb_dp.c \
	$(LOCAL_DIR)/platform_data/tegrabl_display_dtb_util.c \
	$(LOCAL_DIR)/platform_data/tegrabl_display_dtb_backlight.c \
	$(LOCAL_DIR)/backlight/lp8556.c \
	$(LOCAL_DIR)/panel/tegrabl_display_panel.c \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/drivers/soc/$(TARGET)/display/tegrabl_display_soc.c

include make/module.mk
