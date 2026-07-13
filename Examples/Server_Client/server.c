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

int main(void)
{
    struct addrinfo hints, *res;
    int sockfd, new_fd;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, MYPORT, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket");
        exit(1);
    }

    // allow reusing the port immediately after restart
    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        exit(1);
    }

    freeaddrinfo(res);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections on port %s...\n", MYPORT);

    while (1) {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        // print who connected
        void *addr;
        if (their_addr.ss_family == AF_INET) {
            addr = &(((struct sockaddr_in *)&their_addr)->sin_addr);
        } else {
            addr = &(((struct sockaddr_in6 *)&their_addr)->sin6_addr);
        }
        inet_ntop(their_addr.ss_family, addr, s, sizeof s);
        printf("server: got connection from %s\n", s);

        // send a message to the client
        char *msg = "Hello from server!\n";
        if (send(new_fd, msg, strlen(msg), 0) == -1) {
            perror("send");
        }

        close(new_fd); // done with this client
    }

    close(sockfd);
    return 0;
}