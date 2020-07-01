#
# Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
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
	$(LOCAL_DIR)/tegrabl_sdmmc_bdev.c \
	$(LOCAL_DIR)/tegrabl_sdmmc_protocol.c \
	$(LOCAL_DIR)/tegrabl_sdmmc_host.c \
	$(LOCAL_DIR)/tegrabl_sdmmc_rpmb.c \
	$(LOCAL_DIR)/tegrabl_sdmmc_protocol_rpmb.c

MODULE_SRCS += \
	$(LOCAL_DIR)/tegrabl_sd_bdev.c \
	$(LOCAL_DIR)/tegrabl_sd_protocol.c \
	$(LOCAL_DIR)/tegrabl_sd_pdata.c \
	$(LOCAL_DIR)/tegrabl_sd_card.c

include make/module.mk
