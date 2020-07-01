#
# Copyright (c) 2015-2017, NVIDIA Corporation.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from NVIDIA Corporation
# is strictly prohibited.
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_DEPS += \
	$(LOCAL_DIR)/../libfdt \
	$(LOCAL_DIR)/../tegrabl_devicetree \
	$(LOCAL_DIR)/../tegrabl_decompress \
	$(LOCAL_DIR)/../tegrabl_board_info \
	$(LOCAL_DIR)/../../../t18x/common/lib/odmdata \
	$(LOCAL_DIR)/../../../t18x/common/lib/a_b_boot_control \
	$(LOCAL_DIR)/../../../t18x/common/lib/partitionloader

GLOBAL_INCLUDES += \
	$(LOCAL_DIR) \
	$(LOCAL_DIR)/../../include \
	$(LOCAL_DIR)/../../include/lib \
	$(LOCAL_DIR)/../../include/soc/$(TARGET) \
	$(LOCAL_DIR)/../../../t18x/common/include/soc/$(TARGET) \
	$(LOCAL_DIR)/../../../t18x/common/include/drivers \
	$(LOCAL_DIR)/../../../t18x/nvtboot/common/soc/$(TARGET)/include

MODULE_SRCS += \
	$(LOCAL_DIR)/cmdline.c \
	$(LOCAL_DIR)/dtb_update.c \
	$(LOCAL_DIR)/../../../t18x/common/lib/linuxboot/$(TARGET)/linuxboot_helper.c \
	$(LOCAL_DIR)/linux_load.c

include make/module.mk

