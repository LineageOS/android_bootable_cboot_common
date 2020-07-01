#
# Copyright (c) 2016-2017, NVIDIA Corporation.  All Rights Reserved.
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
	$(LOCAL_DIR)/../../../../common/include \
	$(LOCAL_DIR)/../../../../common/include/lib

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/../../include/lib \
	$(LOCAL_DIR)/../../include/soc/$(TARGET) \
	$(LOCAL_DIR)/../../../../../hardware/nvidia/hwinc/$(TARGET_FAMILY)

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_odmdata.c \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/lib/odmdata/odmdata.c

include make/module.mk

