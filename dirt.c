#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "common.h"
#include "audio.h"
#include "server.h"

int main (int argc, char **argv) {
  if (argc > 1 && strncmp(argv[1], "-h", 2) == 0) {
      printf("Dirt - a software sampler, which listens for\n"
             "OpenSoundControl messages on port %s.\n"
             "Mainly used with Tidal http://yaxu.org/tidal/\n"
             "Released as free software under the terms of the\n"
             "GNU Public License version 3.0 and later.\n",
             OSC_PORT
             );
      return(0);
  }

  bool dirty_compressor = true;
  if (argc > 1 && strncmp(argv[1], "-dc", 3) >= 0) {
      dirty_compressor = false;
      fprintf(stderr, "dirty compressor disabled\n");
  }

  fprintf(stderr, "init audio\n");
  audio_init(dirty_compressor);

  fprintf(stderr, "init open sound control\n");
  server_init();

  sleep(-1);
  return(0);
}
