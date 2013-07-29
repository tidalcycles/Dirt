#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include <math.h>
#include "file.h"
#include <xtract/libxtract.h>

extern void pitch_init(t_loop *loop, int samplerate) {
  //  loop->win_s      = 1024;
  //  loop->hop_s      = loop->win_s/4;
  loop->win_s      = loop->chunksz;
  loop->hop_s      = loop->chunksz;
  loop->samplerate = samplerate;
  loop->channels   = 1;
  
  xtract_init_fft(loop->chunksz, XTRACT_SPECTRUM);

  loop->initialised = 1;
}

void pitch_destruct(t_loop *loop) {
}

extern float *pitch_calc(t_loop *loop) {
  float rmsAmplitude  = 0;
  double pitch = -1;
  static float result[3];
  result[0] = -1;
  result[1] = -1;
  result[2] = -1;

  if (loop->initialised == 0) {
    return(NULL);
  }
  
  int j = (loop->now - loop->chunksz) % loop->frames;
  
  for (int i = 0; i < loop->chunksz; i++){
    rmsAmplitude += sqrt(loop->items[j]*loop->items[j]);
    loop->in[i] = (double) loop->items[j];
    ++j;
    if (j >= loop->frames) {
      j = 0;
    }
  }

  rmsAmplitude /= loop->chunksz;

  if( rmsAmplitude > 0.005 ){
    double flux = -1;
    double centroid = -1;
    double param[4];
    double spectrum[loop->chunksz];

    xtract[XTRACT_WAVELET_F0](loop->in, loop->chunksz, &loop->samplerate, &pitch);

    param[0] = (double) loop->samplerate / (double)loop->chunksz;
    param[1] = XTRACT_MAGNITUDE_SPECTRUM;
    param[2] = 0.f;
    param[3] = 0.f;

    xtract[XTRACT_SPECTRUM](loop->in, loop->chunksz, &param[0], spectrum);

    xtract_spectral_centroid(spectrum, loop->chunksz, NULL,  &centroid);

    // order
    param[0] = 1;
    // type
    param[1] = XTRACT_POSITIVE_SLOPE;
    xtract[XTRACT_FLUX](loop->in, loop->chunksz, &param, &flux);

    printf("pitches: %f %f %f %f\n", pitch, flux, centroid, rmsAmplitude);
    result[0] = pitch;
    result[1] = flux;
    result[2] = centroid;
  }
  return(result);
}

/*
extern int *segment_get_onsets(t_sample *sample) {
  int *result;
  pitch_init(sample->info->channels);
  result = pitch_process(sample, sample->items, sample->info->frames);
  pitch_destruct();
  return(result);
}
*/
