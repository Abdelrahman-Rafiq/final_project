#ifndef HELPERS_H
#define HELPERS_H

typedef unsigned char BYTE;

typedef struct
{
    const char *extension;
    const char *mime;
} MimeType;
long fileLength(const char *filename);
const char *getMimeType(const char *filename);
const char *getMsgFromCode(int code);
int sendFile(int sockfd, const char *filename);
#endif