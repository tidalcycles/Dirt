#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

#include "common.h"
#include "audio.h"
#include "server.h"

static int dirty_compressor_flag = 1;
#ifdef JACK
static int jack_auto_connect_flag = 1;
#endif
static int late_trigger_flag = 1;


int main (int argc, char **argv) {
  /* Use getopt to parse command-line arguments */
  /* see http://www.gnu.org/savannah-checkouts/gnu/libc/manual/html_node/Getopt.html */
  int c;
  int num_channels;

  unsigned int num_workers = DEFAULT_WORKERS;

  while (1)
  {
    static struct option long_options[] =
    {
      /* Use flags like so:
      {"verbose",  no_argument,  &verbose_flag, 'V'}*/
      /* Argument styles: no_argument, required_argument, optional_argument */
      {"channels",              required_argument, 0, 'c'},
      {"dirty-compressor",      no_argument, &dirty_compressor_flag, 1},
      {"no-dirty-compressor",   no_argument, &dirty_compressor_flag, 0},
#ifdef JACK
      {"jack-auto-connect",     no_argument, &jack_auto_connect_flag, 1},
      {"no-jack-auto-connect",  no_argument, &jack_auto_connect_flag, 0},
#endif
      {"late-trigger",          no_argument, &late_trigger_flag, 1},
      {"no-late-trigger",       no_argument, &late_trigger_flag, 0},
      {"workers",               required_argument, 0, 'w'},

      {"version", no_argument, 0, 'v'},
      {"help",    no_argument, 0, 'h'},

      {0, 0, 0, 0}
    };

    int option_index = 0;

    /* Argument parameters:
      no_argument: " "
      required_argument: ":"
      optional_argument: "::" */

    c = getopt_long(argc, argv, "c:w:vh",
                    long_options, &option_index);

    if (c == -1)
      break;

    switch (c)
    {
      case 0:
        /* If this option set a flag, do nothing else now. */
        if (long_options[option_index].flag != 0) break;

      case 'v':
      case 'h':
        printf("Usage: dirt [OPTION]...\n"
               "\n"
               "Dirt - a software sampler, mainly used with Tidal: http://yaxu.org/tidal/\n"
               "Released as free software under the terms of the GNU Public License version 3.0 and later.\n"
               "\n"
               "Listens to OSC messages on port %s.\n"
               "\n"
               "Arguments:\n"
               "  -c, --channels                  number of output channels (default: %u)\n"
               "      --dirty-compressor          enable dirty compressor on audio output (default)\n"
               "      --no-dirty-compressor       disable dirty compressor on audio output\n"
#ifdef JACK
               "      --jack-auto-connect         automatically connect to writable clients (default)\n"
               "      --no-jack-auto-connect      do not connect to writable clients  \n"
#endif
               "      --late-trigger              enable sample retrigger after loading (default)\n"
               "      --no-late-trigger           disable sample retrigger after loading\n"
               "  -w, --workers                   number of sample-reading workers (default: %u)\n"
               "  -h, --help                      display this help and exit\n"
               "  -v, --version                   output version information and exit\n",
               OSC_PORT, DEFAULT_CHANNELS, DEFAULT_WORKERS);
        return 1;

      case 'c':
        num_channels = atoi(optarg);
        if (num_channels < MIN_CHANNELS || num_channels > MAX_CHANNELS) {
          fprintf(stderr, "invalid number of channels: %u (min: %u, max: %u). resetting to default\n", num_channels, MIN_CHANNELS, MAX_CHANNELS);
          num_channels = DEFAULT_CHANNELS;
        }
        g_num_channels = num_channels;
        break;

      case 'w':
        num_workers = atoi(optarg);
        if (num_workers < 1) {
          fprintf(stderr, "invalid number of workers: %u. resetting to default\n", num_workers);
          num_workers = DEFAULT_WORKERS;
        }
        break;

      case '?':
        /* getopt_long will have already printed an error */
        break;

      default:
        return 1;
    }
  }

  fprintf(stderr, "channels: %u\n", g_num_channels);

  if (!dirty_compressor_flag) {
    fprintf(stderr, "dirty compressor disabled\n");
  }

#ifdef JACK
  if (!jack_auto_connect_flag) {
    fprintf(stderr, "port auto-connection disabled\n");
  }
#endif

  if (!late_trigger_flag) {
    fprintf(stderr, "late trigger disabled\n");
  }

  fprintf(stderr, "workers: %u\n", num_workers);

  fprintf(stderr, "init audio\n");
#ifdef JACK
  audio_init(dirty_compressor_flag, jack_auto_connect_flag, late_trigger_flag, num_workers);
#else
  audio_init(dirty_compressor_flag, true, late_trigger_flag, num_workers);
#endif

  fprintf(stderr, "init open sound control\n");
  server_init();

  sleep(-1);
  return(0);
}
