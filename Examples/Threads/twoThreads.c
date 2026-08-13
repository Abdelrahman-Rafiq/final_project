#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/*
The source of this code is : https://embeddedprep.com/posix-threads-pthread/
*/

void *print_message_function(void *ptr);

int main() {
    pthread_t thread1, thread2;
    char *message1 = "Thread 1";
    char *message2 = "Thread 2";
    int iret1, iret2;

    // Create two threads running the same function but with different arguments
    iret1 = pthread_create(&thread1, NULL, print_message_function, (void*) message1);
    iret2 = pthread_create(&thread2, NULL, print_message_function, (void*) message2);

    // Wait for both threads to finish before continuing
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    printf("Thread 1 returns: %d\n", iret1);
    printf("Thread 2 returns: %d\n", iret2);

    return 0;
}

void *print_message_function(void *ptr) {
    char *message = (char *) ptr;
    printf("%s \n", message);
    return NULL;
}