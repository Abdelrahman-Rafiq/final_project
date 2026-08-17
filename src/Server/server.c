// #define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
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
#include "../../include/Config.h"

#define MAXDATASIZE 8192
/*
 * server.c
 * HTTP server implementation responsible for accepting clients,
 * parsing requests, serving files, and running CGI programs.
 *
 * AI assistance: Claude by Anthropic was used as a guidance tool
 * during development — for discussing connection handling, keep-alive
 * logic, and HTTP response behavior.
 * All code was written and verified by the author.
 */




/// @brief Check whether the beginning of a buffer looks like an HTTP request line
/// @param buf The input buffer to inspect
/// @return 1 if the buffer begins with a supported HTTP method, otherwise 0
int isValidHttpStart(const char *buf)
{
    return (strncmp(buf, "GET ", 4) == 0 ||
            strncmp(buf, "POST ", 5) == 0 ||
            strncmp(buf, "PUT ", 4) == 0 ||
            strncmp(buf, "DELETE ", 7) == 0 ||
            strncmp(buf, "HEAD ", 5) == 0 ||
            strncmp(buf, "OPTIONS ", 8) == 0);
}

/// @brief Send a minimal HTTP error response to a client socket
/// @param fd The client socket descriptor
/// @param code The HTTP status code to send
/// @param reason The reason phrase for the response
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

/// @brief Receive an HTTP request from a client, including any body data
/// @param fd The client socket descriptor
/// @param buf Buffer that receives the full request data
/// @param size Output parameter for the number of bytes stored in the buffer
/// @param leftover Buffer for any bytes that extend past the current request
/// @param leftover_len Output parameter for the leftover byte count
/// @return 1 on success, 0 on disconnect, -1 on receive error, or -2 on protocol error
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
            if (body_received < 0)
                body_received = 0;
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
                        if (cfg->verbose)
                            printf("client disconnected during body\n");
                        return 0;
                    }
                    if (numbytes == -1)
                    {
                        if ((errno == EAGAIN || errno == EWOULDBLOCK) && (cfg->verbose))
                        {
                            printf("timeout waiting for body\n");
                        }
                        else if (cfg->verbose)
                        {
                            perror("recv body");
                        }
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
            if (cfg->verbose)
                printf("garbage data received — sending 400\n");
            sendQuickError(fd, 400, "Bad Request");
            return -2;
        }

        if (*size >= MAXDATASIZE - 1)
        {
            if (cfg->verbose)
                printf("request headers too large — sending 431\n");
            sendQuickError(fd, 431, "Request Header Fields Too Large");
            return -2;
        }

        int numbytes = recv(fd, buf + *size, MAXDATASIZE - 1 - *size, 0);

        if (numbytes == 0)
        {
            if (cfg->verbose)
                printf("client disconnected\n");
            return 0;
        }
        if (numbytes == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                if (cfg->verbose)
                    printf("client idle too long, closing\n");
            }
            else if (cfg->verbose)
            {
                perror("recv");
            }
            return -1;
        }

        *size += numbytes;
        buf[*size] = '\0';
    }
}

