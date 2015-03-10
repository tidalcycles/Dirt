CC=gcc


CFLAGS += -g -I/usr/local/include -Wall -O3 -std=gnu99
LDFLAGS += -lm -L/usr/local/lib -llo -lsndfile -lsamplerate

dirt: CFLAGS += -DJACK -DSCALEPAN
dirt: LDFLAGS += -ljack
dirt-pa: LDFLAGS += -lportaudio
dirt-pulse: CFLAGS += -DPULSE `pkg-config --cflags libpulse-simple`
dirt-pulse: LDFLAGS += `pkg-config --libs libpulse-simple` -lpthread

all: dirt

clean:
	rm -f *.o *~ dirt dirt-analyse

dirt: dirt.o common.o jack.o audio.o file.o server.o  Makefile
	$(CC) dirt.o common.o jack.o audio.o file.o server.o $(CFLAGS) $(LDFLAGS) -o dirt

dirt-pa: dirt.o common.o audio.o file.o server.o  Makefile
	$(CC) dirt.o common.o audio.o file.o server.o $(CFLAGS) $(LDFLAGS) -o dirt-pa

dirt-pulse: dirt.o common.o audio.o file.o server.o  Makefile
	$(CC) dirt.o common.o audio.o file.o server.o $(CFLAGS) $(LDFLAGS) -o dirt-pulse

dirt-analyse: dirt.o common.o jack.o audio.o file.o server.o pitch.o Makefile
	$(CC) dirt.o common.o jack.o audio.o file.o server.o pitch.o $(CFLAGS) $(LDFLAGS) -o dirt-analyse

test : test.c Makefile
	$(CC) test.c -llo -o test

install: dirt
	install -d $(DESTDIR)/bin/
	install -m 0755 dirt $(DESTDIR)/bin/dirt


