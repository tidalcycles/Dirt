#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <lo/lo.h>
#include <sys/types.h>
#include <math.h>
#include <sys/time.h>
#include <assert.h>

#include "server.h"
#include "audio.h"
#include "config.h"

#ifdef ZEROMQ
#include <zmq.h>
#define MAXOSCSZ 1024
#endif

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

int kriole_handler(const char *path, const char *types, lo_arg **argv,
                   int argc, void *data, void *user_data) {

  double when = (double) argv[0]->i + ((double) argv[1]->i / 1000000.0);
  float duration = argv[2]->f;
  float pitch_start = argv[3]->f;
  float pitch_stop = argv[4]->f;
  
  audio_kriole(when, duration, pitch_start, pitch_stop);

  return(0);
}

/**/

#ifdef FEEDBACK
int preload_handler(const char *path, const char *types, lo_arg **argv,
                   int argc, void *data, void *user_data) {

  preload_kriol((char *) argv[0]);
  return(0);
}

int pause_input_handler(const char *path, const char *types, lo_arg **argv,
			int argc, void *data, void *user_data) {
  audio_pause_input(argv[0]->i);
  return(0);
}
#endif

/**/

int play_handler(const char *path, const char *types, lo_arg **argv,
                 int argc, void *data, void *user_data) {

  /* lo_timetag ts = lo_message_get_timestamp(data); */

  double when = (double) argv[0]->i + ((double) argv[1]->i / 1000000.0);
#ifdef SUBLATENCY
  when -= SUBLATENCY;
#endif
  int poffset = 2;

  float cps = argv[2]->f;
  poffset = 3;
  //printf("timing info: when, cps = %f\t%f\n", when, cps);

  char *sample_name = (char *) argv[0+poffset];

  float offset = argv[1+poffset]->f;
  float start = argv[2+poffset]->f;
  float end  = argv[3+poffset]->f;
  float speed  = argv[4+poffset]->f;
  float pan  = argv[5+poffset]->f;
  float velocity  = argv[6+poffset]->f;
  char *vowel_s = (char *) argv[7+poffset];
  float cutoff = argv[8+poffset]->f;
  float resonance = argv[9+poffset]->f;
  float accelerate = argv[10+poffset]->f;
  float shape = argv[11+poffset]->f;
  int kriole_chunk = argv[12+poffset]->i;
  
  float gain = argc > (13+poffset) ? argv[13+poffset]->f : 0;
  int cutgroup = argc > (14+poffset) ? argv[14+poffset]->i : 0;

  float delay = argc > (15+poffset) ? argv[15+poffset]->f : 0;
  float delaytime = argc > (16+poffset) ? argv[16+poffset]->f : 0;
  float delayfeedback = argc > (17+poffset) ? argv[17+poffset]->f : 0;
  
  float crush = argc > (18+poffset) ? argv[18+poffset]->f : 0;
  int coarse = argc > (19+poffset) ? argv[19+poffset]->i : 0;
  float hcutoff = argc > (20+poffset) ? argv[20+poffset]->f : 0;
  float hresonance = argc > (21+poffset) ? argv[21+poffset]->f : 0;
  float bandf = argc > (22+poffset) ? argv[22+poffset]->f : 0;
  float bandq = argc > (23+poffset) ? argv[23+poffset]->f : 0;

  char *unit_name = argc > (24+poffset) ? (char *) argv[24+poffset] : "r";

  if (argc > 25+poffset) {
    printf("play server unexpectedly received extra parameters, maybe update Dirt?\n");
  }

  if (speed == 0) {
    return(0);
  }

  int vowelnum = -1;

  switch(vowel_s[0]) {
  case 'a': case 'A': vowelnum = 0; break;
  case 'e': case 'E': vowelnum = 1; break;
  case 'i': case 'I': vowelnum = 2; break;
  case 'o': case 'O': vowelnum = 3; break;
  case 'u': case 'U': vowelnum = 4; break;
  }
  //printf("vowel: %s num: %d\n", vowel_s, vowelnum);

  int unit = -1;
  switch(unit_name[0]) {
     // rate
     case 'r': case 'R': unit = 'r'; break;
     // sec
     case 's': case 'S': unit = 's'; break;
     // cycle
     case 'c': case 'C': unit = 'c'; break;
  }

  t_play_args args = {
    when,
    cps,
    sample_name,
    offset,
    start,
    end,
    speed,
    pan,
    velocity,
    vowelnum,
    cutoff,
    resonance,
    accelerate,
    shape,
    kriole_chunk,
    gain,
    cutgroup,
    delay,
    delaytime,
    delayfeedback,
    crush,
    coarse,
    hcutoff,
    hresonance,
    bandf,
    bandq,
    unit
  };
  audio_play(&args);

  return(0);
}

