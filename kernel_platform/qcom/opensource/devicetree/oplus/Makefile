SUN_BASE_DTB += ../qcom/sun.dtb ../qcom/sun-v2.dtb ../qcom/sunp.dtb ../qcom/sunp-v2.dtb \
                ../qcom/sun-tp.dtb ../qcom/sun-tp-v2.dtb ../qcom/sunp-tp.dtb ../qcom/sunp-tp-v2.dtb

OPLUS_SUN_BOARDS += petrel-24001-sun-overlay.dtbo\
                    petrel-24201-sun-overlay.dtbo\
                    petrel-24001-sun-overlay-T0.dtbo\
                    petrel-24201-sun-overlay-T0.dtbo\
                    petrel-24001-sun-overlay-EVT1.dtbo\
                    petrel-24002-sun-overlay-EVT1.dtbo\
                    petrel-24201-sun-overlay-EVT1.dtbo\
                    petrel-24001-sun-overlay-EVT2.dtbo\
                    petrel-24002-sun-overlay-EVT2.dtbo\
                    petrel-24201-sun-overlay-EVT2.dtbo\
                    zhufeng-24019-sun-overlay.dtbo\
                    zhufeng-24018-sun-overlay.dtbo\
                    petrel-24002-sun-overlay.dtbo\
                    zhufeng-24019-sun-overlay-PreT0.dtbo\
                    zhufeng-24018-sun-overlay-PreT0.dtbo\
                    erhai-24924-sun-overlay.dtbo\
                    erhai-24924-sun-overlay-DVT.dtbo


dtb-y += $(OPLUS_SUN_BOARDS) $(SUN_BASE_DTB)

always-y        := $(dtb-y)
subdir-y        := $(dts-dirs)
clean-files     := *.dtb *.dtbo
