#include <sndfile.h>
#include <samplerate.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <pthread.h>

#include "common.h"
#include "file.h"
#include "segment.h"

t_sample *samples[MAXSAMPLES];
int sample_count = 0;

int samplerate = 44100;

bool loading_file = false;
bool mutex_init = false;

pthread_t file_thread;
pthread_mutex_t mutex_loading_file;
pthread_mutex_t mutex_samples;


extern void file_set_samplerate(int s) {
  samplerate = s;
}

t_loop *new_loop(float seconds) {
  t_loop *result = (t_loop *) calloc(1, sizeof(t_loop));
  //result->chunksz = 2048 * 2;
  result->chunksz = 2048;
  result->frames = seconds * (float) samplerate;
  result->items = (float *) calloc(result->frames, sizeof(double));
  result->in = (double *) calloc(result->chunksz, sizeof(double));
  result->now = 0;
  result->loops = 0;
  return(result);
}

void free_loop(t_loop *loop) {
  if (loop) {
    if (loop->items) free(loop->items);
    if (loop->in) free(loop->in);
    free(loop);
  }
}

t_sample *find_sample (char *samplename) {
  int c;
  t_sample *sample = NULL;

  pthread_mutex_lock(&mutex_samples);

  for(c = 0; c < sample_count; ++c) {
    if(strcmp(samples[c]->name, samplename) == 0) {
      sample = samples[c];
      break;
    }
  }

  pthread_mutex_unlock(&mutex_samples);

  return(sample);
}

int wav_filter (const struct dirent *d) {
  if (strlen(d->d_name) > 4) {
    return(strcmp(d->d_name + strlen(d->d_name) - 4, ".wav") == 0
           || strcmp(d->d_name + strlen(d->d_name) - 4, ".WAV") == 0
           );
  }
  return(0);
}

void fix_samplerate (t_sample *sample) {
  SRC_DATA data;
  int max_output_frames;
  int channels = sample->info->channels;

  //printf("start frames: %d\n", sample->info->frames);
  if (sample->info->samplerate == samplerate) {
    return;
  }
  data.src_ratio = (float) samplerate / (float) sample->info->samplerate;
  //printf("ratio: %d / %d = %f\n", sample->info->samplerate, samplerate, data.src_ratio);
  max_output_frames = sample->info->frames * data.src_ratio + 32;

  data.data_in = sample->items;
  data.input_frames = sample->info->frames;

  data.data_out = (float *) calloc(1, sizeof(float) 
                                      * max_output_frames 
                                      * channels
                                   );
  data.output_frames = max_output_frames;
  
  src_simple(&data, SRC_SINC_BEST_QUALITY, channels);
  
  if (sample->items) free(sample->items);
  sample->items = data.data_out;
  sample->info->samplerate = samplerate;
  sample->info->frames = data.output_frames_gen;
  //printf("end samplerate: %d frames: %d\n", (int) sample->info->samplerate, sample->info->frames);  
}

void* file_read_thr(void *args) {
  t_sample *sample = NULL;
  SNDFILE *sndfile;
  char path[MAXPATHSIZE];
  char error[62];
  sf_count_t count;
  float *items;
  SF_INFO *info;
  char set[MAXPATHSIZE];
  char sep[2];
  int set_n = 0;
  struct dirent **namelist;

  char* samplename = args;

  // load it from disk
  if (sscanf(samplename, "%[a-z0-9A-Z]%[/:]%d", set, sep, &set_n)) {
    int n;
    snprintf(path, MAXPATHSIZE -1, "%s/%s", SAMPLEROOT, set);
    //printf("looking in %s\n", set);
    n = scandir(path, &namelist, wav_filter, alphasort);
    if (n > 0) {
      snprintf(path, MAXPATHSIZE -1,
          "%s/%s/%s", SAMPLEROOT, set, namelist[set_n % n]->d_name);
      while (n--) {
        free(namelist[n]);
      }
      free(namelist);
    } else {
      snprintf(path, MAXPATHSIZE -1, "%s/%s", SAMPLEROOT, samplename);
    }
  } else {
    snprintf(path, MAXPATHSIZE -1, "%s/%s", SAMPLEROOT, samplename);
  }

  info = (SF_INFO *) calloc(1, sizeof(SF_INFO));

  printf("opening %s.\n", path);

  if ((sndfile = (SNDFILE *) sf_open(path, SFM_READ, info)) == NULL) {
    printf("nope.\n");
    free(info);
  } else {
    items = (float *) calloc(1, sizeof(float) * info->frames * info->channels);
    //snprintf(error, (size_t) 61, "hm: %d\n", sf_error(sndfile));
    //perror(error);
    count  = sf_read_float(sndfile, items, info->frames * info->channels);
    snprintf(error, (size_t) 61, "count: %d frames: %d channels: %d\n", (int) count, (int) info->frames, info->channels);
    perror(error);

    if (count == info->frames * info->channels) {
      sample = (t_sample *) calloc(1, sizeof(t_sample));
      strncpy(sample->name, samplename, MAXPATHSIZE - 1);
      sample->info = info;
      sample->items = items;

    } else {
      snprintf(error, (size_t) 61, "didn't get the right number of items: %d vs %d %d\n", (int) count, (int) info->frames * info->channels, sf_error(sndfile));
      perror(error);
      free(info);
      free(items);
    }
    sf_close(sndfile);
  }

  if (sample == NULL) {
    printf("failed.\n");
  } else {
    fix_samplerate(sample);
    sample->onsets = NULL;
    //sample->onsets = segment_get_onsets(sample);
  }

  if (sample) {
    pthread_mutex_lock(&mutex_samples);

    printf("saving sample %s on index %u\n", samplename, sample_count);
    samples[sample_count++] = sample;

    pthread_mutex_unlock(&mutex_samples);
  }

  pthread_mutex_lock(&mutex_loading_file);
  loading_file = false;
  pthread_mutex_unlock(&mutex_loading_file);

  free(samplename);

  pthread_exit(NULL);
}

extern t_sample *file_get(char *samplename) {
  t_sample *sample;

  if (!mutex_init) {
    pthread_mutex_init(&mutex_loading_file, NULL);
    pthread_mutex_init(&mutex_samples, NULL);
    mutex_init = true;
  }

  sample = find_sample(samplename);

  if (sample == NULL) {
    pthread_mutex_lock(&mutex_loading_file);
    if (!loading_file) {
      loading_file = true;
      pthread_mutex_unlock(&mutex_loading_file);

      char* name = malloc(strlen(samplename) + 1);
      strcpy(name, samplename);

      int rc = pthread_create(&file_thread, NULL, file_read_thr, (void*) name);
      if (rc) {
        printf("ERROR. Failed to create file reading thread. Code %d\n", rc);
        return NULL;
      }
    } else {
      pthread_mutex_unlock(&mutex_loading_file);
    }
  }

  return(sample);
}
