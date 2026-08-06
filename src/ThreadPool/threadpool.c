#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "../../include/threadpool.h"
#include "../../include/Server.h"


/// @brief Run the worker loop that processes queued client file descriptors
/// @param arg A pointer to the threadpool instance
/// @return Always NULL
static void *threadpool_worker(void *arg)
{
    threadpool_t *pool = (threadpool_t *)arg;

    while (1)
    {
        pthread_mutex_lock(&pool->lock);

        // sleep until there's a task or shutdown is requested
        while (pool->count == 0 && !pool->shutdown)
            pthread_cond_wait(&pool->notify, &pool->lock);

        // exit if shutting down and nothing left to do
        if (pool->shutdown && pool->count == 0)
        {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        // dequeue the next fd
        int fd = pool->fd_queue[pool->head];
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count--;

        pthread_mutex_unlock(&pool->lock);

        // handle the client — worker owns fd until done
        handleClient(fd);
        close(fd);
    }

    return NULL;
}


/// @brief Create and initialize a new thread pool
/// @param thread_count The number of worker threads to start
/// @param queue_size The maximum number of queued file descriptors
/// @return A pointer to the created threadpool, or NULL on failure
threadpool_t *threadpool_create(int thread_count, int queue_size)
{
    if (thread_count <= 0 || queue_size <= 0)
        return NULL;

    threadpool_t *pool = malloc(sizeof(threadpool_t));
    if (!pool)
        return NULL;

    pool->thread_count = thread_count;
    pool->queue_size = queue_size;
    pool->head = 0;
    pool->tail = 0;
    pool->count = 0;
    pool->shutdown = false;

    pool->threads = malloc(sizeof(pthread_t) * thread_count);
    pool->fd_queue = malloc(sizeof(int) * queue_size);

    if (!pool->threads || !pool->fd_queue)
    {
        free(pool->threads);
        free(pool->fd_queue);
        free(pool);
        return NULL;
    }

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->notify, NULL);

    for (int i = 0; i < thread_count; i++)
    {
        if (pthread_create(&pool->threads[i], NULL,
                           threadpool_worker, pool) != 0)
        {
            threadpool_destroy(pool);
            return NULL;
        }
    }

    return pool;
}


/// @brief Enqueue a client file descriptor for processing by the thread pool
/// @param pool The threadpool to add the task to
/// @param new_fd The file descriptor to schedule
/// @return 0 on success, or -1 if the queue is full or the pool is invalid
int threadpool_add(threadpool_t *pool, int new_fd)
{
    if (!pool)
        return -1;

    pthread_mutex_lock(&pool->lock);

    if (pool->count == pool->queue_size)
    {
        pthread_mutex_unlock(&pool->lock);
        return -1; // queue full
    }

    pool->fd_queue[pool->tail] = new_fd;
    pool->tail = (pool->tail + 1) % pool->queue_size;
    pool->count++;

    pthread_cond_signal(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    return 0;
}


/// @brief Shut down the thread pool and release all associated resources
/// @param pool The threadpool to destroy
/// @return 0 on success, or -1 if the pool is invalid
int threadpool_destroy(threadpool_t *pool)
{
    if (!pool)
        return -1;

    pthread_mutex_lock(&pool->lock);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->notify); // wake all sleeping workers
    pthread_mutex_unlock(&pool->lock);

    for (int i = 0; i < pool->thread_count; i++)
        pthread_join(pool->threads[i], NULL);

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->notify);
    free(pool->threads);
    free(pool->fd_queue);
    free(pool);

    return 0;
}