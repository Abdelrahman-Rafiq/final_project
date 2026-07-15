#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include "../../include/helpers.h"
#include "../../include/Request.h"
#include "../../include/Threadpool.h"

#define MYPORT                    "3490"
#define BACKLOG                   10
#define MAXDATASIZE               8192
#define DEFAULT_KEEPALIVE_TIMEOUT 30
#define MAX_KEEPALIVE_TIMEOUT     120
#define INITIAL_TIMEOUT           5
#define THREAD_COUNT              16
#define QUEUE_SIZE                50

// ─────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *)sa)->sin_addr);
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int isValidHttpStart(const char *buf)
{
    return (strncmp(buf, "GET ",     4) == 0 ||
            strncmp(buf, "POST ",    5) == 0 ||
            strncmp(buf, "PUT ",     4) == 0 ||
            strncmp(buf, "DELETE ",  7) == 0 ||
            strncmp(buf, "HEAD ",    5) == 0 ||
            strncmp(buf, "OPTIONS ", 8) == 0);
}

void sendQuickError(int fd, int code, const char *reason)
{
    char buf[512];
    int  len = 0;
    len += sprintf(buf + len, "HTTP/1.1 %d %s\r\n", code, reason);
    len += sprintf(buf + len, "Content-Type: text/html\r\n");
    len += sprintf(buf + len, "Connection: close\r\n");
    len += sprintf(buf + len, "\r\n");
    len += sprintf(buf + len,
                   "<html><body><h1>%d %s</h1></body></html>", code, reason);
    send(fd, buf, len, 0);
}

// ─────────────────────────────────────────────────────────────
//  recvRequest
// ─────────────────────────────────────────────────────────────
int recvRequest(int fd, char *buf, int *size,
                char *leftover, int *leftover_len)
{
    buf[0] = '\0';
    *size  = 0;

    if (*leftover_len > 0)
    {
        memcpy(buf, leftover, *leftover_len);
        buf[*leftover_len] = '\0';
        *size              = *leftover_len;
        *leftover_len      = 0;
        leftover[0]        = '\0';
    }

    while (1)
    {
        char *end = strstr(buf, "\r\n\r\n");
        if (end != NULL)
        {
            char *next     = end + 4;
            int   next_len = (buf + *size) - next;
            if (next_len > 0)
            {
                memcpy(leftover, next, next_len);
                leftover[next_len] = '\0';
                *leftover_len      = next_len;
                *(end + 4)         = '\0';
                *size              = (int)(end + 4 - buf);
            }
            return 1;
        }

        if (*size > 0 && !isValidHttpStart(buf))
        {
            printf("garbage data received — sending 400\n");
            sendQuickError(fd, 400, "Bad Request");
            return -2;
        }

        if (*size >= MAXDATASIZE - 1)
        {
            printf("request headers too large — sending 431\n");
            sendQuickError(fd, 431, "Request Header Fields Too Large");
            return -2;
        }

        int numbytes = recv(fd, buf + *size, MAXDATASIZE - 1 - *size, 0);

        if (numbytes == 0)  { printf("client disconnected\n"); return 0;  }
        if (numbytes == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                printf("client idle too long, closing\n");
            else
                perror("recv");
            return -1;
        }

        *size     += numbytes;
        buf[*size] = '\0';
    }
}

