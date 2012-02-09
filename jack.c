#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>

#include "jack.h"

jack_port_t *output_port;
jack_client_t *client;

int process(jack_nframes_t nframes, void *arg) {
  jack_default_audio_sample_t *out;
  t_callback callback = (t_callback) arg;

  out = jack_port_get_buffer(output_port, nframes);
  callback(nframes, out);

  return 0;      
}

void jack_shutdown(void *arg) {
  exit(1);
}

extern int jack_start(t_callback callback) {
  const char **ports;
  const char *client_name = "dirt";
  const char *server_name = NULL;
  jack_options_t options = JackNullOption;
  jack_status_t status;
  
  /* open a client connection to the JACK server */
  
  client = jack_client_open(client_name, options, &status, server_name);
  if (client == NULL) {
    fprintf(stderr, "jack_client_open() failed, "
             "status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      fprintf(stderr, "Unable to connect to JACK server\n");
    }
    exit(1);
  }
  if (status & JackServerStarted) {
    fprintf(stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(client);
    fprintf(stderr, "unique name `%s' assigned\n", client_name);
  }
  
  jack_set_process_callback(client, process, callback);
  
  jack_on_shutdown(client, jack_shutdown, 0);

  printf("engine sample rate: %" PRIu32 "\n",
          jack_get_sample_rate(client));
  
  output_port = jack_port_register(client, "output",
                                   JACK_DEFAULT_AUDIO_TYPE,
                                   JackPortIsOutput, 0);
  
  if (output_port == NULL) {
    fprintf(stderr, "no more JACK ports available\n");
    exit(1);
  }
  
  if (jack_activate(client)) {
    fprintf(stderr, "cannot activate client");
    exit(1);
  }

  ports = jack_get_ports(client, NULL, NULL,
                         JackPortIsPhysical|JackPortIsInput);
  if (ports == NULL) {
    fprintf(stderr, "no physical playback ports\n");
    exit(1);
  }
  
  if (jack_connect(client, jack_port_name(output_port), ports[0])) {
    fprintf(stderr, "cannot connect output ports\n");
  }
  
  free(ports);

  return(jack_get_sample_rate(client));
}

