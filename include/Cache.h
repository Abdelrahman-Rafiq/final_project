#ifndef CACHE_H
#define CACHE_H
#include "./HashTable.h"


// cache.h
void  cache_init(int slots);
Node *cache_get(const char *path);   
Node *cache_put(const char *path);  
int cache_hits(void);
int cache_misses(void); 
void  cache_destroy(void);

#endif