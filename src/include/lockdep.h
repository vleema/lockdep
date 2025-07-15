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

// Node representing a lock in the lock dependency graph.
typedef struct lock_node {
    const void* lock_addr;                 // Address of the lock.
    char lock_name[LOCKDEP_MAX_LOCK_NAME]; // Symbolic name for the lock.
    bool was_visited;                      // Flag to indicate if the lock was visited during DFS.
    adjacency_locks_t* children;           // List of adjacent (child) locks.
    struct lock_node* next;                // Next lock node in the list.
    held_lock_t* current_held_lock;        // Reference to current held_lock_t if owned.
} lock_node_t;

// Represents an adjacency (edge) in the lock dependency graph.
typedef struct adjacent_locks {
    lock_node_t* lock;           // Pointer to the adjacent lock node.
    struct adjacent_locks* next; // Next adjacency in the list.
} adjacency_locks_t;

// Represents a lock currently held by a thread.
typedef struct held_lock {
    lock_node_t* lock;                                // Pointer to the held lock node.
    struct timespec acquired_at;                      // When this lock was acquired by this thread.
    void* acquisition_stack[LOCKDEP_MAX_STACK_DEPTH]; // Stack trace where acquired.
    int stack_depth;                                  // Number of valid stack frames.
    thread_context_t* thread_context;                 // Reference to the thread context holding this lock.
    struct held_lock* next;                           // Next held lock in the list.
} held_lock_t;

// Context information for a thread, including held locks.
typedef struct thread_context {
    pthread_t pthread_id;        // pthread identifier (for internal tracking).
    pid_t thread_id;             // Kernel thread ID (for user-friendly display).
    held_lock_t* held_locks;     // List of locks currently held by the thread.
    char thread_name[32];        // thread name for debugging.
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
