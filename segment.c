#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include <aubio/aubio.h>
#include "file.h"

int overlap_size = 256;
aubio_pvoc_t * pv;
fvec_t * ibuf;
cvec_t * fftgrain;
aubio_onsetdetection_t *o;
aubio_onsetdetection_type type_onset  = aubio_onset_kl;
fvec_t *onset;

smpl_t threshold                      = 0.3;
smpl_t silence                        = -90.;
uint_t buffer_size                    = 512; /*1024;*/
aubio_pickpeak_t * parms;
unsigned int pos = 0; /*frames%dspblocksize*/
int channels;

#define MAXONSETS 128
int onsets[MAXONSETS];
int onset_n = 0;

void aubio_init(int c) {
  channels = c;
  /* phase vocoder */
  pv = new_aubio_pvoc(buffer_size, overlap_size, channels);  
  ibuf = new_fvec(overlap_size, channels);
  fftgrain  = new_cvec(buffer_size, channels);
  o = new_aubio_onsetdetection(type_onset, buffer_size, channels);
  parms = new_aubio_peakpicker(threshold);
  onset = new_fvec(1, channels);
  pos = 0;
  onset_n = 0;
}

void aubio_destruct() {
  del_aubio_pvoc(pv);
  del_fvec(ibuf);
  del_cvec(fftgrain);
  del_aubio_onsetdetection(o);
  del_aubio_peakpicker(parms);
  del_fvec(onset);
}

int *aubio_process(t_sample *sound, float *input, sf_count_t nframes) {
  unsigned int j, i;       /*frames*/

  int *result;

  for (j=0; j < nframes; j++) {
    for (i=0; i < channels; i++) {
      /* write input to datanew */
      fvec_write_sample(ibuf, input[channels*j+i], i, pos);
    }
  }

  for (j=0; j < nframes; j++) {
    /*time for fft*/
    if ((pos % (overlap_size-1)) == 0) {
      int isonset;
      /* block loop */
      aubio_pvoc_do(pv, ibuf, fftgrain);
      aubio_onsetdetection(o, fftgrain, onset);
      /*
      if (usedoubled) {
        aubio_onsetdetection(o2,fftgrain, onset2);
        onset->data[0][0] *= onset2->data[0][0];
      }
      */
      isonset = aubio_peakpick_pimrt(onset, parms);
      if (isonset) {
        /* test for silence */
        if (aubio_silence_detection(ibuf, silence)==1)
          isonset=0;
        else {
          printf("onset! %d\n", pos);
          onsets[onset_n++] = pos;
        }
      }
    }
    pos++;
  }
  
  result = (int *) calloc(1, sizeof(unsigned int) * (onset_n + 1));
  memcpy(result, onsets, sizeof(int) * onset_n);
  result[onset_n] = -1;
  printf("found %d onsets\n", onset_n);

  return(result);
}

extern int *segment_get_onsets(t_sample *sound) {
  int *result;
  aubio_init(sound->info->channels);
  result = aubio_process(sound, sound->items, sound->info->frames);
  aubio_destruct();
  return(result);
}
