#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lockdep.h"

bool lockdep_enabled = true;

static lock_node_t* lock_registry;
static thread_context_t* thread_registry;
static pthread_mutex_t lockdep_mutex = PTHREAD_MUTEX_INITIALIZER;

static void* smalloc(const size_t bytes)
{
    void* ptr = malloc(bytes);
    if (!ptr) {
        fprintf(stderr, "out of memory, aborting\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static lock_node_t* find_or_create_lock(const void* lock_addr)
{
    lock_node_t* lock = lock_registry;

    while (lock) {
        if (lock->lock_addr == lock_addr) return lock;
        lock = lock->next;
    }
    lock = smalloc(sizeof(lock_node_t));
    lock->children = NULL;
    lock->lock_addr = lock_addr;
    lock->next = lock_registry;
    return lock_registry = lock;
}

static thread_context_t* find_thread_context(const pthread_t thread_id)
{
    thread_context_t* ctx = thread_registry;
    while (ctx) {
        if (ctx->thread_id == thread_id) return ctx;
        ctx = ctx->next;
    }
    return NULL;
}

static thread_context_t* add_lock_to_thread_context(thread_context_t* ctx, lock_node_t* lock)
{
    if (ctx) {
        held_lock_t* new_held = smalloc(sizeof(held_lock_t));
        new_held->lock = lock;
        new_held->next = ctx->held_locks;
        ctx->held_locks = new_held;
        return ctx;
    }
    ctx = smalloc(sizeof(thread_context_t));
    ctx->held_locks = smalloc(sizeof(held_lock_t));
    ctx->thread_id = pthread_self();
    ctx->held_locks->lock = lock;
    ctx->held_locks->next = NULL;
    ctx->next = thread_registry;
    thread_registry = ctx;
    return ctx;
}

static thread_context_t* release_lock_from_thread_context(thread_context_t* ctx, const void* lock_addr)
{
    held_lock_t** held = &ctx->held_locks;
    while (*held) {
        if ((*held)->lock->lock_addr == lock_addr) {
            held_lock_t* to_free = *held;
            *held = (*held)->next;
            free(to_free);
            continue;
        }
        held = &(*held)->next;
    }
    return ctx;
}

void lockdep_init(void)
{
    const char* env = getenv("LOCKDEP_DISABLE");
    if (env && strcmp(env, "1") == 0) {
        lockdep_enabled = false;
        return;
    }

    fprintf(stderr, "[LOCKDEP] Lockdep initialized\n");
}
//This function adds the adjacency between the lock that was just added to the thread context
//to the one that was added just before that one
void lockdep_add_adjacencies(lock_node_t* lock, const thread_context_t* ctx){
    adjacency_locks_t* tmp = lock->children;
    while(tmp){
        tmp=tmp->next;
    }
    //if thread context has only one mutex lock
    //there is nothing to add
    if(!ctx->held_locks->next){
        return;
    }
    tmp = smalloc(sizeof(adjacency_locks_t));
    tmp->lock = ctx->held_locks->next->lock;
    tmp->next = lock->children;
    lock->children = tmp;
    return;
}

bool lockdep_acquire_lock(const void* lock_addr)
{
    printf("[LOCKDEP] Acquiring lock  %p\n", lock_addr);

    pthread_mutex_lock(&lockdep_mutex);

    lock_node_t* lock = find_or_create_lock(lock_addr);
    const thread_context_t* ctx = add_lock_to_thread_context(find_thread_context(pthread_self()), lock);
    //here we check if there's a deadlock

    //QUESTION: What happens if the same thread is trying to lock a mutex that it has already locked?
    lockdep_add_adjacencies(lock, ctx);
    //TODO: Detect cycles
    // TODO: Remove this print when finishing adding the remaining functionalities
    held_lock_t* held = ctx->held_locks;
    printf("[LOCKDEP] Thread %lu currently holds locks:\n", ctx->thread_id);
    while (held) {
        printf("[LOCKDEP] - %p\n", held->lock->lock_addr);
        held = held->next;
    }
    pthread_mutex_unlock(&lockdep_mutex);

    return false;
}

void lockdep_release_lock(const void* lock_addr)
{
    printf("[LOCKDEP] Releasing lock %p\n", lock_addr);

    pthread_mutex_lock(&lockdep_mutex);

    const thread_context_t* ctx = release_lock_from_thread_context(find_thread_context(pthread_self()), lock_addr);

    // TODO: Remove this print when finishing adding the remaining functionalities
    held_lock_t* held = ctx->held_locks;
    printf("[LOCKDEP] Thread %lu currently holds locks:\n", ctx->thread_id);
    while (held) {
        printf("[LOCKDEP] - %p\n", held->lock->lock_addr);
        held = held->next;
    }

    pthread_mutex_unlock(&lockdep_mutex);
}
