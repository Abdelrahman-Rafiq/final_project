#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define SERVERPORT "4950"
#define MAXBUFLEN 100

int main(int argc, char *argv[])
{
    struct addrinfo hints, *res, *p;
    int sockfd, numbytes;
    char buf[MAXBUFLEN];

    if (argc != 3) {
        fprintf(stderr, "usage: client hostname message\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    getaddrinfo(argv[1], SERVERPORT, &hints, &res);

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd != -1) break;
    }

    // note: no connect()! just send straight away with sendto()
    sendto(sockfd, argv[2], strlen(argv[2]), 0, p->ai_addr, p->ai_addrlen);
    freeaddrinfo(res);

    printf("client: sent '%s'\n", argv[2]);

    // wait for the reply
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
                         (struct sockaddr *)&their_addr, &addr_len);
    buf[numbytes] = '\0';
    printf("client: received reply '%s'", buf);

    close(sockfd);
    return 0;
}