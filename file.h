
#define MAXSAMPLES 1024
#define MAXFILES 4096
#define MAXPATHSIZE 256

#define SAMPLEROOT "./"

typedef struct {
  char name[MAXPATHSIZE];
  SF_INFO *info;
  float *frames;
} t_sample;
