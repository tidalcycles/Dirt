#include <stdio.h>

#include <unistd.h>

#include "jack.h"
#include "audio.h"
#include "server.h"

int main (int argc, char **argv) {
  printf("init audio\n");
  audio_init();
  printf("init osc\n");
  server_init();
  sleep(-1);
  return(0);
}
