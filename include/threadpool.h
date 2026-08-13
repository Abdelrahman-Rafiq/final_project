#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stdbool.h>

typedef struct threadpool
{
    pthread_t *threads; // array of worker threads
    int *fd_queue;      // circular queue of client fds
    int queue_size;     // max tasks in queue
    int head;           // queue head index
    int tail;           // queue tail index
    int count;          // current tasks in queue

    pthread_mutex_t lock;  // protects queue access
    pthread_cond_t notify; // signals workers when a task arrives

    int thread_count;
    bool shutdown;
} threadpool_t;

threadpool_t *threadpool_create(int thread_count, int queue_size);
int threadpool_add(threadpool_t *pool, int new_fd);
int threadpool_destroy(threadpool_t *pool);

#endif