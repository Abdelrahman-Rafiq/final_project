#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <pthread.h>
#include "DLL.h"

#define MAX_BYTES 67108864   // 64 MB

// In separate chaining each slot holds a linked list of Nodes
// that hashed to the same bucket.
// The LRU DLL is shared across all buckets for eviction.

typedef struct chain
{
    Node        *node;   // the cached file node
    struct chain *next;  // next entry in this bucket's chain
} Chain;

typedef struct hash
{
    Chain **arr;          // array of Chain* buckets
    DLL   *list;          // global LRU list across all buckets
    int    totalSlots;
    int    usedSlots;
    int    bytesConsumed; 
    int hits_counter;
    int miss_counter;
} Hash;

Hash *createHashTable(int size);
Node *insertHash(Hash *h, const char *path);
Node *searchNodeInHash(Hash *h, const char *path);
void  deleteLRU(Hash *h);
void  destroyHash(Hash *h);

#endif