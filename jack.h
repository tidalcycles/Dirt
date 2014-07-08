#ifndef _DIRTJACKH_
#define _DIRTJACKH_
#include <jack/jack.h>
#include "common.h"

typedef int (*t_callback)(int, float *, float **);

extern jack_client_t *jack_start(t_callback callback, bool autoconnect);
#endif
