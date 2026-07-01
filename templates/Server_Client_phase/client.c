#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define MAXDATASIZE 100

int main(int argc, char *argv[])
{
    struct addrinfo hints, *res, *p;
    int sockfd, numbytes;
    char buf[MAXDATASIZE];

    if (argc != 2) {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(argv[1], "3490", &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd); continue;
        }
        break;
    }

    if (p == NULL) { fprintf(stderr, "client: failed to connect\n"); exit(1); }

    freeaddrinfo(res);

    // // TURN 1 — client receives what server sends first
    // if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
    //     perror("recv");
    //     exit(1);
    // }
    // buf[numbytes] = '\0';
    // printf("client: received from server: %s\n", buf);

    // TURN 2 — client sends its message
    char *msg = "Hello from client!\n";
    if (send(sockfd, msg, strlen(msg), 0) == -1)
        perror("send");

    close(sockfd);
    return 0;
}