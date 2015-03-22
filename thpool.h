#ifndef __THPOOL_H__
#define __THPOOL_H__

#include <stdbool.h>

#include "jobqueue.h"

typedef struct thpool thpool_t;

// Initialize a thread pool of a specified size
thpool_t* thpool_init(unsigned int num_threads);

// Push a job to the queue
bool thpool_add_job(thpool_t* p, void *(*function)(void*), void* args);

// Return the size (number of threads) a pool has
unsigned int thpool_size(const thpool_t* p);

// De-allocated thread pool and its associated structures
void thpool_destroy(thpool_t* p);

#endif
