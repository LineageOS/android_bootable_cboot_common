#
# Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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
	$(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/usbh.c \
	$(LOCAL_DIR)/xhci.c \
	$(LOCAL_DIR)/xhci_pad_ctrl.c \
	$(LOCAL_DIR)/tegrabl_xusbh_fw.c \
	$(LOCAL_DIR)/tegrabl_xusbh_scsi_storage_test.c \
	$(LOCAL_DIR)/hub.c

include make/module.mk
