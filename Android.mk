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
-Wno-documentation-unknown-command \
-DSDL2 \

LOCAL_CPPFLAGS := \
-fPIC \
-std=c++17 \
-Wno-documentation-unknown-command \
-DSDL2 \
-DDIRT_VERSION_STRING="\"$(VERSION)\"" \
-DIMGUI_USER_CONFIG="\"dirt-imconfig.h\"" \
-DIMGUI_GIT_VERSION_STRING="\"$(shell cd $(LOCAL_PATH)/imgui && git describe --tags --always --dirty=+)\"" \
-DIMGUI_FILE_BROWSER_GIT_VERSION_STRING="\"$(shell cd $(LOCAL_PATH)/imgui-filebrowser && git describe --tags --always --dirty=+)\"" \

LOCAL_CPP_FEATURES := exceptions
LOCAL_CPP_EXTENSION := .cpp .cc

LOCAL_SRC_FILES := \
audio.c \
common.c \
dirt-gui.cc \
file.c \
jobqueue.c \
log-imgui.cc \
sdl2.c \
server.c \
thpool.c \
imgui/imgui.cpp \
imgui/imgui_draw.cpp \
imgui/imgui_tables.cpp \
imgui/imgui_widgets.cpp \
imgui/backends/imgui_impl_sdl2.cpp \
imgui/backends/imgui_impl_opengl3.cpp \

LOCAL_SHARED_LIBRARIES := SDL2
LOCAL_STATIC_LIBRARIES := lo samplerate sndfile
LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lGLESv3 -llog
#ifneq ($(TARGET_ARCH_ABI), arm64-v8a)
#LOCAL_LDFLAGS := -Wl,--no-warn-shared-textrel
#endif
include $(BUILD_SHARED_LIBRARY)

