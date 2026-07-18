#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/HashTable.h"
#include "../include/DLL.h"

Hash *creatHashTable(int size)
{
    Hash *newHash = (Hash *)malloc(sizeof(Hash));
    newHash->arr = (Node **)malloc(sizeof(Node *) * size);
    for(int i=0 ;i<size ;i++)
    {
        newHash->arr[i] = NULL;
    }
    newHash->totalSlots = size;
    newHash->usedSlots = newHash->bytesConsumed = 0;
    newHash->tombstone = createNode("\0\0",2);
    newHash->list = createDLL();
    
    return newHash;
}

int isOccupied(Hash* h, int index) {
    Node* slot = h->arr[index];
    // Empty slot or tombstone (deleted) counts as available
    if (!slot || slot == h->tombstone) return 0;
    return 1;
}

int hashFunction(Hash *h, char *key)
{
    int x=0; int z = 33;
    while(*key)
    {
        x = x*z + *key;
    }
    return x % h->totalSlots; //////////////////////////////////////////
}

int next(Hash *h,int currentIndex, int currentCounter)
{

}

int insertHash(Hash *h,char *path)
{
    if(!h) return 0;

    int counter = 0;
    int index = hashFunction(h,path);

    if(!isOccupied(h,index))
    {
        // insert
        // Update consumed bytes here
        h->usedSlots ++;
    }
    else
    {
        // get next
        int i;
        int max_tries = 2*h->totalSlots;
        for( i=0 ;i <max_tries ;i++)
        {
            counter ++;
            index = next(h,index,counter);
            if(!isOccupied(h,index))
            {
                // Insert 
                // Update consumed bytes here
                h->usedSlots ++;
                break;
            }
        }
        if(i==max_tries) return 0;
    }
    return 1;
}

int searchIndex(Hash *h,char *path)
{
    if(!h) return -1;
    int counter = 0;
    int index = hashFunction(h,path);
    int i = 0, max_tries = h->totalSlots *2;
    for(i = 0; i<max_tries ;i++)
    {
        //check here
        if(!h->arr[index])
        {

        }
        
        counter++;
        index = next(h,index,counter);
    }
}