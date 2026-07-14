#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../../include/helpers.h"
#ifndef MAX_PATH
#define MAX_PATH 1024
#endif
#define FILE_CHUNK_SIZE 4096 // send 4KB at a time

static const MimeType mimeTypes[] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".txt", "text/plain"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".webp", "image/webp"},
    {".pdf", "application/pdf"},
    {".zip", "application/zip"},
    {".mp3", "audio/mpeg"},
    {".wav", "audio/wav"},
    {".mp4", "video/mp4"},
    {".webm", "video/webm"},
    {NULL, NULL}};

const char *getMimeType(const char *filename)
{
    const char *ext = strrchr(filename, '.');

    if (ext == NULL)
        return "application/octet-stream";

    for (int i = 0; mimeTypes[i].extension != NULL; i++)
    {
        if (strcmp(ext, mimeTypes[i].extension) == 0)
            return mimeTypes[i].mime;
    }

    return "application/octet-stream";
}

// Filename should start with '/' returns -1 in failure
long fileLength(const char *filename)
{
    char path[MAX_PATH];

    snprintf(path, sizeof(path),
             "/home/rafiq/final_project/src/data%s",
             filename);

    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return -1;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return -1;
    }

    long length = ftell(f);
    fclose(f);

    return length;
}

const char *getMsgFromCode(int code)
{
    switch (code)
    {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 500:
        return "Internal Server Error";
    case 505:
        return "HTTP Version Not Supported";
    default:
        return NULL;
    }
}

// send an entire file in 4KB chunks with a sendall inner loop
int sendFile(int sockfd, const char *filename)
{
    char path[MAX_PATH];
    snprintf(path, sizeof(path),
             "/home/rafiq/final_project/src/data%s", filename);

    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return -1;

    BYTE chunk[FILE_CHUNK_SIZE];
    size_t bytesRead;

    while ((bytesRead = fread(chunk, 1, FILE_CHUNK_SIZE, f)) > 0)
    {
        // sendall loop — send() might not send everything in one call
        int total_sent = 0;
        int remaining = bytesRead;

        while (remaining > 0)
        {
            int sent = send(sockfd, chunk + total_sent, remaining, 0);
            if (sent == -1)
            {
                fclose(f);
                return -1;
            }
            total_sent += sent;
            remaining -= sent;
        }
    }

    fclose(f);
    return 0;
}

