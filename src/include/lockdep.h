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

typedef struct adjacent_locks adjacency_locks_t;

// Node representing a lock in the lock dependency graph.
typedef struct lock_node {
    const void* lock_addr;       // Address of the lock.
    adjacency_locks_t* children; // List of adjacent (child) locks.
    bool lock_was_visited;       //Boolean used for the cycle detection
    struct lock_node* next;      // Next lock node in the list.
} lock_node_t;

// Represents an adjacency (edge) in the lock dependency graph.
typedef struct adjacent_locks {
    lock_node_t* lock;           // Pointer to the adjacent lock node.
    struct adjacent_locks* next; // Next adjacency in the list.
} adjacency_locks_t;

// Represents a lock currently held by a thread.
typedef struct held_lock {
    lock_node_t* lock;      // Pointer to the held lock node.
    struct held_lock* next; // Next held lock in the list.
} held_lock_t;

// Context information for a thread, including held locks.
typedef struct thread_context {
    pthread_t thread_id;         // Thread identifier.
    held_lock_t* held_locks;     // List of locks currently held by the thread.
    struct thread_context* next; // Next thread context in the list.
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
