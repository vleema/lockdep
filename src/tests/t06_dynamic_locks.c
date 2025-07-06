#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * This test demonstrates deadlock scenarios with dynamically created locks
 *
 * We create locks dynamically in different patterns:
 * 1. An array of locks accessed in-order vs reverse-order
 * 2. Locks created and destroyed within threads
 * 3. Locks stored in a linked list structure
 *
 * The lockdep tool should track these dynamic locks just like static ones.
 */

// A linked list node that contains its own mutex
typedef struct node {
    pthread_mutex_t lock;
    int value;
    struct node* next;
} node_t;

typedef struct arg_t {
    int id;
} arg_t;

// Global variables
#define LOCK_COUNT 5
pthread_mutex_t* lock_array;
node_t* list_head = NULL;

// Create a linked list with 'count' nodes, each with its own mutex
void create_list(int count) {
    node_t* current = NULL;

    for (int i = 0; i < count; i++) {
        node_t* new_node = malloc(sizeof(node_t));
        if (!new_node) {
            perror("malloc failed");
            exit(1);
        }

        pthread_mutex_init(&new_node->lock, NULL);
        new_node->value = i;
        new_node->next = NULL;

        if (list_head == NULL) {
            list_head = new_node;
            current = list_head;
        } else {
            current->next = new_node;
            current = new_node;
        }
    }
}

// Free the linked list and destroy all mutexes
void destroy_list() {
    node_t* current = list_head;
    while (current != NULL) {
        node_t* next = current->next;
        pthread_mutex_destroy(&current->lock);
        free(current);
        current = next;
    }
    list_head = NULL;
}

// Thread that locks array elements in ascending order
void* ascending_thread(void* arg) {
    struct arg_t* args = (struct arg_t*) arg;

    printf("Thread %d: Locking array elements in ascending order\n", args->id);

    for (int i = 0; i < LOCK_COUNT; i++) {
        printf("Thread %d: Acquiring lock %d\n", args->id, i);
        pthread_mutex_lock(&lock_array[i]);
        usleep(10000); // Small delay to increase interleaving probability
    }

    printf("Thread %d: All locks acquired, now releasing\n", args->id);

    // Release locks in reverse order (LIFO)
    for (int i = LOCK_COUNT - 1; i >= 0; i--) {
        pthread_mutex_unlock(&lock_array[i]);
        printf("Thread %d: Released lock %d\n", args->id, i);
    }

    return NULL;
}

// Thread that locks array elements in descending order
void* descending_thread(void* arg) {
    struct arg_t* args = (struct arg_t*) arg;

    printf("Thread %d: Locking array elements in descending order\n", args->id);

    // Wait a bit to ensure Thread 1 has started acquiring locks
    usleep(30000);

    for (int i = LOCK_COUNT - 1; i >= 0; i--) {
        printf("Thread %d: Acquiring lock %d\n", args->id, i);
        pthread_mutex_lock(&lock_array[i]);
        usleep(10000); // Small delay to increase interleaving probability
    }

    printf("Thread %d: All locks acquired, now releasing\n", args->id);

    // Release locks in reverse order (LIFO)
    for (int i = 0; i < LOCK_COUNT; i++) {
        pthread_mutex_unlock(&lock_array[i]);
        printf("Thread %d: Released lock %d\n", args->id, i);
    }

    return NULL;
}

// Thread that creates its own locks dynamically
void* dynamic_lock_thread(void* arg) {
    int lock_count = *((int*)arg);
    printf("Thread 3: Creating %d dynamic locks\n", lock_count);

    // Create an array of locks
    pthread_mutex_t* locks = malloc(lock_count * sizeof(pthread_mutex_t));
    if (!locks) {
        perror("malloc failed");
        return NULL;
    }

    // Initialize all locks
    for (int i = 0; i < lock_count; i++) {
        pthread_mutex_init(&locks[i], NULL);
    }

    // Acquire them in order
    printf("Thread 3: Acquiring all locks\n");
    for (int i = 0; i < lock_count; i++) {
        pthread_mutex_lock(&locks[i]);
    }

    // Release them in same order (different from other threads)
    printf("Thread 3: Releasing all locks\n");
    for (int i = 0; i < lock_count; i++) {
        pthread_mutex_unlock(&locks[i]);
    }

    // Destroy and free
    for (int i = 0; i < lock_count; i++) {
        pthread_mutex_destroy(&locks[i]);
    }
    free(locks);

    printf("Thread 3: All dynamic locks destroyed\n");
    return NULL;
}

