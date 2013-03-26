#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include <aubio/aubio.h>
#include <math.h>
#include "file.h"

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
  loop->initialised = 1;
}

void pitch_destruct(t_loop *loop) {
  del_fvec(loop->in);
  del_aubio_pitchdetection(loop->pitch_output);

}
extern float pitch_calc(t_loop *loop) {
  float rmsAmplitude  = 0;
  float pitch = -1;

  if (loop->initialised == 0) {
    return(-1);
  }
  
  int j = (loop->now - loop->chunksz) % loop->frames;
  
  for (int i = 0; i < loop->chunksz; i++){
    //calculate the root mean square amplitude
    rmsAmplitude += sqrt(loop->items[j]*loop->items[j]);
    
    loop->in->data[0][i] = loop->items[j];
    ++j;
    if (j >= loop->frames) {
      j = 0;
    }
  }
  
  //now we need to get the average
  rmsAmplitude /= loop->chunksz;
  //printf("loud: %f\n", rmsAmplitude);
  
  //don't update the pitch if the sound is very quiet
  if( rmsAmplitude > 0.2 ){
    //finally get the pitch of the sound
    pitch = aubio_pitchdetection(loop->pitch_output, loop->in);
  }
  
  return(pitch);
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
