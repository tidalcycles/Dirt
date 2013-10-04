#include <jack/jack.h>
#include "file.h"
#include "jack.h"
#include "config.h"

#define MAXLINE  44100
#define MAXSOUNDS 1024
#define ROUNDOFF 128

typedef struct {
 float cutoff;
 float res;
 float f;
 float k;
 float p;
 float scale;
 float r;
 float y1;
 float y2;
 float y3;
 float y4;
 float oldx;
 float oldy1;
 float oldy2;
 float oldy3;
 float x;
} t_vcf;

typedef struct {
  float samples[MAXLINE];
  int   point;
} t_line;

typedef struct t_node {
  int    active;
  jack_nframes_t startFrame;
  char samplename[MAXPATHSIZE+1];
  int is_loop;
  union {
    t_sample *sample;
    t_loop *loop;
  };
  unsigned int loop_start;
  int    channels;
  float  *items;
  struct t_node *next, *prev;
  double position;
  float  speed;
  int    reverse;
  float  pan;
  float  offset;
  float  start;
  float  end;
  float  velocity;
  double formant_history[CHANNELS][10];
  int    formant_vowelnum;
  float  cutoff;
  float  resonance;
  t_vcf  vcf[CHANNELS];
  float  accellerate;
  int    shape;
  float  shape_k;
  int    kriole_chunk;
  int    is_kriole;
  int    started;
  int    checks;
  float  delay_in;
} t_sound;



extern int audio_callback(int frames, float *input, float **outputs);
extern void audio_init(void);
extern int audio_play(double when, char *samplename, float offset, float start, float end, float speed, float pan, float velocity, int vowelnum, float cutoff, float resonance, float accellerate, float shape, int kriole_chunk);

extern void audio_kriole(double when, 
                         float duration, 
                         float pitch_start, 
                         float pitch_stop
                         );

#ifdef FEEDBACK
void preload_kriol(char *dir);
void audio_pause_input(int paused);
#endif
