#include <stdbool.h>

#include "file.h"
#include "config.h"
#include "common.h"

#define sampletime_t double

#define MAXLINE  44100
#define MAX_SOUNDS 512 // includes queue!

// not a hard limit, after this number sounds will start being
// culled (given ROUNDOFF samples to live to avoid
// discontinuities).
#define MAX_PLAYING 8

#define ROUNDOFF 16
#define MAX_DB 12

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

extern t_line* delays;
extern float line_feedback_delay;

typedef struct {
  struct { int channel; float value; } out[2];
} t_pan;

typedef struct {
  double formant_history[10];
  t_vcf  vcf;
  t_crs  coarsef;
  t_vcf  hpf;
  t_vcf  bpf;
  t_pan  pan;
} t_sound_per_channel;

struct t_node;
typedef struct t_node t_sound;

#define MAX_EFFECTS 10
typedef float (*t_effect)(float, t_sound *, int);

struct t_node {
  int    active;
  int    is_playing;
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
  float  position;
  float  speed;
  int    reverse;
  float  pan;
  float  offset;
  float  start;
  float  end;
  float  velocity;
  int    formant_vowelnum;
  float  cutoff;
  float  resonance;
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
  float  crush_range;
  int    coarse;
  float  hcutoff;
  float  hresonance;
  float  bandf;
  float  bandq;
  int    sample_loop;
  int    cut_continue;
  char   unit;
  float  cps;
  double when;
  float  attack;
  float  hold;
  float  release;
  float  playtime;
  int    orbit;
  int    played;
  char   do_formant_filter;
  char   do_effect_vcf;
  char   do_effect_hpf;
  char   do_effect_bpf;
  char   do_effect_bpf2;
  char   do_effect_coarse_pos;
  char   do_effect_coarse_neg;
  char   do_effect_shape;
  char   do_effect_crush_pos;
  char   do_effect_crush_neg;
  char   do_effect_env;
  t_sound_per_channel per_channel[MAX_CHANNELS];
};

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

#ifdef SEND_RMS
typedef struct {
  int n;
  float sum;
  float squares[RMS_SZ];
  float sum_of_squares;
} t_rms;
#endif

extern int audio_callback(int frames, float *input, float **outputs);
extern int audio_init(const char *output, bool dirty_compressor, bool autoconnect, bool late_trigger, unsigned int num_workers, const char *sampleroot, bool shape_gain_comp, bool preload_flag, bool output_time_flag);
extern void audio_close(void);
extern int audio_play(t_sound*);
t_sound *new_sound();

extern void playback(float **buffers, int frame, sampletime_t now);
void dequeue(sampletime_t now);
extern double epochOffset;
