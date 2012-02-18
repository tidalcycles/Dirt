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

extern void file_set_samplerate(int s);
extern t_sample *file_get(char *samplename);
