#ifndef REQUEST_H
#define REQUEST_H
#define MAX_HEADER_VALUE 1024
#define MAX_PATH 1024
#define MAX_HEADERS 50

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
    Header headers[MAX_HEADERS]; // Like an array-based Stack
    int header_count;
} httpRequest;

httpRequest *newHttpRequest();
void printHttpRequest(httpRequest *);
void destroyRequest(httpRequest *req);
int parseRequestMessage(httpRequest *, char *);
void addHeader(httpRequest *, Header);
int getStatusCode(httpRequest *request);
int getTimeout(httpRequest *request);

#endif