#ifndef SERVER_H
#define SERVER_H
#include "./Request.h"

int isValidHttpStart(const char *buf);
void sendQuickError(int fd, int code, const char *reason);
void childRoutine(int *fds_pipe1, int *fds_pipe2, httpRequest *request);
int recvRequest(int fd, char *buf, int *size, char *leftover, int *leftover_len);
int sendResponse(int fd, httpRequest *request, int statusCode,
                 int *keepalive_secs_out, int requests_remaining);
int sendCGIResponse(int fd, char *cgiOutput, size_t cgiLen,
                    int requests_remaining);
void handleClient(int fd);

#endif