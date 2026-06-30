#include <stdio.h>
#include <unistd.h>   // gethostname() lives here, not netdb.h!

int main(void)
{
    char hostname[256];

    if (gethostname(hostname, sizeof hostname) == -1) {
        perror("gethostname");
        return 1;
    }

    printf("My hostname is: %s\n", hostname);
    return 0;
}