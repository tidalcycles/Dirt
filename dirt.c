#include <stdio.h>

#include <unistd.h>

#include "jack.h"
#include "audio.h"

int main (int argc, char **argv) {
  int samplerate = jack_start(audio_callback);
  
  sleep(-1);
  return(0);
}
