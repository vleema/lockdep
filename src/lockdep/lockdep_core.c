#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "lockdep.h"

bool lockdep_enabled = true;

static lock_node_t* lock_registry;
static thread_context_t* thread_registry;
static pthread_mutex_t lockdep_mutex = PTHREAD_MUTEX_INITIALIZER;

static lock_node_t** cycle_path = NULL;
static size_t cycle_path_size = 0;
static size_t cycle_path_capacity = 0;

static void* smalloc(const size_t bytes)
{
    void* ptr = malloc(bytes);
    if (!ptr) {
        fprintf(stderr, "out of memory, aborting\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static void* srealloc(void* ptr, const size_t bytes)
{
    void* new_ptr = realloc(ptr, bytes);
    if (!new_ptr) {
        fprintf(stderr, "out of memory, aborting\n");
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

static void capture_stack_trace(void** stack, int* depth, int max_depth)
{
    *depth = backtrace(stack, max_depth);
}

static void resolve_address(void* addr, char* result, size_t result_size)
{
    char command[512];
    FILE* pipe;

    // Try to find the executable name for addr2line.
    Dl_info info;
    if (dladdr(addr, &info) && info.dli_fname) {
        // Calculate relative address for shared libraries.
        void* relative_addr = (void*)((char*)addr - (char*)info.dli_fbase);

        snprintf(command, sizeof(command), "addr2line -e '%s' -f -C %p 2>/dev/null || echo '?? at ??:0'",
                 info.dli_fname, relative_addr);

        pipe = popen(command, "r");
        if (pipe) {
            char function[128] = {0};
            char location[256] = {0};

            if (fgets(function, sizeof(function), pipe) && fgets(location, sizeof(location), pipe)) {

                // Remove newlines.
                function[strcspn(function, "\n")] = 0;
                location[strcspn(location, "\n")] = 0;

                // Check if we got useful info.
                if (strcmp(function, "??") != 0 && strcmp(location, "??:0") != 0) {
                    snprintf(result, result_size, "%s at %s", function, location);
                } else {
                    snprintf(result, result_size, "%p in %s", addr, info.dli_fname);
                }
            } else {
                snprintf(result, result_size, "%p in %s", addr, info.dli_fname);
            }
            pclose(pipe);
            return;
        }
    }

    // Fallback to raw address if addr2line fails.
    snprintf(result, result_size, "%p", addr);
}

static void print_stack_trace(void** stack, int depth, const char* prefix)
{
    char** symbols = backtrace_symbols(stack, depth);

    for (int i = 0; i < depth; i++) {
        char resolved[512];
        resolve_address(stack[i], resolved, sizeof(resolved));

        fprintf(stderr, "%s    [%d] %s\n", prefix, i, resolved);

        if (symbols) {
            fprintf(stderr, "%s         Raw: %s\n", prefix, symbols[i]);
        }
    }

    if (symbols) {
        free(symbols);
    }
}

static const char* generate_lock_name(const void* lock_addr)
{
    static char name_buffer[LOCKDEP_MAX_LOCK_NAME];
    snprintf(name_buffer, sizeof(name_buffer), "lock_%p", lock_addr);
    return name_buffer;
}

static pid_t get_thread_id()
{
    return syscall(SYS_gettid);
}

static lock_node_t* find_or_register_lock(const void* lock_addr)
{
    lock_node_t* lock = lock_registry;

    while (lock) {
        if (lock->lock_addr == lock_addr) return lock;
        lock = lock->next;
    }

    lock = smalloc(sizeof(lock_node_t));
    lock->children = NULL;
    lock->lock_addr = lock_addr;
    strncpy(lock->lock_name, generate_lock_name(lock_addr), LOCKDEP_MAX_LOCK_NAME - 1);
    lock->lock_name[LOCKDEP_MAX_LOCK_NAME - 1] = '\0';
    lock->next = lock_registry;
    lock->was_visited = false;
    lock->current_held_lock = NULL;
    return lock_registry = lock;
}

static thread_context_t* find_thread_context(const pthread_t pthread_id)
{
    thread_context_t* ctx = thread_registry;
    while (ctx) {
        if (ctx->pthread_id == pthread_id) return ctx;
        ctx = ctx->next;
    }
    return NULL;
}

static thread_context_t* add_lock_to_thread_context(thread_context_t* ctx, lock_node_t* lock)
{
    if (ctx) {
        held_lock_t* new_held = smalloc(sizeof(held_lock_t));
        new_held->lock = lock;
        new_held->thread_context = ctx;
        clock_gettime(CLOCK_MONOTONIC, &new_held->acquired_at);
        capture_stack_trace(new_held->acquisition_stack, &new_held->stack_depth, LOCKDEP_MAX_STACK_DEPTH);
        new_held->next = ctx->held_locks;
        ctx->held_locks = new_held;

        // Update lock ownership reference
        lock->current_held_lock = new_held;

    } else {
        // Create new thread context.
        ctx = smalloc(sizeof(thread_context_t));
        ctx->held_locks = smalloc(sizeof(held_lock_t));
        ctx->pthread_id = pthread_self();
        ctx->thread_id = get_thread_id();
        snprintf(ctx->thread_name, sizeof(ctx->thread_name), "thread_%d", ctx->thread_id);

        ctx->held_locks->lock = lock;
        ctx->held_locks->thread_context = ctx;
        clock_gettime(CLOCK_MONOTONIC, &ctx->held_locks->acquired_at);
        capture_stack_trace(ctx->held_locks->acquisition_stack, &ctx->held_locks->stack_depth, LOCKDEP_MAX_STACK_DEPTH);
        ctx->held_locks->next = NULL;

        ctx->next = thread_registry;
        thread_registry = ctx;

        // Update lock ownership reference
        lock->current_held_lock = ctx->held_locks;
    }

    return ctx;
}

static thread_context_t* release_lock_from_thread_context(thread_context_t* ctx, const void* lock_addr)
{
    held_lock_t** held = &ctx->held_locks;
    while (*held) {
        if ((*held)->lock->lock_addr == lock_addr) {
            held_lock_t* to_free = *held;

            // Clear lock ownership reference
            (*held)->lock->current_held_lock = NULL;

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

static void print_thread_states()
{
    fprintf(stderr, "\n[LOCKDEP] Current thread states:\n");
    thread_context_t* ctx = thread_registry;

    while (ctx) {
        fprintf(stderr, "  Thread %s:\n", ctx->thread_name);

        held_lock_t* held = ctx->held_locks;
        if (held) {
            fprintf(stderr, "    HOLDING locks:\n");
            while (held) {
                fprintf(stderr, "      - %s acquired at %.5fs\n", held->lock->lock_name,
                        (double)held->acquired_at.tv_sec + (double)held->acquired_at.tv_nsec / 1e9);
                held = held->next;
            }
        } else {
            fprintf(stderr, "    HOLDING no locks\n");
        }

        ctx = ctx->next;
    }
}

static void suggest_lock_ordering()
{
    fprintf(stderr, "\n[LOCKDEP] Suggested lock ordering (by address):\n");
    fprintf(stderr, "  Always acquire locks in this order to avoid deadlocks:\n");

    lock_node_t* locks[256];
    int lock_count = 0;
    lock_node_t* l = lock_registry;

    while (l && lock_count < 256) {
        locks[lock_count++] = l;
        l = l->next;
    }

    // Panis et Circenses, bubble sort to suggest a lock ordering XD
    for (int i = 0; i < lock_count - 1; i++) {
        for (int j = 0; j < lock_count - i - 1; j++) {
            if (locks[j]->lock_addr > locks[j + 1]->lock_addr) {
                lock_node_t* temp = locks[j];
                locks[j] = locks[j + 1];
                locks[j + 1] = temp;
            }
        }
    }

    for (int i = 0; i < lock_count; i++) {
        fprintf(stderr, "    %d. %s\n", i + 1, locks[i]->lock_name);
    }
}

static void print_cycle(const lock_node_t* lock)
{
    fprintf(stderr, "\n"
                    "===============================================\n"
                    "[LOCKDEP] *** DEADLOCK DETECTED ***\n"
                    "===============================================\n");

    fprintf(stderr, "\n[LOCKDEP] Deadlock cycle detected:\n");
    for (size_t i = 0; i < cycle_path_size; i++) {
        lock_node_t* current_lock = cycle_path[i];
        fprintf(stderr, "  [%zu] %s\n", i + 1, current_lock->lock_name);

        if (current_lock->current_held_lock) {
            held_lock_t* held = current_lock->current_held_lock;
            fprintf(stderr, "      Acquired: %.5fs ago\n",
                    (double)held->acquired_at.tv_sec + (double)held->acquired_at.tv_nsec / 1e9);

            fprintf(stderr, "      Owner: %s\n", held->lock->current_held_lock->thread_context->thread_name);

            if (held->stack_depth > 0) {
                fprintf(stderr, "      Acquisition stack trace:\n");
                print_stack_trace(held->acquisition_stack, held->stack_depth, "        ");
            }
        }

        if (i < cycle_path_size - 1) {
            fprintf(stderr, "      ↓ depends on\n");
        }
    }

    fprintf(stderr, "  [%zu] %s ← CYCLE CLOSES HERE\n", cycle_path_size + 1, lock->lock_name);

    print_thread_states();
    suggest_lock_ordering();

    fprintf(stderr, "\n"
                    "===============================================\n"
                    "[LOCKDEP] End of deadlock report\n"
                    "===============================================\n\n");
}

static bool has_cycle(const lock_node_t* l)
{
    adjacency_locks_t* to_visit = l->children;
    while (to_visit) {
        if (!to_visit->lock->was_visited) {
            to_visit->lock->was_visited = true;

            if (cycle_path_size >= cycle_path_capacity) {
                cycle_path_capacity = cycle_path_capacity == 0 ? 16 : cycle_path_capacity * 2;
                cycle_path = srealloc(cycle_path, cycle_path_capacity * sizeof(lock_node_t*));
            }
            cycle_path[cycle_path_size++] = to_visit->lock;

            const bool returnval = has_cycle(to_visit->lock);

            cycle_path_size--;

            to_visit->lock->was_visited = false;
            if (returnval) return true;
        } else {
            print_cycle(to_visit->lock);
            return true;
        }
        to_visit = to_visit->next;
    }
    return false;
}

static bool has_deadlock(lock_node_t* lock)
{
    cycle_path_size = 0;
    return has_cycle(lock);
}

void lockdep_init(void)
{
    const char* env = getenv("LOCKDEP_DISABLE");
    if (env && strcmp(env, "1") == 0) {
        lockdep_enabled = false;
        return;
    }
}

bool lockdep_acquire_lock(const void* lock_addr)
{
    fprintf(stderr, "[LOCKDEP] Acquiring lock %p\n", lock_addr);

    pthread_mutex_lock(&lockdep_mutex);

    thread_context_t* ctx = find_thread_context(pthread_self());

    lock_node_t* lock = find_or_register_lock(lock_addr);
    add_dependency_to_graph(add_lock_to_thread_context(ctx, lock), lock);
    const bool deadlock = has_deadlock(lock);
    deadlock ? 42 : unvisit_locks();

    pthread_mutex_unlock(&lockdep_mutex);

    return !deadlock;
}

void lockdep_release_lock(const void* lock_addr)
{
    fprintf(stderr, "[LOCKDEP] Releasing lock %p\n", lock_addr);

    pthread_mutex_lock(&lockdep_mutex);

    release_lock_from_thread_context(find_thread_context(pthread_self()), lock_addr);

    pthread_mutex_unlock(&lockdep_mutex);
}
