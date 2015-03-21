#ifndef __JOBQUEUE_H__
#define __JOBQUEUE_H__

#include <stdbool.h>


typedef struct {
    void* (*function)(void* args);   // function pointer
    void* args;                      // function's argument
} job_t;
#define JOB(function, args) ((job_t) { function, args })

typedef struct jobqueue jobqueue_t;


// Initialize job queue
//
jobqueue_t* jobqueue_init ();

// Push a job onto the queue
//
bool jobqueue_push (jobqueue_t* q, job_t j);

// Determine if queue is empty
//
bool jobqueue_is_empty(const jobqueue_t* q);

// Returns a reference to the job at the top of the queue
job_t* jobqueue_top (jobqueue_t* q);

// Removes top job and returns a copy of it
//
// If queue is empty, function returns false.  Otherwise, returns true and
// copies job onto j.  If j is not NULL, the top job is copied to the job
// referenced by t.  The job structure must be created beforehand by the
// calling thread.
//
bool jobqueue_pop (jobqueue_t* q, job_t* j);

// Returns the size or number of jobs in queue
//
unsigned int jobqueue_size (const jobqueue_t* q);

// De-allocates queue and its entries
//
// Job structures are not freed, the user is responsible
// of freeing them afterwards.
//
void jobqueue_destroy (jobqueue_t* q);

#endif
