#include <sndfile.h>
#include <dirent.h>

#define MAXSAMPLES 1024
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
  double *in;
  //fvec_t *in;
  //fvec_t *ibuf;

  unsigned int win_s;
  unsigned int hop_s;
  unsigned int samplerate;
  unsigned int channels;

  int initialised;
} t_loop;

int wav_filter (const struct dirent *d);
extern void file_set_samplerate(int s);
extern t_sample *file_get(char *samplename, const char *sampleroot);
extern t_sample *file_get_from_cache(char *samplename);
t_loop *new_loop(float seconds);
void free_loop(t_loop*);
