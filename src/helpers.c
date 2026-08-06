#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "../include/helpers.h"

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

/// @brief Return the MIME type associated with a file extension
/// @param filename The name of the file to inspect
/// @return The MIME type string, or application/octet-stream if unknown
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

/// @brief Get the size of a file from the server data directory
/// @param filename The path of the file to inspect, starting with '/'
/// @return The file size in bytes, or -1 on failure
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

/// @brief Map an HTTP status code to its human-readable message
/// @param code The HTTP status code
/// @return The corresponding message string, or NULL if the code is unknown
const char *getMsgFromCode(int code)
{
    switch (code)
    {
    case 200:
        return "OK";
    case 302:
        return "Found";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 500:
        return "Internal Server Error";
    case 504:
        return "Gateway Timeout";
    case 505:
        return "HTTP Version Not Supported";
    default:
        return NULL;
    }
}

/// @brief Send a file from the server data directory over a socket in chunks of 4KB
/// @param sockfd The socket descriptor to write to
/// @param filename The file path to send, starting with '/'
/// @return 0 on success, or -1 on failure
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
