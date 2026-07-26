// cache.c
#include <pthread.h>
#include "../../include/Cache.h"

static Hash *cache = NULL; // private — not exposed
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void cache_init(int slots)
{
    cache = createHashTable(slots);
}

Node *cache_get(const char *path)
{
    pthread_mutex_lock(&lock);
    Node *n = searchNodeInHash(cache, path);
    pthread_mutex_unlock(&lock);
    return n;
}

Node *cache_put(const char *path)
{
    pthread_mutex_lock(&lock);
    Node *result = insertHash(cache, path);
    pthread_mutex_unlock(&lock);
    return result;
}
int cache_hits(void)
{
    pthread_mutex_lock(&lock);
    int h = cache->hits_counter;
    pthread_mutex_unlock(&lock);
    return h;
}

int cache_misses(void)
{
    pthread_mutex_lock(&lock);
    int m = cache->miss_counter;
    pthread_mutex_unlock(&lock);
    return m;
}
void cache_destroy(void)
{
    pthread_mutex_lock(&lock);
    destroyHash(cache);
    cache = NULL;
    pthread_mutex_unlock(&lock);
}