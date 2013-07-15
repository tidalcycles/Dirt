#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include <aubio/aubio.h>
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
  
  loop->mode = aubio_pitchm_freq; // or midi
  loop->type = aubio_pitch_yinfft;
  loop->in = new_fvec(loop->hop_s, loop->channels);
  loop->pitch_output = 
    new_aubio_pitchdetection(loop->win_s, 
                             loop->hop_s, 
                             loop->channels, 
                             loop->samplerate, 
                             loop->type, 
                             loop->mode);
  xtract_init_fft(loop->chunksz, XTRACT_SPECTRUM);

  loop->initialised = 1;
}

void pitch_destruct(t_loop *loop) {
  del_fvec(loop->in);
  del_aubio_pitchdetection(loop->pitch_output);
}

extern float *pitch_calc(t_loop *loop) {
  float rmsAmplitude  = 0;
  float pitch = -1;
  float pitch2 = -1;
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
    
    loop->in->data[0][i] = loop->items[j];
    ++j;
    if (j >= loop->frames) {
      j = 0;
    }
  }
  

  rmsAmplitude /= loop->chunksz;

  if( rmsAmplitude > 0.015 ){
    float flux = -1;
    float centroid = -1;
    float param[4];
    float spectrum[loop->chunksz];

    pitch = aubio_pitchdetection(loop->pitch_output, loop->in);
    
    int x = xtract_failsafe_f0(loop->in->data[0],
			       loop->chunksz,
			       &loop->samplerate,
			       &pitch2
			      );

    param[0] = (float) loop->samplerate / (float)loop->chunksz;
    param[1] = XTRACT_MAGNITUDE_SPECTRUM;
    param[2] = 0.f;
    param[3] = 0.f;

    xtract[XTRACT_SPECTRUM](loop->in->data[0], loop->chunksz, &param[0], spectrum);

    xtract_spectral_centroid(spectrum, loop->chunksz, NULL,  &centroid);

    // order
    param[0] = 1;
    // type
    param[1] = XTRACT_POSITIVE_SLOPE;
    xtract[XTRACT_FLUX](loop->in->data[0], loop->chunksz, &param, &flux);

    printf("pitches: %f vs %f (%d) %f %f\n", pitch, pitch2, x, flux, centroid);
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
