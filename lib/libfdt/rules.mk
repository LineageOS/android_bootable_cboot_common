#
# Copyright (c) 2015 - 2016, NVIDIA Corporation.  All Rights Reserved.
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
	$(LOCAL_DIR)/../../include/lib

MODULE_SRCS += \
	$(LOCAL_DIR)/fdt.c \
	$(LOCAL_DIR)/fdt_ro.c \
	$(LOCAL_DIR)/fdt_rw.c \
	$(LOCAL_DIR)/fdt_strerror.c \
	$(LOCAL_DIR)/fdt_sw.c \
	$(LOCAL_DIR)/fdt_wip.c

include make/module.mk
