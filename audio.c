#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <jack/jack.h>

#include "jack.h"
#include "audio.h"

pthread_mutex_t queue_lock;

t_queue *waiting = NULL;
t_queue *playing = NULL;

double offset = 0;

jack_client_t *client = NULL;

extern void audio_init(void) {
  int samplerate;

  pthread_mutex_init(&queue_lock, NULL);
  client = jack_start(audio_callback);
  samplerate = jack_get_sample_rate(client);
  file_set_samplerate(samplerate);
}

void queue_add(t_queue *queue, t_queue *new) {
  if (queue == NULL) {
    queue = new;
  }
  else {
    t_queue *tmp = queue;
    while (1) {
      if (tmp->next == NULL) {
        tmp->next = new;
        new->last = tmp;
        break;
      }

      if (tmp->when > new->when) {
        new->next = tmp;
        new->last = tmp->last;
        if (tmp->last) {
          tmp->last->next = new;
          tmp->last = new;
        }

        if (queue == tmp) {
          queue = new;
        }
        break;
      }

      tmp = tmp->next;
    }
  }
}

void queue_remove(t_queue *queue, t_queue *old) {
  if (queue == old) {
    queue = old->next;
  }
  if (old->last) {
    old->last->next = old->next;
  }
  if (old->next) {
    old->next->last = old->last;
  }
  free(old);
}

extern int audio_play(char *samplename) {
  int result = 0;
  t_sound *sound;
  t_sample *sample = file_get(samplename);

  if (sample != NULL) {
    t_queue *new;
    
    sound = (t_sound *) malloc(sizeof(t_sound));
    strncpy(sound->samplename, samplename, MAXPATHSIZE);
    sound->sample = sample;

    new = (t_queue *) malloc(sizeof(t_queue));
    new->when = -1;
    new->sound = sound;
    new->next = NULL;
    new->last = NULL;
    new->position = 0;
    new->speed = 1;

    pthread_mutex_lock(&queue_lock);
    queue_add(waiting, new);
    pthread_mutex_unlock(&queue_lock);

    result = 1;
  }
  return(result);
}

t_queue *queue_next(t_queue *queue, double now) {
  t_queue *result = NULL;
  if (queue != NULL && queue->when <= now) {
    result = queue;
    queue = queue->next;
  }
  return(result);
}

void dequeue() {
  t_queue *p;
  while (p = queue_next(waiting, now) != NULL) {
    p->last = NULL;
    p->next = playing;
    if (playing != NULL) {
      playing->last = p;
    }
    playing = p;
  }
}

extern int audio_callback(int frames, float *buffer) {
  int i;
  if (offset == 0) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    offset = (tv.sec + (tv.usec / 1000000)) - jack_get_time();
  }

  for (i=0; i < frames; ++i) {
    pthread_mutex_lock(&queue_lock);
    dequeue();
    pthread_mutex_unlock(&queue_lock);
    buffer[0] = playback();
  }
  return(0);
}
