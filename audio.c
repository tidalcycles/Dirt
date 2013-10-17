#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <assert.h>
#include <dirent.h>

#include "config.h"
#include "jack.h"
#include "audio.h"
#include "server.h"
#include "pitch.h"

#define HALF_PI 1.5707963267948966

pthread_mutex_t queue_waiting_lock;

t_sound *waiting = NULL;
t_sound *playing = NULL;

t_sound sounds[MAXSOUNDS];

#ifdef FEEDBACK
t_loop *loop = NULL;
int input_paused = 0;
#endif

double epochOffset = 0;
float starttime = 0;

jack_client_t *client = NULL;
static int samplerate = 0;
float compression_speed = -1;

int queue_size(t_sound *queue) {
  int result = 0;
  while (queue != NULL) {
    result++;
    queue = queue->next;
    if (result > 4096) {
      printf("whoops, big queue\n");
      break;
    }
  }
  return(result);
}

void queue_add(t_sound **queue, t_sound *new) {
  //printf("queuing %s @ %lld\n", new->samplename, new->start);
  int added = 0;
  if (*queue == NULL) {
    *queue = new;
    added++;
  }
  else {
    t_sound *tmp = *queue;
    assert(tmp->prev == NULL);

    int i =0;
    while (1) {
      if (tmp->startFrame > new->startFrame) {
        // insert in front of later event
        new->next = tmp;
        new->prev = tmp->prev;
        if (new->prev != NULL) {
          new->prev->next = new;
        }
        else {
          *queue = new;
        }
        tmp->prev = new;

        added++;
        break;
      }

      if (tmp->next == NULL) {
        // add to end of queue
        tmp->next = new;
        new->prev = tmp;
        added++;
        break;
      }
      ++i;
      tmp = tmp->next;
    }
  }

  assert(added == 1);
}


void queue_remove(t_sound **queue, t_sound *old) {
  if (old->prev == NULL) {
    *queue = old->next;
    if (*queue  != NULL) {
      (*queue)->prev = NULL;
    }
  }
  else {
    old->prev->next = old->next;

    if (old->next) {
      old->next->prev = old->prev;
    }
  }
  old->active = 0;
}

const double coeff[5][11]= {
  { 3.11044e-06,
    8.943665402,    -36.83889529,    92.01697887,    -154.337906,    181.6233289,
    -151.8651235,   89.09614114,    -35.10298511,    8.388101016,    -0.923313471  
  },
  {4.36215e-06,
   8.90438318,    -36.55179099,    91.05750846,    -152.422234,    179.1170248,  
   -149.6496211,87.78352223,    -34.60687431,    8.282228154,    -0.914150747
  },
  { 3.33819e-06,
    8.893102966,    -36.49532826,    90.96543286,    -152.4545478,    179.4835618,
    -150.315433,    88.43409371,    -34.98612086,    8.407803364,    -0.932568035  
},
  {1.13572e-06,
   8.994734087,    -37.2084849,    93.22900521,    -156.6929844,    184.596544,   
   -154.3755513,    90.49663749,    -35.58964535,    8.478996281,    -0.929252233
  },
  {4.09431e-07,
   8.997322763,    -37.20218544,    93.11385476,    -156.2530937,    183.7080141,  
   -153.2631681,    89.59539726,    -35.12454591,    8.338655623,    -0.910251753
  }
};

float formant_filter(float in, t_sound *sound, int channel) {
  float res = 
    (float) ( coeff[sound->formant_vowelnum][0] * in +
              coeff[sound->formant_vowelnum][1] * sound->formant_history[channel][0] +  
              coeff[sound->formant_vowelnum][2] * sound->formant_history[channel][1] +
              coeff[sound->formant_vowelnum][3] * sound->formant_history[channel][2] +
              coeff[sound->formant_vowelnum][4] * sound->formant_history[channel][3] +
              coeff[sound->formant_vowelnum][5] * sound->formant_history[channel][4] +
              coeff[sound->formant_vowelnum][6] * sound->formant_history[channel][5] +
              coeff[sound->formant_vowelnum][7] * sound->formant_history[channel][6] +
              coeff[sound->formant_vowelnum][8] * sound->formant_history[channel][7] +
              coeff[sound->formant_vowelnum][9] * sound->formant_history[channel][8] +
              coeff[sound->formant_vowelnum][10] * sound->formant_history[channel][9] 
             );

  sound->formant_history[channel][9] = sound->formant_history[channel][8];
  sound->formant_history[channel][8] = sound->formant_history[channel][7];
  sound->formant_history[channel][7] = sound->formant_history[channel][6];
  sound->formant_history[channel][6] = sound->formant_history[channel][5];
  sound->formant_history[channel][5] = sound->formant_history[channel][4];
  sound->formant_history[channel][4] = sound->formant_history[channel][3];
  sound->formant_history[channel][3] = sound->formant_history[channel][2];
  sound->formant_history[channel][2] = sound->formant_history[channel][1];
  sound->formant_history[channel][1] = sound->formant_history[channel][0];
  sound->formant_history[channel][0] = (double) res;
  return res;
}

