#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * This test demonstrates how pthread_mutex_trylock can be used to avoid deadlocks
 * 
 * Thread 1: Acquires mutex1, then tries to acquire mutex2 using trylock
 * Thread 2: Acquires mutex2, then tries to acquire mutex1 using trylock
 * 
 * When trylock fails, the thread releases any held locks and retries after a delay,
 * thus preventing the classic AB-BA deadlock pattern.
 */

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

// Shared resources protected by the mutexes
int resource1 = 0;
int resource2 = 0;

void* thread1_func(void* arg __attribute__((unused))) {
    int retry_count = 0;
    const int max_retries = 5;
    
    while (retry_count < max_retries) {
        printf("Thread 1: Acquiring mutex1\n");
        pthread_mutex_lock(&mutex1);
        printf("Thread 1: Got mutex1, sleeping briefly...\n");
        usleep(100000); // Sleep for 100ms to increase chance of contention
        
        printf("Thread 1: Trying to acquire mutex2 (trylock)\n");
        if (pthread_mutex_trylock(&mutex2) == 0) {
            // Success! We got both locks
            printf("Thread 1: Successfully acquired both locks\n");
            
            // Critical section - update protected resources
            resource1 += 10;
            resource2 += 20;
            printf("Thread 1: Updated resources: %d, %d\n", resource1, resource2);
            
            // Release locks in reverse order of acquisition
            pthread_mutex_unlock(&mutex2);
            pthread_mutex_unlock(&mutex1);
            printf("Thread 1: Released all locks\n");
            return NULL;
        } else {
            // trylock failed, release mutex1 to prevent deadlock
            printf("Thread 1: Could not get mutex2, releasing mutex1 and retrying...\n");
            pthread_mutex_unlock(&mutex1);
            
            // Wait a random amount of time before retrying to reduce contention
            usleep((rand() % 500000) + 100000); // Sleep for 100-600ms
            retry_count++;
        }
    }
    
    printf("Thread 1: Gave up after %d retries\n", max_retries);
    return NULL;
}

void* thread2_func(void* arg __attribute__((unused))) {
    int retry_count = 0;
    const int max_retries = 5;
    
    while (retry_count < max_retries) {
        printf("Thread 2: Acquiring mutex2\n");
        pthread_mutex_lock(&mutex2);
        printf("Thread 2: Got mutex2, sleeping briefly...\n");
        usleep(100000); // Sleep for 100ms to increase chance of contention
        
        printf("Thread 2: Trying to acquire mutex1 (trylock)\n");
        if (pthread_mutex_trylock(&mutex1) == 0) {
            // Success! We got both locks
            printf("Thread 2: Successfully acquired both locks\n");
            
            // Critical section - update protected resources
            resource1 += 1;
            resource2 += 2;
            printf("Thread 2: Updated resources: %d, %d\n", resource1, resource2);
            
            // Release locks in reverse order of acquisition
            pthread_mutex_unlock(&mutex1);
            pthread_mutex_unlock(&mutex2);
            printf("Thread 2: Released all locks\n");
            return NULL;
        } else {
            // trylock failed, release mutex2 to prevent deadlock
            printf("Thread 2: Could not get mutex1, releasing mutex2 and retrying...\n");
            pthread_mutex_unlock(&mutex2);
            
            // Wait a random amount of time before retrying to reduce contention
            usleep((rand() % 500000) + 100000); // Sleep for 100-600ms
            retry_count++;
        }
    }
    
    printf("Thread 2: Gave up after %d retries\n", max_retries);
    return NULL;
}

int main() {
    pthread_t t1, t2;

    // Initialize random number generator
    srand(time(NULL));

    printf("Starting trylock deadlock avoidance test\n");
    printf("This test demonstrates how trylock can be used to avoid deadlocks\n");
    printf("Initial resource values: %d, %d\n", resource1, resource2);

    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Test completed\n");
    printf("Final resource values: %d, %d\n", resource1, resource2);
    return 0;
}