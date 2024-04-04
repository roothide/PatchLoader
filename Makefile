ARCHS = arm64 arm64e
TARGET := iphone:clang:latest:15.0

THEOS_PACKAGE_SCHEME = roothide

FINALPACK = 1
DEBUG ?= 0

include $(THEOS)/makefiles/common.mk

LIBRARY_NAME = roothidepatch

$(LIBRARY_NAME)_FILES = PatchLoader.c
# $(LIBRARY_NAME)_CFLAGS = -fobjc-arc
$(LIBRARY_NAME)_INSTALL_PATH = /usr/lib

include $(THEOS_MAKE_PATH)/library.mk

MIRROR_PATH=/var/mobile/Library/pkgmirror

before-package::
	mkdir -p $(THEOS_STAGING_DIR)/var
	ln -s / $(THEOS_STAGING_DIR)/var/jb

	mkdir -p $(THEOS_STAGING_DIR)/$(MIRROR_PATH)/var
	ln -s $(MIRROR_PATH) $(THEOS_STAGING_DIR)/$(MIRROR_PATH)/var/jb

	mkdir -p $(THEOS_STAGING_DIR)/$(MIRROR_PATH)/usr/lib
	ln -s $(MIRROR_PATH)/Library/MobileSubstrate/DynamicLibraries $(THEOS_STAGING_DIR)/$(MIRROR_PATH)/usr/lib/TweakInject

clean::
	rm -rf ./packages/*
	