#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <math.h>

#ifdef linux
#include <signal.h>
#endif

#include "common.h"
#include "audio.h"
#include "log.h"
#include "server.h"

#ifndef DEFAULT_OUTPUT
#error DEFAULT_OUTPUT is not defined
#endif

static const char *output = DEFAULT_OUTPUT;
static int dirty_compressor_flag = 1;
static int jack_auto_connect_flag = 1;
static int late_trigger_flag = 1;
static int shape_gain_comp_flag = 0;
static int preload_flag = 0;

#ifdef linux
void sigint_handler(int sig) {
  log_printf(LOG_OUT, "\nCTRL-C detected\n");
  // explicitly call exit on signit so things registered via atexit() fire
  exit(-1);
}
#endif

int main (int argc, char **argv) {
  /* Use getopt to parse command-line arguments */
  /* see http://www.gnu.org/savannah-checkouts/gnu/libc/manual/html_node/Getopt.html */
  int c;
  int num_channels;
  int samplerate;
  float gain = 20.0 * log10(g_gain/16.0);
  char *osc_port = DEFAULT_OSC_PORT;
  char *sampleroot = "./samples";
  char *version = "1.1.0~prerelease";

  unsigned int num_workers = DEFAULT_WORKERS;

#ifdef linux
  signal(SIGINT, sigint_handler);
#endif
  
  while (1)
  {
    static struct option long_options[] =
    {
      /* Use flags like so:
      {"verbose",  no_argument,  &verbose_flag, 'V'}*/
      /* Argument styles: no_argument, required_argument, optional_argument */
      {"port",                  required_argument, 0, 'p'},
      {"output",                required_argument, 0, 'o'},
      {"channels",              required_argument, 0, 'c'},
      {"samplerate",            required_argument, 0, 'r'},
      {"dirty-compressor",      no_argument, &dirty_compressor_flag, 1},
      {"no-dirty-compressor",   no_argument, &dirty_compressor_flag, 0},
      {"shape-gain-compensation",      no_argument, &shape_gain_comp_flag, 1},
      {"no-shape-gain-compensation",   no_argument, &shape_gain_comp_flag, 0},
#ifdef JACK
      {"jack-auto-connect",     no_argument, &jack_auto_connect_flag, 1},
      {"no-jack-auto-connect",  no_argument, &jack_auto_connect_flag, 0},
#endif
      {"late-trigger",          no_argument, &late_trigger_flag, 1},
      {"no-late-trigger",       no_argument, &late_trigger_flag, 0},
      {"samples-root-path",     required_argument, 0, 's'},
      {"workers",               required_argument, 0, 'w'},

      {"gain",                  required_argument, 0, 'g'},

      {"preload",               no_argument, &preload_flag, 1},
      {"no-preload",            no_argument, &preload_flag, 0},

      {"version", no_argument, 0, 'v'},
      {"help",    no_argument, 0, 'h'},

      {0, 0, 0, 0}
    };

    int option_index = 0;

    /* Argument parameters:
      no_argument: " "
      required_argument: ":"
      optional_argument: "::" */

    c = getopt_long(argc, argv, "p:o:c:r:s:w:g:vh",
                    long_options, &option_index);

    if (c == -1)
      break;

    switch (c)
    {
      case 0:
        /* If this option set a flag, do nothing else now. */
        if (long_options[option_index].flag != 0) break;

      case 'v':
        log_printf(LOG_OUT, "%s\n", version);
        return 1;
      case 'h':
        log_printf(LOG_OUT, "Usage: dirt [OPTION]...\n"
               "\n"
               "Dirt - a software sampler, mainly used with Tidal: http://yaxu.org/tidal/\n"
               "Released as free software under the terms of the GNU Public License version 3.0 and later.\n"
               "\n"
               "Arguments:\n"
	             "  -p, --port                       OSC port to listen to (default: %s)\n"
               "  -o, --output                     audio output (default: %s)\n"
#ifdef JACK
               "                  jack             JACK Audio Connection Kit\n"
#endif
#ifdef PULSE
               "                  pulse            PulseAudio\n"
#endif
#ifdef PORTAUDIO
               "                  portaudio        PortAudio\n"
#endif
#ifdef SDL2
               "                  sdl2             SDL2\n"
#endif
               "  -c, --channels                   number of output channels (default: %u)\n"
               "  -r, --samplerate                 samplerate (default: %u)\n"
#ifdef JACK
               "                                   (-o jack uses engine rate)\n"
#endif
               "      --dirty-compressor           enable dirty compressor on audio output (default)\n"
               "      --no-dirty-compressor        disable dirty compressor on audio output\n"
               "      --shape-gain-compensation    enable distortion gain compensation\n"
               "      --no-shape-gain-compensation disable distortion gain compensation (default)\n"
               "  -g, --gain                       gain adjustment (default %f db)\n"
#ifdef JACK
               "      --jack-auto-connect          automatically connect to writable clients (default)\n"
               "      --no-jack-auto-connect       do not connect to writable clients  \n"
#endif
               "      --late-trigger               enable sample retrigger after loading (default)\n"
               "      --no-late-trigger            disable sample retrigger after loading\n"
               "      --preload                    enable sample preloading at startup\n"
               "      --no-preload                 disable sample preloading at startup (default)\n"
	             "  -s  --samples-root-path          set a samples root directory path\n"
               "  -w, --workers                    number of sample-reading workers (default: %u)\n"
               "  -h, --help                       display this help and exit\n"
               "  -v, --version                    output version information and exit\n",
               DEFAULT_OSC_PORT, DEFAULT_OUTPUT, DEFAULT_CHANNELS,
	       DEFAULT_SAMPLERATE,
               20.0*log10(DEFAULT_GAIN/16.0),
               DEFAULT_WORKERS);
        return 1;

      case 'p':
        osc_port = optarg;
	break;
      case 'o':
        output = DEFAULT_OUTPUT;
#ifdef JACK
        if (0 == strcmp("jack", optarg)) output = optarg;
#endif
#ifdef PULSE
        if (0 == strcmp("pulse", optarg)) output = optarg;
#endif
#ifdef PORTAUDIO
        if (0 == strcmp("portaudio", optarg)) output = optarg;
#endif
#ifdef SDL2
        if (0 == strcmp("sdl2", optarg)) output = optarg;
#endif
        if (0 != strcmp(output, optarg)) {
          log_printf(LOG_ERR, "invalid output: %s. resetting to default: %s\n", optarg, output);
        }
        break;
      case 'c':
        num_channels = atoi(optarg);
        if (num_channels < MIN_CHANNELS || num_channels > MAX_CHANNELS) {
          log_printf(LOG_ERR, "invalid number of channels: %u (min: %u, max: %u). resetting to default\n", num_channels, MIN_CHANNELS, MAX_CHANNELS);
          num_channels = DEFAULT_CHANNELS;
        }
        g_num_channels = num_channels;
        break;
      case 'r':
        samplerate = atoi(optarg);
        if (samplerate < MIN_SAMPLERATE || samplerate > MAX_SAMPLERATE) {
          log_printf(LOG_ERR, "invalid number of channels: %u (min: %u, max: %u). resetting to default\n", samplerate, MIN_SAMPLERATE, MAX_SAMPLERATE);
	  samplerate = DEFAULT_SAMPLERATE;
        }
	g_samplerate = samplerate;
        break;
      case 's':
	sampleroot = optarg;
	break;
      case 'w':
        num_workers = atoi(optarg);
        if (num_workers < 1) {
          log_printf(LOG_ERR, "invalid number of workers: %u. resetting to default\n", num_workers);
          num_workers = DEFAULT_WORKERS;
        }
        break;
      
      case 'g':
        gain = atof(optarg);
        gain = (gain > 40)? 40 : gain;
        gain = (gain < -40)?(-40): gain;
        g_gain = 16.0 * pow(10.0, gain/20.0);
        break;

      case '?':
        /* getopt_long will have already printed an error */
        break;

      default:
        return 1;
    }
  }

  log_printf(LOG_ERR, "port: %s\n", osc_port);
  log_printf(LOG_ERR, "output: %s\n", output);
  log_printf(LOG_ERR, "channels: %u\n", g_num_channels);
  log_printf(LOG_ERR, "samplerate: %u\n", g_samplerate);
  log_printf(LOG_ERR, "gain (dB): %f\n", gain);
  log_printf(LOG_ERR, "gain factor: %f\n", g_gain);

  if (!dirty_compressor_flag) {
    log_printf(LOG_ERR, "dirty compressor disabled\n");
  }
  if (shape_gain_comp_flag) {
    log_printf(LOG_ERR, "distortion gain compensation enabled\n");
  }

#ifdef JACK
  if (!jack_auto_connect_flag) {
    log_printf(LOG_ERR, "port auto-connection disabled\n");
  }
#endif

  if (!late_trigger_flag) {
    log_printf(LOG_ERR, "late trigger disabled\n");
  }

    if (preload_flag) {
    log_printf(LOG_ERR, "sample preloading enabled\n");
  }

  log_printf(LOG_ERR, "workers: %u\n", num_workers);

  log_printf(LOG_ERR, "init audio\n");
  audio_init(output, dirty_compressor_flag, jack_auto_connect_flag, late_trigger_flag, num_workers, sampleroot, shape_gain_comp_flag, preload_flag);

  log_printf(LOG_ERR, "init open sound control\n");
  server_init(osc_port);

  sleep(-1);
  return(0);
}
