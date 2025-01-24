#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <jack/jack.h>

#include "jack.h"
#include "config.h"
#include "audio.h"
#include "common.h"
#include "log.h"

typedef int (*t_callback)(int, float *, float **);

jack_client_t *jack_start(t_callback callback, bool autoconnect);

jack_client_t *jack_client = NULL;
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

extern int jack_callback(int frames, float *input, float **outputs) {
    sampletime_t now;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    epochOffset = ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0))
      - ((double) jack_get_time() / 1000000.0);
    //log_printf(LOG_OUT, "jack time: %d tv_sec %d epochOffset: %f\n", jack_get_time(), tv.tv_sec, epochOffset);

  now = jack_last_frame_time(jack_client);

  for (int i=0; i < frames; ++i) {
    sampletime_t nowt = jack_frames_to_time(jack_client, now + i) / 1000000.0;
    playback(outputs, i, nowt);

    dequeue(nowt);
  }
  return(0);
}

void jack_init(bool autoconnect) {
  jack_client = jack_start(jack_callback, autoconnect);
  g_samplerate = jack_get_sample_rate(jack_client);
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
  char portname[24];

  /* open a client connection to the JACK server */
  
  client = jack_client_open(client_name, options, &status, server_name);
  if (client == NULL) {
    log_printf(LOG_ERR, "jack_client_open() failed, "
             "status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      log_printf(LOG_ERR, "Unable to connect to JACK server\n");
    }
    exit(1);
  }
  if (status & JackServerStarted) {
    log_printf(LOG_ERR, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(client);
    log_printf(LOG_ERR, "unique name `%s' assigned\n", client_name);
  }
  
  jack_set_process_callback(client, process, (void *) callback);
  
  jack_on_shutdown(client, jack_shutdown, 0);

  log_printf(LOG_OUT, "engine sample rate: %" PRIu32 "\n",
          jack_get_sample_rate(client));

#ifdef INPUT
  strcpy(portname, "input");
  input_port = jack_port_register(client, portname,
                                  JACK_DEFAULT_AUDIO_TYPE,
                                  JackPortIsInput, 0);

  if (input_port == NULL) {
    log_printf(LOG_ERR, "no JACK input ports available\n");
    exit(1);
  }
#endif

  output_ports = malloc((g_num_channels + 1) * sizeof(jack_port_t*));
  if (!output_ports) {
    log_printf(LOG_ERR, "no memory to allocate `output_ports'\n");
    exit(1);
  }

  for (i = 0; i < g_num_channels; ++i) {
    snprintf(portname, sizeof(portname), "output_%d", i);
    output_ports[i] = jack_port_register(client, portname,
                                         JACK_DEFAULT_AUDIO_TYPE,
                                         JackPortIsOutput, 0);
    if (output_ports[i] == NULL) {
      log_printf(LOG_ERR, "no more JACK ports available\n");
      if (output_ports) free(output_ports);
      exit(1);
    }
  }
  
  output_ports[g_num_channels] = NULL;
  
  if (jack_activate(client)) {
    log_printf(LOG_ERR, "cannot activate client");
    if (output_ports) free(output_ports);
    exit(1);
  }

  if (autoconnect) {
    const char **ports;
    ports = jack_get_ports(client, NULL, NULL,
                           JackPortIsPhysical|JackPortIsInput);
    if (!ports) {
      log_printf(LOG_ERR, "cannot find any physical capture ports\n");
    } else {
      for (i = 0; i < g_num_channels; ++i) {
        if (ports[i] == NULL) {
          break;
        }
        //sprintf(portname, "output_%d", i);
        if (jack_connect(client, jack_port_name(output_ports[i]), ports[i])) {
          log_printf(LOG_ERR, "cannot connect output ports\n");
        }
      }
      free(ports);
    }

#ifdef INPUT
    ports = jack_get_ports(client, NULL, NULL,
                           JackPortIsPhysical|JackPortIsOutput);
    //strcpy(portname, "input");
    if (!ports) {
      log_printf(LOG_ERR, "cannot find any physical capture ports\n");
    } else {
      if (jack_connect(client, ports[0], jack_port_name(input_port))) {
        log_printf(LOG_ERR, "cannot connect input port\n");
      }
      free(ports);
    }
#endif
  }

  return(client);
}

