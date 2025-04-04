ifeq ($(TARGET_DATARMNET_EXT_ENABLE), true)
ifneq ($(TARGET_BOARD_AUTO),true)
ifneq ($(TARGET_BOARD_PLATFORM),qssi)

RMNET_SCH_DLKM_PLATFORMS_LIST := pineapple
RMNET_SCH_DLKM_PLATFORMS_LIST += sun

ifeq ($(call is-board-platform-in-list, $(RMNET_SCH_DLKM_PLATFORMS_LIST)),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#Enabling BAZEL
LOCAL_MODULE_DDK_BUILD := true

LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)

LOCAL_MODULE := rmnet_sch.ko
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)

DLKM_DIR := $(TOP)/device/qcom/common/dlkm

include $(DLKM_DIR)/Build_external_kernelmodule.mk

endif #End of check for target
endif #End of Check for qssi target
endif #End of check for AUTO Target
endif #End of Check for datarmnet