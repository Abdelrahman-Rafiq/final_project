#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/HashTable.h"
#include "../include/DLL.h"
#include "../include/helpers.h"

// ─────────────────────────────────────────────────────────────
//  hashFunction — djb2 
// ─────────────────────────────────────────────────────────────
static unsigned int hashFunction(Hash *h, const char *key)
{
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*key++))  
        hash = hash * 33 + c;
    return (unsigned int)(hash % (unsigned long)h->totalSlots);
}

// ─────────────────────────────────────────────────────────────
//  createHashTable
// ─────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
//  insertHash — returns the inserted node, or NULL on failure
//  returns NULL if file is too large to ever cache (caller should sendFile from disk)
// ─────────────────────────────────────────────────────────────
Node *insertHash(Hash *h, const char *path)
{
    if (!h || !path) return NULL;

    long fsize = fileLength(path);
    if (fsize <= 0)   return NULL;
    if (fsize > MAX_BYTES) return NULL;  // too big — caller uses sendFile

    while (h->bytesConsumed + (int)fsize > MAX_BYTES)
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

// ─────────────────────────────────────────────────────────────
//  searchNodeInHash
//  also promotes the found node to MRU position
// ─────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
//  deleteLRU — evict the least recently used node
// ─────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
//  destroyHash
// ─────────────────────────────────────────────────────────────
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