void init_vcf (t_sound *sound) {
  for (int channel = 0; channel < CHANNELS; ++channel) {
    t_vcf *vcf = &(sound->vcf[channel]);
    vcf->f     = 2 * sound->cutoff;
    vcf->k     = 3.6 * vcf->f - 1.6 * vcf->f * vcf->f -1;
    vcf->p     = (vcf->k+1) * 0.5;
    vcf->scale = exp((1-vcf->p)*1.386249);
    vcf->r     = sound->resonance * vcf->scale;
    vcf->y1    = 0;
    vcf->y2    = 0;
    vcf->y3    = 0;
    vcf->y4    = 0;
    vcf->oldx  = 0;
    vcf->oldy1 = 0;
    vcf->oldy2 = 0;
    vcf->oldy3 = 0;
  }
}

float effect_vcf(float in, t_sound *sound, int channel) {
  t_vcf *vcf = &(sound->vcf[channel]);
  vcf->x  = in - vcf->r * vcf->y4;
  
  vcf->y1 = vcf->x  * vcf->p + vcf->oldx  * vcf->p - vcf->k * vcf->y1;
  vcf->y2 = vcf->y1 * vcf->p + vcf->oldy1 * vcf->p - vcf->k * vcf->y2;
  vcf->y3 = vcf->y2 * vcf->p + vcf->oldy2 * vcf->p - vcf->k * vcf->y3;
  vcf->y4 = vcf->y3 * vcf->p + vcf->oldy3 * vcf->p - vcf->k * vcf->y4;
  
  vcf->y4 = vcf->y4 - pow(vcf->y4,3) / 6;
  
  vcf->oldx  = vcf->x;
  vcf->oldy1 = vcf->y1;
  vcf->oldy2 = vcf->y2;
  vcf->oldy3 = vcf->y3;
  
  return vcf->y4;
}

extern void audio_kriole(double when, 
                         float duration, 
                         float pitch_start, 
                         float pitch_stop
			 ) {
#ifdef FEEDBACK
  
  int lowchunk = loop->chunk_n - (loop->frames / loop->chunksz);
  
  // subtract a bit for some legroom..
  lowchunk -= 16;

  lowchunk = lowchunk < 0 ? 0 : lowchunk;

  float chunklen = (float) loop->chunksz / (float) samplerate;
  int chunks = (int) (duration / chunklen);
  if (chunks == 0) {
    chunks = 1;
  }
  float pitch_diff = pitch_stop - pitch_start;
  for (int i = 0; i < chunks; ++i) {
    float pitch = pitch_start + (((float) i / (float) chunks) * pitch_diff);
    osc_send_play(when + ((float) i) * chunklen,
                  lowchunk,
                  pitch,
                  0, // flux
		  0 // centroid
                  );
  }
#endif
}

t_sound *new_sound() {
  t_sound *result = NULL;
  for (int i = 0; i < MAXSOUNDS; ++i) {
    if (sounds[i].active == 0) {
      result = &sounds[i];
      memset(result, 0, sizeof(t_sound));
      break;
    }
  }
  return(result);
}

