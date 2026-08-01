#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

/*
	The source of that code : https://www.codequoi.com/en/pipe-an-inter-process-communication-method/
*/
int main(void)
{
    int fd;
    int fd_copy;
    int nb_read;
    char buf[15];

    //	Print this process' PID to be able to check open fds
    printf("My PID is %d\n", getpid());
    //	Open the file and get its file descriptor
    fd = open("alpha.txt", O_RDONLY);
    if (fd == -1)
        return (1);
    //	Duplicate the fd on the smallest unused fd
    fd_copy = dup(fd);
    if (!fd_copy)
        return (1);
    //	Read the original fd
    nb_read = read(fd, buf, 10);
    if (nb_read == -1)
        return (1);
    buf[nb_read] = '\0';
    printf("fd %d contains : %s\n", fd, buf);
    //	Read the duplicated fd
    nb_read = read(fd_copy, buf, 10);
    if (nb_read == -1)
        return (0);
    buf[nb_read] = '\0';
    printf("fd %d contains : %s\n", fd_copy, buf);
    //	Duplicate fd on standard input
    dup2(fd, STDIN_FILENO);
    //	Infinite loop to go check open fds in this
    //	process with ls -la /proc/PID/fd
    while (1)
        ;
}