/// @brief Send the appropriate HTTP response for a parsed request
/// @param fd The client socket descriptor
/// @param request The parsed HTTP request structure
/// @param statusCode The computed status code for the request
/// @param keepalive_secs_out Output parameter for the keep-alive timeout value
/// @return 1 if the connection should remain open, otherwise 0
int sendResponse(int fd, httpRequest *request, int statusCode,
                 int *keepalive_secs_out, int requests_remaining)
{
    if (strcmp(request->target, "/stats") == 0)
    {
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
        headerLen += sprintf(header + headerLen, "Connection: close\r\n\r\n");
        send(fd, header, headerLen, 0);
        send(fd, body, bodyLen, 0);
        *keepalive_secs_out = 0;
        return 0;
    }

    int keepalive_secs = 0;
    int should_close = 0;

    if (statusCode == 200)
    {
        keepalive_secs = getTimeout(request);
        if (keepalive_secs > cfg->max_keepalive_timeout)
            keepalive_secs = cfg->max_keepalive_timeout;
        else if (keepalive_secs == 0)
            keepalive_secs = cfg->default_keepalive_timeout;
        else if (keepalive_secs < 0)
            should_close = 1;
    }
    else
    {
        should_close = 1;
    }

    if (statusCode != 200)
    {
        sendQuickError(fd, statusCode, getMsgFromCode(statusCode));
        *keepalive_secs_out = 0;
        return 0;
    }

    // statusCode == 200
    char headerBuf[1024];
    int len = 0;
    len += sprintf(headerBuf + len, "HTTP/1.1 200 OK\r\n");
    len += sprintf(headerBuf + len, "Content-Type: %s\r\n",
                   getMimeType(request->target));
    len += sprintf(headerBuf + len, "Content-Length: %ld\r\n",
                   fileLength(request->target));
    len += sprintf(headerBuf + len, "Connection: %s\r\n",
                   (should_close || requests_remaining == 0) ? "close" : "keep-alive");
    if (!should_close && requests_remaining > 0)
        len += sprintf(headerBuf + len, "Keep-Alive: timeout=%d, max=%d\r\n",
                       keepalive_secs, requests_remaining);
    len += sprintf(headerBuf + len, "\r\n");

    if (send(fd, headerBuf, len, 0) == -1)
    {
        if (cfg->verbose)
            perror("send headers");
        return 0;
    }

    Node *cached = cache_get(request->target);
    if (!cached)
        cached = cache_put(request->target);

    if (cached)
    {
        if (send(fd, cached->data, cached->len, 0) == -1)
            if (errno != EPIPE && cfg->verbose)
                perror("send cached body");
    }
    else
    {
        if (sendFile(fd, request->target) == -1)
            if (errno != EPIPE && cfg->verbose)
                perror("send file");
    }

    *keepalive_secs_out = keepalive_secs;
    return !should_close;
}

/// @brief Send an HTTP response generated from a CGI script output
/// @param fd The client socket descriptor
/// @param cgiOutput The raw CGI output buffer
/// @param cgiLen The length of the CGI output buffer
/// @return 1 if the connection should remain open, otherwise 0
int sendCGIResponse(int fd, char *cgiOutput, size_t cgiLen, int requests_remaining)
{
    char *copy = malloc(cgiLen + 1);
    if (!copy)
    {
        sendQuickError(fd, 500, "Internal Server Error");
        return 0;
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
        return 0;
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
        return 0;
    }

    if (!content_type[0])
    {
        sendQuickError(fd, 500, "Internal Server Error");
        free(copy);
        return 0;
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
        return 0;
    }
    originalBody += sep_len;
    if (!content_length)
        content_length = cgiLen - (int)(originalBody - cgiOutput);

    int cgi_should_close = (content_length <= 0) || (requests_remaining == 0); // close if we're guessing
    // send HTTP response
    char headerBuf[1024];
    int len = 0;
    len += sprintf(headerBuf + len, "HTTP/1.1 %d %s\r\n",
                   status_code, getMsgFromCode(status_code));
    len += sprintf(headerBuf + len, "Content-Type: %s\r\n", content_type);
    len += sprintf(headerBuf + len, "Content-Length: %d\r\n", content_length);

    len += sprintf(headerBuf + len, "Connection: %s\r\n",
                   cgi_should_close ? "close" : "keep-alive");
    if (!cgi_should_close)
        len += sprintf(headerBuf + len, "Keep-Alive: timeout=%d, max=%d\r\n",
                       cfg->default_keepalive_timeout, requests_remaining);
    len += sprintf(headerBuf + len, "\r\n");

    if (send(fd, headerBuf, len, 0) == -1)
    {
        if (cfg->verbose)
            perror("send cgi headers");
        free(copy);
        return 0;
    }

    if (content_length && send(fd, originalBody, content_length, 0) == -1)
    {
        if (cfg->verbose)
            perror("send cgi body");
        free(copy);
        return 0;
    }
    free(copy);
    return !cgi_should_close;
}

