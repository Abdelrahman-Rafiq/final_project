#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <pthread.h>

// ======================= THREAD POOL STRUCT ==================
typedef struct threadpool
{
    pthread_t *threads; // Array of worker threads
    int *fd_queue;      // Circular queue of tasks
    int queue_size;     // Max number of tasks in queue
    int head;           // Queue head index
    int tail;           // Queue tail index
    int count;          // Number of tasks in queue

    pthread_mutex_t lock;  // Mutex for queue access
    pthread_cond_t notify; // Condition variable for new tasks

    int thread_count; // Number of worker threads
    bool shutdown;    // Shutdown flag
} threadpool_t;

threadpool_t *threadpool_create(int thread_count, int queue_size);
int threadpool_add(threadpool_t *pool, int new_fd);
void *threadpool_worker(void *arg);
int threadpool_destroy(threadpool_t *pool);

#endif