extern int audio_play(double when, char *samplename, float offset, float start, float end, float speed, float pan, float velocity, int vowelnum, float cutoff, float resonance, float accellerate, float shape, int kriole_chunk) {
  struct timeval tv;
#ifdef FEEDBACK
  int is_kriole = 0;
#endif
  t_sample *sample = NULL;
  t_sound *new;
  
  if (speed == 0) {
    return(0);
  }

  gettimeofday(&tv, NULL);


#ifdef FEEDBACK
  if (strcmp(samplename, "kriole") == 0) {
    is_kriole = 1;
  }
  else {
#endif
    sample = file_get(samplename);
    if (sample == NULL) {
      return(0);
    }
#ifdef FEEDBACK
  }
#endif
  
  new = new_sound();
  if (new == NULL) {
    printf("hit max sounds (%d)\n", MAXSOUNDS);
    return(-1);
  }
  
  new->active = 1;
  //printf("samplename: %s when: %f\n", samplename, when);
  strncpy(new->samplename, samplename, MAXPATHSIZE);
  
#ifdef FEEDBACK
  if (is_kriole) {
    new->loop    = loop;
    //printf("calculating chunk %d\n", kriole_chunk);
    new->kriole_chunk = kriole_chunk;
    
    //printf("now %d\n", loop->now);
    //printf("since_chunk %d\n", loop->since_chunk);
    int last_chunk_start = (loop->now - loop->since_chunk);
    //printf("last_chunk_start %d\n", last_chunk_start);
    int chunks_back = loop->chunk_n - kriole_chunk;
    //printf("chunks_back %d\n", chunks_back);
    int samples_back = (chunks_back * loop->chunksz);
    //printf("samples_back %d\n", samples_back);
    int unmodded = last_chunk_start - samples_back;
    //printf("unmodded %d\n", unmodded);
    int modded = unmodded % loop->frames;
    //printf("modded %d\n", modded);
    new->start = modded;
    new->end      = new->start + loop->chunksz;
    new->items    = loop->items;
    new->channels = 1;
    //printf("kriole %d: start %f end %f\n", new->kriole_chunk, new->start, new->end);
    new->loop_start = (loop->now + (loop->frames / 2)) % loop->frames;
  }
  else {
#endif
    new->sample   = sample;
    new->start = 0;
    new->end   = sample->info->frames;
    new->items    = sample->items;
    new->channels = sample->info->channels;
#ifdef FEEDBACK
  }
  new->is_kriole = is_kriole;
#endif

  new->startFrame = 
    jack_time_to_frames(client, ((when-epochOffset) * 1000000));
  
  new->next = NULL;
  new->prev = NULL;
  new->speed    = fabsf(speed);
  new->reverse  = speed < 0;
  new->pan      = pan;
  if (new->channels == 2) {
    new->pan -= 0.5;
  }
#ifdef FAKECHANNELS
  new->pan *= (float) CHANNELS / FAKECHANNELS;
#endif
  new->velocity = velocity;
  
  new->offset = offset;

  new->cutoff = cutoff;
  new->resonance = resonance;

  if (shape != 0) {
    new->shape = 1;
    new->shape_k = (2.0f * shape) / (1.0f - shape);
  }

  init_vcf(new);

  new->accellerate = accellerate;

  //printf("frames: %f\n", new->end);
  if (start > 0 && start <= 1) {
    new->start = start * new->end;
  }
  
  if (end > 0 && end < 1) {
    new->end *= end;
  }

  /*  if (new->speed < 0) {
    float tmp;
    tmp = new->start;
    new->start = new->end;
    new->end = tmp;
  }
  */
  new->position = new->reverse ? new->end : new->start;
  //printf("position: %f\n", new->position);
  new->formant_vowelnum = vowelnum;
  
  pthread_mutex_lock(&queue_waiting_lock);
  queue_add(&waiting, new);
  //printf("added: %d\n", waiting != NULL);
  pthread_mutex_unlock(&queue_waiting_lock);
  
  return(1);
}

t_sound *queue_next(t_sound **queue, jack_nframes_t now) {
  t_sound *result = NULL;
  if (*queue != NULL && (*queue)->startFrame <= now) {
    result = *queue;
    *queue = (*queue)->next;
    if ((*queue) != NULL) {
      (*queue)->prev = NULL;
    }
  }

  return(result);
}

void dequeue(jack_nframes_t now) {
  t_sound *p;
  pthread_mutex_lock(&queue_waiting_lock);
  while ((p = queue_next(&waiting, now)) != NULL) {
#ifdef DEBUG
    int s = queue_size(playing);
#endif
    //printf("dequeuing %s @ %d\n", p->samplename, p->startFrame);
    p->prev = NULL;
    p->next = playing;
    if (playing != NULL) {
      playing->prev = p;
    }
    playing = p;
#ifdef DEBUG
    assert(s == (queue_size(playing) - 1));
#endif
    
    //printf("done.\n");
  }
  pthread_mutex_unlock(&queue_waiting_lock);
}

