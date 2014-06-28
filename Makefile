CC=gcc


CFLAGS += -g -I/usr/local/include -Wall -O3 -std=gnu99 -DCHANNELS=2 -DDIRTYCOMPRESSOR
LDFLAGS += -lm -L/usr/local/lib -llo -lsndfile -lsamplerate

dirt: CFLAGS += -DJACK
dirt: LDFLAGS += -ljack
dirt-pa: LDFLAGS += -lportaudio

all: dirt

clean:
	rm -f *.o *~ dirt dirt-analyse

dirt: dirt.o jack.o audio.o file.o server.o  Makefile
	$(CC) dirt.o jack.o audio.o file.o server.o $(CFLAGS) $(LDFLAGS) -o dirt

dirt-pa: dirt.o audio.o file.o server.o  Makefile
	$(CC) dirt.o audio.o file.o server.o $(CFLAGS) $(LDFLAGS) -o dirt-pa

dirt-analyse: dirt.o jack.o audio.o file.o server.o pitch.o Makefile
	$(CC) dirt.o jack.o audio.o file.o server.o pitch.o $(CFLAGS) $(LDFLAGS) -o dirt-analyse

test : test.c Makefile
	$(CC) test.c -llo -o test

install: dirt
	install -d $(DESTDIR)/bin/
	install -m 0755 dirt $(DESTDIR)/bin/dirt


