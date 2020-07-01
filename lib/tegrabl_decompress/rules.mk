#
# Copyright (c) 2016, NVIDIA Corporation.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from NVIDIA Corporation
# is strictly prohibited.
#

LOCAL_DIR := $(GET_LOCAL_DIR)
EXTERNAL_LIB_DIR := $(LOCAL_DIR)/../external

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
		$(LOCAL_DIR)/tegrabl_decompress.c

GLOBAL_INCLUDES += \
		$(LOCAL_DIR)/$(TARGET) \
		../common/include \
		../common/include/lib

ifeq ($(TARGET), t210)
MODULE_DEPS += \
		$(LOCAL_DIR)/../tegrabl_utils

# This macro is used to instruct the common header file
# tegrabl_decompress_private.h whether the build is for cboot or nvtboot.
# That in turn helps the header file to alias the calls to stdlib APIs to
# respective implementations
MODULE_DEFINES := CBOOT_WAR_DECOMP=1
GLOBAL_DEFINES += NO_BUILD_CONFIG=1
endif


# enable decompress lib as per need
TEGRABL_DECOMPRESSOR_ZLIB := yes
#TEGRABL_DECOMPRESSOR_LZF := yes
TEGRABL_DECOMPRESSOR_LZ4 := yes

ifeq ($(TEGRABL_DECOMPRESSOR_LZF), yes)
MODULE_DEFINES += CONFIG_ENABLE_LZF=1

MODULE_SRCS += \
		$(EXTERNAL_LIB_DIR)/lzf/lzf_d.c \
		$(LOCAL_DIR)/tegrabl_lzf_decompress.c

GLOBAL_INCLUDES += $(EXTERNAL_LIB_DIR)/lzf
endif

ifeq ($(TEGRABL_DECOMPRESSOR_ZLIB), yes)
MODULE_DEFINES += CONFIG_ENABLE_ZLIB=1

MODULE_SRCS +=          \
		$(EXTERNAL_LIB_DIR)/zlib/adler32.c   \
		$(EXTERNAL_LIB_DIR)/zlib/inffast.c   \
		$(EXTERNAL_LIB_DIR)/zlib/inftrees.c  \
		$(EXTERNAL_LIB_DIR)/zlib/inflate.c   \
		$(EXTERNAL_LIB_DIR)/zlib/zutil.c     \
		$(LOCAL_DIR)/tegrabl_zlib_decompress.c

GLOBAL_INCLUDES += $(EXTERNAL_LIB_DIR)/zlib
endif

ifeq ($(TEGRABL_DECOMPRESSOR_LZ4), yes)
MODULE_DEFINES += CONFIG_ENABLE_LZ4=1

MODULE_SRCS +=      \
		$(EXTERNAL_LIB_DIR)/lz4/lz4.c \
		$(LOCAL_DIR)/tegrabl_lz4_decompress.c

GLOBAL_INCLUDES += $(EXTERNAL_LIB_DIR)/lz4
endif

include make/module.mk

