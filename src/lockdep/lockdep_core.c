#include <execinfo.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "graph.h"
#include "lockdep.h"

bool lockdep_enabled = true;

// Global lock graph state
static graph_t* dependency_graph = NULL;
static thread_context_t* thread_contexts = NULL;

// Mutex to protect the lock graph's internal state
static pthread_mutex_t lockdep_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations for internal functions
static thread_context_t* get_thread_context(void);
static void print_backtrace(void);
static const char* format_ptr(void* ptr);

void lockdep_init(void) {
    const char* env = getenv("LOCKDEP_DISABLE");
    if (env && strcmp(env, "1") == 0) {
        lockdep_enabled = false;
        return;
    }

    // Initialize the dependency graph
    dependency_graph = graph_create();
    if (!dependency_graph) {
        fprintf(stderr, "[LOCKDEP] Failed to create dependency graph\n");
        exit(1);
    }

    fprintf(stderr, "[LOCKDEP] Lockdep initialized\n");
}

void lockdep_cleanup(void) {
    pthread_mutex_lock(&lockdep_mutex);
    
    // Destroy the dependency graph
    if (dependency_graph) {
        graph_destroy(dependency_graph);
        dependency_graph = NULL;
    }
    
    // Free thread contexts and their held locks
    thread_context_t* ctx = thread_contexts;
    while (ctx) {
        thread_context_t* next_ctx = ctx->next;
        
        // Free held locks stack
        held_lock_t* held = ctx->held_locks;
        while (held) {
            held_lock_t* next_held = held->next;
            free(held);
            held = next_held;
        }
        
        free(ctx);
        ctx = next_ctx;
    }
    
    thread_contexts = NULL;
    
    pthread_mutex_unlock(&lockdep_mutex);
}

bool lockdep_acquire_lock(void* lock_addr) {
    if (!lockdep_enabled || !dependency_graph) {
        return true;
    }
    
    pthread_mutex_lock(&lockdep_mutex);
    
    printf("[LOCKDEP] Thread %lu acquiring lock at %p\n", 
           (unsigned long)pthread_self(), lock_addr);
    
    // Get or create lock node for this lock address
    graph_node_t* lock_node = graph_find_or_create_node(dependency_graph, lock_addr);
    
    // Get thread context
    thread_context_t* thread_ctx = get_thread_context();
    
    // Check if we already have locks held and need to add dependencies
    if (thread_ctx->held_locks != NULL) {
        // The most recently acquired lock should have a dependency on this new lock
        graph_node_t* prev_lock_node = thread_ctx->held_locks->lock;
        
        // Add dependency: prev_lock -> lock_node
        if (graph_would_create_cycle(dependency_graph, prev_lock_node, lock_node)) {
            // Dependency would create a cycle - potential deadlock!
            void* prev_lock_addr = graph_node_get_id(prev_lock_node);
            fprintf(stderr, "[LOCKDEP] DEADLOCK PREVENTED: Lock order violation detected!\n");
            fprintf(stderr, "[LOCKDEP] Thread %lu attempting to acquire %p while holding %p\n",
                    (unsigned long)pthread_self(), lock_addr, prev_lock_addr);
            fprintf(stderr, "[LOCKDEP] This violates previously observed lock ordering and WILL lead to deadlocks.\n");
            print_backtrace();
            
            // Always prevent lock order violations - don't wait for actual cycles
            fprintf(stderr, "[LOCKDEP] PREVENTING LOCK ACQUISITION to avoid deadlock!\n");
            pthread_mutex_unlock(&lockdep_mutex);
            return false;
        } else {
            // Add the dependency to the graph
            graph_add_edge(dependency_graph, prev_lock_node, lock_node);
        }
    }
    
    // Push this lock onto the thread's held locks stack
    held_lock_t* new_held = malloc(sizeof(held_lock_t));
    if (!new_held) {
        perror("[LOCKDEP] Failed to allocate memory for held lock");
        pthread_mutex_unlock(&lockdep_mutex);
        return true; // Continue without tracking in case of allocation failure
    }
    
    new_held->lock = lock_node;
    new_held->next = thread_ctx->held_locks;
    thread_ctx->held_locks = new_held;
    thread_ctx->lock_depth++;
    
    pthread_mutex_unlock(&lockdep_mutex);
    return true;
}

