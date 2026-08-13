#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

/*
 I got this code from this tutorial https://thelinuxcode.com/fork-exec-coding-c/
*/
int main() {

  pid_t pid = fork();

  if (pid == 0) { // child

    char *args[] = {"./process2", NULL};
    execvp(args[0], args);

  } else { // parent

    int status;
    waitpid(pid, &status, 0);

    printf("Child process finished\n");
    printf("my pid : %d\n",getpid());  //This line is added by me
  }

  return 0;
}