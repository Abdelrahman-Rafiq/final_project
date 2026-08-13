#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

/*
 I got this code from this tutorial https://thelinuxcode.com/fork-exec-coding-c/
*/
int main() {

  printf("Original program PID: %d\n", getpid());  

  char *args[] = {"./process2", NULL};
  execvp(args[0], args);

  printf("This line does not run!\n");

  return 0;
}