void lockdep_release_lock(void* lock_addr) {
    if (!lockdep_enabled || !dependency_graph) {
        return;
    }
    
    pthread_mutex_lock(&lockdep_mutex);
    
    printf("[LOCKDEP] Thread %lu releasing lock at %p\n", 
           (unsigned long)pthread_self(), lock_addr);
    
    // Get thread context
    thread_context_t* thread_ctx = get_thread_context();
    
    // Find and remove the lock from the thread's held locks stack
    held_lock_t** curr = &thread_ctx->held_locks;
    while (*curr) {
        void* curr_lock_addr = graph_node_get_id((*curr)->lock);
        if (curr_lock_addr == lock_addr) {
            held_lock_t* to_free = *curr;
            *curr = (*curr)->next;
            free(to_free);
            thread_ctx->lock_depth--;
            break;
        }
        curr = &(*curr)->next;
    }
    
    pthread_mutex_unlock(&lockdep_mutex);
}

bool lockdep_check_deadlock(void) {
    if (!lockdep_enabled || !dependency_graph) {
        return false;
    }
    
    pthread_mutex_lock(&lockdep_mutex);
    
    bool deadlock_detected = graph_has_cycle(dependency_graph);
    
    if (deadlock_detected) {
        fprintf(stderr, "[LOCKDEP] Deadlock potential: Cycle detected in lock dependency graph\n");
    }
    
    pthread_mutex_unlock(&lockdep_mutex);
    return deadlock_detected;
}

void lockdep_print_dependencies(void) {
    if (!lockdep_enabled || !dependency_graph) {
        return;
    }
    
    pthread_mutex_lock(&lockdep_mutex);
    
    printf("\n[LOCKDEP] === Lock Dependency Graph ===\n");
    
    // Print the graph
    graph_print(dependency_graph, format_ptr);
    
    // Print all thread contexts and their held locks
    printf("\n[LOCKDEP] === Thread Lock States ===\n");
    thread_context_t* ctx = thread_contexts;
    while (ctx) {
        printf("[LOCKDEP] Thread %lu holds %d locks: ", 
               (unsigned long)ctx->thread_id, ctx->lock_depth);
        
        held_lock_t* held = ctx->held_locks;
        while (held) {
            void* lock_addr = graph_node_get_id(held->lock);
            printf("%p ", lock_addr);
            held = held->next;
        }
        printf("\n");
        
        ctx = ctx->next;
    }
    
    printf("[LOCKDEP] ===========================\n\n");
    
    pthread_mutex_unlock(&lockdep_mutex);
}

// Helper function to get the thread context for the current thread
static thread_context_t* get_thread_context(void) {
    pthread_t self = pthread_self();
    
    // Check if we already have a context for this thread
    thread_context_t* ctx = thread_contexts;
    while (ctx) {
        if (pthread_equal(ctx->thread_id, self)) {
            return ctx;
        }
        ctx = ctx->next;
    }
    
    // Create new thread context if not found
    ctx = malloc(sizeof(thread_context_t));
    if (!ctx) {
        perror("[LOCKDEP] Failed to allocate memory for thread context");
        return NULL;
    }
    
    ctx->thread_id = self;
    ctx->held_locks = NULL;
    ctx->lock_depth = 0;
    ctx->next = thread_contexts;
    thread_contexts = ctx;
    
    return ctx;
}



// Helper function to format a pointer as a string for graph printing
static const char* format_ptr(void* ptr) {
    static char buffer[32];
    snprintf(buffer, sizeof(buffer), "%p", ptr);
    return buffer;
}

// Helper function to print a backtrace when lock order violations are detected
static void print_backtrace(void) {
    void* callstack[128];
    int frames = backtrace(callstack, 128);
    char** symbols = backtrace_symbols(callstack, frames);
    
    fprintf(stderr, "[LOCKDEP] Lock order violation backtrace:\n");
    for (int i = 0; i < frames; i++) {
        fprintf(stderr, "  %s\n", symbols[i]);
    }
    
    free(symbols);
}