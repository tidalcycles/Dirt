#include <sndfile.h>
#include <samplerate.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>

#include "file.h"

t_sample *samples[MAXSAMPLES];
int sample_count = 0;

int samplerate = 44100;

extern void file_set_samplerate(int s) {
  samplerate = s;
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
    return(strcmp(d->d_name + strlen(d->d_name) - 4, ".wav") == 0);
  }
  return(0);
}

void fix_samplerate (t_sample *sample) {
  SRC_DATA data;
  int max_output_frames;
  int channels = sample->info->channels;

  data.src_ratio = sample->info->samplerate / samplerate;

  max_output_frames = sample->info->frames * data.src_ratio + 32;

  data.data_in = sample->frames;
  data.input_frames = sample->info->frames;

  data.data_out = (float *) malloc(sizeof(float) 
                                   * max_output_frames 
                                   * channels
                                   );
  data.output_frames = max_output_frames;

  src_simple(&data, SRC_SINC_BEST_QUALITY, channels);
  
  sample->frames = data.data_out;
  sample->info->samplerate = samplerate;
  sample->info->frames = data.output_frames_gen;
}

extern t_sample *file_get(char *samplename) {
  SNDFILE *sndfile;
  char path[MAXPATHSIZE];
  char error[62];
  t_sample *sample;
  sf_count_t count;
  float *frames;
  SF_INFO *info;
  char set[MAXPATHSIZE];
  int set_n = 0;
  struct dirent **namelist;

  printf("find %s\n", samplename);
  sample = find_sample(samplename);
  
  if (sample == NULL) {
    /* load it from disk */
    if (sscanf(samplename, "%[a-z0-9A-Z]/%d", set, &set_n)) {
      int n;
      snprintf(path, MAXPATHSIZE -1, "%s/%s", SAMPLEROOT, set);
      n = scandir(path, &namelist, wav_filter, alphasort);
      if (n > 0) {
        snprintf(path, MAXPATHSIZE -1, 
                 "%s/%s/%s", SAMPLEROOT, set, namelist[set_n % n]->d_name);
        while (n--) {
          free(namelist[n]);
        }
        free(namelist);
      }
      else {
        path[0] = '\0';
      }
    }
    else {
      snprintf(path, MAXPATHSIZE -1, "%s/%s", SAMPLEROOT, samplename);
    }
    info = (SF_INFO *) calloc(1, sizeof(SF_INFO));
    
    if ((sndfile = (SNDFILE *) sf_open(path, SFM_READ, info)) == NULL) {
      free(info);
    }
    else {
      frames = (float *) malloc(sizeof(float) * info->frames);
      /*snprintf(error, (size_t) 61, "hm: %d\n", sf_error(sndfile));
      perror(error);*/
      count  = sf_read_float(sndfile, frames, info->frames);
      /* snprintf(error, (size_t) 61, "hmm: %d vs %d %d\n", (int) count, (int) info->frames, sf_error(sndfile)); 
         perror(error);*/
      
      if (count == info->frames) {
        sample = (t_sample *) calloc(1, sizeof(t_sample));
        strncpy(sample->name, samplename, MAXPATHSIZE - 1);
        sample->info = info;
        sample->frames = frames;
        samples[sample_count++] = sample;
      }
      else {
	snprintf(error, (size_t) 61, "didn't get the right number of frames: %d vs %d %d\n", (int) count, (int) info->frames, sf_error(sndfile));
        perror(error);
        free(info);
        free(frames);
      }
    }
    if (sample == NULL) {
      printf("failed.\n");
    }
    else {
      fix_samplerate(sample);
    }
  }

  return(sample);
}