// ─────────────────────────────────────────────────────────────
//  sendResponse
// ─────────────────────────────────────────────────────────────
int sendResponse(int fd, httpRequest *request, int statusCode,
                 int *keepalive_secs_out)
{
    int keepalive_secs = 0;
    int should_close   = 0;

    if (statusCode == 200)
    {
        keepalive_secs = getTimeout(request);
        if      (keepalive_secs > MAX_KEEPALIVE_TIMEOUT) keepalive_secs = MAX_KEEPALIVE_TIMEOUT;
        else if (keepalive_secs == 0)                    keepalive_secs = DEFAULT_KEEPALIVE_TIMEOUT;
        else if (keepalive_secs < 0)                     should_close   = 1;
    }
    else
    {
        should_close = 1;
    }

    char headerBuf[1024];
    int  len = 0;
    len += sprintf(headerBuf + len, "HTTP/1.1 %d %s\r\n",
                   statusCode, getMsgFromCode(statusCode));
    len += sprintf(headerBuf + len, "Content-Type: %s\r\n",
                   getMimeType(request->target));
    len += sprintf(headerBuf + len, "Content-Length: %ld\r\n",
                   fileLength(request->target));
    len += sprintf(headerBuf + len, "Connection: %s\r\n",
                   should_close ? "close" : "keep-alive");
    if (!should_close)
        len += sprintf(headerBuf + len, "Keep-Alive: timeout=%d, max=%d\r\n",
                       keepalive_secs, MAX_KEEPALIVE_TIMEOUT);
    len += sprintf(headerBuf + len, "\r\n");

    if (send(fd, headerBuf, len, 0) == -1) { perror("send headers"); return 0; }

    if (statusCode == 200)
    {
        if (sendFile(fd, request->target) == -1) { perror("send file"); return 0; }
    }
    else
    {
        char errBody[256];
        int  errLen = sprintf(errBody,
                              "<html><body><h1>%d %s</h1></body></html>",
                              statusCode, getMsgFromCode(statusCode));
        if (send(fd, errBody, errLen, 0) == -1) perror("send error body");
    }

    *keepalive_secs_out = keepalive_secs;
    return !should_close;
}

// ─────────────────────────────────────────────────────────────
//  handleClient — called by the thread pool worker
// ─────────────────────────────────────────────────────────────
void handleClient(int fd)
{
    struct timeval tv;
    tv.tv_sec  = INITIAL_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1)
    {
        perror("setsockopt");
        return;
    }

    char buf[MAXDATASIZE];
    char leftover[MAXDATASIZE] = {0};
    int  leftover_len          = 0;

    while (1)
    {
        int size   = 0;
        int status = recvRequest(fd, buf, &size, leftover, &leftover_len);
        if (status != 1) break;

        printf("======================request==================\n");
        printf("%s\n", buf);
        printf("======================request==================\n");

        httpRequest *request = newHttpRequest();

        int statusCode;
        if (!parseRequestMessage(request, buf))
        {
            statusCode = getStatusCode(request);
            printHttpRequest(request);
        }
        else
        {
            statusCode = 400;
            printf("Error in parsing request!\n");
        }

        printf("Status Code : %d\n", statusCode);
        printf("Timeout     : %d\n", getTimeout(request));
        printf("----------------------------------------------\n");

        int keepalive_secs = 0;
        int keep_alive     = sendResponse(fd, request, statusCode, &keepalive_secs);
        destroyRequest(request);

        if (!keep_alive) break;

        tv.tv_sec  = keepalive_secs;
        tv.tv_usec = 0;
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1)
        {
            perror("setsockopt");
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────
int main(void)
{
    struct addrinfo         hints, *res;
    int                     sockfd, new_fd;
    struct sockaddr_storage their_addr;
    socklen_t               addr_size;
    char                    s[INET6_ADDRSTRLEN];

    signal(SIGPIPE, SIG_IGN);
    threadpool_t *pool = threadpool_create(THREAD_COUNT, QUEUE_SIZE);
    if (!pool) { fprintf(stderr, "failed to create thread pool\n"); exit(1); }

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    if (getaddrinfo(NULL, MYPORT, &hints, &res) != 0)
        { perror("getaddrinfo"); exit(1); }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) { perror("socket"); exit(1); }

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
        { perror("bind"); exit(1); }

    freeaddrinfo(res);

    if (listen(sockfd, BACKLOG) == -1) { perror("listen"); exit(1); }

    printf("server: waiting for connections on port %s...\n", MYPORT);

    while (1)
    {
        addr_size = sizeof their_addr;
        new_fd    = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) { perror("accept"); continue; }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (threadpool_add(pool, new_fd) != 0)
        {
            printf("queue full — rejecting connection from %s\n", s);
            sendQuickError(new_fd, 503, "Service Unavailable");
            close(new_fd);   // rejected — safe to close here
        }
        // accepted — worker thread owns new_fd now, don't close here
    }

    threadpool_destroy(pool);
    close(sockfd);
    return 0;
}