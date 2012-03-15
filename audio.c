#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <jack/jack.h>
#include <math.h>
#include <assert.h>

#include "jack.h"
#include "audio.h"

#define HALF_PI 1.5707963267948966

pthread_mutex_t queue_waiting_lock;

t_sound *waiting = NULL;
t_sound *playing = NULL;

double epochOffset = 0;

jack_client_t *client = NULL;

extern void audio_init(void) {
  int samplerate;

  pthread_mutex_init(&queue_waiting_lock, NULL);
  client = jack_start(audio_callback);
  samplerate = jack_get_sample_rate(client);
  file_set_samplerate(samplerate);
}

int queue_size(t_sound *queue) {
  int result = 0;
  while (queue != NULL) {
    result++;
    queue = queue->next;
    if (result > 1024) {
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

extern int audio_play(double when, char *samplename, float offset, float duration, float speed, float pan, float velocity) {
  int result = 0;
  t_sample *sample = file_get(samplename);
  //printf("samplename: %s when: %f\n", samplename, when);
  if (sample != NULL) {
    //printf("got\n");
    t_sound *new = (t_sound *) calloc(1, sizeof(t_sound));
    
    strncpy(new->samplename, samplename, MAXPATHSIZE);
    new->sample = sample;
    new->startFrame = 
      jack_time_to_frames(client, ((when-epochOffset) * 1000000));
    //printf("start: %lld\n", new->start);
    new->next = NULL;
    new->prev = NULL;
    new->position = 0;

    new->speed    = speed;
    new->pan      = pan;
    new->velocity = velocity;

    new->offset = offset;
    new->frames = new->sample->info->frames;
    new->duration = duration;
    if (new->duration < 1)  {
      new->frames *= new->duration;
    }

    pthread_mutex_lock(&queue_waiting_lock);
    queue_add(&waiting, new);
    //printf("added: %d\n", waiting != NULL);
    pthread_mutex_unlock(&queue_waiting_lock);

    result = 1;
  }
  return(result);
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
    channels = p->sample->info->channels;
    //printf("channels: %d\n", channels);
    for (channel = 0; channel < channels; ++channel) {
      float value = 
        p->sample->items[(channels * ((int) p->position)) + channel];

      if ((((int) p->position) + 1) < p->frames) {
        float next = 
          p->sample->items[(channels * (((int) p->position) + 1))
                           + channel
                           ];
        float tween_amount = (p->position - (int) p->position);
        /* linear interpolation */
        value += (next - value) * tween_amount;
      }

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

      // equal power
      buffers[channel_a][frame] += value * cos(HALF_PI * d);
      buffers[channel_b][frame] += value * sin(HALF_PI * d);
    }

    p->position += p->speed;
    //printf("position: %d of %d\n", p->position, playing->frames);

    /* remove dead sounds */
    tmp = p;
    p = p->next;
    if (tmp->position >= tmp->frames) {
      //printf("remove %s\n", tmp->samplename);
      queue_remove(&playing, tmp);
    }
  }
}

extern int audio_callback(int frames, float **buffers) {
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
    dequeue(now + frames);

    playback(buffers, i, now + i);
  }
  return(0);
}
