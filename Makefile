ARCHS = arm64 arm64e
TARGET := iphone:clang:latest:15.0

THEOS_PACKAGE_SCHEME = roothide

include $(THEOS)/makefiles/common.mk

LIBRARY_NAME = roothidepatch

$(LIBRARY_NAME)_FILES = PatchLoader.c
$(LIBRARY_NAME)_CFLAGS = -fobjc-arc
$(LIBRARY_NAME)_INSTALL_PATH = /usr/lib

include $(THEOS_MAKE_PATH)/library.mk

clean::
	rm -rf ./packages/*