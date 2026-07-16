#ifndef SERVER_H
#define SERVER_H
#include "./Request.h"

int isValidHttpStart(const char *buf);
void sendQuickError(int fd, int code, const char *reason);
int recvRequest(int fd, char *buf, int *size, char *leftover, int *leftover_len);
int sendResponse(int fd, httpRequest *request, int statusCode, int *keepalive_secs_out);
void handleClient(int fd);

#endif