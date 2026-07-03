#include <stdio.h>
#include <stdlib.h>
#include "../src/requestParser/Request.h"

int main(void)
{
    httpRequest*req = newHttpRequest();
    char *msg=
    "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n"
    ;
    if(!parseRequestMessage(req,msg))
    printHttpRequest(req);
    else printf("Error in parsing!");
    
    return 0;
}