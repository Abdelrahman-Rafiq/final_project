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
#include "../requestParser/Request.h"
#include "../helpers/helpers.h"

#define MYPORT                   "3490"
#define BACKLOG                  10
#define MAXDATASIZE              8192
#define DEFAULT_KEEPALIVE_TIMEOUT 30
#define MAX_KEEPALIVE_TIMEOUT    120
#define INITIAL_TIMEOUT          5      // tight timeout before first valid request
#define FILE_CHUNK_SIZE          4096   // send file 4KB at a time

// ─────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *)sa)->sin_addr);
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// returns 1 if buf starts with a recognised HTTP method
int isValidHttpStart(const char *buf)
{
    return (strncmp(buf, "GET ",     4) == 0 ||
            strncmp(buf, "POST ",    5) == 0 ||
            strncmp(buf, "PUT ",     4) == 0 ||
            strncmp(buf, "DELETE ",  7) == 0 ||
            strncmp(buf, "HEAD ",    5) == 0 ||
            strncmp(buf, "OPTIONS ", 8) == 0);
}

// send an error response immediately and return
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
//  Main
// ─────────────────────────────────────────────────────────────

int main(void)
{
    struct addrinfo         hints, *res;
    int                     sockfd, new_fd;
    struct sockaddr_storage their_addr;
    socklen_t               addr_size;
    char                    s[INET6_ADDRSTRLEN];
    char                    buf[MAXDATASIZE];
    int                     numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    if (getaddrinfo(NULL, MYPORT, &hints, &res) != 0)
    {
        perror("getaddrinfo");
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) { perror("socket"); exit(1); }

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("bind");
        exit(1);
    }

    freeaddrinfo(res);

    if (listen(sockfd, BACKLOG) == -1) { perror("listen"); exit(1); }

    printf("server: waiting for connections on port %s...\n", MYPORT);

    // ── outer loop — accept new clients ──────────────────────
    while (1)
    {
        addr_size = sizeof their_addr;
        new_fd    = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) { perror("accept"); continue; }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        // tight initial timeout — catches garbage senders immediately
        struct timeval tv;
        tv.tv_sec  = INITIAL_TIMEOUT;
        tv.tv_usec = 0;
        if (setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1)
        {
            perror("setsockopt");
            close(new_fd);
            continue;
        }

        // leftover buffer — holds bytes that arrived after \r\n\r\n
        char leftover[MAXDATASIZE] = {0};
        int  leftover_len          = 0;

        // ── inner loop — keep-alive: one iteration per request ─
        while (1)
        {
            httpRequest *request = newHttpRequest();

            buf[0]   = '\0';
            int size = 0;

            // restore any leftover bytes from the previous request
            if (leftover_len > 0)
            {
                memcpy(buf, leftover, leftover_len);
                buf[leftover_len] = '\0';
                size              = leftover_len;
                leftover_len      = 0;
                leftover[0]       = '\0';
            }

            // ── recv loop — accumulate until \r\n\r\n ──────────
            while (1)
            {
                // check if the buffer already holds a complete request
                char *end = strstr(buf, "\r\n\r\n");
                if (end != NULL)
                {
                    char *next     = end + 4;
                    int   next_len = (buf + size) - next;
                    if (next_len > 0)
                    {
                        // save the second request for the next iteration
                        memcpy(leftover, next, next_len);
                        leftover[next_len] = '\0';
                        leftover_len       = next_len;
                        *(end + 4)         = '\0';
                        size               = (int)(end + 4 - buf);
                    }
                    break;  // first request is isolated — ready to parse
                }

                // fix 2a — garbage detection: check after first bytes arrive
                if (size > 0 && !isValidHttpStart(buf))
                {
                    printf("garbage data received — sending 400\n");
                    sendQuickError(new_fd, 400, "Bad Request");
                    numbytes = -2;
                    break;
                }

                // fix 2b — buffer full but still no \r\n\r\n
                if (size >= MAXDATASIZE - 1)
                {
                    printf("request headers too large — sending 431\n");
                    sendQuickError(new_fd, 431,
                                   "Request Header Fields Too Large");
                    numbytes = -2;
                    break;
                }

                numbytes = recv(new_fd, buf + size, MAXDATASIZE - 1 - size, 0);

                if (numbytes == 0)
                {
                    printf("client disconnected\n");
                    break;
                }
                if (numbytes == -1)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        printf("client idle too long, closing\n");
                    else
                        perror("recv");
                    break;
                }

                size     += numbytes;
                buf[size] = '\0';
            }
            // ── end recv loop ───────────────────────────────────

            // any non-data exit → clean up and stop keep-alive
            if (numbytes <= 0 || numbytes == -2)
            {
                destroyRequest(request);
                break;
            }

            printf("======================request==================\n");
            printf("%s\n", buf);
            printf("======================request==================\n");

            // parse
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

            // decide keep-alive
            int keepalive_secs = 0;
            int should_close   = 0;

            if (statusCode == 200)
            {
                keepalive_secs = getTimeout(request);
                if (keepalive_secs > 0)
                {
                    if (keepalive_secs > MAX_KEEPALIVE_TIMEOUT)
                        keepalive_secs = MAX_KEEPALIVE_TIMEOUT;
                }
                else if (keepalive_secs == 0)
                {
                    keepalive_secs = DEFAULT_KEEPALIVE_TIMEOUT;
                }
                else
                {
                    should_close = 1;  // Connection: close
                }
            }
            else
            {
                should_close = 1;  // non-200 always closes
            }

            // ── Step 1: send headers ────────────────────────────
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
                len += sprintf(headerBuf + len,
                               "Keep-Alive: timeout=%d, max=%d\r\n",
                               keepalive_secs, MAX_KEEPALIVE_TIMEOUT);
            len += sprintf(headerBuf + len, "\r\n");

            if (send(new_fd, headerBuf, len, 0) == -1)
            {
                perror("send headers");
                destroyRequest(request);
                break;
            }

            // ── Step 2: send body ───────────────────────────────
            if (statusCode == 200)
            {
                if (sendFile(new_fd, request->target) == -1)
                {
                    perror("send file");
                    destroyRequest(request);
                    break;
                }
            }
            else
            {
                char errBody[256];
                int  errLen = sprintf(errBody,
                                      "<html><body><h1>%d %s</h1></body></html>",
                                      statusCode, getMsgFromCode(statusCode));
                if (send(new_fd, errBody, errLen, 0) == -1)
                    perror("send error body");
            }

            destroyRequest(request);

            // close after response if needed
            if (should_close) break;

            // fix 3 — switch to longer keep-alive timeout after first valid request
            tv.tv_sec  = keepalive_secs;
            tv.tv_usec = 0;
            if (setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1)
            {
                perror("setsockopt");
                break;
            }

        }  // end inner keep-alive loop

        close(new_fd);

    }  // end outer accept loop

    close(sockfd);
    return 0;
}