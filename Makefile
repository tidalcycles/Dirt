CC=gcc

CFLAGS += -g -I/usr/local/include -Wall -O3 -std=gnu99
LDFLAGS += -lm -L/usr/local/lib -llo -lsndfile -lsamplerate -lpthread

SOURCES=dirt.c common.c audio.c file.c server.c jobqueue.c thpool.c
OBJECTS=$(SOURCES:.c=.o)

dirt: CFLAGS += -DJACK -DSCALEPAN
dirt: LDFLAGS += -ljack
dirt-pa: LDFLAGS += -lportaudio
dirt-pulse: CFLAGS += -DPULSE `pkg-config --cflags libpulse-simple`
dirt-pulse: LDFLAGS += `pkg-config --libs libpulse-simple` -lpthread
dirt-feedback: CFLAGS += -DFEEDBACK -DINPUT
dirt-feedback: dirt
all: dirt

clean:
	rm -f *.o *~ dirt dirt-analyse

dirt: $(OBJECTS) jack.o Makefile
	$(CC) $(OBJECTS) jack.o $(CFLAGS) $(LDFLAGS) -o $@

dirt-pa: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@

dirt-pulse: dirt.o common.o audio.o file.o server.o  Makefile
	$(CC) dirt.o common.o audio.o file.o server.o $(CFLAGS) $(LDFLAGS) -o dirt-pulse

test: test.c Makefile
	$(CC) test.c -llo -o test

install: dirt
	install -d $(DESTDIR)/bin/
	install -m 0755 dirt $(DESTDIR)/bin/dirt
