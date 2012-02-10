#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lo/lo.h>

int main(int argc, char *argv[]) {
    lo_address t = lo_address_new(NULL, "7771");

    int result = lo_send(t, "/play", "s", "drum/0");
    printf("result: %d\n", result);

    return(0);
}
