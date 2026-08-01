#include <stdio.h>

/*
 I got this code from this tutorial https://thelinuxcode.com/fork-exec-coding-c/
*/
int main() {
  printf("New program PID: %d\n", getpid());

  return 0;
}