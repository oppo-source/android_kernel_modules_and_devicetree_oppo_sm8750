ifeq ($(CONFIG_ARCH_SUN),y)
dtbo-y += sun-ese-mtp.dtbo
dtbo-y += sun-ese-rcm-v8.dtbo
dtbo-y += sun-ese-rcm.dtbo
dtbo-y += sun-ese-mtp-kiwi-v8.dtbo
dtbo-y += sun-ese-cdp-kiwi-v8.dtbo
dtbo-y += sun-ese-atp.dtbo
dtbo-y += sun-ese-qrd-sku1.dtbo
dtbo-y += sun-ese-qrd-sku1-v8.dtbo
dtbo-y += sun-ese-qrd-sku2-v8.dtbo
dtbo-y += sun-ese-cdp.dtbo
dtbo-y += sun-ese-qrd.dtbo
dtbo-y += sun-v2-ese-mtp.dtbo
dtbo-y += sun-v2-ese-cdp.dtbo
dtbo-y += sun-v2-ese-qrd.dtbo
endif

always-y	:= $(dtb-y) $(dtbo-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
