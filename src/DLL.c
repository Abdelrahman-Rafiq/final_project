#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/DLL.h"

DLL *createDLL()
{
    DLL *new = (DLL *)malloc(sizeof(DLL));
    new->head = new->tail = NULL;
    return new;
}

Node *createNode(EntryType data ,int len)
{
    Node *new= (Node *)malloc(sizeof(Node));
    new->next = new->prev = NULL;
    memcpy(new->data,data,len); 
}

void addAtTheBeginning (DLL *list, EntryType data ,int len)
{
    if(!list) return;
    Node *new = createNode(data,len);
    if(!list->head)
    {
        list->head = list->tail = new;
    }
    else
    {
        new->prev = list->head;
        list->head->next = new;
        list->head = new;
    }
}
// Evict
void removeFromEnd(DLL *list)
{
    if(!list || !list->head) return;
    Node *n;
    if(list->head == list->tail)
    {
        n = list->tail;
        list->head = list->tail = NULL;
    }
    else
    {
        n = list->tail;
        n->next->prev = NULL;
        list->tail = n->next;
    }
    free(n);
}

void destroyDLL (DLL *list)
{
    Node *n = list->head;
    Node *p = n;
    while(n)
    {
        n = n->prev;
        free(p);
        p = n;
    }
    free(list);
}

