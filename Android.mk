LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := liblo
LOCAL_SRC_FILES := $(LOCAL_PATH)/$(TARGET_ARCH_ABI)/lib/liblo.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libsamplerate
LOCAL_SRC_FILES := $(LOCAL_PATH)/$(TARGET_ARCH_ABI)/lib/libsamplerate.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libsndfile
LOCAL_SRC_FILES := $(LOCAL_PATH)/$(TARGET_ARCH_ABI)/lib/libsndfile.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := main
SDL_PATH := ../SDL
LOCAL_C_INCLUDES := \
$(LOCAL_PATH)/$(SDL_PATH)/include \
$(LOCAL_PATH)/$(TARGET_ARCH_ABI)/include \
$(LOCAL_PATH)/imgui \
$(LOCAL_PATH)/imgui/backends \
$(LOCAL_PATH)/imgui/misc/cpp \
$(LOCAL_PATH)/imgui-filebrowser \

VERSION = 1.1.0

LOCAL_CFLAGS := \
-fPIC \
-DSDL2
-DDIRT_VERSION_STRING="\"$(VERSION)\"" \
-DIMGUI_USER_CONFIG="\"dirt-imconfig.h\"" \
-DIMGUI_GIT_VERSION_STRING="\"$(shell cd $(LOCAL_PATH)/imgui && git describe --tags --always --dirty=+)\"" \

LOCAL_CPPFLAGS := -std=c++2a -Wno-documentation-unknown-command
LOCAL_CPP_FEATURES := 
LOCAL_CPP_EXTENSION := .cpp .cc

LOCAL_SRC_FILES := \
android.cc \
audio.c \
common.c \
file.c \
jobqueue.c \
server.c \
thpool.c \
sdl2.c \
imgui/imgui.cpp \
imgui/imgui_demo.cpp \
imgui/imgui_draw.cpp \
imgui/imgui_tables.cpp \
imgui/imgui_widgets.cpp \
imgui/backends/imgui_impl_sdl2.cpp \
imgui/backends/imgui_impl_opengl3.cpp \
imgui/misc/cpp/imgui_stdlib.cpp \

LOCAL_SHARED_LIBRARIES := SDL2
LOCAL_STATIC_LIBRARIES := lo samplerate sndfile
LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lGLESv3 -llog
#ifneq ($(TARGET_ARCH_ABI), arm64-v8a)
#LOCAL_LDFLAGS := -Wl,--no-warn-shared-textrel
#endif
include $(BUILD_SHARED_LIBRARY)

