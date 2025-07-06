/**
 * @file pthread_structures.h
 * @brief Definitions and functions for interpreting pthread structures in memory
 *
 * This file provides declarations for accessing and interpreting pthread mutex 
 * and thread structures in the memory of a traced process, allowing for
 * external analysis of lock states and relationships.
 */

#ifndef PTHREAD_STRUCTURES_H
#define PTHREAD_STRUCTURES_H

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Different types of pthread mutexes
 */
typedef enum {
    MUTEX_TYPE_NORMAL = 0,
    MUTEX_TYPE_ERRORCHECK = 1,
    MUTEX_TYPE_RECURSIVE = 2,
    MUTEX_TYPE_ADAPTIVE = 3,
    MUTEX_TYPE_UNKNOWN = -1
} pthread_mutex_type_t;

/**
 * @brief States a mutex can be in
 */
typedef enum {
    MUTEX_STATE_UNLOCKED = 0,
    MUTEX_STATE_LOCKED_UNCONTENDED = 1,
    MUTEX_STATE_LOCKED_CONTENDED = 2,
    MUTEX_STATE_UNKNOWN = -1
} pthread_mutex_state_t;

/**
 * @brief Information about a pthread mutex
 */
typedef struct {
    void* mutex_addr;             // Address of the mutex in the target process
    pthread_mutex_type_t type;    // Type of mutex
    pthread_mutex_state_t state;  // Current state of the mutex
    pid_t owner;                  // Thread ID of the owner (0 if not owned)
    int lock_count;               // Number of times locked (for recursive mutexes)
} pthread_mutex_info_t;

/**
 * @brief Information about a pthread thread
 */
typedef struct {
    pid_t thread_id;              // Thread ID (usually same as the process ID for the thread)
    void* thread_struct_addr;     // Address of the pthread structure in the target process
    void* stack_addr;             // Base address of the thread's stack
    size_t stack_size;            // Size of the thread's stack
} pthread_thread_info_t;

/**
 * @brief Initialize the pthread structures subsystem
 * 
 * This function prepares the subsystem for interpreting pthread structures,
 * potentially by loading debug symbols or determining offsets in structures.
 * 
 * @return true if initialization was successful, false otherwise
 */
bool pthread_structures_init(void);

/**
 * @brief Clean up resources used by the pthread structures subsystem
 */
void pthread_structures_cleanup(void);

/**
 * @brief Read information about a pthread mutex from the target process
 * 
 * @param pid The process ID of the target process
 * @param mutex_addr The address of the mutex in the target process's memory
 * @param info Pointer to a structure to receive the mutex information
 * @return true if the information was successfully read, false otherwise
 */
bool pthread_read_mutex_info(pid_t pid, void* mutex_addr, pthread_mutex_info_t* info);

/**
 * @brief Read information about a pthread thread from the target process
 * 
 * @param pid The process ID of the target process
 * @param thread_id The thread ID to get information for
 * @param info Pointer to a structure to receive the thread information
 * @return true if the information was successfully read, false otherwise
 */
bool pthread_read_thread_info(pid_t pid, pid_t thread_id, pthread_thread_info_t* info);

/**
 * @brief Find all pthread mutexes in the target process's memory
 * 
 * This function attempts to locate all pthread mutexes in the target process
 * by scanning memory regions or other means.
 * 
 * @param pid The process ID of the target process
 * @param mutex_addrs Array to receive mutex addresses
 * @param max_mutexes Maximum number of mutexes to return
 * @param found Pointer to a variable that will receive the number of mutexes found
 * @return true if the operation was successful, false otherwise
 */
bool pthread_find_all_mutexes(pid_t pid, void** mutex_addrs, size_t max_mutexes, size_t* found);

/**
 * @brief Get a human-readable name for a mutex type
 * 
 * @param type The mutex type
 * @return A string describing the mutex type
 */
const char* pthread_mutex_type_name(pthread_mutex_type_t type);

/**
 * @brief Get a human-readable name for a mutex state
 * 
 * @param state The mutex state
 * @return A string describing the mutex state
 */
const char* pthread_mutex_state_name(pthread_mutex_state_t state);

/**
 * @brief Set whether to use debug symbols when available
 * 
 * If debug symbols are available for the target process, they can be used
 * to get more accurate information about pthread structures.
 * 
 * @param use_symbols Whether to use debug symbols
 */
void pthread_set_use_debug_symbols(bool use_symbols);

#endif /* PTHREAD_STRUCTURES_H */