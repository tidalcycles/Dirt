#ifndef _DIRTJACKH_
#define _DIRTJACKH_
#include <jack/jack.h>

#ifndef CHANNELS
#define CHANNELS 2
#endif

typedef int (*t_callback)(int, float *, float **);

extern jack_client_t *jack_start(t_callback callback);
#endif
