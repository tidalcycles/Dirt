#include <unistd.h>

#include "config.h"

extern "C"
{
#include "common.h"
extern void audio_init(const char *output, bool dirty_compressor, bool autoconnect, bool late_trigger, unsigned int num_workers, const char *sampleroot, bool shape_gain_comp, bool preload_flag);
extern int server_init(const char *osc_port);
};

int main(int argc, char **argv)
{
  bool dirty_compressor_flag = true;
  bool jack_auto_connect_flag = true;
  bool late_trigger_flag = true;
  int num_workers = 2;
  const char *sampleroot = "./samples/";
  bool shape_gain_comp_flag = false;
  bool preload_flag = false;
  const char *osc_port = DEFAULT_OSC_PORT;
  g_num_channels = DEFAULT_CHANNELS;
  g_samplerate = DEFAULT_SAMPLERATE;
  g_gain = DEFAULT_GAIN;
  audio_init
    ( "sdl2"
    , dirty_compressor_flag
    , jack_auto_connect_flag
    , late_trigger_flag
    , num_workers
    , sampleroot
    , shape_gain_comp_flag
    , preload_flag
    );
  server_init(osc_port);
  sleep(-1);
  return 0;
}
