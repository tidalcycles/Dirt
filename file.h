#include <sndfile.h>

#define MAXSAMPLES 512
#define MAXFILES 4096
#define MAXPATHSIZE 256

typedef struct {
  char name[MAXPATHSIZE];
  SF_INFO *info;
  float *items;
  int *onsets;
} t_sample;

typedef struct {
  unsigned int max_frames;
  unsigned int frames;
  unsigned int now;
  int loops;
  unsigned int chunksz;
  unsigned int since_chunk;
  unsigned int chunk_n;
  float *items;
  float *in;
  //fvec_t *in;
  //fvec_t *ibuf;

  unsigned int win_s;
  unsigned int hop_s;
  unsigned int samplerate;
  unsigned int channels;

  int initialised;
} t_loop;

extern void file_set_samplerate(int s);
extern t_sample *file_get(char *samplename, const char *sampleroot);
extern t_sample *file_get_from_cache(char *samplename);
t_loop *new_loop(float seconds);
void free_loop(t_loop*);
extern int file_count_samples(char *set, const char *sampleroot);
extern void file_preload_samples(const char *sampleroot);
