#ifndef REQUEST_H
#define REQUEST_H
#define MAX_HEADER_VALUE 1024
#define MAX_PATH 1024

typedef struct header
{
    char name[256];
    char value[MAX_HEADER_VALUE];

} Header;

typedef struct http_request
{
    char method[8];
    char target[MAX_PATH];
    char version[16];
    Header *first_header;
    int header_count;
} httpRequest;



httpRequest *newHttpRequest();
httpRequest * parseRequestMessage(char *);



#endif