float compress(float in) {
  static float env = 0;
  env += (float) 50 / samplerate;
  if (fabs(in * env) > 1) {
    env = env / fabs(in * env);
  }
  return(env);
}

float compressdave(float in) {
  static float threshold = 0.5;
  static float env = 0;
  float result = in;
  // square input (to abs and make logarithmic)
  float t=in*in;
  
  // blend to create simple envelope follower
  env = env*(1-compression_speed) + t*compression_speed;
  
  // if we are over the threshold
  if (env > threshold) {
    // calculate the gain related to amount over thresh
    result *= 1.0f / (1.0f+(env - threshold));
  }
  return(result);
}

/**/

void playback(float **buffers, int frame, jack_nframes_t frametime) {
  int channel;
  t_sound *p = playing;
  
  for (channel = 0; channel < CHANNELS; ++channel) {
    buffers[channel][frame] = 0;
  }

  while (p != NULL) {
    int channels;
    t_sound *tmp;
    
    //printf("compare start %d with frametime %d\n", p->startFrame, frametime);
    if (p->startFrame > frametime) {
      p->checks++;
      p = p->next;
      continue;
    }
    if ((!p->started) && p->checks == 0 && p->startFrame < frametime) {
      /*printf("started late by %d frames\n",
	     frametime - p->startFrame	     
	     );*/
      p->started = 1;
    }
    //printf("playing %s\n", p->samplename);
    channels = p->channels;

    //printf("channels: %d\n", channels);
    for (channel = 0; channel < channels; ++channel) {
      float roundoff = 1;
      float value;

#ifdef FEEDBACK
      if (p->is_kriole) {
        unsigned int i = ((int) (p->position)
                          % p->loop->frames
                          );
        value = p->items[i];
      }
      else {
#endif
        value = p->items[(channels * ((int) p->position)) + channel];
#ifdef FEEDBACK
      }
#endif

      int pos = ((int) p->position) + 1;
#ifdef FEEDBACK
      if ((!p->is_kriole) && pos < p->end) {
#else
      if (pos < p->end) {
#endif
        float next = 
            p->items[(channels * pos)
                     + channel
                     ];
        float tween_amount = (p->position - (int) p->position);

        /* linear interpolation */
        value += (next - value) * tween_amount;
      }

      if (p->formant_vowelnum >= 0) {
        value = formant_filter(value, p, 0);
      }

      // why 44000 (or 44100)? init_vcf divides by samplerate..
      if (p->resonance > 0 && p->resonance < 1 
	  && p->cutoff > 0 && p->cutoff < 1) {
	value = effect_vcf(value, p, channel);
      }

      if ((p->end - p->position) < ROUNDOFF) {
        // TODO what if end < ROUNDOFF?)
        //printf("roundoff: %f\n", (p->end - pos) / (float) ROUNDOFF);
        roundoff = (p->end - p->position) / (float) ROUNDOFF;
        //printf("end roundoff: %f (%f)\n", roundoff, p->end - p->position);
      }
      else {
        if ((p->position - p->start) < ROUNDOFF) {
          roundoff = (p->position - p->start) / (float) ROUNDOFF;
          //printf("start roundoff: %f (%f / %d)\n", roundoff, p->position - p->start, ROUNDOFF);
        }
      }

      if (p->shape) {
        value = (1+p->shape_k)*value/(1+p->shape_k*fabs(value));
      }

      value *= roundoff;

      float c = (float) channel + p->pan;
      float d = c - floor(c);
      int channel_a =  ((int) c) % CHANNELS;
      int channel_b =  ((int) c + 1) % CHANNELS;

      if (channel_a < 0) {
        channel_a += CHANNELS;
      }
      if (channel_b < 0) {
        channel_b += CHANNELS;
      }

      // equal power panning
      buffers[channel_a][frame] += value * cos(HALF_PI * d);
      buffers[channel_b][frame] += value * sin(HALF_PI * d);
    }
      
    if (p->accellerate != 0) {
      // ->startFrame ->end ->position
      float duration = (p->end - p->start);
      float tmppos = (p->position - p->start) / duration;
      p->position += (1 - (tmppos * 2)) * p->accellerate + p->speed;
    }
    else {
      if (p->reverse) {
        p->position -= p->speed;
      }
      else {
        p->position += p->speed;
      }
    }
    //printf("position: %d of %d\n", p->position, playing->end);

    /* remove dead sounds */
    tmp = p;
    p = p->next;
    if (tmp->speed > 0) {
      if (tmp->position >= tmp->end) {
        //printf("remove %s %f\n", tmp->samplename, tmp->position);
        queue_remove(&playing, tmp);
      }
    }
    else {
      if (tmp->position <= tmp->end) {
        //printf("remove %s (zerospeed)\n", tmp->samplename);
        queue_remove(&playing, tmp);
      }
    }
  }
  float max = 0;
    
  for (channel = 0; channel < CHANNELS; ++channel) {
    if (fabsf(buffers[channel][frame]) > max) {
      max = buffers[channel][frame];
    }
  }
  float factor = compress(max);
  for (channel = 0; channel < CHANNELS; ++channel) {
    buffers[channel][frame] *= factor * 0.4;
  }
}



#ifdef FEEDBACK
void loop_input(float s) {
  loop->items[loop->now++] = s;
  if (loop->now >= loop->frames) {
    loop->now = 0;
    loop->loops++;
  }
  if (loop->since_chunk == loop->chunksz) {
    loop->since_chunk = 0;
    float *extracted = pitch_calc(loop);
    if (extracted != NULL) {
      float pitch = extracted[0];
      float flux = extracted[1];
      float centroid = extracted[2];
      
      if (pitch >= 0) {
        osc_send_pitch(starttime, loop->chunk_n, pitch, flux, centroid);
      }
    }
    loop->chunk_n++;
  }
  loop->since_chunk++;
}
#endif

extern int audio_callback(int frames, float *input, float **outputs) {
  jack_nframes_t now;

  if (epochOffset == 0) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    epochOffset = ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0)) 
      - ((double) jack_get_time() / 1000000.0);
    //printf("jack time: %d tv_sec %d epochOffset: %f\n", jack_get_time(), tv.tv_sec, epochOffset);
  }
  now = jack_last_frame_time(client);
  
  for (int i=0; i < frames; ++i) {
    playback(outputs, i, now + i);

#ifdef FEEDBACK
#ifdef INPUT
    if (! input_paused) {
      loop_input(input[i]);
    }
#else
    loop_input(outputs[0][i]);
#endif
#endif
    dequeue(now + frames);
  }
  return(0);
}

