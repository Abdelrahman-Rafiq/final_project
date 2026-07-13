#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 5
#define INCREMENTS_PER_THREAD 100000

long shared_counter = 0;
pthread_mutex_t lock;   // the mutex that protects shared_counter

void *increment(void *arg) {
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        pthread_mutex_lock(&lock);     // enter critical section
        shared_counter++;              // only one thread can be here at a time
        pthread_mutex_unlock(&lock);   // leave critical section
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    pthread_mutex_init(&lock, NULL);   // initialize the mutex

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, increment, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);   // wait for all threads to finish

    pthread_mutex_destroy(&lock);      // clean up the mutex

    printf("Final counter value: %ld\n", shared_counter);
    printf("Expected value: %d\n", NUM_THREADS * INCREMENTS_PER_THREAD);

    return 0;
}