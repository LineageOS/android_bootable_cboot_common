LOCAL_DIR := $(GET_LOCAL_DIR)

LIBAVB_DIR := $(LOCAL_DIR)/../../../../../../external/avb/libavb

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/../../../../../../external/avb \
	$(LOCAL_DIR)/../../../../../../external/avb/libavb/sha

MODULE_CFLAGS := \
        -Wno-error=maybe-uninitialized \
        -Wno-error=format

MODULE_SRCS += \
        $(LIBAVB_DIR)/avb_chain_partition_descriptor.c \
        $(LIBAVB_DIR)/avb_cmdline.c \
        $(LIBAVB_DIR)/avb_crc32.c \
        $(LIBAVB_DIR)/avb_crypto.c \
        $(LIBAVB_DIR)/avb_descriptor.c \
        $(LIBAVB_DIR)/avb_footer.c \
        $(LIBAVB_DIR)/avb_hash_descriptor.c \
        $(LIBAVB_DIR)/avb_hashtree_descriptor.c \
        $(LIBAVB_DIR)/avb_kernel_cmdline_descriptor.c \
        $(LIBAVB_DIR)/avb_property_descriptor.c \
        $(LIBAVB_DIR)/avb_rsa.c \
        $(LIBAVB_DIR)/avb_slot_verify.c \
        $(LIBAVB_DIR)/avb_sysdeps_posix.c \
        $(LIBAVB_DIR)/avb_util.c \
        $(LIBAVB_DIR)/avb_vbmeta_image.c \
        $(LIBAVB_DIR)/avb_version.c \
        $(LIBAVB_DIR)/sha/sha256_impl.c \
        $(LIBAVB_DIR)/sha/sha512_impl.c

include make/module.mk
