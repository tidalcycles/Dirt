#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include <sys/time.h>

#include "audio.h"
#include "log.h"
#include "sdl2.h"

float **sdl2_audio_buffer;

double sdl2_dac_time;

const double sdl2_latency = 0.125;

void sdl2_update_timebase(double now)
{
  sdl2_dac_time = now - sdl2_latency;
  epochOffset = now - sdl2_dac_time;
}

void sdl2_audio_callback(void *userdata, Uint8 *stream, int len)
{
  (void) userdata;

  // sdl2 has no built-in way to get audio-at-DAC time
  // so count audio frames
  // and try to detect when out-of-sync with real time
  // (unconditionally updating timebase leads to
  // unacceptable timing jitter)
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double now = tv.tv_sec + tv.tv_usec / 1000000.0;
  if (sdl2_dac_time > now || now > sdl2_dac_time + 2 * sdl2_latency)
  {
    log_printf(LOG_OUT, "xrun?\n");
    sdl2_update_timebase(now);
  }

  float *b = (float *) stream;
  int m = len / sizeof(float) / g_num_channels;
  int k = 0;

  for (int i = 0; i < m; ++i)
  {
    double framenow = sdl2_dac_time + i / (double) g_samplerate;
    playback(sdl2_audio_buffer, i, framenow);
    for (int j = 0; j < g_num_channels; ++j)
    {
      b[k++] = sdl2_audio_buffer[j][i];
    }
    dequeue(framenow);
  }
  sdl2_dac_time = sdl2_dac_time + m / (double) g_samplerate;
}

void sdl2_init(void)
{
  // initialize audio
  SDL_Init(SDL_INIT_AUDIO);
  SDL_AudioSpec want, have;
  want.freq = g_samplerate;
  want.format = AUDIO_F32;
  want.channels = g_num_channels;
  want.samples = 1024;
  want.callback = sdl2_audio_callback;
  SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
  log_printf(LOG_OUT, "engine sample rate: %d\n", have.freq);
  log_printf(LOG_OUT, "engine block size: %d\n", have.samples);
  g_samplerate = have.freq;

  // allocate output buffers
  sdl2_audio_buffer = calloc(1, sizeof(*sdl2_audio_buffer) * g_num_channels);
  for (int i = 0; i < g_num_channels; ++i)
  {
    sdl2_audio_buffer[i] = calloc(1, sizeof(*sdl2_audio_buffer[i]) * have.samples);
  }

  // start audio
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double now = tv.tv_sec + tv.tv_usec / 1000000.0;
  sdl2_update_timebase(now);
  SDL_PauseAudioDevice(dev, 0);
}
