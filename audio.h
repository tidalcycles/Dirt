#include <jack/jack.h>
#include "file.h"
#include "jack.h"

#define MAXDELAYS 16
#define MAXDELAY 44100

#define ROUNDOFF 8

typedef struct t_node {
  jack_nframes_t startFrame;
  char samplename[MAXPATHSIZE+1];
  int is_loop;
  union {
    t_sample *sample;
    t_loop *loop;
  };
  unsigned int loop_start;
  int channels;
  float *items;
  struct t_node *next, *prev;
  double position;
  float speed;
  float pan;
  float offset;
  float frames;
  float startframe;
  float velocity;
  double formant_history[CHANNELS][10];
  int    formant_vowelnum;
} t_sound;

extern int audio_callback(int frames, float *input, float **outputs);
extern void audio_init(void);
extern int audio_play(double when, char *samplename, float offset, float start, float end, float speed, float pan, float velocity, int vowelnum);
