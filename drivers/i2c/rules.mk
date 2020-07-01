#
# Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)
MODULE_DEPS += \
	$(LOCAL_DIR)/../dpaux

GLOBAL_INCLUDES += \
	$(LOCAL_DIR) \
	$(LOCAL_DIR)/../../include/soc/$(TARGET) \
    $(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/drivers/soc/$(TARGET)/i2c \
    $(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/lib/bpmp-abi

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_i2c.c \
	$(LOCAL_DIR)/tegrabl_i2c_bpmpfw.c \
    $(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/drivers/soc/$(TARGET)/i2c/tegrabl_i2c_soc.c

include make/module.mk
