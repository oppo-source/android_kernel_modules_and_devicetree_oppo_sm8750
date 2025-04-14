dtbo-$(CONFIG_ARCH_SUN)   := sun-camera.dtbo
# dtbo-$(CONFIG_ARCH_SUN)   += sun-camera-sensor-mtp.dtbo \
# 				sun-camera-sensor-rumi.dtbo \
# 				sun-camera-sensor-cdp.dtbo  \
# 				sun-camera-sensor-qrd.dtbo

dtbo-$(CONFIG_ARCH_SUN)   += oplus/zhufeng-camera-overlay-evb.dtbo \
			     oplus/zhufeng-camera-overlay-preT0.dtbo \
			     oplus/zhufeng-camera-overlay-T0.dtbo \
			     oplus/zhufeng-camera-overlay-EVT.dtbo \
			     oplus/zhufeng-camera-overlay-DVT.dtbo \


dtbo-$(CONFIG_ARCH_SUN)   += oplus/petrel-camera-overlay-evb.dtbo \
			     oplus/petrel-camera-overlay-T0.dtbo \

dtbo-$(CONFIG_ARCH_SUN)   += oplus/erhai-camera-overlay-evb.dtbo \

