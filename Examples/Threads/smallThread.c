#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


void *worker(void *arg) {
    pthread_exit((void *)123);   // thread ends, "returns" 123
    // return (void *)123; -> the same
}

int main() {
    pthread_t t1;
    void *ret;

    pthread_create(&t1, NULL, worker, NULL);
    pthread_join(t1, &ret);      // blocks until worker calls pthread_exit

    printf("Worker returned: %ld\n", (long)ret);  // prints 123
    return 0;
}