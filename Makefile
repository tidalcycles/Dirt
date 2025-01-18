CC=gcc

#CFLAGS += -O2 -march=armv6zk -mcpu=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -g -I/usr/local/include -I/opt/local/include -Wall -std=gnu99 -DDEBUG -DHACK -DFASTSIN -Wdouble-promotion
CFLAGS += -O2 -g -I/usr/local/include -I/opt/local/include -Wall -std=gnu99 -DDEBUG -DHACK -DFASTSIN -MMD

LDFLAGS += -g -lm -L/usr/local/lib -L/opt/local/lib -llo -lsndfile -lsamplerate -lpthread 

SOURCES=dirt.c common.c audio.c file.c server.c jobqueue.c thpool.c 
OBJECTS=$(SOURCES:.c=.o)
DEPENDS=$(OBJECTS:.o=.d)

dirt: CFLAGS += -DJACK -DSCALEPAN
dirt: LDFLAGS += -ljack
dirt-pa: LDFLAGS += -lportaudio
dirt-pulse: CFLAGS += -DPULSE `pkg-config --cflags libpulse-simple`
dirt-pulse: LDFLAGS += `pkg-config --libs libpulse-simple` -lpthread
dirt-feedback: CFLAGS += -DFEEDBACK -DINPUT
dirt-feedback: dirt

clean:
	rm -f *.o *~ dirt dirt-analyse dirt-pa

all: dirt

dirt: $(OBJECTS) jack.o Makefile
	$(CC) $(OBJECTS) jack.o $(CFLAGS) $(LDFLAGS) -o $@

dirt-pa: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@

dirt-pulse: dirt.o common.o audio.o file.o server.o Makefile
	$(CC) dirt.o common.o audio.o file.o server.o $(CFLAGS) $(LDFLAGS) -o dirt-pulse

test: test.c Makefile
	$(CC) test.c -llo -o test

install: dirt
	install -d $(PREFIX)/bin
	install -m 0755 dirt $(PREFIX)/bin/dirt

-include $(DEPENDS)
