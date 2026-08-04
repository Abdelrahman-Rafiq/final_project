// #define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include "../../include/Server.h"
#include "../../include/helpers.h"
#include "../../include/Cache.h"

#define MAXDATASIZE 8192
#define DEFAULT_KEEPALIVE_TIMEOUT 30
#define MAX_KEEPALIVE_TIMEOUT 120
#define INITIAL_TIMEOUT 5

#define CHILD_EXIT_CODE 21
#define EXPECTED_CODE 21

int isValidHttpStart(const char *buf)
{
    return (strncmp(buf, "GET ", 4) == 0 ||
            strncmp(buf, "POST ", 5) == 0 ||
            strncmp(buf, "PUT ", 4) == 0 ||
            strncmp(buf, "DELETE ", 7) == 0 ||
            strncmp(buf, "HEAD ", 5) == 0 ||
            strncmp(buf, "OPTIONS ", 8) == 0);
}

void sendQuickError(int fd, int code, const char *reason)
{
    char body[1024];
    sprintf(body,
            "<html><body><h1>%d %s</h1></body></html>",
            code, reason);

    int contentLength = strlen(body);
    char buf[512];
    int len = 0;
    len += sprintf(buf + len, "HTTP/1.1 %d %s\r\n", code, reason);
    len += sprintf(buf + len, "Content-Type: text/html\r\n");
    len += sprintf(buf + len, "Content-Length: %d\r\n", contentLength);
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
    *size = 0;

    if (*leftover_len > 0)
    {
        memcpy(buf, leftover, *leftover_len);
        buf[*leftover_len] = '\0';
        *size = *leftover_len;
        *leftover_len = 0;
        leftover[0] = '\0';
    }

    while (1)
    {
        char *end = strstr(buf, "\r\n\r\n");
        if (end != NULL)
        {
            char *headers_start = buf;
            char *body_start = end + 4;
            int body_received = (buf + *size) - body_start;

            // check if there is a Content-Length header
            int content_length = 0;
            char *cl = strcasestr(headers_start, "Content-Length:");
            if (cl)
                content_length = atoi(cl + strlen("Content-Length:"));

            if (content_length > 0)
            {
                // keep receiving until we have the full body
                while (body_received < content_length)
                {
                    int space = MAXDATASIZE - 1 - *size;
                    if (space <= 0)
                    {
                        // body too large for buffer
                        sendQuickError(fd, 413, "Content Too Large");
                        return -2;
                    }

                    int numbytes = recv(fd, buf + *size, space, 0);

                    if (numbytes == 0)
                    {
                        printf("client disconnected during body\n");
                        return 0;
                    }
                    if (numbytes == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            printf("timeout waiting for body\n");
                        else
                            perror("recv body");
                        return -1;
                    }

                    *size += numbytes;
                    buf[*size] = '\0';
                    body_received += numbytes;
                }
            }

            // handle leftover (bytes beyond the body)
            int total_request_size = (int)(body_start - buf) + content_length;
            char *next = buf + total_request_size;
            int next_len = (buf + *size) - next;

            if (next_len > 0)
            {
                memcpy(leftover, next, next_len);
                leftover[next_len] = '\0';
                *leftover_len = next_len;
                buf[total_request_size] = '\0';
                *size = total_request_size;
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

        if (numbytes == 0)
        {
            if (VERBOSE)
                printf("client disconnected\n");
            return 0;
        }
        if (numbytes == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                printf("client idle too long, closing\n");
            else
                perror("recv");
            return -1;
        }

        *size += numbytes;
        buf[*size] = '\0';
    }
}

// ─────────────────────────────────────────────────────────────
//  sendResponse
// ─────────────────────────────────────────────────────────────
int sendResponse(int fd, httpRequest *request, int statusCode,
                 int *keepalive_secs_out)
{
    if (strcmp(request->target, "/stats") == 0)
    {
        // get counters from cache
        int hits = cache_hits();
        int misses = cache_misses();

        char body[256];
        int bodyLen = sprintf(body,
                              "hits: %d\nmisses: %d\ntotal: %d\nhit rate: %.1f%%\n",
                              hits, misses, hits + misses,
                              (hits + misses) > 0 ? (100.0f * hits / (hits + misses)) : 0.0f);

        char header[256];
        int headerLen = 0;
        headerLen += sprintf(header + headerLen, "HTTP/1.1 200 OK\r\n");
        headerLen += sprintf(header + headerLen, "Content-Type: text/plain\r\n");
        headerLen += sprintf(header + headerLen, "Content-Length: %d\r\n", bodyLen);
        headerLen += sprintf(header + headerLen, "Connection: close\r\n");
        headerLen += sprintf(header + headerLen, "\r\n");

        send(fd, header, headerLen, 0);
        send(fd, body, bodyLen, 0);

        *keepalive_secs_out = 0;
        return 0; // close after stats
    }

    int keepalive_secs = 0;
    int should_close = 0;

    if (statusCode == 200)
    {
        keepalive_secs = getTimeout(request);
        if (keepalive_secs > MAX_KEEPALIVE_TIMEOUT)
            keepalive_secs = MAX_KEEPALIVE_TIMEOUT;
        else if (keepalive_secs == 0)
            keepalive_secs = DEFAULT_KEEPALIVE_TIMEOUT;
        else if (keepalive_secs < 0)
            should_close = 1;
    }
    else
    {
        should_close = 1;
    }

    char headerBuf[1024];
    int len = 0;
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

    if (send(fd, headerBuf, len, 0) == -1)
    {
        perror("send headers");
        return 0;
    }

    if (statusCode == 200)
    {
        Node *cached = cache_get(request->target);

        if (!cached)
            cached = cache_put(request->target); // miss → try to insert

        if (cached)
        {
            // serve from memory — either hit or just inserted
            if (send(fd, cached->data, cached->len, 0) == -1)
                if (errno != EPIPE)
                    perror("send cached body");
        }
        else
        {
            // file too big or read error — fall back to disk
            if (sendFile(fd, request->target) == -1)
                if (errno != EPIPE)
                    perror("send file");
        }
    }
    else
    {
        char errBody[256];
        int errLen = sprintf(errBody,
                             "<html><body><h1>%d %s</h1></body></html>",
                             statusCode, getMsgFromCode(statusCode));
        if (send(fd, errBody, errLen, 0) == -1)
            perror("send error body");
    }

    *keepalive_secs_out = keepalive_secs;
    return !should_close;
}

void sendCGIResponse(int fd, char *cgiOutput, size_t cgiLen)
{
    char *copy = malloc(cgiLen + 1);
    if (!copy)
    {
        sendQuickError(fd, 500, "Internal Server Error");
        return;
    }
    memcpy(copy, cgiOutput, cgiLen);
    copy[cgiLen] = '\0';

    char *start_body = strstr(copy, "\r\n\r\n");
    int sep_len = 4;
    if (!start_body)
    {
        start_body = strstr(copy, "\n\n");
        sep_len = 2;
    }

    if (!start_body)
    {
        sendQuickError(fd, 500, "Internal Server Error");
        free(copy);
        return;
    }
    start_body += sep_len;

    // parse CGI headers
    int status_code = 200;
    int content_length = 0;
    char content_type[256] = {0};
    char location[256] = {0};

    char *save_ptr;
    char *token = strtok_r(copy, "\r\n", &save_ptr);

    while (token)
    {
        if (token >= start_body)
            break;

        char *colon = strchr(token, ':');
        if (!colon)
        {
            token = strtok_r(NULL, "\r\n", &save_ptr);
            continue;
        }

        *colon = '\0';
        char *name = token;
        char *value = colon + 1 + strspn(colon + 1, " ");

        if (!strcmp(name, "Content-Type"))
            strcpy(content_type, value);
        else if (!strcmp(name, "Status"))
            status_code = atoi(value);
        else if (!strcmp(name, "Location"))
            strcpy(location, value);
        else if (!strcmp(name, "Content-Length"))
            content_length = atoi(value);

        token = strtok_r(NULL, "\r\n", &save_ptr);
    }

    if (location[0])
    {
        char headerBuf[512];
        int len = 0;
        len += sprintf(headerBuf + len, "HTTP/1.1 302 Found\r\n");
        len += sprintf(headerBuf + len, "Location: %s\r\n", location);
        len += sprintf(headerBuf + len, "Content-Length: 0\r\n\r\n");
        send(fd, headerBuf, len, 0);
        free(copy);
        return;
    }

    if (!content_type[0])
    {
        sendQuickError(fd, 500, "Internal Server Error");
        free(copy);
        return;
    }

    char *originalBody = strstr(cgiOutput, "\r\n\r\n");
    sep_len = 4;
    if (!originalBody)
    {
        originalBody = strstr(cgiOutput, "\n\n");
        sep_len = 2;
    }
    if (!originalBody)
    {
        sendQuickError(fd, 500, "Invalid CGI Response");
        free(copy);
        return;
    }
    originalBody += sep_len;
    if (!content_length)
        content_length = cgiLen - (int)(originalBody - cgiOutput);

    // send HTTP response
    char headerBuf[1024];
    int len = 0;
    len += sprintf(headerBuf + len, "HTTP/1.1 %d %s\r\n",
                   status_code, getMsgFromCode(status_code));
    len += sprintf(headerBuf + len, "Content-Type: %s\r\n", content_type);
    len += sprintf(headerBuf + len, "Content-Length: %d\r\n", content_length); // fix 4
    len += sprintf(headerBuf + len, "Connection: close\r\n\r\n");

    if (send(fd, headerBuf, len, 0) == -1)
    {
        perror("send cgi headers");
        free(copy);
        return;
    }

    if (send(fd, originalBody, content_length, 0) == -1)
        perror("send cgi body");

    free(copy);
}

void childRoutine(int *fds_pipe1, int *fds_pipe2, httpRequest *request)
{

    // Close unused ends
    close(fds_pipe1[1]);
    close(fds_pipe2[0]);

    if (dup2(fds_pipe1[0], STDIN_FILENO) == -1 || dup2(fds_pipe2[1], STDOUT_FILENO) == -1)
    {
        perror("Error duplicating the fds\n");
        close(fds_pipe1[0]);
        close(fds_pipe2[1]);
        exit(1);
    }
    // Environment Variables generation
    char env_method[256] = "REQUEST_METHOD=";
    char env_query[256] = "QUERY_STRING=";
    char env_path[256] = "PATH_INFO=";
    char env_script[256] = "SCRIPT_NAME=";
    char env_content_len[256] = "CONTENT_LENGTH=";
    char env_content_type[256] = "CONTENT_TYPE=";
    char env_server_name[256] = "SERVER_NAME=localhost";
    char env_server_port[256] = "SERVER_PORT=3490";
    char env_host[256] = "HTTP_HOST=";
    char env_agent[256] = "HTTP_USER_AGENT=";
    char env_accept[256] = "HTTP_ACCEPT=";
    strcat(env_method, request->method);
    char *s = strchr(request->target, '?');
    strcat(env_query, s ? s + 1 : "");
    strcat(env_path, request->target);
    strcat(env_script, request->target);

    for (int i = 0; i < request->header_count; i++)
    {
        if (!strcmp(request->headers[i].name, "Host"))
        {
            strcat(env_host, request->headers[i].value);
        }
        else if (!strcmp(request->headers[i].name, "User-Agent"))
        {
            strcat(env_agent, request->headers[i].value);
        }
        else if (!strcmp(request->headers[i].name, "Accept"))
        {
            strcat(env_accept, request->headers[i].value);
        }
        else if (!strcmp(request->headers[i].name, "Content-Length"))
        {
            strcat(env_content_len, request->headers[i].value);
        }
        else if (!strcmp(request->headers[i].name, "Content-Type"))
        {
            strcat(env_content_type, request->headers[i].value);
        }
    }
    char scriptPath[MAX_PATH];
    strncpy(scriptPath, request->target, MAX_PATH - 1);
    char *q = strchr(scriptPath, '?');
    if (q)
        *q = '\0'; // cut off query string

    char totalPath[MAX_PATH] = "/home/rafiq/final_project/src/data";
    strcat(totalPath, scriptPath);
    char *argv[2] = {totalPath, NULL};
    if (request->bodyLen > 0)
    {
        char *env[] = {
            env_method, env_query, env_path, env_script,
            env_content_len, env_content_type, env_server_name,
            env_server_port, env_host, env_agent, env_accept, NULL};
        execve(totalPath, argv, env);
    }
    else
    {
        char *env[] = {
            env_method, env_query, env_path, env_script, env_server_name,
            env_server_port, env_host, env_agent, env_accept, NULL};
        execve(totalPath, argv, env);
    }
    exit(CHILD_EXIT_CODE);
}

// ─────────────────────────────────────────────────────────────
//  handleClient — called by the thread pool worker
// ─────────────────────────────────────────────────────────────
void handleClient(int fd)
{
    struct timeval tv;
    tv.tv_sec = INITIAL_TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1)
    {
        perror("setsockopt");
        return;
    }

    char buf[MAXDATASIZE];
    char leftover[MAXDATASIZE] = {0};
    int leftover_len = 0;

    while (1)
    {
        int size = 0;
        int status = recvRequest(fd, buf, &size, leftover, &leftover_len);
        if (status != 1)
            break;

        if (VERBOSE)
        {
            printf("======================request==================\n");
            printf("%s\n", buf);
            printf("======================request==================\n");
        }

        httpRequest *request = newHttpRequest();
        size_t capacity = 8192;
        char *cgiOutput = malloc(sizeof(char) * capacity);
        int statusCode;
        if (!parseRequestMessage(request, buf))
        {
            statusCode = getStatusCode(request);
            if (VERBOSE)
                printHttpRequest(request);

            // CGI works only if request is correct and target startswith "/cgi-bin/"!

            if (!strncmp(request->target, "/cgi-bin/", 9))
            {
                // Create 2 Pipes
                int fds_pipe1[2], fds_pipe2[2];

                if (pipe(fds_pipe1) == -1 || pipe(fds_pipe2) == -1)
                {
                    perror("Error creating the pipes!\n");
                }

                pid_t pid = fork();
                if (pid == 0) // Child
                {
                    childRoutine(fds_pipe1, fds_pipe2, request);
                }
                else if (pid > 0) // Parent
                {
                    int Status;
                    // Close unused ends
                    close(fds_pipe1[0]);
                    close(fds_pipe2[1]);

                    if (!strncmp(request->method, "POST", 4) && request->body)
                    {
                        // Server gets the data from body of the request
                        // Use the std_in pipe to send the data to child
                        write(fds_pipe1[1], request->body, request->bodyLen);
                    }
                    close(fds_pipe1[1]);

                    size_t cgiLen = 0;

                    char temp[4096];
                    ssize_t n;
                    while ((n = read(fds_pipe2[0], temp, sizeof(temp))) > 0)
                    {
                        if (cgiLen + n > capacity)
                        {
                            capacity *= 2;
                            cgiOutput = realloc(cgiOutput, capacity);
                        }

                        memcpy(cgiOutput + cgiLen, temp, n);
                        cgiLen += n;
                    }
                    cgiOutput[cgiLen] = '\0';

                    close(fds_pipe2[0]);

                    waitpid(pid, &Status, 0);

                    // Send the CGI Response
                    if (VERBOSE)
                    {
                        printf("cgiLen = %ld\n", cgiLen);

                        char *body = strstr(cgiOutput, "\r\n\r\n");
                        if (!body)
                            body = strstr(cgiOutput, "\n\n");

                        if (body)
                        {
                            body += strchr(body, '\r') ? 4 : 2;

                            printf("Body length = %ld\n", (long)(cgiOutput + cgiLen - body));
                        }
                    }
                    sendCGIResponse(fd, cgiOutput, cgiLen);
                    free(cgiOutput);
                    continue;
                }
                else
                {
                    perror("Error while creating fork!\n");
                }
            }
        }
        else
        {
            statusCode = 400;
            if (VERBOSE)
                printf("Error in parsing request!\n");
        }

        if (VERBOSE)
        {
            printf("Status Code : %d\n", statusCode);
            printf("Timeout     : %d\n", getTimeout(request));
            printf("----------------------------------------------\n");
        }
        int keepalive_secs = 0;

        // Time loggings
        struct timespec t1, t2;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        // Send Response normally
        int keep_alive = sendResponse(fd, request, statusCode, &keepalive_secs);

        clock_gettime(CLOCK_MONOTONIC, &t2);

        long ms = (t2.tv_sec - t1.tv_sec) * 1000 +
                  (t2.tv_nsec - t1.tv_nsec) / 1000000;
        // Time to serve Response
        if (VERBOSE)
            printf("[%s] %d served in %ldms\n", request->target, statusCode, ms);
        // Destroy Request
        destroyRequest(request);

        if (!keep_alive)
            break;

        tv.tv_sec = keepalive_secs;
        tv.tv_usec = 0;
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1)
        {
            perror("setsockopt");
            break;
        }
    }
}
