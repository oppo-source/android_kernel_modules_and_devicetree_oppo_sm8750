#for AWINIC AW8697 Haptic

ccflags-y += -DOPLUS_FEATURE_CHG_BASIC

obj-$(CONFIG_AW8697_HAPTIC) += aw8697.o
obj-$(CONFIG_AW8697_HAPTIC)	+= haptic.o
haptic-objs			:= aw8692x.o aw869xx.o haptic_hv.o

obj-$(CONFIG_HAPTIC_FEEDBACK)	+= haptic_feedback.o