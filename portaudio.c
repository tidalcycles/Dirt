#include <stddef.h>
#include <stdlib.h>
#include <sys/time.h>

#include <portaudio.h>

#include "audio.h"
#include "common.h"
#include "portaudio.h"
#include "log.h"

#ifdef __linux
#include <pa_linux_alsa.h>
#endif

PaStream *stream;

#define PA_FRAMES_PER_BUFFER 1024

bool pa_trust_dac_time;
double pa_time_origin;
double pa_estimated_dac_time;
const double pa_latency = 0.125;

void pa_update_timebase(double now, double dac_time)
{
  if (pa_trust_dac_time)
  {
    epochOffset = now - dac_time;
  }
  else
  {
    pa_estimated_dac_time = now + pa_latency;
    epochOffset = now - pa_estimated_dac_time;
  }
}

static int pa_callback(const void *inputBuffer, void *outputBuffer,
		       unsigned long framesPerBuffer,
		       const PaStreamCallbackTimeInfo* timeInfo,
		       PaStreamCallbackFlags statusFlags,
		       void *userData) {

  // portaudio has a way to get audio-at-DAC time
  // but it has historically been unreliable on some platforms
  // so when portaudio is untrusted, count audio frames
  // try to detect when out-of-sync with real time
  // (unconditionally updating timebase leads to
  // unacceptable timing jitter)
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double now = tv.tv_sec + tv.tv_usec / 1000000.0;
  double start = pa_trust_dac_time ? (timeInfo->outputBufferDacTime - pa_time_origin) : pa_estimated_dac_time;
  if (! (start > now && now > start - 2 * pa_latency))
  {
    log_printf(LOG_OUT, "xrun?\n");
    pa_update_timebase(now, start);
  }

  float **buffers = (float **) outputBuffer;
  for (unsigned long i=0; i < framesPerBuffer; ++i) {
    double framenow = start + i / (double) g_samplerate;
    playback(buffers, i, framenow);
    dequeue(framenow);
  }
  pa_estimated_dac_time += framesPerBuffer / (double) g_samplerate;

  return paContinue;
}

static void StreamFinished( void* userData ) {
  printf( "Stream Completed\n");
}

void pa_init(bool trustDacTime) {

  PaStreamParameters outputParameters;

  PaError err;

  log_printf(LOG_OUT, "init pa\n");

  err = Pa_Initialize();
  if( err != paNoError ) {
    goto error;
  }

  int num = Pa_GetDeviceCount();
  const PaDeviceInfo *d;
  if (num <0) {
    err = num;
    goto error;
  }

  log_printf(LOG_OUT, "Devices = #%d\n", num);
  for (int i =0; i < num; i++) {
     d = Pa_GetDeviceInfo(i);
     log_printf(LOG_OUT, "%d = %s: %fHz\n", i, d->name, d->defaultSampleRate);
  }

  outputParameters.device = Pa_GetDefaultOutputDevice();
  if (outputParameters.device == paNoDevice) {
    log_printf(LOG_ERR, "Error: No default output device.\n");
    goto error;
  }
  log_printf(LOG_OUT, "default device: %s\n", Pa_GetDeviceInfo(outputParameters.device)->name);
  outputParameters.channelCount = g_num_channels;
  outputParameters.sampleFormat = paFloat32 | paNonInterleaved;
  outputParameters.suggestedLatency = 0.050;
  // Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;

  char foo[] = "hello";
  err = Pa_OpenStream(
            &stream,
            NULL, /* no input */
            &outputParameters,
            g_samplerate,
            PA_FRAMES_PER_BUFFER,
            paNoFlag,
            pa_callback,
            (void *) foo );
    if( err != paNoError ) {
      log_printf(LOG_OUT, "failed to open stream.\n");
      goto error;
    }

    err = Pa_SetStreamFinishedCallback( stream, &StreamFinished );
    if( err != paNoError ) {
      goto error;
    }

#ifdef __linux__
    log_printf(LOG_OUT, "setting realtime priority\n");
    PaAlsa_EnableRealtimeScheduling(stream, 1);
#endif

  pa_trust_dac_time = trustDacTime;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double now = tv.tv_sec + tv.tv_usec / 1000000.0;
  pa_time_origin = Pa_GetStreamTime(stream) - now;
  pa_update_timebase(now, now);
    
    err = Pa_StartStream(stream);
    if( err != paNoError ) {
      goto error;
    }

  return;
error:
    log_printf(LOG_ERR, "An error occured while using the portaudio stream\n" );
    log_printf(LOG_ERR, "Error number: %d\n", err );
    log_printf(LOG_ERR, "Error message: %s\n", Pa_GetErrorText( err ) );
    if( err == paUnanticipatedHostError) {
	const PaHostErrorInfo *hostErrorInfo = Pa_GetLastHostErrorInfo();
	log_printf(LOG_ERR, "Host API error = #%ld, hostApiType = %d\n", hostErrorInfo->errorCode, hostErrorInfo->hostApiType );
	log_printf(LOG_ERR, "Host API error = %s\n", hostErrorInfo->errorText );
    }
    Pa_Terminate();
    exit(-1);
}
