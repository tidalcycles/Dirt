CC=gcc

JACK = 1
SDL2 = 1
PORTAUDIO = 1
PULSE = 1

WINDOWS = 0
ARCH = x86_64

#CFLAGS += -O2 -march=armv6zk -mcpu=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -g -I/usr/local/include -I/opt/local/include -Wall -std=gnu99 -DDEBUG -DHACK -DFASTSIN -Wdouble-promotion
DEBUG = -g -DDEBUG
CFLAGS += $(DEBUG)
LDFLAGS += $(DEBUG)
CFLAGS += -O2 -I/usr/local/include -I/opt/local/include -Wall -std=gnu99 -DHACK -DFASTSIN -DSCALEPAN -MMD -pthread
LDFLAGS += -lm -L/usr/local/lib -L/opt/local/lib -pthread

SOURCES = dirt.c common.c audio.c file.c server.c jobqueue.c thpool.c log-stdio.c

PKGS = liblo sndfile samplerate

ifeq ($(WINDOWS),1)
PKG_CONFIG_PATH = $(HOME)/opt/windows/posix/$(ARCH)/lib/pkgconfig
PKG_CONFIG_FLAGS = --static
CFLAGS += -I../dirent_h
LDFLAGS += -lshlwapi -Wl,-Bstatic -lpthread -Wl,-Bdynamic
EXEEXT = .exe
endif

ifeq ($(JACK),1)
CFLAGS += -DJACK
PKGS += jack
SOURCES += jack.c
endif

ifeq ($(SDL2),1)
CFLAGS += -DSDL2
PKGS += sdl2
SOURCES += sdl2.c
endif

ifeq ($(PORTAUDIO),1)
CFLAGS += -DPORTAUDIO
PKGS += portaudio-2.0
SOURCES += portaudio.c
endif

ifeq ($(PULSE),1)
CFLAGS += -DPULSE
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

CFLAGS += `PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config $(PKG_CONFIG_FLAGS) --cflags $(PKGS)`
LDFLAGS += `PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config $(PKG_CONFIG_FLAGS) --libs $(PKGS)`

OBJECTS = $(SOURCES:.c=.o)
DEPENDS = $(OBJECTS:.o=.d)

all: dirt$(EXEEXT)

clean:
	rm -f *.d *.o *~ dirt

dirt-feedback: CFLAGS += -DFEEDBACK -DINPUT
dirt-feedback: dirt

dirt$(EXEEXT): $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@

install: dirt
	install -d $(PREFIX)/bin
	install -m 0755 dirt $(PREFIX)/bin/dirt

-include $(DEPENDS)
