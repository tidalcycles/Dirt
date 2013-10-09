#include <lo/lo.h>
#include <zmq.h>
#include <assert.h>
#include <stdlib.h>

#define MAXSZ 1024

lo_server s;
void *publisher;

void error(int num, const char *msg, const char *path) {
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
}

/**/

int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, lo_message msg, void *user_data) {
    int i;
    int sz = lo_message_length(msg, path);
    void *m = lo_message_serialise(msg, path, NULL, NULL);
    printf("message\n");
    zmq_send(publisher, m, sz, 0);
    free(m);
    return 1;
}

void osc_init() {
  s = lo_server_new("7777", error);
  lo_server_add_method(s, NULL, NULL, generic_handler, NULL);
}

int main (void) {
    //  Prepare our context and publisher
    void *context = zmq_ctx_new ();
    publisher = zmq_socket (context, ZMQ_PUB);
    int rc = zmq_bind (publisher, "tcp://*:5556");
    void *osc[MAXSZ];

    assert (rc == 0);
    rc = zmq_bind(publisher, "ipc://dirt.ipc");
    assert (rc == 0);

    osc_init();

    while (1) {
      int sz = lo_server_recv(s);
      //printf("processed message of size %d\n", sz);
    }
    zmq_close (publisher);
    zmq_ctx_destroy (context);
    return 0;
}

