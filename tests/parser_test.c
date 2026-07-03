#include <stdio.h>
#include <stdlib.h>
#include "../src/requestParser/Request.h"

int main(void)
{
    char *msg=
    "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n"
    ;
    parseRequestMessage(msg);
    return 0;
}