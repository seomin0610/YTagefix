TARGET := iphone:clang:latest:14.0
ARCHS = arm64

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = AgeFix
AgeFix_FILES = AgeFix.c
AgeFix_CFLAGS = -Wall
AgeFix_USE_MODULES = 0

include $(THEOS_MAKE_PATH)/tweak.mk