// Thread that traverses and locks nodes in a linked list
void* list_thread(void* arg) {
    int direction = *((int*)arg);  // 1 for forward, -1 for backward
    printf("Thread %d: Traversing list in %s order\n",
           direction == 1 ? 4 : 5,
           direction == 1 ? "forward" : "backward");

    if (direction == 1) {
        // Forward traversal
        node_t* current = list_head;
        while (current != NULL) {
            printf("Thread 4: Locking node with value %d\n", current->value);
            pthread_mutex_lock(&current->lock);
            usleep(50000); // Sleep to make deadlocks more likely
            current = current->next;
        }

        // Release all locks
        printf("Thread 4: Unlocking all nodes\n");
        current = list_head;
        while (current != NULL) {
            pthread_mutex_unlock(&current->lock);
            current = current->next;
        }
    } else {
        // Backward traversal requires building an array of nodes first
        int count = 0;
        node_t* current = list_head;
        while (current != NULL) {
            count++;
            current = current->next;
        }

        node_t** nodes = malloc(count * sizeof(node_t*));
        if (!nodes) {
            perror("malloc failed");
            return NULL;
        }

        // Fill array with nodes
        current = list_head;
        for (int i = 0; i < count; i++) {
            nodes[i] = current;
            current = current->next;
        }

        // Lock in reverse order
        for (int i = count - 1; i >= 0; i--) {
            printf("Thread 5: Locking node with value %d\n", nodes[i]->value);
            pthread_mutex_lock(&nodes[i]->lock);
            usleep(50000); // Sleep to make deadlocks more likely
        }

        // Unlock in same order
        printf("Thread 5: Unlocking all nodes\n");
        for (int i = count - 1; i >= 0; i--) {
            pthread_mutex_unlock(&nodes[i]->lock);
        }

        free(nodes);
    }

    return NULL;
}

int main() {
    pthread_t t1, t2, t3, t4, t5;
    int dynamic_lock_count = 3;
    int forward = 1, backward = -1;

    printf("Starting dynamic locks deadlock test\n");
    printf("This test demonstrates various deadlock scenarios with dynamic locks\n\n");

    // Initialize array of locks
    lock_array = malloc(LOCK_COUNT * sizeof(pthread_mutex_t));
    if (!lock_array) {
        perror("malloc failed");
        return 1;
    }

    for (int i = 0; i < LOCK_COUNT; i++) {
        pthread_mutex_init(&lock_array[i], NULL);
    }

    // Create linked list
    create_list(LOCK_COUNT);

    printf("1. Testing lock acquisition order with dynamic array locks\n");
    printf("   This should cause a deadlock if both threads run concurrently\n");

    // Uncomment to see the actual deadlock
    /*
    pthread_create(&t1, NULL, ascending_thread, NULL);
    pthread_create(&t2, NULL, descending_thread, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    */

    struct arg_t arg1 = {1};
    // Instead of actual deadlock, run them separately
    pthread_create(&t1, NULL, ascending_thread, (void *) &arg1);
    pthread_join(t1, NULL);

    struct arg_t arg2 = {2};
    pthread_create(&t2, NULL, descending_thread, (void *) &arg2);
    pthread_join(t2, NULL);

    printf("\n2. Testing dynamically created locks within a thread\n");
    pthread_create(&t3, NULL, dynamic_lock_thread, &dynamic_lock_count);
    pthread_join(t3, NULL);

    printf("\n3. Testing locks in linked list structure\n");

    // Again, run these separately to avoid actual deadlock
    pthread_create(&t4, NULL, list_thread, &forward);
    pthread_join(t4, NULL);

    pthread_create(&t5, NULL, list_thread, &backward);
    pthread_join(t5, NULL);

    // Cleanup
    destroy_list();

    for (int i = 0; i < LOCK_COUNT; i++) {
        pthread_mutex_destroy(&lock_array[i]);
    }
    free(lock_array);

    printf("\nTest completed\n");
    return 0;
}
