CC = gcc
CXX = g++

JACK = 1
SDL2 = 1
PORTAUDIO = 1
PULSE = 1

WINDOWS = 0
ARCH = x86_64

VERSION = 1.1.0

#CFLAGS += -O2 -march=armv6zk -mcpu=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -g -I/usr/local/include -I/opt/local/include -Wall -std=gnu99 -DDEBUG -DFASTSIN -Wdouble-promotion
DEBUG = -g -DDEBUG
CFLAGS += $(DEBUG)
LDFLAGS += $(DEBUG)
CFLAGS += -std=gnu99
CXXFLAGS += -std=c++17
FLAGS += -O2 -I/usr/local/include -I/opt/local/include -Wall -DFASTSIN -DSCALEPAN -MMD -pthread -DDIRT_VERSION_STRING="\"$(VERSION)\""
CXXFLAGS += -I../imgui -I../imgui/backends -I../imgui-filebrowser -DIMGUI_GIT_VERSION_STRING="\"$(shell cd ../imgui && git describe --tags --always --dirty=+)\"" -DIMGUI_FILE_BROWSER_GIT_VERSION_STRING="\"$(shell cd ../imgui-filebrowser && git describe --tags --always --dirty=+)\""
LDFLAGS += -lm -L/usr/local/lib -L/opt/local/lib -pthread

SOURCES = common.c audio.c file.c server.c jobqueue.c thpool.c

SOURCES_IMGUI = \
../imgui/imgui.cpp \
../imgui/imgui_draw.cpp \
../imgui/imgui_tables.cpp \
../imgui/imgui_widgets.cpp \
../imgui/backends/imgui_impl_sdl2.cpp \
../imgui/backends/imgui_impl_opengl2.cpp

PKGS = liblo sndfile samplerate

ifeq ($(WINDOWS),1)
CC = $(ARCH)-w64-mingw32-gcc
CXX = $(ARCH)-w64-mingw32-g++
PULSE = 0
JACK = 0
PKG_CONFIG_PATH = $(HOME)/opt/windows/posix/$(ARCH)/lib/pkgconfig
PKG_CONFIG_FLAGS = --static
FLAGS += -I../dirent_h -D__USE_MINGW_ANSI_STDIO=1 -DWINVER=0x501 -D_WIN32_WINNT=0x501
LDFLAGS += -lshlwapi -Wl,-Bstatic -lpthread -Wl,-Bdynamic -static-libgcc -static-libstdc++
EXEEXT = .exe
dirt$(EXEEXT): LDFLAGS += -mconsole
dirt-gui$(EXEEXT): LDFLAGS += -lopengl32 -mwindows
else
dirt-gui$(EXEEXT): LDFLAGS += -lGL
endif

ifeq ($(JACK),1)
FLAGS += -DJACK
PKGS += jack
SOURCES += jack.c
endif

ifeq ($(SDL2),1)
FLAGS += -DSDL2
PKGS += sdl2
SOURCES += sdl2.c
endif

ifeq ($(PORTAUDIO),1)
FLAGS += -DPORTAUDIO
PKGS += portaudio-2.0
SOURCES += portaudio.c
endif

ifeq ($(PULSE),1)
FLAGS += -DPULSE
PKGS += libpulse-simple
SOURCES += pulse.c
endif

ifeq ($(JACK),1)
CFLAGS += -DDEFAULT_OUTPUT="\"jack\""
else
ifeq ($(SDL2),1)
CFLAGS += -DDEFAULT_OUTPUT="\"sdl2\""
else
ifeq ($(PORTAUDIO),1)
CFLAGS += -DDEFAULT_OUTPUT="\"portaudio\""
else
ifeq ($(PULSE),1)
CFLAGS += -DDEFAULT_OUTPUT="\"pulse\""
endif
endif
endif
endif

FLAGS += `PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config $(PKG_CONFIG_FLAGS) --cflags $(PKGS)`
LDFLAGS += `PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config $(PKG_CONFIG_FLAGS) --libs $(PKGS)`

CFLAGS += $(FLAGS)
CXXFLAGS += $(FLAGS)

OBJECTS = $(SOURCES:.c=.o)
DEPENDS = $(OBJECTS:.o=.d)

OBJECTS_IMGUI = $(SOURCES_IMGUI:.cpp=.o)
DEPENDS_IMGUI = $(OBJECTS_IMGUI:.o=.d)

OBJECTS_EXTRA = dirt.o dirt-gui.o log-stdio.o log-imgui.o
DEPENDS_EXTRA = $(OBJECTS_EXTRA:.o=.d)

all: dirt$(EXEEXT) dirt-gui$(EXEEXT)

clean:
	rm -f $(DEPENDS) $(DEPENDS_IMGUI) $(DEPENDS_EXTRA) $(OBJECTS) $(OBJECTS_IMGUI) $(OBJECTS_EXTRA)

dirt-feedback: CFLAGS += -DFEEDBACK -DINPUT
dirt-feedback: dirt

dirt$(EXEEXT): dirt.o log-stdio.o $(OBJECTS) Makefile
	$(CC) dirt.o log-stdio.o $(OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@

dirt-gui$(EXEEXT): dirt-gui.o log-imgui.o $(OBJECTS) $(OBJECTS_IMGUI) Makefile
	$(CXX) dirt-gui.o log-imgui.o $(OBJECTS) $(OBJECTS_IMGUI) $(LDFLAGS) -o $@

install: dirt
	install -d $(PREFIX)/bin
	install -m 0755 dirt $(PREFIX)/bin/dirt

-include $(DEPENDS) $(DEPENDS_IMGUI) $(DEPENDS_EXTRA)
