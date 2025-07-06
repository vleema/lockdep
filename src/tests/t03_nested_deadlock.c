#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * This test demonstrates a nested lock deadlock scenario
 * where deadlock occurs due to the interleaving of nested lock acquisitions
 * 
 * Thread 1: Acquires mutex1, then mutex2, then tries to acquire mutex3
 * Thread 2: Acquires mutex3, then mutex1
 * 
 * The deadlock occurs because:
 * 1. Thread 1 holds mutex1 and mutex2, and waits for mutex3
 * 2. Thread 2 holds mutex3 and waits for mutex1 (held by Thread 1)
 */

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;

void* thread1_func(void* arg __attribute__((unused))) {
    printf("Thread 1: Acquiring mutex1\n");
    pthread_mutex_lock(&mutex1);
    printf("Thread 1: Got mutex1, sleeping briefly...\n");
    usleep(10000); // Sleep for 10ms to allow for predictable interleaving

    printf("Thread 1: Acquiring mutex2 (nested)\n");
    pthread_mutex_lock(&mutex2);
    printf("Thread 1: Got mutex2, sleeping...\n");
    sleep(1); // Sleep to ensure Thread 2 has time to acquire mutex3

    printf("Thread 1: Trying to acquire mutex3\n");
    pthread_mutex_lock(&mutex3); // This will block if Thread 2 has mutex3
    printf("Thread 1: Got mutex3\n");

    // Cleanup (this code won't execute if deadlock occurs)
    pthread_mutex_unlock(&mutex3);
    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);
    printf("Thread 1: Released all locks\n");

    return NULL;
}

void* thread2_func(void* arg __attribute__((unused))) {
    printf("Thread 2: Sleeping briefly to let Thread 1 acquire mutex1...\n");
    usleep(50000); // Sleep for 50ms to ensure Thread 1 gets mutex1 first

    printf("Thread 2: Acquiring mutex3\n");
    pthread_mutex_lock(&mutex3);
    printf("Thread 2: Got mutex3, sleeping briefly...\n");
    usleep(10000); // Short sleep

    printf("Thread 2: Trying to acquire mutex1\n");
    pthread_mutex_lock(&mutex1); // This will block because Thread 1 has mutex1
    printf("Thread 2: Got mutex1\n");

    // Cleanup (this code won't execute if deadlock occurs)
    pthread_mutex_unlock(&mutex1);
    pthread_mutex_unlock(&mutex3);
    printf("Thread 2: Released all locks\n");

    return NULL;
}

int main() {
    pthread_t t1, t2;

    printf("Starting nested lock deadlock test\n");
    printf("This test demonstrates a deadlock scenario with nested locks\n");

    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);

    // In a real deadlock scenario, these joins will hang
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Test completed (if you see this, deadlock was prevented)\n");
    return 0;
}