#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../include/DLL.h"

// ─────────────────────────────────────────────────────────────
//  createDLL
// ─────────────────────────────────────────────────────────────
DLL *createDLL(void)
{
    DLL *list = malloc(sizeof(DLL));
    if (!list)
        return NULL;
    list->head = list->tail = NULL;
    return list;
}

// ─────────────────────────────────────────────────────────────
//  createNode
//
//  path == NULL  → creates a sentinel/tombstone node (no data)
//  path != NULL  → reads file into heap buffer
//
//  returns NULL on any failure
// ─────────────────────────────────────────────────────────────
Node *createNode(const char *path, int index, int *numOfBytes)
{
    Node *n = malloc(sizeof(Node));
    if (!n)
        return NULL;

    n->next = n->prev = NULL;
    n->data = NULL;
    n->len = 0;
    n->index = index;
    n->key[0] = '\0';

    if (!path)
    {
        // sentinel node — used as tombstone
        if (numOfBytes)
            *numOfBytes = 0;
        return n;
    }

    // build full path
    char totalPath[MAX_PATH];
    snprintf(totalPath, sizeof(totalPath),
             "/home/rafiq/final_project/src/data%s", path);

    // get file size
    FILE *f = fopen(totalPath, "rb");
    if (!f)
    {
        free(n);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    rewind(f);

    if (fileSize <= 0)
    {
        fclose(f);
        free(n);
        return NULL;
    }

    // allocate buffer and read — fixes the uninitialized pointer bug
    n->data = malloc((size_t)fileSize);
    if (!n->data)
    {
        fclose(f);
        free(n);
        return NULL;
    }

    size_t bytesRead = fread(n->data, 1, (size_t)fileSize, f);
    fclose(f);

    if ((long)bytesRead != fileSize)
    {
        free(n->data);
        free(n);
        return NULL;
    }

    n->len = (int)fileSize;
    strncpy(n->key, path, MAX_PATH - 1);
    n->key[MAX_PATH - 1] = '\0';

    if (numOfBytes)
        *numOfBytes = n->len;
    return n;
}

// ─────────────────────────────────────────────────────────────
//  addAtBeginning — insert at MRU end (head)
// ─────────────────────────────────────────────────────────────
void addAtBeginning(DLL *list, Node *n)
{
    if (!list || !n)
        return;

    n->next = NULL;
    n->prev = list->head;

    if (list->head)
        list->head->next = n;
    else
        list->tail = n; // first node — also the tail

    list->head = n;
}

// ─────────────────────────────────────────────────────────────
//  moveToFront — called on cache hit to mark as most recently used
// ─────────────────────────────────────────────────────────────
void moveToFront(DLL *list, Node *n)
{
    if (!list || !n || list->head == n)
        return; // already at front

    // unlink from current position
    if (n->prev)
        n->prev->next = n->next;
    if (n->next)
        n->next->prev = n->prev;

    if (list->tail == n)
        list->tail = n->next; // n was tail, promote its neighbor

    n->prev = NULL;
    n->next = NULL;

    // re-insert at head
    addAtBeginning(list, n);
}

// ─────────────────────────────────────────────────────────────
//  removeFromEnd — evict LRU node (tail)
//  returns the chain index of the removed node, or -1
// ─────────────────────────────────────────────────────────────
int removeFromEnd(DLL *list, int *numOfBytes)
{
    if (!list || !list->tail)
        return -1;

    Node *n = list->tail;

    if (list->head == list->tail)
    {
        list->head = list->tail = NULL;
    }
    else
    {
        list->tail = n->next; // tail moves toward head
        list->tail->prev = NULL;
    }

    if (numOfBytes)
        *numOfBytes = n->len;
    int index = n->index;

    free(n->data);
    free(n);
    return index;
}

// ─────────────────────────────────────────────────────────────
//  destroyDLL — free every node then the list itself
// ─────────────────────────────────────────────────────────────
void destroyDLL(DLL *list)
{
    if (!list)
        return;

    Node *cur = list->tail; // start from LRU end
    while (cur)
    {
        Node *next = cur->next;
        free(cur->data);
        free(cur);
        cur = next;
    }
    free(list);
}