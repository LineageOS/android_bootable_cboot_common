#
# Copyright (c) 2015 - 2017, NVIDIA CORPORATION.  All Rights Reserved.
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
	$(LOCAL_DIR)/include \
	$(LOCAL_DIR) \
	$(LOCAL_DIR)/../../../../$(TARGET_FAMILY)/common/drivers/soc/$(TARGET)/usbf/xusbf


MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_xusbf.c \
	$(LOCAL_DIR)/../../../../$(TARGET_FAMILY)/common/drivers/soc/$(TARGET)/usbf/xusbf/tegrabl_xusbf_soc.c

GLOBAL_DEFINES += \
	XUSBF_DEBUG=0

include make/module.mk
