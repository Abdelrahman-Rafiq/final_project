#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include "../../include/Threadpool.h"

// ======================= THREADPOOL CREATE ====================
threadpool_t *threadpool_create(int thread_count, int queue_size) {
    if (thread_count <= 0 || queue_size <= 0) return NULL;

    threadpool_t *pool = malloc(sizeof(threadpool_t));
    if (!pool) return NULL;

    pool->thread_count = thread_count;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = false;

    pool->threads = malloc(sizeof(pthread_t) * thread_count);
    pool->fd_queue = malloc(sizeof(int) * queue_size);

    if (!pool->threads || !pool->fd_queue) {
        free(pool->threads);
        free(pool->fd_queue);
        free(pool);
        return NULL;
    }

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->notify, NULL);
    // pthread_cond_init(&pool->empty, NULL);

    // Create worker threads
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, threadpool_worker, pool) != 0) {
            threadpool_destroy(pool);
            return NULL;
        }
    }

    return pool;
}

// ======================= ADD TASK TO POOL ====================
int threadpool_add(threadpool_t *pool, int new_fd) {
    if (!pool) return -1;

    pthread_mutex_lock(&pool->lock);

    // Queue full
    if (pool->count == pool->queue_size) {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    // Add new file descriptor to queue
    pool->fd_queue[pool->tail] = new_fd;
    pool->tail = (pool->tail + 1) % pool->queue_size;
    pool->count++;

    // Signal a worker
    pthread_cond_signal(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    return 0;
}

// ======================= WORKER THREAD FUNCTION ===============
void *threadpool_worker(void *arg) {
    threadpool_t *pool = (threadpool_t *)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        // Wait for tasks
        while (pool->count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->notify, &pool->lock);
        }

        // Shutdown check
        if (pool->shutdown && pool->count == 0) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        // Get task from queue
        int fd = pool->fd_queue[pool->head];
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count--;

        pthread_mutex_unlock(&pool->lock);

        // Do its work with fd
    }

    return NULL;
}

// ======================= DESTROY THREADPOOL ===================
int threadpool_destroy(threadpool_t *pool) {
    if (!pool) return -1;

    pthread_mutex_lock(&pool->lock);
    pool->shutdown = true;

    // Wake up all threads
    pthread_cond_broadcast(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    // Join all threads
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // Cleanup
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->notify);
    // pthread_cond_destroy(&pool->empty);
    free(pool->threads);
    free(pool->fd_queue);
    free(pool);

    return 0;
}

