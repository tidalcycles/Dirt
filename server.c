#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <lo/lo.h>
#include <sys/types.h>
#include <math.h>
#include <sys/time.h>
#include <assert.h>
#include <libgen.h>

#include "server.h"
#include "audio.h"
#include "config.h"
#include "log.h"

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
    log_printf(LOG_OUT, "liblo server error %d in path %s: %s\n", num, path, msg);
}

/**/

int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data) {
    int i;
    
    log_printf(LOG_OUT, "path: <%s>\n", path);
    for (i=0; i<argc; i++) {
      log_printf(LOG_OUT, "arg %d '%c' ", i, types[i]);
      lo_arg_pp(types[i], argv[i]);
      log_printf(LOG_OUT, "\n");
    }
    log_printf(LOG_OUT, "\n");

    return 1;
}

/**/

/*
F, S, I macros will be redefined several times below,
to avoid error prone manual duplication of all the field names.
order and types must match positional arguments of /play
(minus initial ii time stamp)
default values are provided for both /play and /dirt/play
(where later vs all fields are optional)
this table was sourced (with regex replacement) from:
<https://github.com/tidalcycles/Tidal/blob/02e5e3ac22abacaea5ed59472dbc34b7b975c4bb/src/Sound/Tidal/Stream/Target.hs#L171-L208>
default for s is "" instead of NULL, to make it optional like all the rest
*/

#define FIELDS \
F(cps, 0) \
S(s, "") \
F(offset, 0) \
F(begin, 0) \
F(end, 1) \
F(speed, 1) \
F(pan, 0.5) \
F(velocity, 0.5) \
S(vowel, "") \
F(cutoff, 0) \
F(resonance, 0) \
F(accelerate, 0) \
F(shape, 0) \
I(kriole, 0) \
F(gain, 1) \
I(cut, 0) \
F(delay, 0) \
F(delaytime, -1) \
F(delayfeedback, -1) \
F(crush, 0) \
I(coarse, 0) \
F(hcutoff, 0) \
F(hresonance, 0) \
F(bandf, 0) \
F(bandq, 0) \
S(unit, "rate") \
F(loop, 0) \
F(n, 0) \
F(attack, -1) \
F(hold, 0) \
F(release, -1) \
I(orbit, 0) \
// I(id, 0)

/**/

/* put fields into a sound and play it */

#define F(field, defvalue) , float field
#define I(field, defvalue) , int field
#define S(field, defvalue) , const char *field
int play_dispatch(double when FIELDS) {
#undef F
#undef I
#undef S

  if (speed == 0) {
    return(0);
  }

  int vowelnum = -1;
  switch(vowel[0]) {
  case 'a': case 'A': vowelnum = 0; break;
  case 'e': case 'E': vowelnum = 1; break;
  case 'i': case 'I': vowelnum = 2; break;
  case 'o': case 'O': vowelnum = 3; break;
  case 'u': case 'U': vowelnum = 4; break;
  }
  //log_printf(LOG_OUT, "vowel: %s num: %d\n", vowel_s, vowelnum);

  int unitnum = -1;
  switch(unit[0]) {
    // rate
  case 'r': case 'R': unitnum = 'r'; break;
    // sec
  case 's': case 'S': unitnum = 's'; break;
    // cycle
  case 'c': case 'C': unitnum = 'c'; break;
  }

  t_sound *sound = new_sound();
  if (sound == NULL) {
    //log_printf(LOG_OUT, "hit max sounds (%d)\n", MAXSOUNDS);
    return(0);
  }
  sound->active = 1;
  sound->speed = speed;
  sound->pan = pan;
  sound->start = begin;
  sound->end = end;
  sound->velocity = velocity;
  sound->formant_vowelnum = vowelnum;
  sound->cutoff = cutoff / CUTOFFRATIO * 44100.0f/g_samplerate;
  sound->resonance = resonance;
  sound->accelerate = accelerate;
  sound->shape = (shape != 0);
  shape = fabsf(shape);
  shape = (shape > 0.99f)?0.99f:shape;
  sound->shape_k = (2.0f * shape) / (1.0f - shape);
  sound->delay = delay;
  sound->delaytime = delaytime;
  sound->delayfeedback = delayfeedback;
  sound->gain = powf(gain/2, 4);
  sound->cutgroup = cut;
  sound->crush = crush;
  sound->coarse = coarse;
  sound->hcutoff = hcutoff / CUTOFFRATIO * 44100.0f/g_samplerate;
  sound->hresonance = hresonance;
  sound->bandf = bandf / CUTOFFRATIO * 44100.0f/g_samplerate;
  sound->bandq = bandq;
  sound->sample_loop = loop;
  sound->unit = unitnum;
  sound->offset = offset;
  sound->cps = cps;
  sound->when = when;
  sound->orbit = (orbit <= MAX_ORBIT) ? orbit : MAX_ORBIT;
  //log_printf(LOG_OUT, "orbit: %d\n", sound->orbit);
  n = fabsf(floorf(n));
  if (s[0]) {
    char base[MAXPATHSIZE];
    strncpy(base, s, sizeof(base) - 1);
    const char *basep = basename(base); // either GNU basename or POSIX basename is fine at this point
    if (0 == strcmp(basep, s)) {
      // name has no path component
      // add :n
      snprintf(sound->samplename, MAXPATHSIZE, "%s:%d", basep, (int) n);
    } else {
      // name has a path component (either relative or absolute)
      // allowability will be checked in file.c
      strncpy(sound->samplename, s, sizeof(sound->samplename) - 1);
    }
  } else {
    sound->samplename[0] = 0; // silence
  }
  sound->attack = attack;
  sound->hold = hold;
  sound->release = release;

  audio_play(sound);

  return(0);
}

