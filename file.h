#include <sndfile.h>

#define MAXSAMPLES 1024
#define MAXFILES 4096
#define MAXPATHSIZE 256

#define SAMPLEROOT "./samples"

typedef struct {
  char name[MAXPATHSIZE];
  SF_INFO *info;
  float *items;
  int *onsets;
} t_sample;

typedef struct {
  unsigned int frames;
  unsigned int now;
  float *items;
} t_loop;

extern void file_set_samplerate(int s);
extern t_sample *file_get(char *samplename);
t_loop *new_loop(float seconds);
