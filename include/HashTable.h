#ifndef HASH_TABLE_H
#define HASH_TABLE_H
#include <stdbool.h>
#include "DLL.h"
#define MAX_BYTES 67108864   // 64 MB

typedef struct hash{
    Node **arr;        // array of Node * 
    DLL *list;
    Node *tombstone;   // for deletion in hash table
    int totalSlots;
    int usedSlots;
    int bytesConsumed;


}Hash;

#endif