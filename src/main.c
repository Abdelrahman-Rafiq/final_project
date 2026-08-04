#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include "../include/threadpool.h"
#include "../include/Server.h"
#include "../include/Cache.h"
#define THREAD_COUNT 16
#define QUEUE_SIZE 50
#define BACKLOG 10
#define CACHE_SLOTS 1024
#define MYPORT "3490"

static void *get_in_addr(struct sockaddr *sa)
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

    signal(SIGPIPE, SIG_IGN);
    threadpool_t *pool = threadpool_create(THREAD_COUNT, QUEUE_SIZE);
    if (!pool)
    {
        fprintf(stderr, "failed to create thread pool\n");
        exit(1);
    }

    cache_init(CACHE_SLOTS);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, MYPORT, &hints, &res) != 0)
    {
        perror("getaddrinfo");
        exit(1);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1)
    {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("bind");
        exit(1);
    }

    freeaddrinfo(res);

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }

    printf("server: waiting for connections on port %s...\n", MYPORT);

    while (1)
    {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        if (VERBOSE)
        {
            printf("server: got connection from %s\n", s);
        }

        if (threadpool_add(pool, new_fd) != 0)
        {
            printf("queue full — rejecting connection from %s\n", s);
            sendQuickError(new_fd, 503, "Service Unavailable");
            close(new_fd); // rejected — safe to close here
        }
        // accepted — worker thread owns new_fd now, don't close here
    }

    threadpool_destroy(pool);
    cache_destroy();
    close(sockfd);
    return 0;
}