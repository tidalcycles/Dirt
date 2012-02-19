#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <lo/lo.h>

#include "server.h"
#include "audio.h"

void error(int num, const char *m, const char *path);

int trigger_handler(const char *path, const char *types, lo_arg **argv,
                    int argc, void *data, void *user_data);

int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);

/**/

void error(int num, const char *msg, const char *path) {
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
}

/**/

int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data) {
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

int play_handler(const char *path, const char *types, lo_arg **argv,
                    int argc, void *data, void *user_data) {

  /* lo_timetag ts = lo_message_get_timestamp(data); */
  int i = 0;

  double when = (double) argv[0]->d + ((double) argv[1]->d / 1000000.0);

  char *sample_name = strdup((char *) argv[2]);

  float offset = argv[3]->f;
  float duration = argv[4]->f;
  float speed  = argv[5]->f;

  audio_play(when,
             sample_name,
             offset,
             duration,
             speed
             );
  return 0;
}

/**/

extern int server_init(void) {
  lo_server_thread st = lo_server_thread_new("7771", error);

  //lo_server_thread_add_method(st, NULL, NULL, generic_handler, NULL);

  lo_server_thread_add_method(st, "/play", "ddsfff",
                              play_handler, 
                              NULL
                             );
  lo_server_thread_start(st);
  
  return(1);
}
