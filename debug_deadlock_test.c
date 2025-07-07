#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

void* thread1_func(void* arg __attribute__((unused))) {
    printf("Thread 1: Starting\n");
    
    printf("Thread 1: Acquiring mutex1\n");
    pthread_mutex_lock(&mutex1);
    printf("Thread 1: Got mutex1\n");
    
    // Give thread 2 time to acquire mutex2
    sleep(1);
    
    printf("Thread 1: Trying to acquire mutex2 (this should trigger lockdep)\n");
    int result = pthread_mutex_lock(&mutex2);
    
    if (result == 0) {
        printf("Thread 1: Got mutex2 - this means lockdep didn't prevent deadlock!\n");
        pthread_mutex_unlock(&mutex2);
    } else {
        printf("Thread 1: Failed to get mutex2 (error: %d) - lockdep working!\n", result);
    }
    
    pthread_mutex_unlock(&mutex1);
    printf("Thread 1: Finished\n");
    return NULL;
}

void* thread2_func(void* arg __attribute__((unused))) {
    printf("Thread 2: Starting\n");
    
    printf("Thread 2: Acquiring mutex2\n");
    pthread_mutex_lock(&mutex2);
    printf("Thread 2: Got mutex2\n");
    
    // Give thread 1 time to acquire mutex1
    sleep(1);
    
    printf("Thread 2: Trying to acquire mutex1 (this should trigger lockdep)\n");
    int result = pthread_mutex_lock(&mutex1);
    
    if (result == 0) {
        printf("Thread 2: Got mutex1 - this means lockdep didn't prevent deadlock!\n");
        pthread_mutex_unlock(&mutex1);
    } else {
        printf("Thread 2: Failed to get mutex1 (error: %d) - lockdep working!\n", result);
    }
    
    pthread_mutex_unlock(&mutex2);
    printf("Thread 2: Finished\n");
    return NULL;
}

int main() {
    pthread_t t1, t2;
    
    printf("=== Controlled Deadlock Test ===\n");
    printf("This test will create a classic AB-BA deadlock scenario\n");
    printf("Lockdep should detect and prevent the deadlock\n\n");
    
    // Create threads
    printf("Creating threads...\n");
    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);
    
    // Wait with timeout to avoid hanging
    printf("Waiting for threads to complete...\n");
    
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 5; // 5 second timeout
    
    void* result1 = NULL;
    void* result2 = NULL;
    
    // Try to join with timeout (not standard, but let's see what happens)
    int join_result1 = pthread_join(t1, &result1);
    int join_result2 = pthread_join(t2, &result2);
    
    if (join_result1 == 0 && join_result2 == 0) {
        printf("Both threads completed successfully\n");
    } else {
        printf("Thread join failed - possible deadlock not prevented\n");
    }
    
    printf("=== Test Complete ===\n");
    return 0;
}