/**/

#ifdef ZEROMQ
void *zmqthread(void *data){
  void *context = zmq_ctx_new ();
  void *subscriber = zmq_socket (context, ZMQ_SUB);
  void *buffer = (void *) malloc(MAXOSCSZ);

  int rc = zmq_connect (subscriber, ZEROMQ);
  lo_server s = lo_server_new("7772", error);

  lo_server_add_method(s, "/play", "iisffffffsffffififfffiffff",
		       play_handler, 
		       NULL
		       );

  lo_server_add_method(s, "/play", "iisffffffsffffififfffifff",
		       play_handler, 
		       NULL
		       );

  lo_server_add_method(s, "/play", "iisffffffsffffififff",
		       play_handler, 
		       NULL
		       );

  lo_server_add_method(s, "/kriole", "iifff",
		       kriole_handler, 
		       NULL
		       );

  lo_server_add_method(s, NULL, NULL, generic_handler, NULL);

  assert(rc == 0);
  //  Subscribe to all
  rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE,
		      NULL, 0);
  assert (rc == 0);
  while(1) {
    int size = zmq_recv(subscriber, buffer, MAXOSCSZ, 0);
    if (size > 0) {
      lo_server_dispatch_data(s, buffer, size);
    }
    else {
      printf("oops.\n");
    }
  }
  return(NULL);
}
#endif

/**/

extern int server_init(void) {

  lo_server_thread st = lo_server_thread_new(OSC_PORT, error);

  lo_server_thread_add_method(st, "/play", NULL, play_handler, NULL);

  lo_server_thread_add_method(st, "/kriole", "iifff",
                              kriole_handler, 
                              NULL
                             );

#ifdef FEEDBACK
  lo_server_thread_add_method(st, "/preload", "s",
                              preload_handler, 
                              NULL
                             );
  lo_server_thread_add_method(st, "/pause_input", "i",
                              pause_input_handler, 
                              NULL
                             );
#endif

  lo_server_thread_add_method(st, NULL, NULL, generic_handler, NULL);
  lo_server_thread_start(st);

  
#ifdef ZEROMQ
  pthread_t t;
  pthread_create(&t, NULL, (void *(*)(void *)) zmqthread, NULL);
#endif
  
  return(1);
}

extern void osc_send_pitch(float starttime, unsigned int chunk, 
			   float pitch, float flux, float centroid) {
  static lo_address t = NULL;
  static int pid = 0;
  if (t == NULL) {
    t = lo_address_new(NULL, "6010");
  }
  if (pid == 0) {
    pid = (int) getpid();
  }
  //printf("send [%d] %f\n", chunk, pitch);
  // pid, starttime, chunk, v_pitch, v_flux, v_centroid
  lo_send(t, "/chunk", "ififff", 
          pid,
          starttime,
          (int) chunk,
          pitch,
          flux,
	  centroid
          );
  
}

extern void osc_send_play(double when, int lowchunk, float pitch, float flux, float centroid) {
  static lo_address t = NULL;
  static int pid = 0;
  if (t == NULL) {
    t = lo_address_new(NULL, "6010");
  }
  if (pid == 0) {
    pid = (int) getpid();
  }
  printf("play [%d] %f\n", lowchunk, pitch);
  // pid, starttime, chunk, v_pitch, v_flux, v_centroid
  lo_send(t, "/play", "iiiifff", 
          pid,
          (int) when,
          (int) ((when - floor(when)) * 1000000.0),
          lowchunk,
          pitch,
          flux,
	  centroid
          );
  
}

