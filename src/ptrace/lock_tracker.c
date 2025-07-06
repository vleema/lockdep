/**
 * @file lock_tracker.c
 * @brief Implementation of the lock tracking system using ptrace
 *
 * This file implements the lock tracking system that monitors mutex operations
 * in a traced process, builds a lock dependency graph, and detects potential
 * deadlocks using the graph library.
 */

#include "lock_tracker.h"
#include "pthread_structures.h"
#include "../include/graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Print debugging information if verbose mode is enabled
static bool verbose = false;
#define verbose_print(fmt, ...) \
    do { if (verbose) fprintf(stderr, "[LOCKTRACKER] " fmt, ##__VA_ARGS__); } while (0)

// Global lock dependency graph
static graph_t* lock_graph = NULL;

// Structure to track locks held by a thread
typedef struct held_lock {
    void* lock_addr;             // Address of the lock
    struct held_lock* next;      // Next lock in the thread's held lock list
} held_lock_t;

// Structure to track thread information
typedef struct thread_info {
    pid_t thread_id;             // Thread ID
    held_lock_t* held_locks;     // List of locks held by this thread
    int lock_count;              // Number of locks currently held
    struct thread_info* next;    // Next thread in the thread list
} thread_info_t;

// Global list of tracked threads
static thread_info_t* threads = NULL;

// Helper function to format a pointer as a string for graph printing
static const char* format_ptr(void* ptr) {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "%p", ptr);
    return buffer;
}

// Helper function to find thread info
static thread_info_t* find_thread(pid_t thread_id) {
    thread_info_t* thread = threads;
    while (thread != NULL) {
        if (thread->thread_id == thread_id) {
            return thread;
        }
        thread = thread->next;
    }
    return NULL;
}

// Helper function to check if a thread holds a lock
static bool thread_holds_lock(thread_info_t* thread, void* lock_addr) {
    held_lock_t* lock = thread->held_locks;
    while (lock != NULL) {
        if (lock->lock_addr == lock_addr) {
            return true;
        }
        lock = lock->next;
    }
    return false;
}

bool lock_tracker_init(void) {
    // Create the lock graph
    lock_graph = graph_create();
    if (lock_graph == NULL) {
        fprintf(stderr, "Failed to create lock graph\n");
        return false;
    }
    
    verbose_print("Lock tracker initialized\n");
    return true;
}

void lock_tracker_cleanup(void) {
    // Clean up the graph
    if (lock_graph != NULL) {
        graph_destroy(lock_graph);
        lock_graph = NULL;
    }
    
    // Clean up thread information
    thread_info_t* thread = threads;
    while (thread != NULL) {
        thread_info_t* next_thread = thread->next;
        
        // Free held locks
        held_lock_t* lock = thread->held_locks;
        while (lock != NULL) {
            held_lock_t* next_lock = lock->next;
            free(lock);
            lock = next_lock;
        }
        
        free(thread);
        thread = next_thread;
    }
    
    threads = NULL;
    
    verbose_print("Lock tracker cleaned up\n");
}

bool lock_tracker_register_thread(pid_t thread_id) {
    // Check if the thread is already registered
    if (find_thread(thread_id) != NULL) {
        verbose_print("Thread %d already registered\n", thread_id);
        return true;
    }
    
    // Create a new thread info structure
    thread_info_t* thread = malloc(sizeof(thread_info_t));
    if (thread == NULL) {
        fprintf(stderr, "Failed to allocate memory for thread info\n");
        return false;
    }
    
    thread->thread_id = thread_id;
    thread->held_locks = NULL;
    thread->lock_count = 0;
    thread->next = threads;
    threads = thread;
    
    verbose_print("Registered thread %d\n", thread_id);
    return true;
}

void lock_tracker_unregister_thread(pid_t thread_id) {
    thread_info_t** thread_ptr = &threads;
    thread_info_t* thread;
    
    // Find the thread in our list
    while (*thread_ptr != NULL) {
        thread = *thread_ptr;
        
        if (thread->thread_id == thread_id) {
            // Remove the thread from the list
            *thread_ptr = thread->next;
            
            // Free held locks
            held_lock_t* lock = thread->held_locks;
            while (lock != NULL) {
                held_lock_t* next_lock = lock->next;
                free(lock);
                lock = next_lock;
            }
            
            free(thread);
            verbose_print("Unregistered thread %d\n", thread_id);
            return;
        }
        
        thread_ptr = &(thread->next);
    }
    
    verbose_print("Thread %d not found for unregistration\n", thread_id);
}

bool lock_tracker_register_acquisition(pid_t thread_id, void* lock_addr, bool is_recursive) {
    // Find or create the thread info
    thread_info_t* thread = find_thread(thread_id);
    if (thread == NULL) {
        if (!lock_tracker_register_thread(thread_id)) {
            return false;
        }
        thread = find_thread(thread_id);
    }
    
    // If this is a recursive acquisition, just return success
    if (is_recursive && thread_holds_lock(thread, lock_addr)) {
        verbose_print("Thread %d recursively acquiring lock %p\n", thread_id, lock_addr);
        return true;
    }
    
    // Find or create the lock node
    graph_node_t* lock_node = graph_find_or_create_node(lock_graph, lock_addr);
    if (lock_node == NULL) {
        fprintf(stderr, "Failed to create node for lock %p\n", lock_addr);
        return false;
    }
    
    // Check if we need to add dependencies based on currently held locks
    bool potential_deadlock = false;
    
    if (thread->lock_count > 0) {
        // For each lock already held by this thread
        held_lock_t* held_lock = thread->held_locks;
        while (held_lock != NULL) {
            graph_node_t* held_node = graph_find_or_create_node(lock_graph, held_lock->lock_addr);
            
            // Check if adding this dependency would create a cycle
            if (graph_would_create_cycle(lock_graph, held_node, lock_node)) {
                fprintf(stderr, "WARNING: Lock order violation detected!\n");
                fprintf(stderr, "Thread %d attempting to acquire lock %p while holding lock %p\n",
                        thread_id, lock_addr, held_lock->lock_addr);
                fprintf(stderr, "This violates a previously established lock ordering and may lead to deadlock.\n");
                
                potential_deadlock = true;
                // We don't return immediately to ensure we check all held locks
            } else {
                // Add the dependency: held_lock -> lock_addr
                graph_add_edge(lock_graph, held_node, lock_node);
                verbose_print("Added dependency: %p -> %p\n", held_lock->lock_addr, lock_addr);
            }
            
            held_lock = held_lock->next;
        }
    }
    
    // Add the lock to the thread's held locks
    held_lock_t* new_lock = malloc(sizeof(held_lock_t));
    if (new_lock == NULL) {
        fprintf(stderr, "Failed to allocate memory for held lock\n");
        return false;
    }
    
    new_lock->lock_addr = lock_addr;
    new_lock->next = thread->held_locks;
    thread->held_locks = new_lock;
    thread->lock_count++;
    
    verbose_print("Thread %d acquired lock %p (total: %d)\n", 
                  thread_id, lock_addr, thread->lock_count);
    
    return !potential_deadlock;
}

void lock_tracker_register_release(pid_t thread_id, void* lock_addr) {
    // Find the thread info
    thread_info_t* thread = find_thread(thread_id);
    if (thread == NULL) {
        verbose_print("Thread %d not found for lock release\n", thread_id);
        return;
    }
    
    // Find and remove the lock from the thread's held locks
    held_lock_t** lock_ptr = &(thread->held_locks);
    held_lock_t* lock;
    
    while (*lock_ptr != NULL) {
        lock = *lock_ptr;
        
        if (lock->lock_addr == lock_addr) {
            // Remove the lock from the list
            *lock_ptr = lock->next;
            free(lock);
            thread->lock_count--;
            
            verbose_print("Thread %d released lock %p (remaining: %d)\n", 
                         thread_id, lock_addr, thread->lock_count);
            return;
        }
        
        lock_ptr = &(lock->next);
    }
    
    verbose_print("Warning: Thread %d releasing lock %p that it doesn't hold\n",
                  thread_id, lock_addr);
}

bool lock_tracker_check_deadlocks(void) {
    if (lock_graph == NULL) {
        return false;
    }
    
    bool has_cycle = graph_has_cycle(lock_graph);
    
    if (has_cycle) {
        fprintf(stderr, "DEADLOCK DETECTED: Circular lock dependencies found!\n");
        lock_tracker_print_graph();
        lock_tracker_print_thread_locks();
    }
    
    return has_cycle;
}

void lock_tracker_print_graph(void) {
    if (lock_graph == NULL) {
        printf("Lock graph not initialized\n");
        return;
    }
    
    printf("\n=== Lock Dependency Graph ===\n");
    graph_print(lock_graph, format_ptr);
    printf("============================\n\n");
}

void lock_tracker_print_thread_locks(void) {
    printf("\n=== Thread Lock States ===\n");
    
    thread_info_t* thread = threads;
    while (thread != NULL) {
        printf("Thread %d holds %d locks: ", thread->thread_id, thread->lock_count);
        
        held_lock_t* lock = thread->held_locks;
        while (lock != NULL) {
            printf("%p ", lock->lock_addr);
            lock = lock->next;
        }
        printf("\n");
        
        thread = thread->next;
    }
    
    printf("========================\n\n");
}

size_t lock_tracker_get_lock_count(void) {
    if (lock_graph == NULL) {
        return 0;
    }
    
    return graph_node_count(lock_graph);
}

size_t lock_tracker_get_thread_count(void) {
    size_t count = 0;
    thread_info_t* thread = threads;
    
    while (thread != NULL) {
        count++;
        thread = thread->next;
    }
    
    return count;
}

void lock_tracker_set_verbose(bool enable) {
    verbose = enable;
}