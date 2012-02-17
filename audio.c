#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <jack/jack.h>
#include <math.h>
#include <assert.h>

#include "jack.h"
#include "audio.h"

pthread_mutex_t queue_waiting_lock;

t_queue *waiting = NULL;
t_queue *playing = NULL;

double offset = 0;

jack_client_t *client = NULL;

extern void audio_init(void) {
  int samplerate;

  pthread_mutex_init(&queue_waiting_lock, NULL);
  client = jack_start(audio_callback);
  samplerate = jack_get_sample_rate(client);
  file_set_samplerate(samplerate);
}

int queue_size(t_queue *queue) {
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

void queue_add(t_queue **queue, t_queue *new) {
  //printf("queuing %s @ %lld\n", new->sound->samplename, new->start);
  int s = queue_size(*queue);
  int added = 0;
  if (*queue == NULL) {
    *queue = new;
    assert(s == (queue_size(*queue) - 1));
    added++;
  }
  else {
    t_queue *tmp = *queue;
    assert(tmp->prev == NULL);

    int i =0;
    while (1) {
      if (tmp->start > new->start) {
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


void queue_remove(t_queue **queue, t_queue *old) {
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
  //free(old);
}

extern int audio_play(double when, char *samplename) {
  int result = 0;
  t_sound *sound;
  t_sample *sample = file_get(samplename);
  //printf("samplename: %s when: %f\n", samplename, when);
  if (sample != NULL) {
    //printf("got\n");
    t_queue *new;
    
    sound = (t_sound *) calloc(1, sizeof(t_sound));
    strncpy(sound->samplename, samplename, MAXPATHSIZE);
    sound->sample = sample;

    new = (t_queue *) calloc(1, sizeof(t_queue));
    new->start = jack_time_to_frames(client, ((when-offset) * 1000000));
    //printf("start: %lld\n", new->start);
    new->sound = sound;
    new->next = NULL;
    new->prev = NULL;
    new->position = 0;
    new->speed = 1;

    pthread_mutex_lock(&queue_waiting_lock);
    queue_add(&waiting, new);
    //printf("added: %d\n", waiting != NULL);
    pthread_mutex_unlock(&queue_waiting_lock);

    result = 1;
  }
  return(result);
}

t_queue *queue_next(t_queue **queue, jack_nframes_t now) {
  t_queue *result = NULL;
  if (*queue != NULL && (*queue)->start <= now) {
    result = *queue;
    *queue = (*queue)->next;
    if ((*queue) != NULL) {
      (*queue)->prev = NULL;
    }
  }

  if (*queue != NULL && (*queue)->start > now) {
    //printf("diff %f - %f = %f\n", *queue->start, now, *queue->start - now);
  }

  return(result);
}

void dequeue(jack_nframes_t now) {
  t_queue *p;
  pthread_mutex_lock(&queue_waiting_lock);
  while ((p = queue_next(&waiting, now)) != NULL) {
    int s = queue_size(playing);
    //printf("dequeuing %s @ %d\n", p->sound->samplename, p->start);
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
  t_queue *p = playing;
  
  for (channel = 0; channel < CHANNELS; ++channel) {
    buffers[channel][frame] = 0;
  }
  //printf("ah\n");
  while (p != NULL) {
    int channels;
    t_queue *tmp;
    //printf("compare start %d with frametime %d\n", p->start, frametime);
    if (p->start > frametime) {
      p = p->next;
      continue;
    }
    //printf("playing %s\n", p->sound->samplename);
    channels = p->sound->sample->info->channels;
    //printf("channels: %d\n", channels);
    for (channel = 0; channel < channels; ++channel) {
      /* todo tween */
      buffers[channel][frame] += 
        p->sound->sample->frames[(channels * ((int) p->position)) + channel];
    }

    p->position += p->speed;
    //printf("position: %d of %d\n", p->position, playing->sound->sample->info->frames);

    /* remove dead sounds */
    tmp = p;
    p = p->next;
    if (tmp->position >= tmp->sound->sample->info->frames) {
      //printf("remove %s\n", tmp->sound->samplename);
      queue_remove(&playing, tmp);
    }
  }
}

extern int audio_callback(int frames, float **buffers) {
  int i;
  jack_nframes_t now;

  if (offset == 0) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    offset = ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0)) 
      - ((double) jack_get_time() / 1000000.0);
    //printf("jack time: %d tv_sec %d offset: %f\n", jack_get_time(), tv.tv_sec, offset);
  }
  
  now = jack_last_frame_time(client);
  
  for (i=0; i < frames; ++i) {
    dequeue(now + frames);

    playback(buffers, i, now + i);
  }
  return(0);
}
