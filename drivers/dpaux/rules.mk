#
# Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR) \
	$(LOCAL_DIR)/../../include/soc/$(TARGET) \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/drivers/soc/$(TARGET)/dpaux \

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_dpaux.c \
	$(LOCAL_DIR)/../../../$(TARGET_FAMILY)/common/drivers/soc/$(TARGET)/dpaux/tegrabl_dpaux_soc.c

include make/module.mk
