#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <lo/lo.h>

#include "server.h"

void error(int num, const char *m, const char *path);

int trigger_handler(const char *path, const char *types, lo_arg **argv,
                    int argc, void *data, void *user_data);

int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);

/**/

extern int server_init(void) {
  lo_server_thread st = lo_server_thread_new("7771", error);

#ifdef DEBUG  
  lo_server_thread_add_method(st, NULL, NULL, generic_handler, NULL);
#endif

  lo_server_thread_add_method(st, "/trigger", "sfffffsfffssffffff",
                              trigger_handler, 
                              NULL
                             );
  lo_server_thread_start(st);
  
  return(1);
}

/**/

void error(int num, const char *msg, const char *path) {
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
}

/**/

int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data)
{
    int i;
    
    printf("path: <%s>\n", path);
    for (i=0; i<argc; i++) {
      printf("arg %d '%c' ", i, types[i]);
      lo_arg_pp(types[i], argv[i]);
      printf("\n");
    }
    printf("\n");

    return 1;
}

/**/

int trigger_handler(const char *path, const char *types, lo_arg **argv,
                    int argc, void *data, void *user_data) {

  lo_timetag ts = lo_message_get_timestamp(data);

  char *sample_name = strdup((char *) argv[0]);
  float speed  = argv[1]->f;
  float shape  = argv[2]->f;
  float pan    = argv[3]->f;
  float pan_to = argv[4]->f;
  float volume = argv[5]->f;
  char *envelope_name = strdup((char *) argv[6]);
  float anafeel_strength  = argv[7]->f;
  float anafeel_frequency = argv[8]->f;
  float accellerate = argv[9]->f;
  char *vowel_s = (char *) argv[10];
  char *scale_name = strdup((char *) argv[11]);
  float loops      = argv[12]->f;
  float duration   = argv[13]->f;
  float delay      = argv[14]->f;
  float delay2     = argv[15]->f;
  float cutoff     = argv[16]->f;
  float resonance  = argv[17]->f;

  int vowel = -1;
  switch(vowel_s[0]) {
  case 'a': case 'A': vowel = 0; break;
  case 'e': case 'E': vowel = 1; break;
  case 'i': case 'I': vowel = 2; break;
  case 'o': case 'O': vowel = 3; break;
  case 'u': case 'U': vowel = 4; break;
  }
    
  audio_trigger(ts,
                sample_name,
                speed,
                shape,
                pan,
                pan_to,
                volume,
                envelope_name,
                anafeel_strength,
                anafeel_frequency,
                accellerate,
                vowel,
                scale_name,
                loops,
                duration,
		delay,
		delay2,
		cutoff,
		resonance
               );
  return 0;
}

