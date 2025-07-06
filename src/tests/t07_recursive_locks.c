#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * This test demonstrates recursive locks which allow the same thread
 * to lock a mutex multiple times without causing deadlock
 *
 * We show:
 * 1. A regular mutex that will deadlock when locked recursively
 * 2. A recursive mutex that can be locked multiple times by the same thread
 */

pthread_mutex_t regular_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize a recursive mutex
pthread_mutex_t recursive_mutex;
pthread_mutexattr_t recursive_attr;

// Counter to track recursive lock depth
int recursive_lock_count = 0;

// A recursive function that attempts to lock the same mutex recursively
void recursive_lock(pthread_mutex_t* mutex, const char* mutex_type, int depth, int max_depth) {
    if (depth >= max_depth) {
        printf("[%s] Max depth %d reached, returning\n", mutex_type, max_depth);
        return;
    }

    printf("[%s] Attempting to acquire lock recursively (depth %d)\n", mutex_type, depth);
    int result = pthread_mutex_lock(mutex);

    if (result == 0) {
        printf("[%s] Successfully acquired lock at depth %d\n", mutex_type, depth);
        recursive_lock_count++;

        // Sleep briefly to make output readable
        usleep(100000);

        // Recursive call to lock the mutex again
        recursive_lock(mutex, mutex_type, depth + 1, max_depth);

        printf("[%s] Unlocking at depth %d\n", mutex_type, depth);
        pthread_mutex_unlock(mutex);
    } else {
        printf("[%s] Failed to acquire lock at depth %d: error %d\n",
               mutex_type, depth, result);
    }
}

void* regular_mutex_thread(void* arg) {
    int max_depth = *((int*)arg);
    printf("Starting regular mutex test (will deadlock)...\n");

    // This will deadlock after the first recursion
    recursive_lock(&regular_mutex, "Regular", 0, max_depth);

    return NULL;
}

void* recursive_mutex_thread(void* arg) {
    int max_depth = *((int*)arg);
    printf("Starting recursive mutex test...\n");

    // This should succeed for all recursive calls
    recursive_lock(&recursive_mutex, "Recursive", 0, max_depth);

    printf("Recursive mutex test completed with %d successful recursive locks\n",
           recursive_lock_count);

    return NULL;
}

int main() {
    pthread_t regular_thread, recursive_thread;
    int regular_depth = 2;  // Will attempt to lock twice (causing deadlock)
    int recursive_depth = 5; // Will attempt to lock 5 times

    printf("Recursive Lock Test\n");
    printf("===================\n");

    // Initialize recursive mutex
    pthread_mutexattr_init(&recursive_attr);
    pthread_mutexattr_settype(&recursive_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&recursive_mutex, &recursive_attr);

    printf("First testing recursive mutex (should succeed):\n");
    pthread_create(&recursive_thread, NULL, recursive_mutex_thread, &recursive_depth);
    pthread_join(recursive_thread, NULL);

    printf("\nNow testing regular mutex (will deadlock):\n");
    printf("NOTE: If lockdep is working, it should detect this potential deadlock\n");
    printf("Otherwise, this program will hang indefinitely\n\n");

    // Uncomment to actually test the deadlocking behavior
    pthread_create(&regular_thread, NULL, regular_mutex_thread, (void *) &regular_depth);
    pthread_join(regular_thread, NULL);

    // We don't run the regular mutex test by default because it will deadlock
    // Instead, we simulate it:
    printf("Simulating regular mutex deadlock (not actually running it):\n");
    printf("[Regular] Attempting to acquire lock recursively (depth 0)\n");
    printf("[Regular] Successfully acquired lock at depth 0\n");
    printf("[Regular] Attempting to acquire lock recursively (depth 1)\n");
    printf("... program would hang here due to deadlock ...\n");

    // Clean up
    pthread_mutexattr_destroy(&recursive_attr);
    pthread_mutex_destroy(&recursive_mutex);

    printf("\nTest completed\n");
    return 0;
}