/// @brief Prepare a child process for CGI execution by setting up environment variables and pipes
/// @param fds_pipe1 The stdin pipe file descriptors
/// @param fds_pipe2 The stdout pipe file descriptors
/// @param request The parsed HTTP request to convert into CGI environment values
void childRoutine(int *fds_pipe1, int *fds_pipe2, httpRequest *request)
{

    // Close unused ends
    close(fds_pipe1[1]);
    close(fds_pipe2[0]);

    if (dup2(fds_pipe1[0], STDIN_FILENO) == -1 || dup2(fds_pipe2[1], STDOUT_FILENO) == -1)
    {
        if (cfg->verbose)
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

    char totalPath[MAX_PATH * 2];
    totalPath[0] = '\0';
    snprintf(totalPath, sizeof(totalPath),
             "%s%s", cfg->data_root, scriptPath);
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
}

/// @brief Handle the parent side of a CGI fork by managing I/O and sending response
/// @param fd The client socket descriptor
/// @param pid The child process ID
/// @param fds_pipe1 The stdin pipe file descriptors (0=read, 1=write)
/// @param fds_pipe2 The stdout pipe file descriptors (0=read, 1=write)
/// @param request The parsed HTTP request data
/// @param requests_remaining The number of remaining requests on this connection
/// @return 1 to keep connection alive, 0 to close
static int parentRoutine(int fd, pid_t pid,
                          int *fds_pipe1, int *fds_pipe2,
                          httpRequest *request, int requests_remaining)
{
    
    close(fds_pipe1[0]);
    close(fds_pipe2[1]);

    // send POST body to child stdin
    if (!strncmp(request->method, "POST", 4) && request->body)
    {
        ssize_t total_written = 0;
        while (total_written < request->bodyLen)
        {
            ssize_t n = write(fds_pipe1[1],
                              request->body + total_written,
                              request->bodyLen - total_written);
            if      (n > 0)              total_written += n;
            else if (errno == EINTR)     continue;
            else { perror("write CGI stdin"); break; }
        }
    }
    close(fds_pipe1[1]);

    // set pipe read end non-blocking 
    int flags = fcntl(fds_pipe2[0], F_GETFL, 0);
    if (flags == -1 || fcntl(fds_pipe2[0], F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl");
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        close(fds_pipe2[0]);
        sendQuickError(fd, 500, "Internal Server Error");
        return 0;
    }

    // allocate CGI output buffer 
    size_t capacity  = 8192;
    size_t cgiLen    = 0;
    char  *cgiOutput = malloc(capacity);

    if (!cgiOutput)
    {
        perror("malloc");
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        close(fds_pipe2[0]);
        sendQuickError(fd, 500, "Internal Server Error");
        return 0;
    }

    // read CGI output with select() loop
    // outer loop: keeps calling select() until EOF or error
    // inner loop: drains all currently available bytes after select() fires
    int cgi_error    = 0;
    int cgi_finished = 0;
    char temp[4096];

    while (!cgi_finished && !cgi_error)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fds_pipe2[0], &readfds);

        struct timeval pipe_timeout = {.tv_sec = cfg->cgi_timeout, .tv_usec = 0};

        int ready = select(fds_pipe2[0] + 1, &readfds, NULL, NULL, &pipe_timeout);

        if (ready == -1)
        {
            if (errno == EINTR) continue;   // interrupted — try again
            perror("select");
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            close(fds_pipe2[0]);
            free(cgiOutput);
            sendQuickError(fd, 500, "Internal Server Error");
            return 0;
        }

        if (ready == 0)
        {
            // timeout — CGI script is hanging
            if (cfg->verbose) printf("CGI script timed out — killing child\n");
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            close(fds_pipe2[0]);
            free(cgiOutput);
            sendQuickError(fd, 504, "Gateway Timeout");
            return 0;
        }

        // data available — drain everything currently in the pipe
        while (!cgi_error)
        {
            ssize_t n = read(fds_pipe2[0], temp, sizeof temp);

            if (n > 0)
            {
                // grow buffer if needed
                while (cgiLen + (size_t)n + 1 > capacity)
                {
                    size_t  new_cap = capacity * 2;
                    char   *new_buf = realloc(cgiOutput, new_cap);
                    if (!new_buf)
                    {
                        perror("realloc");
                        kill(pid, SIGKILL);
                        waitpid(pid, NULL, 0);
                        close(fds_pipe2[0]);
                        free(cgiOutput);
                        sendQuickError(fd, 500, "Internal Server Error");
                        return 0;
                    }
                    cgiOutput = new_buf;
                    capacity  = new_cap;
                }
                memcpy(cgiOutput + cgiLen, temp, (size_t)n);
                cgiLen += (size_t)n;
            }
            else if (n == 0)
            {
                // EOF — CGI script closed stdout
                if (cfg->verbose) printf("CGI stdout EOF\n");
                cgi_finished = 1;
                break;
            }
            else if (errno == EINTR)
            {
                continue;   // interrupted syscall — retry read
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // pipe temporarily empty — more data may come
                // break inner loop, go back to select()
                break;
            }
            else
            {
                perror("read CGI pipe");
                cgi_error = 1;
                break;
            }
        }
    }

    cgiOutput[cgiLen] = '\0';
    close(fds_pipe2[0]);

    // reap child
    int child_status;
    waitpid(pid, &child_status, 0);

    if (cfg->verbose)
    {
        if (WIFEXITED(child_status))
            printf("CGI exit code = %d\n", WEXITSTATUS(child_status));
        else if (WIFSIGNALED(child_status))
            printf("CGI killed by signal %d\n", WTERMSIG(child_status));

        printf("cgiLen = %zu\n", cgiLen);
    }

    int alive = 0;
    if (!cgi_error)
        alive = sendCGIResponse(fd, cgiOutput, cgiLen, requests_remaining);

    free(cgiOutput);
    return alive;
}

