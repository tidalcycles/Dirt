#ifndef _DIRTCONFIGH_
#define _DIRTCONFIGH_

//#define FEEDBACK
//#define INPUT
#define DEFAULT_OSC_PORT "7771"

#define DEFAULT_CHANNELS 2
#define MIN_CHANNELS 1
#define MAX_CHANNELS 16

#define DEFAULT_GAIN_DB (-18.0f)
#define DEFAULT_GAIN (2.01428065887067f)
#define MIN_GAIN_DB (-40.0f)
#define MAX_GAIN_DB (40.0f)

#define DEFAULT_SAMPLERATE 44100
#define MIN_SAMPLERATE 1024
#define MAX_SAMPLERATE 128000

#define DEFAULT_WORKERS 2

// Brings it into being roughly equivalent to superdirt
#define CUTOFFRATIO 30000.0f

// #define SEND_RMS

#define MAX_ORBIT 15

#ifdef SEND_RMS
// 300ms assuming 44100
#define RMS_SZ 13230
#endif

#endif
