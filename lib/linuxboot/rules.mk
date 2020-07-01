#
# Copyright (c) 2015-2018, NVIDIA Corporation.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from NVIDIA Corporation
# is strictly prohibited.
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR) \
	$(LOCAL_DIR)/../../include \
	$(LOCAL_DIR)/../../include/lib \
	$(LOCAL_DIR)/../../include/soc/$(TARGET) \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/include/lib \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/include/soc/$(TARGET) \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/include/drivers \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/nvtboot/common/soc/$(TARGET)/include

MODULE_DEPS += \
	$(LOCAL_DIR)/../libfdt \
	$(LOCAL_DIR)/../devicetree \
	$(LOCAL_DIR)/../board_info \
	$(LOCAL_DIR)/../plugin_manager \
	$(LOCAL_DIR)/../external/libufdt \
	$(LOCAL_DIR)/../odmdata \
	$(LOCAL_DIR)/../../../t18x/common/lib/partitionloader \
	$(LOCAL_DIR)/../decompress

ifneq ($(filter t18x, $(TARGET_FAMILY)),)
MODULE_DEPS += \
	$(LOCAL_DIR)/../a_b_boot
endif

ifneq ($(filter t19x, $(TARGET_FAMILY)),)
ALLMODULE_OBJS += $(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/lib/linuxboot/$(TARGET)/prebuilt/vpr.mod.o
endif

MODULE_SRCS += \
	$(LOCAL_DIR)/cmdline.c \
	$(LOCAL_DIR)/dtb_update.c \
	$(LOCAL_DIR)/dtb_overlay.c \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/lib/linuxboot/$(TARGET)/linuxboot_helper.c \
	$(LOCAL_DIR)/linux_load.c

include make/module.mk
