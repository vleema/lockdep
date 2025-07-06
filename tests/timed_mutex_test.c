#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/*
 * This test demonstrates the use of timed mutex operations (pthread_mutex_timedlock)
 * to avoid indefinite blocking during deadlock scenarios
 * 
 * Thread 1: Acquires mutex1, then tries to acquire mutex2 with a timeout
 * Thread 2: Acquires mutex2, then tries to acquire mutex1 with a timeout
 * 
 * Without timeouts, this would be a classic AB-BA deadlock pattern.
 * With timeouts, the threads can detect potential deadlock and take corrective action.
 */

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

// Helper function to create an absolute timeout
struct timespec get_deadline(int seconds_from_now) {
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += seconds_from_now;
    return deadline;
}

void* thread1_func(void* arg __attribute__((unused))) {
    int retry_count = 0;
    const int max_retries = 3;
    const int timeout_seconds = 2;
    
    printf("Thread 1: Starting\n");
    
    while (retry_count < max_retries) {
        printf("Thread 1: Acquiring mutex1\n");
        pthread_mutex_lock(&mutex1);
        printf("Thread 1: Got mutex1, sleeping...\n");
        
        sleep(1); // Sleep to ensure Thread 2 has time to acquire mutex2
        
        printf("Thread 1: Trying to acquire mutex2 with %d second timeout\n", 
               timeout_seconds);
        
        struct timespec deadline = get_deadline(timeout_seconds);
        int result = pthread_mutex_timedlock(&mutex2, &deadline);
        
        if (result == 0) {
            // Successfully got both locks
            printf("Thread 1: Successfully acquired mutex2\n");
            
            // Critical section
            printf("Thread 1: Executing critical section with both locks\n");
            sleep(1);
            
            // Release locks in reverse order
            printf("Thread 1: Releasing mutex2\n");
            pthread_mutex_unlock(&mutex2);
            printf("Thread 1: Releasing mutex1\n");
            pthread_mutex_unlock(&mutex1);
            
            printf("Thread 1: Successfully completed operation\n");
            return NULL;
        } else if (result == ETIMEDOUT) {
            // Timed out waiting for mutex2 - potential deadlock
            printf("Thread 1: Timed out waiting for mutex2, releasing mutex1 to avoid deadlock\n");
            pthread_mutex_unlock(&mutex1);
            
            // Backoff before retry
            int backoff_time = (rand() % 3) + 1; // 1-3 seconds
            printf("Thread 1: Backing off for %d seconds before retry\n", backoff_time);
            sleep(backoff_time);
            
            retry_count++;
        } else {
            // Some other error
            printf("Thread 1: Error acquiring mutex2: %d\n", result);
            pthread_mutex_unlock(&mutex1);
            return NULL;
        }
    }
    
    printf("Thread 1: Gave up after %d retries\n", max_retries);
    return NULL;
}

void* thread2_func(void* arg __attribute__((unused))) {
    int retry_count = 0;
    const int max_retries = 3;
    const int timeout_seconds = 2;
    
    printf("Thread 2: Starting\n");
    
    while (retry_count < max_retries) {
        printf("Thread 2: Acquiring mutex2\n");
        pthread_mutex_lock(&mutex2);
        printf("Thread 2: Got mutex2, sleeping...\n");
        
        sleep(1); // Sleep to ensure Thread 1 has time to acquire mutex1
        
        printf("Thread 2: Trying to acquire mutex1 with %d second timeout\n", 
               timeout_seconds);
        
        struct timespec deadline = get_deadline(timeout_seconds);
        int result = pthread_mutex_timedlock(&mutex1, &deadline);
        
        if (result == 0) {
            // Successfully got both locks
            printf("Thread 2: Successfully acquired mutex1\n");
            
            // Critical section
            printf("Thread 2: Executing critical section with both locks\n");
            sleep(1);
            
            // Release locks in reverse order
            printf("Thread 2: Releasing mutex1\n");
            pthread_mutex_unlock(&mutex1);
            printf("Thread 2: Releasing mutex2\n");
            pthread_mutex_unlock(&mutex2);
            
            printf("Thread 2: Successfully completed operation\n");
            return NULL;
        } else if (result == ETIMEDOUT) {
            // Timed out waiting for mutex1 - potential deadlock
            printf("Thread 2: Timed out waiting for mutex1, releasing mutex2 to avoid deadlock\n");
            pthread_mutex_unlock(&mutex2);
            
            // Backoff before retry
            int backoff_time = (rand() % 3) + 1; // 1-3 seconds
            printf("Thread 2: Backing off for %d seconds before retry\n", backoff_time);
            sleep(backoff_time);
            
            retry_count++;
        } else {
            // Some other error
            printf("Thread 2: Error acquiring mutex1: %d\n", result);
            pthread_mutex_unlock(&mutex2);
            return NULL;
        }
    }
    
    printf("Thread 2: Gave up after %d retries\n", max_retries);
    return NULL;
}

int main() {
    pthread_t t1, t2;
    
    printf("Timed Mutex Deadlock Avoidance Test\n");
    printf("===================================\n");
    printf("This test demonstrates how timed mutex operations can be used\n");
    printf("to detect and recover from potential deadlock situations.\n\n");
    
    // Initialize random number generator
    srand(time(NULL));
    
    // Create threads
    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);
    
    // Wait for threads to complete
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    
    printf("\nTest completed\n");
    printf("Note: The threads used timeouts to detect potential deadlock and\n");
    printf("released resources to avoid permanent deadlock. In a real application,\n");
    printf("this technique can prevent the system from becoming unresponsive.\n");
    
    return 0;
}