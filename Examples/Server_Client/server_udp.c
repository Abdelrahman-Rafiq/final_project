#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MYPORT "4950"
#define MAXBUFLEN 100

int main(void)
{
    struct addrinfo hints, *res;
    int sockfd;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char buf[MAXBUFLEN];
    char s[INET6_ADDRSTRLEN];
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM; // UDP, not SOCK_STREAM
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, MYPORT, &hints, &res);

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    bind(sockfd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    // no listen(), no accept() — UDP has no connections!

    printf("server: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;
    numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
                         (struct sockaddr *)&their_addr, &addr_len);

    buf[numbytes] = '\0';

    inet_ntop(their_addr.ss_family,
              their_addr.ss_family == AF_INET
                ? (void *)&(((struct sockaddr_in *)&their_addr)->sin_addr)
                : (void *)&(((struct sockaddr_in6 *)&their_addr)->sin6_addr),
              s, sizeof s);

    printf("server: got packet from %s\n", s);
    printf("server: packet contains '%s'\n", buf);

    // send a reply back to wherever it came from
    char *msg = "Got it!\n";
    sendto(sockfd, msg, strlen(msg), 0,
           (struct sockaddr *)&their_addr, addr_len);

    close(sockfd);
    return 0;
}