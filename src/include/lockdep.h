// ARCHITECTURE OVERVIEW:
//
// 1. LOCK GRAPH: All locks should be tracked as nodes in a directed graph where
//    edges represent ordering dependencies (A → B means A acquired before B).
//
// 2. THREAD TRACKING: Each thread should maintain a stack of currently held
//    locks to detect nested locking patterns and build dependencies.
//
// 3. DEADLOCK DETECTION: The system checks for cycles in the lock graph
//    to detect potential deadlocks. If a cycle is found, the system should
//    identify the lock and prevent the acquisition that would lead to a
//    deadlock.

#ifndef LOCKDEP_H
#define LOCKDEP_H

#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

#define LOCKDEP_MAX_STACK_DEPTH 8
#define LOCKDEP_MAX_LOCK_NAME 64

typedef struct adjacent_locks adjacency_locks_t;
typedef struct held_lock held_lock_t;
typedef struct thread_context thread_context_t;


typedef struct lock_node {
    const void* lock_addr;
    char lock_name[LOCKDEP_MAX_LOCK_NAME];
    bool was_visited;
    adjacency_locks_t* children;
    struct lock_node* next;
    held_lock_t* current_held_lock;
} lock_node_t;


typedef struct adjacent_locks {
    lock_node_t* lock;
    struct adjacent_locks* next;
} adjacency_locks_t;


typedef struct held_lock {
    lock_node_t* lock;
    struct timespec acquired_at;
    void* acquisition_stack[LOCKDEP_MAX_STACK_DEPTH];
    int stack_depth;
    thread_context_t* thread_context;
    struct held_lock* next;
} held_lock_t;


typedef struct thread_context {
    pthread_t pthread_id;
    pid_t thread_id;
    held_lock_t* held_locks;
    char thread_name[32];
    struct thread_context* next;
} thread_context_t;

void lockdep_init(void);

// Register the acquisition of a lock by the current thread. `lock_addr` is the
// address of the lock being acquired. Returns true if acquisition is allowed,
// false if it would cause a deadlock.
bool lockdep_acquire_lock(const void* lock_addr);

// Register the release of a lock by the current thread. `lock_addr` is the
// lock being released.
void lockdep_release_lock(const void* lock_addr);

// For disabling lockdep without recompilation.
extern bool lockdep_enabled;

#endif // LOCKDEP_H!
