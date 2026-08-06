// cache.c
#include <pthread.h>
#include "../../include/Cache.h"

static Hash *cache = NULL; // private — not exposed
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/// @brief Initialize the shared cache with a given number of slots
/// @param slots The number of hash-table buckets to create
void cache_init(int slots)
{
    cache = createHashTable(slots);
}

/// @brief Retrieve a cached node for the given path
/// @param path The cache key to look up
/// @return The cached node if found, otherwise NULL
Node *cache_get(const char *path)
{
    pthread_mutex_lock(&lock);
    Node *n = searchNodeInHash(cache, path);
    pthread_mutex_unlock(&lock);
    return n;
}

/// @brief Insert a file into the cache for the given path
/// @param path The file path to cache
/// @return The inserted node on success, or NULL on failure
Node *cache_put(const char *path)
{
    pthread_mutex_lock(&lock);
    Node *result = insertHash(cache, path);
    pthread_mutex_unlock(&lock);
    return result;
}
/// @brief Return the number of cache hits recorded so far
/// @return The total number of cache hits
int cache_hits(void)
{
    pthread_mutex_lock(&lock);
    int h = cache->hits_counter;
    pthread_mutex_unlock(&lock);
    return h;
}

/// @brief Return the number of cache misses recorded so far
/// @return The total number of cache misses
int cache_misses(void)
{
    pthread_mutex_lock(&lock);
    int m = cache->miss_counter;
    pthread_mutex_unlock(&lock);
    return m;
}
/// @brief Destroy the shared cache and release all associated memory
void cache_destroy(void)
{
    pthread_mutex_lock(&lock);
    destroyHash(cache);
    cache = NULL;
    pthread_mutex_unlock(&lock);
}