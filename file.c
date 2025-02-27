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
#include "log.h"
//#include "segment.h"

t_sample *samples[MAXSAMPLES];
int sample_count = 0;

pthread_mutex_t mutex_samples;

int g_allow_unsafe_paths = 0;

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

#ifdef _WIN32
#include <shlwapi.h>
#define realpath(name, resolved) _fullpath((resolved), (name), PATH_MAX)
#define SEPARATOR '\\'
#else
#define SEPARATOR '/'
#endif

int is_absolute_path(const char *name) {
#ifdef _WIN32
  return ! PathIsRelative(name);
#else
  return name[0] == '/';
#endif
}

int path_inside(const char *haystack, const char *needle)
{
  char *haystack_path = realpath(haystack, NULL);
  char *needle_path = realpath(needle, NULL);
  int allowed =
    haystack_path &&
    needle_path &&
    0 == strncmp(haystack_path, needle_path, strlen(haystack_path)) &&
    (needle_path[strlen(haystack_path)] == '/' ||
     needle_path[strlen(haystack_path)] == SEPARATOR) ;
  free(haystack_path);
  free(needle_path);
  return allowed;
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

  //log_printf(LOG_OUT, "start frames: %d\n", sample->info->frames);
  //log_printf(LOG_OUT, "compare %d and %d\n", sample->info->samplerate, g_samplerate);
  if (sample->info->samplerate == g_samplerate) {
    return;
  }
  data.src_ratio = (float) g_samplerate / (float) sample->info->samplerate;
  //log_printf(LOG_OUT, "ratio: %d / %d = %f\n", sample->info->samplerate, samplerate, data.src_ratio);
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
  //log_printf(LOG_OUT, "end samplerate: %d frames: %d\n", (int) sample->info->samplerate, sample->info->frames);
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
  static int warned_samples_count = 0; // only print the too-many-samples message once
  static int warned_samples_error = 0; // only print the too-many-errors message once
  static char warned_samples[1024] = {0}; // failed-to-load sample names (without :n)
  t_sample* sample;
  sample = find_sample(samplename);

  // If sample was not in cache, read it from disk asynchronously
  if (sample == NULL) {

    // if the sample cache is full, don't bother trying to load
    int worth_trying;
    pthread_mutex_lock(&mutex_samples);
    worth_trying = sample_count < MAXSAMPLES;
    pthread_mutex_unlock(&mutex_samples);
    if (! worth_trying)
    {
      if (! warned_samples_count)
      {
        warned_samples_count = 1;
        log_printf(LOG_ERR, "too many samples, not loading any more\n");
      }
      return NULL;
    }
    // another thread might get in before we do, checked later...

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
    if (3 == sscanf(samplename, "%[a-z0-9A-Z]%[/:]%d", set, sep, &set_n)) {
      int n;
      snprintf(path, sizeof(path), "%s/%s", sampleroot, set);
      //log_printf(LOG_OUT, "looking in %s\n", set);
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
      if (is_absolute_path(samplename)) {
        strncpy(path, samplename, sizeof(path) - 1);
      } else {
        snprintf(path, sizeof(path), "%s/%s", sampleroot, samplename);
      }
    }

    info = (SF_INFO *) calloc(1, sizeof(SF_INFO));

    //log_printf(LOG_OUT, "opening %s.\n", path);
    if (! (g_allow_unsafe_paths || path_inside(sampleroot, path)))
    {
      // file is forbidden
      // only print the message once per sample
      char samplebase[MAXPATHSIZE];
      samplebase[0] = ' ';
      strncpy(samplebase + 1, samplename, sizeof(samplebase) - 2);
      char *colon = strchr(samplebase, ':');
      if (colon) {
        colon[0] = ' ';
        colon[1] = 0;
      }
      int warn = 0;
      pthread_mutex_lock(&mutex_samples);
      if (! strstr(warned_samples, samplebase))
      {
        if (strlen(warned_samples) + strlen(samplebase) < sizeof(warned_samples) - 1) {
          strncat(warned_samples, samplebase, sizeof(warned_samples) - 1);
          warn = 1;
        } else {
          warn = 2;
        }
      }
      pthread_mutex_unlock(&mutex_samples);
      if (warn == 1) {
        log_printf(LOG_ERR, "forbidden sound file %s for sample %s\n", path, samplename);
      } else if (warn == 2) {
        if (! warned_samples_error) {
          warned_samples_error = 1;
          log_printf(LOG_ERR, "too many sound file errors, not printing any more\n");
        }
      }

    } else if ((sndfile = (SNDFILE *) sf_open(path, SFM_READ, info)) == NULL) {
      // only print the message once per sample
      char samplebase[MAXPATHSIZE];
      samplebase[0] = ' ';
      strncpy(samplebase + 1, samplename, sizeof(samplebase) - 2);
      char *colon = strchr(samplebase, ':');
      if (colon) {
        colon[0] = ' ';
        colon[1] = 0;
      }
      int warn = 0;
      pthread_mutex_lock(&mutex_samples);
      if (! strstr(warned_samples, samplebase))
      {
        if (strlen(warned_samples) + strlen(samplebase) < sizeof(warned_samples) - 1) {
          strncat(warned_samples, samplebase, sizeof(warned_samples) - 1);
          warn = 1;
        } else {
          warn = 2;
        }
      }
      pthread_mutex_unlock(&mutex_samples);
      if (warn == 1) {
        log_printf(LOG_ERR, "could not open sound file %s for sample %s\n", path, samplename);
      } else if (warn == 2) {
        if (! warned_samples_error) {
          warned_samples_error = 1;
          log_printf(LOG_ERR, "too many sound file errors, not printing any more\n");
        }
      }
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

    if (sample) {
      fix_samplerate(sample);
      sample->onsets = NULL;
      //sample->onsets = segment_get_onsets(sample);
    }
    // else an error message will already have been printed

    // If sample was succesfully read, load it into cache if there is space
    if (sample) {
      int failed = 0;
      pthread_mutex_lock(&mutex_samples);
      if (sample_count < MAXSAMPLES)
      {
        samples[sample_count++] = sample;
      }
      else
      {
        // another thread got in before us
        failed = 1;
      }
      pthread_mutex_unlock(&mutex_samples);
      if (failed)
      {
        if (! warned_samples_count)
        {
          warned_samples_count = 1;
          log_printf(LOG_ERR, "too many samples, not loading any more\n");
        }
        // don't leak the sample data
        free(sample->info);
        free(sample->items);
        free(sample);
        sample = NULL;
      }
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
  log_printf(LOG_ERR, "preloading ..\n");
  while((dent = readdir(srcdir)) != NULL) {
    struct stat st;
    if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
      continue;

#ifdef _WIN32
    snprintf(samplename, sizeof(samplename), "%s/%s", sampleroot, dent->d_name);
    if (stat(samplename, &st) < 0) {
      continue;
    }
#else
    if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
      continue;
    }
#endif

    if (S_ISDIR(st.st_mode)) {
      int n = file_count_samples(dent->d_name, sampleroot);
      for (int i = 0; i < n; ++i) {
        snprintf(samplename, sizeof(samplename), "%s:%d", dent->d_name, i);
        t_sample *s = file_get(samplename, sampleroot);
        if (! s) goto done;
        log_printf(LOG_ERR, "> %s\n", s->name);
      }
    }
  }
done:
  log_printf(LOG_ERR, "preload done.\n");
  closedir(srcdir);
}

extern void file_init(int allow_unsafe_sample_paths)
{
  g_allow_unsafe_paths = allow_unsafe_sample_paths;
  pthread_mutex_init(&mutex_samples, NULL);
  if (sample_count < MAXSAMPLES)
  {
    // add tiny silent sample with name ""
    // this is to allow setting global effects (delaytime, delayfeedback)
    // without having to do hacks like
    // choosing a known-existing sample and setting gain to 0
    t_sample *silence = calloc(1, sizeof(*silence));
    silence->info = calloc(1, sizeof(*silence->info));
    silence->info->channels = 1;
    silence->info->frames = 1;
    silence->info->samplerate = g_samplerate;
    silence->items = calloc(1, sizeof(*silence->items));
    samples[sample_count++] = silence;
  }
}
