#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "jobqueue.h"

typedef struct {
    job_t job;
    void* next;
} entry_t;

struct jobqueue {
    entry_t* head;
    entry_t* tail;
    unsigned int size;
    pthread_mutex_t lock;
};

jobqueue_t* jobqueue_init() {
    jobqueue_t* q = malloc(sizeof(jobqueue_t));
    if (!q) return NULL;

    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);

    return q;
}

bool jobqueue_push(jobqueue_t* q, job_t j) {
    entry_t* e = malloc(sizeof(entry_t));
    if (!e) return false;
    e->job = j;
    e->next = NULL;

    pthread_mutex_lock(&q->lock);

    if (q->head == NULL) {
        q->head = e;
        q->tail = e;
    } else {
        q->tail->next = e;
        q->tail = e;
    }
    q->size++;

    pthread_mutex_unlock(&q->lock);

    return true;
}

bool jobqueue_is_empty(const jobqueue_t* q) {
    return q->head == NULL;
}

job_t* jobqueue_top(jobqueue_t* q) {
    pthread_mutex_lock(&q->lock);

    assert(q->head != NULL);
    job_t* top = &q->head->job;

    pthread_mutex_unlock(&q->lock);

    return top;
};

bool jobqueue_pop (jobqueue_t* q, job_t* j) {
    pthread_mutex_lock(&q->lock);

    if (q->head == NULL) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }

    entry_t* top = q->head;

    if (j) *j = top->job;

    if (q->head == q->tail) {
        q->head = q->tail = NULL;
    } else {
        q->head = q->head->next;
    }
    q->size--;

    pthread_mutex_unlock(&q->lock);

    free(top);

    return true;
}

unsigned int jobqueue_size(const jobqueue_t* q) {
    return q->size;
}

void jobqueue_destroy(jobqueue_t* q) {
    pthread_mutex_lock(&q->lock);

    entry_t* cur = q->head;
    while (cur != NULL) {
        entry_t* next = cur->next;
        free(cur);
        cur = next;
    };

    pthread_mutex_unlock(&q->lock);
    pthread_mutex_destroy(&q->lock);

    free(q);
}
