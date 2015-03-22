#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

#include "thpool.h"

static void* thread_do(thpool_t* p);

struct thpool {
    pthread_t* threads;
    jobqueue_t* queue;
    unsigned int num_threads;

    pthread_mutex_t update_mutex;
    pthread_cond_t  update_cv;

    bool running;
};

thpool_t* thpool_init(unsigned int num_threads) {
    thpool_t* p = malloc(sizeof(thpool_t));
    if (!p) return NULL;

    p->threads = malloc(sizeof(pthread_t) * num_threads);
    if (!p->threads) return NULL;

    p->queue = jobqueue_init();

    pthread_mutex_init(&p->update_mutex, NULL);
    pthread_cond_init(&p->update_cv, NULL);

    p->num_threads = num_threads;
    p->running = true;

    // Initialize and detach threads
    for (int i = 0; i < num_threads; i++) {
        int rc = pthread_create(&p->threads[i], NULL, (void*) thread_do, p);
        if (rc) return NULL;
        pthread_detach(p->threads[i]);
    }

    return p;
}

bool thpool_add_job(thpool_t* p, void *(*function)(void*), void* args) {
    bool res = jobqueue_push(p->queue, JOB(function, args));

    if (res) {
        pthread_mutex_lock(&p->update_mutex);
        pthread_cond_broadcast(&p->update_cv);
        pthread_mutex_unlock(&p->update_mutex);
    }

    return res;
}

unsigned int thpool_size(const thpool_t* p) {
    return p->num_threads;
}

void thpool_destroy(thpool_t* p) {
    p->running = false;

    pthread_mutex_lock(&p->update_mutex);
    pthread_cond_broadcast(&p->update_cv);
    pthread_mutex_unlock(&p->update_mutex);

    // TODO: Wait for threads to destroy themselves, somehow
    // ...

    free(p->threads);
    jobqueue_destroy(p->queue);

    pthread_mutex_destroy(&p->update_mutex);
    pthread_cond_destroy(&p->update_cv);

    free(p);
}


static void* thread_do(thpool_t* p) {
    job_t j;

    while (true) {
        while (p->running && jobqueue_pop(p->queue, &j)) {
            j.function(j.args);
        }

        if (!p->running) break;

        pthread_mutex_lock(&p->update_mutex);
        pthread_cond_wait(&p->update_cv, &p->update_mutex);
        pthread_mutex_unlock(&p->update_mutex);
    }

    pthread_exit(NULL);
}
