ARCHS = arm64 arm64e
TARGET := iphone:clang:latest:15.0

THEOS_PACKAGE_SCHEME = roothide

include $(THEOS)/makefiles/common.mk

LIBRARY_NAME = roothidepatch

PatchLoader_FILES = PatchLoader.c
PatchLoader_CFLAGS = -fobjc-arc
PatchLoader_INSTALL_PATH = /usr/lib

include $(THEOS_MAKE_PATH)/library.mk

clean::
	rm -rf ./packages/*