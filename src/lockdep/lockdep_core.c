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
    lock->was_visited = false;
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

static void add_dependency_to_graph(const thread_context_t* ctx, lock_node_t* lock)
{
    held_lock_t* held = ctx->held_locks->next;
    if (held) {
        adjacency_locks_t* adj = held->lock->children;
        bool already_depends = false;
        while (adj) {
            if (adj->lock->lock_addr == lock->lock_addr) {
                already_depends = true;
                break;
            }
            adj = adj->next;
        }
        if (!already_depends) {
            adjacency_locks_t* new_adj = smalloc(sizeof(adjacency_locks_t));
            new_adj->lock = lock;
            new_adj->next = held->lock->children;
            held->lock->children = new_adj;
        }
    }
}

static void unvisit_locks()
{
    lock_node_t* l = lock_registry;
    while (l) {
        l->was_visited = false;
        l = l->next;
    }
}

static bool has_cycle(const lock_node_t* l)
{
    adjacency_locks_t* to_visit = l->children;
    while (to_visit) {
        // There is a node to visit.
        if (!to_visit->lock->was_visited) {
            to_visit->lock->was_visited = true;
            const bool returnval = has_cycle(to_visit->lock);
            to_visit->lock->was_visited = false;
            return returnval;
        }
        // Tried to visit a node that had already been visited
        // cycle detected.
        else {
            return true;
        }
        to_visit = to_visit->next;
    }
    // Reached the end without finding a cycle.
    return false;
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

bool lockdep_acquire_lock(const void* lock_addr)
{
    printf("[LOCKDEP] Acquiring lock  %p\n", lock_addr);

    pthread_mutex_lock(&lockdep_mutex);

    lock_node_t* lock = find_or_create_lock(lock_addr);
    add_dependency_to_graph(add_lock_to_thread_context(find_thread_context(pthread_self()), lock), lock);
    const bool deadlock = has_cycle(lock);
    deadlock ? 42 : unvisit_locks();

    pthread_mutex_unlock(&lockdep_mutex);

    return !deadlock;
}

void lockdep_release_lock(const void* lock_addr)
{
    printf("[LOCKDEP] Releasing lock %p\n", lock_addr);

    pthread_mutex_lock(&lockdep_mutex);

    release_lock_from_thread_context(find_thread_context(pthread_self()), lock_addr);

    pthread_mutex_unlock(&lockdep_mutex);
}
