#ifndef DLL_H
#define DLL_H

#include "helpers.h"

#define EntryType unsigned char *

typedef struct node
{
    char      key[MAX_PATH];
    EntryType data;          // heap-allocated file bytes
    int       index;         // which chain index this node lives in (for LRU eviction)
    int       len;           // byte length of data
    struct node *prev;       // toward tail  (LRU end)
    struct node *next;       // toward head  (MRU end)
} Node;

typedef struct dll
{
    Node *head;  // MRU end — most recently used
    Node *tail;  // LRU end — least recently used, evicted first
} DLL;

DLL  *createDLL(void);
Node *createNode(const char *path, int index, int *numOfBytes);
void  addAtBeginning(DLL *list, Node *n);
void  moveToFront(DLL *list, Node *n);      // call on cache hit
int   removeFromEnd(DLL *list, int *numOfBytes);
void  destroyDLL(DLL *list);

#endif