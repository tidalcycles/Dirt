#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <assert.h>
#include <dirent.h>

#include "common.h"
#include "config.h"

#ifdef JACK
#include "jack.h"
#else
#include "portaudio.h"

PaStream *stream;
#define PA_FRAMES_PER_BUFFER 1024
#endif

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

#ifdef JACK
jack_client_t *jack_client = NULL;
#endif
static int samplerate = 0;
float compression_speed = -1;

float delay_time = 0.1;
float delay_feedback = 0.7;

bool use_dirty_compressor = false;


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
      if (tmp->startT > new->startT) {
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

void init_formant_history(t_sound *sound) {
  bool failed = false;

  sound->formant_history = malloc(g_num_channels * sizeof(double*));
  if (!sound->formant_history) {
    failed = true;
  }

  for (int c = 0; !failed && c < g_num_channels; c++) {
    sound->formant_history[c] = calloc(10, sizeof(double));
    if (!sound->formant_history[c]) failed = true;
  }

  if (failed) {
    fprintf(stderr, "no memory to allocate `formant_history' array\n");
    exit(1);
  }
}

void free_formant_history(t_sound *sound) {
  if (sound->formant_history) {
    for (int c = 0; c < g_num_channels; c++) {
      double* fh = sound->formant_history[c];
      if (fh) free(fh);
    }
    free(sound->formant_history);
  }
}

void init_vcf (t_sound *sound) {
  sound->vcf = malloc(g_num_channels * sizeof(t_vcf));
  if (!sound->vcf) {
    fprintf(stderr, "no memory to allocate vcf struct\n");
    exit(1);
  }

  for (int channel = 0; channel < g_num_channels; ++channel) {
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

void init_hpf (t_sound *sound) {
  sound->hpf = malloc(g_num_channels * sizeof(t_vcf));
  if (!sound->hpf) {
    fprintf(stderr, "no memory to allocate hpf struct\n");
    exit(1);
  }

  for (int channel = 0; channel < g_num_channels; ++channel) {
    t_vcf *vcf = &(sound->hpf[channel]);
    vcf->f     = 2 * sound->hcutoff;
    vcf->k     = 3.6 * vcf->f - 1.6 * vcf->f * vcf->f -1;
    vcf->p     = (vcf->k+1) * 0.5;
    vcf->scale = exp((1-vcf->p)*1.386249);
    vcf->r     = sound->hresonance * vcf->scale;
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

void init_bpf (t_sound *sound) {
  sound->bpf = malloc(g_num_channels * sizeof(t_vcf));
  if (!sound->bpf) {
    fprintf(stderr, "no memory to allocate bpf struct\n");
    exit(1);
  }
  // I've changed the meaning of some of these a bit
  for (int channel = 0; channel < g_num_channels; ++channel) {
    t_vcf *vcf = &(sound->bpf[channel]);
    vcf->f     = sound->bandf;
    vcf->r     = sound->bandq;
    vcf->k     = vcf->f / vcf->r;
    vcf->p     = 2.0 - vcf->f * vcf->f;
    vcf->scale = 1.0 / (1.0 + vcf->k);
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

void free_vcf(t_sound *sound) {
  if (sound->vcf) free(sound->vcf);
}
void free_hpf(t_sound *sound) {
  if (sound->hpf) free(sound->hpf);
}
void free_bpf(t_sound *sound) {
  if (sound->bpf) free(sound->bpf);
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

float effect_hpf(float in, t_sound *sound, int channel) {
  t_vcf *vcf = &(sound->hpf[channel]);
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
  
  return (in - vcf->y4);
}

float effect_bpf(float in, t_sound *sound, int channel) {
  t_vcf *vcf = &(sound->bpf[channel]);
  vcf->x  = in;

  vcf->y3 = vcf->p * vcf->y2 - vcf->y1 + vcf->k * (vcf->x - vcf->oldx +
        vcf->y2);
  vcf->y3 = vcf->scale * vcf->y3;
  
  vcf->oldx  = vcf->x;
  vcf->y1 = vcf->y2;
  vcf->y2 = vcf->y3;
  return (vcf->y3);
}

/**/

/**/

void add_delay(t_line *line, float sample, float delay, float feedback) {
  float rand_no = rand() / RAND_MAX;
  int point = (line->point + (int) ( (delay + (rand_no * 0.2)) * MAXLINE )) % MAXLINE;

  //printf("'feedback': %f\n", feedback);
  line->samples[point] += (sample * feedback);
}

/**/

float shift_delay(t_line *line) {
  float result = line->samples[line->point];
  line->samples[line->point] = 0;
  line->point = (line->point + 1) % MAXLINE;
  return(result);
}

/**/

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
      free_vcf(result);
      free_hpf(result);
      free_bpf(result);
      free_formant_history(result);
      memset(result, 0, sizeof(t_sound));
      break;
    }
  }
  return(result);
}

extern int audio_play(double when, char *samplename, float offset, float
      start, float end, float speed, float pan, float velocity, int vowelnum,
      float cutoff, float resonance, float accelerate, float shape, int
      kriole_chunk, float gain, int cutgroup, float delay, float delaytime,
      float delayfeedback, float crush, int coarse, float hcutoff, float
      hresonance, float bandf, float bandq) {
  struct timeval tv;
#ifdef FEEDBACK
  int is_kriole = 0;
#endif
  t_sample *sample = NULL;
  t_sound *new;
  
  gettimeofday(&tv, NULL);

  if (delay > 1) {
    delay = 1;
  }

  if (delaytime > 1) {
    delaytime = 1;
  }

  if (delayfeedback >= 1) {
    delayfeedback = 0.9999;
  }
  if (delayfeedback < 0) {
    delayfeedback = 0;
  }


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

#ifdef JACK
  new->startT = 
    jack_time_to_frames(jack_client, ((when-epochOffset) * 1000000));
# else
  new->startT = when - epochOffset;
#endif

  new->next = NULL;
  new->prev = NULL;
  new->reverse  = speed < 0;
  new->speed    = fabsf(speed);
  new->pan      = pan;
  if (new->channels == 2 && g_num_channels == 2 && new->pan == 0.5) {
    new->pan = 0;
  }
  else {
    new->mono = 1;
  }
#ifdef FAKECHANNELS
  new->pan *= (float) g_num_channels / FAKECHANNELS;
#endif
#ifdef SCALEPAN
  if (g_num_channels > 2) {
    new->pan *= (float) g_num_channels;
  }
#endif
  new->velocity = velocity;

  init_formant_history(new);
  
  new->offset = offset;

  new->cutoff = cutoff;
  new->hcutoff = hcutoff;
  new->resonance = resonance;
  new->hresonance = hresonance;
  new->bandf = bandf;
  new->bandq = bandq;
  new->cutgroup = cutgroup;

  if (shape != 0) {
    new->shape = 1;
    new->shape_k = (2.0f * shape) / (1.0f - shape);
  }
  if (crush != 0) {
     new->crush = 1;
     new->crush_bits = crush;
  }
  if (coarse != 0) {
     new->coarse = coarse;
     new->coarse_ind = 0;
     new->coarse_last = 0;
  }

  init_vcf(new);
  init_hpf(new);
  init_bpf(new);

  new->accelerate = accelerate;
  new->delay = delay;

  if (delaytime >= 0) {
    delay_time = delaytime;
  }
  if (delay_feedback >= 0) {
    delay_feedback = delayfeedback;
  }

  if (new->reverse) {
    float tmp;
    tmp = start;
    start = 1 - end;
    end = 1 - tmp;
  }


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
  new->position = new->start;
  //printf("position: %f\n", new->position);
  new->formant_vowelnum = vowelnum;
  new->gain = powf(gain/2, 4);
  
  pthread_mutex_lock(&queue_waiting_lock);
  queue_add(&waiting, new);
  //printf("added: %d\n", waiting != NULL);
  pthread_mutex_unlock(&queue_waiting_lock);
  
  return(1);
}

t_sound *queue_next(t_sound **queue, sampletime_t now) {
  t_sound *result = NULL;
  //printf("%f vs %f\n", *queue == NULL ? 0 : (*queue)->startT, now);
  if (*queue != NULL && (*queue)->startT <= now) {
    result = *queue;
    *queue = (*queue)->next;
    if ((*queue) != NULL) {
      (*queue)->prev = NULL;
    }
  }

  return(result);
}

void cut(t_sound *s) {
  t_sound *p = NULL;
  p = playing;

  int group = s->cutgroup;

  if (group != 0) {
    while (p != NULL) {
      // If group is less than 0, only cut playback of the same sample
      if (p->cutgroup == group && (group > 0 || p->sample == s->sample)) {
        // schedule this sound to end in ROUNDOFF samples time, so we
        // don't get a click
        float newend = p->position + ROUNDOFF;
        // unless it's dying soon anyway..
        if (newend < p->end) {
          p->end = newend;
        }
      }
      p = p->next;
    }
  }
}

void dequeue(sampletime_t now) {
  t_sound *p;
  pthread_mutex_lock(&queue_waiting_lock);
  while ((p = queue_next(&waiting, now)) != NULL) {
#ifdef DEBUG
    int s = queue_size(playing);
#endif
    
    cut(p);
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

void playback(float **buffers, int frame, sampletime_t now) {
  int channel;
  t_sound *p = playing;

  for (channel = 0; channel < g_num_channels; ++channel) {
    buffers[channel][frame] = 0;
  }

  while (p != NULL) {
    int channels;
    t_sound *tmp;
    
    if (p->startT > now) {
      p->checks++;
      p = p->next;
      continue;
    }
    if ((!p->started) && p->checks == 0 && p->startT < now) {
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
        value = p->items[(channels * (p->reverse ? (p->sample->info->frames - (int) p->position) : (int) p->position)) + channel];
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
          p->items[(channels * (p->reverse ? p->sample->info->frames - pos : pos))
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
      if (p->hresonance > 0 && p->hresonance < 1 
          && p->hcutoff > 0 && p->hcutoff < 1) {
        value = effect_hpf(value, p, channel);
      }
      if (p->bandf > 0 && p->bandf < 1 && p->bandq > 0) {
         value = effect_bpf(value, p, channel);
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

      if (p->coarse) {
         (p->coarse_ind)++;
         if (p->coarse_ind == p->coarse) {
            p->coarse_ind = 0;
            p->coarse_last = value;
         } else {
            value = p->coarse_last;
         }
      }
      if (p->shape) {
        value = (1+p->shape_k)*value/(1+p->shape_k*fabs(value));
      }
      if (p->crush) {
        //value = (1.0 + log(fabs(value)) / 16.63553) * (value / fabs(value));
        value = trunc(pow(2,p->crush_bits-1) * value) / pow(2,p->crush_bits-1);
        //value = exp( (fabs(value) - 1.0) * 16.63553 ) * (value / fabs(value));
      }

      value *= p->gain;
      value *= roundoff;

      float c = (float) channel + p->pan;
      float d = c - floor(c);
      int channel_a =  ((int) c) % g_num_channels;
      int channel_b =  ((int) c + 1) % g_num_channels;

      if (channel_a < 0) {
        channel_a += g_num_channels;
      }
      if (channel_b < 0) {
        channel_b += g_num_channels;
      }

      // equal power panning
      float tmpa = value * cos(HALF_PI * d);
      float tmpb = value * sin(HALF_PI * d);

      buffers[channel_a][frame] += tmpa;
      buffers[channel_b][frame] += tmpb;

      add_delay(&delays[channel_a], tmpa, delay_time, p->delay);
      add_delay(&delays[channel_b], tmpb, delay_time, p->delay);

      if (p->mono) {
        break;
      }
    }
      
    if (p->accelerate != 0) {
      // ->startFrame ->end ->position
      p->speed += p->accelerate/samplerate;
    }
    p->position += p->speed;

    //printf("position: %d of %d\n", p->position, playing->end);

    /* remove dead sounds */
    tmp = p;
    p = p->next;
    if (tmp->position >= tmp->end || tmp->position < tmp->start) {
      //printf("remove %s %f\n", tmp->samplename, tmp->position);
      queue_remove(&playing, tmp);
    }
  }

  for (channel = 0; channel < g_num_channels; ++channel) {
    float tmp = shift_delay(&delays[channel]);
    if (delay_feedback != 0) {
      add_delay(&delays[channel], tmp, delay_time, delay_feedback);
    }
    buffers[channel][frame] += tmp;
  }

  if (use_dirty_compressor) {
    float max = 0;

    for (channel = 0; channel < g_num_channels; ++channel) {
      if (fabsf(buffers[channel][frame]) > max) {
        max = buffers[channel][frame];
      }
    }
    float factor = compress(max);
    for (channel = 0; channel < g_num_channels; ++channel) {
      buffers[channel][frame] *= factor * 0.4;
    }
  } else {
    for (channel = 0; channel < g_num_channels; ++channel) {
      buffers[channel][frame] *= 2;
    }
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

#ifdef JACK
extern int jack_callback(int frames, float *input, float **outputs) {
    sampletime_t now;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    epochOffset = ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0)) 
      - ((double) jack_get_time() / 1000000.0);
    //printf("jack time: %d tv_sec %d epochOffset: %f\n", jack_get_time(), tv.tv_sec, epochOffset);

  now = jack_last_frame_time(jack_client);
  
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
    dequeue(now + i);
  }
  return(0);
}
#else


static int pa_callback(const void *inputBuffer, void *outputBuffer,
		       unsigned long framesPerBuffer,
		       const PaStreamCallbackTimeInfo* timeInfo,
		       PaStreamCallbackFlags statusFlags,
		       void *userData) {
  
  struct timeval tv;
  
  if (epochOffset == 0) {
    gettimeofday(&tv, NULL);
    epochOffset = ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0))
      - timeInfo->outputBufferDacTime;
    printf("set offset (%f - %f) to %f\n", ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0))
	   , timeInfo->outputBufferDacTime, epochOffset);
  }

  double now = timeInfo->outputBufferDacTime;

  float **buffers = (float **) outputBuffer;

  for (int i=0; i < framesPerBuffer; ++i) {
    double framenow = now + (((double) i)/((double) samplerate));
    //printf("i: %d %f\n", i, framenow);
    playback(buffers, i, framenow);
    dequeue(framenow);
  }
  return paContinue;
}
#endif


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

#ifdef JACK
void jack_init(bool autoconnect) {
  jack_client = jack_start(jack_callback, autoconnect);
  samplerate = jack_get_sample_rate(jack_client);
}
#else

static void StreamFinished( void* userData ) {
  printf( "Stream Completed\n");
}

void pa_init(void) {
  PaStreamParameters outputParameters;

  PaError err;

  printf("init pa\n");
  samplerate = 44100;

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
            samplerate,
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
#endif

extern void audio_init(bool dirty_compressor, bool autoconnect) {
  struct timeval tv;

  atexit(audio_close);

  gettimeofday(&tv, NULL);
  starttime = (float) tv.tv_sec + ((float) tv.tv_usec / 1000000.0);
#ifdef FEEDBACK
  loop = new_loop(60 * 60);
#endif

  delays = calloc(g_num_channels, sizeof(t_line));
  if (!delays) {
    fprintf(stderr, "no memory to allocate `delays' array\n");
    exit(1);
  }

  pthread_mutex_init(&queue_waiting_lock, NULL);
#ifdef JACK
  jack_init(autoconnect);
#else
  pa_init();
#endif
printf("hm.\n");
  compression_speed = 1000 / samplerate;
  file_set_samplerate(samplerate);

#ifdef FEEDBACK
  pitch_init(loop, samplerate);
#endif

  use_dirty_compressor = dirty_compressor;
}

extern void audio_close(void) {
#ifdef FEEDBACK
  free_loop(loop);
#endif
  if (delays) free(delays);

  // free all active sounds, if any
  for (int i = 0; i < MAXSOUNDS; ++i) {
    if (sounds[i].active) {
      free_vcf(&sounds[i]);
      free_hpf(&sounds[i]);
      free_bpf(&sounds[i]);
      free_formant_history(&sounds[i]);
    }
  }
}
