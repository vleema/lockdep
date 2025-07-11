#include <execinfo.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/lockdep.h"

bool lockdep_enabled = true;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static lock_node_t* lock_registry = NULL;

static void* smalloc(size_t bytes) {
    void* v = malloc(bytes);
    if (v == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(EXIT_FAILURE);
    }
    return v;
}

static lock_node_t* find_or_create_lock(void* lock_addr) {
    lock_node_t* ptr = lock_registry;

    while (ptr && ptr->lock_addr != lock_addr) ptr = ptr->next;

    if (ptr) return ptr;

    lock_node_t* new = smalloc(sizeof *new);

    new->lock_addr = lock_addr;
    new->next = lock_registry;

    return lock_registry = new;
}

void lockdep_init(void) {
    const char* env = getenv("LOCKDEP_DISABLE");
    if (env && strcmp(env, "1") == 0) {
        lockdep_enabled = false;
        return;
    }

    fprintf(stderr, "[LOCKDEP] Lockdep initialized\n");
}

void lockdep_cleanup(void) {
    // TODO: Implement if needed
    return;
}

bool lockdep_acquire_lock(void* lock_addr) {
    printf("[LOCKDEP] Acquiring lock at %p\n", lock_addr);

    pthread_mutex_lock(&mutex);
    find_or_create_lock(lock_addr);
    pthread_mutex_unlock(&mutex);
    // verificar se tem ciclo
    return true;
}

void lockdep_release_lock(void* lock_addr) {
    printf("[LOCKDEP] Releasing lock at %p\n", lock_addr);
}
