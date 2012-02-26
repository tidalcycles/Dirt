#include <jack/jack.h>
#include "file.h"

typedef struct {
  char samplename[MAXPATHSIZE+1];
  t_sample *sample;
} t_sound;


typedef struct t_node {
  jack_nframes_t startFrame;
  t_sound *sound;
  struct t_node *next, *prev;
  double position;
  float speed;
  float pan;
  float velocity;
} t_queue;

extern int audio_callback(int frames, float **buffers);
extern void audio_init(void);
extern int audio_play(double when, char *samplename, float offset, float duration, float speed, float pan, float velocity);
