#
# Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
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
	$(LOCAL_DIR)/micron \
	$(LOCAL_DIR)/spansion \
	$(LOCAL_DIR)/macronix

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_qspi_flash.c \
	$(LOCAL_DIR)/micron/tegrabl_qspi_flash_micron.c \
	$(LOCAL_DIR)/spansion/tegrabl_qspi_flash_spansion.c \
	$(LOCAL_DIR)/macronix/tegrabl_qspi_flash_macronix.c

include make/module.mk