/**/

/* handle positional arguments (/play message) */

int play_handler(const char *path, const char *types, lo_arg **argv,
                 int argc, void *data, void *user_data) {

  /* lo_timetag ts = lo_message_get_timestamp(data); */

  double when = (double) argv[0]->i + (((double) argv[1]->i) / 1000000.0);
#ifdef SUBLATENCY
  when -= SUBLATENCY;
#endif

  int arg = 2;

#define F(field, defvalue) float field = arg < argc ? argv[arg]->f : defvalue; ++arg;
#define I(field, defvalue) int   field = arg < argc ? argv[arg]->i : defvalue; ++arg;
#define S(field, defvalue) const char *field = arg < argc ? &argv[arg]->s : defvalue; ++arg;
  FIELDS
#undef F
#undef I
#undef S

  static bool extraWarned = false;
  if (arg < argc && ! extraWarned) {
    log_printf(LOG_OUT, "play server unexpectedly received extra parameters, maybe update Dirt?\n");
    extraWarned = true;
  }

#define F(field, defvalue) , field
#define I(field, defvalue) , field
#define S(field, defvalue) , field
  return play_dispatch(when FIELDS);
#undef F
#undef I
#undef S

}

/**/

/* handle key-value arguments (/dirt/play message) */

int dirt_play_handler(const char *path, const char *types,
        lo_arg **argv, int argc, lo_message msg, void *user_data) {

  lo_timetag ts = lo_message_get_timestamp(msg);
  const double epoch = 2208988800.0; // rfc868
  double when = ts.sec + ts.frac / (double) (((int64_t) 1) << 32) - epoch;
#ifdef SUBLATENCY
  when -= SUBLATENCY;
#endif

  if (argc & 1)
  {
    static int warned = 0;
    if (! warned)
    {
      warned = 1;
      log_printf(LOG_ERR, "received /dirt/play with odd argument count %d\n", argc);
    }
    return 0;
  }

#define F(field,defvalue) float field = defvalue; bool has_##field = false;
#define I(field,defvalue) int field = defvalue; bool has_##field = false;
#define S(field,defvalue) const char *field = defvalue; bool has_##field = false;
FIELDS
#undef F
#undef I
#undef S

  for (int arg = 0; arg < argc; arg += 2)
  {
    if (types[arg] != 's' && types[arg] != 'S') {
      log_printf(LOG_ERR, "expected type s or S for field name, got %c\n", types[arg]);
      return 0;
    }

#define N(field,defvalue,fromf,froms) \
    if (0 == strcmp(&argv[arg]->s, #field)) { \
      if (has_##field) { \
        static int warned =0 ; \
        if (! warned) { \
          warned = 1; \
          log_printf(LOG_ERR, "duplicate field %s\n", #field); \
        } \
      } \
      has_##field = true; \
      switch (types[arg + 1]) { \
        case 'i': field = argv[arg + 1]->i; break; \
        case 'f': field = fromf(argv[arg + 1]->f); break; \
        case 'h': field = argv[arg + 1]->h; break; \
        case 'd': field = fromf(argv[arg + 1]->d); break; \
        case 's': field = froms(&argv[arg + 1]->s); break; \
        case 'S': field = froms(&argv[arg + 1]->S); break; \
        case 'T': field = true; break; \
        case 'F': field = false; break; \
        default: { \
          static int warned = 0; \
          if (! warned) { \
            warned = 1; \
            log_printf(LOG_ERR, "can't handle type %c for field %s\n", types[arg + 1], #field); \
          } \
          return 0; \
        } \
      } \
    } else
#define F(field,defvalue) N(field,defvalue,(float),atof)
#define I(field,defvalue) N(field,defvalue,floor,atoi)

#define S(field, defvalue) \
    if (0 == strcmp(&argv[arg]->s, #field)) { \
      if (has_##field) { \
        static int warned =0 ; \
        if (! warned) { \
          warned = 1; \
          log_printf(LOG_ERR, "duplicate field %s\n", #field); \
        } \
      } \
      has_##field = true; \
      switch (types[arg + 1]) { \
        case 's': field = &argv[arg + 1]->s; break; \
        case 'S': field = &argv[arg + 1]->S; break; \
        default: { \
          static int warned = 0; \
          if (! warned) { \
            warned = 1; \
            log_printf(LOG_ERR, "can't handle type %c for field %s\n", types[arg + 1], #field); \
          } \
          return 0; \
        } \
      } \
    } else

FIELDS

    {
      // warn at most once about each unknown field
      static int warned_overflow = 0;
      static char warned_fields[1024] = {0};
      char field_with_spaces[256];
      snprintf(field_with_spaces, sizeof(field_with_spaces), " %s ", &argv[arg]->s);
      if (! strstr(warned_fields, field_with_spaces))
      {
        int overflow = ! (strlen(warned_fields) + strlen(field_with_spaces) + 1 < sizeof(warned_fields));
        if (! overflow)
        {
          strncat(warned_fields, field_with_spaces, sizeof(warned_fields) - 1);
          log_printf(LOG_ERR, "unknown field %s\n", &argv[arg]->s);
        }
        else
        {
          if (! warned_overflow)
          {
            warned_overflow = 1;
            log_printf(LOG_ERR, "too many unknown fields, not reporting any more\n");
          }
        }
      }
    }

#undef N
#undef F
#undef I
#undef S
  }

#define F(field, defvalue) , field
#define I(field, defvalue) , field
#define S(field, defvalue) , field
  return play_dispatch(when FIELDS);
#undef F
#undef I
#undef S
}

/**/

#ifdef ZEROMQ
void *zmqthread(void *data){
  void *context = zmq_ctx_new ();
  void *subscriber = zmq_socket (context, ZMQ_SUB);
  void *buffer = (void *) malloc(MAXOSCSZ);

  int rc = zmq_connect (subscriber, ZEROMQ);
  lo_server s = lo_server_new("7772", error);

  lo_server_add_method(s, "/play", "iisffffffsffffififfffiffffi",
		       play_handler, 
		       NULL
		       );

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
      log_printf(LOG_OUT, "oops.\n");
    }
  }
  return(NULL);
}
#endif

/**/

extern int server_init(const char *osc_port) {

  lo_server_thread st = lo_server_thread_new(osc_port, error);
  if (! st)
  {
    return 0;
  }
  // disable lo's bundle scheduler; we do our own scheduling
  lo_server_enable_queue(lo_server_thread_get_server(st), 0, 0);

  lo_server_thread_add_method(st, "/play", NULL, play_handler, NULL);
  lo_server_thread_add_method(st, "/dirt/play", NULL, dirt_play_handler, NULL);

  lo_server_thread_add_method(st, NULL, NULL, generic_handler, NULL);
  lo_server_thread_start(st);

  
#ifdef ZEROMQ
  pthread_t t;
  pthread_create(&t, NULL, (void *(*)(void *)) zmqthread, NULL);
#endif
  
  return(1);
}

extern void osc_send_pitch(double starttime, unsigned int chunk, 
			   float pitch, float flux, float centroid) {
  static lo_address t = NULL;
  static int pid = 0;
  if (t == NULL) {
    t = lo_address_new(NULL, "6010");
  }
  if (pid == 0) {
    pid = (int) getpid();
  }
  //log_printf(LOG_OUT, "send [%d] %f\n", chunk, pitch);
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
  log_printf(LOG_OUT, "play [%d] %f\n", lowchunk, pitch);
  // pid, starttime, chunk, v_pitch, v_flux, v_centroid
  lo_send(t, "/play", "iiiifff", 
          pid,
          (int) when,
          (int) ((when - floor(when)) * 1000000.0f),
          lowchunk,
          pitch,
          flux,
	  centroid
          );
  
}
