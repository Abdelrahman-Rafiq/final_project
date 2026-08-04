#include <stdio.h>
#include <stdlib.h>
#include "../include/Request.h"

int main(void)
{
    httpRequest *req = newHttpRequest();
    char *msg =
        "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\nKeep-Alive: timeout=30\r\n\r\n";
    if (!parseRequestMessage(req, msg))
        printHttpRequest(req);
    else
        printf("Error in parsing!");
    printf("Status Code : %d\n", getStatusCode(req));
    printf("timout : %d\n", getTimeout(req));
    return 0;
}