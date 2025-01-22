#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include "audio.h"
#include "common.h"
#include "pulse.h"

void *run_pulse(void *arg) {
  #define FRAMES 64
  struct timeval tv;
  double samplelength = (((double) 1)/((double) g_samplerate));

  float *buf[g_num_channels];
  for (int i = 0 ; i < g_num_channels; ++i) {
    buf[i] = (float*) malloc(sizeof(float)*FRAMES);
  }
  float interlaced[g_num_channels*FRAMES];

  pa_sample_spec ss;
  ss.format = PA_SAMPLE_FLOAT32LE;
  ss.rate = g_samplerate;
  ss.channels = g_num_channels;

  pa_simple *s = NULL;
  //  int ret = 1;
  int error;
  if (!(s = pa_simple_new(NULL, "dirt", PA_STREAM_PLAYBACK, NULL,
    "playback", &ss, NULL, NULL, &error))) {
    fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n",
    pa_strerror(error));
    goto finish;
  }

  for (;;) {

    pa_usec_t latency;
    if ((latency = pa_simple_get_latency(s, &error)) == (pa_usec_t) -1) {
      fprintf(stderr, __FILE__": pa_simple_get_latency() failed: %s\n",
	      pa_strerror(error));
      goto finish;
    }
    //fprintf(stderr, "%f sec    \n", ((float)latency)/1000000.0f);

    gettimeofday(&tv, NULL);
    double now = ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0));

    for (int i=0; i < FRAMES; ++i) {
      double framenow = now + (samplelength * (double) i);
      playback(buf, i, framenow);
      for (int j=0; j < g_num_channels; ++j) {
	interlaced[g_num_channels*i+j] = buf[j][i];
      }
      dequeue(framenow);
    }

    if (pa_simple_write(s, interlaced, sizeof(interlaced), &error) < 0) {
      fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
      goto finish;
    }
  }
  /* Make sure that every single sample was played */
  if (pa_simple_drain(s, &error) < 0) {
    fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
    goto finish;
  }
  //    ret = 0;
 finish:
  if (s)
    pa_simple_free(s);
  //    return ret;
  return NULL;
}

pthread_t pulse_thread;

void pulse_init(void) {
  pthread_create(&pulse_thread, NULL, (void *(*)(void *)) run_pulse, NULL);
  //sleep(1);
  //  pulse = pa_threaded_mainloop_new();
  //  pa_threaded_mainloop_set_name(pulse, "dirt");
}
