#include "file.h"

typedef struct {
  char samplename[MAXPATHSIZE+1];
  t_sample *sample;
  double position;
  double speed;
} t_sound;


typedef struct t_node {
  double when;
  t_sound *sound;
  struct t_node *next, *last;
  double position;
} t_queue;

extern int audio_callback(int samples, float *buffer);
extern void audio_init(void);
extern int audio_play(char *samplename);
