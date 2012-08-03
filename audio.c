#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <jack/jack.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "jack.h"
#include "audio.h"

#define HALF_PI 1.5707963267948966

pthread_mutex_t queue_waiting_lock;

t_sound *waiting = NULL;
t_sound *playing = NULL;

#ifdef FEEDBACK
t_loop *loop = NULL;
#endif

double epochOffset = 0;

jack_client_t *client = NULL;
static int samplerate = 0;

extern void audio_init(void) {
  pthread_mutex_init(&queue_waiting_lock, NULL);
  client = jack_start(audio_callback);
  samplerate = jack_get_sample_rate(client);
  file_set_samplerate(samplerate);
#ifdef FEEDBACK
  loop = new_loop(8);
#endif
}

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
  int s = queue_size(*queue);
  int added = 0;
  if (*queue == NULL) {
    *queue = new;
    assert(s == (queue_size(*queue) - 1));
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

        if (s != (queue_size(*queue) - 1)) {
          assert(s == (queue_size(*queue) - 1));
        }
        added++;
        break;
      }

      if (tmp->next == NULL) {
        // add to end of queue
        tmp->next = new;
        new->prev = tmp;
        added++;
        assert(s == (queue_size(*queue) - 1));
        break;
      }
      ++i;
      tmp = tmp->next;
    }
  }

  if (s != (queue_size(*queue) - added)) {
    assert(s == (queue_size(*queue) - added));
  }
  assert(added == 1);
}


void queue_remove(t_sound **queue, t_sound *old) {
  int s = queue_size(*queue);
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
  assert(s == (queue_size(*queue) + 1));
  free(old);
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

extern int audio_play(double when, char *samplename, float offset, float start, float end, float speed, float pan, float velocity, int vowelnum) {
  struct timeval tv;
#ifdef FEEDBACK
  int is_loop = 0;
#endif
  t_sample *sample = NULL;
  t_sound *new;
  
  gettimeofday(&tv, NULL);


#ifdef FEEDBACK
  if (strcmp(samplename, "loop") == 0) {
    is_loop = 1;
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
  
  new = (t_sound *) calloc(1, sizeof(t_sound));
  printf("samplename: %s when: %f\n", samplename, when - (float) tv.tv_sec);
  strncpy(new->samplename, samplename, MAXPATHSIZE);
  
#ifdef FEEDBACK
  if (is_loop) {
    new->loop    = loop;
    new->frames   = loop->frames;
    new->items    = loop->items;
    new->channels = 1;
    new->loop_start = (loop->now + (loop->frames / 2)) % loop->frames;
  }
  else {
#endif
    new->sample   = sample;
    new->frames   = sample->info->frames;
    new->items    = sample->items;
    new->channels = sample->info->channels;
#ifdef FEEDBACK
  }
  new->is_loop = is_loop;
#endif

  new->startFrame = 
    jack_time_to_frames(client, ((when-epochOffset) * 1000000));
  
  new->next = NULL;
  new->prev = NULL;
  new->startframe = 0;
  new->speed    = speed;
  new->pan      = pan;
  new->velocity = velocity;
  
  new->offset = offset;

  printf("frames: %f\n", new->frames);
  if (start > 0 && start <= 1) {
    new->startframe = start * new->frames;
  }
  
  if (end > 0 && end < 1) {
    new->frames *= end;
  }
  
  if (new->speed == 0) {
    new->speed = 1;
  }
  
  if (new->speed < 0) {
    new->startframe = new->frames - new->startframe;
  }
  
  new->position = new->startframe;
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
    int s = queue_size(playing);
    //printf("dequeuing %s @ %d\n", p->samplename, p->startFrame);
    p->prev = NULL;
    p->next = playing;
    if (playing != NULL) {
      playing->prev = p;
    }
    playing = p;
    assert(s == (queue_size(playing) - 1));
    
    //printf("done.\n");
  }
  pthread_mutex_unlock(&queue_waiting_lock);
}


inline void playback(float **buffers, int frame, jack_nframes_t frametime) {
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
      p = p->next;
      continue;
    }
    //printf("playing %s\n", p->samplename);
    channels = p->channels;
    //printf("channels: %d\n", channels);
    for (channel = 0; channel < channels; ++channel) {
      float roundoff = 1;
      float value;

#ifdef FEEDBACK
      if (p->is_loop) {
        // only one channel, but relative to 'now'
        unsigned int i = (p->loop_start + ((int) p->position)) % p->loop->frames;
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
      if ((!p->is_loop) && pos < p->frames) {
#else
      if (pos < p->frames) {
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

      if ((p->frames - p->position) < ROUNDOFF) {
        // TODO what frames < ROUNDOFF?)
        //printf("roundoff: %f\n", (p->frames - pos) / (float) ROUNDOFF);
        roundoff = (p->frames - pos) / (float) ROUNDOFF;
      }
      else {
        if ((pos - p->startframe) < ROUNDOFF) {
          roundoff = (pos - p->startframe) / (float) ROUNDOFF;
        }
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

    p->position += p->speed;
    //printf("position: %d of %d\n", p->position, playing->frames);

    /* remove dead sounds */
    tmp = p;
    p = p->next;
    if (tmp->speed > 0) {
      if (tmp->position >= tmp->frames) {
        //printf("remove %s\n", tmp->samplename);
        queue_remove(&playing, tmp);
      }
    }
    else {
      if (tmp->position <= 0) {
        //printf("remove %s\n", tmp->samplename);
        queue_remove(&playing, tmp);
      }
    }
  }
}

extern int audio_callback(int frames, float *input, float **outputs) {
  int i;
  jack_nframes_t now;

  if (epochOffset == 0) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    epochOffset = ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0)) 
      - ((double) jack_get_time() / 1000000.0);
    //printf("jack time: %d tv_sec %d epochOffset: %f\n", jack_get_time(), tv.tv_sec, epochOffset);
  }
  now = jack_last_frame_time(client);
  
  for (i=0; i < frames; ++i) {
#ifdef FEEDBACK
    loop->items[loop->now++] = input[i];

    if (loop->now >= loop->frames) {
      loop->now = 0;
    }
#endif
    dequeue(now + frames);

    playback(outputs, i, now + i);
  }
  return(0);
}