#ifdef FEEDBACK
void preload_kriol(char *dir) {
  int n;
  char path[MAXPATHSIZE];
  struct dirent **namelist = NULL;

  snprintf(path, MAXPATHSIZE -1, "%s/%s", SAMPLEROOT, dir);
  n = scandir(path, &namelist, wav_filter, alphasort);
  for (int i = 0; i < n; ++i) {
    snprintf(path, MAXPATHSIZE -1, 
             "kriol_preload/%s", namelist[i]->d_name
             );
    t_sample *sample = file_get(path);
    if (sample == NULL) {
      printf("failed to preload %s\n", path);
    }
    else {
      printf("preloading %d frames of %s\n", (int) sample->info->frames, path);
      for (int j = 0; j < sample->info->frames; j++) {
	loop_input(sample->items[j * sample->info->channels]);
      }
    }
    free(namelist[i]);
  }
  if (namelist == NULL) {
    printf("couldn't load %s (%s)\n", dir, path);
  }
  else {
    free(namelist);
  }
}

void audio_pause_input(int paused) {
   input_paused = paused;
   printf("input is now %s.\n", input_paused ? "off" : "on");
}

#endif

extern void audio_init(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  starttime = (float) tv.tv_sec + ((float) tv.tv_usec / 1000000.0);
#ifdef FEEDBACK
  loop = new_loop(60 * 60);
#endif
  pthread_mutex_init(&queue_waiting_lock, NULL);
  client = jack_start(audio_callback);
  samplerate = jack_get_sample_rate(client);
  compression_speed = 1000 / samplerate;
  file_set_samplerate(samplerate);

#ifdef FEEDBACK
  pitch_init(loop, samplerate);
#endif
}