/// @brief Handle requests from a single client connection for the lifetime of the socket
/// @param fd The client socket descriptor
void handleClient(int fd)
{
    struct timeval tv = {.tv_sec = cfg->initial_timeout, .tv_usec = 0};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1)
    {
        perror("setsockopt");
        return;
    }

    char buf[MAXDATASIZE];
    char leftover[MAXDATASIZE] = {0};
    int  leftover_len   = 0;
    int  requests_served = 0;

    while (1)
    {
        int size   = 0;
        int status = recvRequest(fd, buf, &size, leftover, &leftover_len);
        if (status != 1) break;

        requests_served++;
        int requests_remaining = cfg->max_keepalive_requests - requests_served;
        if (requests_remaining < 0) requests_remaining = 0;

        if (cfg->verbose)
        {
            printf("======================request==================\n");
            printf("%s\n", buf);
            printf("======================request==================\n");
        }

        httpRequest *request   = newHttpRequest();
        int          statusCode = 400;

        if (!parseRequestMessage(request, buf))
        {
            statusCode = getStatusCode(request);
            if (cfg->verbose) printHttpRequest(request);

            // CGI path
            if (!strncmp(request->target, "/cgi-bin/", 9))
            {
                int fds_pipe1[2], fds_pipe2[2];

                if (pipe(fds_pipe1) == -1 || pipe(fds_pipe2) == -1)
                {
                    perror("pipe");
                    sendQuickError(fd, 500, "Internal Server Error");
                    destroyRequest(request);
                    break;
                }

                pid_t pid = fork();

                if (pid == 0)
                {
                    // child
                    childRoutine(fds_pipe1, fds_pipe2, request);
                    // childRoutine never returns — it calls execve/exit
                }
                else if (pid > 0)
                {
                    // parent
                    int alive = parentRoutine(fd, pid,
                                              fds_pipe1, fds_pipe2,
                                              request, requests_remaining);
                    destroyRequest(request);
                    if (!alive || requests_remaining == 0) break;
                    continue;
                }
                else
                {
                    perror("fork");
                    close(fds_pipe1[0]); close(fds_pipe1[1]);
                    close(fds_pipe2[0]); close(fds_pipe2[1]);
                    sendQuickError(fd, 500, "Internal Server Error");
                    destroyRequest(request);
                    break;
                }
            }
        }
        else
        {
            if (cfg->verbose) printf("Error in parsing request!\n");
        }

        // normal (non-CGI) response path
        if (cfg->verbose)
        {
            printf("Status Code : %d\n", statusCode);
            printf("Timeout     : %d\n", getTimeout(request));
            printf("----------------------------------------------\n");
        }

        int keepalive_secs = 0;

        struct timespec t1, t2;
        clock_gettime(CLOCK_MONOTONIC, &t1);

        int keep_alive = sendResponse(fd, request, statusCode,
                                      &keepalive_secs, requests_remaining);

        clock_gettime(CLOCK_MONOTONIC, &t2);
        long ms = (t2.tv_sec - t1.tv_sec) * 1000 +
                  (t2.tv_nsec - t1.tv_nsec) / 1000000;

        if (cfg->verbose)
            printf("[%s] %d served in %ldms\n", request->target, statusCode, ms);

        destroyRequest(request);

        if (!keep_alive || requests_remaining == 0) break;

        tv.tv_sec = keepalive_secs;
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == -1)
        {
            if (cfg->verbose) perror("setsockopt");
            break;
        }
    }
}

