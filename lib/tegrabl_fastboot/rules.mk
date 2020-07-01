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
	$(LOCAL_DIR)/../../include \
	$(LOCAL_DIR)/../../include/lib

MODULE_DEPS += \
	$(LOCAL_DIR)/../../../common/lib/linuxboot \
	$(LOCAL_DIR)/../../../common/drivers/usbf/class/transport \
	$(LOCAL_DIR)/../../../common/drivers/usbf/xusbf \
	$(LOCAL_DIR)/../../../common/lib/tegrabl_sparse

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_fastboot_partinfo.c \
	$(LOCAL_DIR)/tegrabl_fastboot_protocol.c \
	$(LOCAL_DIR)/tegrabl_fastboot_oem.c \
	$(LOCAL_DIR)/tegrabl_fastboot_a_b.c

include make/module.mk

