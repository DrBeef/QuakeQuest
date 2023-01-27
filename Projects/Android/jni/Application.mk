APP_PLATFORM := android-24

APP_CFLAGS += -Wl,--no-undefined

APPLICATIONMK_PATH = $(call my-dir)
NDK_MODULE_PATH := $(APPLICATIONMK_PATH)/../..

TOP_DIR			:= $(APPLICATIONMK_PATH)
SUPPORT_LIBS	:= $(APPLICATIONMK_PATH)/SupportLibs

APP_STL := c++_shared

# Make sure every shared lib includes a .note.gnu.build-id header, for crash reporting
APP_LDFLAGS := -Wl,--build-id

NDK_TOOLCHAIN_VERSION := clang

# ndk-r14 introduced failure for missing dependencies. If 'false', the clean
# step will error as we currently remove prebuilt artifacts on clean.
APP_ALLOW_MISSING_DEPS=true