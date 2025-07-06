#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * This test demonstrates a circular deadlock involving three threads
 * 
 * Thread 1: Acquires mutex1, then tries to acquire mutex2
 * Thread 2: Acquires mutex2, then tries to acquire mutex3
 * Thread 3: Acquires mutex3, then tries to acquire mutex1
 * 
 * The deadlock occurs in a circular pattern:
 * Thread 1 waits for Thread 2
 * Thread 2 waits for Thread 3
 * Thread 3 waits for Thread 1
 */

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;

void* thread1_func(void* arg __attribute__((unused))) {
    printf("Thread 1: Acquiring mutex1\n");
    pthread_mutex_lock(&mutex1);
    printf("Thread 1: Got mutex1, sleeping...\n");
    
    sleep(1); // Sleep to ensure other threads acquire their first locks
    
    printf("Thread 1: Trying to acquire mutex2\n");
    pthread_mutex_lock(&mutex2); // Will block when Thread 2 has it
    printf("Thread 1: Got mutex2\n");
    
    // Cleanup (this code won't execute if deadlock occurs)
    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);
    printf("Thread 1: Released all locks\n");
    
    return NULL;
}

void* thread2_func(void* arg __attribute__((unused))) {
    printf("Thread 2: Acquiring mutex2\n");
    pthread_mutex_lock(&mutex2);
    printf("Thread 2: Got mutex2, sleeping...\n");
    
    sleep(1); // Sleep to ensure other threads acquire their first locks
    
    printf("Thread 2: Trying to acquire mutex3\n");
    pthread_mutex_lock(&mutex3); // Will block when Thread 3 has it
    printf("Thread 2: Got mutex3\n");
    
    // Cleanup (this code won't execute if deadlock occurs)
    pthread_mutex_unlock(&mutex3);
    pthread_mutex_unlock(&mutex2);
    printf("Thread 2: Released all locks\n");
    
    return NULL;
}

void* thread3_func(void* arg __attribute__((unused))) {
    printf("Thread 3: Acquiring mutex3\n");
    pthread_mutex_lock(&mutex3);
    printf("Thread 3: Got mutex3, sleeping...\n");
    
    sleep(1); // Sleep to ensure other threads acquire their first locks
    
    printf("Thread 3: Trying to acquire mutex1\n");
    pthread_mutex_lock(&mutex1); // Will block when Thread 1 has it
    printf("Thread 3: Got mutex1\n");
    
    // Cleanup (this code won't execute if deadlock occurs)
    pthread_mutex_unlock(&mutex1);
    pthread_mutex_unlock(&mutex3);
    printf("Thread 3: Released all locks\n");
    
    return NULL;
}

int main() {
    pthread_t t1, t2, t3;

    printf("Starting three-thread circular deadlock test\n");
    printf("This test demonstrates a circular wait deadlock with three threads\n");

    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);
    pthread_create(&t3, NULL, thread3_func, NULL);

    // In a real deadlock scenario, these joins will hang
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    printf("Test completed (if you see this, deadlock was prevented)\n");
    return 0;
}