CC=gcc
CFLAGS = -O3 -g -Wall -pedantic
LDFLAGS = -llo -lsndfile -lsamplerate -ljack

all: dirt

clean :
	rm -f *.o *~ dirt

dirt: dirt.o jack.o audio.o file.o Makefile
	$(CC) dirt.o jack.o audio.o file.o $(CFLAGS) $(LDFLAGS) -o dirt 
