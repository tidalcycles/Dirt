#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

#include "common.h"
#include "audio.h"
#include "server.h"

static int dirty_compressor_flag = 1;

int main (int argc, char **argv) {
  /* Use getopt to parse command-line arguments */
  /* see http://www.gnu.org/savannah-checkouts/gnu/libc/manual/html_node/Getopt.html */
  int c;
  while (1)
  {
    static struct option long_options[] =
    {
      /* Use flags like so:
      {"verbose",  no_argument,  &verbose_flag, 'V'}*/
      /* Argument styles: no_argument, required_argument, optional_argument */

      {"dirty-compressor",      no_argument, &dirty_compressor_flag, 1},
      {"no-dirty-compressor",   no_argument, &dirty_compressor_flag, 0},

      {"version", no_argument, 0, 'v'},
      {"help",    no_argument, 0, 'h'},

      {0, 0, 0, 0}
    };

    int option_index = 0;

    /* Argument parameters:
      no_argument: " "
      required_argument: ":"
      optional_argument: "::" */

    c = getopt_long(argc, argv, "vh",
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
               "      --dirty-compressor          enable dirty compressor on audio output (default)\n"
               "      --no-dirty-compressor       disable dirty compressor on audio output\n"
               "  -h, --help                      display this help and exit\n"
               "  -v, --version                   output version information and exit\n",
               OSC_PORT);
        return 1;

      case '?':
        /* getopt_long will have already printed an error */
        break;

      default:
        return 1;
    }
  }

  if (!dirty_compressor_flag) {
    fprintf(stderr, "dirty compressor disabled\n");
  }

  fprintf(stderr, "init audio\n");
  audio_init(dirty_compressor_flag);

  fprintf(stderr, "init open sound control\n");
  server_init();

  sleep(-1);
  return(0);
}
