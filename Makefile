CC=gcc

JACK = 1
PORTAUDIO = 1
PULSE = 1
SDL2 = 1

#CFLAGS += -O2 -march=armv6zk -mcpu=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -g -I/usr/local/include -I/opt/local/include -Wall -std=gnu99 -DDEBUG -DHACK -DFASTSIN -Wdouble-promotion
CFLAGS += -O2 -g -I/usr/local/include -I/opt/local/include -Wall -std=gnu99 -DDEBUG -DHACK -DFASTSIN -DSCALEPAN -MMD
LDFLAGS += -g -lm -L/usr/local/lib -L/opt/local/lib -llo -lsndfile -lsamplerate -lpthread 

SOURCES = dirt.c common.c audio.c file.c server.c jobqueue.c thpool.c log-stdio.c

ifeq ($(JACK),1)
CFLAGS += -DJACK
LDFLAGS += -ljack
SOURCES += jack.c
endif

ifeq ($(PORTAUDIO),1)
CFLAGS += -DPORTAUDIO
LDFLAGS += -lportaudio
SOURCES += portaudio.c
endif

ifeq ($(PULSE),1)
CFLAGS += -DPULSE `pkg-config --cflags libpulse-simple`
LDFLAGS += `pkg-config --libs libpulse-simple` -lpthread
SOURCES += pulse.c
endif

ifeq ($(SDL2),1)
CFLAGS += -DSDL2 `pkg-config --cflags sdl2`
LDFLAGS += `pkg-config --libs sdl2`
SOURCES += sdl2.c
endif

ifeq ($(JACK),1)
CFLAGS += -DDEFAULT_OUTPUT="\"jack\""
else
ifeq ($(PORTAUDIO),1)
CFLAGS += -DDEFAULT_OUTPUT="\"portaudio\""
else
ifeq ($(PULSE),1)
CFLAGS += -DDEFAULT_OUTPUT="\"pulse\""
else
ifeq ($(SDL2),1)
CFLAGS += -DDEFAULT_OUTPUT="\"sdl2\""
endif
endif
endif
endif

OBJECTS=$(SOURCES:.c=.o)
DEPENDS=$(OBJECTS:.o=.d)

all: dirt

clean:
	rm -f *.d *.o *~ dirt

dirt-feedback: CFLAGS += -DFEEDBACK -DINPUT
dirt-feedback: dirt

dirt: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@

install: dirt
	install -d $(PREFIX)/bin
	install -m 0755 dirt $(PREFIX)/bin/dirt

-include $(DEPENDS)
