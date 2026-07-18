#ifndef DLL_H
#define DLL_H
#define EntryType unsigned char *
#define MAX_FILE_SIZE 800000000

typedef struct node
{
    EntryType data[MAX_FILE_SIZE];
    struct node *prev, *next;
} Node;

typedef struct dll
{
    Node *head;
    Node *tail;
} DLL;
DLL *createDLL();
Node *createNode(EntryType data ,int len);
void addAtTheBeginning (DLL *list, EntryType data ,int len);
void removeFromEnd(DLL *list);
void destroyDLL (DLL *list);

#endif