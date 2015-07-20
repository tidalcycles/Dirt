#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>

#include "jack.h"
#include "config.h"

jack_client_t *client;
jack_port_t **output_ports;

#ifdef INPUT
jack_port_t *input_port;
#endif

int process(jack_nframes_t nframes, void *arg) {
  jack_default_audio_sample_t *out[g_num_channels+1];
#ifdef INPUT
  jack_default_audio_sample_t *in;
#endif
  t_callback callback = (t_callback) arg;
  int i;

  for (i = 0; i < g_num_channels; ++i) {
    out[i] = jack_port_get_buffer(output_ports[i], nframes);
  }

#ifdef INPUT
  in = jack_port_get_buffer(input_port, nframes);
  callback(nframes, in, out);
#else
  callback(nframes, NULL, out);
#endif


  return 0;      

}

void jack_shutdown(void *arg) {
  if (output_ports) free(output_ports);
  exit(1);
}

extern jack_client_t *jack_start(t_callback callback, bool autoconnect) {
  const char *client_name = "dirt";
  const char *server_name = NULL;
  jack_options_t options = JackNullOption;
  jack_status_t status;
  int i;
  char portname[16];

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
  
  jack_set_process_callback(client, process, (void *) callback);
  
  jack_on_shutdown(client, jack_shutdown, 0);

  printf("engine sample rate: %" PRIu32 "\n",
          jack_get_sample_rate(client));

#ifdef INPUT
  strcpy(portname, "input");
  input_port = jack_port_register(client, portname,
                                  JACK_DEFAULT_AUDIO_TYPE,
                                  JackPortIsInput, 0);

  if (input_port == NULL) {
    fprintf(stderr, "no JACK input ports available\n");
    exit(1);
  }
#endif

  output_ports = malloc((g_num_channels + 1) * sizeof(jack_port_t*));
  if (!output_ports) {
    fprintf(stderr, "no memory to allocate `output_ports'\n");
    exit(1);
  }

  for (i = 0; i < g_num_channels; ++i) {
    sprintf(portname, "output_%d", i);
    output_ports[i] = jack_port_register(client, portname,
                                         JACK_DEFAULT_AUDIO_TYPE,
                                         JackPortIsOutput, 0);
    if (output_ports[i] == NULL) {
      fprintf(stderr, "no more JACK ports available\n");
      if (output_ports) free(output_ports);
      exit(1);
    }
  }
  
  output_ports[g_num_channels] = NULL;
  
  if (jack_activate(client)) {
    fprintf(stderr, "cannot activate client");
    if (output_ports) free(output_ports);
    exit(1);
  }

  if (autoconnect) {
    const char **ports;
    ports = jack_get_ports(client, NULL, NULL,
                           JackPortIsPhysical|JackPortIsInput);
    if (!ports) {
      fprintf(stderr, "cannot find any physical capture ports\n");
    } else {
      for (i = 0; i < g_num_channels; ++i) {
        if (ports[i] == NULL) {
          break;
        }
        //sprintf(portname, "output_%d", i);
        if (jack_connect(client, jack_port_name(output_ports[i]), ports[i])) {
          fprintf(stderr, "cannot connect output ports\n");
        }
      }
      free(ports);
    }

#ifdef INPUT
    ports = jack_get_ports(client, NULL, NULL,
                           JackPortIsPhysical|JackPortIsOutput);
    //strcpy(portname, "input");
    if (!ports) {
      fprintf(stderr, "cannot find any physical capture ports\n");
    } else {
      if (jack_connect(client, ports[0], jack_port_name(input_port))) {
        fprintf(stderr, "cannot connect input port\n");
      }
      free(ports);
    }
#endif
  }

  return(client);
}

