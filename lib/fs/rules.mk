LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/ext2 \
	$(LOCAL_DIR)/ext4 \
	$(LOCAL_DIR)/../include/lib

MODULE_DEPS += \
	$(LOCAL_DIR)/ext2 \
	$(LOCAL_DIR)/ext4

MODULE_SRCS += \
	$(LOCAL_DIR)/fs.c

#EXTRA_LINKER_SCRIPTS += $(LOCAL_DIR)/fs.ld

include make/module.mk
