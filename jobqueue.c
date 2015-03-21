#include <stdlib.h>
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
};

jobqueue_t* jobqueue_init() {
    jobqueue_t* q = malloc(sizeof(jobqueue_t));
    if (!q) return NULL;
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;

    return q;
}

bool jobqueue_push(jobqueue_t* q, job_t j) {
    entry_t* e = malloc(sizeof(entry_t));
    if (!e) return false;
    e->job = j;
    e->next = NULL;

    if (q->head == NULL) {
        q->head = e;
        q->tail = e;
    } else {
        q->tail->next = e;
        q->tail = e;
    }
    q->size++;

    return true;
}

bool jobqueue_is_empty(const jobqueue_t* q) {
    return q->head == NULL;
}

job_t* jobqueue_top(const jobqueue_t* q) {
    assert(q->head != NULL);
    job_t* top = &q->head->job;

    return top;
}

bool jobqueue_pop (jobqueue_t* q, job_t* j) {
    if (q->head == NULL) return false;

    entry_t* top = q->head;

    if (j) *j = top->job;

    if (q->head == q->tail) {
        q->head = q->tail = NULL;
    } else {
        q->head = q->head->next;
    }
    q->size--;

    free(top);

    return true;
}

unsigned int jobqueue_size(const jobqueue_t* q) {
    return q->size;
}

void jobqueue_destroy(jobqueue_t* q) {
    entry_t* cur = q->head;
    while (cur != NULL) {
        entry_t* next = cur->next;
        free(cur);
        cur = next;
    };
    free(q);
}
