#Build ssg kernel driver

ENABLE_SECUREMSM_DLKM := true
ENABLE_SECUREMSM_QTEE_DLKM := true
ENABLE_QCEDEV_FE := false
ENABLE_SI_CORE_TEST := false

ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
  ifeq ($(TARGET_KERNEL_DLKM_SECURE_MSM_OVERRIDE), false)
      ENABLE_SECUREMSM_DLKM := false
  endif
  ifeq ($(TARGET_KERNEL_DLKM_SECUREMSM_QTEE_OVERRIDE), false)
      ENABLE_SECUREMSM_QTEE_DLKM := false
  endif
endif

#enable QCEDEV_FE driver only on Automotive Lemans HQX LA GVM.
ifeq ($(ENABLE_HYP),true)
  ifeq ($(TARGET_BOARD_PLATFORM),gen4)
    ifneq ($(TARGET_USES_GY), true)
      ENABLE_QCEDEV_FE := true
    endif #TARGET_USES_GY
  endif #TARGET_BOARD_PLATFORM
endif #ENABLE_HYP

ifeq ($(ENABLE_SECUREMSM_DLKM), true)
  ENABLE_QCRYPTO_DLKM := true
  ENABLE_HDCP_QSEECOM_DLKM := true
  ENABLE_QRNG_DLKM := true
  ifeq ($(TARGET_USES_SMMU_PROXY), true)
    ENABLE_SMMU_PROXY := true
  endif #TARGET_USES_SMMU_PROXY
endif #ENABLE_SECUREMSM_DLKM

ifeq ($(ENABLE_SECUREMSM_QTEE_DLKM), true)
  ENABLE_SMCINVOKE_DLKM := true
  ENABLE_TZLOG_DLKM := true
  #Enable Qseecom if TARGET_ENABLE_QSEECOM or TARGET_BOARD_AUTO is set to true
  ifneq (, $(filter true, $(TARGET_ENABLE_QSEECOM) $(TARGET_BOARD_AUTO)))
    ENABLE_QSEECOM_DLKM := true
  endif #TARGET_ENABLE_QSEECOM OR TARGET_BOARD_AUTO
endif #ENABLE_SECUREMSM_QTEE_DLKM

ifeq ($(TARGET_USES_GY), true)
  ENABLE_QCRYPTO_DLKM := false
  ENABLE_HDCP_QSEECOM_DLKM := false
  ENABLE_QRNG_DLKM := true
  ENABLE_SMMU_PROXY := false
  ENABLE_SMCINVOKE_DLKM := true
  ENABLE_TZLOG_DLKM := false
  ENABLE_QSEECOM_DLKM := false
endif #TARGET_USES_GY

ifeq ($(ENABLE_QCRYPTO_DLKM), true)
PRODUCT_PACKAGES += qcedev-mod_dlkm.ko
PRODUCT_PACKAGES += qce50_dlkm.ko
PRODUCT_PACKAGES += qcrypto-msm_dlkm.ko
endif #ENABLE_QCRYPTO_DLKM

ifeq ($(ENABLE_HDCP_QSEECOM_DLKM), true)
PRODUCT_PACKAGES += hdcp_qseecom_dlkm.ko
endif #ENABLE_HDCP_QSEECOM_DLKM

ifeq ($(ENABLE_QRNG_DLKM), true)
PRODUCT_PACKAGES += qrng_dlkm.ko
endif #ENABLE_QRNG_DLKM

ifeq ($(ENABLE_SMMU_PROXY), true)
PRODUCT_PACKAGES += smmu_proxy_dlkm.ko
endif #ENABLE_SMMU_PROXY

ifeq ($(ENABLE_SMCINVOKE_DLKM), true)
PRODUCT_PACKAGES += smcinvoke_dlkm.ko
endif #ENABLE_SMCINVOKE_DLKM

ifeq ($(ENABLE_SI_CORE_TEST), true)
PRODUCT_PACKAGES += si_core_test.ko
endif #ENABLE_SI_CORE_TEST

ifeq ($(ENABLE_TZLOG_DLKM), true)
PRODUCT_PACKAGES += tz_log_dlkm.ko
endif #ENABLE_TZLOG_DLKM

ifeq ($(ENABLE_QSEECOM_DLKM), true)
PRODUCT_PACKAGES += qseecom_dlkm.ko
endif #ENABLE_QSEECOM_DLKM

ifeq ($(ENABLE_QCEDEV_FE), true)
PRODUCT_PACKAGES += qcedev_fe_dlkm.ko
endif #ENABLE_QCEDEV_FE
