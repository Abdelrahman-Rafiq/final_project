#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int ready = 0;

void *waiter_thread(void *arg) {
    printf("Waiter: waiting for condition...\n");

    pthread_mutex_lock(&mutex);
    while (!ready) {
        // Atomically: unlocks mutex + sleeps until signaled.
        // On wakeup, it re-locks mutex before continuing.
        pthread_cond_wait(&cond, &mutex);
    }
    printf("Waiter: condition met! Continuing work.\n");
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void *signaler_thread(void *arg) {
    sleep(2);   // simulate doing some work before the condition becomes true
    printf("Signaler: setting ready = 1 and signaling.\n");

    pthread_mutex_lock(&mutex);
    ready = 1;
    pthread_cond_signal(&cond);   // wake up waiter_thread
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main() {
    pthread_t t1, t2;

    pthread_create(&t1, NULL, waiter_thread, NULL);
    pthread_create(&t2, NULL, signaler_thread, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    return 0;
}