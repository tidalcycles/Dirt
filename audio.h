#include "file.h"
#include "config.h"
#include "common.h"

#define MAXLINE  44100
#define MAXSOUNDS 512
#define ROUNDOFF 16
#define MAX_DB 12

#ifdef JACK
#include <jack/jack.h>
#include "jack.h"
#define sampletime_t jack_nframes_t
#else
#define sampletime_t double
#endif


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
  int index;
  float last;
  float sum;
} t_crs;

typedef struct {
  float samples[MAXLINE];
  int   point;
} t_line;

t_line* delays;
float line_feedback_delay;

typedef struct t_node {
  int    active;
  sampletime_t startT;
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
  double **formant_history;
  int    formant_vowelnum;
  float  cutoff;
  float  resonance;
  t_vcf  *vcf;
  float  accelerate;
  int    shape;
  float  shape_k;
  int    kriole_chunk;
  int    is_kriole;
  int    started;
  int    checks;
  float  delay;
  float delaytime;
  float delayfeedback;
  float  gain;
  int    cutgroup;
  int    mono;
  int    crush;
  float  crush_bits;
  int    coarse;
  t_crs  *coarsef;
  float  hcutoff;
  float  hresonance;
  t_vcf  *hpf;
  float  bandf;
  float  bandq;
  t_vcf  *bpf;
  int    sample_loop;
  int    cut_continue;
  char   unit;
  float  cps;
  double when;
  float  attack;
  float  hold;
  float  release;
  float  playtime;
} t_sound;

typedef struct {
  double when;
  float cps;
  char *samplename;

  float start;
  float end;
  float speed;
  float pan;
  float velocity;
  int vowelnum;
  float cutoff;
  float resonance;
  float accelerate;
  float shape;
  int kriole_chunk;
  float gain;
  int cutgroup;
  float delay;
  float delaytime;
  float delayfeedback;
  float crush;
  int coarse;
  float hcutoff;
  float hresonance;
  float bandf;
  float bandq;
  char unit;
  int sample_loop;
  int sample_n;
  float attack;
  float hold;
  float release;
} t_play_args;



extern int audio_callback(int frames, float *input, float **outputs);
extern void audio_init(bool dirty_compressor, bool autoconnect, bool late_trigger, unsigned int num_workers, char *sampleroot, bool shape_gain_comp);
extern void audio_close(void);
extern int audio_play(t_sound*);
t_sound *new_sound();

