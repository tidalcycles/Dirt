#include <sndfile.h>
#include <samplerate.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/stat.h>

#include "file.h"
#include "common.h"
#include "segment.h"

t_sample *samples[MAXSAMPLES];
int sample_count = 0;

pthread_mutex_t mutex_samples;
bool mutex_samples_init = false;

t_loop *new_loop(float seconds) {
  t_loop *result = (t_loop *) calloc(1, sizeof(t_loop));
  //result->chunksz = 2048 * 2;
  result->chunksz = 2048;
  result->max_frames = result->frames = seconds * (float) g_samplerate;
  result->items = (float *) calloc(result->frames, sizeof(double));
  result->in = (float *) calloc(result->chunksz, sizeof(double));
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

  for(c = 0; c < sample_count; ++c) {
    if(strcmp(samples[c]->name, samplename) == 0) {
      sample = samples[c];
      break;
    }
  }
  return(sample);
}

int wav_filter (const struct dirent *d) {
  if (strlen(d->d_name) > 4) {
    return(strcasecmp(d->d_name + strlen(d->d_name) - 4, ".wav") == 0);
  }
  return(0);
}

void fix_samplerate (t_sample *sample) {
  SRC_DATA data;
  int max_output_frames;
  int channels = sample->info->channels;

  //printf("start frames: %d\n", sample->info->frames);
  //printf("compare %d and %d\n", sample->info->samplerate, g_samplerate);
  if (sample->info->samplerate == g_samplerate) {
    return;
  }
  data.src_ratio = (float) g_samplerate / (float) sample->info->samplerate;
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
  sample->info->samplerate = g_samplerate;
  sample->info->frames = data.output_frames_gen;
  //printf("end samplerate: %d frames: %d\n", (int) sample->info->samplerate, sample->info->frames);
}

extern int file_count_samples(char *set, const char *sampleroot) {
  int result = 0;
  struct dirent **namelist;
  char path[MAXPATHSIZE * 2 + 24];

  snprintf(path, sizeof(path), "%s/%s", sampleroot, set);
  result = scandir(path, &namelist, wav_filter, alphasort);
  
  if (result >= 0) {
    free(namelist);
  }
  else {
    // Some error reading the folder
    result = 0;
  }
  
  return(result);
}

extern t_sample *file_get(char *samplename, const char *sampleroot) {
  t_sample* sample;

  sample = find_sample(samplename);

  // If sample was not in cache, read it from disk asynchronously
  if (sample == NULL) {
    // Initialize mutexes if needed
    if (!mutex_samples_init) {
      pthread_mutex_init(&mutex_samples, NULL);
      mutex_samples_init = true;
    }

    SNDFILE *sndfile;
    char path[2 * MAXPATHSIZE + 24];
    char error[62];
    sf_count_t count;
    float *items;
    SF_INFO *info;
    char set[MAXPATHSIZE];
    char sep[2];
    int set_n = 0;
    struct dirent **namelist;

    // load it from disk
    if (sscanf(samplename, "%[a-z0-9A-Z]%[/:]%d", set, sep, &set_n)) {
      int n;
      snprintf(path, sizeof(path), "%s/%s", sampleroot, set);
      //printf("looking in %s\n", set);
      n = scandir(path, &namelist, wav_filter, alphasort);
      if (n > 0) {
        snprintf(path, sizeof(path),
	    "%s/%s/%s", sampleroot, set, namelist[set_n % n]->d_name);
        while (n--) {
          free(namelist[n]);
        }
        free(namelist);
      } else {
	snprintf(path, sizeof(path), "%s/%s", sampleroot, samplename);
      }
    } else {
      snprintf(path, MAXPATHSIZE -1, "%s/%s", sampleroot, samplename);
    }

    info = (SF_INFO *) calloc(1, sizeof(SF_INFO));

    //printf("opening %s.\n", path);

    if ((sndfile = (SNDFILE *) sf_open(path, SFM_READ, info)) == NULL) {
      printf("nope.\n");
      free(info);
    } else {
      items = (float *) calloc(1, sizeof(float) * info->frames * info->channels);
      //snprintf(error, (size_t) 61, "hm: %d\n", sf_error(sndfile));
      //perror(error);
      count  = sf_read_float(sndfile, items, info->frames * info->channels);
      //snprintf(error, (size_t) 61, "count: %d frames: %d channels: %d\n", (int) count, (int) info->frames, info->channels);
      //perror(error);

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

    // If sample was succesfully read, load it into cache
    if (sample) {
      pthread_mutex_lock(&mutex_samples);
      samples[sample_count++] = sample;
      pthread_mutex_unlock(&mutex_samples);
    }
  }

  return(sample);
}

extern t_sample *file_get_from_cache(char *samplename) {
  return find_sample(samplename);
}


extern void file_preload_samples(const char *sampleroot) {
  struct dirent* dent;
  DIR* srcdir = opendir(sampleroot);

  // sample set name
  char samplename[MAXPATHSIZE + 24];
  
  if (srcdir == NULL) {
    return;
  }
  fprintf(stderr, "preloading ..\n");
  while((dent = readdir(srcdir)) != NULL) {
    struct stat st;
    if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
      continue;

    if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
      continue;
    }

    if (S_ISDIR(st.st_mode)) {
      int n = file_count_samples(dent->d_name, sampleroot);
      for (int i = 0; i < n; ++i) {
	snprintf(samplename, sizeof(samplename), "%s:%d", dent->d_name, i);
	fprintf(stderr, "> %s\n", samplename);
	file_get(samplename, sampleroot);
      }
    }
  }
  fprintf(stderr, "preload done.\n");
  closedir(srcdir);
}
