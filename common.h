#ifndef __COMMON_H__
#define __COMMON_H__

extern int g_num_channels;
extern float g_gain;
extern int g_samplerate;

typedef enum {
  compressor_none = 0,
  compressor_dirty,
  compressors
} compressor_t;
extern const char *compressor_names[compressors];

#endif // __COMMON_H__
