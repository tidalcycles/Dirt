#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <portaudio.h>

#include "audio.h"
#include "common.h"
#include "portaudio.h"

#ifdef __linux
#include <pa_linux_alsa.h>
#endif

PaStream *stream;

#define PA_FRAMES_PER_BUFFER 1024

static int pa_callback(const void *inputBuffer, void *outputBuffer,
		       unsigned long framesPerBuffer,
		       const PaStreamCallbackTimeInfo* timeInfo,
		       PaStreamCallbackFlags statusFlags,
		       void *userData) {

  struct timeval tv;

  if (epochOffset == 0) {
    gettimeofday(&tv, NULL);
    #ifdef HACK
    epochOffset = 0;
    #else
    epochOffset = ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0))
      - timeInfo->outputBufferDacTime;
    #endif
    /* printf("set offset (%f - %f) to %f\n", ((float) tv.tv_sec + ((float) tv.tv_usec / 1000000.0f))
       , timeInfo->outputBufferDacTime, epochOffset); */
  }
  #ifdef HACK
  double now = ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0));
  #else
  double now = timeInfo->outputBufferDacTime;
  #endif
  // printf("%f %f %f\n", timeInfo->outputBufferDacTime, timeInfo->currentTime,   Pa_GetStreamTime(stream));
  float **buffers = (float **) outputBuffer;
  for (int i=0; i < framesPerBuffer; ++i) {
    double framenow = now + (((double) i)/((double) g_samplerate));
    playback(buffers, i, framenow);
    dequeue(framenow);
  }
  return paContinue;
}

static void StreamFinished( void* userData ) {
  printf( "Stream Completed\n");
}

void pa_init(void) {
  PaStreamParameters outputParameters;

  PaError err;

  printf("init pa\n");

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

  printf("Devices = #%d\n", num);
  for (int i =0; i < num; i++) {
     d = Pa_GetDeviceInfo(i);
     printf("%d = %s: %fHz\n", i, d->name, d->defaultSampleRate);
  }

  outputParameters.device = Pa_GetDefaultOutputDevice();
  if (outputParameters.device == paNoDevice) {
    fprintf(stderr,"Error: No default output device.\n");
    goto error;
  }
  printf("default device: %s\n", Pa_GetDeviceInfo(outputParameters.device)->name);
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
      printf("failed to open stream.\n");
      goto error;
    }

    err = Pa_SetStreamFinishedCallback( stream, &StreamFinished );
    if( err != paNoError ) {
      goto error;
    }

#ifdef __linux__
    printf("setting realtime priority\n");
    PaAlsa_EnableRealtimeScheduling(stream, 1);
#endif
    
    err = Pa_StartStream(stream);
    if( err != paNoError ) {
      goto error;
    }

  return;
error:
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    if( err == paUnanticipatedHostError) {
	const PaHostErrorInfo *hostErrorInfo = Pa_GetLastHostErrorInfo();
	fprintf( stderr, "Host API error = #%ld, hostApiType = %d\n", hostErrorInfo->errorCode, hostErrorInfo->hostApiType );
	fprintf( stderr, "Host API error = %s\n", hostErrorInfo->errorText );
    }
    Pa_Terminate();
    exit(-1);
}
