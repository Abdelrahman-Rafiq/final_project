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

    if (!h->list) { free(h->arr); free(h); return NULL; }

    return h;
}

// ─────────────────────────────────────────────────────────────
//  insertHash
//
//  returns:
//    1  — inserted successfully
//    0  — failure (malloc, file read error)
//    2  — file too large to ever cache (> MAX_BYTES alone)
// ─────────────────────────────────────────────────────────────
int insertHash(Hash *h, const char *path)
{
    if (!h || !path) return 0;

    // don't insert duplicates
    if (searchNodeInHash(h, path)) return 1;

    // check file size BEFORE allocating the node
    long fsize = fileLength(path);
    if (fsize <= 0)  return 0;
    if (fsize > MAX_BYTES) return 2;   // single file too big to ever fit

    // evict LRU entries until there is room
    while (h->bytesConsumed + (int)fsize > MAX_BYTES)
        deleteLRU(h);

    // now load the file
    int   numOfBytes = 0;
    Node *n          = createNode(path, -1, &numOfBytes);
    if (!n) return 0;

    // allocate a chain link
    Chain *link = malloc(sizeof(Chain));
    if (!link) { free(n->data); free(n); return 0; }

    // compute bucket and store the chain index in the node
    unsigned int bucket = hashFunction(h, path);
    n->index = (int)bucket;
    link->node = n;

    // prepend to the bucket's chain
    link->next   = h->arr[bucket];
    h->arr[bucket] = link;

    // add to front of LRU list (most recently used)
    addAtBeginning(h->list, n);

    h->bytesConsumed += numOfBytes;
    h->usedSlots++;

    return 1;
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
            return link->node;
        }
        link = link->next;
    }
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