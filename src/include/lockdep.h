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
#include <stddef.h>
#include <stdint.h>

typedef struct thread_context thread_context_t;

// Forward declaration for graph node
struct graph_node;
typedef struct graph_node graph_node_t;

// Stack node representing a lock currently held by a thread.
// Enables tracking of nested lock acquisitions per thread.
typedef struct held_lock {
    // The lock being held (represented as a graph node)
    graph_node_t* lock;
    // Next lock in the stack (newest lock on top)
    struct held_lock* next;
} held_lock_t;

// Per-thread context for tracking locks held by each thread.
// Maintains a stack of held locks, the current stack depth, and links
// all thread contexts together for traversal.
typedef struct thread_context {
    pthread_t thread_id;
    // Stack of locks currently held by this thread
    held_lock_t* held_locks;
    // The size of the held locks stack
    int lock_depth;
    // Links all thread contexts together for easy traversal
    struct thread_context* next;
} thread_context_t;

void lockdep_init(void);
void lockdep_cleanup(void);

// Register the acquisition of a lock by the current thread. `lock_addr` is the
// address of the lock being acquired. Returns true if acquisition is allowed,
// false if it would cause a deadlock.
bool lockdep_acquire_lock(void* lock_addr);

// Register the release of a lock by the current thread. `lock_addr` is the
// lock being released.
void lockdep_release_lock(void* lock_addr);

// Check the lock graph for cycles (potential deadlocks).
// Returns true if a deadlock is detected, false otherwise.
bool lockdep_check_deadlock(void);

// Shows all A → B relationships in the lock graph.
void lockdep_print_dependencies(void);

// For disabling lockdep without recompilation.
extern bool lockdep_enabled;

#endif  // LOCKDEP_H!
