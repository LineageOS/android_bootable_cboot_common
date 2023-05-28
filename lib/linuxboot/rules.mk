#
# Copyright (c) 2015-2019, NVIDIA Corporation.  All Rights Reserved.
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
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/nvtboot/common/soc/$(TARGET)/include \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/nvtboot/bpmp/soc/$(TARGET)/include \
	$(LOCAL_DIR)/../../../t18x/cboot/lib/lwip/include

MODULE_DEPS += \
	$(LOCAL_DIR)/../libfdt \
	$(LOCAL_DIR)/../devicetree \
	$(LOCAL_DIR)/../board_info \
	$(LOCAL_DIR)/../odmdata \
	$(LOCAL_DIR)/../../../t18x/common/lib/partitionloader \
	$(LOCAL_DIR)/../decompress \
	$(LOCAL_DIR)/../file_manager

ifneq ($(NVDISP_INIT_ONLY),true)
MODULE_DEPS += \
	$(LOCAL_DIR)/../plugin_manager \
	$(LOCAL_DIR)/../external/libufdt
endif

ifneq ($(filter t18x, $(TARGET_FAMILY)),)
MODULE_DEPS += \
	$(LOCAL_DIR)/../a_b_boot
endif

ifneq ($(filter t19x, $(TARGET_FAMILY)),)
ALLMODULE_OBJS += $(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/lib/linuxboot/$(TARGET)/prebuilt/vpr.mod.o
endif

MODULE_SRCS += \
	$(LOCAL_DIR)/cmdline.c \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/lib/linuxboot/$(TARGET)/linuxboot_helper.c \
	$(LOCAL_DIR)/linuxboot_utils.c \
	$(LOCAL_DIR)/fixed_boot.c \
	$(LOCAL_DIR)/linux_load.c

ifneq ($(NVDISP_INIT_ONLY),true)
MODULE_SRCS += \
	$(LOCAL_DIR)/dtb_update.c \
	$(LOCAL_DIR)/dtb_overlay.c
endif

ifneq ($(filter t19x, $(TARGET_FAMILY)),)
MODULE_SRCS += \
	$(LOCAL_DIR)/extlinux_boot.c

ifneq ($(NVDISP_INIT_ONLY),true)
MODULE_SRCS += \
	$(LOCAL_DIR)/removable_boot.c \
	$(LOCAL_DIR)/net_boot.c
endif
endif

include make/module.mk
