#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MYPORT "3490"
#define BACKLOG 10
#define MAXDATASIZE 100

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in *)sa)->sin_addr);
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(void)
{
    struct addrinfo hints, *res;
    int sockfd, new_fd;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    char s[INET6_ADDRSTRLEN];
    char buf[MAXDATASIZE];
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, MYPORT, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) { perror("socket"); exit(1); }

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind"); exit(1);
    }

    freeaddrinfo(res);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen"); exit(1);
    }

    printf("server: waiting for connections...\n");

    while (1) {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) { perror("accept"); continue; }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if ((numbytes = recv(new_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
            perror("recv");
            exit(1);
        }
        buf[numbytes] = '\0';
        printf("server: message from client: %s\n", buf);

        close(new_fd);
    }

    close(sockfd);
    return 0;
}