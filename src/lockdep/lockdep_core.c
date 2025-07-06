#include <execinfo.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lockdep.h"

bool lockdep_enabled = true;

// Global lock graph state
static lock_node_t* lock_graph = NULL;
static dependency_edge_t* dependencies = NULL;
static thread_context_t* thread_contexts = NULL;

// Mutex to protect the lock graph's internal state
static pthread_mutex_t lockdep_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations for internal functions
static thread_context_t* get_thread_context(void);
static lock_node_t* find_or_create_lock_node(void* lock_addr);
static bool check_cycle_from(lock_node_t* start, lock_node_t* target, bool* visited);
static bool add_dependency(lock_node_t* from, lock_node_t* to);
static void print_backtrace(void);

void lockdep_init(void) {
    const char* env = getenv("LOCKDEP_DISABLE");
    if (env && strcmp(env, "1") == 0) {
        lockdep_enabled = false;
        return;
    }

    fprintf(stderr, "[LOCKDEP] Lockdep initialized\n");
}

void lockdep_cleanup(void) {
    pthread_mutex_lock(&lockdep_mutex);
    
    // Free lock nodes
    lock_node_t* node = lock_graph;
    while (node) {
        lock_node_t* next = node->next;
        free(node);
        node = next;
    }
    
    // Free dependencies
    dependency_edge_t* edge = dependencies;
    while (edge) {
        dependency_edge_t* next = edge->next;
        free(edge);
        edge = next;
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
    
    lock_graph = NULL;
    dependencies = NULL;
    thread_contexts = NULL;
    
    pthread_mutex_unlock(&lockdep_mutex);
}

bool lockdep_acquire_lock(void* lock_addr) {
    if (!lockdep_enabled) {
        return true;
    }
    
    pthread_mutex_lock(&lockdep_mutex);
    
    printf("[LOCKDEP] Thread %lu acquiring lock at %p\n", 
           (unsigned long)pthread_self(), lock_addr);
    
    // Get or create lock node for this lock address
    lock_node_t* lock_node = find_or_create_lock_node(lock_addr);
    
    // Get thread context
    thread_context_t* thread_ctx = get_thread_context();
    
    // Check if we already have locks held and need to add dependencies
    if (thread_ctx->held_locks != NULL) {
        // The most recently acquired lock should have a dependency on this new lock
        lock_node_t* prev_lock = thread_ctx->held_locks->lock;
        
        // Add dependency: prev_lock -> lock_node
        if (!add_dependency(prev_lock, lock_node)) {
            // Dependency would create a cycle - potential deadlock!
            fprintf(stderr, "[LOCKDEP] WARNING: Lock order violation detected!\n");
            fprintf(stderr, "[LOCKDEP] Thread %lu attempting to acquire %p while holding %p\n",
                    (unsigned long)pthread_self(), lock_addr, prev_lock->lock_addr);
            fprintf(stderr, "[LOCKDEP] This violates previously observed lock ordering and may lead to deadlocks.\n");
            print_backtrace();
            
            // Check if we have an actual cycle in the dependency graph
            bool result = lockdep_check_deadlock();
            if (result) {
                fprintf(stderr, "[LOCKDEP] DEADLOCK POTENTIAL: Circular lock dependency detected!\n");
                pthread_mutex_unlock(&lockdep_mutex);
                return false;
            } else {
                fprintf(stderr, "[LOCKDEP] Warning only: No circular dependency yet, but lock order inconsistent\n");
            }
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
    if (!lockdep_enabled) {
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
        if ((*curr)->lock->lock_addr == lock_addr) {
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
    if (!lockdep_enabled) {
        return false;
    }
    
    pthread_mutex_lock(&lockdep_mutex);
    
    bool deadlock_detected = false;
    
    // Allocate visited array for each lock node
    int node_count = 0;
    lock_node_t* node;
    for (node = lock_graph; node != NULL; node = node->next) {
        node_count++;
    }
    
    // For each lock, check if there's a path back to itself
    for (node = lock_graph; node != NULL; node = node->next) {
        bool* visited = calloc(node_count, sizeof(bool));
        if (!visited) {
            perror("[LOCKDEP] Failed to allocate memory for deadlock detection");
            continue;
        }
        
        if (check_cycle_from(node, node, visited)) {
            fprintf(stderr, "[LOCKDEP] Deadlock potential: Found cycle starting at lock %p\n", 
                    node->lock_addr);
            deadlock_detected = true;
            free(visited);
            break;
        }
        
        free(visited);
    }
    
    pthread_mutex_unlock(&lockdep_mutex);
    return deadlock_detected;
}

void lockdep_print_dependencies(void) {
    if (!lockdep_enabled) {
        return;
    }
    
    pthread_mutex_lock(&lockdep_mutex);
    
    printf("\n[LOCKDEP] === Lock Dependency Graph ===\n");
    
    // Print all edges in the dependency graph
    dependency_edge_t* edge = dependencies;
    while (edge) {
        printf("[LOCKDEP] %p -> %p\n", edge->from->lock_addr, edge->to->lock_addr);
        edge = edge->next;
    }
    
    // Print all thread contexts and their held locks
    printf("\n[LOCKDEP] === Thread Lock States ===\n");
    thread_context_t* ctx = thread_contexts;
    while (ctx) {
        printf("[LOCKDEP] Thread %lu holds %d locks: ", 
               (unsigned long)ctx->thread_id, ctx->lock_depth);
        
        held_lock_t* held = ctx->held_locks;
        while (held) {
            printf("%p ", held->lock->lock_addr);
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

// Helper function to find or create a lock node
static lock_node_t* find_or_create_lock_node(void* lock_addr) {
    // Check if lock already exists
    lock_node_t* node = lock_graph;
    while (node) {
        if (node->lock_addr == lock_addr) {
            return node;
        }
        node = node->next;
    }
    
    // Create new lock node
    node = malloc(sizeof(lock_node_t));
    if (!node) {
        perror("[LOCKDEP] Failed to allocate memory for lock node");
        return NULL;
    }
    
    node->lock_addr = lock_addr;
    node->next = lock_graph;
    lock_graph = node;
    
    return node;
}

// Helper function to check for cycles in the dependency graph using DFS
static bool check_cycle_from(lock_node_t* current, lock_node_t* target, bool* visited) {
    // Find current node's index
    int current_idx = 0;
    lock_node_t* node = lock_graph;
    while (node != current) {
        current_idx++;
        node = node->next;
    }
    
    // If we've already visited this node in this DFS traversal, skip it
    if (visited[current_idx]) {
        return false;
    }
    
    // Mark current node as visited
    visited[current_idx] = true;
    
    // Check all outgoing edges from current node
    dependency_edge_t* edge = dependencies;
    while (edge) {
        if (edge->from == current) {
            // If we found our target, we have a cycle
            if (edge->to == target) {
                return true;
            }
            
            // Continue DFS from the destination node
            if (check_cycle_from(edge->to, target, visited)) {
                return true;
            }
        }
        edge = edge->next;
    }
    
    return false;
}

// Helper function to add a dependency between locks
static bool add_dependency(lock_node_t* from, lock_node_t* to) {
    // First check if this dependency already exists
    dependency_edge_t* edge = dependencies;
    while (edge) {
        if (edge->from == from && edge->to == to) {
            return true; // Dependency already exists
        }
        edge = edge->next;
    }
    
    // Add the new dependency
    edge = malloc(sizeof(dependency_edge_t));
    if (!edge) {
        perror("[LOCKDEP] Failed to allocate memory for dependency edge");
        return true; // Continue without adding in case of allocation failure
    }
    
    edge->from = from;
    edge->to = to;
    edge->next = dependencies;
    dependencies = edge;
    
    // Check if this new dependency creates a cycle
    bool* visited = calloc(1000, sizeof(bool)); // Assuming max 1000 locks for simplicity
    if (!visited) {
        perror("[LOCKDEP] Failed to allocate memory for cycle detection");
        return true; // Continue without checking in case of allocation failure
    }
    
    // Check if there's a path from 'to' back to 'from', which would create a cycle
    bool has_cycle = check_cycle_from(to, from, visited);
    
    free(visited);
    return !has_cycle; // Return false if cycle exists
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