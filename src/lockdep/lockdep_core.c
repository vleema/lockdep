#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/lockdep.h"

bool lockdep_enabled = true;

lock_node_t* list = NULL;

void add_lock(void* lock_addr) {
    lock_node_t* new = malloc(sizeof *new);

    if (!new) {
        perror("malloc");
        exit(1);
    }

    new->lock_addr = lock_addr;
    new->next = NULL;

    if (!list) {
        list = new;
        return;
    } else {
        lock_node_t* ptr = list;
        while (ptr->next != NULL) {
            ptr = ptr->next;
        }
        ptr->next = new;
    }
}

void remove_lock(void* lock_addr) {
    if (!list) return;

    lock_node_t* node;

    if (list->lock_addr == lock_addr) {
        node = list;
        list = node->next;
        free(node);
        return;
    }

    lock_node_t* ptr = list;
    while (ptr->next && ptr->next->lock_addr != lock_addr) {
        ptr = ptr->next;
    }

    if (!ptr->next) return;

    node = ptr->next;
    ptr->next = node->next;
    free(node);
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

    add_lock(lock_addr);
    // verificar se tem ciclo
    return true;
}

void lockdep_release_lock(void* lock_addr) {
    printf("[LOCKDEP] Releasing lock at %p\n", lock_addr);
    remove_lock(lock_addr);
}
