#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/HashTable.h"
#include "../../include/DLL.h"
#include "../../include/helpers.h"
#include "../../include/Config.h"

/*
 * HashTable.c
 * Hash table-backed cache index that maps request paths to cached nodes
 * and manages lookup, insertion, and eviction behavior.
 *
 * AI assistance: Claude by Anthropic was used as a guidance tool
 * during development — for discussing hashing strategy, collision handling,
 * and cache replacement decisions.
 * All code was written and verified by the author.
 */

/// @brief Compute the bucket index for a cache key using the djb2 hash function
/// @param h The hash table instance
/// @param key The cache key to hash
/// @return The computed bucket index
static unsigned int hashFunction(Hash *h, const char *key)
{
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*key++))  
        hash = hash * 33 + c;
    return (unsigned int)(hash % (unsigned long)h->totalSlots);
}


/// @brief Create and initialize a new hash table for cached content
/// @param size The number of buckets to create
/// @return A pointer to the new hash table, or NULL on failure
Hash *createHashTable(int size)
{
    if (size <= 0) return NULL;

    Hash *h = malloc(sizeof(Hash));
    if (!h) return NULL;

    h->arr = calloc(size, sizeof(Chain *));  // all buckets start NULL
    if (!h->arr) { free(h); return NULL; }

    h->list          = createDLL();
    h->totalSlots    = size;
    h->usedSlots     = 0;
    h->bytesConsumed = 0;
    h->hits_counter = h->miss_counter = 0;

    if (!h->list) { free(h->arr); free(h); return NULL; }

    return h;
}


/// @brief Insert a file into the cache hash table if it fits within the cache limit
/// @param h The hash table to update
/// @param path The file path to cache
/// @return The inserted node on success, or NULL on (failure - file is too large to ever cache)
Node *insertHash(Hash *h, const char *path)
{
    if (!h || !path) return NULL;

    long fsize = fileLength(path);
    if (fsize <= 0)   return NULL;
    if (fsize > cfg->max_cache_bytes) return NULL;  // too big — caller uses sendFile to handle it

    while (h->bytesConsumed + (int)fsize > cfg->max_cache_bytes)
        deleteLRU(h);

    int   numOfBytes = 0;
    Node *n          = createNode(path, -1, &numOfBytes);
    if (!n) return NULL;

    Chain *link = malloc(sizeof(Chain));
    if (!link) { free(n->data); free(n); return NULL; }

    unsigned int bucket = hashFunction(h, path);
    n->index   = (int)bucket;
    link->node = n;
    link->next = h->arr[bucket];
    h->arr[bucket] = link;

    addAtBeginning(h->list, n);
    h->bytesConsumed += numOfBytes;
    h->usedSlots++;

    return n; 
}


/// @brief Search for a cached node by path and promote it to MRU position on hit
/// @param h The hash table to search
/// @param path The cache key to look up
/// @return The matching node on cache hit, or NULL on cache miss
Node *searchNodeInHash(Hash *h, const char *path)
{
    if (!h || !path) return NULL;

    unsigned int bucket = hashFunction(h, path);
    Chain       *link   = h->arr[bucket];

    while (link)
    {
        if (strcmp(link->node->key, path) == 0)
        {
            // cache hit — move to front of LRU list
            moveToFront(h->list, link->node);
            h->hits_counter++;
            return link->node;
        }
        link = link->next;
    }
    h->miss_counter++;
    return NULL;   // cache miss
}


/// @brief Evict the least recently used node from the cache
/// @param h The hash table whose LRU entry should be removed
void deleteLRU(Hash *h)
{
    if (!h || !h->list->tail) return;

    // peek at LRU node before removing it
    Node        *victim = h->list->tail;
    unsigned int bucket = (unsigned int)victim->index;
    const char  *key    = victim->key;

    // remove the chain link from the bucket
    Chain *prev = NULL;
    Chain *cur  = h->arr[bucket];
    while (cur)
    {
        if (strcmp(cur->node->key, key) == 0)
        {
            if (prev) prev->next    = cur->next;
            else      h->arr[bucket] = cur->next;

            free(cur);   // free the chain link 
            break;
        }
        prev = cur;
        cur  = cur->next;
    }

    // remove from LRU list and free the node
    int numOfBytes = 0;
    removeFromEnd(h->list, &numOfBytes);

    h->bytesConsumed -= numOfBytes;
    h->usedSlots--;
}


/// @brief Free all memory used by the hash table and its cached nodes
/// @param h The hash table to destroy
void destroyHash(Hash *h)
{
    if (!h) return;

    // free all chain links (nodes are freed by destroyDLL)
    for (int i = 0; i < h->totalSlots; i++)
    {
        Chain *cur = h->arr[i];
        while (cur)
        {
            Chain *next = cur->next;
            free(cur);   // just the link — node memory owned by DLL
            cur = next;
        }
    }

    destroyDLL(h->list);   // frees all nodes and their data buffers
    free(h->arr);
    free(h);
}