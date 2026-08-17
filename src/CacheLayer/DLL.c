#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../include/DLL.h"
#include "../../include/Config.h"

/*
 * DLL.c
 * Doubly linked list implementation used to maintain LRU ordering
 * and eviction state for cached file entries.
 *
 * AI assistance: Claude by Anthropic was used as a guidance tool
 * during development — for discussing list semantics, eviction order,
 * and memory-management trade-offs.
 * All code was written and verified by the author.
 */


/// @brief Create and initialize a new doubly linked list
/// @return A pointer to the newly created DLL structure, or NULL on failure
DLL *createDLL(void)
{
    DLL *list = malloc(sizeof(DLL));
    if (!list)
        return NULL;
    list->head = list->tail = NULL;
    return list;
}

/// @brief Create a new linked-list node containing file data
/// @param path The file path to load
/// @param index The node index used by the cache
/// @param numOfBytes Output parameter for the loaded data size
/// @return A pointer to the new node, or NULL on failure
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
    totalPath[0] = '\0';
    snprintf(totalPath, sizeof(totalPath),
             "%s%s",cfg->data_root, path);

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


/// @brief Insert a node at the front of the list as the most recently used entry
/// @param list The doubly linked list to update
/// @param n The node to insert
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


/// @brief Move an existing node to the front of the list
/// @param list The doubly linked list to update
/// @param n The node to promote to MRU position
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


/// @brief Remove and return the least recently used node from the end of the list
/// @param list The doubly linked list to update
/// @param numOfBytes Output parameter for the removed node's data size
/// @return The removed node's index, or -1 if the list is empty
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


/// @brief Free all nodes in the list and destroy the list structure
/// @param list The doubly linked list to release
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